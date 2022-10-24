#include "search.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "bitboard.h"
#include "evaluate.h"
#include "hash.h"
#include "movegen.h"
#include "threads.h"
#include "time.h"
#include "uci.h"

using namespace Search;
TimeManager Time;

inline int value_from_tt(int val, int ply);
inline int value_to_tt(int val, int ply);

enum node_type
{
	non_pv,
	PV
};

template <node_type Nt>
int alpha_beta(bit_board& board, int depth, int alpha, int beta, search_stack* ss, bool allow_null);
template <node_type Nt, bool InCheck>
int quiescent(bit_board& board, int alpha, int beta, search_stack* ss, int depth);

int tempo = 10;
int reductions[2][2][64][64];
int futility_move_count[2][32];
int draw_value[Color];

namespace Search
{
	search_params search_param;
}

constexpr int th_skip_size0 [] =
{
	1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3
};

constexpr int th_skip_size1 [] =
{
	0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5
};

template <bool IsPv>
int reduction(const bool improving, const int depth, const int move_count)
{
	return reductions[IsPv][improving][std::min(depth, 63)][std::min(move_count, 63)];
}

inline int stat_bonus(const int depth)
{
	return depth > 14 ? 0 : depth * depth + 2 * depth - 2;
}

void Search::init_search()
{
	for (auto imp = 0; imp <= 1; ++imp)
		for (auto d = 1; d < 64; ++d)
			for (auto mc = 1; mc < 64; ++mc)
			{
				const auto r = std::log(d) * std::log(mc) / 2.45;
				reductions[non_pv][imp][d][mc] = static_cast<int>(std::round(r));
				reductions[PV][imp][d][mc] = std::max(reductions[non_pv][imp][d][mc] - 1, 0);
				if (!imp && reductions[non_pv][imp][d][mc] >= 2)
					reductions[non_pv][imp][d][mc]++;
			}

	for (auto d = 0; d < 32; ++d)
	{
		futility_move_count[0][d] = static_cast<int>(2.4 + 0.222 * pow(d * 2.0 + 0.00, 1.8));
		futility_move_count[1][d] = static_cast<int>(3.0 + 0.300 * pow(d * 2.0 + 0.98, 1.8));
	}
}

void Search::clear()
{
	tt.clear_table();
	threads.main()->wait_for_search_stop();
	for (auto* th : threads)
		th->clear();
	threads.main()->previous_score = inf;
}

void MainThread::search()
{
	const auto color = board.stm();
	constexpr auto contempt = piece_value[pawn] / 100;
	draw_value[color] = draw - contempt;
	draw_value[!color] = draw + contempt;
	Time.init_time(color, num_moves, search_param);
	tt.new_search();

	for (auto* th : threads)
	{
		if (th != this)
			th->start_searching();
	}

	thread::search();
	threads.stop = true;

	for (auto* th : threads)
		if (th != this)
			th->wait_for_search_stop();

	const thread* best_thread = this;

	const auto m = best_thread->root_moves[0].pv[0];
	const auto best_move = Uci::move_to_str(m);
	std::cout << "bestmove " << best_move << std::endl;
	previous_score = best_thread->root_moves[0].score;
}

void thread::search()
{
	const auto color = board.stm();
	const auto* const main_thread = this == threads.main() ? threads.main() : nullptr;

	search_stack stack[max_ply + 6], *ss = stack + 4;
	std::memset(ss - 4, 0, 7 * sizeof(search_stack));

	for (auto i = 4; i > 0; i--)
		(ss - i)->piece_sq_history = this->piece_sq_history[no_piece].data();

	Move best_move;
	constexpr auto alpha = -inf, beta = inf;

	while (root_depth < max_ply && !threads.stop
		&& !(search_param.depth && main_thread && root_depth > search_param.depth))
	{
		if (thread_id)
		{
			if (const auto s = (thread_id - 1) % 12; (root_depth + num_moves + th_skip_size1[s] / th_skip_size0[s]) % 2)
			{
				++root_depth;
				continue;
			}
		}
		for (auto& rm : root_moves)
			rm.previous_score = rm.score;

		std::stable_sort(root_moves.begin(), root_moves.end());
		const auto best_score = search_root(board, root_depth, alpha, beta, ss);
		std::stable_sort(root_moves.begin(), root_moves.end());

		if (!threads.stop)
		{
			best_move = root_moves[0].pv[0];
			completed_depth = root_depth;

			if (!main_thread)
				continue;

			if (main_thread)
				print(best_score, root_depth, board);

			if (search_param.use_time())
			{
				if (root_moves.size() == 1 || Time.elapsed() > Time.optimum())
					threads.stop = true;
			}
		}
		else
			break;
		root_depth++;
	}

	if (main_thread && search_param.depth)
		threads.stop = true;

	state_info st{};
	board.make_move(best_move, st, color);
}

