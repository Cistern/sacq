#include <memory>
#include <cstring>
#include <iostream>
#include <tweetnacl/tweetnacl.h>

#include "message.hpp"
#include "codec.hpp"
#include "randombytes.h"

/**
 * A message header has the following fields:
 * - length (4 bytes)
 * - nonce or hash (24 bytes)
 * - type (1 byte)
 * - flags (1 byte)
 * - id (8 bytes)
 * Total: 38 bytes
 */
const int MSG_HEADER_SIZE = 38;
const int MSG_PADDING_SIZE = 16; // for crypto

const int NONCE_HASH_OFFSET = 4;
const int NONCE_HASH_SIZE = 24;

const int TYPE_OFFSET = 4+NONCE_HASH_SIZE;

int
Message :: pack(uint8_t* dest, int dest_len) const {
	int length = packed_size();
	if (dest_len < length) {
		return -1;
	}

	write32be(length, dest);
	dest += 4;
	dest_len -= 4;
	memcpy(dest, nonce_hash, NONCE_HASH_SIZE);
	dest += NONCE_HASH_SIZE;
	dest_len -= NONCE_HASH_SIZE;
	write8be(type, dest);
	dest++;
	dest_len--;
	write8be(flags, dest);
	dest++;
	dest_len--;
	write64be(message_id, dest);
	dest += 8;
	dest_len -= 8;

	int status = pack_body(dest, dest_len);
	if (status < 0) {
		return -2;
	}
	return length;
}

int
Message :: packed_size() const {
	return body_size() + MSG_HEADER_SIZE + MSG_PADDING_SIZE;
}

int
Message :: unpack(uint8_t* src, int src_len) {
	if (src_len < MSG_HEADER_SIZE) {
		return -1;
	}

	uint32_t length = read32be(src);
	src += 4;
	if (src_len < length) {
		return -2;
	}
	memcpy(nonce_hash, src, NONCE_HASH_SIZE);
	src += NONCE_HASH_SIZE;
	type = read8be(src);
	src++;
	flags = read8be(src);
	src++;
	message_id = read64be(src);
	src += 8;
	unpack_body(src, length);
	return 0;
}

int
Codec :: decode_message(std::unique_ptr<Message>& m, uint8_t* src, int src_len) {
	if (src_len < MSG_HEADER_SIZE) {
		return -1;
	}

	if (m_key == "") {
		// No encryption. Just verify hash.
		uint8_t h[64];
		crypto_hash(h,
			src+NONCE_HASH_OFFSET+NONCE_HASH_SIZE,
			src_len-NONCE_HASH_OFFSET-NONCE_HASH_SIZE);
		if (memcmp(h, src+NONCE_HASH_OFFSET, NONCE_HASH_SIZE) != 0) {
			return -2;
		}
	} else {
		std::string nonce((const char*)src+NONCE_HASH_OFFSET, NONCE_HASH_SIZE);
		std::string c(0, crypto_secretbox_BOXZEROBYTES);
		c += std::string((const char*)src+NONCE_HASH_OFFSET+NONCE_HASH_SIZE,
			src_len-NONCE_HASH_OFFSET-NONCE_HASH_SIZE);
		std::string message_data(0, c.size());
		if (crypto_secretbox_open(
			(uint8_t*)message_data.data(),
			(uint8_t*)c.data(),
			c.size(),
			(uint8_t*)nonce.data(),
			(uint8_t*)m_key.data()) < 0) {
			return -3;
		}
		memcpy(src+NONCE_HASH_OFFSET+NONCE_HASH_SIZE,
			message_data.data()+crypto_secretbox_ZEROBYTES,
			message_data.size()-crypto_secretbox_ZEROBYTES);
	}

	// Peek at the message type
	switch (src[TYPE_OFFSET]) {
	case MSG_IDENT_REQUEST:
		m = std::make_unique<IdentityRequest>();
		break;
	case MSG_IDENT:
		m = std::make_unique<IdentityMessage>();
		break;
	case MSG_LEADER_ACTIVE:
		m = std::make_unique<LeaderActiveMessage>();
		break;
	case MSG_LEADER_ACTIVE_ACK:
		m = std::make_unique<LeaderActiveAck>();
		break;
	default:
		return -1;
	}

	return m->unpack(src, src_len);
}

int
Codec :: pack_message(const Message* m, uint8_t* dest, int dest_len) {
	auto ret = m->pack(dest, dest_len);
	if (ret < 0) {
		return ret;
	}
	if (m_key == "") {
		// No encryption. Just compute a checksum.
		uint8_t h[64];
		crypto_hash(h,
			dest+NONCE_HASH_OFFSET+NONCE_HASH_SIZE,
			m->packed_size()-NONCE_HASH_OFFSET-NONCE_HASH_SIZE);
		memcpy(dest+NONCE_HASH_OFFSET, h, NONCE_HASH_SIZE);
	} else {
		std::string nonce(0, NONCE_HASH_SIZE);
		randombytes((uint8_t*)nonce.data(), NONCE_HASH_SIZE);
		std::string message_data(0, crypto_secretbox_ZEROBYTES);
		message_data += std::string((const char*)dest+NONCE_HASH_OFFSET+NONCE_HASH_SIZE,
			m->packed_size()-NONCE_HASH_OFFSET-NONCE_HASH_SIZE);
		std::string c(0, message_data.size());
		crypto_secretbox(
			(uint8_t*)c.data(),
			(uint8_t*)message_data.data(),
			message_data.size(),
			(uint8_t*)nonce.data(),
			(uint8_t*)m_key.data());
		memcpy(dest+NONCE_HASH_OFFSET+NONCE_HASH_SIZE, c.data(), c.size()-crypto_secretbox_BOXZEROBYTES);
	}
	return 0;
}

int
Codec :: decode_message_length(uint8_t* src, int src_len) {
	if (src_len < 4) {
		return -1;
	}
	uint32_t length = read32be(src);
	return (int)length;
}

#undef MSG_HEADER_SIZE
