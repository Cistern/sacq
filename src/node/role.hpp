#pragma once

#include <map>
#include <memory>
#include <functional>
#include <unordered_map>

#include "ab.h"
#include "message/message.hpp"
#include "peer_registry.hpp"
#include "registry.hpp"

enum State
{
	Leader,
	PotentialLeader,
	Follower
};

struct LeaderData
{
	LeaderData()
	: m_last_broadcast(0)
	, m_pending_round(0)
	{
	}

	uint64_t                               m_last_broadcast;
	std::function<void(int, void*)>        m_callback;
	void*                                  m_callback_data;
	uint64_t                               m_pending_round;
	std::unordered_map<uint64_t, uint64_t> m_acks;
}; // LeaderData

struct PotentialLeaderData
{
	PotentialLeaderData()
	: m_last_broadcast(0)
	{
	}

	std::unordered_map<uint64_t, uint64_t> m_acks;
	uint64_t                               m_last_broadcast;
}; // PotentialLeaderData

struct FollowerData
{
	FollowerData()
	: m_current_leader(0)
	, m_last_leader_active(0)
	, m_pending_round(0)
	{
	}

	uint64_t m_current_leader;
	uint64_t m_last_leader_active;
	uint64_t m_pending_round;
}; // FollowerData

class Role
{
public:
	Role(Registry& registry, uint64_t id, int cluster_size)
	: m_registry(registry)
	, m_state(Follower)
	, m_follower_data(std::make_unique<FollowerData>())
	, m_id(id)
	, m_seq(0)
	, m_cluster_size(cluster_size)
	, m_round(0)
	, m_client_callbacks({
		.on_append = nullptr,
		.gained_leadership = nullptr,
		.lost_leadership = nullptr,
		.on_leader_change = nullptr
	})
	, m_client_callbacks_data(nullptr)
	{
	}

	void
	periodic(uint64_t ts);

	void
	handle_leader_active(uint64_t ts, const LeaderActiveMessage& msg);

	void
	handle_leader_active_ack(uint64_t ts, const LeaderActiveAck& msg);

	void
	client_confirm_append(uint64_t round)
	{
		if (m_state != Follower) {
			return;
		}

		if (m_follower_data->m_pending_round != round) {
			return;
		}

		// Send ack.
		LeaderActiveAck ack(m_id, m_seq, round);
		m_registry.send_to_id(m_follower_data->m_current_leader, &ack);
		m_follower_data->m_pending_round = 0;
	}

	void
	send_append(std::string append_content, std::function<void(int, void*)> cb, void* data)
	{
		if (m_state != Leader) {
			// Not a leader so this is an invalid operation.
			cb(-1, data);
			return;
		}
		if (m_leader_data->m_callback != nullptr) {
			// There's already a pending append.
			cb(-2, data);
			return;
		}
		// Set up callbacks for the append.
		m_leader_data->m_callback = cb;
		m_leader_data->m_callback_data = data;
		m_leader_data->m_pending_round = m_round+1;

		// Broadcast it.
		LeaderActiveMessage msg(m_id, ++m_seq, m_round, m_round+1, append_content);
		m_registry.broadcast(&msg);
		m_leader_data->m_last_broadcast = uv_hrtime();
		m_leader_data->m_acks.clear();
	}

	void
	cancel_append()
	{
		if (m_leader_data->m_callback != nullptr) {
			m_leader_data->m_callback(-1, m_leader_data->m_callback_data);
			m_leader_data->m_callback = nullptr;
			m_leader_data->m_callback_data = nullptr;
		}
	}

	void
	drop_leadership(uint64_t new_leader_id)
	{
		m_potential_leader_data = nullptr;
		m_leader_data = nullptr;
		m_follower_data = std::make_unique<FollowerData>();
		m_state = Follower;
		m_follower_data->m_current_leader = new_leader_id;
		if (m_client_callbacks.on_leader_change != nullptr) {
			m_client_callbacks.on_leader_change(new_leader_id, m_client_callbacks_data);
		}
	}

	void
	set_callbacks(ab_callbacks_t callbacks, void* callbacks_data)
	{
		m_client_callbacks = callbacks;
		m_client_callbacks_data = callbacks_data;
	}

	State
	state() const
	{
		return m_state;
	}

	uint64_t
	round() const
	{
		return m_round;
	}

	uint64_t
	current_leader() const
	{
		if (m_state == Follower) {
			return m_follower_data->m_current_leader;
		}
		return 0;
	}

private:
	void
	periodic_leader(uint64_t ts);

	void
	periodic_potential_leader(uint64_t ts);

	void
	periodic_follower(uint64_t ts);

private:
	Registry&     m_registry;
	uint64_t      m_id;
	uint64_t      m_seq;
	int           m_cluster_size;
	State         m_state;
	uint64_t      m_round;

	// Per-state data
	std::unique_ptr<LeaderData>          m_leader_data;
	std::unique_ptr<PotentialLeaderData> m_potential_leader_data;
	std::unique_ptr<FollowerData>        m_follower_data;

	ab_callbacks_t  m_client_callbacks;
	void*           m_client_callbacks_data;
}; // Role
