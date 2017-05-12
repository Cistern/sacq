#pragma once

#include <string>
#include <cassert>
#include <cstdlib>
#include <iostream>

#include "message.hpp"

const int KEY_SIZE = 32;

class Codec {
public:
	Codec()
	: m_key("")
	{
		m_dev_urandom = fopen("/dev/urandom", "r");
		if (m_dev_urandom == nullptr) {
			std::cerr << "libab: failed to open /dev/urandom" << std::endl;
			exit(1);
		}
	}

	void
	set_key(const std::string& key)
	{
		if (key == "") {
			m_key = "";
			return;
		}
		if (key.size() != KEY_SIZE) {
			std::cerr << "libab: invalid encryption key" << std::endl;
			exit(1);
		}
		m_key = key;
	}

	int
	pack_message(const Message* m, uint8_t* dest, int dest_len);

	int
	decode_message(std::unique_ptr<Message>& m, uint8_t* src, int src_len);

	int
	decode_message_length(uint8_t* src, int src_len);

	~Codec()
	{
		fclose(m_dev_urandom);
	}

private:
	void
	randombytes(unsigned char* buf, unsigned int len)
	{
		unsigned int remaining = len;
		while (remaining > 0) {
			size_t read = fread(buf + (len-remaining), remaining, 1, m_dev_urandom);
			if (read == 0) {
				std::cerr << "libab: failed to read from /dev/urandom" << std::endl;
				exit(2);
			}
			remaining -= read;
		}
	}

private:
	std::string m_key;
	FILE*       m_dev_urandom;
}; // Codec
