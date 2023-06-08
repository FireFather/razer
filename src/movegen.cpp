#include "movegen.h"

#include "bitboard.h"
#include "material.h"
#include "pawns.h"

template <int Shift, int GenType>
s_move* generate_promotions(const uint64_t pawns, s_move* m_list, uint64_t target)
{
	uint64_t moves = shift_bb<Shift>(pawns) & target;

	while (moves)
	{
		const auto index = pop_lsb(&moves);

		if (GenType == captures || GenType == main_gen || GenType == evasions)
			m_list++->move = create_special<promotion, queen>(index - Shift, index);

		if (GenType == quiets || GenType == main_gen || GenType == evasions)
		{
			m_list++->move = create_special<promotion, rook>(index - Shift, index);
			m_list++->move = create_special<promotion, bishop>(index - Shift, index);
			m_list++->move = create_special<promotion, knight>(index - Shift, index);
		}
	}

	return m_list;
}

template <int Color, int GenType>
s_move* pawn_moves(const bit_board& board, s_move* m_list, const uint64_t target)
{
	const int them = Color == white ? black : white;
	const int up = Color == white ? north : south;
	const int down = Color == white ? south : north;
	const int right = Color == white ? northeast : southeast;
	const int left = Color == white ? northwest : southwest;
	const auto dpush = Color == white ? 16 : -16;
	const auto third_rank = Color == white ? rank3 : rank6;
	const auto seventh_rank = Color == white ? rank7 : rank2;
	const auto eighth_rank = Color == white ? rank8 : rank1;
	const auto pawns = board.pieces(Color, pawn) & ~seventh_rank;
	const auto candidate_pawns = board.pieces(Color, pawn) & seventh_rank;
	const auto enemys = GenType == evasions
		                    ? (board.pieces(them) ^ board.pieces(them, king)) & target
		                    : board.pieces(them) ^ board.pieces(them, king);

	uint64_t moves, moves1;

	if (GenType != captures)
	{
		moves = shift_bb<up>(pawns) & board.empty_squares;
		moves1 = shift_bb<up>(moves & third_rank) & board.empty_squares;

		if (GenType == evasions)
		{
			moves &= target;
			moves1 &= target;
		}

		while (moves)
		{
			const auto index = pop_lsb(&moves);

			m_list++->move = create_move(index + down, index);
		}

		while (moves1)
		{
			const auto index = pop_lsb(&moves1);

			m_list++->move = create_move(index + dpush, index);
		}
	}

	if (candidate_pawns && (GenType != evasions || target & eighth_rank))
	{
		auto target_p = board.empty_squares;

		if (GenType == evasions)
			target_p &= target;

		m_list = generate_promotions<up, GenType>(candidate_pawns, m_list, target_p);
		m_list = generate_promotions<right, GenType>(candidate_pawns, m_list, enemys);
		m_list = generate_promotions<left, GenType>(candidate_pawns, m_list, enemys);
	}

	if (GenType == captures || GenType == evasions || GenType == main_gen)
	{
		moves = shift_bb<right>(pawns) & enemys;
		while (moves)
		{
			const auto index = pop_lsb(&moves);
			m_list++->move = create_move(index - right, index);
		}

		moves = shift_bb<left>(pawns) & enemys;
		while (moves)
		{
			const auto index = pop_lsb(&moves);
			m_list++->move = create_move(index - left, index);
		}

		if (board.can_enpassant())
		{
			if (GenType == evasions && !(target & board.square_bb(board.ep_square() - up)))
				return m_list;

			const auto ep_sq = board.ep_square();
			auto ep_pawns = board.psuedo_attacks(pawn, them, ep_sq) & pawns;

			while (ep_pawns)
			{
				const auto from = pop_lsb(&ep_pawns);
				m_list++->move = create_special<enpassant, no_piece>(from, ep_sq);
			}
		}
	}
	return m_list;
}

template <int Color, int Cs>
s_move* castling(const bit_board& board, s_move* m_list)
{
	const int castling_rights = Cs == kingside
		                           ? Color == white
			                             ? white_oo
			                             : black_oo
		                           : Color == white
		                           ? white_ooo
		                           : black_ooo;

	if (!(castling_rights & board.castling_rights()))
		return m_list;

	if (board.castling_impeded(castling_rights))
		return m_list;

	const auto ks = board.king_square(Color);
	const auto kto = relative_square(Color, Cs == kingside ? g1 : c1);
	const int c_delta = Cs == kingside ? east : west;
	const auto enemies = board.pieces(!Color);

	for (auto i = ks; i != kto + c_delta; i += c_delta)
		if (board.attackers_to(i, board.full_squares) & enemies)
			return m_list;

	m_list++->move = create_special<castle, no_piece>(ks, kto);
	return m_list;
}

