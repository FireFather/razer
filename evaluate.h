#pragma once
#include "bitops.h"

class bit_board;
class eval_info;

class Evaluate
{
public:
	[[nodiscard]] int evaluate(const bit_board& board) const;
private:
	static int w_king_shield(const bit_board& board);
	static int b_king_shield(const bit_board& board);
	static bool is_piece(const uint64_t& piece, const bit_board& board, int sq);
};

struct Score
{
	int mg = 0, eg = 0;
};

inline Score make_score(const int m, const int e)
{
	Score x;
	x.mg = m;
	x.eg = e;
	return x;
}

inline bool operator <(const s_move& f, const s_move& s)
{
	return f.score < s.score;
}

inline Score operator -(Score s1, const Score s2)
{
	s1.mg -= s2.mg;
	s1.eg -= s2.eg;
	return s1;
}

inline void operator -=(Score& s1, const Score s2)
{
	s1.mg -= s2.mg;
	s1.eg -= s2.eg;
}

inline Score operator +(Score s1, const Score s2)
{
	s1.mg += s2.mg;
	s1.eg += s2.eg;
	return s1;
}

inline Score operator +(Score s1, const int s2)
{
	s1.mg += s2;
	s1.eg += s2;
	return s1;
}

inline void operator +=(Score& s1, const Score s2)
{
	s1.mg += s2.mg;
	s1.eg += s2.eg;
}

inline Score operator *(Score s1, const Score s2)
{
	s1.mg *= s2.mg;
	s1.eg *= s2.eg;
	return s1;
}

inline Score operator *(Score s1, const int s2)
{
	s1.mg *= s2;
	s1.eg *= s2;
	return s1;
}

inline void operator *=(Score& s1, const Score s2)
{
	s1.mg *= s2.mg;
	s1.eg *= s2.eg;
}

inline Score operator /(Score s1, const Score s2)
{
	s1.mg /= s2.mg;
	s1.eg /= s2.eg;
	return s1;
}

inline Score operator /(Score s1, const int s2)
{
	s1.mg /= s2;
	s1.eg /= s2;
	return s1;
}

inline void operator/=(Score& s1, const Score s2)
{
	s1.mg /= s2.mg;
	s1.eg /= s2.eg;
}

inline int frontmost_sq(const int color, const uint64_t b)
{
	return color == white ? lsb(b) : msb(b);
}

inline int backmost_sq(const int color, const uint64_t b)
{
	return color == white ? msb(b) : lsb(b);
}
