#pragma once

#include "common.h"

class bit_board;
class hash_entry;

template <int GenType>
s_move* generate(const bit_board& board, s_move* m_list);

template <int GenType>
struct move_list
{
	explicit move_list(const bit_board& board) :
		last_(generate<GenType>(board, m_list_))
	{
	}

	[[nodiscard]] const s_move* begin() const
	{
		return m_list_;
	}

	[[nodiscard]] const s_move* end() const
	{
		return last_;
	}

	[[nodiscard]] size_t size() const
	{
		return last_ - m_list_;
	}

	[[nodiscard]] bool contains(const Move m) const
	{
		return std::find(begin(), end(), m) != end();
	}

private:
	s_move m_list_[256]{}, *last_;
};

inline Move create_move(const int from, const int to)
{
	return static_cast<Move>(from | to << 6);
}

template <movetype T, int Pt>
Move create_special(const int from, const int to)
{
	return static_cast<Move>(from | static_cast<unsigned long long>(to) << 6 | T | (static_cast<unsigned __int64>(Pt)
		                                               ? static_cast<unsigned __int64>(Pt - knight) << 15
		                                               : 0LL));
}