template <int Color, int Pt>
s_move* generate_moves(const bit_board& board, s_move* m_list, const uint64_t& target)
{
	const auto* piece_list = board.piece_loc[Color][Pt];
	int square;

	while ((square = *piece_list++) != sq_none)
	{
		auto moves = board.attacks_from<Pt>(square) & target;
		while (moves)
		{
			m_list++->move = create_move(square, pop_lsb(&moves));
		}
	}

	return m_list;
}

template <int Color, int GenType>
s_move* generate_all(const bit_board& board, s_move* m_list, const uint64_t& target)
{
	m_list = pawn_moves<Color, GenType>(board, m_list, target);
	m_list = generate_moves<Color, knight>(board, m_list, target);
	m_list = generate_moves<Color, bishop>(board, m_list, target);
	m_list = generate_moves<Color, rook>(board, m_list, target);
	m_list = generate_moves<Color, queen>(board, m_list, target);

	if (GenType != evasions)
		m_list = generate_moves<Color, king>(board, m_list, target);

	if (GenType != captures && GenType != evasions && board.can_castle(Color))
	{
		m_list = castling<Color, kingside>(board, m_list);
		m_list = castling<Color, queenside>(board, m_list);
	}

	return m_list;
}

template <int GenType>
s_move* generate(const bit_board& board, s_move* m_list)
{
	const auto color = board.stm();

	const auto target = GenType == captures
		                    ? board.pieces(!color) ^ board.pieces(!color, king)
		                    : GenType == main_gen
		                    ? ~(board.pieces(color) | board.pieces(!color, king))
		                    : GenType == quiets
		                    ? board.empty_squares
		                    : 0;

	return color == white
		       ? generate_all<white, GenType>(board, m_list, target)
		       : generate_all<black, GenType>(board, m_list, target);
}

template s_move* generate<main_gen>(const bit_board& board, s_move* m_list);
template s_move* generate<captures>(const bit_board& board, s_move* m_list);
template s_move* generate<quiets>(const bit_board& board, s_move* m_list);

template <>
s_move* generate<evasions>(const bit_board& board, s_move* m_list)
{
	const auto color = board.stm();
	const auto ksq = board.king_square(color);
	uint64_t slider_att = 0LL;

	auto sliders = board.checkers() & ~board.pieces_by_type(pawn, knight);
	while (sliders)
	{
		const auto check_sq = pop_lsb(&sliders);
		slider_att |= line_bb[check_sq][ksq] ^ board.square_bb(check_sq);
	}

	auto bb = board.attacks_from<king>(ksq) & ~board.pieces(color) & ~slider_att;
	while (bb)
		m_list++->move = create_move(ksq, pop_lsb(&bb));

	if (more_than_one(board.checkers()))
		return m_list;

	const auto check_sq = lsb(board.checkers());
	const auto target = between_squares[check_sq][ksq] | ((board.square_bb(check_sq) & ~(board.pieces(color))) | board.
		pieces(!color, king));

	return color == white
		       ? generate_all<white, evasions>(board, m_list, target)
		       : generate_all<black, evasions>(board, m_list, target);
}

template <>
s_move* generate<legal>(const bit_board& board, s_move* m_list)
{
	auto* current = m_list;
	const auto pinned = board.pinned_pieces(board.stm());
	const auto ksq = board.king_square(board.stm());
	auto* end = board.checkers() ? generate<evasions>(board, m_list) : generate<main_gen>(board, m_list);

	while (current != end)
	{
		if ((pinned || from_sq(current->move) == ksq || move_type(current->move) == enpassant)
			&& !board.is_legal(current->move, pinned))
			current->move = (--end)->move;
		else
			++current;
	}

	return end;
}

template <>
s_move* generate<perft_testing>(const bit_board& board, s_move* m_list)
{
	auto* current = m_list;
	const auto pinned = board.pinned_pieces(board.stm());
	const auto ksq = board.king_square(board.stm());
	auto* end = board.checkers() ? generate<evasions>(board, m_list) : generate<main_gen>(board, m_list);

	while (current != end)
	{
		if ((pinned || from_sq(current->move) == ksq || move_type(current->move) == enpassant)
			&& !board.is_legal(current->move, pinned))
			current->move = (--end)->move;
		else
			++current;
	}

	return end;
}
