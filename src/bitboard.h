#pragma once
#include <string>
#include <algorithm>
#include <memory>
#include <deque>
#include "zobrist.h"
#include "attacks.h"

extern attacks slider_attacks;

class zobrist_hash;
class move_gen;
class thread;

struct board_info
{
	int side_material[Color];
	int non_pawn_material[Color];
	int side_to_move;
};

struct check_info
{
	explicit check_info(const bit_board& board);

	uint64_t dc_candidates;
	uint64_t pinned;
	uint64_t check_sq[piece]{};
	int ksq;
};

class state_info
{
public:
	uint64_t key;
	uint64_t pawn_key;
	uint64_t material_key;

	int captured_piece;
	int ep_square;
	int castling_rights;
	int rule50;
	int plies_from_null;
	uint64_t checkers;
	state_info* previous;
};

typedef std::unique_ptr<std::deque<state_info>> state_list;

class bit_board
{
public:
	void init_board();

	zobrist_hash zobrist{};
	[[nodiscard]] uint64_t next_key(Move m) const;

	void reset_board(const std::string* fen, thread* th, state_info* si);
	void set_state(state_info* si, thread* th);
	void read_fen_string(const std::string& fen);

	[[nodiscard]] bool is_legal(const Move& m, uint64_t pinned) const;
	[[nodiscard]] bool pseudo_legal(Move m) const;
	[[nodiscard]] bool is_square_attacked(int square, int color) const;

	void make_move(const Move& m, state_info& new_st, int color);
	void unmake_move(const Move& m, int color);
	void make_null_move(state_info& new_st);
	void undo_null_move();

	[[nodiscard]] bool is_draw(int ply) const;
	board_info b_info{};

	[[nodiscard]] int SEE(const Move& m, int color, bool is_capture) const;
	[[nodiscard]] bool see_ge(Move m, int threshold = 0) const;
	[[nodiscard]] int see_sign(Move m, int color, bool is_capture) const;
	template <int Pt>
	int min_attacker(int color, int to, const uint64_t& stm_attackers, uint64_t& occupied, uint64_t& attackers) const;

	uint64_t full_squares{};
	uint64_t empty_squares{};

	uint64_t by_color_pieces_bb[Color][piece]{};
	uint64_t all_pieces_color_bb[Color]{};
	uint64_t by_piece_type[piece]{};
	uint64_t squareBB[sq_all]{};

	int piece_loc[Color][piece][16]{};
	int piece_index[sq_all]{};
	int piece_count[Color][piece]{};
	int piece_on[sq_all]{};

	[[nodiscard]] uint64_t pieces(int color) const;
	[[nodiscard]] uint64_t pieces(int color, int pt) const;
	[[nodiscard]] uint64_t pieces(int color, int pt, int pt1) const;
	[[nodiscard]] uint64_t pieces_by_type(int p1) const;
	[[nodiscard]] uint64_t pieces_by_type(int p1, int p2) const;

	template <int Pt>
	[[nodiscard]] int count(int color) const;

	[[nodiscard]] bool empty(int sq) const;
	[[nodiscard]] int piece_on_sq(int sq) const;
	[[nodiscard]] int color_of_pc(int sq) const;

	[[nodiscard]] bool capture(Move m) const;
	[[nodiscard]] bool capture_or_promotion(Move m) const;
	[[nodiscard]] int captured_piece() const;
	[[nodiscard]] int moved_piece(Move m) const;

	[[nodiscard]] int king_square(int color) const;
	[[nodiscard]] bool is_pawn_push(Move m, int color) const;
	[[nodiscard]] bool is_made_pawn_push(Move m, int color) const;
	[[nodiscard]] bool pawn_on7_th(int color) const;
	[[nodiscard]] int non_pawn_material(int color) const;
	[[nodiscard]] int non_pawn_material() const;
	[[nodiscard]] uint64_t square_bb(int sq) const;

	[[nodiscard]] int stm() const;

	[[nodiscard]] uint64_t tt_key() const;
	[[nodiscard]] uint64_t pawn_key() const;
	[[nodiscard]] uint64_t material_key() const;

	[[nodiscard]] int game_phase() const;

	template <int Pt>
	[[nodiscard]] uint64_t attacks_from(int from) const;
	template <int Pt>
	[[nodiscard]] uint64_t attacks_from(int from, int color) const;

