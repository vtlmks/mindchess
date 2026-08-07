# Zen 5 optimization notes

Mindchess retains separate Zen 4 and Zen 5 kernels. This document records why the split exists, which Zen 5 changes survived measurement, which approaches were rejected, and what currently limits the retained implementation.

The raw machine logs and chronological experiment diary are intentionally not part of the repository. Values here are the measurements that materially affected the design.

## Why Zen 5 needs a separate kernel

The retained Zen 4 kernel was tuned around Zen 4 instruction behavior and scheduler decisions. Running the unchanged binary on a Ryzen 9 9950X3D exposed a false dependency on the destination of TZCNT on the tested family 26 model 68 stepping 0 core. The behavior is variant-dependent; this result does not imply that every Zen 5 processor is affected.

A dependency microbenchmark measured:

| TZCNT sequence | cycles/iteration |
|---|---:|
| destination not cleared | 24.87 |
| prescribed zero idiom | 2.66 |

The isolated application-level cost was smaller but still material. On position 6, the hazardous Mindchess build required 1.2008 cycles/node. Inserting the required zero idioms reduced that to 1.0984 cycles/node. The hazard raised cycles/node by 9.32% and reduced throughput by 8.53%.

Both Mindchess and Chessbit use TZCNT. The cost depends on register allocation and the dependency chain inherited through the destination, not merely the number of TZCNT instructions. A same-register form has a real source dependency and cannot be preceded by a destructive zero idiom. A distinct-register form needs the destination cleared to break the false dependency.

Applying the Zen 5 changes to the Zen 4 kernel would alter register pressure, scheduling, instruction footprint, and lookup overlap on an architecture without this hazard. The Zen 4 source therefore remains intact, while the Zen 5 kernel is compiled separately.

## Measurement rules

All accepted results used one physical core with its SMT sibling idle. The benchmark CPU and sibling were isolated from ordinary processes, IRQs, RCU callbacks, and unbound workqueues. Timed processes ran with CPU affinity and `SCHED_FIFO` priority 99. Node counts were checked before a result was accepted, execution order alternated, and medians used an odd run count.

No benchmark result from a sandboxed, overlapping, interrupted, or uncertain execution environment was retained.

The final Ryzen 9 9950X3D comparison used CPU 9 on the 32 MiB L3 CCD. Fixed-clock runs disabled boost and used a 4.3 GHz maximum. Stock-boost runs averaged approximately 5.678 GHz in the position-6 counter workload.

## Retained changes

### Paired slider lookups

The depth-2 delta leaf counter processes pairs of independent bishop or rook candidates. It calculates both square indices, loads both lookup records, executes both PEXT operations, and issues both attack-table loads before consuming either result.

The original scalar loop serialized:

```text
tzcnt
square-index shift
lookup-record load
pext
attack-table load
mask
popcnt
accumulate
blsr
branch
```

The paired form exposes two independent `pext`, pointer, and table-load chains to the out-of-order core. In the initial position-6 comparison it changed:

| metric | scalar | paired | delta |
|---|---:|---:|---:|
| cycles/node | 1.720 | 1.253 | -27.2% |
| instructions/node | 5.203 | 5.256 | +1.0% |
| load-wait no-retire cycles | 2828469760 | 1055947143 | -62.7% |
| memory-backend-bound slots | 54.8% | 40.1% | -14.7 points |
| retiring slots | 33.8% | 47.0% | +13.2 points |

The extra instructions are profitable because they create overlap across otherwise serial load-to-use chains.

### Post-register-allocation TZCNT repair

Source-level inline assembly changed LLVM register allocation and caused large workload-dependent regressions. The retained Clang build instead:

1. saves optimized LTO bitcode;
2. runs LLVM code generation to assembly;
3. inserts `xorl dest,dest` immediately before each distinct-register `tzcntq`;
4. assembles and links the transformed output.

This adds 7624 zero idioms without changing LLVM's register choices. Same-register forms remain unchanged because clearing the destination would destroy the source.

On position 6 the repair changed:

| metric | hazardous | repaired | delta |
|---|---:|---:|---:|
| cycles/node | 1.2008 | 1.0984 | -8.52% |
| instructions/node | 5.2850 | 5.4893 | +3.86% |
| IPC | 4.4014 | 4.9974 | +13.54% |
| load-not-complete events | 1121417652 | 583718404 | -47.95% |
| memory-backend-bound slots | 39.0% | 31.8% | -7.2 points |
| retiring fast-path slots | 49.3% | 56.2% | +6.9 points |

The zero idioms increase retired work but nearly halve the load-not-complete events inherited through TZCNT destinations.

### Selective depth-2 specialization

A fully specialized call graph produced excessive instruction footprint. A fully generic depth-2 function reduced text size but lost substantial throughput. The retained hybrid merges king-moved state where the smaller footprint helps, while preserving a fully specialized `(side=0, km=1, ke=0)` body for position 4.

The runtime `(side=0, ke=0)` body deliberately remains reachable with both `km` values. This produces better Clang code for `(0,0,0)` than separate exact specializations.

### Discovered-check ray gate

Before a legal move, the side to move cannot already attack the opposing king. A new slider check can therefore occur only when the move's from/to mask intersects an empty-board bishop or rook ray from that king.

In the hot position-4 specialization, 4407988 of 13848291 candidate leaves, or 31.83%, intersect those rays. Other leaves skip `slider_checks`. The retained gate gained 4.37% in an alternating comparison and reduced memory-backend-bound slots from 29.5% to 25.3%.

Applying the gate globally was not profitable. It is retained only in the state where measurement supports it.

### Compiler target selection

