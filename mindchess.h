// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// mindchess: single-algorithm make-recurse engine. A faithful absolute-board port
// of chessbit (pieces[color][piece], no M/E perspective swap; `us` is the side-to-move
// template param so pieces[us]/[them] fold at compile time), incremental checks carried
// by make, PEXT sliders. NO density gate, NO lazy leaf, NO timer-driven mode selection:
// one move generator for every node, like the reference. The win must come from doing
// genuinely less work per node than chessbit, not from routing. Square-indexed tables
// are generated at init. Layout a8 = 0 .. h1 = 63, white = 0. Enumeration lives in
// mindchess_body.inc.

#ifndef MINDCHESS_H
#define MINDCHESS_H

#include <stdint.h>
#include <string.h>
#include <immintrin.h>

#define PC_P 0u
#define PC_N 1u
#define PC_B 2u
#define PC_R 3u
#define PC_Q 4u
#define PC_K 5u

#define WHITE 0
#define BLACK 1
#define NOSQUARE 64u

#define AINLINE __attribute__((always_inline)) static inline
#define NOINLINE_COLD __attribute__((noinline, cold)) static


// absolute board: pieces[color][piece]. checks = bitboard of (stm^1) pieces
// giving check to stm's king (carried by make). King attacks are derived from
// king_sq: caching them here enlarges every copied child and causes STLI replays.
struct position {
	uint64_t pieces[2][6];
	uint64_t occ[2];
	uint64_t occ_both;
	uint64_t checks;
	uint32_t king_sq[2];
	int32_t  cas_perms;
	uint32_t ep;
	uint32_t stm;
};

// [=]===^=[ globals: generated tables (square-indexed, shared with m6) ]==[=]

// Entry 64 remains zero: tzc(0) can consume one empty knight candidate
// without a branch in the depth-2 delta king filter.
static uint64_t KNIGHT_ATTACKS[65];
static uint64_t KING_ATTACKS[64];
static uint64_t PAWN_CAPTURES[2][65];
static uint64_t PASSANT_CAPTURES[65];
static uint64_t BISHOP_XRAYS[64];
static uint64_t ROOK_XRAYS[64];
static uint64_t PIN_MASKS[65][64];

struct slider_lookup {
	uint64_t *ptr;
	uint64_t  mask;
};

static struct slider_lookup bishop_lu[64];
static struct slider_lookup rook_lu[64];
static uint64_t bishop_table[5248];
static uint64_t rook_table[102400];

static uint64_t KNIGHT_ATTACK_ZONES[65];
static uint64_t KNIGHT_ATTACK_ZONE_CASTLE[2];
static uint64_t BISHOP_ATTACK_ZONES[64][256];
static uint64_t ROOK_ATTACK_ZONES[64][256];
static uint64_t BISHOP_ZONE_CASTLE[2][256];
static uint64_t ROOK_ZONE_CASTLE[2][256];
static uint64_t BISHOP_ZONE_CASTLE_MASK[2];
static uint64_t ROOK_ZONE_CASTLE_MASK[2];
static uint64_t KNIGHT_ZONE_CASTLE_MASK[2];

// [=]===^=[ constant data (a8 = 0 layout, verbatim from chessbit/m6) ]==[=]

static const uint64_t FIRST_COL = 0x0101010101010101ull;
static const uint64_t LAST_COL  = 0x8080808080808080ull;
static const uint64_t FULL_BOARD = ~0ull;

static const int32_t  PAWN_PUSH[2]        = { -8, 8 };
static const int32_t  PAWN_DOUBLE_PUSH[2] = { -16, 16 };
static const int32_t  PAWN_LEFT[2]        = { -9, 7 };
static const int32_t  PAWN_RIGHT[2]       = { -7, 9 };

static const uint64_t EN_PASSANT_RANK[2]  = { 0xff000000ull, 0xff00000000ull };
static const uint64_t FIRST_PUSH_RANK[2]  = { 0xff0000000000ull, 0xff0000ull };
static const uint64_t PROMO_RANKS[2]      = { 0xff00ull, 0xff000000000000ull };
static const uint64_t LAST_RANKS[2]       = { 0xffull, 0xff00000000000000ull };

static const int32_t  NO_CASTLE[2]        = { 12, 3 };
static const int32_t  NO_CASTLE_ROOK[64]  = {
	7, 15, 15, 15, 15, 15, 15, 11,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	13, 15, 15, 15, 15, 15, 15, 14
};

