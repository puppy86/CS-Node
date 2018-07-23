#pragma once

#include <csdb/pool.h>
#include <csdb/transaction.h>
#include <Solver/ISolver.hpp>

class IPackStream {
public:
	void init(const char* ptr, const size_t size) {
		ptr_ = ptr;
		end_ = ptr_ + size;
		good_ = true;
	}

	template <typename T>
	IPackStream& operator>>(T& cont) {
		if ((ptr_ - end_) < sizeof(cont)) good_ = false;
		else {
			cont = *(T*)ptr_;
			ptr_ += sizeof(T);
		}

		return *this;
	}

	template <size_t Length>
	IPackStream& operator>>(FixedString<Length>& str) {
		if (end_ - ptr_ < Length) good_ = false;
		else {
			memcpy(str.str, ptr_, Length);
			ptr_ += Length;
		}

		return *this;
	}

	bool good() const { return good_; }
	bool end() const { return ptr_ == end_; }

	operator bool() const { return good() && !end(); }

private:
	const char* ptr_;
	const char* end_;
	bool good_ = false;
};

class OPackStream {
public:
	OPackStream(SessionIO* net) : net_(net) { }

	void init() {
		parts_.clear();
		newPack();
	}

	template <typename T>
	OPackStream& operator<<(const T& d) {
		static_assert(sizeof(T) <= sizeof(Packet::data), "Type too long");

		const auto left = end_ - ptr_;

		if (left >= sizeof(T)) {
			*((T*)ptr_) = d;
			ptr_ += sizeof(T);
		}
		else {  // On border
			memcpy(ptr_, &d, left);
			newPack();
			memcpy(ptr_, ((char*)&d) + left, sizeof(T) - left);
			ptr_ += sizeof(T) - left;
		}

		return *this;
	}

	template <size_t Length>
	OPackStream& operator<<(const FixedString<Length>& str) {
		insertBytes(str.str, Length);
		return *this;
	}

	std::vector<PacketPtr>& get() { return parts_; }
	size_t lastSize() const { return ptr_ - parts_.back()->data; }

private:
	void newPack() {
		parts_.emplace_back(net_->getEmptyPacket());
		ptr_ = parts_.back()->data;
		end_ = ptr_ + sizeof(Packet::data);
	}

	void insertBytes(char const* bytes, size_t size) {
		while (size > 0) {
			if (ptr_ == end_) newPack();

			const auto toPut = std::min((size_t)(end_ - ptr_), size);
			memcpy(ptr_, bytes, toPut);
			size -= toPut;
			ptr_ += toPut;
			bytes += toPut;
		}
	}

	char* ptr_;
	char* end_;
	std::vector<PacketPtr> parts_;

	SessionIO* net_;
};

template <>
IPackStream& IPackStream::operator>>(Credits::NodeId& cont);

template <>
IPackStream& IPackStream::operator>>(std::string& str);

template <>
IPackStream& IPackStream::operator>>(csdb::Transaction& cont);

template <>
IPackStream& IPackStream::operator>>(csdb::Pool& pool);

template <>
OPackStream& OPackStream::operator<<(const Credits::NodeId& d);

template <>
OPackStream& OPackStream::operator<<(const std::string& str);

template <>
OPackStream& OPackStream::operator<<(const csdb::Transaction& trans);

template <>
OPackStream& OPackStream::operator<<(const csdb::Pool& pool);
