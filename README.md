# mindchess

`mindchess` is a legal-move perft engine written in C99 with separately tuned AMD Zen 4 and Zen 5 kernels. It exists for one deliberately narrow purpose: explore how fast a general, correct chess move generator can count legal leaf nodes.

Mindchess uses the same legal-move generator for every position. It does not recognize benchmark positions or select algorithms based on FEN, depth, material signature, board density, or elapsed time. Compile-time specialization is limited to genuine chess state, such as side to move and castling rights.

## Results

The current reference is [Chessbit](external/chessbit/README.md), pinned as a Git submodule so the comparison can be reproduced.

### Zen 5

Measured on the 32 MiB L3 CCD of an AMD Ryzen 9 9950X3D with the `performance` governor selected. CPU 9 and its SMT sibling CPU 25 were isolated from normal work, IRQs, and unbound workqueues. Every timed process was launched under `taskset -c 9 chrt -f 99`.

#### Author-distributed Chessbit binary

The strongest direct comparison uses the [Windows executable distributed by the Chessbit author](external/chessbit/README.md), SHA-256 `445dbf1adf6544d50467b417784007873fc3a57d90792ad343437a3cdf96dbc0`. It was run under Wine 11.14 with stock boost enabled. Wine's persistent service threads were pinned to CPU 0 before measurement, leaving only the timed Chessbit process on CPU 9. The Chessbit values are medians of three runs; the Mindchess values are medians of five runs on the same machine and positions. Rates are MNPS.

| position | depth | nodes | Mindchess | Chessbit | lead |
|---|---:|---:|---:|---:|---:|
| start | 7 | 3,195,901,860 | 3582.48 | 2774.52 | +29.12% |
| kiwipete | 6 | 8,031,647,685 | 4536.99 | 4002.58 | +13.35% |
| midgame | 6 | 6,923,051,137 | 5172.02 | 4583.86 | +12.83% |
| endgame | 7 | 24,958,831,314 | 5079.09 | 4289.61 | +18.40% |

Across the 43,109,431,996 final-depth nodes, Mindchess produced 4835.64 MNPS and Chessbit produced 4110.65 MNPS. The node-weighted aggregate lead is 17.64%. Every reported node count matched. Chessbit's built-in aggregate is not used because its timer includes all preceding depths while its node total contains only the four final depths. This is the primary Zen 5 comparison because it measures the binary selected and published by the Chessbit author. It does not imply how that Windows binary would perform natively on Linux or on another processor.

#### Reproducible source comparison

The pinned Chessbit source was also rebuilt locally for a six-position comparison. These fixed-clock results used boost disabled and a 4.3 GHz maximum:

| position | depth | nodes | Mindchess | Chessbit GCC | lead |
|---|---:|---:|---:|---:|---:|
| start | 7 | 3,195,901,860 | 2729.91 | 1949.89 | +40.00% |
| kiwipete | 6 | 8,031,647,685 | 3453.08 | 2622.75 | +31.66% |
| position 3 | 8 | 3,009,794,393 | 2321.17 | 1567.83 | +48.05% |
| position 4 | 6 | 706,045,033 | 2826.30 | 2232.61 | +26.59% |
| position 5 | 6 | 3,048,196,529 | 3031.76 | 2200.18 | +37.80% |
| position 6 | 6 | 6,923,051,137 | 3927.01 | 3153.26 | +24.54% |

The geometric-mean lead is 34.53%. Stock boost produced:

| position | Mindchess | Chessbit GCC | lead |
|---|---:|---:|---:|
| start | 3580.07 | 2564.15 | +39.62% |
| kiwipete | 4533.24 | 3456.31 | +31.16% |
| position 3 | 3027.61 | 2060.30 | +46.95% |
| position 4 | 3720.66 | 2947.13 | +26.25% |
| position 5 | 3980.02 | 2907.26 | +36.90% |
| position 6 | 5163.29 | 4139.08 | +24.74% |

The boosted geometric-mean lead is 34.05%. Boost raised Mindchess throughput by a 31.21% geometric mean. A position-6 counter run averaged 5.678 GHz.

