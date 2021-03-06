#include <string>
#include <signal.h>
#include <iostream>
#include <thread>
#include <chrono>

#include <cpl/flags.hpp>
#include <cpl/net/sockaddr.hpp>

#include "flags.hpp"
#include "node/node.hpp"

const std::string NAME    = "libab-main";
const std::string VERSION = "DEV";

int
main(int argc, char* argv[]) {
	// Ignore SIGPIPE. This way we don't exit if we attempt to
	// write to a closed connection.
	signal(SIGPIPE, SIG_IGN);

	// Local listen address
	std::string addr_str;
	// Shared encryption key
	std::string key;
	// Peers to initially connect to.
	// These are automatically marked as valid.
	std::vector<cpl::net::SockAddr> peer_addrs;
	uint64_t id = 0;
	int cluster_size = 0;

	// Flags
	cpl::Flags flags(NAME, VERSION);
	flags.add_option("--help", "-h", "show help documentation", show_help, &flags);
	flags.add_option("--listen", "-l", "set listen address for cluster nodes", set_string, &addr_str);
	flags.add_option("--key", "-k", "set shared cluster encryption key", set_string, &key);
	flags.add_option("--peers", "-p", "list of peers", add_peers, &peer_addrs);
	flags.add_option("--id", "-i", "ID, unique among the cluster", set_id, &id);
	flags.add_option("--cluster-size", "-s", "Total size of the cluster. Determines quorum size.",
		set_cluster_size, &cluster_size);
	flags.parse(argc, argv);

	// Check required flags
	if (addr_str == "") {
		std::cerr << "--listen flag unset. Please specify a listen address." << std::endl;
		return 1;
	}

	if (id == 0) {
		std::cerr << "--id flag unset. Please specify a nonzero unique node ID." << std::endl;
		return 1;
	}

	if (cluster_size == 0) {
		std::cerr << "--cluster-size is currently 0. Increasing to 1." << std::endl;
		cluster_size = 1;
	}

	// Initialize node
	auto n = std::make_unique<Node>(id, cluster_size);
	if (n->set_key(key) < 0) {
		std::cerr << "invalid key" << std::endl;
		return 1;
	}

	// Set up callbacks
	ab_callbacks_t callbacks;
	callbacks.gained_leadership = [](void* cb_data) {
		std::cerr << "gained leadership" << std::endl;
	};
	callbacks.lost_leadership = [](void* cb_data) {
		std::cerr << "lost leadership" << std::endl;
	};
	callbacks.on_leader_change = [](uint64_t leader_id, void* cb_data) {
		std::cerr << "leader is now " << leader_id << std::endl;
	};
	callbacks.on_append = [](uint64_t round, const char* data, int data_len, void* cb_data) {
		auto node = (Node*)(cb_data);
		std::cerr << "on append " << round << std::endl;
		node->confirm_append(round);
	};
	n->set_callbacks(callbacks, n.get());

	// Start
	auto ret = n->start(addr_str);
	if (ret < 0) {
		std::cerr << "failed to start: " << ret << std::endl;
		return 1;
	}
	std::cerr << "listening on " << addr_str << " with ID " << id << std::endl;

	for (int i = 0; i < peer_addrs.size(); i++) {
		n->connect_to_peer(peer_addrs[i]);
	}

	std::thread t([&n]() {
		n->run();
	});

	std::this_thread::sleep_for(std::chrono::seconds(10));

	n->append("hello", [](int status, void*) {
		std::cerr << "append result " << status << std::endl;
	}, nullptr);

	std::this_thread::sleep_for(std::chrono::seconds(10));

	n->shutdown();
	t.join();
	n = nullptr;
	return 0;
}
