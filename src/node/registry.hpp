#pragma once

#include "message/message.hpp"

class Registry
{
public:
	virtual void
	send_to_id(uint64_t id, const Message* msg) = 0;

	virtual void
	send_to_index(int index, const Message* msg) = 0;

	virtual void
	broadcast(const Message* msg) = 0;

}; // Registry
