#include "bitboard.h"

#include <iostream>
#include <random>
#include <sstream>

#include "evaluate.h"
#include "hash.h"
#include "movegen.h"
#include "threads.h"
#include "zobrist.h"

uint64_t forward_bb[Color][sq_all];
uint64_t between_squares[sq_all][sq_all];
uint64_t line_bb[sq_all][sq_all];

inline bool aligned(const int s0, const int s1, const int s2)
{
	return line_bb[s0][s1] & 1LL << s2;
}

check_info::check_info(const bit_board& board)
{
	const auto color = board.stm();
	ksq = board.king_square(!color);

	pinned = board.pinned_pieces(board.stm());
	dc_candidates = board.check_candidates();

	check_sq[pawn] = board.attacks_from<pawn>(ksq, !color);
	check_sq[knight] = board.attacks_from<knight>(ksq);
	check_sq[bishop] = board.attacks_from<bishop>(ksq);
	check_sq[rook] = board.attacks_from<rook>(ksq);
	check_sq[queen] = check_sq[rook] | check_sq[bishop];
	check_sq[king] = 0LL;
}

inline uint64_t attacks_bb(const int pt, const int sq, const uint64_t occ)
{
	switch (pt)
	{
	case bishop:
		return slider_attacks.bishopAttacks(occ, sq);
	case rook:
		return slider_attacks.rookAttacks(occ, sq);
	case queen:
		return slider_attacks.queenAttacks(occ, sq);
	default:
		std::cout << "attacks_bb invalid Input"
			<< std::endl;
		return 0;
	}
}

void bit_board::init_board()
{
	std::memcpy(zobrist::zob_array, zobrist.z_array, sizeof zobrist.z_array);
	std::memcpy(zobrist::en_passant, zobrist.z_en_passant, sizeof zobrist.z_en_passant);
	std::memcpy(zobrist::castling, zobrist.z_castle, sizeof zobrist.z_castle);
	zobrist::color = zobrist.z_black_move;

	for (auto i = 0; i < 64; ++i)
	{
		uint64_t moves;
		const uint64_t p = 1LL << i;
		auto wp = p >> 7;
		wp |= p >> 9;
		auto bp = p << 9;
		bp |= p << 7;

		if (i % 8 < 4)
		{
			wp &= ~file_h;
			bp &= ~file_h;
		}
		else
		{
			wp &= ~file_a;
			bp &= ~file_a;
		}
		pseudo_attacks[pawn - 1][i] = wp;
		pseudo_attacks[pawn][i] = bp;

		if (i > 18)
		{
			moves = knight_span << (i - 18);
		}
		else
		{
			moves = knight_span >> (18 - i);
		}

		if (i % 8 < 4)
		{
			moves &= ~file_gh;
		}
		else
		{
			moves &= ~file_ab;
		}

		pseudo_attacks[knight][i] = moves;
		pseudo_attacks[queen][i] = pseudo_attacks[bishop][i] = slider_attacks.bishopAttacks(0LL, i);
		pseudo_attacks[queen][i] |= pseudo_attacks[rook][i] = slider_attacks.rookAttacks(0LL, i);

		if (i > 9)
		{
			moves = king_span << (i - 9);
		}
		else
		{
			moves = king_span >> (9 - i);
		}

		if (i % 8 < 4)
		{
			moves &= ~file_gh;
		}
		else
		{
			moves &= ~file_ab;
		}

		pseudo_attacks[king][i] = moves;
	}

	for (auto color = 0; color < 2; ++color)
	{
		for (auto sq = 0; sq < 64; ++sq)
		{
			squareBB[sq] = 1LL << sq;
			forward_bb[color][sq] = 1LL << sq;
			PassedPawnMask[color][sq] = 0LL;
			PawnAttackSpan[color][sq] = 0LL;

			if (color == white)
			{
				forward_bb[color][sq] = shift_bb<north>(forward_bb[color][sq]);

				for (auto i = 0; i < 8; ++i)
				{
					forward_bb[color][sq] |= shift_bb<north>(forward_bb[color][sq]);
					PassedPawnMask[color][sq] |= psuedo_attacks(pawn, white, sq) | forward_bb[color][sq];
					PassedPawnMask[color][sq] |= shift_bb<north>(PassedPawnMask[color][sq]);
					PawnAttackSpan[color][sq] |= psuedo_attacks(pawn, white, sq);
					PawnAttackSpan[color][sq] |= shift_bb<north>(PawnAttackSpan[color][sq]);
				}
			}
			else
			{
				forward_bb[color][sq] = shift_bb<south>(forward_bb[color][sq]);

				for (auto i = 0; i < 8; ++i)
				{
					forward_bb[color][sq] |= shift_bb<south>(forward_bb[color][sq]);
					PassedPawnMask[color][sq] |= psuedo_attacks(pawn, black, sq) | forward_bb[color][sq];
					PassedPawnMask[color][sq] |= shift_bb<south>(PassedPawnMask[color][sq]);
					PawnAttackSpan[color][sq] |= psuedo_attacks(pawn, black, sq);
					PawnAttackSpan[color][sq] |= shift_bb<south>(PawnAttackSpan[color][sq]);
				}
			}
		}
	}

	for (auto s1 = 0; s1 < 64; ++s1)
	{
		for (auto s2 = 0; s2 < 64; ++s2)
		{
			between_squares[s1][s2] = 0LL;
			line_bb[s1][s2] = 0LL;

			if (s1 != s2)
			{
				square_distance[s1][s2] = std::max(file_distance(s1, s2), rank_distance(s1, s2));
			}

			const int pc = pseudo_attacks[bishop][s1] & squareBB[s2]
				               ? bishop
				               : pseudo_attacks[rook][s1] & squareBB[s2]
				               ? rook
				               : no_piece;

			if (pc == no_piece)
				continue;

			line_bb[s1][s2] = (attacks_bb(pc, s1, 0LL) & attacks_bb(pc, s2, 0LL)) | squareBB[s1] | squareBB[s2];
			between_squares[s1][s2] = attacks_bb(pc, s1, squareBB[s2]) & attacks_bb(pc, s2, squareBB[s1]);
		}
	}
}

