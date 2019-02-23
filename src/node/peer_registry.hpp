#pragma once

#include <map>
#include <unordered_map>
#include <memory>
#include <atomic>

#include "peer/peer.hpp"
#include "node/registry.hpp"

class PeerRegistry : public Registry
{
	using shared_peer = std::shared_ptr<Peer>;

public:
	PeerRegistry(uint64_t id)
	: m_id(id)
	{
	}

	void
	register_peer(int index, shared_peer peer)
	{
		peer->set_index(index);
		m_peers[index] = peer;
	}

	void
	set_identity(const int index, const uint64_t id, const std::string& address)
	{
		Peer* peer;
		try {
			auto peer_ptr = m_peers.at(index);
			peer = peer_ptr.get();
		} catch (...) {
			return;
		}
		peer->set_identity(id, address);
		// Check if another index has the same ID or address. If so,
		// update the other one.
		for (auto i = std::begin(m_peers); i != std::end(m_peers); ++i) {
			auto p = i->second;
			if (i->first < index && (p->id() == id || p->address() == address)) {
				*p = std::move(*peer);
				break;
			}
		}
	}

	void
	send_to_index(int index, const Message* msg)
	{
		auto peer = m_peers.find(index);
		if (peer != m_peers.end()) {
			peer->second->send(msg);
		}
	}

	void
	send_to_id(uint64_t id, const Message* msg)
	{
		for (auto i = std::begin(m_peers); i != std::end(m_peers); ++i) {
			if (i->second->id() == id) {
				i->second->send(msg);
			}
		}
	}

	void
	broadcast(const Message* msg)
	{
		for (auto i = std::begin(m_peers); i != std::end(m_peers); ++i) {
			if (i->second->id() < m_id) {
				// TODO: Don't broadcast to nodes more authoritative.
			}
			i->second->send(msg);
		}
	}

	void
	cleanup()
	{
		for (auto i = std::begin(m_peers); i != std::end(m_peers); ) {
			if (i->second == nullptr) {
				i = m_peers.erase(i);
			} else if (i->second->done()) {
				i = m_peers.erase(i);
			} else {
				++i;
			}
		}
	}

private:
	uint64_t                             m_id;
	std::unordered_map<int, shared_peer> m_peers;
}; // PeerRegistry
