#include "hash.h"

#include <iostream>
#include <cstring>
#include "bitops.h"

transposition_table tt;

void transposition_table::resize(const size_t mb_size)
{
	const auto new_cluster_count = static_cast<size_t>(1) << msb(mb_size * 1024 * 1024 / sizeof(tt_cluster));

	if (new_cluster_count == cluster_count_)
		return;

	cluster_count_ = new_cluster_count;
	free(mem_);
	mem_ = calloc(cluster_count_ * sizeof(tt_cluster) + cache_line_size - 1, 1);

	if (!mem_)
	{
		std::cerr << "Failed to allocate " << mb_size << "MB for transposition table." << std::endl;
		exit(EXIT_FAILURE);
	}

	table_ = reinterpret_cast<tt_cluster*>(reinterpret_cast<uintptr_t>(mem_) + cache_line_size - 1 & ~(cache_line_size - 1));
}

void transposition_table::clear_table() const
{
	std::memset(table_, 0, cluster_count_ * sizeof(tt_cluster));
}

hash_entry* transposition_table::probe(const uint64_t key, bool& hit) const
{
	const uint16_t key16 = key >> 48;
	auto* const tte = first_entry(key);

	for (auto i = 0; i < tt_cluster_size; ++i)
	{
		if (!tte[i].zobrist16_ || tte[i].zobrist16_ == key16)
		{
			if ((tte[i].flag8_ & 0xFC) != age8_ && tte[i].zobrist16_)
				tte[i].flag8_ = static_cast<uint8_t>(age8_ | tte[i].bound());

			return hit = static_cast<bool>(tte[i].zobrist16_), &tte[i];
		}
	}
	auto* replace = tte;

	for (auto i = 0; i < tt_cluster_size; ++i)
	{
		if (replace->depth8_ - (259 + age8_ - replace->flag8_ & 0xFC) * 2
			> tte[i].depth8_ - (259 + age8_ - tte[i].flag8_ & 0xFC) * 2)
			replace = &tte[i];
	}
	return hit = false, replace;
}