static const int32_t  CASTLING_SIDE_K[2]  = { 0, 2 };
static const int32_t  CASTLING_SIDE_Q[2]  = { 1, 3 };
static const int32_t  CASTLING[4]         = { 1, 2, 4, 8 };
static const int32_t  CASTLING_SIDE[4]    = { WHITE, WHITE, BLACK, BLACK };
static const int32_t  CAS_BOTH[2]         = { 3, 12 };
static const uint64_t CASTLING_OCCUPIED_SQUARES[4] = { 0x6000000000000000ull, 0x0e00000000000000ull, 0x60ull, 0x0eull };
static const uint64_t CASTLING_PASSING_SQUARES[4]  = { 0x6000000000000000ull, 0x0c00000000000000ull, 0x60ull, 0x0cull };
static const int32_t  CASTLING_KING_TARGET[4]      = { 62, 58, 6, 2 };
static const uint64_t CASTLING_KING_SWITCH[4]      = { 0x5000000000000000ull, 0x1400000000000000ull, 0x50ull, 0x14ull };
static const uint64_t CASTLING_ROOK_SWITCH[4]      = { 0xa000000000000000ull, 0x0900000000000000ull, 0xa0ull, 0x09ull };
static const uint64_t CASTLING_BOTH_SWITCH[4]      = {
	0x5000000000000000ull | 0xa000000000000000ull,
	0x1400000000000000ull | 0x0900000000000000ull,
	0x50ull | 0xa0ull,
	0x14ull | 0x09ull
};

static const uint32_t HOME_KING[2]     = { 60, 4 };
static const uint32_t CASTLE_K_TGT[2]  = { 62, 6 };
static const uint32_t CASTLE_Q_TGT[2]  = { 58, 2 };

// [=]===^=[ low-level bit helpers ]==================================[=]

AINLINE uint32_t tzc(uint64_t x) {
	return (uint32_t)_tzcnt_u64(x);
}

AINLINE uint64_t blsr(uint64_t x) {
	return _blsr_u64(x);
}

AINLINE uint64_t popc(uint64_t x) {
	return (uint64_t)_mm_popcnt_u64(x);
}

// [=]===^=[ attack getters ]=========================================[=]

AINLINE uint64_t get_bishop(uint32_t sq, uint64_t occ) {
	return bishop_lu[sq].ptr[_pext_u64(occ, bishop_lu[sq].mask)];
}

AINLINE uint64_t get_rook(uint32_t sq, uint64_t occ) {
	return rook_lu[sq].ptr[_pext_u64(occ, rook_lu[sq].mask)];
}

AINLINE uint64_t get_queen(uint32_t sq, uint64_t occ) {
	return get_bishop(sq, occ) | get_rook(sq, occ);
}

AINLINE uint64_t slider_checks(uint64_t bb, uint64_t rr, uint64_t qq, uint64_t occB, uint32_t ksq) {
	uint64_t checks = 0;
	if(BISHOP_XRAYS[ksq] & (bb | qq)) {
		checks |= get_bishop(ksq, occB) & (bb | qq);
	}
	if(ROOK_XRAYS[ksq] & (rr | qq)) {
		checks |= get_rook(ksq, occB) & (rr | qq);
	}
	return checks;
}

// [=]===^=[ table generation (verbatim from m6) ]====================[=]

static uint64_t slide_attacks(uint32_t sq, uint64_t occ, const int32_t dirs[][2], uint32_t ndirs) {
	uint64_t attacks = 0;
	int32_t r0 = (int32_t)(sq >> 3);
	int32_t f0 = (int32_t)(sq & 7);
	for(uint32_t d = 0; d < ndirs; ++d) {
		int32_t r = r0 + dirs[d][0];
		int32_t f = f0 + dirs[d][1];
		while(r >= 0 && r < 8 && f >= 0 && f < 8) {
			uint32_t s = (uint32_t)(r * 8 + f);
			attacks |= 1ull << s;
			if(occ & (1ull << s)) {
				break;
			}
			r += dirs[d][0];
			f += dirs[d][1];
		}
	}
	return attacks;
}

static uint64_t slide_mask(uint32_t sq, const int32_t dirs[][2], uint32_t ndirs) {
	uint64_t mask = 0;
	int32_t r0 = (int32_t)(sq >> 3);
	int32_t f0 = (int32_t)(sq & 7);
	for(uint32_t d = 0; d < ndirs; ++d) {
		int32_t r = r0 + dirs[d][0];
		int32_t f = f0 + dirs[d][1];
		while(r >= 0 && r < 8 && f >= 0 && f < 8) {
			int32_t nr = r + dirs[d][0];
			int32_t nf = f + dirs[d][1];
			if(nr < 0 || nr >= 8 || nf < 0 || nf >= 8) {
				break;
			}
			mask |= 1ull << (uint32_t)(r * 8 + f);
			r = nr;
			f = nf;
		}
	}
	return mask;
}

static const int32_t BISHOP_DIRS[4][2] = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
static const int32_t ROOK_DIRS[4][2]   = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

