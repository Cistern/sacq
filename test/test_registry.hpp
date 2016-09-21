#pragma once

#include "node/registry.hpp"

class TestRegistry : public Registry
{
public:
	void
	send_to_id(uint64_t id, const Message* msg)
	{

	}

	void
	send_to_index(int index, const Message* msg)
	{

	}

	void
	broadcast(const Message* msg)
	{

	}
}; // Registry
