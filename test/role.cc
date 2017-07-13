#include <catch.hpp>

#include "node/role.hpp"
#include "test_registry.hpp"

TEST_CASE( "Default role values are valid", "[role]" ) {
	TestRegistry reg;

	Role role(reg, 1, 2);

	REQUIRE( role.state() == Follower );
	REQUIRE( role.round() == 0 );
}

TEST_CASE( "Role becomes PotentialLeader after timeout", "[role]" ) {
	TestRegistry reg;

	std::unique_ptr<LeaderActiveMessage> broadcasted;
	reg.m_broadcast = std::function<void(const Message*)>([&](const Message* msg) {
		REQUIRE( msg->type == MSG_LEADER_ACTIVE );

		auto leader_ack = std::make_unique<LeaderActiveMessage>();
		*leader_ack = *static_cast<const LeaderActiveMessage*>(msg);
		broadcasted = std::move(leader_ack);
	});

	Role role(reg, 1, 2);

	uint64_t ts = 1e9;
	role.periodic(ts);
	ts += 1e9; // 1 second
	role.periodic(ts);
	ts += 1e9; // 1 second
	role.periodic(ts);
	ts += 1e9; // 1 second
	role.periodic(ts);

	// Role should be PotentialLeader now.

	REQUIRE( role.state() == PotentialLeader );
	REQUIRE( role.round() == 0 );

	REQUIRE( broadcasted != nullptr );
	REQUIRE( broadcasted->seq == 1);
}

TEST_CASE( "Follower responds to a leader heartbeat", "[role]" ) {
	TestRegistry reg;

	std::unique_ptr<LeaderActiveAck> ack_msg;
	reg.m_send_to_index = std::function<void(int, const Message*)>([&](int index, const Message* msg) {
		REQUIRE( msg->type == MSG_LEADER_ACTIVE_ACK );

		auto leader_ack = std::make_unique<LeaderActiveAck>();
		*leader_ack = *static_cast<const LeaderActiveAck*>(msg);
		ack_msg = std::move(leader_ack);
	});

	Role role(reg, 2, 2);

	uint64_t ts = 1e9;
	role.periodic(ts);

	auto leader_active = std::make_unique<LeaderActiveMessage>();
	leader_active->id = 1;
	leader_active->source = 1;
	role.handle_leader_active(ts, *leader_active);

	// Role should still be Follower.

	REQUIRE( role.state() == Follower );
	REQUIRE( role.round() == 0 );

	REQUIRE( ack_msg != nullptr );
	REQUIRE( ack_msg->id == 2 );
	REQUIRE( ack_msg->seq == 0 );
	REQUIRE( ack_msg->round == 0 );
}

TEST_CASE( "Follower ignores other leader heartbeat", "[role]" ) {
	TestRegistry reg;

	std::unique_ptr<LeaderActiveAck> ack_msg;
	reg.m_send_to_index = std::function<void(int, const Message*)>([&](int index, const Message* msg) {
		REQUIRE( msg->type == MSG_LEADER_ACTIVE_ACK );

		auto leader_ack = std::make_unique<LeaderActiveAck>();
		*leader_ack = *static_cast<const LeaderActiveAck*>(msg);
		ack_msg = std::move(leader_ack);
	});

	Role role(reg, 2, 2);

	uint64_t ts = 1e9;
	role.periodic(ts);

	auto leader_active = std::make_unique<LeaderActiveMessage>();
	leader_active->id = 1;
	leader_active->source = 1;
	role.handle_leader_active(ts, *leader_active);

	// Role should still be Follower.

	REQUIRE( role.state() == Follower );
	REQUIRE( role.round() == 0 );

	REQUIRE( ack_msg != nullptr );
	REQUIRE( ack_msg->id == 2 );
	REQUIRE( ack_msg->seq == 0 );
	REQUIRE( ack_msg->round == 0 );

	ack_msg = nullptr;
	leader_active->id = 2;
	leader_active->source = 2;
	role.handle_leader_active(ts, *leader_active);

	REQUIRE( ack_msg == nullptr );
}