int Search::search_root(bit_board& board, const int depth, int alpha, const int beta, search_stack* ss)
{
	auto score = 0;
	auto best = -inf;
	auto legal_moves = 0;
	auto hash_flag = Alpha;
	Move quiet_moves[64]{};
	int quiets_count;
	state_info st{};
	auto* this_thread = board.this_thread();

	if (this_thread == threads.main())
		MainThread::check_time();

	ss->move_count = quiets_count = 0;
	ss->stat_score = 0;
	(ss + 2)->killers[0] = (ss + 2)->killers[1] = move_none;
	ss->current_move = move_none;
	ss->piece_sq_history = this_thread->piece_sq_history[no_piece].data();
	bool tt_hit;
	auto* tt_entry = tt.probe(board.tt_key(), tt_hit);
	const auto tt_move = this_thread->root_moves[0].pv[0];
	ss->ply = 1;
	const auto color = board.stm();
	const bool flag_in_check = board.checkers();
	const check_info ci(board);

	const piece_history* piece_hist [] =
	{
		(ss - 1)->piece_sq_history, (ss - 2)->piece_sq_history, nullptr, (ss - 4)->piece_sq_history
	};

	move_picker mp(board, tt_move, depth, &this_thread->move_history, piece_hist, move_none, ss->killers);
	Move new_move, best_move = move_none;

	while ((new_move = mp.next_move()) != move_none)
	{
		if (!board.is_legal(new_move, ci.pinned))
		{
			continue;
		}
		board.make_move(new_move, st, color);
		ss->move_count = ++legal_moves;
		ss->current_move = new_move;

		if (best == -inf)
		{
			Move pv[max_ply + 1];
			(ss + 1)->pv = pv;
			(ss + 1)->pv[0] = move_none;
			score = -alpha_beta<PV>(board, depth - 1, -beta, -alpha, ss + 1, true);
		}
		else
		{
			score = -alpha_beta<non_pv>(board, depth - 1, -alpha - 1, -alpha, ss + 1, true);
			if (score > alpha)
			{
				score = -alpha_beta<PV>(board, depth - 1, -beta, -alpha, ss + 1, true);
			}
		}

		board.unmake_move(new_move, color);
		if (!board.capture_or_promotion(new_move) && quiets_count < 64)
		{
			quiet_moves[quiets_count] = new_move;
			quiets_count++;
		}

		auto& rm = *std::find(this_thread->root_moves.begin(), this_thread->root_moves.end(), new_move);

		if (score > best)
		{
			best = score;
			best_move = new_move;
			rm.score = best;
			rm.pv.resize(1);

			for (auto* m = (ss + 1)->pv; *m != move_none; ++m)
				rm.pv.push_back(*m);
		}
		else
			rm.score = -inf;

		if (score > alpha)
		{
			if (score > beta)
			{
				hash_flag = Beta;
				alpha = beta;
				break;
			}

			alpha = score;
			hash_flag = exact;
		}
	}
	tt_entry->save(board.tt_key(), depth, value_to_tt(alpha, ss->ply), best_move, hash_flag, tt.age());

	if (alpha >= beta && !flag_in_check && !board.capture_or_promotion(best_move))
	{
		update_stats(board, best_move, ss, quiet_moves, quiets_count, stat_bonus(depth));
	}
	return alpha;
}

void update_pv(Move* pv, const Move m, Move* child_pv)
{
	for (*pv++ = m; child_pv && *child_pv != move_none;)
		*pv++ = *child_pv++;
	*pv = move_none;
}

int razor_margin(const int depth)
{
	return 50 * depth + 50;
}

int se_min_depth = 3;
int se_beta_min_margin = 100;
int se_depth_factor = 120;

int raz_max_depth = 4;

int fp_max_depth = 7;
int fp_factor = 50;
int nms_min_depth = 2;
int nms_base_reduction = 3;
int nms_depth_divisor = 6;
int vs_max_depth = 12;

int iid_min_depth = 6;
int iid_se_margin = 100;
int iid_depth_factor = 3;
int iid_depth_divisor = 4;
int iid_depth_subtractant = 2;

