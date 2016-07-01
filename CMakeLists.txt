cmake_minimum_required(VERSION 2.8)

project(libab)
SET(CMAKE_C_FLAGS "-fPIC -march=native")
SET(CMAKE_CXX_FLAGS "-std=c++14 -fPIC -march=native")

include_directories(
	src
	include
	dep
	dep/libuv/include
)

SET(BUILD_TESTING OFF)
add_subdirectory(dep/cryptopp)
add_subdirectory(dep/libuv)

add_executable(main
	src/main.cc
)

add_executable(abtest
	test/catch.cc
	test/factorial.cc
)

add_library(ab SHARED
	src/message/message.cc
	src/peer/peer.cc
	src/node/node.cc
	src/node/role.cc
	src/c.cc
)

target_link_libraries(ab uv)
target_link_libraries(ab cryptopp-static)
target_link_libraries(main ab)

install(TARGETS ab DESTINATION lib)
install(FILES include/ab.h DESTINATION include)

add_test(NAME abtest COMMAND abtest)

enable_testing(true)
