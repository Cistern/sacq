#pragma once

#include <map>
#include <memory>
#include <functional>
#include <unordered_map>
#include <glog/logging.h>

#include "ab.h"
#include "message/message.hpp"
#include "peer_registry.hpp"

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
	{
	}

	uint64_t                                            m_last_broadcast;
	std::function<void(int, uint64_t, uint64_t, void*)> m_callback;
	void*                                               m_callback_data;
	uint64_t                                            m_pending_commit;
	std::unordered_map<uint64_t, uint64_t>              m_acks;
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
	, m_pending_commit(0)
	{
	}

	uint64_t m_current_leader;
	uint64_t m_last_leader_active;
	uint64_t m_pending_commit;
}; // FollowerData

class Role
{
public:
	Role(PeerRegistry& registry, uint64_t id, int cluster_size)
	: m_registry(registry)
	, m_state(Follower)
	, m_follower_data(std::make_unique<FollowerData>())
	, m_id(id)
	, m_seq(0)
	, m_cluster_size(cluster_size)
	, m_commit(0)
	, m_round(0)
	{
	}

	void
	periodic(uint64_t ts);

	void
	handle_leader_active(uint64_t ts, const LeaderActiveMessage& msg);

	void
	handle_leader_active_ack(uint64_t ts, const LeaderActiveAck& msg);

	void
	client_confirm_append(uint64_t round, uint64_t commit)
	{
		if (m_state != Follower) {
			return;
		}

		// Send ack.
		LeaderActiveAck ack(m_id, m_seq, commit, round);
		m_registry.send_to_id(m_follower_data->m_current_leader, &ack);
	}

	void
	send_append(std::string append_content, std::function<void(int, uint64_t, uint64_t, void*)> cb, void* data)
	{
		if (m_state != Leader) {
			cb(-1, 0, 0, data);
			return;
		}
		if (m_leader_data->m_callback != nullptr) {
			// There's already a pending append.
			cb(-2, 0, 0, data);
			return;
		}
		m_leader_data->m_callback = cb;
		m_leader_data->m_callback_data = data;
		m_leader_data->m_pending_commit = m_commit+1;
		LeaderActiveMessage msg(m_id, ++m_seq, m_commit, m_round, m_commit+1, append_content);
		m_registry.broadcast(&msg);
		m_leader_data->m_last_broadcast = uv_hrtime();
		m_leader_data->m_acks.clear();
	}

	void
	set_callbacks(ab_callbacks_t callbacks, void* callbacks_data)
	{
		m_client_callbacks = callbacks;
		m_client_callbacks_data = callbacks_data;
	}

	void
	set_committed(uint64_t round, uint64_t commit)
	{
		m_round = round;
		m_commit = commit;
	}

private:
	void
	periodic_leader(uint64_t ts);

	void
	periodic_potential_leader(uint64_t ts);

	void
	periodic_follower(uint64_t ts);

private:
	PeerRegistry& m_registry;
	uint64_t      m_id;
	uint64_t      m_seq;
	int           m_cluster_size;
	State         m_state;
	uint64_t      m_commit;
	uint64_t      m_round;

	// Per-state data
	std::unique_ptr<LeaderData>          m_leader_data;
	std::unique_ptr<PotentialLeaderData> m_potential_leader_data;
	std::unique_ptr<FollowerData>        m_follower_data;

	ab_callbacks_t  m_client_callbacks;
	void*           m_client_callbacks_data;
}; // Role