uint64_t bit_board::next_key(const Move m) const
{
	auto k = st_->key;
	const auto color = stm();
	const auto to = to_sq(m);
	const auto captured = piece_on_sq(to);

	k ^= zobrist::zob_array[color][piece_on_sq(from_sq(m))][to];
	k ^= zobrist::color;

	if (captured)
		k ^= zobrist::zob_array[color][captured][to];
	return k;
}

void bit_board::reset_board(const std::string* fen, thread* th, state_info* si)
{
	st_ = si;
	full_squares = 0LL;

	for (auto i = 0; i < 2; ++i)
	{
		all_pieces_color_bb[i] = 0LL;

		for (auto j = 0; j < 7; ++j)
		{
			by_color_pieces_bb[i][j] = 0LL;
			by_piece_type[j] = 0LL;
			piece_count[i][j] = 0;

			for (auto h = 0; h < 16; ++h)
			{
				piece_loc[i][j][h] = sq_none;
				castling_path[h] = 0LL;
			}
		}
	}

	for (auto i = 0; i < 64; i++)
	{
		piece_index[i] = sq_none;
		piece_on[i] = no_piece;
		castling_rights_masks[i] = 0LL;
	}

	if (fen)
		read_fen_string(*fen);
	else
	{
		const std::string startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
		read_fen_string(startpos);
	}

	set_state(st_, th);
	empty_squares = ~full_squares;
}

