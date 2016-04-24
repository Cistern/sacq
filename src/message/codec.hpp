#pragma once

#include <string>
#include <cassert>
#include <cryptopp/sha3.h>
#include <cryptopp/osrng.h>

#include "message.hpp"

class Codec {
public:
	Codec()
	: m_key("")
	{
	}

	void
	set_key(const std::string& key)
	{
		if (key.size() == CryptoPP::SHA3_256::DIGESTSIZE) {
			m_key = key;
			return;
		}
		CryptoPP::SHA3_256 hash;
		uint8_t digest[CryptoPP::SHA3_256::DIGESTSIZE];
		if (key.size() > 0) {
			hash.Update((const uint8_t*)key.c_str(), key.size());
		}
		hash.Final(digest);
		m_key = std::string((const char*)digest, CryptoPP::SHA3_256::DIGESTSIZE);
	}

	int
	pack_message(const Message* m, uint8_t* dest, int dest_len);

	int
	decode_message(std::unique_ptr<Message>& m, uint8_t* src, int src_len);

	int
	decode_message_length(uint8_t* src, int src_len);
private:
	std::string                    m_key;
	CryptoPP::AutoSeededRandomPool m_rng;
}; // Codec
