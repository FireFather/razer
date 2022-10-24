#include "threads.h"

#include "movegen.h"

ThreadPool threads;

std::ostream& operator <<(std::ostream& os, const SyncOut sp)
{
	static Mutex m;
	if (sp == OUT_LOCK)
		m.lock();
	if (sp == OUT_UNLOCK)
		m.unlock();
	return os;
}

thread::thread(const size_t n) : std_thread_(&thread::idle_loop, this), thread_id(static_cast<int>(n))
{
	wait_for_search_stop();
	clear();
}

thread::~thread()
{
	exit_ = true;
	start_searching();
	std_thread_.join();
}

void ThreadPool::initialize()
{
	push_back(new MainThread(0));
}

void thread::clear()
{
	counter_move_history.fill(move_none);
	move_history.fill(0);
	for (auto& to : piece_sq_history)
		for (auto& h : to)
			h.fill(0);
	piece_sq_history[no_piece][0].fill(Search::counter_move_prune_threshold - 1);
}

void thread::start_searching()
{
	std::lock_guard lock(mutex_);
	searching_ = true;
	cv_.notify_one();
}

void thread::wait_for_search_stop()
{
	std::unique_lock lock(mutex_);
	cv_.wait(lock, [&] { return !searching_; });
}

void thread::idle_loop()
{
	while (true)
	{
		std::unique_lock lock(mutex_);
		searching_ = false;
		cv_.notify_one();
		cv_.wait(lock, [&] { return searching_; });

		if (exit_)
			return;

		lock.unlock();
		search();
	}
}

void ThreadPool::number_of_threads(const size_t n)
{
	while (size() < n)
		push_back(new thread(size()));
	while (size() > n && n > 0)
		delete back(), pop_back();
}

void ThreadPool::search_start(const bit_board& board, state_list& state, const Search::search_params& sp)
{
	main()->wait_for_search_stop();
	stop = false;
	Search::root_moves root_moves;
	for (const auto m : move_list<legal>(board))
		root_moves.emplace_back(m);
	Search::search_param = sp;
	if (state.get())
		set_state_ = std::move(state);
	const auto st = set_state_->back();
	for (auto* th : threads)
	{
		th->nodes = 0;
		th->board = board;
		th->root_moves = root_moves;
		th->root_depth = 1;
		th->board.set_state(&set_state_->back(), th);
	}
	set_state_->back() = st;
	main()->start_searching();
}
