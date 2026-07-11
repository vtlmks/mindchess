// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// mindchess perft kernel. The (side, king-moved) instantiations chessbit gets from
// C++ templates are produced here by including the shared move-enumeration body
// (mindchess_body.inc) into three always_inline templated functions - gen_leaf
// (depth 1), gen_move (depth 2, fuses the leaf), gen_deep (depth>=3) - each
// parameterized by compile-time-constant (side, km, ke). Both gcc and clang
// monomorphize them per instantiation; the thin MK_* wrappers below name the 24
// entry points perft() dispatches to. Keeping leaf/move/deep as SEPARATE functions
// (not one self-recursive body) is load-bearing - clang declines to specialize a
// recursive always_inline body and collapses it to one un-specialized copy.
//
// There is exactly ONE move generator. No density gate, no lazy leaf, no mode
// selection, no timing of any kind: every ordinary depth-2 child uses the same
// delta view, while promotion and other special moves retain copy-make.
// idx = (side<<2)|(km<<1)|ke.

#ifndef MINDCHESS_PERFT_H
#define MINDCHESS_PERFT_H
#include "mindchess.h"

#define MOVES(PC, FRM, ATT, RECUR) do { \
		uint64_t _q = (ATT) & ~MC_O(MC_S ^ 1u); \
		while(_q) { \
			to = tzc(_q); \
			_q = blsr(_q); \
			nodes += GENERIC((FRM), to, (PC), 0u, RECUR); \
		} \
		uint64_t _c = (ATT) & MC_O(MC_S ^ 1u); \
		while(_c) { \
			to = tzc(_c); \
			_c = blsr(_c); \
			nodes += GENERIC((FRM), to, (PC), 1u, RECUR); \
		} \
	} while(0)

#define MC_P(C, P) b->pieces[(C)][(P)]
#define MC_O(C) b->occ[(C)]
#define MC_OB b->occ_both
#define MC_K(C) b->king_sq[(C)]
#define MC_CH b->checks
#define MC_EP b->ep
#define MC_FILTER(KA, CA) filter_king(b, (KA), (CA), MC_S, MC_KM)
#define MC_PASSANT(F) passant_pin_mask(b, (F), MC_S)
#define MC_CAN_CASTLE(A, CS) can_castle(b, (A), (CS))

static uint64_t perft_2_0_0_0(struct position *b); static uint64_t perft_2_0_0_1(struct position *b);
static uint64_t perft_2_0_1_0(struct position *b); static uint64_t perft_2_0_1_1(struct position *b);
static uint64_t perft_2_1_0_0(struct position *b); static uint64_t perft_2_1_0_1(struct position *b);
static uint64_t perft_2_1_1_0(struct position *b); static uint64_t perft_2_1_1_1(struct position *b);

static uint64_t perft_deep_0_0_0(struct position *b, uint32_t depth); static uint64_t perft_deep_0_0_1(struct position *b, uint32_t depth);
static uint64_t perft_deep_0_1_0(struct position *b, uint32_t depth); static uint64_t perft_deep_0_1_1(struct position *b, uint32_t depth);
static uint64_t perft_deep_1_0_0(struct position *b, uint32_t depth); static uint64_t perft_deep_1_0_1(struct position *b, uint32_t depth);
static uint64_t perft_deep_1_1_0(struct position *b, uint32_t depth); static uint64_t perft_deep_1_1_1(struct position *b, uint32_t depth);

struct leaf_delta {
	struct position *b;
	uint64_t mv;
	uint64_t clr;
	uint64_t occ[2];
	uint64_t occ_both;
	uint64_t checks;
	uint64_t knight_moves;
	uint32_t king_sq[2];
	int32_t cas;
	uint32_t us;
	uint32_t piece;
	uint32_t ep;
};

AINLINE uint64_t delta_piece(struct leaf_delta *d, uint32_t side, uint32_t piece) {
	uint64_t value = d->b->pieces[side][piece];
	if(side == d->us) {
		if(piece == d->piece) {
			value ^= d->mv;
		}
		return value;
	}
	return value & d->clr;
}