	template <int Pt>
	[[nodiscard]] static uint64_t attacks_from(int sq, uint64_t occ);
	[[nodiscard]] uint64_t attackers_to(int square, uint64_t occupied) const;
	uint64_t pseudo_attacks[piece][sq_all]{};
	[[nodiscard]] uint64_t psuedo_attacks(int piece, int color, int sq) const;
	[[nodiscard]] uint64_t checkers() const;
	[[nodiscard]] uint64_t check_candidates() const;
	[[nodiscard]] uint64_t pinned_pieces(int color) const;
	[[nodiscard]] bool gives_check(Move m, const check_info& ci) const;
	[[nodiscard]] uint64_t check_blockers(int color, int king_color) const;

	void set_castling_rights(int color, int rfrom);
	int castling_rights_masks[sq_all]{};
	uint64_t castling_path[castling_rights]{};
	[[nodiscard]] int castling_rights() const;
	[[nodiscard]] int can_castle(int color) const;
	[[nodiscard]] bool castling_impeded(int castling_rights) const;

	[[nodiscard]] bool can_enpassant() const;
	[[nodiscard]] int ep_square() const;

	[[nodiscard]] thread* this_thread() const;

private:
	state_info* st_ = nullptr;
	thread* this_thread_ = nullptr;

	template <int Make>
	void do_castling(int from, int to, int color);

	void move_piece(int piece, int color, int from, int to);
	void add_piece(int piece, int color, int sq);
	void remove_piece(int piece, int color, int sq);
};

inline uint64_t bit_board::pieces(const int color) const
{
	return all_pieces_color_bb[color];
}

inline uint64_t bit_board::pieces(const int color, const int pt) const
{
	return by_color_pieces_bb[color][pt];
}

inline uint64_t bit_board::pieces(const int color, const int pt, const int pt1) const
{
	return by_color_pieces_bb[color][pt] | by_color_pieces_bb[color][pt1];
}

inline uint64_t bit_board::pieces_by_type(const int p1) const
{
	return by_piece_type[p1];
}

inline uint64_t bit_board::pieces_by_type(const int p1, const int p2) const
{
	return by_piece_type[p1] | by_piece_type[p2];
}

inline int bit_board::color_of_pc(const int sq) const
{
	return empty(sq) ? 3 : !(squareBB[sq] & all_pieces_color_bb[white]);
}

inline int bit_board::piece_on_sq(const int sq) const
{
	return piece_on[sq];
}

inline bool bit_board::empty(const int sq) const
{
	return piece_on[sq] == no_piece;
}

inline bool bit_board::capture(const Move m) const
{
	return !empty(to_sq(m)) || move_type(m) == enpassant;
}

inline bool bit_board::capture_or_promotion(const Move m) const
{
	return move_type(m) != normal ? move_type(m) != castle : !empty(to_sq(m));
}

inline int bit_board::captured_piece() const
{
	return st_->captured_piece;
}

inline int bit_board::moved_piece(const Move m) const
{
	return piece_on_sq(from_sq(m));
}

inline int bit_board::stm() const
{
	return b_info.side_to_move;
}

inline int bit_board::castling_rights() const
{
	return st_->castling_rights;
}

inline int bit_board::can_castle(const int color) const
{
	return st_->castling_rights & (white_oo | white_ooo) << 2 * color;
}

inline bool bit_board::castling_impeded(const int castling_rights) const
{
	return full_squares & castling_path[castling_rights];
}

inline int bit_board::game_phase() const
{
	auto npm = b_info.non_pawn_material[white] + b_info.non_pawn_material[black];
	npm = std::max(endgame_limit, std::min(npm, midgame_limit));
	return (npm - endgame_limit) * 64 / (midgame_limit - endgame_limit);
}

inline uint64_t bit_board::tt_key() const
{
	return st_->key;
}

inline uint64_t bit_board::pawn_key() const
{
	return st_->pawn_key;
}

inline uint64_t bit_board::material_key() const
{
	return st_->material_key;
}

inline thread* bit_board::this_thread() const
{
	return this_thread_;
}

inline int bit_board::king_square(const int color) const
{
	return piece_loc[color][king][0];
}

template <int Pt>
uint64_t bit_board::attacks_from(const int from) const
{
	return Pt
	       == knight
		       ? psuedo_attacks(knight, white, from)
		       : Pt == bishop
		       ? slider_attacks.bishopAttacks(full_squares, from)
		       : Pt == rook
		       ? slider_attacks.rookAttacks(full_squares, from)
		       : Pt == queen
		       ? slider_attacks.queenAttacks(full_squares, from)
		       : Pt == king
		       ? psuedo_attacks(king, white, from)
		       : 0LL;
}