void bit_board::set_state(state_info* si, thread* th)
{
	si->material_key = 0LL;
	si->pawn_key = 0LL;
	si->captured_piece = 0;
	si->key = zobrist.get_zobrist_hash(*this);
	si->checkers = attackers_to(king_square(stm()), full_squares) & pieces(!stm());

	this_thread_ = th;

	for (auto c = 0; c < Color; ++c)
	{
		b_info.side_material[c] = 0;
		b_info.non_pawn_material[c] = 0;

		for (int pt = pawn; pt < piece; ++pt)
		{
			b_info.side_material[c] += piece_value[pt] * piece_count[c][pt];

			if (pt > pawn)
				b_info.non_pawn_material[c] += piece_value[pt] * piece_count[c][pt];

			if (pt == pawn)
			{
				auto pawn_board = pieces(c, pawn);

				while (pawn_board)
				{
					si->pawn_key ^= zobrist.z_array[c][pawn][pop_lsb(&pawn_board)];
				}
			}

			for (auto count = 0; count <= piece_count[c][pt]; ++count)
			{
				si->material_key ^= zobrist.z_array[c][pt][count];
			}
		}
	}
}

void bit_board::read_fen_string(const std::string& fen)
{
	constexpr int lookup[18] =
	{
		0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 2, 0, 1, 5, 4
	};

	std::istringstream ss(fen);
	char token;
	ss >> std::noskipws;
	int sq = a8;

	while (ss >> token && !isspace(token))
	{
		if (isdigit(token))
			sq += token - '0';

		if (token == '/')
			continue;

		if (isalpha(token))
		{
			if (isupper(token))
			{
				add_piece(lookup[token - 'A'], white, sq);
			}
			else
			{
				add_piece(lookup[static_cast<char>(toupper(token)) - 'A'], black, sq);
			}
			sq++;
		}
	}
	ss >> token;
	b_info.side_to_move = token == 'w' ? white : black;
	ss >> token;

	while (ss >> token && !isspace(token))
	{
		int rook_square;
		const int color = islower(token) ? black : white;
		token = static_cast<char>(toupper(token));

		if (token == 'K')
		{
			rook_square = relative_square(color, h1);
		}
		else if (token == 'Q')
		{
			rook_square = relative_square(color, a1);
		}
		else
			continue;

		set_castling_rights(color, rook_square);
	}

	char col;

	if (char row; ss >> col && (col >= 'a' && col <= 'h') && (ss >> row && (row == '3' || row == '6')))
	{
		st_->ep_square = (col - 'a') * 8 + (row - '1');

		if (!(attackers_to(st_->ep_square, full_squares) & pieces(stm(), pawn))
			|| !(pieces(!stm(), pawn) & square_bb(st_->ep_square + pawn_push(!stm()))))
			st_->ep_square = sq_none;
	}
	else
		st_->ep_square = sq_none;

	ss >> std::skipws >> st_->rule50 >> num_moves;
}

bool bit_board::is_legal(const Move& m, const uint64_t pinned) const
{
	const auto color = stm();
	const auto from = from_sq(m);

	if (move_type(m) == enpassant)
	{
		const auto to = to_sq(m);
		const auto ksq = king_square(color);
		const auto capsq = to - pawn_push(color);
		const auto occ = (full_squares ^ squareBB[from] ^ squareBB[capsq]) | squareBB[to];

		return !(attacks_from<rook>(ksq, occ) & pieces(!color, queen, rook))
			&& !(attacks_from<bishop>(ksq, occ) & pieces(!color, queen, bishop));
	}

	if (piece_on_sq(from) == king)
		return move_type(m) == castle || !(attackers_to(to_sq(m), full_squares) & pieces(!color));

	return !pinned || !(pinned & squareBB[from]) || aligned(from, to_sq(m), king_square(color));
}

