#include "uci.h"

#include <iostream>
#include <sstream>
#include <string>

#include "bitboard.h"
#include "hash.h"
#include "movegen.h"
#include "perft.h"
#include "search.h"
#include "threads.h"

using namespace std;
int num_moves = 0;
bool is_white = true;
const std::string startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

UCI::UCI()
{
	tt.resize(1024);
}

void UCI::loop(const int argc, char* argv[]) const
{
	std::string line;
	std::string token, cmd;

	slider_attacks.initialize();
	bit_board board{};
	board.zobrist.zobrist_fill();
	board.init_board();
	threads.initialize();
	state_list state(new std::deque<state_info>(1));
	std::cout.setf(std::ios::unitbuf);
	new_game(board, state);
	Search::init_search();

	for (auto i = 1; i < argc; ++i)
		cmd += std::string(argv[i]) + " ";

	do
	{
		// read cmd 'bench' if present, for automated external PGO compile
		if (argc == 1 && !getline(std::cin, cmd))
			cmd = "quit";
		//cmd = trim(cmd);
		if (cmd.empty())
			continue;

		std::istringstream is(cmd);

		token.clear();
		is >> std::skipws >> token;

		if (token == "uci")
		{
			sync_out << "id name " << engine << " " << version << " " << platform << sync_endl;
			sync_out << "id author " << author << sync_endl;
			sync_out << "option name Hash type spin default 1024 min 1 max 1048576" << sync_endl;
			sync_out << "option name Threads type spin default 1 min 1 max 128" << sync_endl;
			sync_out << "option name Clear Hash type button" << sync_endl;
			sync_out << "uciok" << sync_endl;
		}
		else if (token == "isready")
		{
			sync_out << "readyok" << sync_endl;
		}
		else if (token == "ucinewgame")
		{
			new_game(board, state);
		}
		else if (token == "setoption")
		{
			set_option(is);
		}
		else if (token == "position")
		{
			update_position(board, is, state);
		}
		else if (token == "go")
		{
			go(board, is, state);
		}
		else if (token == "stop")
		{
			threads.stop = true;
		}
		else if (token == "quit")
		{
			break;
		}
		else if (token == "threads")
		{
			is >> token;
			threads.number_of_threads(stoi(token));
		}
		else if (token == "perft" || token == "divide")
		{
			const auto div = token == "divide";
			perft(board, div, is);
		}
		else if (token == "bench")
		{
			bench(board, state);
		}
		else
		{
		}
	} while (token != "quit" && argc == 1);
}

void UCI::update_position(bit_board& board, std::istringstream& input, state_list& state) const
{
	std::string token, fen;
	input >> token;
	state = std::make_unique<std::deque<state_info>>(1);

	if (token == "startpos")
	{
		new_game(board, state);
	}
	else if (token == "fen")
	{
		while (input >> token && token != "moves")
			fen += token + " ";

		board.reset_board(&fen, threads.main(), &state->back());
		is_white = !board.b_info.side_to_move;
	}
	else
	{
		return;
	}

	while (input >> token)
	{
		if (token != "moves")
		{
			auto m = str_to_move(board, token);

			state->emplace_back();
			board.make_move(m, state->back(), !is_white);
			num_moves += 1;

			is_white = !is_white;
		}
	}
}

void UCI::new_game(bit_board& board, state_list& state) const
{
	state = std::make_unique<std::deque<state_info>>(1);
	board.reset_board(nullptr, threads.main(), &state->back());
	board.zobrist.get_zobrist_hash(board);
	num_moves = 0;
	is_white = true;
	tt.clear_table();
	Search::clear();
}

void UCI::set_option(std::istringstream& input) const
{
	std::string token;
	input >> token;

	if (token == "name")
	{
		while (input >> token)
		{
			if (token == "Hash")
			{
				input >> token;
				input >> token;
				tt.resize(stoi(token));
				break;
			}
			if (token == "Clear")
			{
				tt.clear_table();
				break;
			}
			if (token == "Threads")
			{
				input >> token;
				input >> token;
				threads.number_of_threads(stoi(token));
				break;
			}
		}
	}
}

void UCI::go(const bit_board& board, std::istringstream& input, state_list& state)
{
	Search::search_params scs;
	scs.start_time = now();
	std::string token;

	while (input >> token)
	{
		if (token == "wtime")
		{
			input >> scs.time[white];
			scs.infinite = 0;
		}
		else if (token == "btime")
		{
			input >> scs.time[black];
			scs.infinite = 0;
		}
		else if (token == "winc")
		{
			input >> scs.inc[white];
			scs.infinite = 0;
		}
		else if (token == "binc")
		{
			input >> scs.inc[black];
			scs.infinite = 0;
		}
		else if (token == "movestogo")
		{
			input >> scs.moves_to_go;
			scs.infinite = 0;
		}
		else if (token == "depth")
		{
			input >> scs.depth;
			scs.infinite = 0;
		}
		else if (token == "infinite")
			scs.infinite = 1;
	}

	threads.search_start(board, state, scs);
	is_white = !is_white;
	num_moves += 1;
}

namespace Uci
{
	std::string move_to_str(const Move& m)
	{
		const std::string flips_l[8] =
		{
			"a", "b", "c", "d", "e", "f", "g", "h"
		};
		constexpr int flips_n[8] =
		{
			8, 7, 6, 5, 4, 3, 2, 1
		};

		if (m == move_none)
			return "(none)";
		if (m == move_null)
			return "0000";

		const auto x = file_of(from_sq(m));
		const auto y = rank_of(from_sq(m)) ^ 7;
		const auto x1 = file_of(to_sq(m));
		const auto y1 = rank_of(to_sq(m)) ^ 7;

		std::string prom;

		if (move_type(m) == promotion)
			prom = " pnbrqk"[promotion_type(m)];

		std::stringstream ss;
		ss << flips_l[x] << flips_n[y] << flips_l[x1] << flips_n[y1] << prom;

		return ss.str();
	}
}

Move UCI::str_to_move(const bit_board& board, std::string& input)
{
	if (input.length() == 5)
		input[4] = static_cast<char>(tolower(input[4]));

	for (auto& m : move_list<legal>(board))
		if (input == Uci::move_to_str(m))
			return m;

	return move_none;
}

void UCI::perft(const bit_board& board, const bool is_divide, std::istringstream& input) const
{
	auto depth = 6;
	std::string tk;
	input >> tk;
	if (input)
		depth = std::stoi(tk);
	perft::perft_init(board, is_divide, depth);
}
