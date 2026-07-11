#!/usr/bin/env bash
# Copyright (c) 2026 Peter Fors
# SPDX-License-Identifier: MIT
# usage: ./build_chessbit.sh [gcc|clang]
set -e
CC="${1:-clang}"
case "$CC" in
	gcc)   CXX=g++; LTO=-flto=auto; VENDOR_WARNINGS=-Wno-invalid-constexpr ;;
	clang) CXX=clang++ ;;
	*) echo "usage: $0 [gcc|clang]" >&2; exit 1 ;;
esac

ROOT=$(pwd)
SRCDIR="$ROOT/external/chessbit/chessbit"
OUT="$ROOT/chessbit_$CC"
TMP="$OUT.tmp"
SRCS="Main.cpp Cui.cpp Game.cpp Utils.cpp MoveArray.cpp"

CXXFLAGS="-std=c++20 -O3 -flto -mtune=znver4 -march=x86-64-v3 -DNDEBUG"
CXXFLAGS+=" ${LTO:-}"
CXXFLAGS+=" ${VENDOR_WARNINGS:-}"
CXXFLAGS+=" -fomit-frame-pointer -foptimize-sibling-calls"
CXXFLAGS+=" -fno-stack-protector -fno-PIE -no-pie -fcf-protection=none"
CXXFLAGS+=" -ffunction-sections -fdata-sections -fno-plt"
CXXFLAGS+=" -fno-unwind-tables -fno-asynchronous-unwind-tables"
CXXFLAGS+=" -U_FORTIFY_SOURCE -fno-pic -fno-semantic-interposition"
CXXFLAGS+=" -include $ROOT/chessbit_compat.h"
LDFLAGS="-Wl,--gc-sections -Wl,--as-needed"

( cd "$SRCDIR" && $CXX $CXXFLAGS $SRCS -o "$TMP" $LDFLAGS )
mv "$TMP" "$OUT"
echo "built $OUT with $CXX"
