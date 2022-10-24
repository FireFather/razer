#include <iostream>
#include "common.h"
#include "bitboard.h"
#include "material.h"
#include "psqtables.h"
#include "pawns.h"
#include "evaluate.h"

const Score rook_on_pawn = S(3, 10);
const Score rook_half_open = S(8, 4);
const Score rook_open = S(17, 8);
const Score unstoppable = S(0, 6);
const Score minor_behind_pawn = S(7, 0);
const Score bishop_pawns = S(3, 5);
const Score threat_by_hanging_pawn = S(30, 26);
const Score hanging = S(21, 12);
const Score threat_by_pawn_push = S(16, 9);

constexpr int queen_contact_check = 7;
constexpr int rook_contact_check = 5;
constexpr int queen_check = 3;
constexpr int rook_check = 2;
constexpr int bishop_check = 1;
constexpr int knight_check = 1;

constexpr int queen_att_factor = 15;

constexpr int passed_pawn_mg = 17;
constexpr int passed_pawn_eg = 7;
constexpr int pp_block_sq_dist_them = 5;
constexpr int pp_block_sq_dist = 2;
constexpr int pp_safe_sq = 15;
constexpr int pp_safe_sq_block_sq = 9;

constexpr int pp_def_sq_to_queen = 6;
constexpr int pp_def_sq_block_q = 4;
constexpr int pp_blocked_s_qrr = 3;
constexpr int pp_blocked_s_qr = 2;
constexpr int pp_blocked_sq = 3;

constexpr int king_shield_rank2 = 10;
constexpr int king_shield_rank3 = 5;

constexpr int strong_material = 400;

int square_distance[64][64];

inline int squareDistance(const int s1, const int s2)
{
	return square_distance[s1][s2];
}

uint64_t king_side = file_masks8[5] | file_masks8[6] | file_masks8[7];
uint64_t queen_side = file_masks8[2] | file_masks8[1] | file_masks8[0];

static constexpr int b_sq[64] =
{
	56, 57, 58, 59, 60, 61, 62, 63,
	48, 49, 50, 51, 52, 53, 54, 55,
	40, 41, 42, 43, 44, 45, 46, 47,
	32, 33, 34, 35, 36, 37, 38, 39,
	24, 25, 26, 27, 28, 29, 30, 31,
	16, 17, 18, 19, 20, 21, 22, 23,
	8, 9, 10, 11, 12, 13, 14, 15,
	0, 1, 2, 3, 4, 5, 6, 7
};

const Score outpost [][2] =
{
	{S(20, 5), S(30, 8)},
	{S(8, 2), S(9, 3)}
};

const Score reachable_outpost [][2] =
{
	{S(9, 3), S(16, 3)},
	{S(3, 0), S(6, 2)}
};

const Score pawn_threat[piece]
{
	S(0, 0), S(0, 0), S(24, 32), S(25, 32), S(35, 57), S(40, 65)
};

const Score threat_by_safe_pawn[6] =
{
	S(0, 0), S(0, 0), S(119, 60), S(56, 55), S(93, 94), S(87, 92)
};

const Score threat[][6] =
{
	{S(0, 0), S(0, 14), S(19, 18), S(20, 20), S(31, 46), S(21, 51)}, // by Minor
	{S(0, 0), S(0, 11), S(17, 27), S(17, 25), S(0, 15), S(15, 21)} // by Rook
};

const Score threat_by_king[2] =
{
	S(1, 27), S(4, 59)
};

static constexpr int safety_table[100] =
{
	0, 0, 1, 2, 3, 5, 7, 9, 12, 15,
	18, 22, 26, 30, 35, 39, 44, 50, 56, 62,
	68, 75, 82, 85, 89, 97, 105, 113, 122, 131,
	140, 150, 169, 180, 191, 202, 213, 225, 237, 248,
	260, 272, 283, 295, 307, 319, 330, 342, 354, 366,
	377, 389, 401, 412, 424, 436, 448, 459, 471, 483,
	494, 500, 500, 500, 500, 500, 500, 500, 500, 500,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500
};