static void init_leaper_tables(void) {
	static const int32_t KDIRS[8][2] = { { 1, 1 }, { 1, 0 }, { 1, -1 }, { 0, 1 }, { 0, -1 }, { -1, 1 }, { -1, 0 }, { -1, -1 } };
	static const int32_t NDIRS[8][2] = { { 2, 1 }, { 2, -1 }, { -2, 1 }, { -2, -1 }, { 1, 2 }, { 1, -2 }, { -1, 2 }, { -1, -2 } };
	for(uint32_t sq = 0; sq < 64; ++sq) {
		int32_t r0 = (int32_t)(sq >> 3);
		int32_t f0 = (int32_t)(sq & 7);
		uint64_t kb = 0;
		uint64_t nb = 0;
		for(uint32_t d = 0; d < 8; ++d) {
			int32_t kr = r0 + KDIRS[d][0];
			int32_t kf = f0 + KDIRS[d][1];
			if(kr >= 0 && kr < 8 && kf >= 0 && kf < 8) {
				kb |= 1ull << (uint32_t)(kr * 8 + kf);
			}
			int32_t nr = r0 + NDIRS[d][0];
			int32_t nf = f0 + NDIRS[d][1];
			if(nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
				nb |= 1ull << (uint32_t)(nr * 8 + nf);
			}
		}
		KING_ATTACKS[sq] = kb;
		KNIGHT_ATTACKS[sq] = nb;
	}
}

static void init_pawn_tables(void) {
	for(uint32_t sq = 0; sq < 64; ++sq) {
		int32_t f = (int32_t)(sq & 7);
		uint64_t w = 0;
		uint64_t b = 0;
		if(f > 0) {
			w |= 1ull << (sq - 9);
			b |= 1ull << (sq + 7);
		}
		if(f < 7) {
			w |= 1ull << (sq - 7);
			b |= 1ull << (sq + 9);
		}
		PAWN_CAPTURES[WHITE][sq] = w;
		PAWN_CAPTURES[BLACK][sq] = b;
	}
	PAWN_CAPTURES[WHITE][64] = 0;
	PAWN_CAPTURES[BLACK][64] = 0;

	for(uint32_t ep = 0; ep < 65; ++ep) {
		PASSANT_CAPTURES[ep] = 0;
	}
	for(uint32_t x = 0; x < 64; ++x) {
		for(uint32_t ep = 16; ep <= 23; ++ep) {
			if(PAWN_CAPTURES[WHITE][x] & (1ull << ep)) {
				PASSANT_CAPTURES[ep] |= 1ull << x;
			}
		}
		for(uint32_t ep = 40; ep <= 47; ++ep) {
			if(PAWN_CAPTURES[BLACK][x] & (1ull << ep)) {
				PASSANT_CAPTURES[ep] |= 1ull << x;
			}
		}
	}
}

static void init_slider_tables(void) {
	uint32_t boff = 0;
	uint32_t roff = 0;
	for(uint32_t sq = 0; sq < 64; ++sq) {
		uint64_t bmask = slide_mask(sq, BISHOP_DIRS, 4);
		uint64_t rmask = slide_mask(sq, ROOK_DIRS, 4);
		bishop_lu[sq].mask = bmask;
		rook_lu[sq].mask = rmask;
		bishop_lu[sq].ptr = bishop_table + boff;
		rook_lu[sq].ptr = rook_table + roff;

		uint64_t sub = 0;
		do {
			bishop_table[boff + _pext_u64(sub, bmask)] = slide_attacks(sq, sub, BISHOP_DIRS, 4);
			sub = (sub - bmask) & bmask;
		} while(sub);
		boff += 1u << (uint32_t)popc(bmask);

		sub = 0;
		do {
			rook_table[roff + _pext_u64(sub, rmask)] = slide_attacks(sq, sub, ROOK_DIRS, 4);
			sub = (sub - rmask) & rmask;
		} while(sub);
		roff += 1u << (uint32_t)popc(rmask);
	}
}

static void init_xray_pin_tables(void) {
	for(uint32_t sq = 0; sq < 64; ++sq) {
		BISHOP_XRAYS[sq] = get_bishop(sq, 0);
		ROOK_XRAYS[sq] = get_rook(sq, 0);
	}
	for(uint32_t k = 0; k < 64; ++k) {
		PIN_MASKS[64][k] = 0;
	}
	for(uint32_t s = 0; s < 64; ++s) {
		for(uint32_t k = 0; k < 64; ++k) {
			uint64_t mask = 0;
			uint64_t bk = get_bishop(k, 1ull << s);
			if(bk & (1ull << s)) {
				mask = bk & get_bishop(s, 1ull << k);
			} else {
				uint64_t rk = get_rook(k, 1ull << s);
				if(rk & (1ull << s)) {
					mask = rk & get_rook(s, 1ull << k);
				}
			}
			PIN_MASKS[s][k] = mask;
		}
	}
}

static uint64_t deposit(uint32_t idx, uint64_t mask) {
	return _pdep_u64((uint64_t)idx, mask);
}

