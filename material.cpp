#include "material.h"

#include "bitboard.h"
#include "common.h"
#include "threads.h"

constexpr int linear[6] =
{
	1852, -162, -1122, -183, 249, -154
};

constexpr int quadratic_same_side [][piece] =
{
	{0},
	{39, 2},
	{35, 271, -4},
	{0, 105, 4, 0},
	{-27, -2, 46, 100, -141},
	{-177, 25, 129, 142, -137, 0}
};

constexpr int quadratic_opposite_side [][piece] =
{
	{0},
	{37, 0},
	{10, 62, 0},
	{57, 64, 39, 0},
	{50, 40, 23, -22, 0},
	{98, 105, -39, 141, 274, 0}
};

template <int Color>
int material_imbalance(const int piece_count [][piece])
{
	const int them = Color == white ? black : white;
	auto bonus = 0;

	for (int pt1 = no_piece; pt1 <= queen; ++pt1)
	{
		if (!piece_count[Color][pt1])
			continue;

		auto value = linear[pt1];

		for (int pt2 = no_piece; pt2 <= pt1; ++pt2)
		{
			value += quadratic_same_side[pt1][pt2] * piece_count[Color][pt2]
				+ quadratic_opposite_side[pt1][pt2] * piece_count[them][pt2];
		}

		bonus += piece_count[Color][pt1] * value;
	}
	return bonus;
}

namespace material
{
	entry* probe(const bit_board& board)
	{
		const auto key = board.material_key();

		auto* entry = board.this_thread()->material_table[key];

		if (entry->key == key)
			return entry;

		std::memset(entry, 0, sizeof(entry));
		entry->key = key;
		entry->factor[white] = entry->factor[black] = static_cast<uint8_t>(sf_normal);
		entry->game_phase = board.game_phase();

		const int piece_count[Color][piece] =
		{
			{
				board.count<bishop>(white) > 1, board.count<pawn>(white), board.count<knight>(white),
				board.count<bishop>(white), board.count<rook>(white), board.count<queen>(white)
			},
			{
				board.count<bishop>(black) > 1, board.count<pawn>(black), board.count<knight>(black),
				board.count<bishop>(black), board.count<rook>(black), board.count<queen>(black)
			}
		};

		entry->value = static_cast<int16_t>((material_imbalance<white>(piece_count) - material_imbalance<black>(piece_count)
		) / 16);
		return entry;
	}
}
