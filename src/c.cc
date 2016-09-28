#include "ab.h"
#include "node/node.hpp"
#include <string>
#include <cpl/net/sockaddr.hpp>

struct ab_node_t { Node* rep; };

ab_node_t*
ab_node_create(uint64_t id, int cluster_size, ab_callbacks_t callbacks, void* data) {
	auto node = new ab_node_t;
	node->rep = new Node(id, cluster_size, callbacks, data);
	return node;
}

void
ab_set_key(ab_node_t* node, const char* key, int key_len) {
	std::string key_str(key, key_len);
	node->rep->set_key(key_str);
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
	return node->rep->run();
}

int
ab_append(ab_node_t* node, const char* content, int content_len,
	ab_append_cb cb, void* data) {
	node->rep->append(std::string(content, content_len), cb, data);
	return 0;
}

void
ab_confirm_append(ab_node_t* node, uint64_t round) {
	node->rep->confirm_append(round);
}

int
ab_destroy(ab_node_t* node) {
	if (node == nullptr) {
		return -1;
	}
	if (node->rep == nullptr) {
		return -1;
	}
	// Shut down and clean up event loop.
	node->rep->shutdown();
	delete node->rep; // Blocks until the event loop stops.
	delete node;
	node = nullptr;
	return 0;
}