const Score mobility_bonus[piece][30]
{
	{}, {},
	// Knights
	{
		S(-75, -76), S(-56, -54), S(-9, -26), S(-2, -10), S(6, 5), S(15, 11), S(22, 26), S(30, 28), S(36, 29)
	},
	// Bishops
	{
		S(-48, -58), S(-21, -19), S(16, -2), S(26, 12), S(37, 22), S(51, 42), S(54, 54), S(63, 58), S(65, 63),
		S(71, 70),
		S(79, 74), S(81, 86), S(92, 90), S(97, 94)
	},
	// Rooks
	{
		S(-56, -78), S(-25, -18), S(-11, 26), S(-5, 55), S(-4, 70), S(-1, 81), S(8, 109), S(14, 120), S(21, 128),
		S(23, 143),
		S(31, 154), S(32, 160), S(43, 165), S(49, 168), S(59, 169)
	},
	// Queens
	{
		S(-40, -35), S(-25, -12), S(2, 7), S(4, 19), S(14, 37), S(24, 55), S(25, 62), S(40, 76), S(43, 79), S(47, 87),
		S(54, 94), S(56, 102), S(60, 111), S(70, 116), S(72, 118), S(73, 122), S(75, 128), S(77, 130), S(85, 133),
		S(94, 136),
		S(99, 140), S(108, 157), S(112, 158), S(113, 161), S(118, 174), S(119, 177), S(123, 191), S(128, 199)
	}
};

enum
{
	pawn_structure,
	passed_pawns,
	imbalance
};

constexpr struct weight
{
	int mg, eg;
}

weights[] =
{
	{80, 73},
	{25, 34},
	{16, 16}
};

class eval_info
{
public:
	Score psq_scores[Color];
	int king_attackers[Color] = {0};
	int king_att_weights[Color] = {0};
	int adjacent_king_attckers[Color] = {0};
	uint64_t attacked_by[Color][7] =
	{
		{0LL}
	};
	uint64_t pinned_pieces[Color] = {0LL};
	Score mobility[Color];
	int adjust_material[Color] = {0};
	pawns::pawn_entry* pe{};
	material::entry* me{};
};

template <int PType, int Color>
Score evaluate_pieces(const bit_board& board, eval_info& ev, uint64_t* mobility_area)
{
	Score score;
	const auto* piece = board.piece_loc[Color][PType];
	const auto them = Color == white ? black : white;
	const auto us = Color;
	const auto next_piece = Color == white ? PType : PType + 1;
	int square;
	const auto outpost_ranks = us == white ? rank4 | rank5 | rank6 : rank5 | rank4 | rank3;

	while ((square = *piece++) != sq_none)
	{
		const auto adj_sq = Color == white ? square : b_sq[square];
		ev.psq_scores[Color] += make_score(piece_sq_table[PType][mg][adj_sq], piece_sq_table[PType][eg][adj_sq]);

		auto b = PType == bishop
			         ? slider_attacks.bishopAttacks(board.full_squares ^ board.pieces(Color, queen), square)
			         : PType == rook
			         ? slider_attacks.rookAttacks(board.full_squares ^ board.pieces(Color, queen, rook), square)
			         : PType == queen
			         ? slider_attacks.queenAttacks(board.full_squares, square)
			         : board.pseudo_attacks[knight][square];

		if (ev.pinned_pieces[Color] & board.square_bb(square))
			b &= line_bb[board.king_square(Color)][square];

		ev.attacked_by[Color][0] |= ev.attacked_by[Color][PType] |= b;

		if (PType == queen)
			b &= ~(ev.attacked_by[them][knight] | ev.attacked_by[them][bishop] | ev.attacked_by[them][rook]);

		const auto mobility = popcnt(b & mobility_area[Color]);
		ev.mobility[Color] += mobility_bonus[PType][mobility] / 2;

		if (ev.attacked_by[them][pawn] & square)
			score -= pawn_threat[PType];

		if (PType == knight || PType == bishop)
		{
			if (PType == bishop)
				score -= bishop_pawns * ev.pe->pawns_on_same_color_square(Color, square);

			if (relative_rank_sq(Color, square) < 5 && board.pieces_by_type(pawn) & 1LL << (square + pawn_push(Color)))
				score += minor_behind_pawn;

			if (auto bb = outpost_ranks & ~ev.pe->pawn_attacks_span(them); bb & square)
				score += outpost[PType == bishop - 2][!!(ev.attacked_by[us][pawn] & square)];
			else
			{
				bb &= b & ~board.pieces(us);
				if (bb)
					score += reachable_outpost[PType == bishop - 2][!!(ev.attacked_by[us][pawn] & bb)];
			}
		}

		if (PType == rook)
		{
			if (ev.pe->semi_open_file(Color, file_of(square)))
				score += ev.pe->semi_open_file(them, file_of(square)) ? rook_open : rook_half_open;
			if (relative_rank_sq(Color, square) >= 5)
			{
				if (const auto pawns = board.pieces(them, pawn) & board.psuedo_attacks(rook, Color, square))
					score += rook_on_pawn * popcnt(pawns);
			}
		}
	}
	return score - evaluate_pieces<next_piece, them>(board, ev, mobility_area);
}

