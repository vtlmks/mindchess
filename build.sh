#!/usr/bin/env bash
# Copyright (c) 2026 Peter Fors
# SPDX-License-Identifier: MIT
# usage: ./build.sh [gcc|clang]
set -e
CC="${1:-gcc}"
CFLAGS="-std=gnu99 -O3 -flto -mtune=znver4 -march=x86-64-v3"
CFLAGS+=" -falign-functions=32 -falign-loops=32 -fno-plt"
CFLAGS+=" -fwrapv -fvisibility=hidden"
CFLAGS+=" -fno-stack-protector -fno-PIE -no-pie -fcf-protection=none"
CFLAGS+=" -ffunction-sections -fdata-sections"
CFLAGS+=" -fno-unwind-tables -fno-asynchronous-unwind-tables"
CFLAGS+=" -U_FORTIFY_SOURCE -fno-pic -fno-semantic-interposition"
CFLAGS+=" -Wall -Wextra -Wno-unused-parameter"
LDFLAGS="-Wl,--gc-sections -Wl,--as-needed"
if [ "$CC" = "gcc" ]; then
	CFLAGS+=" -flto=auto -fno-inline-functions-called-once"
fi
$CC $CFLAGS main.c -o mindchess.tmp $LDFLAGS
mv mindchess.tmp mindchess
echo "built mindchess with $CC"
