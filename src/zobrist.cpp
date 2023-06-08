#include "zobrist.h"

#include <cmath>
#include <iostream>
#include <random>

#include "bitboard.h"
#include "bitops.h"

std::random_device rd;
std::mt19937_64 mt(rd());
std::uniform_int_distribution<uint64_t> dist(std::llround(std::pow(2, 61)), std::llround(std::pow(2, 62)));

uint64_t zobrist_hash::random64()
{
	const auto ran_ui = dist(mt);
	return ran_ui;
}

void zobrist_hash::zobrist_fill()
{
	for (auto& color : z_array)
	{
		for (auto piece_type = 0; piece_type <= king; piece_type++)
		{
			for (auto square = 0; square < 64; square++)
			{
				if (piece_type == 0)
					color[piece_type][square] = 0LL;
				else
					color[piece_type][square] = random64();
			}
		}
	}

	for (auto& column : z_en_passant)
	{
		column = random64();
	}

	for (int i = no_castling; i <= any_castling; ++i)
	{
		z_castle[i] = random64();
	}
	z_black_move = random64();
}

void zobrist_hash::update_color()
{
	zobrist_key ^= z_black_move;
}

uint64_t zobrist_hash::get_zobrist_hash(const bit_board& bb_board)
{
	uint64_t return_z_key = 0LL;

	auto pieces = bb_board.pieces(white);

	while (pieces)
	{
		const auto square = pop_lsb(&pieces);
		const auto piece = bb_board.piece_on_sq(square);

		return_z_key ^= z_array[white][piece][square];
	}

	pieces = bb_board.pieces(black);

	while (pieces)
	{
		const auto square = pop_lsb(&pieces);
		const auto piece = bb_board.piece_on_sq(square);

		return_z_key ^= z_array[black][piece][square];
	}

	if (bb_board.can_enpassant())
	{
		return_z_key ^= z_en_passant[file_of(bb_board.ep_square())];
	}

	if (bb_board.can_enpassant())
	{
		return_z_key ^= z_en_passant[file_of(bb_board.ep_square())];
	}

	return_z_key ^= z_castle[bb_board.castling_rights()];

	if (bb_board.stm() == black)
		return_z_key ^= z_black_move;

	zobrist_key = return_z_key;

	return return_z_key;
}

void zobrist_hash::test_distribution()
{
	constexpr auto sample_size = 2000;
	int dist_array[sample_size] = {};
	auto t = 0;

	while (t < 1500)
	{
		for (auto i = 0; i < 2000; i++)
		{
			dist_array[static_cast<int>(random64() % sample_size)]++;
		}
		t++;
	}

	for (const auto i : dist_array)
	{
		std::cout << i << std::endl;
	}
}

uint64_t zobrist_hash::debug_key(const bool is_white, const bit_board& bb_board) const
{
	uint64_t return_z_key = 0LL;

	auto pieces = bb_board.pieces(white);

	while (pieces)
	{
		const auto square = pop_lsb(&pieces);
		const auto piece = bb_board.piece_on_sq(square);

		return_z_key ^= z_array[white][piece][square];
	}

	pieces = bb_board.pieces(black);

	while (pieces)
	{
		const auto square = pop_lsb(&pieces);
		const auto piece = bb_board.piece_on_sq(square);

		return_z_key ^= z_array[black][piece][square];
	}

	if (bb_board.can_enpassant())
	{
		return_z_key ^= z_en_passant[file_of(bb_board.ep_square())];
	}

	return_z_key ^= z_castle[bb_board.castling_rights()];

	if (is_white == false)
	{
		return_z_key ^= z_black_move;
	}

	return return_z_key;
}

uint64_t zobrist_hash::debug_pawn_key(const bit_board& bb_board) const
{
	uint64_t p_key = 0LL;

	for (auto s = 0; s < 64; ++s)
	{
		if ((bb_board.by_color_pieces_bb[white][pawn] >> s & 1) == 1)
		{
			p_key ^= z_array[white][pawn][s];
		}

		if ((bb_board.by_color_pieces_bb[black][pawn] >> s & 1) == 1)
		{
			p_key ^= z_array[black][pawn][s];
		}
	}
	return p_key;
}

uint64_t zobrist_hash::debug_material_key(const bit_board& bb_board) const
{
	uint64_t m_key = 0LL;

	for (auto color = 0; color < Color; ++color)
	{
		for (int pt = pawn; pt <= king; ++pt)
		{
			for (auto count = 0; count <= bb_board.piece_count[color][pt]; ++count)
			{
				m_key ^= z_array[color][pt][count];
			}
		}
	}
	return m_key;
}