These source-build results are medians from five alternating paired runs on 2026-08-07. Mindchess used Clang 22.1.8 with `-march=znver5 -mtune=znver4` and the post-register-allocation TZCNT workaround described below. Chessbit used GCC 16.1.1 with `-march=znver5 -mtune=znver5`. GCC beat Clang for Chessbit locally, and the Zen 5 scheduler beat the Zen 4 scheduler for Chessbit. The table records the selected reproducible source builds rather than forcing both programs through one compiler or scheduler model. Its larger lead is not substituted for the direct comparison with the author's distributed binary above.

#### Why the native Chessbit build loses on Zen 5

The following analysis applies to the locally built GCC binary in the six-position source comparison, not to the author's Windows executable. Its gap is not a cache-capacity result and it is not evidence that Chessbit uses the core poorly. A matched boosted position-6 counter run gave:

| metric | Mindchess | Chessbit GCC | Chessbit delta |
|---|---:|---:|---:|
| cycles/node | 1.0972 | 1.3732 | +25.16% |
| instructions/node | 5.4899 | 7.3669 | +34.19% |
| IPC | 5.0037 | 5.3648 | +7.22% |
| branches/node | 0.6898 | 0.6516 | -5.54% |
| branch-miss rate | 0.3312% | 0.3546% | +0.0234 points |
| cache misses/M nodes | 13.33 | 33.90 | +20.57 |
| retiring slots | 56.1% | 62.3% | +6.2 points |
| memory-backend-bound slots | 31.8% | 19.3% | -12.5 points |
| frontend-bound slots | 6.8% | 11.8% | +5.0 points |

Chessbit retires a larger fraction of the machine and sustains higher IPC. It reduces the expected cost of its 34.19% additional instruction count to 25.16% additional cycles. Its remaining loss comes from doing more total work per leaf, not from low IPC. Mindchess's depth-2 delta path keeps piece, capture, side, castling, and king-state decisions as compile-time facts, avoids constructing complete child positions, and batches independent slider lookups before consuming either result. Chessbit retains a more general recursive move-generation path. That generality costs instructions and lengthens the total dataflow even when GCC schedules it well.

The cache-miss counts are tiny for both engines and cannot account for the gap. Chessbit also executes fewer branches per node, while the branch-miss rates are close. Its larger frontend share reflects the larger executed instruction stream; in the same top-down runs Chessbit accumulated 8.96 billion frontend-empty dispatch slots against Mindchess's 4.16 billion.

Compiler and target selection matter substantially. On the qualification positions, changing Chessbit from GCC `-march=x86-64-v3 -mtune=znver4` to GCC `-march=znver5 -mtune=znver5` gained 1.54% on Kiwipete and 9.89% on position 6. Combining the Zen 5 ISA target with the Zen 4 scheduler lost performance. The source-build tables use the winning native Zen 5 configuration.

The TZCNT false-destination dependency explains why the old Zen 4 binaries regressed badly on Zen 5 and why Mindchess required a separate code-generation path. It does not explain the entire final gap after both engines are rebuilt for Zen 5. GCC's native Zen 5 Chessbit build substantially improves dependency scheduling; the residual difference is dominated by dynamic work per node.

### Zen 4

The original result is retained as the Zen 4 reference. It was measured on an AMD Ryzen 9 7950X with boost disabled, the `performance` governor selected, and the maximum frequency set to 4.501 GHz. Core 1 was isolated from normal work and IRQs, and every timed process was launched under `taskset -c 1 chrt -f 99`:

| position | depth | nodes | Mindchess | Chessbit | lead |
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

All values are local measurements, not claims about other CPUs or compiler versions. Rebuild and remeasure on the target machine.

## Zen 4 raw start-position reference

