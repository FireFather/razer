#pragma once
#include <sstream>
#include "common.h"
#include "bitboard.h"

class bit_board;
class state_info;

namespace Uci
{
	std::string move_to_str(const Move& m);
}

class UCI
{
public:
	UCI();
	void loop(int argc, char* argv[]) const;
	void new_game(bit_board& board, state_list& state) const;
	void update_position(bit_board& board, std::istringstream& input, state_list& state) const;
	void set_option(std::istringstream& input) const;
	static void go(const bit_board& board, std::istringstream& input, state_list& state);
	void bench(bit_board& board, state_list& state) const;
	void perft(const bit_board& board, bool is_divide, std::istringstream& input) const;
	static Move str_to_move(const bit_board& board, std::string& input);
};

static UCI uci;
