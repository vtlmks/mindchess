// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// mindchess perft driver: validation suite or single-position perft. The engine
// is the generated depth-specialized recursion in mindchess_perft.h over the
// primitives in mindchess.h. Layout a8 = 0 .. h1 = 63.

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mindchess_perft.h"

struct perft_case {
	char    *fen;
	uint32_t depth;
	uint64_t expected;
	char    *name;
};

struct perft_case test_cases[] = {
	{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5,    4865609ull, "startpos d5" },
	{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6,  119060324ull, "startpos d6" },
	{ "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4,    4085603ull, "kiwipete d4" },
	{ "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5,  193690690ull, "kiwipete d5" },
	{ "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",                            5,     674624ull, "position 3 d5" },
	{ "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",                            6,   11030083ull, "position 3 d6" },
	{ "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",     4,     422333ull, "position 4 d4" },
	{ "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",     5,   15833292ull, "position 4 d5" },
	{ "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",            5,   89941194ull, "position 5 d5" },
	{ "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 5, 164075551ull, "position 6 d5" },
	{ "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 6,  8031647685ull, "kiwipete d6" },
};

static int32_t run_tests(void) {
	int32_t fail = 0;
	for(size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); ++i) {
		struct perft_case *t = &test_cases[i];
		struct position p;
		if(pos_set_fen(&p, t->fen) != 0) {
			printf("FAIL  %-20s : FEN parse error\n", t->name);
			++fail;
			continue;
		}
		struct timespec t0, t1;
		clock_gettime(CLOCK_MONOTONIC, &t0);
		uint64_t got = perft(&p, t->depth);
		clock_gettime(CLOCK_MONOTONIC, &t1);
		double secs = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1.0e9;  /* calibration time NOT subtracted: full honest wall time */
		double mnps = (secs > 0) ? ((double)got / secs / 1.0e6) : 0;
		if(got == t->expected) {
			printf("ok    %-20s : %12llu  %8.3fs  %8.2f MNPS\n", t->name, (unsigned long long)got, secs, mnps);
		} else {
			printf("FAIL  %-20s : got %llu  expected %llu\n", t->name, (unsigned long long)got, (unsigned long long)t->expected);
			++fail;
		}
	}
	return fail;
}

int main(int argc, char **argv) {
	mc_init();

	if(argc > 1 && strcmp(argv[1], "perft") == 0) {
		if(argc < 4) {
			fprintf(stderr, "usage: %s perft <fen> <depth>\n", argv[0]);
			return 1;
		}
		struct position p;
		if(pos_set_fen(&p, argv[2]) != 0) {
			fprintf(stderr, "bad FEN\n");
			return 1;
		}
		uint32_t depth = (uint32_t)atoi(argv[3]);
		struct timespec t0, t1;
		clock_gettime(CLOCK_MONOTONIC, &t0);
		uint64_t nodes = perft(&p, depth);
		clock_gettime(CLOCK_MONOTONIC, &t1);
		double secs = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1.0e9;  /* calibration time NOT subtracted: full honest wall time */
		double mnps = (secs > 0) ? ((double)nodes / secs / 1.0e6) : 0;
		printf("perft(%u): %llu  %.3fs  %.2f MNPS\n", depth, (unsigned long long)nodes, secs, mnps);
		return 0;
	}

	return run_tests();
}
