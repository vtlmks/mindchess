// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#ifndef CHESSBIT_COMPAT_H
#define CHESSBIT_COMPAT_H

#include <immintrin.h>

#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif

#ifndef __popcnt64
#define __popcnt64(x) _mm_popcnt_u64(x)
#endif

#ifndef __lzcnt64
#define __lzcnt64(x) _lzcnt_u64(x)
#endif

#endif
