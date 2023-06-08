#pragma once
#include "search.h"

enum TimeType
{
	Maximum,
	Optimum
};

class TimeManager
{
public:
	void init_time(int color, int ply, const Search::search_params& sp);
	[[nodiscard]] int calc_move_time(long our_time, int move_number, TimeType t) const;
	[[nodiscard]] int get_time() const;
	[[nodiscard]] int get_nps(int nodes) const;

	[[nodiscard]] int maximum() const
	{
		return max_move_time_;
	}

	[[nodiscard]] int optimum() const
	{
		return optimal_move_time_;
	}

	[[nodiscard]] long elapsed() const
	{
		return static_cast<long>(now() - start_time_);
	}

private:
	time_point start_time_ = 0;
	int optimal_move_time_ = 0;
	int max_move_time_ = 0;
};
