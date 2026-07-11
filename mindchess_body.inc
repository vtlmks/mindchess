// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
//
// Shared move-enumeration body for the mindchess perft kernel (ABSOLUTE board). Included by each
// (side, king-moved) instantiation with MC_S / MC_KM / MC_LEAF set to compile-time
// constants and RN()/RK() bound to the recursion target. One source, monomorphized
// per instantiation by the compiler - no code generator, the kernel lives once here.
	uint64_t nodes = 0;
	uint32_t from, to;
	uint64_t bb, attacks;
	struct position nb;
	uint64_t castle_attacks = 0;
	attacks = KING_ATTACKS[MC_K(MC_S)] & ~MC_O(MC_S);
	MC_FILTER(&attacks, &castle_attacks);
	if(MC_LEAF) {
		nodes += popc(attacks);

	} else {
		MOVES(PC_K, MC_K(MC_S), attacks, RK);
	}

	if(__builtin_expect(MC_CH != 0, 0)) {
		uint32_t check_sq = tzc(MC_CH);
		if(popc(MC_CH) == 1) {
			uint64_t b_pins = find_pins((MC_P(MC_S ^ 1u, PC_B) | MC_P(MC_S ^ 1u, PC_Q)) & BISHOP_XRAYS[MC_K(MC_S)] & ~MC_CH, MC_OB, MC_K(MC_S));
			uint64_t r_pins = find_pins((MC_P(MC_S ^ 1u, PC_R) | MC_P(MC_S ^ 1u, PC_Q)) & ROOK_XRAYS[MC_K(MC_S)] & ~MC_CH, MC_OB, MC_K(MC_S));
			uint64_t all_pins = b_pins | r_pins;
			if(MC_CH & (MC_P(MC_S ^ 1u, PC_P) | MC_P(MC_S ^ 1u, PC_N))) {
				uint64_t pawns = MC_P(MC_S, PC_P) & ~all_pins;
				uint64_t en_passant = pawns & PASSANT_CAPTURES[MC_EP];
				uint64_t caps = pawns & PAWN_CAPTURES[MC_S ^ 1u][check_sq];
				uint64_t promos = caps & PROMO_RANKS[MC_S];
				if(MC_LEAF) {
					if(promos) {
						nodes += popc(promos) << 2;

					} else {
						nodes += popc(en_passant | caps);
					}

				} else {
					to = check_sq;
					caps ^= promos;
					while(en_passant) {
						from = tzc(en_passant);
						en_passant = blsr(en_passant);
						make_ep(b, from, MC_EP, &nb, MC_S);
						nodes += RN(&nb);
					}
					while(promos) {
						from = tzc(promos);
						promos = blsr(promos);
						nodes += PROMOTION(from, to, PC_N, 1u, RN);
						nodes += PROMOTION(from, to, PC_B, 1u, RN);
						nodes += PROMOTION(from, to, PC_R, 1u, RN);
						nodes += PROMOTION(from, to, PC_Q, 1u, RN);
					}
					while(caps) {
						from = tzc(caps);
						caps = blsr(caps);
						nodes += GENERIC(from, to, PC_P, 1u, RN);
					}
				}
				bb = MC_P(MC_S, PC_N) & ~all_pins;
				if(bb) {
					attacks = KNIGHT_ATTACKS[check_sq] & bb;
					if(MC_LEAF) {
						nodes += popc(attacks);

					} else {
						while(attacks) {
							from = tzc(attacks);
							attacks = blsr(attacks);
							nodes += GENERIC(from, check_sq, PC_N, 1u, RN);
						}
					}
				}
				bb = MC_P(MC_S, PC_B) & ~all_pins;
				if(bb) {
					attacks = get_bishop(check_sq, MC_OB) & bb;
					if(MC_LEAF) {
						nodes += popc(attacks);

					} else {
						while(attacks) {
							from = tzc(attacks);
							attacks = blsr(attacks);
							nodes += GENERIC(from, check_sq, PC_B, 1u, RN);
						}
					}
				}
				bb = MC_P(MC_S, PC_R) & ~all_pins;
				if(bb) {
					attacks = get_rook(check_sq, MC_OB) & bb;
					if(MC_LEAF) {
						nodes += popc(attacks);

					} else {
						while(attacks) {
							from = tzc(attacks);
							attacks = blsr(attacks);
							nodes += GENERIC(from, check_sq, PC_R, 1u, RN);
						}
					}
				}
				bb = MC_P(MC_S, PC_Q) & ~all_pins;
				if(bb) {
					attacks = get_queen(check_sq, MC_OB) & bb;
					if(MC_LEAF) {
						nodes += popc(attacks);

					} else {
						while(attacks) {
							from = tzc(attacks);
							attacks = blsr(attacks);
							nodes += GENERIC(from, check_sq, PC_Q, 1u, RN);
						}
					}
				}

			} else {
				uint64_t valid_squares = MC_CH | PIN_MASKS[check_sq][MC_K(MC_S)];
				uint64_t pawns = MC_P(MC_S, PC_P) & ~all_pins;
				uint64_t pawns_l = pawns_left(pawns & ~FIRST_COL, MC_S) & MC_O(MC_S ^ 1u) & valid_squares;
				uint64_t pawns_r = pawns_right(pawns & ~LAST_COL, MC_S) & MC_O(MC_S ^ 1u) & valid_squares;
				uint64_t pawns_f = pawns_fwd(pawns, MC_S) & ~MC_OB;
				uint64_t pawns_d = pawns_fwd(pawns_f & FIRST_PUSH_RANK[MC_S], MC_S) & ~MC_OB & valid_squares;
				pawns_f &= valid_squares;
				if(__builtin_expect(((pawns_l | pawns_r | pawns_f) & LAST_RANKS[MC_S]) != 0, 0)) {
					uint64_t pl = pawns_l & LAST_RANKS[MC_S];
					uint64_t pr = pawns_r & LAST_RANKS[MC_S];
					uint64_t pf = pawns_f & LAST_RANKS[MC_S];
					pawns_l ^= pl;
					pawns_r ^= pr;
					pawns_f ^= pf;
					if(MC_LEAF) {
						nodes += (popc(pl) + popc(pr | pf)) << 2;

					} else {
						while(pl) {
							to = tzc(pl);
							pl = blsr(pl);
							from = (uint32_t)((int32_t)to + PAWN_RIGHT[MC_S ^ 1u]);
							nodes += PROMOTION(from, to, PC_N, 1u, RN);
							nodes += PROMOTION(from, to, PC_B, 1u, RN);
							nodes += PROMOTION(from, to, PC_R, 1u, RN);
							nodes += PROMOTION(from, to, PC_Q, 1u, RN);
						}
						while(pr) {
							to = tzc(pr);
							pr = blsr(pr);
							from = (uint32_t)((int32_t)to + PAWN_LEFT[MC_S ^ 1u]);
							nodes += PROMOTION(from, to, PC_N, 1u, RN);
							nodes += PROMOTION(from, to, PC_B, 1u, RN);
							nodes += PROMOTION(from, to, PC_R, 1u, RN);
							nodes += PROMOTION(from, to, PC_Q, 1u, RN);
						}
						while(pf) {
							to = tzc(pf);
							pf = blsr(pf);
							from = (uint32_t)((int32_t)to + PAWN_PUSH[MC_S ^ 1u]);
							nodes += PROMOTION(from, to, PC_N, 0u, RN);
							nodes += PROMOTION(from, to, PC_B, 0u, RN);
							nodes += PROMOTION(from, to, PC_R, 0u, RN);
							nodes += PROMOTION(from, to, PC_Q, 0u, RN);
						}
					}
				}
				if(MC_LEAF) {
					nodes += popc(pawns_l) + popc(pawns_r | pawns_f | pawns_d);

				} else {
					while(pawns_l) {
						to = tzc(pawns_l);
						pawns_l = blsr(pawns_l);
						from = (uint32_t)((int32_t)to + PAWN_RIGHT[MC_S ^ 1u]);
						nodes += GENERIC(from, to, PC_P, 1u, RN);
					}
					while(pawns_r) {
						to = tzc(pawns_r);
						pawns_r = blsr(pawns_r);
						from = (uint32_t)((int32_t)to + PAWN_LEFT[MC_S ^ 1u]);
						nodes += GENERIC(from, to, PC_P, 1u, RN);
					}
					while(pawns_f) {
						to = tzc(pawns_f);
						pawns_f = blsr(pawns_f);
						from = (uint32_t)((int32_t)to + PAWN_PUSH[MC_S ^ 1u]);
						nodes += GENERIC(from, to, PC_P, 0u, RN);
					}
					while(pawns_d) {
						to = tzc(pawns_d);
						pawns_d = blsr(pawns_d);
						from = (uint32_t)((int32_t)to + PAWN_DOUBLE_PUSH[MC_S ^ 1u]);
						nodes += DOUBLE(from, to, RN);
					}
				}
				bb = MC_P(MC_S, PC_N) & ~all_pins;
				while(bb) {
					from = tzc(bb);
					bb = blsr(bb);
					attacks = KNIGHT_ATTACKS[from] & valid_squares;
					if(MC_LEAF) {
						nodes += popc(attacks);

					} else {
						MOVES(PC_N, from, attacks, RN);
					}
				}
				bb = MC_P(MC_S, PC_B) & ~all_pins;
				while(bb) {
					from = tzc(bb);
					bb = blsr(bb);
					attacks = get_bishop(from, MC_OB) & valid_squares;
					if(MC_LEAF) {
						nodes += popc(attacks);

					} else {
						MOVES(PC_B, from, attacks, RN);
					}
				}
				bb = MC_P(MC_S, PC_R) & ~all_pins;
				while(bb) {
					from = tzc(bb);
					bb = blsr(bb);
					attacks = get_rook(from, MC_OB) & valid_squares;
					if(MC_LEAF) {
						nodes += popc(attacks);

					} else {
						MOVES(PC_R, from, attacks, RN);
					}
				}
				bb = MC_P(MC_S, PC_Q) & ~all_pins;
				while(bb) {
					from = tzc(bb);
					bb = blsr(bb);
					attacks = get_queen(from, MC_OB) & valid_squares;
					if(MC_LEAF) {
						nodes += popc(attacks);

					} else {
						MOVES(PC_Q, from, attacks, RN);
					}
				}
			}
		}
		return nodes;
	}

	uint64_t b_pins = find_pins((MC_P(MC_S ^ 1u, PC_B) | MC_P(MC_S ^ 1u, PC_Q)) & BISHOP_XRAYS[MC_K(MC_S)], MC_OB, MC_K(MC_S));
	uint64_t r_pins = find_pins((MC_P(MC_S ^ 1u, PC_R) | MC_P(MC_S ^ 1u, PC_Q)) & ROOK_XRAYS[MC_K(MC_S)], MC_OB, MC_K(MC_S));
	uint64_t all_pins = b_pins | r_pins;
	uint64_t my_pawns = MC_P(MC_S, PC_P);
	// Most nodes are pinless, where the b_pins/r_pins split is pure waste.
	// Keep that common path small without changing the legal-move algorithm.
	uint64_t pawns_l_all, pawns_r_all, pawns_f;
