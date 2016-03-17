#include "sacq.h"
#include "node/node.hpp"
#include <string>
#include <cpl/net/sockaddr.hpp>

struct ab_node_t { Node* rep; };

ab_node_t*
ab_node_create(uint64_t id, int cluster_size) {
	auto node = new ab_node_t;
	node->rep = new Node(id, cluster_size);
	return node;
}

int
ab_listen(ab_node_t* node, const char* address) {
	return node->rep->start(address);
}

int
ab_connect_to_peer(ab_node_t* node, const char* address) {
	cpl::net::SockAddr addr;
	int status = addr.parse(address);
	if (status < 0) {
		return status;
	}
	node->rep->connect_to_peer(addr);
	return 0;
}

int
ab_run(ab_node_t* node) {
	node->rep->run();
	return -1;
}

int
ab_append(ab_node_t* node, const uint8_t* content, int content_len,
	ab_append_cb cb, void* data) {
	node->rep->append(std::string((const char*)content, content_len), cb, data);
	return 0;
}

int
ab_destroy(ab_node_t* node) {
	delete node->rep;
	delete node;
	return 0;
}