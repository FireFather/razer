#pragma once
#include <algorithm>
#include <cstdint>
#include <intrin.h>
#include <chrono>
#include <cmath>
#include <cstring>

constexpr auto engine = "Razer";
constexpr auto version = "0.0";
constexpr auto platform = "x64";
constexpr auto author = "Max Carlson & Firefather";


#ifdef _MSC_VER
#pragma warning(disable: 4244) // '+=': conversion from 'int' to 'T', possible loss of data
#pragma warning(disable: 4127) // conditional expression is constant
#else
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

constexpr auto cache_line_size = 64;

inline void prefetch(void* addr)
{
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
	_mm_prefetch(static_cast<char*>(addr), _MM_HINT_T0);
#else
    __builtin_prefetch(addr);
#endif
}

typedef std::chrono::milliseconds::rep time_point;

#define SCORE_ZERO make_score(0, 0)
#define S(MG, EG) make_score(MG, EG)

constexpr auto main_gen = 0;
constexpr auto captures = 1;
constexpr auto evasions = 2;
constexpr auto quiets = 3;
constexpr auto legal = 4;
constexpr auto perft_testing = 5;

constexpr auto draw = 0;
constexpr auto known_win = 9000;
constexpr auto endgame_mat = 1300;

constexpr auto midgame_limit = 6090;
constexpr auto endgame_limit = 1475;

constexpr auto kingside = 0;
constexpr auto queenside = 1;

constexpr auto tt_alpha = 1;
constexpr auto tt_beta = 2;
constexpr auto tt_exact = 3;

constexpr auto max_ply = 128;

constexpr auto depth_qs = 0;
constexpr auto depth_qs_no_check = -2;

constexpr auto inf = 32001;
constexpr auto mate = 32000;
constexpr auto mate_in_max_ply = 31872;
constexpr auto mated_in_max_ply = -31872;

constexpr uint64_t rank5 = 4278190080L;
constexpr uint64_t rank6 = rank5 >> 8;
constexpr uint64_t rank7 = rank6 >> 8;
constexpr uint64_t rank8 = rank7 >> 8;
constexpr uint64_t rank4 = 1095216660480L;
constexpr uint64_t rank3 = rank4 << 8;
constexpr uint64_t rank2 = rank3 << 8;
constexpr uint64_t rank1 = rank2 << 8;

constexpr uint64_t file_a = 0x0101010101010101ULL;
constexpr uint64_t file_b = file_a << 1;
constexpr uint64_t file_c = file_a << 2;
constexpr uint64_t file_d = file_a << 3;
constexpr uint64_t file_e = file_a << 4;
constexpr uint64_t file_f = file_a << 5;
constexpr uint64_t file_g = file_a << 6;
constexpr uint64_t file_h = file_a << 7;

constexpr uint64_t file_ab = file_a + file_b;
constexpr uint64_t file_gh = file_g + file_h;

constexpr uint64_t king_span = 460039L;
constexpr uint64_t knight_span = 43234889994L;

enum square
{
	a8 = 0, b8, c8, d8, e8, f8, g8, h8,
	a7 = 8, b7, c7, d7, e7, f7, g7, h7,
	a6 = 16, b6, c6, d6, e6, f6, g6, h6,
	a5 = 24, b5, c5, d5, e5, f5, g5, h5,
	a4 = 32, b4, c4, d4, e4, f4, g4, h4,
	a3 = 40, b3, c3, d3, e3, f3, g3, h3,
	a2 = 48, b2, c2, d2, e2, f2, g2, h2,
	a1 = 56, b1, c1, d1, e1, f1, g1, h1,
	sq_none,
	sq_all = 64,

	delta_n = 8,
	delta_e = 1,
	delta_s = -8,
	delta_w = -1,

	delta_nn = delta_n + delta_n,
	delta_ne = delta_n + delta_e,
	delta_se = delta_s + delta_e,
	delta_ss = delta_s + delta_s,
	delta_sw = delta_s + delta_w,
	delta_nw = delta_n + delta_w
};

enum Piece
{
	no_piece,
	pawn,
	knight,
	bishop,
	rook,
	queen,
	king,
	piece
};

enum color
{
	white,
	black,
	Color
};

enum Castling
{
	no_castling,
	white_oo,
	white_ooo = white_oo << 1,
	black_oo = white_oo << 2,
	black_ooo = white_oo << 3,
	any_castling = white_oo | white_ooo | black_oo | black_ooo,
	castling_rights = 16
};

enum game_stage
{
	mg,
	eg,
	stage
};

enum scale_factor
{
	sf_draw = 0,
	sf_one_pawn = 48,
	sf_normal = 64,
	sf_max = 128,
	sf_none = 255
};

enum flag
{
	flag_none,
	Alpha,
	Beta,
	exact = Alpha | Beta
};

enum directions
{
	north = -8,
	northeast = -7,
	northwest = -9,
	west = -1,
	south = 8,
	southwest = 7,
	southeast = 9,
	east = 1
};

enum movetype
{
	normal,
	castle = 1 << 12,
	enpassant = 2 << 12,
	promotion = 3 << 12
};

enum Move
{
	move_none,
	move_null = 65
};

struct s_move
{
	Move move;
	int score;

	operator Move() const
	{
		return move;
	}

	void operator =(const Move m)
	{
		move = m;
	}
};

constexpr int piece_value[7] =
{
	0, 100, 325, 350, 500, 1000, 0
};

constexpr uint64_t rank_masks8[8] =
{
	0xFF00000000000000L, 0xFF000000000000L, 0xFF0000000000L, 0xFF00000000L,
	0xFF000000L, 0xFF0000L, 0xFF00L, 0xFFL,
};

constexpr uint64_t file_masks8[8] =
{
	0x101010101010101L, 0x202020202020202L, 0x404040404040404L, 0x808080808080808L,
	0x1010101010101010L, 0x2020202020202020L, 0x4040404040404040L, 0x8080808080808080L
};

inline time_point now()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).
		count();
}

inline int from_sq(const Move m)
{
	return m & 0x3f;
}

inline int to_sq(const Move m)
{
	return m >> 6 & 0x3f;
}

inline int from_to(const Move m)
{
	return m & 0xFFF;
}

inline movetype move_type(const Move m)
{
	return static_cast<movetype>(m & 3 << 12);
}

inline int promotion_type(const Move m)
{
	return (m >> 15 & 3) + 2;
}

inline int file_of(const int sq)
{
	return sq & 7;
}

inline int rank_of(const int sq)
{
	return sq >> 3 ^ 7;
}

inline int file_distance(const int s1, const int s2)
{
	return abs(file_of(s1) - file_of(s2));
}

inline int rank_distance(const int s1, const int s2)
{
	return abs(rank_of(s1) - rank_of(s2));
}

inline int relative_rank(const int color, const int rank)
{
	return rank ^ color * 7;
}

inline int relative_rank_sq(const int color, const int square)
{
	return relative_rank(color, rank_of(square));
}

inline int relative_square(const int color, const int square)
{
	return square ^ color * 56;
}

inline int pawn_push(const int color)
{
	return color == white ? -8 : 8;
}

extern int num_moves;
extern uint64_t forward_bb[Color][sq_all];
extern uint64_t between_squares[sq_all][sq_all];
extern uint64_t PassedPawnMask[Color][sq_all];
extern uint64_t PawnAttackSpan[Color][sq_all];
extern uint64_t line_bb[sq_all][sq_all];
extern int square_distance[sq_all][sq_all];