int lmr_min_depth = 3;
int lmr_move_count_min = 15;
int lmr_stat_score_margin = 4000;
int lmr_depth_divisor = 20000;
int lmr_min_n_pmaterial = 2100;

template <node_type Nt>
int alpha_beta(bit_board& board, int depth, int alpha, int beta, search_stack* ss, bool allow_null)
{
	const auto is_pv = Nt == PV;
	const auto color = board.stm();
	int score;
	int new_depth, quiets_count;
	auto extension = 0;
	bool flag_in_check;
	bool capture_or_promotion, gives_check;
	flag_in_check = false;
	capture_or_promotion = gives_check = false;
	auto* this_thread = board.this_thread();

	if (this_thread == threads.main())
		static_cast<MainThread*>(this_thread)->check_time();

	if (threads.stop.load(std::memory_order_relaxed) || board.is_draw(ss->ply) || ss->ply >= max_ply)
		return draw_value[color];

	state_info st{};
	Move quiet_moves[64]{};

	ss->move_count = quiets_count = 0;
	ss->ply = (ss - 1)->ply + 1;
	ss->stat_score = 0;
	(ss + 2)->killers[0] = (ss + 2)->killers[1] = move_none;
	ss->current_move = move_none;
	ss->piece_sq_history = this_thread->piece_sq_history[no_piece].data();
	(ss + 1)->semp = false;
	auto prev_sq = to_sq((ss - 1)->current_move);
	alpha = std::max(mated_in(ss->ply), alpha);
	beta = std::min(mate_in(ss->ply + 1), beta);

	if (alpha >= beta)
		return alpha;

	hash_entry* tt_entry;
	Move tt_move;
	int tt_value;
	bool tt_hit;
	tt_entry = tt.probe(board.tt_key(), tt_hit);
	tt_move = tt_hit ? tt_entry->move() : move_none;
	tt_value = tt_hit ? value_from_tt(tt_entry->eval(), ss->ply) : 0;

	if (!is_pv
		&& tt_hit
		&& tt_entry->depth() >= depth
		&& (tt_value >= beta ? tt_entry->bound() == Beta : tt_entry->bound() == Alpha))
	{
		if (tt_move)
		{
			if (tt_value >= beta)
			{
				if (!board.capture_or_promotion(tt_move))
					update_stats(board, tt_move, ss, nullptr, 0, stat_bonus(depth));
				if ((ss - 1)->move_count == 1 && !board.captured_piece())
					update_piece_sq_history(ss - 1, board.piece_on_sq(prev_sq), prev_sq, -stat_bonus(depth + 1));
			}
			else if (!board.capture_or_promotion(tt_move))
			{
				auto penalty = -stat_bonus(depth);
				this_thread->move_history.update(board.stm(), tt_move, penalty);
				update_piece_sq_history(ss, board.piece_on_sq(from_sq(tt_move)), to_sq(tt_move), penalty);
			}
		}
		return tt_value;
	}

	// static eval
	flag_in_check = board.checkers();
	if (flag_in_check)
	{
		ss->static_eval = 0;
		goto moves_loop;
	}

	Evaluate eval;
	ss->static_eval = (ss - 1)->current_move != move_null ? eval.evaluate(board) : -(ss - 1)->static_eval + 2 * tempo;

	if (ss->semp)
		goto moves_loop;

	// razoring
	if (!is_pv
		&& depth < raz_max_depth
		&& ss->static_eval + razor_margin(depth) <= alpha
		&& !tt_move)
	{
		if (depth <= 1 && ss->static_eval + razor_margin(depth) <= alpha)
			return quiescent<non_pv, false>(board, alpha, beta, ss, 0);

		auto raz_alpha = alpha - razor_margin(depth);

		if (auto qvalue = quiescent<non_pv, false>(board, raz_alpha, raz_alpha + 1, ss, 0); qvalue <= raz_alpha)
			return qvalue;
	}

	// futility pruning
	if (depth < fp_max_depth
		&& ss->static_eval - fp_factor * depth >= beta
		&& ss->static_eval < known_win
		&& board.non_pawn_material(board.stm()))
		return ss->static_eval;

	// null move search
	if (allow_null
		&& !is_pv
		&& depth > nms_min_depth)
	{
		const auto R = nms_base_reduction + depth / nms_depth_divisor;
		ss->current_move = move_null;
		ss->piece_sq_history = this_thread->piece_sq_history[no_piece].data();
		board.make_null_move(st);
		(ss + 1)->semp = true;
		score = depth - 1 - R > 0
			? -alpha_beta<non_pv>(board, depth - R - 1, -beta, -beta + 1, ss + 1, false)
			: -quiescent<non_pv, false>(board, -beta, -beta + 1, ss, 0);
		(ss + 1)->semp = false;
		board.undo_null_move();

		// verification search
		if (score >= beta)
		{
			if (score >= mate_in_max_ply)
				score = beta;
			if (depth < vs_max_depth && abs(beta) < known_win)
				return score;
			ss->semp = true;
			auto value = depth - R < 1
				? quiescent<non_pv, false>(board, beta - 1, beta, ss, 0)
				: alpha_beta<non_pv>(board, depth - R, beta - 1, beta, ss, false);
			ss->semp = false;
			if (value >= beta)
				return score;
		}
	}

	// internal iterative deepening
	if (depth >= iid_min_depth
		&& !tt_move
		&& (is_pv || ss->static_eval + iid_se_margin >= beta))
	{
		auto d = iid_depth_factor * depth / iid_depth_divisor - iid_depth_subtractant;
		ss->semp = true;
		alpha_beta<Nt>(board, d, alpha, beta, ss, false);
		ss->semp = false;
		tt_entry = tt.probe(board.tt_key(), tt_hit);
		tt_move = tt_hit ? tt_entry->move() : move_none;
	}

moves_loop:

	auto improving = ss->static_eval >= (ss - 2)->static_eval || ss->static_eval == 0 || (ss - 2)->static_eval == 0;
	auto hash_flag = Alpha;
	auto legal_moves = 0, best_score = -inf;
	score = best_score;
	bool tt_move_capture = false, do_full_depth_search;

	const piece_history* piece_hist [] =
	{
		(ss - 1)->piece_sq_history, (ss - 2)->piece_sq_history, nullptr, (ss - 4)->piece_sq_history
	};

	auto counter_move = this_thread->counter_move_history[board.piece_on_sq(prev_sq)][prev_sq];
	check_info ci(board);
	Move new_move, best_move = move_none;
	move_picker mp(board, tt_move, depth, &this_thread->move_history, piece_hist, counter_move, ss->killers);

	while ((new_move = mp.next_move()) != move_none)
	{
		if (!board.is_legal(new_move, ci.pinned))
		{
			continue;
		}

		prefetch(tt.first_entry(board.next_key(new_move)));
		auto moved_piece = board.moved_piece(new_move);
		capture_or_promotion = board.capture_or_promotion(new_move);
		tt_move_capture = tt_move && board.capture_or_promotion(tt_move);
		board.make_move(new_move, st, color);
		ss->move_count = ++legal_moves;
		gives_check = st.checkers;
		ss->current_move = new_move;
		ss->piece_sq_history = &this_thread->piece_sq_history[moved_piece][to_sq(new_move)];

		// check extension
		extension = gives_check ? 1 : 0;
		new_depth = depth - 1 + extension;

		// reduced depth search (LMR)
		if (depth >= lmr_min_depth
			&& legal_moves > 1
			&& !capture_or_promotion)
		{
			int depth_r = reduction<Nt>(improving, depth, legal_moves);

			if (capture_or_promotion)
				depth_r -= depth_r ? 1 : 0;
			else
			{
				if ((ss - 1)->move_count > lmr_move_count_min)
					depth_r -= 1;
				if (tt_hit && tt_entry->bound() == exact)
					depth_r -= 1;
				if (tt_move_capture)
					depth_r += 1;
				if (depth_r && move_type(new_move) == normal
					&& !board.see_ge(create_move(to_sq(new_move), from_sq(new_move))))
					depth_r -= 2;

				ss->stat_score = this_thread->move_history[color][from_to(new_move)]
					+ (*piece_hist[0])[moved_piece][to_sq(new_move)] + (*piece_hist[1])[moved_piece][to_sq(new_move)]
					+ (*piece_hist[3])[moved_piece][to_sq(new_move)] - lmr_stat_score_margin;

				if (ss->stat_score >= 0 && (ss - 1)->stat_score < 0)
					depth_r -= 1;
				else if ((ss - 1)->stat_score >= 0 && ss->stat_score < 0)
					depth_r += 1;

				depth_r = std::max(0, depth_r - ss->stat_score / lmr_depth_divisor);
			}

			auto d = std::max(new_depth - depth_r, 1);
			score = -alpha_beta<non_pv>(board, d, -(alpha + 1), -alpha, ss + 1, true);
			do_full_depth_search = score > alpha && d != new_depth;
		}
		else
			do_full_depth_search = !is_pv || legal_moves > 1;

		if (is_pv)
			(ss + 1)->pv = nullptr;

		// full depth search
		if (do_full_depth_search)
			score = new_depth < 1
				? gives_check
				? -quiescent<non_pv, true>(board, -(alpha + 1), -alpha, ss + 1, 0)
				: -quiescent<non_pv, false>(board, -(alpha + 1), -alpha, ss + 1, 0)
				: -alpha_beta<non_pv>(board, new_depth, -(alpha + 1), -alpha, ss + 1, true);

		if (is_pv
			&& (legal_moves == 1 || score > alpha
			&& score < beta))
		{
			Move pv[max_ply + 1];
			(ss + 1)->pv = pv;
			(ss + 1)->pv[0] = move_none;
			score = new_depth < 1
				? gives_check
				? -quiescent<PV, true>(board, -beta, -alpha, ss + 1, 0)
				: -quiescent<PV, false>(board, -beta, -alpha, ss + 1, 0)
				: -alpha_beta<PV>(board, new_depth, -beta, -alpha, ss + 1, true);
		}

		if (!capture_or_promotion && new_move != best_move && quiets_count < 64)
		{
			quiet_moves[quiets_count++] = new_move;
		}
		board.unmake_move(new_move, color);

		if (threads.stop.load(std::memory_order_relaxed))
			return 0;

		if (score > best_score)
		{
			best_score = score;
			if (score > alpha)
			{
				best_move = new_move;
				if (is_pv)
					update_pv(ss->pv, best_move, (ss + 1)->pv);
				if (score >= beta)
				{
					hash_flag = Beta;
					alpha = beta;
					break;
				}
				alpha = score;
				hash_flag = exact;
			}
		}
	}

	if (!legal_moves)
		alpha = flag_in_check ? mated_in(ss->ply) : draw_value[color];
	else if (best_move)
	{
		if (!board.capture_or_promotion(best_move))
			update_stats(board, best_move, ss, quiet_moves, quiets_count, stat_bonus(depth));
		if ((ss - 1)->move_count == 1 && !board.captured_piece())
			update_piece_sq_history(ss - 1, board.piece_on_sq(prev_sq), prev_sq, -stat_bonus(depth + 1));
	}
	else if (depth >= 3 && !board.captured_piece() && is_ok((ss - 1)->current_move))
		update_piece_sq_history(ss - 1, board.piece_on_sq(prev_sq), prev_sq, stat_bonus(depth));

	tt_entry->save(board.tt_key(), depth, value_to_tt(alpha, ss->ply), best_move, hash_flag, tt.age());
	return alpha;
}

