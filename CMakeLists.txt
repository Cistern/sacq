cmake_minimum_required(VERSION 3.0)

project(libab)
SET(CMAKE_C_FLAGS "-fPIC -march=native")
SET(CMAKE_CXX_FLAGS "-std=c++14 -fPIC -march=native")
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})

include_directories(
	src
	include
	dep
	dep/libuv/include
)

set(BUILD_TESTING OFF)
add_subdirectory(dep/libuv EXCLUDE_FROM_ALL)

add_executable(main
	src/main.cc
)

add_executable(abtest
	test/catch.cc
	test/role.cc
)

add_library(ab SHARED
	src/message/message.cc
	src/message/randombytes.cc
	src/peer/peer.cc
	src/node/node.cc
	src/node/role.cc
	src/c.cc
	dep/tweetnacl/tweetnacl.c
)

add_library(ab_static STATIC
	src/message/message.cc
	src/message/randombytes.cc
	src/peer/peer.cc
	src/node/node.cc
	src/node/role.cc
	src/c.cc
	dep/tweetnacl/tweetnacl.c
)
set_target_properties(ab_static PROPERTIES OUTPUT_NAME ab)

target_link_libraries(ab uv)
target_link_libraries(ab_static uv)
target_link_libraries(main ab)
target_link_libraries(abtest ab)

install(TARGETS ab DESTINATION lib)
install(FILES build/dep/libuv/libuv.a DESTINATION lib)
install(TARGETS ab_static DESTINATION lib)
install(FILES include/ab.h DESTINATION include)

add_test(NAME abtest COMMAND abtest)
enable_testing(true)
