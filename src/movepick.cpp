#include "movepick.h"

#include "bitboard.h"
#include "material.h"
#include "movegen.h"

void insertion_sort(s_move* begin, const s_move* end)
{
	s_move* q;
	for (auto* p = begin + 1; p < end; ++p)
	{
		auto tmp = *p;
		for (q = p; q != begin && *(q - 1) < tmp; --q)
			*q = *(q - 1);
		*q = tmp;
	}
}

void partial_insertion_sort(s_move* begin, const s_move* end, const int limit)
{
	for (auto sorted_end = begin, p = begin + 1; p < end; ++p)
		if (p->score >= limit)
		{
			s_move tmp = *p, *q;
			*p = *++sorted_end;
			for (q = sorted_end; q != begin && *(q - 1) < tmp; --q)
				*q = *(q - 1);
			*q = tmp;
		}
}

inline bool has_positive_value(const s_move& ms)
{
	return ms.score > 0;
}

inline s_move* pick(s_move* begin, s_move* end)
{
	std::swap(*begin, *std::max_element(begin, end));
	return begin;
}

move_picker::move_picker(const bit_board& board, const Move ttm, const int depth, const move_history* hist, const piece_history** ch,
	const Move cm, Move* killers_p)
	: b_(board), move_hist_(hist), piece_sq_history_(ch), depth_(depth), counter_move_(cm), killers_{{killers_p[0], killers_p[1]}}
{
	stage_ = b_.checkers() ? evasion : main_search;
	tt_move_ = ttm && b_.pseudo_legal(ttm) ? ttm : move_none;
	stage_ += tt_move_ == move_none;
}

move_picker::move_picker(const bit_board& board, Move ttm, const move_history* hist) : b_(board), move_hist_(hist), counter_move_()
{
	stage_ = q_search;
	if (ttm && !b_.capture_or_promotion(ttm))
		ttm = move_none;
	tt_move_ = ttm && b_.pseudo_legal(ttm) ? ttm : move_none;
	stage_ += tt_move_ == move_none;
}

template <>
void move_picker::score<captures>()
{
	for (auto* i = m_list_; i != end_; ++i)
	{
		const auto m = i->move;
		i->score = piece_value[b_.piece_on_sq(to_sq(m))] - piece_value[b_.piece_on_sq(from_sq(m))];
		if (move_type(m) == enpassant)
			i->score += piece_value[pawn];
		else if (move_type(m) == promotion)
			i->score += piece_value[promotion_type(m)] - piece_value[pawn];
	}
}

template <>
void move_picker::score<quiets>()
{
	for (auto* i = m_list_; i != end_; ++i)
	{
		const auto m = i->move;
		i->score = (*move_hist_)[b_.stm()][from_to(m)] + (*piece_sq_history_[0])[b_.piece_on_sq(from_sq(m))][to_sq(m)]
			+ (*piece_sq_history_[1])[b_.piece_on_sq(from_sq(m))][to_sq(m)]
			+ (*piece_sq_history_[3])[b_.piece_on_sq(from_sq(m))][to_sq(m)];
	}
}

template <>
void move_picker::score<evasions>()
{
	int see_val;
	const auto color = b_.stm();
	static constexpr auto max = 1 << 28;

	for (auto* i = m_list_; i != end_; ++i)
	{
		const auto m = i->move;

		if (const auto capture = b_.capture(m); (see_val = b_.see_sign(m, color, capture)) < 0)
			i->score = see_val - max;
		else if (b_.capture(m))
			i->score = piece_value[b_.piece_on_sq(to_sq(m))] - piece_value[b_.piece_on_sq(from_sq(m))];
		else
			i->score = (*move_hist_)[b_.stm()][from_to(m)];
	}
}

Move move_picker::next_move()
{
	Move m;
	switch (stage_)
	{
	case main_search:
	case q_search:
	case evasion:
		++stage_;
		return tt_move_;
	case captures_init:
		end_bad_captures_ = current_ = m_list_;
		end_ = generate<captures>(b_, current_);
		score<captures>();
		stage_++;
	case good_captures:
		while (current_ < end_)
		{
			m = pick(current_++, end_)->move;
			if (m != tt_move_)
			{
				if (b_.see_sign(m, b_.stm(), true) > 0)
				{
					return m;
				}
				end_bad_captures_++->move = m;
			}
		}
		++stage_;
		m = killers_[0].move;
		if (m != move_none && m != tt_move_ && !b_.capture(m) && b_.pseudo_legal(m))
			return m;
	case killers:
		++stage_;
		m = killers_[1].move;
		if (m != move_none && m != tt_move_ && !b_.capture(m) && b_.pseudo_legal(m))
			return m;
	case counter_move:
		++stage_;
		m = counter_move_;
		if (m != move_none && m != tt_move_ && m != killers_[0].move && m != killers_[1].move && b_.pseudo_legal(m)
			&& !b_.capture(m))
			return m;
	case quiets_init:
		current_ = end_bad_captures_;
		end_ = generate<quiets>(b_, current_);
		score<quiets>();
		partial_insertion_sort(current_, end_, -2200 * depth_);
		++stage_;
	case quiet:
		while (current_ < end_)
		{
			m = current_++->move;
			if (m != tt_move_ && m != killers_[0].move && m != killers_[1].move && m != counter_move_)
				return m;
		}
		++stage_;
		current_ = m_list_;
	case bad_captures:
		if (current_ < end_bad_captures_)
			return current_++->move;
		break;
	case captures_q_init:
		current_ = m_list_;
		end_ = generate<captures>(b_, current_);
		score<captures>();
		++stage_;
	case captures_q:
		while (current_ < end_)
		{
			m = pick(current_++, end_)->move;
			if (m != tt_move_)
				return m;
		}
		break;
	case evasions_init:
		current_ = m_list_;
		end_ = generate<evasions>(b_, m_list_);
		score<evasions>();
		++stage_;
	case evasions_s1:
		while (current_ < end_)
		{
			m = pick(current_++, end_)->move;
			return m;
		}
		break;
	default: ;
	}
	return move_none;
}
