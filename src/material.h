#pragma once
#include "common.h"
#include "hash.h"
#include "evaluate.h"

class bit_board;

namespace material
{
	class entry
	{
	public:
		[[nodiscard]] Score material_value() const
		{
			return make_score(value, value);
		}

		uint64_t key = 0;
		int16_t value = 0;
		uint8_t factor[Color] = {0};
		int game_phase{};
	};

	typedef hash_table<entry, 8192> mat_table;
	entry* probe(const bit_board& board);
}
