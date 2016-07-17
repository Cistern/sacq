#include <cstdio>
#include <cstdlib>

#include "randombytes.h"

void randombytes(unsigned char* buf, unsigned int len) {
	FILE* f;
	f = fopen("/dev/urandom", "r");
	if (f == nullptr) {
		exit(1);
	}
	if (fread(buf, len, 1, f) != len) {
		exit(2);
	}
	fclose(f);
}
