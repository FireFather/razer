#pragma once
#include <thread>
#include <atomic>
#include <condition_variable>

#include "movepick.h"
#include "search.h"
#include "pawns.h"
#include "material.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef WIN32_LEAN_AND_MEAN
#undef NOMINMAX

struct Mutex
{
	Mutex()
	{
		InitializeCriticalSection(&cs_);
	}

	~Mutex()
	{
		DeleteCriticalSection(&cs_);
	}

	void lock()
	{
		EnterCriticalSection(&cs_);
	}

	void unlock()
	{
		LeaveCriticalSection(&cs_);
	}

private:
	CRITICAL_SECTION cs_{};
};

enum SyncOut
{
	OUT_LOCK,
	OUT_UNLOCK
};

std::ostream& operator <<(std::ostream&, SyncOut);

#define sync_out std::cout << OUT_LOCK
#define sync_endl std::endl << OUT_UNLOCK
#define sync_indent sync_out << sync_endl

typedef std::condition_variable_any ConditionVariable;

class thread
{
	Mutex mutex_;
	ConditionVariable cv_;

	bool exit_ = false, searching_ = true;
	std::thread std_thread_;

public:
	explicit thread(size_t n);
	virtual ~thread();
	virtual void search();
	void clear();
	void idle_loop();
	void start_searching();
	void wait_for_search_stop();

	int thread_id;

	pawns::pawn_table pawn_table;
	material::mat_table material_table;

	std::atomic<uint64_t> nodes;

	bit_board board{};
	counter_move_history counter_move_history{};
	move_history move_history{};
	piece_sq_history piece_sq_history{};

	int root_depth{};
	int completed_depth{};
	Search::root_moves root_moves;
};

struct MainThread final :
	thread
{
	using thread::thread;
	void search() override;
	static void check_time();
	int previous_score{};
};

struct ThreadPool :
	std::vector<thread*>
{
	void initialize();
	void number_of_threads(size_t);
	void search_start(const bit_board& board, state_list& state, const Search::search_params& sp);

	MainThread* main() const
	{
		return static_cast<MainThread*>(front());
	}

	uint64_t nodes_searched() const
	{
		return accumulate_member(&thread::nodes);
	}

	std::atomic_bool stop;

private:
	state_list set_state_;

	uint64_t accumulate_member(std::atomic<uint64_t> thread::* member) const
	{
		uint64_t sum = 0;
		for (const auto* th : *this)
		{
			sum += (th->*member).load(std::memory_order_relaxed);
		}
		return sum;
	}
};

extern ThreadPool threads;
