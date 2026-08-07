#!/usr/bin/env bash
# Copyright (c) 2026 Peter Fors
# SPDX-License-Identifier: MIT
# usage: ./build.sh [gcc|clang] [zen4|zen5]
set -e
CC="${1:-gcc}"
VARIANT="${2:-zen4}"
CFLAGS="-std=gnu99 -O3 -flto -mtune=znver4 -march=x86-64-v3"
CFLAGS+=" -falign-functions=32 -falign-loops=32 -fno-plt"
CFLAGS+=" -fwrapv -fvisibility=hidden"
CFLAGS+=" -fno-stack-protector -fno-PIE -no-pie -fcf-protection=none"
CFLAGS+=" -ffunction-sections -fdata-sections"
CFLAGS+=" -fno-unwind-tables -fno-asynchronous-unwind-tables"
CFLAGS+=" -U_FORTIFY_SOURCE -fno-pic -fno-semantic-interposition"
CFLAGS+=" -Wall -Wextra"
LDFLAGS="-Wl,--gc-sections -Wl,--as-needed"
POST_LTO_TZCNT=0
if [ "$CC" = "gcc" ]; then
	CFLAGS+=" -flto=auto -fno-inline-functions-called-once"
fi
case "$VARIANT" in
	zen4)
		OUTPUT="mindchess"
		SOURCE="main.c"
		;;
	zen5)
		CFLAGS+=" -DMINDCHESS_ZEN5=1 -march=znver5 -mtune=znver4"
		OUTPUT="mindchess_zen5"
		SOURCE="main_zen5.c"
		if [ "$CC" = "clang" ]; then
			POST_LTO_TZCNT=1
		fi
		;;
	*)
		echo "usage: $0 [gcc|clang] [zen4|zen5]" >&2
		exit 1
		;;
esac
if [ "$POST_LTO_TZCNT" = "1" ]; then
	BUILD_TMP=$(mktemp -d)
	trap 'rm -rf "$BUILD_TMP"' EXIT
	LTO_OUTPUT="$BUILD_TMP/mindchess_lto"
	$CC $CFLAGS "$SOURCE" -o "$LTO_OUTPUT" $LDFLAGS -Wl,-plugin-opt=save-temps
	llc -O=3 -filetype=asm -function-sections -data-sections "$LTO_OUTPUT.0.5.precodegen.bc" -o "$BUILD_TMP/mindchess.s"
	awk 'function reg32(r) { if(r=="%rax") return "%eax"; if(r=="%rbx") return "%ebx"; if(r=="%rcx") return "%ecx"; if(r=="%rdx") return "%edx"; if(r=="%rsi") return "%esi"; if(r=="%rdi") return "%edi"; if(r=="%rbp") return "%ebp"; if(r=="%rsp") return "%esp"; sub(/^%r/, "%r", r); return r "d" } /^[[:space:]]*tzcntq[[:space:]]+.*,[[:space:]]*%[[:alnum:]]+/ { operands=$0; sub(/[[:space:]]*#.*/, "", operands); sub(/^[[:space:]]*tzcntq[[:space:]]+/, "", operands); src=operands; sub(/,[[:space:]]*%[[:alnum:]]+[[:space:]]*$/, "", src); dst=operands; sub(/^.*,[[:space:]]*/, "", dst); gsub(/[[:space:]]/, "", src); gsub(/[[:space:]]/, "", dst); if(src != dst) { d32=reg32(dst); print "\txorl\t" d32 ", " d32 } } { print }' "$BUILD_TMP/mindchess.s" > "$BUILD_TMP/mindchess_tzc.s"
	$CC -no-pie "$BUILD_TMP/mindchess_tzc.s" -o "$OUTPUT.tmp" $LDFLAGS
else
	$CC $CFLAGS "$SOURCE" -o "$OUTPUT.tmp" $LDFLAGS
fi
mv "$OUTPUT.tmp" "$OUTPUT"
echo "built $OUTPUT with $CC"
