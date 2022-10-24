#pragma once
#include <vector>
#include <cstdint>

#include "rook_attacks.h"
#include "bishop_attacks.h"

static constexpr int squares = 64;

struct Magic
{
	uint64_t mask;
	uint64_t magic;
	int shift;
	int offset;
};

class attacks
{
public:
	void initialize();

	[[nodiscard]] uint64_t rookAttacks(const uint64_t bitboard, const int index) const
	{
		const auto& m = rook_magics_[index];
		return rook_attacks[attack_table_index(bitboard, m)];
	}

	[[nodiscard]] uint64_t bishopAttacks(const uint64_t bitboard, const int index) const
	{
		const auto& m = bishop_magics_[index];
		return bishop_attacks[attack_table_index(bitboard, m)];
	}

	[[nodiscard]] uint64_t queenAttacks(const uint64_t bitboard, const int index) const
	{
		return rookAttacks(bitboard, index) | bishopAttacks(bitboard, index);
	}

private:
	[[nodiscard]] static uint64_t attack_table_index(const uint64_t bitboard, const Magic& m)
	{
		const auto occupancy = bitboard & m.mask;
		return (occupancy * m.magic >> (squares - m.shift)) + m.offset;
	}

	Magic rook_magics_[squares] = {};
	Magic bishop_magics_[squares] = {};
};
