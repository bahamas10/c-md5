/**
 * C implementation of the md5 checksum algorithm
 *
 * Usage
 *
 *   $ ./md5 file.txt
 *   $ ./md5 < file.bin
 *   $ DEBUG=1 ./md5 < file.bin
 *
 * # Created
 * Author: Dave Eddy <ysap@daveeddy.com>
 * Date: August 24, 2026
 * License: MIT
 *
 * # Contributors
 * - Dave Eddy <ysap@daveeddy.com>
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define DEBUG(fmt, ...) do { \
	if (g_debug) { \
		fprintf(stderr, "[%d:%s()] ", __LINE__, __func__); \
		fprintf(stderr, fmt, ##__VA_ARGS__); \
	} \
} while (false)

uint64_t g_byte_count = 0;
bool g_debug = false;

const uint32_t s[] = {
	7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
	5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
	4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
	6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,
};

const uint32_t K[] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
	0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
	0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
	0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
	0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

uint32_t a0 = 0x67452301;
uint32_t b0 = 0xefcdab89;
uint32_t c0 = 0x98badcfe;
uint32_t d0 = 0x10325476;

void process_block(uint8_t block[64]) {
	DEBUG("processing block\n");

	uint32_t M[16];

	// break chunk into sixteen 32-bit words M[j], 0 ≤ j ≤ 15
	for (int i = 0; i < 16; i++) {
		int j = i * 4;
		M[i] = block[j]         | block[j+1] << 8 |
		       block[j+2] << 16 | block[j+3] << 24;
	}

	// Initialize hash value for this chunk:
	uint32_t A = a0;
	uint32_t B = b0;
	uint32_t C = c0;
	uint32_t D = d0;

	// Main loop:
	uint32_t F, g;
	for (int i = 0; i < 16; i++) {
        F = (B & C) | ((~B) & D);
        g = i;
		F = F + A + K[i] + M[g];
		A = D;
		D = C;
		C = B;

		B += (F << s[i]) | (F >> (32 - s[i]));
	}
	for (int i = 16; i < 32; i++) {
        F = (D & B) | ((~D) & C);
        g = (5 * i + 1) % 16;
		F = F + A + K[i] + M[g];
		A = D;
		D = C;
		C = B;

		B += (F << s[i]) | (F >> (32 - s[i]));
	}
	for (int i = 32; i < 48; i++) {
        F = B ^ C ^ D;
        g = (3 * i + 5) % 16;
		F = F + A + K[i] + M[g];
		A = D;
		D = C;
		C = B;

		B += (F << s[i]) | (F >> (32 - s[i]));
	}
	for (int i = 48; i < 64; i++) {
        F = C ^ (B | ~D);
        g = (7 * i) % 16;
		F = F + A + K[i] + M[g];
		A = D;
		D = C;
		C = B;

		B += (F << s[i]) | (F >> (32 - s[i]));
	}

	a0 += A;
	b0 += B;
	c0 += C;
	d0 += D;
}


// returns 0 on success, -1 on failure
int process_input(int fd) {
    ssize_t n;
	uint8_t buf[64 * 4096];
    ssize_t curr_idx = 0;
	while (true) {
		// read data from the fd
		n = read(fd, buf+curr_idx, sizeof (buf)-curr_idx);

		if (n == -1) {
			// error
			switch (errno) {
			case EINTR:
				continue;
			default:
				perror("read");
				return -1;
			}
		} else if (n == 0) {
			// we are done reading / EOF
			break;
		}

		// positive-number we read some stuff
		// loop data byte-by-byte
        if (g_debug) {
            for (int i=curr_idx; i<curr_idx+n; i++) {
                DEBUG("processing byte: %u\n", buf[i]);
            }
        }
		for (int i = curr_idx; i < curr_idx+n-63; i+=64) {
            process_block(buf+i);
		}
        curr_idx += n;
        if (curr_idx==sizeof(buf)) {
            curr_idx = 0;
        }
        g_byte_count += n;
	}

	// we've read the entire message - now we have to finalize it by padding
	// it and appending the length
	// pad it with 1 and then a bunch of 0s
	buf[curr_idx++] = 0x80;
	DEBUG("processing byte: %u\n", 0x80);

	// check if we have a full block ready for processing
	if ((curr_idx%64) == 64) {
		process_block(buf + curr_idx - 64);
	}
	while ((curr_idx%64) != 56) {
        buf[curr_idx++] = 0;
	    DEBUG("processing byte: %u\n", 0x0);
	}

	// append original length in bits mod 2^64 to message
	uint64_t bits_count = g_byte_count << 3;

	// encode the full 64bit int (bits length) as little endian
	for (int i = 0; i < 8; i++) {
		uint8_t byte = bits_count >> (i * 8);
		DEBUG("bits_count[%d]=%u\n", i, byte);
        buf[curr_idx++] = byte;
	    DEBUG("processing byte: %u\n", byte);
	}
    process_block(buf+curr_idx-64);

	// assert things are correct
	if ((curr_idx%64) != 0) {
		fprintf(stderr, "uh oh lol bad block index\n");
		return -1;
	}

	return 0;
}

void print_hash() {
	uint32_t words[] = {a0, b0, c0, d0};

	for (int i = 0; i < 4; i++) {
		uint32_t word = words[i];

		for (int j = 0; j < 4; j++) {
			uint8_t byte = word >> (j*8);
			printf("%02x", byte);
		}
	}
	printf("\n");
}

int main(int argc, char **argv) {
	g_debug = getenv("DEBUG") != NULL;

	int fd = 0;

	// read from the file given as an argument
	if (argc > 1) {
		char *fname = argv[1];

		DEBUG("opening file: %s\n", fname);
		fd = open(fname, O_RDONLY);

		if (fd == -1) {
			perror("open");
			return 1;
		}
	}

	// fd is ready for reading
	process_input(fd);

	close(fd);

	print_hash();

	return 0;
}
