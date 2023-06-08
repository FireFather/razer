#pragma once
#include "common.h"
#include "movepick.h"

class bit_board;
class EvaluateBB;
class hash_entry;

namespace Search
{
	constexpr int counter_move_prune_threshold = 0;

	struct search_stack
	{
		Move* pv;
		Move killers[2];
		Move current_move;

		piece_history* piece_sq_history;

		int ply;
		int move_count;
		int static_eval;
		int stat_score;
		bool semp;
	};

	struct search_params
	{
		[[nodiscard]] bool use_time() const
		{
			return !(depth | infinite);
		}

		search_params()
		{
			time[white] = time[black] = inc[white] = inc[black] = depth = moves_to_go = 0;
			infinite = 1;
		}

		long time[Color]{};
		int inc[Color]{}, depth, moves_to_go, infinite;
		time_point start_time = 0;
	};

	extern search_params search_param;

	struct root_move
	{
		explicit root_move(const Move m) :
			pv(1, m)
		{
		}

		bool operator ==(const Move& m) const
		{
			return pv[0] == m;
		}

		bool operator <(const root_move& m) const
		{
			return m.score != score ? m.score < score : m.previous_score < previous_score;
		}

		int score = -inf;
		int previous_score = -inf;

		std::vector<Move> pv;
	};

	typedef std::vector<root_move> root_moves;
	void init_search();
	void clear();
	int search_root(bit_board& board, int depth, int alpha, int beta, search_stack* ss);
	void update_piece_sq_history(search_stack* ss, int piece, int to, int bonus);
	void update_stats(const bit_board& board, Move move, search_stack* ss, const Move* quiet_moves, int q_count, int bonus);
	void print(int best_score, int depth, const bit_board& board);
}

inline bool is_ok(const Move m)
{
	return from_sq(m) != to_sq(m);
}

inline int mate_in(const int ply)
{
	return mate - ply;
}

inline int mated_in(const int ply)
{
	return -mate + ply;
}
