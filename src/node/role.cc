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
	if (m_leader_data->m_pending_round == 0) {
		// No pending round.
		if (ts - m_leader_data->m_last_broadcast < 50e6) {
			// Not enough time has passed to send a regular heartbeat.
			return;
		}

		// Do we have a majority of votes?
		if (m_leader_data->m_acks.size() >= m_cluster_size/2 /* assume a vote for ourselves */) {
			// Send another heartbeat.
			LeaderActiveMessage msg(m_id, ++m_seq, m_round);
			m_registry.broadcast(&msg);
			m_leader_data->m_last_broadcast = ts;
			m_leader_data->m_acks.clear();
			return;
		} else {
			// Did we lose leadership?
			if (ts - m_leader_data->m_last_broadcast > 300e6) {
				// Yes. Forfeit leadership.
				if (m_client_callbacks.lost_leadership != nullptr) {
					m_client_callbacks.lost_leadership(m_client_callbacks_data);
				}
				m_leader_data = nullptr;
				m_state = PotentialLeader;
				m_potential_leader_data = std::make_unique<PotentialLeaderData>();
				return;
			} else {
				// Not yet. Wait.
				return;
			}
		}
	}

	// We have a pending round. Did a majority ack it?
	auto pending_round_votes = 0;
	for (auto it = m_leader_data->m_acks.begin(); it != m_leader_data->m_acks.end(); ++it) {
		if (it->second == m_leader_data->m_pending_round) {
			pending_round_votes++;
		}
	}
	if (pending_round_votes >= m_cluster_size/2) {
		// Yes.
		m_leader_data->m_callback(0, m_leader_data->m_callback_data);
		m_leader_data->m_callback = nullptr;
		m_leader_data->m_callback_data = nullptr;
		m_round = m_leader_data->m_pending_round;
		m_leader_data->m_pending_round = 0;
		return;
	} else {
		// No. Did we wait long enough?
		if (ts - m_leader_data->m_last_broadcast > 300e6) {
			// Yes. Cancel append and forfeit leadership.
			cancel_append();
			if (m_client_callbacks.lost_leadership != nullptr) {
				m_client_callbacks.lost_leadership(m_client_callbacks_data);
			}
			m_leader_data = nullptr;
			m_state = PotentialLeader;
			m_potential_leader_data = std::make_unique<PotentialLeaderData>();
		} else {
			// Not yet. Wait.
			return;
		}
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
			return;
		}

		// Try again.
		++m_seq;
		m_potential_leader_data->m_acks.clear();
		LeaderActiveMessage msg(m_id, m_seq, m_round);
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
		auto previous_leader = m_follower_data->m_current_leader;
		// Leader hasn't been active for over 1000 ms
		m_follower_data = nullptr;
		m_state = PotentialLeader;
		m_potential_leader_data = std::make_unique<PotentialLeaderData>();
		if (m_client_callbacks.on_leader_change != nullptr &&
			// Only invoke callback if there was a previous leader.
			previous_leader != 0) {
			m_client_callbacks.on_leader_change(0, m_client_callbacks_data);
		}
	}
}

void
Role :: handle_leader_active(uint64_t ts, const LeaderActiveMessage& msg) {
	if (msg.seq < m_seq) {
		// Ignore out-of-date heartbeat.
		return;
	}
	m_seq = msg.seq;

	if (m_state != Follower) {
		if (msg.id < m_id) {
			// Other node has more authority. Drop down to follower state.
			if (m_state == Leader) {
				// Cancel append if we have one.
				cancel_append();
				if (m_client_callbacks.lost_leadership != nullptr) {
					m_client_callbacks.lost_leadership(m_client_callbacks_data);
				}
				m_leader_data = nullptr;
			}
			auto new_leader_id = msg.id;
			drop_leadership(new_leader_id);
		}
	}

	if (m_id < msg.id) {
		// We're more authoritative.
		// Ignore this message.
		return;
	}

	if (m_follower_data->m_pending_round != 0) {
		// We haven't confirmed a previous append.
		if (msg.round >= m_follower_data->m_pending_round) {
			// Leader has moved on. Drop our pending append.
			m_follower_data->m_pending_round = 0;
		} else {
			// Still valid. Ignore everything until we confirm.
			return;
		}
	}

	if (m_follower_data->m_current_leader > msg.id || m_follower_data->m_current_leader == 0) {
		// Our current leader is less authoritative. Replace.
		m_follower_data->m_current_leader = msg.id;
		if (m_client_callbacks.on_leader_change != nullptr) {
			m_client_callbacks.on_leader_change(msg.id, m_client_callbacks_data);
		}
		m_follower_data->m_pending_round = 0;
	} else if (m_follower_data->m_current_leader < msg.id) {
		// Less authoritative than the current leader.
		// Ignore this message.
		return;
	}

	if (msg.round > m_round) {
		m_round = msg.round;
	}

	if (msg.next != 0) {
		// Append message
		if (m_client_callbacks.on_append != nullptr) {
			m_client_callbacks.on_append(msg.next, msg.next_content.c_str(),
				msg.next_content.size(), m_client_callbacks_data);
			m_follower_data->m_last_leader_active = ts;
			m_follower_data->m_pending_round = msg.next;
			return;
		}
	}

	// Normal heartbeat
	// Send ack
	LeaderActiveAck ack(m_id, m_seq, m_round);
	m_registry.send_to_id(msg.id, &ack);
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

	if (msg.seq != m_seq) {
		// message is too old
		return;
	}

	if (m_state == Leader) {
		m_leader_data->m_acks[msg.id] = msg.round;
		periodic_leader(ts);
	} else {
		m_potential_leader_data->m_acks[msg.id] = msg.round;
	}
}
