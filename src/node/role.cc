#include "role.hpp"

void
Role :: periodic(uint64_t ts) {
	switch (m_state) {
	case Leader:
		periodic_leader(ts);
		break;
	case PotentialLeader:
		periodic_potential_leader(ts);
		break;
	case Follower:
		periodic_follower(ts);
		break;
	}
}

void
Role :: periodic_leader(uint64_t ts) {
	if (m_leader_data->m_pending_commit == 0) {
		// No pending commit.
		if (ts - m_leader_data->m_last_broadcast < 50e6) {
			// Not enough time has passed to send a regular heartbeat.
			return;
		}
	}
	if (m_leader_data->m_acks.size() >= m_cluster_size/2) {
		uint64_t max_commit = m_commit;
		for (auto it = m_leader_data->m_acks.begin(); it != m_leader_data->m_acks.end(); ++it) {
			if (it->second > max_commit) {
				max_commit = it->second;
			}
		}
		if (max_commit == m_leader_data->m_pending_commit && max_commit > 0) {
			if (m_leader_data->m_callback != nullptr) {
				// Append was confirmed by a majority.
				m_leader_data->m_callback(0, m_round, m_leader_data->m_pending_commit, m_leader_data->m_callback_data);
				m_leader_data->m_callback = nullptr;
				m_leader_data->m_callback_data = nullptr;
			} else {
				// Majority have acknowledged the commit
				// Report that to the client
				if (m_client_callbacks.on_commit != nullptr) {
					m_client_callbacks.on_commit(m_round, m_leader_data->m_pending_commit, m_client_callbacks_data);
				}
				m_commit = m_leader_data->m_pending_commit;
				m_leader_data->m_pending_commit = 0;
			}
		} else {
			if (m_leader_data->m_pending_commit == 0 && max_commit > m_commit) {
				m_commit = max_commit;
				if (m_client_callbacks.on_commit != nullptr) {
					m_client_callbacks.on_commit(m_round, m_commit, m_client_callbacks_data);
				}
			}
		}

		++m_seq;
		auto commit = m_commit;
		if (m_leader_data->m_pending_commit > 0) {
			commit = m_leader_data->m_pending_commit;
		}
		LeaderActiveMessage msg(m_id, m_seq, commit, m_round);
		m_registry.broadcast(&msg);
		m_leader_data->m_last_broadcast = ts;
		m_leader_data->m_acks.clear();
		return;
	}
	if (ts - m_leader_data->m_last_broadcast > 300e6) {
		// It's been over 300 ms since the last broadcast.
		// Didn't get a majority. We're not a leader anymore.
		if (m_client_callbacks.lost_leadership != nullptr) {
			m_client_callbacks.lost_leadership(m_client_callbacks_data);
		}
		if (m_leader_data->m_callback != nullptr) {
			// Append was not confirmed by a majority.
			m_leader_data->m_callback(-1, 0, 0, m_leader_data->m_callback_data);
			m_leader_data->m_callback = nullptr;
			m_leader_data->m_callback_data = nullptr;
			m_leader_data->m_pending_commit = 0;
		}
		m_leader_data = nullptr;
		m_state = PotentialLeader;
		m_potential_leader_data = std::make_unique<PotentialLeaderData>();
		return;
	}
}

void
Role :: periodic_potential_leader(uint64_t ts) {
	if (ts - m_potential_leader_data->m_last_broadcast > 300e6) {
		// It's been over 300 ms since the last broadcast.
		if (m_potential_leader_data->m_acks.size() >= m_cluster_size/2) {
			// Got a majority. We're now a leader.
			if (m_client_callbacks.gained_leadership != nullptr) {
				m_client_callbacks.gained_leadership(m_client_callbacks_data);
			}
			m_leader_data = std::make_unique<LeaderData>();
			m_leader_data->m_last_broadcast = m_potential_leader_data->m_last_broadcast;
			m_leader_data->m_acks = m_potential_leader_data->m_acks;
			m_potential_leader_data = nullptr;
			m_state = Leader;
			m_round++;
			return;
		}

		// Try again.
		++m_seq;
		m_potential_leader_data->m_acks.clear();
		LeaderActiveMessage msg(m_id, m_seq, m_commit, m_round);
		m_registry.broadcast(&msg);
		m_potential_leader_data->m_last_broadcast = ts;
	}
}