static void init_zone_tables(void) {
	for(uint32_t sq = 0; sq < 65; ++sq) {
		KNIGHT_ATTACK_ZONES[sq] = 0;
	}
	for(uint32_t sq = 0; sq < 64; ++sq) {
		uint64_t relevant = (1ull << sq) | KING_ATTACKS[sq];
		uint64_t z = 0;
		uint64_t bb = relevant;
		while(bb) {
			z |= KNIGHT_ATTACKS[tzc(bb)];
			bb = blsr(bb);
		}
		KNIGHT_ATTACK_ZONES[sq] = z;

		uint64_t kmask = KING_ATTACKS[sq];
		uint32_t bits = (uint32_t)popc(kmask);
		uint32_t n = 1u << bits;
		for(uint32_t idx = 0; idx < n; ++idx) {
			uint64_t own = deposit(idx, kmask);
			uint64_t escapes = kmask & ~own;
			uint64_t bz = 0;
			uint64_t rz = 0;
			uint64_t e = escapes;
			while(e) {
				uint32_t s = tzc(e);
				e = blsr(e);
				bz |= BISHOP_XRAYS[s];
				rz |= ROOK_XRAYS[s];
			}
			BISHOP_ATTACK_ZONES[sq][idx] = bz;
			ROOK_ATTACK_ZONES[sq][idx] = rz;
		}
	}

	for(uint32_t side = 0; side < 2; ++side) {
		uint32_t home = HOME_KING[side];
		uint64_t kmask = KING_ATTACKS[home];
		BISHOP_ZONE_CASTLE_MASK[side] = kmask;
		ROOK_ZONE_CASTLE_MASK[side] = kmask;
		KNIGHT_ZONE_CASTLE_MASK[side] = kmask;
		uint64_t extra = (1ull << CASTLE_K_TGT[side]) | (1ull << CASTLE_Q_TGT[side]);
		uint32_t bits = (uint32_t)popc(kmask);
		uint32_t n = 1u << bits;
		for(uint32_t idx = 0; idx < n; ++idx) {
			uint64_t own = deposit(idx, kmask);
			uint64_t escapes = (kmask & ~own) | extra;
			uint64_t bz = 0;
			uint64_t rz = 0;
			uint64_t e = escapes;
			while(e) {
				uint32_t s = tzc(e);
				e = blsr(e);
				bz |= BISHOP_XRAYS[s];
				rz |= ROOK_XRAYS[s];
			}
			BISHOP_ZONE_CASTLE[side][idx] = bz;
			ROOK_ZONE_CASTLE[side][idx] = rz;
		}
	}

	KNIGHT_ATTACK_ZONE_CASTLE[0] = 0;
	KNIGHT_ATTACK_ZONE_CASTLE[1] = 0;
	for(uint32_t side = 0; side < 2; ++side) {
		uint64_t relevant = (1ull << HOME_KING[side]) | KING_ATTACKS[HOME_KING[side]] | (1ull << CASTLE_K_TGT[side]) | (1ull << CASTLE_Q_TGT[side]);
		uint64_t z = 0;
		uint64_t bb = relevant;
		while(bb) {
			z |= KNIGHT_ATTACKS[tzc(bb)];
			bb = blsr(bb);
		}
		KNIGHT_ATTACK_ZONE_CASTLE[side] = z;
	}
}

// [=]===^=[ pawn shift helpers (side-templated) ]====================[=]

AINLINE uint64_t pawns_left(uint64_t pawns, uint32_t side) {
	if(side == WHITE) {
		return pawns >> 9;
	}
	return pawns << 7;
}

AINLINE uint64_t pawns_right(uint64_t pawns, uint32_t side) {
	if(side == WHITE) {
		return pawns >> 7;
	}
	return pawns << 9;
}

AINLINE uint64_t pawns_fwd(uint64_t pawns, uint32_t side) {
	if(side == WHITE) {
		return pawns >> 8;
	}
	return pawns << 8;
}

// [=]===^=[ make: absolute, field-by-field, incremental checks ]=====[=]
// us = mover; child stm = them. checks = us pieces attacking them's king (carried).
// no perspective swap: child keeps color indexing, only moved/captured boards change.

