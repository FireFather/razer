#pragma once
#include <array>
#include "common.h"
#include "bitboard.h"

enum stages
{
	main_search,
	captures_init,
	good_captures,
	killers,
	counter_move,
	quiets_init,
	quiet,
	bad_captures,
	q_search,
	captures_q_init,
	captures_q,
	evasion,
	evasions_init,
	evasions_s1,
	stop
};

template <int Index, int Index1, typename T = int16_t>
struct stat_board :
	std::array<std::array<T, Index1>, Index>
{
	void fill(const T& v)
	{
		T* p = &(*this)[0][0];
		std::fill(p, p + sizeof*this / sizeof*p, v);
	}

	void update(T& entry, const int bonus, const int d)
	{
		entry += bonus * 32 - entry * abs(bonus) / d;
	}
};

typedef stat_board<static_cast<int>(Color), static_cast<int>(sq_all) * static_cast<int>(sq_all)> butterfly_board;
typedef stat_board<piece, sq_all> piece_to_board;

struct move_history :
	butterfly_board
{
	void update(const int color, const Move m, const int bonus)
	{
		stat_board::update((*this)[color][from_to(m)], bonus, 324);
	}
};

struct piece_history :
	piece_to_board
{
	void update(const int piece, const int to, const int bonus)
	{
		stat_board::update((*this)[piece][to], bonus, 936);
	}
};

typedef stat_board<piece, sq_all, Move> counter_move_history;
typedef stat_board<piece, sq_all, piece_history> piece_sq_history;

class move_picker
{
public:
	move_picker(const bit_board& board, Move ttm, int depth, const move_history* hist, const piece_history**, Move cm,
	           Move* killers_p);
	move_picker(const bit_board& board, Move ttm, const move_history* hist);
	Move next_move();

private:
	template <int GenType>
	void score();
	int stage_;
	const bit_board& b_;
	const move_history* move_hist_;
	const piece_history** piece_sq_history_{};
	int depth_{};
	Move tt_move_, counter_move_;
	s_move killers_[2]{};
	s_move *current_ = nullptr, *end_ = nullptr, *end_bad_captures_ = nullptr, *end_quiet_moves_ = nullptr;
	s_move m_list_[256]{};
};