bool bit_board::pseudo_legal(const Move m) const
{
	const auto color = stm();
	const auto to = to_sq(m);
	const auto from = from_sq(m);
	const auto piece = piece_on_sq(from);

	if (move_type(m) != normal)
	{
		return move_list<legal>(*this).contains(m);
	}

	if (to == from)
		return false;
	if (piece == no_piece || color_of_pc(from) != color)
		return false;
	if (square_bb(to) & pieces(color))
		return false;
	if (promotion_type(m) - 2 != no_piece)
		return false;

	if (piece == pawn)
	{
		if (rank_of(to) == relative_rank(color, 7))
			return false;

		if (!(attacks_from<pawn>(from, color) & pieces(!color) & squareBB[to])
			&& !(from + pawn_push(color) == to && empty(to))
			&& !(from + 2 * pawn_push(color) == to && rank_of(from) == relative_rank(color, 1) && empty(to)
				&& empty(to - pawn_push(color))))
			return false;
	}

	else if (!((piece == rook || piece == bishop || piece == queen
		            ? attacks_bb(piece, from, full_squares)
		            : psuedo_attacks(piece, color, from)) & squareBB[to]))
		return false;

	if (checkers())
	{
		if (piece != king)
		{
			if (more_than_one(checkers()))
				return false;

			if (!((between_squares[lsb(checkers())][king_square(color)] | checkers()) & squareBB[to]))
				return false;
		}

		else if (attackers_to(to, full_squares ^ squareBB[from]) & pieces(!color))
			return false;
	}
	return true;
}

bool bit_board::gives_check(const Move m, const check_info& ci) const
{
	const auto from = from_sq(m);
	const auto to = to_sq(m);

	if (const auto piece = piece_on_sq(from); ci.check_sq[piece] & squareBB[to])
		return true;

	if (ci.dc_candidates && ci.dc_candidates & squareBB[from] && !aligned(from, to, ci.ksq))
		return true;

	switch (move_type(m))
	{
	case normal:
		return false;
	case castle:
		{
			const auto rfrom = relative_square(stm(), to < from ? a1 : h1);
			const auto rto = relative_square(stm(), to < from ? d1 : f1);

			return pseudo_attacks[rook][rto] & squareBB[ci.ksq] && attacks_from<rook>(rto, (full_squares ^ squareBB[from] ^ squareBB[rfrom]) | squareBB[rto] | squareBB[to]) & squareBB[ci.ksq];
		}
	case enpassant:
		{
			const auto capsq = stm() == white ? to + south : to + north;
			const auto bb = (full_squares ^ squareBB[from] ^ squareBB[capsq]) | squareBB[to];

			return (attacks_from<rook>(ci.ksq, bb) & pieces(stm(), rook, queen))
				| (attacks_from<bishop>(ci.ksq, bb) & pieces(stm(), bishop, queen));
		}
	case promotion:
		return attacks_from<queen>(to, full_squares ^ squareBB[from]);
	default:
		std::cout << "gives check error!!!!!!!!!!!!" << std::endl;
		return false;
	}
}

uint64_t bit_board::check_blockers(const int color, const int king_color) const
{
	uint64_t pinners, result = 0LL;
	const auto king_sq = king_square(king_color);
	pinners = (pieces_by_type(rook, queen) & psuedo_attacks(rook, white, king_sq)
		| pieces_by_type(bishop, queen) & psuedo_attacks(bishop, white, king_sq)) & pieces(!king_color);

	while (pinners)
	{
		if (const auto b = between_squares[king_sq][pop_lsb(&pinners)] & full_squares; !more_than_one(b))
			result |= b & pieces(color);
	}
	return result;
}

void bit_board::set_castling_rights(const int color, const int rfrom)
{
	const auto king = king_square(color);
	const auto cside = king < rfrom ? kingside : queenside;
	const auto cright = static_cast<int>(1LL) << ((cside == queenside) + 2 * color);
	st_->castling_rights |= cright;
	castling_rights_masks[king] |= cright;
	castling_rights_masks[rfrom] |= cright;
	const auto d = square_distance[rfrom][king] - 2;
	castling_path[cright] |= 1LL << (king + (cside == kingside ? 1 : -1));

	for (auto i = 0; i < d; ++i)
	{
		castling_path[cright] |= cside == kingside ? castling_path[cright] << 1LL : castling_path[cright] >> 1LL;
	}
}