template <node_type Nt, bool InCheck>
int quiescent(bit_board& board, int alpha, const int beta, search_stack* ss, const int depth)
{
	const auto color = board.stm();
	const auto is_pv = Nt == PV;
	bool tt_hit;
	state_info st{};
	Move best_move, new_move;
	ss->ply = (ss - 1)->ply + 1;
	ss->current_move = best_move = move_none;
	auto standing_pat = 0;

	if (board.is_draw(ss->ply) || ss->ply == max_ply)
		return draw_value[color];

	auto* tt_entry = tt.probe(board.tt_key(), tt_hit);
	const auto tt_move = tt_hit ? tt_entry->move() : move_none;

	if (const auto tt_value = tt_hit ? value_from_tt(tt_entry->eval(), ss->ply) : 0; tt_hit && tt_entry->depth() >= depth_qs
		&& (is_pv ? tt_entry->bound() == exact : tt_value >= beta ? tt_entry->bound() == Beta : tt_entry->bound() == Alpha))
	{
		return tt_value;
	}

	const auto* this_thread = board.this_thread();
	if (InCheck)
	{
		ss->static_eval = 0;
		standing_pat = INFINITE;
	}
	else
	{
		constexpr Evaluate eval;
		standing_pat = eval.evaluate(board);

		if (standing_pat >= beta)
		{
			if (tt_hit)
				tt_entry->save(board.tt_key(), depth_qs, value_to_tt(standing_pat, ss->ply), move_none, Beta, tt.age());
			return standing_pat;
		}

		if (is_pv && alpha < standing_pat)
		{
			alpha = standing_pat;
		}
	}

	auto hash_flag = Alpha;
	const check_info ci(board);
	move_picker mp(board, tt_move, &this_thread->move_history);

	while ((new_move = mp.next_move()) != move_none)
	{
		bool gives_check = static_cast<movetype>(new_move) == normal
			&& !ci.dc_candidates
			? ci.check_sq[static_cast<movetype>(board.piece_on_sq(from_sq(new_move)))] & to_sq(new_move)
			: board.gives_check(new_move, ci);

		if (!board.is_legal(new_move, ci.pinned))
		{
			continue;
		}

		if (constexpr auto qs_sp_margin = 100; standing_pat + piece_value[board.piece_on_sq(to_sq(new_move))] + qs_sp_margin < alpha
			&& board.b_info.side_material[!color] - piece_value[board.piece_on_sq(to_sq(new_move))] > endgame_mat
			&& move_type(new_move) != promotion)
			continue;

		if (!is_pv && board.SEE(new_move, color, true) < 0)
			continue;

		prefetch(tt.first_entry(board.next_key(new_move)));
		board.make_move(new_move, st, color);
		const int score = gives_check
			? -quiescent<Nt, true>(board, -beta, -alpha, ss, depth - 1)
			: -quiescent<Nt, false>(board, -beta, -alpha, ss, depth - 1);
		board.unmake_move(new_move, color);

		if (score > alpha)
		{
			best_move = new_move;
			if (score >= beta)
			{
				hash_flag = Beta;
				alpha = beta;
				break;
			}
			hash_flag = exact;
			alpha = score;
		}
	}

	tt_entry->save(board.tt_key(), depth_qs, value_to_tt(alpha, ss->ply), best_move, hash_flag, tt.age());
	return alpha;
}

