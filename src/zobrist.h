#pragma once
#include "common.h"

class bit_board;

class zobrist_hash
{
public:
	uint64_t zobrist_key;
	uint64_t z_array[2][7][64];
	uint64_t z_castle[castling_rights];
	uint64_t z_black_move;
	uint64_t z_en_passant[8];
	static uint64_t random64();
	void zobrist_fill();
	uint64_t get_zobrist_hash(const bit_board& bb_board);
	void update_color();
	static void test_distribution();
	[[nodiscard]] uint64_t debug_key(bool is_white, const bit_board& bb_board) const;
	[[nodiscard]] uint64_t debug_pawn_key(const bit_board& bb_board) const;
	[[nodiscard]] uint64_t debug_material_key(const bit_board& bb_board) const;
};

namespace zobrist
{
	static uint64_t zob_array[Color][piece][sq_all];
	static uint64_t en_passant[8];
	static uint64_t castling[castling_rights];
	static uint64_t color;
}
