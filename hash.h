#pragma once
#include <vector>
#include "common.h"

class hash_entry
{
public:

	[[nodiscard]] Move move() const
	{
		return static_cast<Move>(move16_);
	}

	[[nodiscard]] int depth() const
	{
		return depth8_;
	}

	[[nodiscard]] int eval() const
	{
		return eval16_;
	}

	[[nodiscard]] int bound() const
	{
		return static_cast<flag>(flag8_) & 0x3;
	}

	void save(const uint64_t key, const int d, const int e, const Move m, const flag hash_flag, const uint8_t age)
	{
		if (m || key >> 48 != zobrist16_)
			move16_ = static_cast<uint16_t>(m);

		if (key >> 48 != zobrist16_ || d > depth8_ - 4 || hash_flag == exact)
		{
			depth8_ = static_cast<uint8_t>(d);
			flag8_ = static_cast<uint8_t>(hash_flag) | age;
			eval16_ = static_cast<int16_t>(e);
			zobrist16_ = static_cast<uint16_t>(key >> 48);
		}
	}

private:
	friend class transposition_table;

	uint8_t flag8_;
	uint8_t depth8_;
	int16_t eval16_;
	uint16_t move16_;
	uint16_t zobrist16_;
};

class transposition_table
{
	static constexpr int tt_cluster_size = 4;

	struct tt_cluster
	{
		hash_entry entry[tt_cluster_size];
	};

public:
	~transposition_table()
	{
		free(mem_);
	}

	hash_entry* probe(uint64_t key, bool& hit) const;
	[[nodiscard]] hash_entry* first_entry(uint64_t key) const;
	void resize(size_t mb_size);
	void clear_table() const;

	[[nodiscard]] uint8_t age() const
	{
		return age8_;
	}

	void new_search()
	{
		age8_ += 4;
	}

private:
	size_t cluster_count_ = 0;
	tt_cluster* table_ = nullptr;
	void* mem_ = nullptr;
	uint8_t age8_ = 0;
};

extern transposition_table tt;

inline hash_entry* transposition_table::first_entry(const uint64_t key) const
{
	return &table_[static_cast<size_t>(key) & cluster_count_ - 1].entry[0];
}

template <class Entry, int Size>
struct hash_table
{
	hash_table() : table_(Size, Entry())
	{
	}

	Entry* operator [](const uint64_t k)
	{
		return &table_[static_cast<uint32_t>(k) & Size - 1];
	}

private:
	std::vector<Entry> table_;
};