template <>
Score evaluate_pieces<king, white>(const bit_board&, eval_info&, uint64_t*)
{
	return S(0, 0);
}

template <int Color>
int evaluate_king(const bit_board& board, eval_info& ev, const int mg_score)
{
	const int them = Color == white ? black : white;
	auto score = 0;
	const auto square = board.king_square(Color);

	if (ev.king_attackers[them])
	{
		const auto undefended = ev.attacked_by[them][0] & ev.attacked_by[Color][king] & ~(ev.attacked_by[Color][pawn]
			| ev.attacked_by[Color][knight] | ev.attacked_by[Color][bishop] | ev.attacked_by[Color][rook]
			| ev.attacked_by[Color][queen]);
		auto attack_units = std::min(17, ev.king_attackers[them] * ev.king_att_weights[them] / 2)
			+ 2 * (ev.adjacent_king_attckers[them] + popcnt(undefended)) - mg_score / 32 - !board.piece_count[them][queen] *
			queen_att_factor;

		auto bb = undefended & ev.attacked_by[them][queen] & ~board.pieces(them);

		if (bb)
		{
			bb &= ev.attacked_by[them][pawn] | ev.attacked_by[them][knight] | ev.attacked_by[them][bishop]
				| ev.attacked_by[them][rook];
			if (bb)
				attack_units += queen_contact_check * popcnt(bb);
		}

		bb = undefended & ev.attacked_by[them][rook] & ~board.pieces(them);
		bb &= board.psuedo_attacks(rook, Color, square);

		if (bb)
		{
			bb &= ev.attacked_by[them][pawn] | ev.attacked_by[them][knight] | ev.attacked_by[them][bishop]
				| ev.attacked_by[them][rook];
			if (bb)
				attack_units += rook_contact_check * popcnt(bb);
		}

		const auto safe = ~(board.pieces(them) | ev.attacked_by[Color][0]);
		const auto b1 = slider_attacks.rookAttacks(board.full_squares, square) & safe;
		const auto b2 = slider_attacks.bishopAttacks(board.full_squares, square) & safe;

		bb = (b1 | b2) & ev.attacked_by[them][queen];
		if (bb)
			attack_units += queen_check * popcnt(bb);

		bb = b1 & ev.attacked_by[them][rook];
		if (bb)
			attack_units += rook_check * popcnt(bb);

		bb = b2 & ev.attacked_by[them][bishop];
		if (bb)
			attack_units += bishop_check * popcnt(bb);

		bb = board.psuedo_attacks(knight, them, square) & ev.attacked_by[them][knight] & safe;
		if (bb)
			attack_units += knight_check * popcnt(bb);

		attack_units = std::min(99, attack_units);
		score -= safety_table[attack_units];
	}
	return score;
}