AINLINE void filter_king_delta(struct leaf_delta *d, uint64_t *king_attacks, uint64_t *castle_attacks, uint32_t side, uint32_t km) {
	uint32_t them = side ^ 1u;
	uint64_t e_p = delta_piece(d, them, PC_P);
	uint64_t e_n = delta_piece(d, them, PC_N);
	uint64_t e_b = delta_piece(d, them, PC_B);
	uint64_t e_r = delta_piece(d, them, PC_R);
	uint64_t e_q = delta_piece(d, them, PC_Q);
	uint64_t attacks = pawns_left(e_p & ~FIRST_COL, them) | pawns_right(e_p & ~LAST_COL, them);
	attacks |= KING_ATTACKS[d->king_sq[them]];
	uint64_t occB = d->occ_both ^ delta_piece(d, side, PC_K);
	uint64_t bb;
	if(!km) {
		bb = e_n & KNIGHT_ATTACK_ZONE_CASTLE[side];

	} else {
		bb = e_n & KNIGHT_ATTACK_ZONES[d->king_sq[side]];
	}
	// tzc(0) is NOSQUARE and KNIGHT_ATTACKS[NOSQUARE] is the zero sentinel.
	attacks |= KNIGHT_ATTACKS[tzc(bb)];
	bb = blsr(bb);
	while(bb) {
		attacks |= KNIGHT_ATTACKS[tzc(bb)];
		bb = blsr(bb);
	}
	if(!km) {
		bb = (e_b | e_q) & BISHOP_ZONE_CASTLE[side][_pext_u64(d->occ[side], BISHOP_ZONE_CASTLE_MASK[side])];

	} else {
		bb = (e_b | e_q) & BISHOP_ATTACK_ZONES[d->king_sq[side]][_pext_u64(d->occ[side], KING_ATTACKS[d->king_sq[side]])];
	}
	while(bb) {
		attacks |= get_bishop(tzc(bb), occB);
		bb = blsr(bb);
	}
	if(!km) {
		bb = (e_r | e_q) & ROOK_ZONE_CASTLE[side][_pext_u64(d->occ[side], ROOK_ZONE_CASTLE_MASK[side])];

	} else {
		bb = (e_r | e_q) & ROOK_ATTACK_ZONES[d->king_sq[side]][_pext_u64(d->occ[side], KING_ATTACKS[d->king_sq[side]])];
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

AINLINE uint64_t passant_pin_mask_delta(struct leaf_delta *d, uint32_t from, uint32_t side) {
	uint32_t them = side ^ 1u;
	if(!(EN_PASSANT_RANK[side] & delta_piece(d, side, PC_K))) {
		return FULL_BOARD;
	}
	uint64_t occB = d->occ_both;
	uint32_t enemy_pawn = (uint32_t)((int32_t)d->ep + PAWN_PUSH[them]);
	occB &= ~(1ull << enemy_pawn);
	occB &= ~(1ull << from);
	uint32_t sq = tzc(get_rook(d->king_sq[side], occB) & (delta_piece(d, them, PC_R) | delta_piece(d, them, PC_Q)));
	return (sq == NOSQUARE) ? FULL_BOARD : 0;
}

// [=]===^=[ depth-2 dispatch (make-recurse only) ]==================[=]
AINLINE uint64_t perft2(struct position *nb, uint32_t idx) {
	switch(idx) {
		case 0: {
			return perft_2_0_0_0(nb);
		}

		case 1: {
			return perft_2_0_0_1(nb);
		}

		case 2: {
			return perft_2_0_1_0(nb);
		}

		case 3: {
			return perft_2_0_1_1(nb);
		}

		case 4: {
			return perft_2_1_0_0(nb);
		}

		case 5: {
			return perft_2_1_0_1(nb);
		}

		case 6: {
			return perft_2_1_1_0(nb);
		}

		default: {
			return perft_2_1_1_1(nb);
		}
	}
}

// [=]===^=[ gen_leaf: depth-1 count (terminal) ]=====================[=]
AINLINE uint64_t gen_leaf(struct position *b, uint32_t side, uint32_t kmv) {
#define MC_S side
#define MC_KM kmv
#define MC_LEAF 1
#define RN(nbp) 0
#define RK(nbp) 0
#define GENERIC(F, T, P, C, R) 0
#define PROMOTION(F, T, P, C, R) 0
#define DOUBLE(F, T, R) 0
#define MC_DELTA_VIEW 0
#include "mindchess_body.inc"
#undef MC_S
#undef MC_KM
#undef MC_LEAF
#undef RN
#undef RK
#undef GENERIC
#undef PROMOTION
#undef DOUBLE
#undef MC_DELTA_VIEW
}

#undef MC_P
#undef MC_O
#undef MC_OB
#undef MC_K
#undef MC_CH
#undef MC_EP
#undef MC_FILTER
#undef MC_PASSANT
#undef MC_CAN_CASTLE

#define MC_P(C, P) delta_piece(&d, (C), (P))
#define MC_O(C) d.occ[(C)]
#define MC_OB d.occ_both
#define MC_K(C) d.king_sq[(C)]
#define MC_CH d.checks
#define MC_EP d.ep
#define MC_FILTER(KA, CA) filter_king_delta(&d, (KA), (CA), MC_S, MC_KM)
#define MC_PASSANT(F) passant_pin_mask_delta(&d, (F), MC_S)
#define MC_CAN_CASTLE(A, CS) ((d.cas & CASTLING[(CS)]) && !(CASTLING_OCCUPIED_SQUARES[(CS)] & d.occ_both) && !((A) & CASTLING_PASSING_SQUARES[(CS)]))

AINLINE uint64_t gen_leaf_delta(struct position *b, uint32_t mfrom, uint32_t mto, uint32_t piece, uint32_t capture, uint32_t us, uint32_t kmv, uint32_t ep, uint64_t knight_moves) {
	uint32_t them = us ^ 1u;
	uint64_t t = 1ull << mto;
	struct leaf_delta d;
	d.b = b;
	d.mv = (1ull << mfrom) | t;
	d.clr = capture ? ~t : FULL_BOARD;
	d.us = us;
	d.piece = piece;
	d.occ[us] = b->occ[us] ^ d.mv;
	d.occ[them] = capture ? b->occ[them] ^ t : b->occ[them];
	d.occ_both = d.occ[us] | d.occ[them];
#if defined(__clang__)
	d.knight_moves = knight_moves;
	if(capture) {
		uint64_t old_n = b->pieces[them][PC_N];
		if(old_n) {
			d.knight_moves += popc(KNIGHT_ATTACKS[mto] & old_n);
			if(old_n & t) {
				d.knight_moves -= popc(KNIGHT_ATTACKS[mto] & ~b->occ[them]);
			}
		}
	}
#endif
	d.king_sq[us] = (piece == PC_K) ? mto : b->king_sq[us];
	d.king_sq[them] = b->king_sq[them];
	d.cas = b->cas_perms;
	if(piece == PC_R) {
		d.cas &= NO_CASTLE_ROOK[mfrom];
	}
	if(piece == PC_K) {
		d.cas &= NO_CASTLE[us];
	}
	if(capture) {
		d.cas &= NO_CASTLE_ROOK[mto];
	}
	d.ep = ep;
	d.checks = 0;
	if(piece == PC_P) {
		d.checks |= PAWN_CAPTURES[them][d.king_sq[them]] & delta_piece(&d, us, PC_P);

	} else if(piece == PC_N) {
		d.checks |= KNIGHT_ATTACKS[d.king_sq[them]] & delta_piece(&d, us, PC_N);
	}
	d.checks |= slider_checks(delta_piece(&d, us, PC_B), delta_piece(&d, us, PC_R), delta_piece(&d, us, PC_Q), d.occ_both, d.king_sq[them]);
#define MC_S (us ^ 1u)
#define MC_KM kmv
#define MC_LEAF 1
#define RN(nbp) 0
#define RK(nbp) 0
#define GENERIC(F, T, P, C, R) 0
#define PROMOTION(F, T, P, C, R) 0
#define DOUBLE(F, T, R) 0
#define MC_DELTA_VIEW 1
#include "mindchess_body.inc"
#undef MC_S
#undef MC_KM
#undef MC_LEAF
#undef RN
#undef RK
#undef GENERIC
#undef PROMOTION
#undef DOUBLE
#undef MC_DELTA_VIEW
}

#undef MC_P
#undef MC_O
#undef MC_OB
#undef MC_K
#undef MC_CH
#undef MC_EP
#undef MC_FILTER
#undef MC_PASSANT
#undef MC_CAN_CASTLE

#define MC_P(C, P) b->pieces[(C)][(P)]
#define MC_O(C) b->occ[(C)]
#define MC_OB b->occ_both
#define MC_K(C) b->king_sq[(C)]
#define MC_CH b->checks
#define MC_EP b->ep
#define MC_FILTER(KA, CA) filter_king(b, (KA), (CA), MC_S, MC_KM)
#define MC_PASSANT(F) passant_pin_mask(b, (F), MC_S)
#define MC_CAN_CASTLE(A, CS) can_castle(b, (A), (CS))

// [=]===^=[ gen_move: depth-2, fuses the leaf child ]================[=]
AINLINE uint64_t gen_move(struct position *b, uint32_t side, uint32_t kmv, uint32_t kev) {
	uint64_t leaf_knight_moves = 0;
#if defined(__clang__)
	uint64_t leaf_knights = b->pieces[side ^ 1u][PC_N];
	while(leaf_knights) {
		uint32_t sq = tzc(leaf_knights);
		leaf_knights = blsr(leaf_knights);
		leaf_knight_moves += popc(KNIGHT_ATTACKS[sq] & ~b->occ[side ^ 1u]);
	}
#endif
#define MC_S side
#define MC_KM kmv
#define MC_LEAF 0
#define RN(nbp) gen_leaf((nbp), (side) ^ 1u, kev)
#define RK(nbp) gen_leaf((nbp), (side) ^ 1u, kev)
#define GENERIC(F, T, P, C, R) gen_leaf_delta(b, (F), (T), (P), (C), side, kev, NOSQUARE, leaf_knight_moves)
#define PROMOTION(F, T, P, C, R) (make_promo(b, (F), (T), &nb, (P), side, (C)), R(&nb))
#define DOUBLE(F, T, R) gen_leaf_delta(b, (F), (T), PC_P, 0u, side, kev, (uint32_t)((int32_t)(F) + PAWN_PUSH[side]), leaf_knight_moves)
#define MC_DELTA_VIEW 0
#include "mindchess_body.inc"
#undef MC_S
#undef MC_KM
#undef MC_LEAF
#undef RN
#undef RK
#undef GENERIC
#undef PROMOTION
#undef DOUBLE
#undef MC_DELTA_VIEW
}

// [=]===^=[ deep child dispatch (const-folded per call site) ]=======[=]
AINLINE uint64_t deep_child(struct position *nb, uint32_t cs, uint32_t ckm, uint32_t cke, uint32_t depth) {
	uint32_t i = (cs << 2) | (ckm << 1) | cke;
	if(depth == 3) {
		return perft2(nb, i);
	}
	switch(i) {
		case 0: {
			return perft_deep_0_0_0(nb, depth - 1);
		}

		case 1: {
			return perft_deep_0_0_1(nb, depth - 1);
		}

		case 2: {
			return perft_deep_0_1_0(nb, depth - 1);
		}

		case 3: {
			return perft_deep_0_1_1(nb, depth - 1);
		}

		case 4: {
			return perft_deep_1_0_0(nb, depth - 1);
		}

		case 5: {
			return perft_deep_1_0_1(nb, depth - 1);
		}

		case 6: {
			return perft_deep_1_1_0(nb, depth - 1);
		}

		default: {
			return perft_deep_1_1_1(nb, depth - 1);
		}
	}
}

// [=]===^=[ gen_deep: depth>=3, make-recurse ]=======================[=]
AINLINE uint64_t gen_deep(struct position *b, uint32_t side, uint32_t kmv, uint32_t kev, uint32_t depth) {
#define MC_S side
#define MC_KM kmv
#define MC_LEAF 0
#define RN(nbp) deep_child((nbp), (side) ^ 1u, kev, kmv, depth)
#define RK(nbp) deep_child((nbp), (side) ^ 1u, kev, 1u, depth)
#define GENERIC(F, T, P, C, R) (make_generic(b, (F), (T), &nb, (P), side, (C)), R(&nb))
#define PROMOTION(F, T, P, C, R) (make_promo(b, (F), (T), &nb, (P), side, (C)), R(&nb))
#define DOUBLE(F, T, R) (make_double(b, (F), (T), &nb, side), R(&nb))
#define MC_DELTA_VIEW 0
#include "mindchess_body.inc"
#undef MC_S
#undef MC_KM
#undef MC_LEAF
#undef RN
#undef RK
#undef GENERIC
#undef PROMOTION
#undef DOUBLE
#undef MC_DELTA_VIEW
}

#undef MOVES
#undef MC_P
#undef MC_O
#undef MC_OB
#undef MC_K
#undef MC_CH
#undef MC_EP
#undef MC_FILTER
#undef MC_PASSANT
#undef MC_CAN_CASTLE

#define MK_LEAF(s, km, ke) AINLINE uint64_t perft_1_##s##_##km##_##ke(struct position *b) { return gen_leaf(b, s##u, km##u); }
#define MK_D2(s, km, ke)   static uint64_t perft_2_##s##_##km##_##ke(struct position *b) { return gen_move(b, s##u, km##u, ke##u); }
#define MK_DEEP(s, km, ke) static uint64_t perft_deep_##s##_##km##_##ke(struct position *b, uint32_t depth) { return gen_deep(b, s##u, km##u, ke##u, depth); }
MK_LEAF(0,0,0) MK_LEAF(0,0,1) MK_LEAF(0,1,0) MK_LEAF(0,1,1)
MK_LEAF(1,0,0) MK_LEAF(1,0,1) MK_LEAF(1,1,0) MK_LEAF(1,1,1)
MK_D2(0,0,0) MK_D2(0,0,1) MK_D2(0,1,0) MK_D2(0,1,1)
MK_D2(1,0,0) MK_D2(1,0,1) MK_D2(1,1,0) MK_D2(1,1,1)
MK_DEEP(0,0,0) MK_DEEP(0,0,1) MK_DEEP(0,1,0) MK_DEEP(0,1,1)
MK_DEEP(1,0,0) MK_DEEP(1,0,1) MK_DEEP(1,1,0) MK_DEEP(1,1,1)
#undef MK_LEAF
#undef MK_D2
#undef MK_DEEP

static uint64_t perft_run(struct position *p, uint32_t depth) {
	if(depth == 0) {
		return 1;
	}
	uint32_t side = p->stm;
	uint32_t km = ((p->cas_perms & CAS_BOTH[side]) == 0) ? 1u : 0u;
	uint32_t ke = ((p->cas_perms & CAS_BOTH[side ^ 1]) == 0) ? 1u : 0u;
	uint32_t idx = (km << 1) | ke;
	if(depth == 1) {
		if(side == WHITE) {
			switch(idx) {
				case 0: {
					return perft_1_0_0_0(p);
				}

				case 1: {
					return perft_1_0_0_1(p);
				}

				case 2: {
					return perft_1_0_1_0(p);
				}

				default: {
					return perft_1_0_1_1(p);
				}
			}
		}
		switch(idx) {
			case 0: {
				return perft_1_1_0_0(p);
			}

			case 1: {
				return perft_1_1_0_1(p);
			}

			case 2: {
				return perft_1_1_1_0(p);
			}

			default: {
				return perft_1_1_1_1(p);
			}
		}
	}
	if(depth == 2) {
		return perft2(p, (side << 2) | idx);
	}
	if(side == WHITE) {
		switch(idx) {
			case 0: {
				return perft_deep_0_0_0(p, depth);
			}

			case 1: {
				return perft_deep_0_0_1(p, depth);
			}

			case 2: {
				return perft_deep_0_1_0(p, depth);
			}

			default: {
				return perft_deep_0_1_1(p, depth);
			}
		}
	}
	switch(idx) {
		case 0: {
			return perft_deep_1_0_0(p, depth);
		}

		case 1: {
			return perft_deep_1_0_1(p, depth);
		}

		case 2: {
			return perft_deep_1_1_0(p, depth);
		}

		default: {
			return perft_deep_1_1_1(p, depth);
		}
	}
}

// [=]===^=[ perft entry ]===========================================[=]
static uint64_t perft(struct position *p, uint32_t depth) {
	return perft_run(p, depth);
}

#endif
