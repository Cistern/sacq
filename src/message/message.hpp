#pragma once

#include <cassert>
#include <cstring>
#include <cstdint>
#include <string>
#include <random>
#include <chrono>

#include "encoding.h"

enum MESSAGE_TYPE : uint8_t
{
	// Invalid
	MSG_INVALID,
	// Identification request
	MSG_IDENT_REQUEST,
	// Identification
	MSG_IDENT,
	// Active leader
	MSG_LEADER_ACTIVE,
	// Active leader acknowledgement
	MSG_LEADER_ACTIVE_ACK
};

inline
const char* MSG_STR(uint8_t type) {
	switch (type) {
	case MSG_IDENT_REQUEST: return "MSG_IDENT_REQUEST";
	case MSG_IDENT: return "MSG_IDENT";
	case MSG_LEADER_ACTIVE: return "MSG_LEADER_ACTIVE";
	case MSG_LEADER_ACTIVE_ACK: return "MSG_LEADER_ACTIVE_ACK";
	}

	return "MSG_INVALID";
}

enum MESSAGE_FLAG
{
	// Unused
};

// Initialize RNG
static std::mt19937_64 rng(
	std::chrono::system_clock::now().time_since_epoch().count());

class Message
{
public:
	Message()
	: type(MSG_INVALID)
	{
	}

	Message(uint8_t type)
	: type(type), flags(0), message_id(rng())
	{
	}

	Message(uint8_t type, uint8_t flags)
	: type(type), flags(flags), message_id(rng())
	{
	}

	int
	pack(uint8_t* dest, int dest_len) const;

	int
	packed_size() const;

	int
	unpack(uint8_t* src, int src_len);

	// Virtual methods to override
	virtual int
	body_size() const
	{
		return 0;
	}

	virtual int
	pack_body(uint8_t* dest, int dest_len) const = 0;

	virtual int
	unpack_body(uint8_t* src, int src_len) = 0;

public:
	int      source;
	uint8_t  type;
	uint8_t  flags; // Reserved
	uint64_t message_id;
	uint8_t  nonce_hash[32];
};

class IdentityRequest : public Message
{
public:
	IdentityRequest()
	: Message(MSG_IDENT_REQUEST), id(0), address("")
	{
	}

	IdentityRequest(uint64_t id, std::string& address)
	: Message(MSG_IDENT_REQUEST), id(id), address(address)
	{
	}

	inline int
	body_size() const
	{
		return 8 + 2 + address.size();
	}

	inline int
	pack_body(uint8_t* dest, int dest_len) const
	{
		if (dest_len < body_size()) {
			return -1;
		}
		write64le(id, dest);
		dest += 8;
		write16le(address.size(), dest);
		dest += 2;
		memcpy(dest, address.c_str(), address.size());
		return 0;
	}

	inline int
	unpack_body(uint8_t* src, int src_len)
	{
		if (src_len < body_size()) {
			return -1;
		}
		id = read64le(src);
		src += 8;
		uint16_t address_size = read16le(src);
		src += 2;
		if (src_len < 8 + 2 + address_size) {
			return -2;
		}
		address = std::string((const char*)src, address_size);
		return 0;
	}

public:
	uint64_t id;
	std::string address;
};

class IdentityMessage : public Message
{
public:
	IdentityMessage()
	: Message(MSG_IDENT), id(0), address("")
	{
	}

	IdentityMessage(uint64_t id, std::string& address)
	: Message(MSG_IDENT), id(id), address(address)
	{
	}

	inline int
	body_size() const
	{
		return 8 + 2 + address.size();
	}

	inline int
	pack_body(uint8_t* dest, int dest_len) const {
		if (dest_len < body_size()) {
			return -1;
		}
		write64le(id, dest);
		dest += 8;
		write16le(address.size(), dest);
		dest += 2;
		memcpy(dest, address.c_str(), address.size());
		return 0;
	}

	inline int
	unpack_body(uint8_t* src, int src_len)
	{
		if (src_len < body_size()) {
			return -1;
		}
		id = read64le(src);
		src += 8;
		uint16_t address_size = read16le(src);
		src += 2;
		if (src_len < 8 + 2 + address_size) {
			return -2;
		}
		address = std::string((const char*)src, address_size);
		return 0;
	}

public:
	uint64_t id;
	std::string address;
};

class LeaderActiveMessage : public Message
{
public:
	LeaderActiveMessage()
	: Message(MSG_LEADER_ACTIVE)
	, id(0)
	, seq(0)
	, round(0)
	, next(0)
	, next_content("")
	{
	}

	LeaderActiveMessage(uint64_t id, uint64_t seq, uint64_t round)
	: Message(MSG_LEADER_ACTIVE)
	, id(id)
	, seq(seq)
	, round(round)
	, next(0)
	, next_content("")
	{
	}

	LeaderActiveMessage(uint64_t id, uint64_t seq, uint64_t round,
		uint64_t next, std::string next_content)
	: Message(MSG_LEADER_ACTIVE)
	, id(id)
	, seq(seq)
	, round(round)
	, next(next)
	, next_content(next_content)
	{
	}

	inline int
	body_size() const
	{
		return 8+8+8+8+4+next_content.size();
	}

	inline int
	pack_body(uint8_t* dest, int dest_len) const
	{
		if (dest_len < body_size()) {
			return -1;
		}
		write64le(id, dest);
		dest += 8;
		write64le(seq, dest);
		dest += 8;
		write64le(round, dest);
		dest += 8;
		write64le(next, dest);
		dest += 8;
		write32le(next_content.size(), dest);
		dest += 4;
		memcpy(dest, next_content.c_str(), next_content.size());
		return 0;
	}

	inline int
	unpack_body(uint8_t* src, int src_len)
	{
		if (src_len < body_size()) {
			return -1;
		}
		id = read64le(src);
		src += 8;
		seq = read64le(src);
		src += 8;
		round = read64le(src);
		src += 8;
		next = read64le(src);
		src += 8;
		uint32_t next_content_size = read32le(src);
		src += 4;
		if (src_len < 8+8+8+8+4 + next_content_size) {
			return -2;
		}
		next_content = std::string((const char*)src, next_content_size);
		return 0;
	}

public:
	uint64_t    id;
	uint64_t    seq;
	uint64_t    round;
	uint64_t    next;
	std::string next_content;
};

class LeaderActiveAck : public Message
{
public:
	LeaderActiveAck()
	: Message(MSG_LEADER_ACTIVE_ACK)
	, id(0)
	, seq(0)
	, round(0)
	{
	}

	LeaderActiveAck(uint64_t id, uint64_t seq, uint64_t round)
	: Message(MSG_LEADER_ACTIVE_ACK)
	, id(id)
	, seq(seq)
	, round(round)
	{
	}

	inline int
	body_size() const
	{
		return 8+8+8;
	}

	inline int
	pack_body(uint8_t* dest, int dest_len) const
	{
		if (dest_len < body_size()) {
			return -1;
		}
		write64le(id, dest);
		dest += 8;
		write64le(seq, dest);
		dest += 8;
		write64le(round, dest);
		return 0;
	}

	inline int
	unpack_body(uint8_t* src, int src_len)
	{
		if (src_len < body_size()) {
			return -1;
		}
		id = read64le(src);
		src += 8;
		seq = read64le(src);
		src += 8;
		round = read64le(src);
		return 0;
	}

public:
	uint64_t id;
	uint64_t seq;
	uint64_t round;
};
