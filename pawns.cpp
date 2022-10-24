#include "pawns.h"

#include "bitboard.h"
#include "material.h"
#include "psqtables.h"
#include "threads.h"

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

static constexpr uint64_t adjacent_files[8] =
{
	0x202020202020202L, 0x505050505050505L, 0xa0a0a0a0a0a0a0aL, 0x1414141414141414L,
	0x2828282828282828L, 0x5050505050505050L, 0xa0a0a0a0a0a0a0a0L, 0x4040404040404040L
};

uint64_t PassedPawnMask[Color][sq_all];
uint64_t PawnAttackSpan[Color][sq_all];

inline uint64_t passed_pawn_mask(const int color, const int sq)
{
	return PassedPawnMask[color][sq];
}

inline uint64_t pawn_attack_span(const int color, const int sq)
{
	return PawnAttackSpan[color][sq];
}

const Score doubled_pawns[8] =
{
	S(5, 20), S(8, 22), S(9, 22), S(9, 22),
	S(9, 22), S(9, 22), S(8, 22), S(5, 20)
};

const Score isolated_pawns[2][8]
{
	{
		S(18, 22), S(25, 23), S(28, 24), S(28, 24),
		S(28, 24), S(28, 24), S(25, 23), S(18, 22)
	},
	{
		S(11, 14), S(18, 17), S(20, 18), S(20, 18),
		S(20, 18), S(20, 18), S(18, 17), S(11, 14)
	}
};

const Score backward_pawns[2][8] =
{
	{
		S(14, 20), S(21, 22), S(23, 22), S(23, 22),
		S(23, 22), S(23, 22), S(21, 22), S(14, 20)
	},
	{
		S(9, 13), S(13, 15), S(16, 15), S(16, 15),
		S(16, 15), S(16, 15), S(13, 15), S(9, 13)
	}
};


const Score connected_pawns[8][8] =
{
	{S(0, 0), S(1, 1), S(1, 1), S(1, 1), S(1, 1), S(1, 1), S(1, 1), S(0, 0)},
	{S(0, 0), S(1, 1), S(1, 1), S(1, 1), S(1, 1), S(1, 1), S(1, 1), S(0, 0)},
	{S(0, 0), S(2, 2), S(2, 2), S(3, 3), S(3, 3), S(2, 2), S(2, 2), S(0, 0)},
	{S(3, 3), S(5, 5), S(5, 5), S(6, 6), S(6, 6), S(5, 5), S(5, 5), S(3, 3)},
	{S(11, 11), S(14, 14), S(14, 14), S(15, 15), S(15, 15), S(14, 14), S(14, 14), S(11, 11)},
	{S(27, 27), S(30, 30), S(30, 30), S(31, 31), S(31, 31), S(30, 30), S(30, 30), S(27, 27)},
	{S(53, 53), S(57, 57), S(57, 57), S(59, 59), S(59, 59), S(57, 57), S(57, 57), S(53, 53)}
};

const Score candidate_passed_pawns[8] =
{
	S(0, 0), S(2, 6), S(2, 6), S(6, 14),
	S(16, 33), S(39, 77), S(0, 0), S(0, 0)
};

const Score lever_pawns[8] =
{
	S(0, 0), S(0, 0), S(0, 0), S(0, 0),
	S(10, 10), S(20, 20), S(0, 0), S(0, 0)
};

const Score pawns_file_span = S(0, 6);
const Score unsupported_pawn_penalty = S(10, 5);

