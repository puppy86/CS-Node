#pragma once

#include <cstdint>
#include <memory>
#include <cstring>

#include "Hash.hpp"

/// ������������ ������
enum CommandList {
	Registration = 1,         /// �����������
	UnRegistration,		 /// �������������� 
	Redirect,			 /// ����������
	GetHash,              /// ������� ���
	SendHash,		     /// ��������� ���
	SendTransaction,      /// ��������� ����������
	GetTransaction,       /// ������� ����������
	SendTransactionList,  /// ��������� �������������� ����
	GetTransactionList,   /// ������� �������������� ����
	SendVector,           /// ��������� ������
	GetVector,			 /// ������� ������
	SendMatrix,           /// ��������� �������
	GetMatrix,            /// ������� �������
	SendBlock,            /// ��������� ���� ������
	GetHashAll,          /// ������ ���(�) �� ���� �����
	SendIpTable,         /// ��������� ������ ���������� ����� � �������� ���� �����
	SinhroPacket,
	GiveHash2,
	SuperCommand,
	GetSync,
	SendSync,
	RealBlock,
	SendFirstBlock = 24,
	RegistrationConnectionRefused = 25,
	SendBlockCandidate = 28,
	GetBlockCandidate = 29,
	GetFirstTransaction = 30
};

/// ������������ ���������
enum SubCommandList {
	RegistrationLevelNode = 1, /// ������� ������ ���������� � �������� ����
	GiveHash,              /// ������ �� ���
	GetBlock,			  /// ������ �� ���� ������
	GetBlocks,
	Empty,
	GetFirstBlock,
	SGetTransaction,
	SGetTransactionsList,
	SGetVector,
	SGetMatrix,
	SGetHash,
	SGetIpTable
};

enum Version {
	version_1 = 1
};

enum { max_length = 62440 };

#pragma pack(push, 1)
struct Packet {
	char	 command;                        /// �������
	char	 subcommand;					 /// ����������
	char	 version;						 /// ������
	uint32_t origin_ip;                      /// ����� ������� �����������
	char	 hash[hash_length];              /// ��� �����������/������������ ����
	char	 publicKey[publicKey_length];    /// ��������� ���� �����������/������������ ����
	char	 HashBlock[hash_length];		 /// ��� �����
	uint16_t header;						 /// ����� ���������
	uint16_t countHeader;					 /// ���������� ����������
	char	 data[max_length];               /// ������

	constexpr static unsigned int headerLength() { return sizeof(Packet) - max_length; }
};
#pragma pack(pop)

struct PacketWithCounter {
	uint32_t counter;
	Packet p;
};

class PacketPtr {
public:
	PacketPtr() { }

	PacketPtr(const PacketPtr& rhs) : ptr_(rhs.ptr_) {
		increment();
	}

	~PacketPtr() { 
		decrement();	
	}

	PacketPtr(PacketPtr&& rhs): ptr_(rhs.ptr_) {
		rhs.ptr_ = nullptr;
	}

	PacketPtr& operator=(const PacketPtr& rhs) {
		if (ptr_ != rhs.ptr_) {
			decrement();
			ptr_ = rhs.ptr_;
			increment();
		}

		return *this;
	}

	PacketPtr& operator=(PacketPtr&& rhs) {
		if (ptr_ != rhs.ptr_) {
			decrement();
			ptr_ = rhs.ptr_;
		}

		rhs.ptr_ = nullptr;

		return *this;
	}

	Packet& operator*() { return ptr_->p; }

	Packet* operator->() { return &(ptr_->p); }
	Packet* operator->() const { return &(ptr_->p); }

	operator bool() const { return ptr_; }

	Packet* get() { return &(ptr_->p); }
	Packet* get() const { return &(ptr_->p); }

private:
	PacketPtr(PacketWithCounter* ptr) : ptr_(ptr) {
		ptr_->counter = 1;
	}

	void increment() {
		if (ptr_)
			++(ptr_->counter);
	}

	void decrement() {
		if (ptr_ && !(--(ptr_->counter)))
			freeFunc(this);
	}

	PacketWithCounter* ptr_ = nullptr;
	static std::function<void(PacketPtr*)> freeFunc;

	template <size_t> friend class PacketManager;
};

/* Meant as singleton */
template <size_t PageSize>
class PacketManager {
public:
	PacketManager() { 
		allocateNewPage();
		PacketPtr::freeFunc = [this](PacketPtr* p) { freeMem(p); };
	}

	~PacketManager() {
		for (PacketWithCounter* p : pages_) free(p);
	}

	PacketPtr getFreePack() {
		if (freeStack_.empty())
			allocateNewPage();

		PacketPtr result(freeStack_.back());
		freeStack_.pop_back();

		return result;
	}

	void freeMem(PacketPtr* pack) {
		freeStack_.push_back(pack->ptr_);
	}

private:
	void allocateNewPage() {
		pages_.push_back((PacketWithCounter*)malloc(sizeof(PacketWithCounter) * PageSize));
		freeStack_.reserve(pages_.size() * PageSize);

		PacketWithCounter* ptr = pages_.back();
		for (size_t i = 0; i < PageSize; ++i, ++ptr)
			freeStack_.push_back(ptr);
	}

	std::vector<PacketWithCounter*> pages_;
	std::vector<PacketWithCounter*> freeStack_;
};

template <std::size_t HashSize>
class MessageHasher {
public:
	void init(const PublicKey& publicKey) {
		memset(internal_, 0, 32);
		memcpy(internal_ + 32, publicKey.str, publicKey_length);
	}

	void nextHash(const void* data, const std::size_t length, char* out) {
		blake2s(hash, HashSize, data, length, nullptr, 0);
		memset(out, 0, (hash_length - HashSize));
		blake2s(out + (hash_length - HashSize), HashSize, internal_, sizeof(internal_), nullptr, 0);
		++(*((uint32_t*)internal_));
	}

private:
	static const auto offset = 32 + publicKey_length;
	char internal_[offset + HashSize];  // id + public key + data hash
	char* hash = internal_ + offset;
};

namespace std {
	template <>
	struct hash<Hash> {
		std::size_t operator()(const Hash& hash) const {
			static_assert(sizeof(std::size_t) == 8, "This hashing function is for x64 only");

			std::size_t result = 14695981039346656037u;
			for (std::size_t i = 0; i < hash_length; ++i) {
				const auto next = std::size_t(hash.str[i]);
				result = (result ^ next) * std::size_t(1099511628211u);
			}
			
			return result;
		}
	};
}