# mindchess

`mindchess` is a legal-move perft engine written in C99 and tuned for AMD Zen 4. It exists for one deliberately narrow purpose: explore how fast a general, correct chess move generator can count legal leaf nodes.

Mindchess uses the same legal-move generator for every position. It does not recognize benchmark positions or select algorithms based on FEN, depth, material signature, board density, or elapsed time. Compile-time specialization is limited to genuine chess state, such as side to move and castling rights.

## Current result

The current reference is [Chessbit](external/chessbit/README.md), pinned as a Git submodule so the comparison can be reproduced.

Measured on an AMD Ryzen 9 7950X with boost disabled, the `performance` governor selected, and the maximum frequency set to 4.501 GHz. Core 1 was isolated from normal work and IRQs, and every timed process was launched under `taskset -c 1 chrt -f 99`:

| position | depth | nodes | mindchess | Chessbit | lead |
|---|---:|---:|---:|---:|---:|
| start | 7 | 3,195,901,860 | 2630.69 | 2111.59 | +24.6% |
| kiwipete | 6 | 8,031,647,685 | 3562.75 | 3157.83 | +12.8% |
| position 3 | 8 | 3,009,794,393 | 2126.38 | 1925.90 | +10.4% |
| position 4 | 6 | 706,045,033 | 2905.47 | 2757.07 | +5.4% |
| position 5 | 6 | 3,048,196,529 | 2956.92 | 2698.63 | +9.6% |
| position 6 | 6 | 6,923,051,137 | 3703.99 | 3487.85 | +6.2% |

These are medians from five alternating paired runs on 2026-07-11. Both engines used Clang with `-O3 -flto -mtune=znver4 -march=x86-64-v3`, matched de-hardening flags, and no PGO. MNPS is million nodes per second.

An earlier five-pair run with boost enabled confirmed the same ordering:

| position | boost disabled | boost enabled |
|---|---:|---:|
| start | +24.6% | +24.1% |
| kiwipete | +12.8% | +11.9% |
| position 3 | +10.4% | +10.0% |
| position 4 | +5.4% | +5.9% |
| position 5 | +9.6% | +9.5% |
| position 6 | +6.2% | +5.9% |

These values are local measurements, not claims about every CPU or compiler version. Rebuild and remeasure on the target machine.

## Raw start-position reference

Under the same fixed-clock, single-core conditions, Mindchess completed start-position perft 8 in one measured run as follows:

```text
nodes: 84,998,978,956
time: 32.814 seconds
rate: 2,590.31 MNPS
```

This is raw single-thread enumeration without a transposition table. The command was:

```sh
taskset -c 1 chrt -f 99 ./mindchess perft 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1' 8
```

## Clone and build

Clone with the pinned Chessbit source:

```sh
git clone --recurse-submodules <repository-url>
cd mindchess
```

Build either engine with GCC or Clang:

```sh
./build.sh clang
./build_chessbit.sh clang
```

The outputs are `mindchess` and `chessbit_clang`. Substitute `gcc` to build both with GCC.

Requirements are a C99 compiler, a C++20 compiler, and an x86-64-v3 CPU with AVX2, BMI1/BMI2, POPCNT, and LZCNT. Zen 4 tuning is selected by the build scripts; AVX-512 is intentionally not enabled.

## Validate

Run the built-in correctness suite:

```sh
./mindchess
```

Run one FEN and depth:

```sh
./mindchess perft 'r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1' 6
```

Any move-generation change must reproduce every node count with both compilers before it is timed.

## Reproduce the comparison

The benchmark script builds both engines, checks every node count, alternates execution order, reports medians, and uses the same CPU affinity and realtime policy for both:

```sh
./bench.sh clang 5
./bench.sh gcc 5
```

The machine must permit `chrt -f 99`, and core 1 should be isolated from normal work and IRQs. Before a benchmark, confirm that no user process is executing there:

```sh
ps -eLo pid,tid,psr,cls,rtprio,comm,args | awk '$3 == 1 {print}'
```

The benchmark positions are the six standard Chess Programming Wiki perft positions at depths 7, 6, 8, 6, 6, and 6. Engines are rejected immediately if either node count differs from the expected value.

## Architecture

The active sources are:

- `mindchess.h`: position, attack tables, make operations, and bit primitives
- `mindchess_body.inc`: shared move enumeration
- `mindchess_perft.h`: compile-time instantiations and perft recursion
- `main.c`: FEN driver and correctness suite

The engine uses an absolute-bitboard position, fancy-PEXT sliding attacks, incremental checks, occupancy-aware king-danger tables, copy-make recursion, and a delta view at depth 2. Promotions and other rare special moves retain the normal copy-make path.

Several numbered experimental generations preceded this implementation. They remain available in Git history; the repository presents only the current engine.

## License

Copyright (c) 2026 Peter Fors

SPDX-License-Identifier: MIT
