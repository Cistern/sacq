#include <catch.hpp>

#include "node/role.hpp"
#include "test_registry.hpp"

TEST_CASE( "Default role values are valid", "[role]" ) {
    TestRegistry reg;

	Role role(reg, 1, 2);

	REQUIRE( role.state() == Follower );
	REQUIRE( role.commit() == 0 );
	REQUIRE( role.round() == 0 );
}