void bit_board::make_move(const Move& m, state_info& new_st, const int color)
{
	this_thread_->nodes.fetch_add(1, std::memory_order_relaxed);
	const int them = !color;
	const auto to = to_sq(m);
	const auto from = from_sq(m);
	const auto piece = piece_on_sq(from);
	const auto captured = move_type(m) == enpassant ? pawn : piece_on_sq(to);

	const check_info ci(*this);
	const auto checking = gives_check(m, ci);

	std::memcpy(&new_st, st_, sizeof(state_info));
	new_st.previous = st_;
	st_ = &new_st;

	++st_->rule50;
	++st_->plies_from_null;

	if (captured)
	{
		auto cap_sq = to;

		if (captured == pawn)
		{
			if (move_type(m) == enpassant)
			{
				cap_sq += pawn_push(them);
			}
			st_->pawn_key ^= zobrist::zob_array[them][pawn][cap_sq];
		}
		else
		{
			b_info.non_pawn_material[them] -= piece_value[captured];
		}
		remove_piece(captured, them, cap_sq);
		b_info.side_material[them] -= piece_value[captured];
		st_->key ^= zobrist::zob_array[them][captured][cap_sq];
		st_->material_key ^= zobrist::zob_array[them][captured][piece_count[them][captured] + 1];
		st_->rule50 = 0;
	}

	move_piece(piece, color, from, to);

	st_->captured_piece = captured;

	if (st_->ep_square != sq_none)
	{
		st_->key ^= zobrist::en_passant[file_of(st_->ep_square)];
		st_->ep_square = sq_none;
	}

	if (piece == pawn)
	{
		if ((to ^ from) == 16 && psuedo_attacks(pawn, color, from + pawn_push(color)) & pieces(them, pawn))
		{
			st_->ep_square = (from + to) / 2;
			st_->key ^= zobrist::en_passant[file_of(st_->ep_square)];
		}

		if (move_type(m) == promotion)
		{
			remove_piece(pawn, color, to);
			const auto prom_t = promotion_type(m);

			add_piece(prom_t, color, to);
			b_info.side_material[color] += piece_value[prom_t] - piece_value[pawn];
			b_info.non_pawn_material[color] += piece_value[prom_t];

			st_->key ^= zobrist::zob_array[color][prom_t][to] ^ zobrist::zob_array[color][pawn][to];
			st_->pawn_key ^= zobrist::zob_array[color][pawn][to];
			st_->material_key ^= zobrist::zob_array[color][prom_t][piece_count[color][prom_t]]
				^ zobrist::zob_array[color][pawn][piece_count[color][pawn] + 1];

			st_->rule50;
		}

		st_->pawn_key ^= zobrist.z_array[color][pawn][to] ^ zobrist.z_array[color][pawn][from];
	}
	else if (move_type(m) == castle)
	{
		do_castling<true>(from, to, color);
	}

	if (st_->castling_rights && castling_rights_masks[from] | castling_rights_masks[to])
	{
		const auto cr = castling_rights_masks[from] | castling_rights_masks[to];
		st_->key ^= zobrist::castling[st_->castling_rights];
		st_->castling_rights &= ~cr;
		st_->key ^= zobrist::castling[st_->castling_rights];
	}

	st_->key ^= zobrist::zob_array[color][piece][from] ^ zobrist::zob_array[color][piece][to] ^ zobrist::color;
	prefetch(tt.first_entry(st_->key));
	st_->checkers = 0LL;

	if (checking)
	{
		if (move_type(m) != normal)
			st_->checkers = attackers_to(king_square(them), full_squares) & pieces(color);
		else
		{
			if (ci.check_sq[piece] & squareBB[to])
				st_->checkers |= squareBB[to];

			if (ci.dc_candidates && ci.dc_candidates & squareBB[from])
			{
				if (piece != rook)
					st_->checkers |= attacks_from<rook>(king_square(them)) & pieces(color, rook, queen);

				if (piece != bishop)
					st_->checkers |= attacks_from<bishop>(king_square(them)) & pieces(color, bishop, queen);
			}
		}
	}

	b_info.side_to_move = !b_info.side_to_move;
}