void
Role :: periodic_follower(uint64_t ts) {
	if (m_follower_data->m_last_leader_active == 0) {
		// Initialize m_last_leader_active.
		m_follower_data->m_last_leader_active = ts;
		return;
	}

	if (ts - m_follower_data->m_last_leader_active > 1000e6) {
		// Leader hasn't been active for over 1000 ms
		m_follower_data = nullptr;
		m_state = PotentialLeader;
		m_potential_leader_data = std::make_unique<PotentialLeaderData>();
	}
}

void
Role :: handle_leader_active(uint64_t ts, const LeaderActiveMessage& msg) {
	if (m_state != Follower) {
		if (msg.id < m_id) {
			// Other node has more authority. Drop down to follower state.
			if (m_state == Leader) {
				if (m_leader_data->m_callback != nullptr) {
					// Append was not confirmed by a majority.
					m_leader_data->m_callback(-1, 0, 0, m_leader_data->m_callback_data);
					m_leader_data->m_callback = nullptr;
					m_leader_data->m_callback_data = nullptr;
				}
				if (m_client_callbacks.lost_leadership != nullptr) {
					m_client_callbacks.lost_leadership(m_client_callbacks_data);
				}
				m_leader_data = nullptr;
			}
			m_follower_data = std::make_unique<FollowerData>();
			m_potential_leader_data = nullptr;
			m_state = Follower;
			m_follower_data->m_current_leader = msg.id;
			if (m_client_callbacks.on_leader_change != nullptr) {
				m_client_callbacks.on_leader_change(msg.id, m_client_callbacks_data);
			}
		} else {
			if (msg.round > m_round) {
				m_round = msg.round;
			}
			return;
		}
	}

	if (m_id < msg.id) {
		// We're more authoritative.
		if (msg.round > m_round) {
			m_round = msg.round;
		}
		m_follower_data->m_pending_commit = 0;
		return;
	}

	if (m_follower_data->m_current_leader > msg.id) {
		// Our current leader is less authoritative. Replace.
		m_follower_data->m_current_leader = msg.id;
		if (m_client_callbacks.on_leader_change != nullptr) {
			m_client_callbacks.on_leader_change(msg.id, m_client_callbacks_data);
		}
		m_follower_data->m_pending_commit = 0;
	} else {
		if (m_follower_data->m_current_leader < msg.id) {
			return;
		}
	}

	if (msg.round > m_round) {
		m_round = msg.round;
	}

	if (msg.committed > m_commit) {
		if (m_follower_data->m_pending_commit == msg.committed) {
			m_follower_data->m_pending_commit = 0;
			LeaderActiveAck ack(m_id, msg.seq, msg.committed, m_round);
			m_registry.send_to_index(msg.source, &ack);
			if (m_follower_data->m_current_leader != msg.id) {
				if (m_client_callbacks.on_leader_change != nullptr) {
					m_client_callbacks.on_leader_change(msg.id, m_client_callbacks_data);
				}
			}
			m_follower_data->m_current_leader = msg.id;
			m_follower_data->m_last_leader_active = ts;
			return;
		}
		m_commit = msg.committed;
		if (m_client_callbacks.on_commit != nullptr) {
			m_client_callbacks.on_commit(m_round, m_commit, m_client_callbacks_data);
		}
	}

	if (msg.next != 0) {
		if (m_client_callbacks.on_append != nullptr) {
			m_seq = msg.seq;
			m_client_callbacks.on_append(m_round, msg.next, msg.next_content.c_str(),
				msg.next_content.size(), m_client_callbacks_data);
			m_follower_data->m_last_leader_active = ts;
			m_follower_data->m_pending_commit = msg.next;
			return;
		}
	}

	// Send ack.
	LeaderActiveAck ack(m_id, msg.seq, m_commit, m_round);
	m_registry.send_to_index(msg.source, &ack);
	if (m_follower_data->m_current_leader != msg.id) {
		if (m_client_callbacks.on_leader_change != nullptr) {
			m_client_callbacks.on_leader_change(msg.id, m_client_callbacks_data);
		}
	}
	m_follower_data->m_current_leader = msg.id;
	m_follower_data->m_last_leader_active = ts;
}

void
Role :: handle_leader_active_ack(uint64_t ts, const LeaderActiveAck& msg) {
	if (m_state == Follower) {
		// Nothing to do.
		return;
	}

	if (msg.round > m_round) {
		m_round = msg.round;
	}

	if (msg.seq != m_seq) {
		// message is too old
		return;
	}

	if (m_state == Leader) {
		m_leader_data->m_acks[msg.id] = msg.committed;
		periodic_leader(ts);
	} else {
		m_potential_leader_data->m_acks[msg.id] = msg.committed;
	}
}
