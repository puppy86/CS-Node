#pragma once

#include <algorithm>
#include <list>
#include <memory>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

#include <boost/asio.hpp>

#include "Logger.hpp"
#include "Packet.hpp"

typedef std::chrono::steady_clock Clock;
using boost::asio::ip::udp;
using namespace boost::asio;

template <typename Key, typename Value, size_t Capacity>
class CircularMap {
public:
	typedef std::unordered_map<Key, Value> MapType;

	CircularMap() {
		map_.reserve(Capacity);
	}

	Value pushAndIncrease(const Key& key) {
		auto place = map_.find(key);

		if (place == map_.end()) {
			if (queue_.size() == Capacity) {
				map_.erase(queue_.front());
				queue_.pop_front();
			}

			queue_.push_back(key);
			place = map_.insert(std::make_pair(key, 1)).first;
		}
		else
			++(place->second);

		return place->second;
	}

private:
	MapType map_;
	std::deque<Key> queue_;
};

struct PacketPart {
	PacketPart(PacketPtr* mem, size_t maxSize) : packets(mem), size(maxSize), left(maxSize) {
		memset(packets, 0, sizeof(PacketPtr) * maxSize);
	}

	~PacketPart() {
		clear();
	}

	PacketPtr* packets;
	size_t totalSize = 0;
	size_t left;
	size_t size;

	bool tryInsert(PacketPtr pack, const size_t size) {
		auto& target = packets[pack->header];
		if (target) return false;
		
		target = pack;
		totalSize += size;
		--left;
		return true;
	}

	void clear() {
		PacketPtr* end = packets + size;
		for (PacketPtr* ptr = packets; ptr != end; ++ptr)
			*ptr = PacketPtr();
	}
};

template <typename Key, size_t Capacity, size_t MaxSeqLength>
class PacketCollector {
public:
	typedef std::unordered_map<Key, PacketPart> MapType;

	PacketCollector() {
		map_.reserve(Capacity);
		sequences = (PacketPtr*)malloc(sizeof(PacketPtr) * Capacity * MaxSeqLength);
	}

	~PacketCollector() {
		free(sequences);
	}

	std::pair<PacketPart*, bool> append(PacketPtr packet, const std::size_t dataSize) {
		auto key = Key{ packet->HashBlock };
		bool added = false;

		auto place = map_.find(key);
		if (place == map_.end()) {
			PacketPtr* nextSeq;

			if (queue_.size() == Capacity) {
				auto oldestPack = map_.find(queue_.front());
				nextSeq = oldestPack->second.packets;

				map_.erase(oldestPack);
				queue_.pop_front();
			}
			else
				nextSeq = sequences + (queue_.size() * MaxSeqLength);

			queue_.push_back(key);
			place = map_.insert(std::make_pair<Key, PacketPart>(std::move(key), PacketPart(nextSeq, packet->countHeader))).first;
		}

		return std::make_pair(&(place->second), place->second.tryInsert(packet, dataSize));
	}

private:
	MapType map_;
	PacketPtr* sequences;
	std::deque<Key> queue_;
};

namespace std {
	template <>
	struct hash<udp::endpoint> {
		std::size_t operator()(const udp::endpoint& ep) const { 
			return std::hash<boost::asio::ip::address_v4::uint_type>()(ep.address().to_v4().to_uint());
		}
	};
}

template <size_t Capacity>
class NodesRing {
public:
	NodesRing() {
		nodes_.reserve(Capacity);
	}

	bool place(udp::endpoint&& ep) {
		if (nodes_.find(ep) == nodes_.end()) {
			if (endpoints_.size() == Capacity) {
				LOG_NODESBUF_POP(endpoints_.front());
				nodes_.erase(endpoints_.front());
				endpoints_.pop_front();
			}

			LOG_NODESBUF_PUSH(ep);
			nodes_.insert(ep);
			endpoints_.push_back(std::move(ep));

			return true;
		}

		return false;
	}

	const std::deque<udp::endpoint>& getEndPoints() const {
		return endpoints_;
	}

private:
	std::unordered_set<udp::endpoint> nodes_;
	std::deque<udp::endpoint> endpoints_;
};

const auto BROADCAST_INIT_TIMEOUT = std::chrono::milliseconds(2);
const auto DIRECT_INIT_TIMEOUT = std::chrono::milliseconds(2);
const auto MAX_TIMEOUT = std::chrono::milliseconds(1024);

struct Task {
	Task(std::vector<PacketPtr>&& packs, size_t size, udp::endpoint&& ep) :
		nextLaunch(Clock::now()),
		timeout(DIRECT_INIT_TIMEOUT),
		packets(std::move(packs)),
		lastSize(Packet::headerLength() + size),
		receivers(1, std::move(ep)),
		broadcast(false) { }

	Task(std::vector<PacketPtr>&& packs, size_t size, const std::deque<udp::endpoint>& recvs) :
		nextLaunch(Clock::now()),
		timeout(BROADCAST_INIT_TIMEOUT),
		packets(std::move(packs)),
		lastSize(Packet::headerLength() + size),
		receivers(recvs.begin(), recvs.end()),
		broadcast(true) { }

	Clock::time_point nextLaunch;
	std::chrono::milliseconds timeout;

	std::vector<PacketPtr> packets;
	std::size_t lastSize;

	std::vector<ip::udp::endpoint> receivers;
	bool broadcast;
};

typedef std::list<Task>::iterator TaskId;

class TaskManager {
public:
	TaskManager():  // Ctrl + Alt + Delete
		nextTime_(Clock::now()) { }

	void stop() { running_ = false; }

	TaskId add(Task&& t) {
		TaskId result;

		{
			//std::lock_guard<std::mutex> l(mut_);

			if (tasks_.empty() || t.nextLaunch < nextTime_)
				nextTime_ = t.nextLaunch;

			tasks_.emplace_front(std::move(t));
			result = tasks_.begin();
		}

		cv_.notify_all();
		return result;
	}

	void remove(const TaskId id) {
		//std::lock_guard<std::mutex> l(mut_);
		tasks_.erase(id);
	}

	void clear() {
		//std::lock_guard<std::mutex> l(mut_);
		tasks_.clear();
	}

	template <typename Func>
	void run(Func f) {
		if (nextTime_ > Clock::now()) return;
		//while (running_) {
			bool wroteNewNT = false;
			{
				//std::unique_lock<std::mutex> l(mut_);
				for (auto& t : tasks_) {
					if (t.nextLaunch <= Clock::now()) {
						f(t);
						t.nextLaunch += t.timeout;

						if (t.timeout == MAX_TIMEOUT)
							t.timeout = t.broadcast ? BROADCAST_INIT_TIMEOUT : DIRECT_INIT_TIMEOUT;
						else
							t.timeout *= 4;
					}

					if (!wroteNewNT || t.nextLaunch < nextTime_)
						nextTime_ = t.nextLaunch;
				}

				//cv_.wait_until(l, nextTime_, [this]() { return !tasks_.empty() && nextTime_ <= Clock::now(); });
			}
		//}
	}

private:
	std::mutex mut_;
	std::condition_variable cv_;

	std::atomic_bool running_{true};

	Clock::time_point nextTime_;
	std::list<Task> tasks_;
};