Mindchess uses `-march=znver5 -mtune=znver4`. Changing only the ISA target gained approximately 0.33% geometric mean. The Zen 5 scheduler was neutral on position 3, 0.9% slower on position 5, and only 0.3% faster on position 6.

Chessbit behaved differently. Its best installed compiler was GCC, and its best target was `-march=znver5 -mtune=znver5`:

| position | x86-64-v3, Zen 4 tune | Zen 5 ISA, Zen 4 tune | Zen 5 ISA, Zen 5 tune |
|---|---:|---:|---:|
| Kiwipete | 3400.40 | 3264.02 | 3452.65 |
| position 6 | 3768.97 | 3664.34 | 4141.71 |

The build scripts preserve these independently measured choices rather than forcing both programs through one compiler or scheduler model.

## Rejected approaches

Each item below was correct before performance was considered unless stated otherwise.

| approach | result |
|---|---|
| Dedicated single-slider branch | Lost 2.7% to 4.4% on representative workloads. |
| Batch slider lookups in the king-danger filter | Gained up to 1.5% on two positions but lost 0.8% to 6.3% on four. |
| Fully generic depth-2 function | Reduced text to 781806 bytes but dropped Kiwipete to about 2836 MNPS. |
| Merge king-moved state everywhere | Improved five positions but lost 6.1% on position 4. The retained hybrid restores its hot exact state. |
| Pair pinned sliders | Lost 9.15% to 24.06%. |
| Move pinned-slider accounting to a shared noinline function | Lost 0.70% to 22.18%. |
| Pair bishop and rook candidates during pin discovery | Gained 0.73% on start but lost 3.37% to 10.32% elsewhere. |
| Source-level TZCNT inline assembly | Changed register allocation. Blanket insertion gained 2.3% on position 6 but lost 5.7% on position 4 and 9.7% on position 3. |
| Add the zero idiom to distinct-register BLSR | Reduced position 3 and several TZCNT gains. |
| Cross-pair bishops and rooks | Lost 3.19% to 15.56%, depending on remainder handling. |
| Replace runtime `(0,x,0)` with exact states | Reduced Kiwipete by 2.74%. |
| Align only the position-4 specialization to 64 bytes | Small net loss. |
| Pair bishop and rook lookups inside the king filter | Flat on position 4 and lost 2.9% to 15.2% elsewhere. |
| Make the bishop discovered-check lookup unconditional | Gained 2.2% to 3.4% on positions 5 and 6 but lost 10.4% on position 4. |
| Apply the ray gate to every state | Gained 2.8% on position 4 but lost 5.8% to 17.5% on four other workloads. |
| Hoist or split the discovered-check ray mask | Hoisting lost 15.3%; splitting by ray type lost 6.6%. |
| Generic child descriptor stream | Reduced text but lost 12.1% on position 4. Instructions rose 31.7% and branches rose 18.6%. |
| Pair complete scalar quiet-pawn children | Lost 18.4% from increased live state and register pressure. |
| Standalone paired quiet-pawn dataflow | Remained about 15.5% slower; queue and live-state costs exceeded shared-filter savings. |
| Outline captures into piece-specialized functions | Reduced text but lost 6.43% on position 4. |
| Fixed-stride slider tables | Removed a loaded pointer dependency but increased BSS to 3530464 bytes and lost roughly 10% to 15%. |
| Equal-stride rook-table classes | Preserved the table payload but scalar address arithmetic reduced Kiwipete from about 3.0 to 2.5 GNPS. |
| Four directional scalar rays instead of PEXT tables | Approximately halved throughput and greatly increased text. |
| LLVM max-ILP scheduler options | Produced a byte-identical executable. |
| Benchmark-trained PGO | Not pursued because training on the six comparison positions would make the claimed position independence meaningless. |

The failed variants show that independent operations are not sufficient by themselves. Pairing helps only when the two dependency chains overlap enough latency to repay loop control, register pressure, and increased instruction footprint.

## Why the final Chessbit build remains slower

The final Chessbit gap is not evidence of poor core utilization. A matched boosted position-6 counter comparison gave:

| metric | Mindchess | Chessbit | Chessbit delta |
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

Chessbit sustains higher IPC and retires a larger fraction of dispatch slots. That efficiency reduces a 34.19% instruction-count excess to a 25.16% cycle penalty. The residual loss is dynamic work per leaf.

Mindchess keeps piece, capture, side, castling, and king-state decisions as compile-time facts in its delta leaf path. It avoids constructing complete child positions and issues paired slider lookup chains before consuming either result. Chessbit retains a more general recursive move-generation path. That generality executes more instructions and accumulated 8.96 billion frontend-empty dispatch slots against Mindchess's 4.16 billion in the matched runs.

Cache capacity and branch prediction do not explain the gap. Both cache-miss rates are negligible per node, Chessbit executes fewer branches per node, and the branch-miss rates are close. The old TZCNT hazard explains the large regression of Zen 4-targeted binaries, but it is no longer the dominant final difference after both engines are rebuilt for Zen 5.

## Current limits

Position 6 remains 31.8% memory-backend-bound. The primary remaining cost is ordinary load-completion latency in the occupancy-dependent attack-table chains, not DRAM bandwidth or PEXT throughput.

Position 4 is split between a 25.7% frontend limit and a 25.3% memory-backend limit. The frontend component is 10.4% bandwidth and 15.3% latency. No single move category dominates: the largest piece/capture category accounts for only 18.6% of ordinary leaves in the hot specialization.

The present implementation is therefore not described as optimal. Position 6 reaches 56.1% retiring slots and remains limited by memory-backend latency; position 4 is balanced between frontend delivery and lookup latency. Further gains require reducing total scalar-child work or creating useful overlap without repeating the register-pressure and instruction-footprint failures above.