#if MC_DELTA_VIEW && defined(__clang__)
	if(__builtin_expect(all_pins != 0, 0)) {
#else
	if(all_pins) {
#endif
		uint64_t pawns_atk = my_pawns & ~r_pins;
		uint64_t pawns_push = my_pawns & ~b_pins;
		pawns_l_all = pawns_left(pawns_atk & ~b_pins & ~FIRST_COL, MC_S) | (pawns_left(pawns_atk & b_pins & ~FIRST_COL, MC_S) & b_pins);
		pawns_r_all = pawns_right(pawns_atk & ~b_pins & ~LAST_COL, MC_S) | (pawns_right(pawns_atk & b_pins & ~LAST_COL, MC_S) & b_pins);
		pawns_f = (pawns_fwd(pawns_push & ~r_pins, MC_S) & ~MC_OB) | (pawns_fwd(pawns_push & r_pins, MC_S) & ~MC_OB & r_pins);

	} else {
		pawns_l_all = pawns_left(my_pawns & ~FIRST_COL, MC_S);
		pawns_r_all = pawns_right(my_pawns & ~LAST_COL, MC_S);
		pawns_f = pawns_fwd(my_pawns, MC_S) & ~MC_OB;
	}
	uint64_t pawns_l = pawns_l_all & MC_O(MC_S ^ 1u);
	uint64_t pawns_r = pawns_r_all & MC_O(MC_S ^ 1u);
	uint64_t pawns_d = pawns_fwd(pawns_f & FIRST_PUSH_RANK[MC_S], MC_S) & ~MC_OB;
	if(__builtin_expect(MC_EP != NOSQUARE, 0)) {
		uint64_t ep_bit = 1ull << MC_EP;
		uint64_t epp = pawns_right(pawns_l_all & ep_bit, MC_S ^ 1u) | pawns_left(pawns_r_all & ep_bit, MC_S ^ 1u);
		while(epp) {
			from = tzc(epp);
			epp = blsr(epp);
			if(MC_LEAF) {
				nodes += popc(ep_bit & MC_PASSANT(from));

			} else {
				if(ep_bit & MC_PASSANT(from)) {
					make_ep(b, from, MC_EP, &nb, MC_S);
					nodes += RN(&nb);
				}
			}
		}
	}
#if MC_DELTA_VIEW
	nodes += popc(pawns_l) + popc(pawns_r | pawns_f | pawns_d);
#if defined(__clang__)
	if((my_pawns & PROMO_RANKS[MC_S]) != 0) {
		nodes += 3u * (popc(pawns_l & LAST_RANKS[MC_S]) + popc((pawns_r | pawns_f) & LAST_RANKS[MC_S]));
	}
#else
	nodes += 3u * (popc(pawns_l & LAST_RANKS[MC_S]) + popc((pawns_r | pawns_f) & LAST_RANKS[MC_S]));
#endif
#else
	if((pawns_l | pawns_r | pawns_f) & LAST_RANKS[MC_S]) {
		uint64_t pl = pawns_l & LAST_RANKS[MC_S];
		uint64_t pr = pawns_r & LAST_RANKS[MC_S];
		uint64_t pf = pawns_f & LAST_RANKS[MC_S];
		pawns_l ^= pl;
		pawns_r ^= pr;
		pawns_f ^= pf;
		if(MC_LEAF) {
			nodes += (popc(pl) + popc(pr | pf)) << 2;

		} else {
			while(pl) {
				to = tzc(pl);
				pl = blsr(pl);
				from = (uint32_t)((int32_t)to + PAWN_RIGHT[MC_S ^ 1u]);
				nodes += PROMOTION(from, to, PC_N, 1u, RN);
				nodes += PROMOTION(from, to, PC_B, 1u, RN);
				nodes += PROMOTION(from, to, PC_R, 1u, RN);
				nodes += PROMOTION(from, to, PC_Q, 1u, RN);
			}
			while(pr) {
				to = tzc(pr);
				pr = blsr(pr);
				from = (uint32_t)((int32_t)to + PAWN_LEFT[MC_S ^ 1u]);
				nodes += PROMOTION(from, to, PC_N, 1u, RN);
				nodes += PROMOTION(from, to, PC_B, 1u, RN);
				nodes += PROMOTION(from, to, PC_R, 1u, RN);
				nodes += PROMOTION(from, to, PC_Q, 1u, RN);
			}
			while(pf) {
				to = tzc(pf);
				pf = blsr(pf);
				from = (uint32_t)((int32_t)to + PAWN_PUSH[MC_S ^ 1u]);
				nodes += PROMOTION(from, to, PC_N, 0u, RN);
				nodes += PROMOTION(from, to, PC_B, 0u, RN);
				nodes += PROMOTION(from, to, PC_R, 0u, RN);
				nodes += PROMOTION(from, to, PC_Q, 0u, RN);
			}
		}
	}
	if(MC_LEAF) {
		nodes += popc(pawns_l) + popc(pawns_r | pawns_f | pawns_d);

	} else {
		while(pawns_l) {
			to = tzc(pawns_l);
			pawns_l = blsr(pawns_l);
			from = (uint32_t)((int32_t)to + PAWN_RIGHT[MC_S ^ 1u]);
			nodes += GENERIC(from, to, PC_P, 1u, RN);
		}
		while(pawns_r) {
			to = tzc(pawns_r);
			pawns_r = blsr(pawns_r);
			from = (uint32_t)((int32_t)to + PAWN_LEFT[MC_S ^ 1u]);
			nodes += GENERIC(from, to, PC_P, 1u, RN);
		}
		while(pawns_f) {
			to = tzc(pawns_f);
			pawns_f = blsr(pawns_f);
			from = (uint32_t)((int32_t)to + PAWN_PUSH[MC_S ^ 1u]);
			nodes += GENERIC(from, to, PC_P, 0u, RN);
		}
		while(pawns_d) {
			to = tzc(pawns_d);
			pawns_d = blsr(pawns_d);
			from = (uint32_t)((int32_t)to + PAWN_DOUBLE_PUSH[MC_S ^ 1u]);
			nodes += DOUBLE(from, to, RN);
		}
	}
#endif
	bb = MC_P(MC_S, PC_N) & ~all_pins;
#if MC_DELTA_VIEW && defined(__clang__)
	nodes += d.knight_moves;
	bb = MC_P(MC_S, PC_N) & all_pins;
	while(bb) {
		from = tzc(bb);
		bb = blsr(bb);
		nodes -= popc(KNIGHT_ATTACKS[from] & ~MC_O(MC_S));
	}
#else
	while(bb) {
		from = tzc(bb);
		bb = blsr(bb);
		attacks = KNIGHT_ATTACKS[from] & ~MC_O(MC_S);
		if(MC_LEAF) {
			nodes += popc(attacks);

		} else {
			MOVES(PC_N, from, attacks, RN);
		}
	}
#endif
	bb = MC_P(MC_S, PC_B) & ~all_pins;
	while(bb) {
		from = tzc(bb);
		bb = blsr(bb);
		attacks = get_bishop(from, MC_OB) & ~MC_O(MC_S);
		if(MC_LEAF) {
			nodes += popc(attacks);

		} else {
			MOVES(PC_B, from, attacks, RN);
		}
	}
#if !MC_DELTA_VIEW || defined(__clang__)
	bb = (MC_P(MC_S, PC_B) | MC_P(MC_S, PC_Q)) & b_pins;
	while(bb) {
		from = tzc(bb);
		bb = blsr(bb);
		attacks = get_bishop(from, MC_OB) & ~MC_O(MC_S) & b_pins;
		if(MC_LEAF) {
			nodes += popc(attacks);

		} else if((1ull << from) & MC_P(MC_S, PC_Q)) {
			MOVES(PC_Q, from, attacks, RN);

		} else {
			MOVES(PC_B, from, attacks, RN);
		}
	}
#endif
	bb = MC_P(MC_S, PC_R) & ~all_pins;
	while(bb) {
		from = tzc(bb);
		bb = blsr(bb);
		attacks = get_rook(from, MC_OB) & ~MC_O(MC_S);
		if(MC_LEAF) {
			nodes += popc(attacks);

		} else {
			MOVES(PC_R, from, attacks, RN);
		}
	}
#if MC_DELTA_VIEW && !defined(__clang__)
	if(all_pins) {
		bb = (MC_P(MC_S, PC_B) | MC_P(MC_S, PC_Q)) & b_pins;
		while(bb) {
			from = tzc(bb);
			bb = blsr(bb);
			attacks = get_bishop(from, MC_OB) & ~MC_O(MC_S) & b_pins;
			if(MC_LEAF) {
				nodes += popc(attacks);

			} else if((1ull << from) & MC_P(MC_S, PC_Q)) {
				MOVES(PC_Q, from, attacks, RN);

			} else {
				MOVES(PC_B, from, attacks, RN);
			}
		}
		bb = (MC_P(MC_S, PC_R) | MC_P(MC_S, PC_Q)) & r_pins;
		while(bb) {
			from = tzc(bb);
			bb = blsr(bb);
			attacks = get_rook(from, MC_OB) & ~MC_O(MC_S) & r_pins;
			if(MC_LEAF) {
				nodes += popc(attacks);

			} else if((1ull << from) & MC_P(MC_S, PC_Q)) {
				MOVES(PC_Q, from, attacks, RN);

			} else {
				MOVES(PC_R, from, attacks, RN);
			}
		}
	}
#else
	bb = (MC_P(MC_S, PC_R) | MC_P(MC_S, PC_Q)) & r_pins;
	while(bb) {
		from = tzc(bb);
		bb = blsr(bb);
		attacks = get_rook(from, MC_OB) & ~MC_O(MC_S) & r_pins;
		if(MC_LEAF) {
			nodes += popc(attacks);

		} else if((1ull << from) & MC_P(MC_S, PC_Q)) {
			MOVES(PC_Q, from, attacks, RN);

		} else {
			MOVES(PC_R, from, attacks, RN);
		}
	}
#endif
	bb = MC_P(MC_S, PC_Q) & ~all_pins;
	while(bb) {
		from = tzc(bb);
		bb = blsr(bb);
		attacks = get_queen(from, MC_OB) & ~MC_O(MC_S);
		if(MC_LEAF) {
			nodes += popc(attacks);

		} else {
			MOVES(PC_Q, from, attacks, RN);
		}
	}
	if(!MC_KM) {
		if(MC_CAN_CASTLE(castle_attacks, (uint32_t)CASTLING_SIDE_K[MC_S])) {
			if(MC_LEAF) {
				nodes += 1;

			} else {
				make_castle(b, (uint32_t)CASTLING_SIDE_K[MC_S], &nb, MC_S);
				nodes += RK(&nb);
			}
		}
		if(MC_CAN_CASTLE(castle_attacks, (uint32_t)CASTLING_SIDE_Q[MC_S])) {
			if(MC_LEAF) {
				nodes += 1;

			} else {
				make_castle(b, (uint32_t)CASTLING_SIDE_Q[MC_S], &nb, MC_S);
				nodes += RK(&nb);
			}
		}
	}
	return nodes;