void Search::update_stats(const bit_board& board, const Move move, search_stack* ss, const Move* quiet_moves, const int q_count, const int bonus)
{
	if (move != ss->killers[0])
	{
		ss->killers[1] = ss->killers[0];
		ss->killers[0] = move;
	}

	const auto color = board.stm();
	auto* this_thread = board.this_thread();
	this_thread->move_history.update(color, move, bonus);
	update_piece_sq_history(ss, board.piece_on_sq(from_sq(move)), to_sq(move), bonus);

	if (is_ok((ss - 1)->current_move))
	{
		const auto prev_sq = to_sq((ss - 1)->current_move);
		this_thread->counter_move_history[board.piece_on_sq(prev_sq)][prev_sq] = move;
	}

	for (auto i = 0; i < q_count; ++i)
	{
		this_thread->move_history.update(color, quiet_moves[i], -bonus);
		update_piece_sq_history(ss, board.piece_on_sq(from_sq(quiet_moves[i])), to_sq(move), -bonus);
	}
}

void Search::update_piece_sq_history(search_stack * ss, const int piece, const int to, const int bonus)
{
	for (const auto i : {1, 2, 4})
		if (is_ok((ss - i)->current_move))
			(ss - i)->piece_sq_history->update(piece, to, bonus);
}