void bit_board::unmake_move(const Move& m, const int color)
{
	const int them = !color;
	const auto to = to_sq(m);
	const auto from = from_sq(m);
	const int type = move_type(m);
	const auto piece = piece_on_sq(to);

	if (type != promotion)
	{
		move_piece(piece, color, to, from);
	}
	else if (type == promotion)
	{
		const auto prom_t = promotion_type(m);
		add_piece(pawn, color, from);
		remove_piece(prom_t, color, to);
		b_info.side_material[color] += piece_value[pawn] - piece_value[prom_t];
		b_info.non_pawn_material[color] -= piece_value[prom_t];
	}

	if (st_->captured_piece)
	{
		auto cap_sq = to;
		const auto captured = st_->captured_piece;

		if (type == enpassant)
		{
			cap_sq += pawn_push(them);
		}

		add_piece(captured, them, cap_sq);
		b_info.side_material[them] += piece_value[captured];

		if (captured != pawn)
			b_info.non_pawn_material[them] += piece_value[captured];
	}

	if (type == castle)
	{
		do_castling<false>(from, to, color);
	}

	st_ = st_->previous;

	b_info.side_to_move = !b_info.side_to_move;
}

template <int Make>
void bit_board::do_castling(const int from, const int to, const int color)
{
	const auto king_side = to > from;
	const auto r_from = relative_square(color, king_side ? h1 : a1);
	const auto r_to = relative_square(color, king_side ? f1 : d1);
	move_piece(rook, color, Make ? r_from : r_to, Make ? r_to : r_from);
	if (Make)
		st_->key ^= zobrist::zob_array[color][rook][r_from] ^ zobrist::zob_array[color][rook][r_to];
}

void bit_board::make_null_move(state_info& new_st)
{
	std::memcpy(&new_st, st_, sizeof(state_info));
	new_st.previous = st_;
	st_ = &new_st;
	++st_->rule50;
	st_->plies_from_null = 0;
	st_->key ^= zobrist::color;

	if (st_->ep_square != sq_none)
	{
		st_->key ^= zobrist::en_passant[file_of(st_->ep_square)];
		st_->ep_square = sq_none;
	}

	b_info.side_to_move = !b_info.side_to_move;
	_mm_prefetch(reinterpret_cast<char*>(tt.first_entry(st_->key)), _MM_HINT_NTA);
}

void bit_board::undo_null_move()
{
	st_ = st_->previous;
	b_info.side_to_move = !b_info.side_to_move;
}

bool bit_board::is_draw(const int ply) const
{
	if (st_->rule50 > 99 && (!checkers() || move_list<legal>(*this).size()))
		return true;

	const auto end = std::min(st_->rule50, st_->plies_from_null);

	if (end < 4)
		return false;

	const auto* stp = st_->previous->previous;
	auto count = 0;

	for (auto i = 4; i <= end; i += 2)
	{
		stp = stp->previous->previous;

		if (stp->key == st_->key && ++count + (ply > i) == 2)
			return true;
	}

	return false;
}

int bit_board::SEE(const Move& m, int color, const bool is_capture) const
{
	int swap_list[32], index = 1;
	const auto from = from_sq(m);
	const auto to = to_sq(m);
	const auto piece = piece_on_sq(from);
	auto captured = piece_on_sq(to);
	const int type = move_type(m);

	if (piece_value[piece] <= piece_value[captured] && is_capture)
		return inf;

	if (type == castle)
		return 0;

	swap_list[0] = piece_value[captured];
	auto occupied = full_squares ^ square_bb(from);

	if (type == enpassant)
	{
		occupied ^= square_bb(to - pawn_push(!color));
		swap_list[0] = piece_value[pawn];
	}

	auto attackers = attackers_to(to, occupied) & occupied;
	color = !color;
	auto stm_attackers = attackers & all_pieces_color_bb[color];

	if (!stm_attackers)
		return swap_list[0];

	captured = piece;

	do
	{
		swap_list[index] = -swap_list[index - 1] + piece_value[captured];
		captured = min_attacker<pawn>(color, to, stm_attackers, occupied, attackers);

		if (captured == king)
		{
			if (stm_attackers == attackers)
			{
				index++;
			}
			break;
		}

		color = !color;
		stm_attackers = attackers & all_pieces_color_bb[color];

		index++;
	}
	while (stm_attackers);

	while (--index)
	{
		swap_list[index - 1] = std::min(-swap_list[index], swap_list[index - 1]);
	}

	return swap_list[0];
}

