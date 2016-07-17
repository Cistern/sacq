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
private:
	std::string                    m_key;
}; // Codec
