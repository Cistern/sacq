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
