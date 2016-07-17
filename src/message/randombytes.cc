#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "randombytes.h"

void randombytes(unsigned char* buf, unsigned int len) {
	FILE* f;
	f = fopen("/dev/urandom", "r");
	if (f == nullptr) {
		std::cerr << "libab: failed to open /dev/urandom" << std::endl;
		exit(1);
	}
	unsigned int remaining = len;
	while (remaining > 0) {
		size_t read = fread(buf, remaining, 1, f);
		if (read == 0) {
			std::cerr << "libab: failed to read from /dev/urandom" << std::endl;
			exit(2);
		}
		remaining -= read;
	}
	fclose(f);
}
