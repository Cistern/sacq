#include <string>
#include <signal.h>
#include <iostream>

#include <glog/logging.h>
#include <cpl/flags.hpp>
#include <cpl/net/sockaddr.hpp>

#include "flags.hpp"
#include "node/node.hpp"

const std::string NAME    = "libab-main";
const std::string VERSION = "DEV";

int
main(int argc, char* argv[]) {
	// Logging setup.
	google::InitGoogleLogging(argv[0]);
	FLAGS_logtostderr = 1;

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
		LOG(INFO) << "--cluster-size is currently 0. Increasing to 1." << std::endl;
		cluster_size = 1;
	}

	ab_callbacks_t callbacks;
	Node n(id, cluster_size, callbacks, nullptr);
	n.set_key(key);
	if (n.start(addr_str) < 0) {
		LOG(WARNING) << "failed to start";
		return 1;
	}
	LOG(INFO) << "listening on " << addr_str << " with ID " << id;

	for (int i = 0; i < peer_addrs.size(); i++) {
		n.connect_to_peer(peer_addrs[i]);
	}

	n.run();

	// Unreachable
	return 1;
}