inline int value_to_tt(const int val, const int ply)
{
	return val >= mate_in_max_ply ? val + ply : val <= mated_in_max_ply ? val - ply : val;
}

inline int value_from_tt(const int val, const int ply)
{
	return val == 0 ? 0 : val >= mate_in_max_ply ? val - ply : val <= mated_in_max_ply ? val + ply : val;
}

void MainThread::check_time()
{
	if (search_param.use_time() && Time.elapsed() > Time.maximum())
		threads.stop = true;
}

void Search::print(const int best_score, const int depth, const bit_board& board)
{
	std::stringstream ss;
	const auto& root_moves = board.this_thread()->root_moves;
	const auto time = Time.get_time();
	const auto nodes = static_cast<int>(threads.nodes_searched());
	auto nps = Time.get_nps(nodes);
	if (nps < 0) nps = 0;

	ss << "info depth " << depth << " time " << time << " nodes " << nodes << " nps " << nps << " score ";
	if (abs(best_score) < mate - max_ply)
		ss << "cp " << best_score;
	else
		ss << "mate " << (best_score > 0 ? mate - best_score + 1 : -mate - best_score) / 2;
	ss << " pv";
	for (auto m : root_moves[0].pv)
		ss << " " << Uci::move_to_str(m);
	std::cout << ss.str() << std::endl;
}
