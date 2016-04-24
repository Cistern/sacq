#pragma once

#include <vector>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <string>

// cpl::Flags
#include <cpl/flags.hpp>

// cpl::net::IP
#include <cpl/net/ip.hpp>

// cpl::net::SockAddr
#include <cpl/net/sockaddr.hpp>

void
show_help(std::string a, std::string b, void* d) {
	auto flags = reinterpret_cast<cpl::Flags*>(d);
	flags->print_usage();
	exit(0);
}

void
set_string(std::string a, std::string b, void* d) {
	auto str = reinterpret_cast<std::string*>(d);
	*str = b;
}

void
set_id(std::string a, std::string b, void* d) {
	auto id = reinterpret_cast<uint64_t*>(d);
	*id = (uint64_t)atoi(b.c_str());
}

void
set_cluster_size(std::string a, std::string b, void* d) {
	auto cluster_size = reinterpret_cast<int*>(d);
	*cluster_size = atoi(b.c_str());
}

void
add_peers(std::string a, std::string b, void* d) {
	auto peers = reinterpret_cast<std::vector<cpl::net::SockAddr>*>(d);

	std::istringstream ss(b);
	std::string token;

	while (std::getline(ss, token, ',')) {
		cpl::net::SockAddr addr;
		int status = addr.parse(token);
		if (status < 0) {
			std::cerr << "`" << token << "'" << " is not a valid peer address" << std::endl;
			continue;
		}
		peers->push_back(addr);
	}
}