template <color Us>
Score evaluate_threats(const bit_board& pos, const eval_info& ei)
{
	const auto them = Us == white ? black : white;
	const auto up = Us == white ? delta_n : delta_s;
	const auto left = Us == white ? delta_nw : delta_se;
	const auto right = Us == white ? delta_ne : delta_sw;
	const auto t_rank2 = Us == white ? rank2 : rank7;
	const auto t_rank7 = Us == white ? rank7 : rank2;

	enum
	{
		minor,
		rook
	};

	uint64_t b, safe_threats;
	auto score = SCORE_ZERO;

	// Non-pawn enemies attacked by a pawn
	auto weak = (pos.pieces(them) ^ pos.pieces(them, pawn)) & ei.attacked_by[Us][pawn];

	if (weak)
	{
		b = pos.pieces(Us, pawn) & (~ei.attacked_by[them][0] | ei.attacked_by[Us][0]);

		safe_threats = (shift_bb<right>(b) | shift_bb<left>(b)) & weak;

		if (weak ^ safe_threats)
			score += threat_by_hanging_pawn / 2;

		while (safe_threats)
			score += threat_by_safe_pawn[static_cast<Piece>(pos.piece_on_sq(pop_lsb(&safe_threats)))] / 2;
	}

	// Non-pawn enemies defended by a pawn
	const auto defended = (pos.pieces(them) ^ pos.pieces(them, pawn)) & ei.attacked_by[them][pawn];

	// Enemies not defended by a pawn and under our attack
	weak = pos.pieces(them) & ~ei.attacked_by[them][pawn] & ei.attacked_by[Us][0];

	// Add a bonus according to the kind of attacking pieces
	if (defended | weak)
	{
		b = (defended | weak) & (ei.attacked_by[Us][knight] | ei.attacked_by[Us][bishop]);

		while (b)
			score += threat[minor][static_cast<Piece>(pos.piece_on_sq(pop_lsb(&b)))] / 2;

		b = (pos.pieces(them, queen) | weak) & ei.attacked_by[Us][rook];

		while (b)
			score += threat[rook][static_cast<Piece>(pos.piece_on_sq(pop_lsb(&b)))] / 2;

		b = weak & ~ei.attacked_by[them][0];

		if (b)
			score += hanging * popcnt(b) / 2;

		b = weak & ei.attacked_by[Us][king];

		if (b)
			score += threat_by_king[more_than_one(b)] / 2;
	}

	// Bonus if some pawns can safely push and attack an enemy piece
	b = pos.pieces(Us, pawn) & ~t_rank7;
	b = shift_bb<up>(b | (shift_bb<up>(b & t_rank2) & ~pos.pieces(Us, pawn)));

	b &= ~pos.pieces(Us, pawn) & ~ei.attacked_by[them][pawn] & (ei.attacked_by[Us][0]
		| ~ei.attacked_by[them][0]);

	b = (shift_bb<left>(b) | shift_bb<right>(b)) & pos.pieces(them) & ~ei.attacked_by[Us][pawn];

	if (b)
		score += threat_by_pawn_push * popcnt(b) / 2;

	return score;
}

template <color Us>
Score evaluate_space(const bit_board& board, const eval_info& ei)
{
	const auto them = Us == white ? black : white;
	const auto space_mask = Us == white
		                       ? (file_c | file_d | file_e | file_f) & (rank2 | rank3 | rank4)
		                       : (file_c | file_d | file_e | file_f) & (rank7 | rank6 | rank5);
	const auto safe = space_mask & ~board.pieces(Us, pawn) & ~ei.attacked_by[them][pawn] & (ei.attacked_by[Us][0] | ~ei
		.attacked_by[them][0]);
	auto behind = board.pieces(Us, pawn);
	behind |= Us == white ? behind >> 8 : behind << 8;
	behind |= Us == white ? behind >> 16 : behind << 16;
	auto bonus = popcnt((Us == white ? safe << 32 : safe >> 32) | (behind & safe));
	bonus = std::min(16, bonus);
	const auto weight = board.count<0>(Us) - 2 * ei.pe->open_files();
	return make_score(bonus * weight * weight / 10, 0);
}

Score apply_weights(Score s, const weight& w)
{
	s.mg = s.mg * w.mg / 100;
	s.eg = s.eg * w.eg / 100;
	return s;
}

