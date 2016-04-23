#pragma once

#include <string>
#include <cassert>
#include <cryptopp/sha.h>
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
		m_key = key;
	}

	int
	decode_message(std::unique_ptr<Message>& m, uint8_t* src, int src_len);

	int
	decode_message_length(uint8_t* src, int src_len);
private:
	std::string                    m_key;
	CryptoPP::AutoSeededRandomPool m_rng;
}; // Codec