bool bit_board::see_ge(const Move m, const int threshold) const
{
	if (move_type(m) != normal)
		return 0 >= threshold;

	const auto to = to_sq(m);
	const auto from = from_sq(m);
	auto victim = piece_on_sq(from);
	int stm = !color_of_pc(from);
	auto balance = piece_value[piece_on_sq(to)];

	if (balance < threshold)
		return false;

	balance -= piece_value[victim];

	if (balance >= threshold)
		return true;

	auto relative_stm = true;
	auto occupied = full_squares ^ squareBB[from] ^ squareBB[to];
	auto attackers = attackers_to(to, occupied);

	while (true)
	{
		auto stm_attackers = attackers & pieces(stm);

		if (!stm_attackers)
			return relative_stm;

		victim = min_attacker<pawn>(stm, to, stm_attackers, occupied, attackers);

		if (victim == king)
			return relative_stm = static_cast<bool>(attackers & pieces(!stm));

		balance += relative_stm ? piece_value[victim] : -piece_value[victim];
		relative_stm = !relative_stm;

		if (relative_stm == balance >= threshold)
			return relative_stm;

		stm = !stm;
	}
}

int bit_board::see_sign(const Move m, const int color, const bool is_capture) const
{
	if (piece_value[moved_piece(m)] <= piece_value[piece_on_sq(to_sq(m))])
		return known_win;

	return SEE(m, color, is_capture);
}

template <int Pt>
int bit_board::min_attacker(const int color, const int to, const uint64_t& stm_attackers, uint64_t& occupied,
                           uint64_t& attackers) const
{
	const auto bb = stm_attackers & pieces(color, Pt);

	if (!bb)
		return min_attacker<Pt + 1>(color, to, stm_attackers, occupied, attackers);

	occupied ^= bb & ~(bb - 1);

	if (Pt == pawn || Pt == bishop || Pt == queen)
	{
		attackers |= slider_attacks.bishopAttacks(occupied, to) & pieces(color, bishop, queen);
	}

	if (Pt == rook || Pt == queen)
	{
		attackers |= slider_attacks.rookAttacks(occupied, to) & pieces(color, rook, queen);
	}

	attackers &= occupied;
	return Pt;
}

template <>
inline int bit_board::min_attacker<king>(int, int, const uint64_t&, uint64_t&, uint64_t&) const
{
	return king;
}

bool bit_board::is_square_attacked(const int square, const int color) const
{
	const int them = !color;
	auto attacks = psuedo_attacks(pawn, color, square);
	if (attacks & pieces(them, pawn))
		return true;

	attacks = psuedo_attacks(knight, them, square);
	if (attacks & pieces(them, knight))
		return true;

	attacks = slider_attacks.bishopAttacks(full_squares, square);
	if (attacks & pieces(them, bishop, queen))
		return true;

	attacks = slider_attacks.rookAttacks(full_squares, square);
	if (attacks & pieces(them, rook, queen))
		return true;

	attacks = psuedo_attacks(king, them, square);
	if (attacks & pieces(them, king))
		return true;

	return false;
}

uint64_t bit_board::attackers_to(const int square, const uint64_t occupied) const
{
	return (attacks_from<pawn>(square, white) & pieces(black, pawn))
		| (attacks_from<pawn>(square, black) & pieces(white, pawn))
		| (attacks_from<knight>(square) & pieces_by_type(knight))
		| (slider_attacks.bishopAttacks(occupied, square) & pieces_by_type(bishop, queen))
		| (slider_attacks.rookAttacks(occupied, square) & pieces_by_type(rook, queen))
		| (attacks_from<king>(square) & pieces_by_type(king));
}
