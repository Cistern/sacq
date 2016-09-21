#pragma once

#include <functional>

#include "node/registry.hpp"

class TestRegistry : public Registry
{
public:
	void
	send_to_id(uint64_t id, const Message* msg)
	{
		if (m_send_to_id) {
			m_send_to_id(id, msg);
		}
	}

	void
	send_to_index(int index, const Message* msg)
	{
		if (m_send_to_index) {
			m_send_to_index(index, msg);
		}
	}

	void
	broadcast(const Message* msg)
	{
		if (m_broadcast) {
			m_broadcast(msg);
		}
	}

public:
	std::function<void(uint64_t, const Message*)> m_send_to_id;
	std::function<void(int, const Message*)>      m_send_to_index;
	std::function<void(const Message*)>           m_broadcast;
}; // Registry
