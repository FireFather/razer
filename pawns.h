#pragma once
#include "common.h"
#include "hash.h"
#include "evaluate.h"

class bit_board;

namespace pawns
{
	constexpr uint64_t dark_squares = 0x55aa55aa55aa55aaULL;
	constexpr uint64_t light_squares = 0xAA55AA55AA55AA55ULL;

	struct pawn_entry
	{
		[[nodiscard]] int pawns_on_same_color_square(const int color, const int sq) const
		{
			return pawns_on_squares[color][!!(dark_squares & 1LL << sq)];
		}

		[[nodiscard]] int semi_open_file(const int color, const int file) const
		{
			return static_cast<int>(semi_open_files[color] & 1LL << file);
		}

		[[nodiscard]] int open_files() const
		{
			return openFiles;
		}

		[[nodiscard]] uint64_t pawn_attacks_span(const color c) const
		{
			return pawnAttacksSpan[c];
		}

		uint64_t key{};

		Score score;
		Score piece_sq_tab_scores[Color];
		uint64_t passed_pawns[Color]{};
		uint64_t candidate_pawns[Color]{};
		uint64_t pawn_attacks[Color]{};
		uint64_t pawnAttacksSpan[Color]{};

		int king_squares[Color] = {0};
		int king_safety[Color]{};
		int openFiles{};
		int semi_open_files[Color]{};
		int pawn_span[Color]{};
		int pawns_on_squares[Color][Color]{};
	};

	typedef hash_table<pawn_entry, 16384> pawn_table;
	pawn_entry* probe(const bit_board& board);
}