Under the same 7950X fixed-clock, single-core conditions, the Zen 4 kernel completed start-position perft 8 in one measured run as follows:

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
git clone --recurse-submodules https://github.com/vtlmks/mindchess.git
cd mindchess
```

Build the Zen 4 engine and Chessbit with GCC or Clang:

```sh
./build.sh clang zen4
./build_chessbit.sh clang zen4
```

The outputs are `mindchess` and `chessbit_clang`. The `zen4` argument is optional and remains the default. Build the separately maintained Zen 5 kernel and the locally selected Chessbit configuration with:

```sh
./build.sh clang zen5
./build_chessbit.sh gcc zen5
```

The outputs are `mindchess_zen5` and `chessbit_gcc_zen5`. The Mindchess Zen 5 build selects `-march=znver5` while retaining the Zen 4 scheduler model. The Clang path uses the linker plugin's saved LTO bitcode and `llc` to insert AMD's TZCNT false-destination-dependency workaround after register allocation. Chessbit uses the Zen 5 scheduler because that was faster in local qualification. Substitute `gcc` or `clang` to build other compiler combinations.

The Mindchess implementations are separate compilation paths. `main.c`, `mindchess_perft.h`, and `mindchess_body.inc` are the retained Zen 4 kernel. `main_zen5.c`, `mindchess_perft_zen5.h`, and `mindchess_body_zen5.inc` contain the Zen 5 kernel and its independently benchmarked specializations. Low-level board representation, table construction, and copy-make primitives remain shared in `mindchess.h`.

The tested Ryzen 9 9950X3D core, CPUID family 26 model 68 stepping 0, exhibited a false dependency on the TZCNT destination. The behavior is variant-dependent, so the Zen 5 build repairs it unconditionally rather than assuming every Zen 5 processor is affected. Fixing it after register allocation changes the profitable scheduling and overlap of the dependent slider-lookup chains. Folding those changes into the Zen 4 kernel would discard a measured architecture-specific advantage, so the Zen 4 source remains intact and independently buildable.

[ZEN5_OPTIMIZATION.md](ZEN5_OPTIMIZATION.md) records the retained changes, measured effects, rejected variants, final Chessbit analysis, and current limiting resources.

Requirements are a C99 compiler and a C++20 compiler. The Clang Zen 5 build additionally requires the matching LLVM `llc` and a linker plugin supporting `-plugin-opt=save-temps`. The Zen 4 engine requires an x86-64-v3 CPU with AVX2, BMI1/BMI2, POPCNT, and LZCNT. The separately maintained Zen 5 engine targets the `znver5` ISA model.

## Validate

Run the built-in correctness suite:

```sh
./mindchess
./mindchess_zen5
```

Run one FEN and depth:

```sh
./mindchess perft 'r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1' 6
```

Any move-generation change must reproduce every node count with both compilers before it is timed.

## Reproduce the comparison

The benchmark script builds both engines, checks every node count, alternates execution order, reports medians, and uses the same CPU affinity and realtime policy for both:

```sh
./bench.sh zen4 5 1 clang clang
./bench.sh zen5 5 9 clang gcc
```

Arguments select the architecture, odd run count, logical CPU, Mindchess compiler, and Chessbit compiler. The machine must permit `chrt -f 99`, and the selected physical core and its SMT sibling should be isolated from normal work, IRQs, and unbound workqueues. Before a benchmark, confirm that no user process is executing there:

```sh
ps -eLo pid,tid,psr,cls,rtprio,comm,args | awk '$3 == 9 || $3 == 25 {print}'
```

The benchmark positions are the six standard Chess Programming Wiki perft positions at depths 7, 6, 8, 6, 6, and 6. Engines are rejected immediately if either node count differs from the expected value.

## Architecture

The active sources are:

- `mindchess.h`: position, attack tables, make operations, and bit primitives
- `mindchess_body.inc`: shared move enumeration
- `mindchess_perft.h`: compile-time instantiations and perft recursion
- `main.c`: FEN driver and correctness suite
- `mindchess_body_zen5.inc`, `mindchess_perft_zen5.h`, and `main_zen5.c`: separately compiled Zen 5 kernel

The engine uses an absolute-bitboard position, fancy-PEXT sliding attacks, incremental checks, occupancy-aware king-danger tables, copy-make recursion, and a delta view at depth 2. Promotions and other rare special moves retain the normal copy-make path.

The Zen 5 kernel batches pairs of independent bishop and rook attack lookups in the delta leaf counter. This exposes both dependent `pext` and table-load chains to the out-of-order core before either result is consumed. Its post-register-allocation pass inserts AMD's prescribed zero idiom before distinct-register TZCNT forms without perturbing LLVM register allocation. Same-register forms remain unchanged because clearing the destination would destroy the source. The Zen 4 source path is unchanged.

Several numbered experimental generations preceded this implementation. They remain available in Git history; the repository presents only the current engine.

## License

Copyright (c) 2026 Peter Fors

SPDX-License-Identifier: MIT
