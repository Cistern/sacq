#include <memory>
#include <cstring>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <tweetnacl/tweetnacl.h>

#include "message.hpp"
#include "codec.hpp"

/**
 * A message header has the following fields:
 * - length (of the entire message, including the payload) (4 bytes)
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
const int PAYLOAD_OFFSET = TYPE_OFFSET;

int
Message :: pack(uint8_t* dest, int dest_len) const {
	int length = packed_size();
	if (dest_len < length) {
		return -1;
	}

	write32le(length, dest);
	dest += 4;
	dest_len -= 4;
	memcpy(dest, nonce_hash, NONCE_HASH_SIZE);
	dest += NONCE_HASH_SIZE;
	dest_len -= NONCE_HASH_SIZE;
	write8le(type, dest);
	dest++;
	dest_len--;
	write8le(flags, dest);
	dest++;
	dest_len--;
	write64le(message_id, dest);
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

	uint32_t length = read32le(src);
	src += 4;
	if (src_len < length) {
		return -2;
	}
	memcpy(nonce_hash, src, NONCE_HASH_SIZE);
	src += NONCE_HASH_SIZE;
	type = read8le(src);
	src++;
	flags = read8le(src);
	src++;
	message_id = read64le(src);
	src += 8;
	unpack_body(src, length - MSG_HEADER_SIZE);
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
			src + PAYLOAD_OFFSET,
			src_len - PAYLOAD_OFFSET);
		if (memcmp(h, src + NONCE_HASH_OFFSET, NONCE_HASH_SIZE) != 0) {
			return -2;
		}
	} else {
		uint8_t* nonce = src+NONCE_HASH_OFFSET;
		int payload_size = src_len - PAYLOAD_OFFSET;
		int clen = payload_size + crypto_secretbox_BOXZEROBYTES;
		std::vector<uint8_t> c(clen, 0);
		std::vector<uint8_t> message_data(clen, 0);

		for (int i = 0; i < payload_size; ++i) {
			c[i + crypto_secretbox_BOXZEROBYTES] = src[i + PAYLOAD_OFFSET];
		}

		int i = crypto_secretbox_open(
			message_data.data(),
			c.data(),
			clen,
			nonce,
			(uint8_t*)m_key.data());
		if (i < 0) {
			return -1;
		}

		for (int i = 0; i < payload_size - MSG_PADDING_SIZE; ++i) {
			src[i + PAYLOAD_OFFSET] = message_data[i + crypto_secretbox_ZEROBYTES];
		}
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
			dest+PAYLOAD_OFFSET,
			m->packed_size()-PAYLOAD_OFFSET);
		memcpy(dest + NONCE_HASH_OFFSET, h, NONCE_HASH_SIZE);
	} else {
		// Initialize nonce.
		uint8_t n[NONCE_HASH_SIZE];
		randombytes(n, NONCE_HASH_SIZE);
		for (int i = 0; i < NONCE_HASH_SIZE; i++) {
			dest[i + NONCE_HASH_OFFSET] = n[i];
		}
		int payload_size = m->packed_size() - PAYLOAD_OFFSET - MSG_PADDING_SIZE;
		int mlen = payload_size + crypto_secretbox_ZEROBYTES;
		std::vector<uint8_t> message_data(mlen, 0);
		std::vector<uint8_t> c(mlen, 0);
		for (int i = 0; i < payload_size; ++i) {
			message_data[i + crypto_secretbox_ZEROBYTES] = dest[i + PAYLOAD_OFFSET];
		}

		crypto_secretbox(
			c.data(),
			message_data.data(),
			mlen,
			n,
			(uint8_t*)m_key.data());

		assert(mlen - crypto_secretbox_BOXZEROBYTES == payload_size + MSG_PADDING_SIZE);
		for (int i = 0; i < mlen-crypto_secretbox_BOXZEROBYTES; ++i) {
			dest[i + PAYLOAD_OFFSET] = c[i + crypto_secretbox_BOXZEROBYTES];
		}
	}
	return 0;
}

int
Codec :: decode_message_length(uint8_t* src, int src_len) {
	if (src_len < 4) {
		return -1;
	}
	uint32_t length = read32le(src);
	return (int)length;
}

#undef MSG_HEADER_SIZE
