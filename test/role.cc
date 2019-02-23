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
	REQUIRE( broadcasted->seq == 1 );
}

TEST_CASE( "Follower responds to a leader heartbeat", "[role]" ) {
	TestRegistry reg;

	std::unique_ptr<LeaderActiveAck> ack_msg;
	reg.m_send_to_id = std::function<void(int, const Message*)>([&](uint64_t id, const Message* msg) {
		REQUIRE( msg->type == MSG_LEADER_ACTIVE_ACK );
		REQUIRE( id == 1 );

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
	leader_active->seq = 1;
	role.handle_leader_active(ts, *leader_active);

	// Role should still be Follower.

	REQUIRE( role.state() == Follower );
	REQUIRE( role.round() == 0 );

	REQUIRE( ack_msg != nullptr );
	REQUIRE( ack_msg->id == 2 );
	REQUIRE( ack_msg->seq == 1 );
	REQUIRE( ack_msg->round == 0 );
}

TEST_CASE( "Follower ignores other leader heartbeat", "[role]" ) {
	TestRegistry reg;

	std::unique_ptr<LeaderActiveAck> ack_msg;
	reg.m_send_to_id = std::function<void(int, const Message*)>([&](uint64_t id, const Message* msg) {
		REQUIRE( msg->type == MSG_LEADER_ACTIVE_ACK );
		REQUIRE( id == 1 );

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
	leader_active->seq = 1;
	role.handle_leader_active(ts, *leader_active);

	// Role should still be Follower.

	REQUIRE( role.state() == Follower );
	REQUIRE( role.round() == 0 );

	REQUIRE( ack_msg != nullptr );
	REQUIRE( ack_msg->id == 2 );
	REQUIRE( ack_msg->seq == 1 );
	REQUIRE( ack_msg->round == 0 );

	ack_msg = nullptr;
	leader_active->id = 2;
	leader_active->source = 2;
	role.handle_leader_active(ts, *leader_active);

	REQUIRE( ack_msg == nullptr );
}


TEST_CASE( "on_leader_change is called when the current leader is lost", "[role]" ) {
	TestRegistry reg;
	Role role(reg, 2, 2);
	ab_callbacks_t callbacks = {};

	struct TestData {
		bool called_on_leader_change;
		uint64_t current_leader;
	};

	TestData test_data = {false, 0};

	auto called_on_leader_change = false;
	callbacks.on_leader_change = [](uint64_t current_leader, void* data) {
		((TestData*)data)->called_on_leader_change = true;
		((TestData*)data)->current_leader = current_leader;
	};
	role.set_callbacks(callbacks, &test_data);

	uint64_t ts = 1e9;
	role.periodic(ts);
	// Tick 2 seconds
	ts += 1e9;
	role.periodic(ts);
	ts += 1e9;
	role.periodic(ts);

	// Role should be PotentialLeader.
	REQUIRE( role.state() == PotentialLeader );
	REQUIRE( test_data.called_on_leader_change == false );

	auto leader_active = std::make_unique<LeaderActiveMessage>();
	leader_active->id = 1;
	leader_active->source = 1;
	role.handle_leader_active(ts, *leader_active);

	// Role should be Follower.
	REQUIRE( role.state() == Follower );
	REQUIRE( role.round() == 0 );
	REQUIRE( role.current_leader() == 1 );

	REQUIRE( test_data.called_on_leader_change == true );
	REQUIRE( test_data.current_leader == 1 );

	// Reset
	test_data.called_on_leader_change = false;

	// Tick 2 seconds
	ts += 1e9;
	role.periodic(ts);
	ts += 1e9;
	role.periodic(ts);

	// current_leader should be 0 now.
	REQUIRE( test_data.called_on_leader_change == true );
	REQUIRE( test_data.current_leader == 0 );
	REQUIRE( role.state() == PotentialLeader );
	REQUIRE( role.current_leader() == 0 );
}
