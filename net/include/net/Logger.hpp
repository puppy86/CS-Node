#pragma once

#include <string>

#define FLAG_LOG_NOTICE 1
#define FLAG_LOG_WARN 2
#define FLAG_LOG_ERROR 4

#define FLAG_LOG_EVENTS 8

#define FLAG_LOG_PACKETS 16
#define FLAG_LOG_NODES_BUFFER 32

///////////////////
#define LOG_LEVEL (FLAG_LOG_NOTICE | FLAG_LOG_WARN | FLAG_LOG_ERROR | FLAG_LOG_EVENTS | (FLAG_LOG_PACKETS & 0) | (FLAG_LOG_NODES_BUFFER & 0))
///////////////////

#if LOG_LEVEL & FLAG_LOG_NOTICE
#define LOG_NOTICE(TEXT) std::cerr << "[NOTICE] " << TEXT << std::endl
#else
#define LOG_NOTICE(TEXT)
#endif

#if LOG_LEVEL & FLAG_LOG_WARN
#define LOG_WARN(TEXT) std::cerr << "[WARNING] " << TEXT << std::endl
#else
#define LOG_WARN(TEXT)
#endif

#if LOG_LEVEL & FLAG_LOG_ERROR
#define LOG_ERROR(TEXT) std::cerr << "[ERROR] " << TEXT << std::endl
#else
#define LOG_ERROR(TEXT)
#endif

////#if LOG_LEVEL & FLAG_LOG_NOTICE
////#define SUPER_TIC() do { std::cerr << __FILE__ << ":" << __LINE__ << std::endl; } while(0)
////#else
////#define SUPER_TIC()
////#endif
#define SUPER_TIC()


#if LOG_LEVEL & FLAG_LOG_EVENTS
#define LOG_EVENT(TEXT) std::cerr << TEXT << std::endl
#else
#define LOG_EVENT(TEXT)
#endif

#if LOG_LEVEL & FLAG_LOG_PACKETS
#define LOG_IN_PACK(PACKET, SIZE) std::cerr << "-> [" << (SIZE - Packet::headerLength()) << "] " << (int)(PACKET)->command << " : " << (int)(PACKET)->subcommand << " " << byteStreamToHex((PACKET)->HashBlock, hash_length) << std::endl
#define LOG_OUT_PACK(PACKET, SIZE) std::cerr << "<- [" << (SIZE - Packet::headerLength()) << "] " << (int)(PACKET)->command << " : " << (int)(PACKET)->subcommand << " " << byteStreamToHex((PACKET)->HashBlock, hash_length) << std::endl
#else
#define LOG_IN_PACK(PACKET, SIZE)
#define LOG_OUT_PACK(PACKET, SIZE)
#endif

#if LOG_LEVEL & FLAG_LOG_NODES_BUFFER
#define LOG_NODESBUF_PUSH(ENDPOINT) std::cerr << "[+] " << (ENDPOINT).address().to_string() << ":" << (ENDPOINT).port() << std::endl
#define LOG_NODESBUF_POP(ENDPOINT) std::cerr << "[-] " << (ENDPOINT).address().to_string() << ":" << (ENDPOINT).port() << std::endl
#else
#define LOG_NODESBUF_PUSH(ENDPOINT)
#define LOG_NODESBUF_POP(ENDPOINT)
#endif

static std::string byteStreamToHex(const char* stream, const size_t length) {
	static std::string map = "0123456789ABCDEF";
	
	std::string result;
	result.reserve(length * 2);

	for (size_t i = 0; i < length; ++i) {
		result.push_back(map[(uint8_t)(stream[i]) >> 4]);
		result.push_back(map[(uint8_t)(stream[i]) & (uint8_t)15]);
	}

	return result;
}