template <>
inline uint64_t bit_board::attacks_from<pawn>(const int from, const int color) const
{
	return psuedo_attacks(pawn, color, from);
}

template <int Pt>
[[nodiscard]] uint64_t bit_board::attacks_from(const int sq, const uint64_t occ)
{
	return Pt == bishop
		       ? slider_attacks.bishopAttacks(occ, sq)
		       : Pt == rook
		       ? slider_attacks.rookAttacks(occ, sq)
		       : slider_attacks.queenAttacks(occ, sq);
}

inline uint64_t bit_board::psuedo_attacks(const int piece, const int color, const int sq) const
{
	return piece
	       == pawn
		       ? color == white
			         ? pseudo_attacks[pawn - 1][sq]
			         : pseudo_attacks[pawn][sq]
		       : pseudo_attacks[piece][sq];
}

inline uint64_t bit_board::checkers() const
{
	return st_->checkers;
}

inline uint64_t bit_board::check_candidates() const
{
	return check_blockers(stm(), !stm());
}

inline uint64_t bit_board::pinned_pieces(const int color) const
{
	return check_blockers(color, color);
}

inline bool bit_board::is_pawn_push(const Move m, const int color) const
{
	return piece_on_sq(from_sq(m)) == pawn && relative_rank_sq(color, from_sq(m)) > 4;
}

inline bool bit_board::is_made_pawn_push(const Move m, const int color) const
{
	return piece_on_sq(to_sq(m)) == pawn && relative_rank_sq(color, from_sq(m)) > 4;
}

inline bool bit_board::pawn_on7_th(const int color) const
{
	return by_color_pieces_bb[color][pawn] & (color ? 0xFF000000000000L : 0xFF00L);
}

inline int bit_board::non_pawn_material(const int color) const
{
	return b_info.non_pawn_material[color];
}

inline int bit_board::non_pawn_material() const
{
	return b_info.non_pawn_material[white] + b_info.non_pawn_material[black];
}

inline uint64_t bit_board::square_bb(const int sq) const
{
	return squareBB[sq];
}

inline bool bit_board::can_enpassant() const
{
	return st_->ep_square > 0 && st_->ep_square < 64;
}

inline int bit_board::ep_square() const
{
	return st_->ep_square;
}

inline void bit_board::move_piece(const int piece, const int color, const int from, const int to)
{
	const auto from_to = square_bb(from) ^ square_bb(to);

	by_color_pieces_bb[color][piece] ^= from_to;
	all_pieces_color_bb[color] ^= from_to;

	by_piece_type[piece] ^= from_to;
	full_squares ^= from_to;
	empty_squares ^= from_to;

	piece_on[from] = no_piece;
	piece_on[to] = piece;

	piece_index[to] = piece_index[from];
	piece_loc[color][piece][piece_index[to]] = to;
}

inline void bit_board::add_piece(const int piece, const int color, const int sq)
{
	by_color_pieces_bb[color][piece] ^= square_bb(sq);
	all_pieces_color_bb[color] ^= square_bb(sq);
	by_piece_type[piece] ^= square_bb(sq);
	full_squares ^= square_bb(sq);
	empty_squares ^= square_bb(sq);

	piece_on[sq] = piece;
	piece_index[sq] = piece_count[color][piece]++;
	piece_loc[color][piece][piece_index[sq]] = sq;
}

inline void bit_board::remove_piece(const int piece, const int color, const int sq)
{
	by_color_pieces_bb[color][piece] ^= square_bb(sq);
	all_pieces_color_bb[color] ^= square_bb(sq);
	by_piece_type[piece] ^= square_bb(sq);
	full_squares ^= square_bb(sq);
	empty_squares ^= square_bb(sq);

	piece_on[sq] = no_piece;

	const auto l_sq = piece_loc[color][piece][--piece_count[color][piece]];
	piece_index[l_sq] = piece_index[sq];
	piece_loc[color][piece][piece_index[l_sq]] = l_sq;
	piece_loc[color][piece][piece_count[color][piece]] = sq_none;
}

template <int Pt>
int bit_board::count(const int color) const
{
	return piece_count[color][Pt];
}

template <int Sh>
uint64_t shift_bb(const uint64_t b)
{
	return Sh == north
		       ? b >> 8
		       : Sh == south
		       ? b << 8
		       : Sh == northeast
		       ? (b & ~file_h) >> 7
		       : Sh == southeast
		       ? (b & ~file_h) << 9
		       : Sh == northwest
		       ? (b & ~file_a) >> 9
		       : Sh == southwest
		       ? (b & ~file_a) << 7
		       : 0LL;
}
