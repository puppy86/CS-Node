#pragma once

enum { hash_length = 40, publicKey_length = 32 };

template <size_t Length>
struct FixedString {
	FixedString() { }
	FixedString(const char* src) { memcpy(str, src, Length); }

	bool operator==(const FixedString& rhs) const { return memcmp(str, rhs.str, Length) == 0; }

	char str[Length];
};

typedef FixedString<hash_length> Hash;
typedef FixedString<publicKey_length> PublicKey;

extern "C" {
	int blake2s(void *out, size_t outlen, const void *in, size_t inlen, const void *key, size_t keylen);
}

PublicKey getHashedPublicKey(const char*);