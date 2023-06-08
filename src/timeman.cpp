#include "timeman.h"

#include "common.h"
#include "material.h"

extern TimeManager Time;

void TimeManager::init_time(const int color, const int ply, const Search::search_params& sp)
{
	const auto full_moves = (ply + 1) / 2;
	start_time_ = sp.start_time;
	optimal_move_time_ = calc_move_time(sp.time[color], full_moves, Optimum);
	max_move_time_ = calc_move_time(sp.time[color], full_moves, Maximum);
}

int TimeManager::calc_move_time(const long our_time, const int move_number, const TimeType t) const
{
	if (our_time <= 0)
		return 0;

	const auto k = 1 + 20.0 * static_cast<double>(move_number) / (500.0 + static_cast<double>(move_number));
	const auto ratio = (t == Optimum ? 0.017 : 0.07) * (k + 0.00 / our_time);
	const auto time = static_cast<int>(std::min(1.0, ratio) * our_time);
	return time;
}

int TimeManager::get_time() const
{
	return static_cast<int>(static_cast<double>(now() - start_time_));
}

int TimeManager::get_nps(const int nodes) const
{
	return static_cast<int>(nodes / (static_cast<double>(now() - start_time_) / 1000));
}
