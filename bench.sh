#!/usr/bin/env bash
# Copyright (c) 2026 Peter Fors
# SPDX-License-Identifier: MIT
set -euo pipefail

POS_NAMES=(startpos kiwipete pos3 pos4 pos5 pos6)
POS_FENS=(
	"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
	"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
	"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
	"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"
	"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
	"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"
)
POS_DEPTH=(7 6 8 6 6 6)
POS_NODES=(3195901860 8031647685 3009794393 706045033 3048196529 6923051137)

CC="${1:-clang}"
RUNS="${2:-5}"
MIND="./mindchess"
CHESSBIT="./chessbit_$CC"

case "$CC" in
	gcc|clang) ;;
	*) echo "usage: $0 [gcc|clang] [runs]" >&2; exit 1 ;;
esac

if ! [[ "$RUNS" =~ ^[1-9][0-9]*$ ]] || ((RUNS % 2 == 0)); then
	echo "runs must be a positive odd number" >&2
	exit 1
fi

echo "building mindchess and Chessbit with $CC"
./build.sh "$CC"
./build_chessbit.sh "$CC"
echo "benchmarking on core 1 with SCHED_FIFO priority 99"

mind_run() {
	taskset -c 1 chrt -f 99 "$MIND" perft "$1" "$2" | awk '{print $2, $4}'
}

chessbit_run() {
	printf 'setfen %s\nperft %s\nexit\n' "$1" "$2" | taskset -c 1 chrt -f 99 "$CHESSBIT" | awk -F'\t' '/Nodes/{gsub(/ /, "", $NF); nodes=$NF} /Average/{gsub(/[^0-9.]/, "", $NF); speed=$NF} END{print nodes, speed}'
}

printf '%-10s %5s %14s %12s %12s %9s\n' position depth nodes mindchess chessbit lead

for idx in "${!POS_NAMES[@]}"; do
	fen="${POS_FENS[$idx]}"
	depth="${POS_DEPTH[$idx]}"
	expected="${POS_NODES[$idx]}"
	mind_values=()
	chessbit_values=()
	for((run = 0; run < RUNS; ++run)); do
		if(((run & 1) == 0)); then
			read -r mind_nodes mind_speed < <(mind_run "$fen" "$depth")
			read -r chessbit_nodes chessbit_speed < <(chessbit_run "$fen" "$depth")
		else
			read -r chessbit_nodes chessbit_speed < <(chessbit_run "$fen" "$depth")
			read -r mind_nodes mind_speed < <(mind_run "$fen" "$depth")
		fi
		if [[ "$mind_nodes" != "$expected" || "$chessbit_nodes" != "$expected" ]]; then
			echo "node-count failure on ${POS_NAMES[$idx]}: expected $expected, mindchess $mind_nodes, chessbit $chessbit_nodes" >&2
			exit 1
		fi
		mind_values+=("$mind_speed")
		chessbit_values+=("$chessbit_speed")
	done
	mind_median=$(printf '%s\n' "${mind_values[@]}" | sort -n | awk -v n="$RUNS" 'NR == int((n + 1) / 2){print}')
	chessbit_median=$(printf '%s\n' "${chessbit_values[@]}" | sort -n | awk -v n="$RUNS" 'NR == int((n + 1) / 2){print}')
	lead=$(awk -v mind="$mind_median" -v chessbit="$chessbit_median" 'BEGIN{printf "%+.1f%%", (mind / chessbit - 1.0) * 100.0}')
	printf '%-10s %5s %14s %12.2f %12.2f %9s\n' "${POS_NAMES[$idx]}" "$depth" "$expected" "$mind_median" "$chessbit_median" "$lead"
done