AINLINE void make_generic(struct position *bd, uint32_t from, uint32_t to, struct position *out, uint32_t piece, uint32_t us, uint32_t capture) {
	uint32_t them = us ^ 1u;
	uint64_t t  = 1ull << to;
	uint64_t mv = (1ull << from) | t;
	int32_t cas = bd->cas_perms;
	uint64_t checks = 0;

	uint64_t uP = bd->pieces[us][PC_P], uN = bd->pieces[us][PC_N], uB = bd->pieces[us][PC_B];
	uint64_t uR = bd->pieces[us][PC_R], uQ = bd->pieces[us][PC_Q], uK = bd->pieces[us][PC_K];
	if(piece == PC_P) {
		uP ^= mv;
		checks |= PAWN_CAPTURES[them][bd->king_sq[them]] & uP;
	} else if(piece == PC_N) {
		uN ^= mv;
		checks |= KNIGHT_ATTACKS[bd->king_sq[them]] & uN;
	} else if(piece == PC_B) {
		uB ^= mv;
	} else if(piece == PC_R) {
		uR ^= mv;
		cas &= NO_CASTLE_ROOK[from];
	} else if(piece == PC_Q) {
		uQ ^= mv;
	} else {
		uK ^= mv;
		cas &= NO_CASTLE[us];
	}
	uint64_t occ_us = bd->occ[us] ^ mv;

	uint64_t clr = capture ? ~t : FULL_BOARD;
	uint64_t occ_them = bd->occ[them];
	if(capture) {
		occ_them ^= t;
		cas &= NO_CASTLE_ROOK[to];
	}
	uint64_t occB = occ_us | occ_them;
	checks |= slider_checks(uB, uR, uQ, occB, bd->king_sq[them]);

	out->pieces[us][PC_P] = uP; out->pieces[us][PC_N] = uN; out->pieces[us][PC_B] = uB;
	out->pieces[us][PC_R] = uR; out->pieces[us][PC_Q] = uQ; out->pieces[us][PC_K] = uK;
	out->pieces[them][PC_P] = bd->pieces[them][PC_P] & clr;
	out->pieces[them][PC_N] = bd->pieces[them][PC_N] & clr;
	out->pieces[them][PC_B] = bd->pieces[them][PC_B] & clr;
	out->pieces[them][PC_R] = bd->pieces[them][PC_R] & clr;
	out->pieces[them][PC_Q] = bd->pieces[them][PC_Q] & clr;
	out->pieces[them][PC_K] = bd->pieces[them][PC_K];
	out->occ[us] = occ_us; out->occ[them] = occ_them; out->occ_both = occB;
	out->king_sq[us]  = (piece == PC_K) ? to : bd->king_sq[us];
	out->king_sq[them]  = bd->king_sq[them];
	out->checks = checks;
	out->cas_perms = cas;
	out->ep = NOSQUARE;
	out->stm = them;
}

AINLINE void make_promo(struct position *bd, uint32_t from, uint32_t to, struct position *out, uint32_t piece, uint32_t us, uint32_t capture) {
	uint32_t them = us ^ 1u;
	uint64_t f = 1ull << from;
	uint64_t t = 1ull << to;
	int32_t cas = bd->cas_perms;
	uint64_t checks = 0;

	uint64_t uP = bd->pieces[us][PC_P] ^ f;
	uint64_t uN = bd->pieces[us][PC_N], uB = bd->pieces[us][PC_B], uR = bd->pieces[us][PC_R], uQ = bd->pieces[us][PC_Q];
	if(piece == PC_N) {
		uN ^= t;
		checks |= KNIGHT_ATTACKS[bd->king_sq[them]] & uN;
	} else if(piece == PC_B) {
		uB ^= t;
	} else if(piece == PC_R) {
		uR ^= t;
	} else {
		uQ ^= t;
	}
	uint64_t occ_us = bd->occ[us] ^ (f | t);

	uint64_t clr = capture ? ~t : FULL_BOARD;
	uint64_t occ_them = bd->occ[them];
	if(capture) {
		occ_them ^= t;
		cas &= NO_CASTLE_ROOK[to];
	}
	uint64_t occB = occ_us | occ_them;
	checks |= slider_checks(uB, uR, uQ, occB, bd->king_sq[them]);

	out->pieces[us][PC_P] = uP; out->pieces[us][PC_N] = uN; out->pieces[us][PC_B] = uB;
	out->pieces[us][PC_R] = uR; out->pieces[us][PC_Q] = uQ; out->pieces[us][PC_K] = bd->pieces[us][PC_K];
	out->pieces[them][PC_P] = bd->pieces[them][PC_P] & clr;
	out->pieces[them][PC_N] = bd->pieces[them][PC_N] & clr;
	out->pieces[them][PC_B] = bd->pieces[them][PC_B] & clr;
	out->pieces[them][PC_R] = bd->pieces[them][PC_R] & clr;
	out->pieces[them][PC_Q] = bd->pieces[them][PC_Q] & clr;
	out->pieces[them][PC_K] = bd->pieces[them][PC_K];
	out->occ[us] = occ_us; out->occ[them] = occ_them; out->occ_both = occB;
	out->king_sq[us]  = bd->king_sq[us];
	out->king_sq[them]  = bd->king_sq[them];
	out->checks = checks;
	out->cas_perms = cas;
	out->ep = NOSQUARE;
	out->stm = them;
}