template <int Color>
Score evaluate_passed_pawns(const bit_board& board, const eval_info& ev)
{
	const int them = Color == white ? black : white;
	uint64_t b, squares_to_queen, unsafe_squares;
	Score score;
	b = ev.pe->passed_pawns[Color];

	while (b)
	{
		const auto sq = pop_lsb(&b);
		const auto r = relative_rank_sq(Color, sq) - 1;
		const auto rr = r * (r - 1);
		auto mg_bonus = passed_pawn_mg * rr;
		auto eg_bonus = passed_pawn_eg * (rr + r + 1);

		if (rr)
		{
			const auto block_sq = sq + pawn_push(Color);
			eg_bonus += squareDistance(board.king_square(them), block_sq) * pp_block_sq_dist_them * rr
				- squareDistance(board.king_square(Color), block_sq) * pp_block_sq_dist * rr;

			if (relative_rank_sq(Color, block_sq) != 7)
			{
				eg_bonus -= squareDistance(board.king_square(Color), block_sq + pawn_push(Color)) * rr;
			}

			if (board.empty(block_sq))
			{
				auto defended_squares = unsafe_squares = squares_to_queen = forward_bb[Color][sq];
				const auto bb = forward_bb[them][sq] & board.pieces_by_type(rook, queen) & slider_attacks.rookAttacks(
					board.full_squares, sq);

				if (!(board.pieces(Color) & bb))
					defended_squares &= ev.attacked_by[Color][0];

				if (!(board.pieces(them) & bb))
					unsafe_squares &= ev.attacked_by[them][0] | board.pieces(them);

				auto k = !unsafe_squares ? pp_safe_sq : !(unsafe_squares & 1LL << block_sq) ? pp_safe_sq_block_sq : 0;

				if (defended_squares == squares_to_queen)
					k += pp_def_sq_to_queen;
				else if (defended_squares & block_sq)
					k += pp_def_sq_block_q;

				mg_bonus += k * rr, eg_bonus += k * rr;
			}
			else if (board.pieces(Color) & 1LL << block_sq)
			{
				mg_bonus += rr * pp_blocked_s_qrr + r * pp_blocked_s_qr + pp_blocked_sq;
				eg_bonus += rr + r * pp_blocked_s_qr;
			}
		}

		if (board.count<pawn>(Color) < board.count<pawn>(them))
			eg_bonus += eg_bonus / 4;
		score += make_score(mg_bonus, eg_bonus);
	}
	return apply_weights(score, weights[passed_pawns]);
}

template <int Color>
Score unstoppable_pawns(const eval_info& ev)
{
	const auto bb = ev.pe->passed_pawns[Color] | ev.pe->candidate_pawns[Color];
	return bb ? unstoppable * relative_rank(Color, frontmost_sq(Color, bb)) : SCORE_ZERO;
}