namespace pawns
{
	template <int Color>
	Score eval_pawns(const bit_board& board, pawn_entry* e)
	{
		Score val;
		const int them = Color == white ? black : white;
		const int us = Color == black ? white : black;
		const int up = Color == white ? north : south;
		const int right = Color == white ? northeast : southwest;
		const int left = Color == white ? northwest : southeast;
		const auto* list = board.piece_loc[Color][pawn];

		const auto our_pawns = board.pieces(Color, pawn);
		auto enemy_pawns = board.pieces(them, pawn);
		uint64_t bb;

		const auto* pawn_attacks_bb = board.pseudo_attacks[pawn];
		bool backward;

		e->piece_sq_tab_scores[Color] = make_score(0, 0);
		e->passed_pawns[Color] = e->candidate_pawns[Color] = 0LL;
		e->king_squares[Color] = sq_none;
		e->semi_open_files[Color] = 0xFF;
		e->pawn_attacks[Color] = shift_bb<right>(our_pawns) | shift_bb<left>(our_pawns);
		e->pawns_on_squares[Color][black] = popcnt(our_pawns & dark_squares);
		e->pawns_on_squares[Color][white] = board.piece_count[Color][pawn] - e->pawns_on_squares[Color][black];

		int square;

		while ((square = *list++) != sq_none)
		{
			const auto f = file_of(square);
			const auto psq = Color == white ? square : b_sq[square];
			e->piece_sq_tab_scores[Color] += make_score(piece_sq_table[pawn][mg][psq], piece_sq_table[pawn][eg][psq]);
			e->pawnAttacksSpan[us] |= pawn_attack_span(us, square);
			e->semi_open_files[Color] &= ~(1LL << f);
			const auto pr = rank_masks8[rank_of(square + pawn_push(them))];
			bb = pr | rank_masks8[rank_of(square)];
			const bool connected = our_pawns & adjacent_files[f] & bb;
			const auto unsupported = !(our_pawns & adjacent_files[f] & pr);
			const auto isolated = !(our_pawns & adjacent_files[f]);
			const auto doubled = our_pawns & forward_bb[Color][square];
			const bool opposed = enemy_pawns & forward_bb[Color][square];
			const auto passed = !(enemy_pawns & passed_pawn_mask(Color, square));
			const bool lever = enemy_pawns & pawn_attacks_bb[square];

			if (passed | isolated | connected || our_pawns & pawn_attack_span(them, square)
				|| board.psuedo_attacks(pawn, Color, square) & enemy_pawns)
			{
				backward = false;
			}
			else
			{
				bb = pawn_attack_span(Color, square) & (our_pawns | enemy_pawns);
				bb = pawn_attack_span(Color, square) & rank_masks8[rank_of(backmost_sq(Color, bb))];
				backward = (bb | shift_bb<up>(bb)) & enemy_pawns;
			}

			if (!(opposed | passed | (pawn_attack_span(Color, square) & enemy_pawns)))
				continue;

			const auto candidate = !(opposed | passed | backward | isolated)
				&& (bb = pawn_attack_span(them, square + pawn_push(Color)) & our_pawns) != 0
				&& popcnt(bb) >= popcnt(pawn_attack_span(Color, square) & enemy_pawns);

			if (passed && !doubled)
				e->passed_pawns[Color] |= board.square_bb(square);
			if (isolated)
				val -= isolated_pawns[opposed][f];
			if (unsupported && !isolated)
				val -= unsupported_pawn_penalty;
			if (doubled)
				val -= doubled_pawns[f] / rank_distance(square, lsb(doubled));
			if (backward)
				val -= backward_pawns[opposed][f];
			if (connected)
				val += connected_pawns[f][relative_rank_sq(Color, square)];
			if (lever)
				val += lever_pawns[relative_rank_sq(Color, square)];
			if (candidate)
			{
				val += candidate_passed_pawns[relative_rank_sq(Color, square)];
				if (!doubled)
					e->candidate_pawns[Color] |= square;
			}
		}

		bb = e->semi_open_files[Color] ^ 0xFF;
		e->pawn_span[Color] = bb ? msb(bb) - lsb(bb) : 0;
		val += pawns_file_span * e->pawn_span[Color];
		return val;
	}

	pawn_entry* probe(const bit_board& board)
	{
		const auto key = board.pawn_key();
		auto* entry = board.this_thread()->pawn_table[key];
		if (entry->key == key)
		{
			return entry;
		}
		entry->key = key;
		entry->score = eval_pawns<white>(board, entry) - eval_pawns<black>(board, entry);
		entry->openFiles = popcnt(entry->semi_open_files[white] & entry->semi_open_files[black]);
		return entry;
	}
}