AINLINE void make_double(struct position *bd, uint32_t from, uint32_t to, struct position *out, uint32_t us) {
	uint32_t them = us ^ 1u;
	uint64_t mv = (1ull << from) | (1ull << to);
	uint64_t uP = bd->pieces[us][PC_P] ^ mv;
	uint64_t occ_us = bd->occ[us] ^ mv;
	uint64_t occB = occ_us | bd->occ[them];
	uint64_t checks = (PAWN_CAPTURES[them][bd->king_sq[them]] & uP)
	                | slider_checks(bd->pieces[us][PC_B], bd->pieces[us][PC_R], bd->pieces[us][PC_Q], occB, bd->king_sq[them]);

	out->pieces[us][PC_P] = uP; out->pieces[us][PC_N] = bd->pieces[us][PC_N]; out->pieces[us][PC_B] = bd->pieces[us][PC_B];
	out->pieces[us][PC_R] = bd->pieces[us][PC_R]; out->pieces[us][PC_Q] = bd->pieces[us][PC_Q]; out->pieces[us][PC_K] = bd->pieces[us][PC_K];
	out->pieces[them][PC_P] = bd->pieces[them][PC_P]; out->pieces[them][PC_N] = bd->pieces[them][PC_N]; out->pieces[them][PC_B] = bd->pieces[them][PC_B];
	out->pieces[them][PC_R] = bd->pieces[them][PC_R]; out->pieces[them][PC_Q] = bd->pieces[them][PC_Q]; out->pieces[them][PC_K] = bd->pieces[them][PC_K];
	out->occ[us] = occ_us; out->occ[them] = bd->occ[them]; out->occ_both = occB;
	out->king_sq[us]  = bd->king_sq[us];
	out->king_sq[them]  = bd->king_sq[them];
	out->checks = checks;
	out->cas_perms = bd->cas_perms;
	out->ep = (uint32_t)((int32_t)from + PAWN_PUSH[us]);
	out->stm = them;
}

AINLINE void make_ep(struct position *bd, uint32_t from, uint32_t to, struct position *out, uint32_t us) {
	uint32_t them = us ^ 1u;
	uint64_t mv = (1ull << from) | (1ull << to);
	uint64_t cap = 1ull << (uint32_t)((int32_t)to + PAWN_PUSH[them]);
	uint64_t uP = bd->pieces[us][PC_P] ^ mv;
	uint64_t occ_us = bd->occ[us] ^ mv;
	uint64_t eP = bd->pieces[them][PC_P] ^ cap;
	uint64_t occ_them = bd->occ[them] ^ cap;
	uint64_t occB = occ_us | occ_them;
	uint64_t checks = (PAWN_CAPTURES[them][bd->king_sq[them]] & uP)
	                | slider_checks(bd->pieces[us][PC_B], bd->pieces[us][PC_R], bd->pieces[us][PC_Q], occB, bd->king_sq[them]);

	out->pieces[us][PC_P] = uP; out->pieces[us][PC_N] = bd->pieces[us][PC_N]; out->pieces[us][PC_B] = bd->pieces[us][PC_B];
	out->pieces[us][PC_R] = bd->pieces[us][PC_R]; out->pieces[us][PC_Q] = bd->pieces[us][PC_Q]; out->pieces[us][PC_K] = bd->pieces[us][PC_K];
	out->pieces[them][PC_P] = eP; out->pieces[them][PC_N] = bd->pieces[them][PC_N]; out->pieces[them][PC_B] = bd->pieces[them][PC_B];
	out->pieces[them][PC_R] = bd->pieces[them][PC_R]; out->pieces[them][PC_Q] = bd->pieces[them][PC_Q]; out->pieces[them][PC_K] = bd->pieces[them][PC_K];
	out->occ[us] = occ_us; out->occ[them] = occ_them; out->occ_both = occB;
	out->king_sq[us]  = bd->king_sq[us];
	out->king_sq[them]  = bd->king_sq[them];
	out->checks = checks;
	out->cas_perms = bd->cas_perms;
	out->ep = NOSQUARE;
	out->stm = them;
}

AINLINE void make_castle(struct position *bd, uint32_t cs, struct position *out, uint32_t us) {
	uint32_t them = us ^ 1u;
	uint64_t uK = bd->pieces[us][PC_K] ^ CASTLING_KING_SWITCH[cs];
	uint64_t uR = bd->pieces[us][PC_R] ^ CASTLING_ROOK_SWITCH[cs];
	uint64_t occ_us = bd->occ[us] ^ CASTLING_BOTH_SWITCH[cs];
	uint64_t occB = occ_us | bd->occ[them];
	int32_t cas = bd->cas_perms & NO_CASTLE[us];
	uint32_t to = (uint32_t)CASTLING_KING_TARGET[cs];
	uint64_t checks = get_rook(bd->king_sq[them], occB) & uR;

	out->pieces[us][PC_P] = bd->pieces[us][PC_P]; out->pieces[us][PC_N] = bd->pieces[us][PC_N]; out->pieces[us][PC_B] = bd->pieces[us][PC_B];
	out->pieces[us][PC_R] = uR; out->pieces[us][PC_Q] = bd->pieces[us][PC_Q]; out->pieces[us][PC_K] = uK;
	out->pieces[them][PC_P] = bd->pieces[them][PC_P]; out->pieces[them][PC_N] = bd->pieces[them][PC_N]; out->pieces[them][PC_B] = bd->pieces[them][PC_B];
	out->pieces[them][PC_R] = bd->pieces[them][PC_R]; out->pieces[them][PC_Q] = bd->pieces[them][PC_Q]; out->pieces[them][PC_K] = bd->pieces[them][PC_K];
	out->occ[us] = occ_us; out->occ[them] = bd->occ[them]; out->occ_both = occB;
	out->king_sq[us]  = to;
	out->king_sq[them]  = bd->king_sq[them];
	out->checks = checks;
	out->cas_perms = cas;
	out->ep = NOSQUARE;
	out->stm = them;
}