int Evaluate::evaluate(const bit_board& board) const
{
	const auto color = board.stm();
	eval_info ev;
	Score score;

	ev.pinned_pieces[white] = board.pinned_pieces(white);
	ev.pinned_pieces[black] = board.pinned_pieces(black);

	ev.me = material::probe(board);
	score += apply_weights(ev.me->material_value(), weights[imbalance]);

	ev.pe = pawns::probe(board);
	score += apply_weights(ev.pe->score, weights[pawn_structure]);

	ev.attacked_by[white][0] |= ev.attacked_by[white][pawn] = ev.pe->pawn_attacks[white];
	ev.attacked_by[black][0] |= ev.attacked_by[black][pawn] = ev.pe->pawn_attacks[black];

	ev.psq_scores[white] += ev.pe->piece_sq_tab_scores[white];
	ev.psq_scores[black] += ev.pe->piece_sq_tab_scores[black];

	uint64_t mobility_area[Color];
	mobility_area[white] = ~(ev.attacked_by[black][pawn] | board.pieces(white, pawn, king));
	mobility_area[black] = ~(ev.attacked_by[white][pawn] | board.pieces(black, pawn, king));

	score += evaluate_pieces<knight, white>(board, ev, mobility_area);

	score += ev.psq_scores[white] + board.b_info.side_material[white]
		- (ev.psq_scores[black] + board.b_info.side_material[black]);

	score += evaluate_passed_pawns<white>(board, ev) - evaluate_passed_pawns<black>(board, ev);

	score.mg += w_king_shield(board) - b_king_shield(board);

	Score ksf;
	ksf.mg = evaluate_king<white>(board, ev, score.mg) - evaluate_king<black>(board, ev, score.mg);

	score += ksf;

	score += evaluate_threats<white>(board, ev) - evaluate_threats<black>(board, ev);

	score += ev.mobility[white] - ev.mobility[black];

	if (!board.non_pawn_material(white) && !board.non_pawn_material(black))
	{
		score += unstoppable_pawns<white>(ev) - unstoppable_pawns<black>(ev);
	}

	if (board.non_pawn_material(white) + board.non_pawn_material(black) >= 5000)
		score += evaluate_space<white>(board, ev) - evaluate_space<black>(board, ev);

	auto result = score.mg * ev.me->game_phase + score.eg * (64 - ev.me->game_phase) * sf_normal / sf_normal;
	result /= 64;
	result += ev.adjust_material[white] - ev.adjust_material[black];

	const int strong = result > 0 ? white : black;
	const int weak = !strong;
	if (board.piece_count[strong][pawn] == 0)
	{
		if (board.b_info.side_material[strong] < strong_material)
			return 0;
		if (board.piece_count[weak][pawn] == 0 && board.b_info.side_material[strong] == 2 * piece_value[knight])
			return 0;
		if (board.b_info.side_material[strong] == piece_value[rook]
			&& board.b_info.side_material[weak] == piece_value[bishop])
			result /= 2;
		if (board.b_info.side_material[strong] == piece_value[rook]
			+ piece_value[bishop] && board.b_info.side_material[weak] == piece_value[rook])
			result /= 2;
		if (board.b_info.side_material[strong] == piece_value[rook]
			+ piece_value[knight] && board.b_info.side_material[weak] == piece_value[rook])
			result /= 2;
	}
	result = color == white ? result : -result;
	return result;
}

int Evaluate::w_king_shield(const bit_board& board)
{
	auto result = 0;
	const auto pawns = board.by_color_pieces_bb[white][pawn];
	const auto King = board.by_color_pieces_bb[white][king];
	constexpr uint64_t location = 1LL;

	if (king_side & King)
	{
		if (pawns & location << f2)
			result += king_shield_rank2;
		else if (pawns & location << f3)
			result += king_shield_rank3;
		if (pawns & location << g2)
			result += king_shield_rank2;
		else if (pawns & location << g3)
			result += king_shield_rank3;
		if (pawns & location << h2)
			result += king_shield_rank2;
		else if (pawns & location << h3)
			result += king_shield_rank3;
	}
	else if (queen_side & King)
	{
		if (pawns & location << a2)
			result += king_shield_rank2;
		else if (pawns & location << a3)
			result += king_shield_rank3;
		if (pawns & location << b2)
			result += king_shield_rank2;
		else if (pawns & location << b3)
			result += king_shield_rank3;
		if (pawns & location << c2)
			result += king_shield_rank2;
		else if (pawns & location << c3)
			result += king_shield_rank3;
	}
	return result;
}

int Evaluate::b_king_shield(const bit_board& board)
{
	auto result = 0;
	const auto pawns = board.by_color_pieces_bb[black][pawn];
	const auto King = board.by_color_pieces_bb[black][king];
	constexpr uint64_t location = 1LL;

	if (queen_side & King)
	{
		if (pawns & location << f7)
			result += king_shield_rank2;
		else if (pawns & location << f6)
			result += king_shield_rank3;
		if (pawns & location << g7)
			result += king_shield_rank2;
		else if (pawns & location << g6)
			result += king_shield_rank3;
		if (pawns & location << h7)
			result += king_shield_rank2;
		else if (pawns & location << h6)
			result += king_shield_rank3;
	}
	else if (king_side & King)
	{
		if (pawns & location << a7)
			result += king_shield_rank2;
		else if (pawns & location << a6)
			result += king_shield_rank3;
		if (pawns & location << b7)
			result += king_shield_rank2;
		else if (pawns & location << b6)
			result += king_shield_rank3;
		if (pawns & location << c7)
			result += king_shield_rank2;
		else if (pawns & location << c6)
			result += king_shield_rank3;
	}
	return result;
}

bool Evaluate::is_piece(const uint64_t& piece, const bit_board& board, const int sq)
{
	return piece & board.square_bb(sq);
}
