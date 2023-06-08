#include "perft.h"

#include <cstdlib>
#include <iostream>


#include "common.h"
#include "movegen.h"
#include "threads.h"
#include "uci.h"

using namespace std;

void perft::idle()
{
	std::unique_lock lock(mutex_);
	cv_.wait(lock, [&] { return searching; });
	lock.unlock();
	start_search();
}

void perft::start_search()
{
	for (const auto& m : my_moves)
	{
		search_move(m, depth);
		if (is_divide)
			sync_out << Uci::move_to_str(m) << ": " << move_nodes << sync_endl;
		total_nodes += move_nodes;
	}
	searching = false;
}

void perft::search_move(const Move m, const int d)
{
	const auto color = board.stm();
	board.make_move(m, si, color);
	move_nodes = perft_divide<true>(d - 1);
	board.unmake_move(m, color);
}

void perft::perft_init(const bit_board& board, const bool is_divide, const int depth)
{
	const auto start = now();
	std::vector<perft*> perft_threads;
	const auto p_threads = static_cast<unsigned>(threads.size());

	if (perft_threads.size() != p_threads)
		for (unsigned int i = 0; i < p_threads; ++i)
			perft_threads.push_back(new perft);

	auto i = 0;
	for (const auto& m : move_list<legal>(board))
	{
		perft_threads[i % p_threads]->my_moves.emplace_back(m);
		++i;
	}

	for (auto* th : perft_threads)
	{
		th->board = board;
		th->depth = depth;
		th->searching = true;
		if (is_divide)
			th->is_divide = true;
		th->notify();
	}

	uint64_t nodes = 0;
	auto it = 0;

	for (auto* th : perft_threads)
	{
		th->std_thread.join();
		nodes += th->total_nodes;
		delete perft_threads[it++];
	}

	const auto elapsed_time = static_cast<double>(now() + 1 - start) / 1000;
	const auto nps = static_cast<double>(nodes) / elapsed_time;

	std::ostringstream ss;
	ss.precision(2);

	ss << "Nodes: " << nodes << endl;
	cout << ss.str();
	ss.str(std::string());

	ss << "Time : " << std::fixed << elapsed_time << " secs" << endl;
	cout << ss.str();
	ss.str(std::string());

	ss.precision(0);
	ss << "NPS  : " << std::fixed << nps << endl;
	cout << ss.str();
	ss.str(std::string());
	sync_indent;
}

template <bool Root>
uint64_t perft::perft_divide(const int d)
{
	state_info st{};
	uint64_t nodes = 0;
	s_move m_list[256];
	const auto* const end = generate<legal>(board, m_list);

	if (d <= 0)
		return 1;

	const auto color = board.stm();
	const auto leaf = d == 2;

	for (auto* i = m_list; i != end; ++i)
	{
		auto m = i->move;
		board.make_move(m, st, color);
		const auto count = leaf ? move_list<legal>(board).size() : perft_divide<false>(d - 1);
		nodes += count;
		board.unmake_move(m, color);
	}
	return nodes;
}

template uint64_t perft::perft_divide<true>(int depth);