// [=]===^=[ king-danger filter / pins (absolute; side = us) ]========[=]

AINLINE void filter_king(struct position *b, uint64_t *king_attacks, uint64_t *castle_attacks, uint32_t side, uint32_t km) {
	uint32_t them = side ^ 1u;
	uint64_t e_p = b->pieces[them][PC_P];
	uint64_t e_n = b->pieces[them][PC_N];
	uint64_t e_b = b->pieces[them][PC_B];
	uint64_t e_r = b->pieces[them][PC_R];
	uint64_t e_q = b->pieces[them][PC_Q];

	uint64_t attacks = 0;
	attacks |= pawns_left(e_p & ~FIRST_COL, them) | pawns_right(e_p & ~LAST_COL, them);
	attacks |= KING_ATTACKS[b->king_sq[them]];

	uint64_t occB = b->occ_both ^ b->pieces[side][PC_K];

	uint64_t bb;
	if(!km) {
		bb = e_n & KNIGHT_ATTACK_ZONE_CASTLE[side];
	} else {
		bb = e_n & KNIGHT_ATTACK_ZONES[b->king_sq[side]];
	}
	while(bb) {
		attacks |= KNIGHT_ATTACKS[tzc(bb)];
		bb = blsr(bb);
	}

	if(!km) {
		bb = (e_b | e_q) & BISHOP_ZONE_CASTLE[side][_pext_u64(b->occ[side], BISHOP_ZONE_CASTLE_MASK[side])];
	} else {
		bb = (e_b | e_q) & BISHOP_ATTACK_ZONES[b->king_sq[side]][_pext_u64(b->occ[side], KING_ATTACKS[b->king_sq[side]])];
	}
	while(bb) {
		attacks |= get_bishop(tzc(bb), occB);
		bb = blsr(bb);
	}

	if(!km) {
		bb = (e_r | e_q) & ROOK_ZONE_CASTLE[side][_pext_u64(b->occ[side], ROOK_ZONE_CASTLE_MASK[side])];
	} else {
		bb = (e_r | e_q) & ROOK_ATTACK_ZONES[b->king_sq[side]][_pext_u64(b->occ[side], KING_ATTACKS[b->king_sq[side]])];
	}
	while(bb) {
		attacks |= get_rook(tzc(bb), occB);
		bb = blsr(bb);
	}

	*king_attacks &= ~attacks;
	if(!km) {
		*castle_attacks = attacks;
	}
}

AINLINE uint32_t can_castle(struct position *b, uint64_t attacks, uint32_t cs) {
	return (b->cas_perms & CASTLING[cs]) && !(CASTLING_OCCUPIED_SQUARES[cs] & b->occ_both) && !(attacks & CASTLING_PASSING_SQUARES[cs]);
}

AINLINE uint64_t find_pins(uint64_t sliders, uint64_t occB, uint32_t ksq) {
	uint64_t pins = 0;
	while(sliders) {
		uint32_t s = tzc(sliders);
		sliders = blsr(sliders);
		uint64_t mask = PIN_MASKS[s][ksq];
		uint64_t pinned = mask & occB;
		if(popc(pinned) == 1) {
			mask |= 1ull << s;
			pins |= mask;
		}
	}
	return pins;
}

AINLINE uint64_t passant_pin_mask(struct position *b, uint32_t from, uint32_t side) {
	uint32_t them = side ^ 1u;
	if(!(EN_PASSANT_RANK[side] & b->pieces[side][PC_K])) {
		return FULL_BOARD;
	}
	uint64_t occB = b->occ_both;
	uint32_t enemy_pawn = (uint32_t)((int32_t)b->ep + PAWN_PUSH[them]);
	occB &= ~(1ull << enemy_pawn);
	occB &= ~(1ull << from);
	uint32_t sq = tzc(get_rook(b->king_sq[side], occB) & (b->pieces[them][PC_R] | b->pieces[them][PC_Q]));
	return (sq == NOSQUARE) ? FULL_BOARD : 0;
}

// [=]===^=[ fen + state construction (absolute) ]====================[=]

