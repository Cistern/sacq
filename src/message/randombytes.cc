#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "randombytes.h"

static FILE* dev_urandom;

void randombytes(unsigned char* buf, unsigned int len) {
	if (dev_urandom == nullptr) {
		dev_urandom = fopen("/dev/urandom", "r");
		if (dev_urandom == nullptr) {
			std::cerr << "libab: failed to open /dev/urandom" << std::endl;
			exit(1);
		}
	}
	unsigned int remaining = len;
	while (remaining > 0) {
		size_t read = fread(buf + (len-remaining), remaining, 1, dev_urandom);
		if (read == 0) {
			std::cerr << "libab: failed to read from /dev/urandom" << std::endl;
			exit(2);
		}
		remaining -= read;
	}
}
