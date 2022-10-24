#pragma once
#include "common.h"
#include "threads.h"

class perft
{
	Mutex mutex_;
	ConditionVariable cv_;

public:
	std::thread std_thread;

	perft() : std_thread(&perft::idle, this)
	{
	}

	void notify()
	{
		cv_.notify_one();
	}

	void idle();
	void start_search();
	static void perft_init(const bit_board& board, bool is_divide, int depth);
	void search_move(Move m, int d);

	template <bool Root>
	uint64_t perft_divide(int d);

	bool searching = false;
	bool is_divide = false;
	int depth = 0;
	uint64_t move_nodes = 0;
	uint64_t total_nodes = 0;
	std::vector<Move> my_moves;
	state_info si{};
	bit_board board;
};