static int32_t piece_for_char(char c) {
	switch(c) {
		case 'P': {
			return PC_P;
		}

		case 'p': {
			return PC_P;
		}

		case 'N': {
			return PC_N;
		}

		case 'n': {
			return PC_N;
		}

		case 'B': {
			return PC_B;
		}

		case 'b': {
			return PC_B;
		}

		case 'R': {
			return PC_R;
		}

		case 'r': {
			return PC_R;
		}

		case 'Q': {
			return PC_Q;
		}

		case 'q': {
			return PC_Q;
		}

		case 'K': {
			return PC_K;
		}

		case 'k': {
			return PC_K;
		}

		default: {
			return -1;
		}
	}
}

static void build_state(struct position *out, uint64_t wp[6], uint64_t bp[6], uint32_t side, int32_t cas, uint32_t ep) {
	uint64_t occ_w = 0;
	uint64_t occ_b = 0;
	for(uint32_t i = 0; i < 6; ++i) {
		out->pieces[WHITE][i] = wp[i];
		out->pieces[BLACK][i] = bp[i];
		occ_w |= wp[i];
		occ_b |= bp[i];
	}
	out->occ[WHITE] = occ_w;
	out->occ[BLACK] = occ_b;
	out->occ_both = occ_w | occ_b;

	uint32_t them = side ^ 1u;
	uint32_t ksq = tzc(out->pieces[side][PC_K]);
	out->king_sq[WHITE] = tzc(out->pieces[WHITE][PC_K]);
	out->king_sq[BLACK] = tzc(out->pieces[BLACK][PC_K]);

	// checks on the side-to-move king from enemy pieces
	uint64_t occ_both = out->occ_both;
	uint64_t king_bit = out->pieces[side][PC_K];
	uint64_t checks = 0;
	uint64_t bbp = out->pieces[them][PC_P];
	while(bbp) {
		uint32_t s = tzc(bbp);
		bbp = blsr(bbp);
		if(PAWN_CAPTURES[them][s] & king_bit) {
			checks |= 1ull << s;
		}
	}
	uint64_t bbn = out->pieces[them][PC_N];
	while(bbn) {
		uint32_t s = tzc(bbn);
		bbn = blsr(bbn);
		if(KNIGHT_ATTACKS[s] & king_bit) {
			checks |= 1ull << s;
		}
	}
	if(get_bishop(ksq, occ_both) & (out->pieces[them][PC_B] | out->pieces[them][PC_Q])) {
		checks |= get_bishop(ksq, occ_both) & (out->pieces[them][PC_B] | out->pieces[them][PC_Q]);
	}
	if(get_rook(ksq, occ_both) & (out->pieces[them][PC_R] | out->pieces[them][PC_Q])) {
		checks |= get_rook(ksq, occ_both) & (out->pieces[them][PC_R] | out->pieces[them][PC_Q]);
	}

	out->checks = checks;
	out->cas_perms = cas;
	out->ep = ep;
	out->stm = side;
}

static int32_t pos_set_fen(struct position *p, char *fen) {
	uint64_t wp[6] = { 0, 0, 0, 0, 0, 0 };
	uint64_t bp[6] = { 0, 0, 0, 0, 0, 0 };
	char *s = fen;

	uint32_t rank = 0;
	uint32_t file = 0;
	while(*s && *s != ' ') {
		char c = *s;
		if(c == '/') {
			++rank;
			file = 0;
			++s;
			continue;
		}
		if(c >= '1' && c <= '8') {
			file += (uint32_t)(c - '0');
			++s;
			continue;
		}
		int32_t pc = piece_for_char(c);
		if(pc < 0) {
			return 1;
		}
		uint32_t sq = rank * 8 + file;
		if(c >= 'a' && c <= 'z') {
			bp[pc] |= 1ull << sq;
		} else {
			wp[pc] |= 1ull << sq;
		}
		++file;
		++s;
	}

	while(*s == ' ') {
		++s;
	}
	uint32_t side = (*s == 'w') ? WHITE : BLACK;
	while(*s && *s != ' ') {
		++s;
	}
	while(*s == ' ') {
		++s;
	}

	int32_t cas = 0;
	while(*s && *s != ' ') {
		switch(*s) {
			case 'K': {
				cas |= 1;
			}
			break;

			case 'Q': {
				cas |= 2;
			}
			break;

			case 'k': {
				cas |= 4;
			}
			break;

			case 'q': {
				cas |= 8;
			}
			break;

			default: {
			}
			break;
		}
		++s;
	}
	while(*s == ' ') {
		++s;
	}

	uint32_t ep = NOSQUARE;
	if(*s && *s != '-') {
		uint32_t f = (uint32_t)(s[0] - 'a');
		uint32_t r = (uint32_t)(8 - (s[1] - '0'));
		ep = r * 8 + f;
	}

	build_state(p, wp, bp, side, cas, ep);
	return 0;
}

static void mc_init(void) {
	init_leaper_tables();
	init_pawn_tables();
	init_slider_tables();
	init_xray_pin_tables();
	init_zone_tables();
}

#endif
