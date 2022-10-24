#include "bench.h"

#include <iostream>
#include <sstream>
#include <string>

#include "bitboard.h"
#include "common.h"
#include "search.h"
#include "threads.h"
#include "uci.h"

void UCI::bench(bit_board& board, state_list& state) const
{
	static FILE* bench_log;
	static char file_name[256];
	char buf[256];

	uint64_t nodes = 0;
	auto start_time = now();

	for (auto& bench_position : bench_positions)
	{
		Search::clear();
		std::string test_fen = "fen ";
		test_fen += bench_position;
		std::istringstream is(test_fen);
		update_position(board, is, state);
		std::string sdepth = "depth 16";
		std::istringstream iss(sdepth);
		go(board, iss, state);
		threads.main()->wait_for_search_stop();
		nodes += threads.nodes_searched();
	}

	auto elapsed_time = static_cast<double>(now() + 1 - start_time) / 1000;
	auto nps = static_cast<double>(nodes) / elapsed_time;

	Sleep(500);

	sync_indent;
	std::cout << "Nodes: " << nodes << std::endl;

	std::ostringstream ss;

	ss.precision(2);
	ss << "Time : " << std::fixed << elapsed_time << " secs" << std::endl;
	std::cout << ss.str();
	ss.str(std::string());

	ss.precision(0);
	ss << "NPS  : " << std::fixed << nps << std::endl;
	std::cout << ss.str();
	ss.str(std::string());

	ss.precision(2);
	ss << "TTD  : " << std::fixed << elapsed_time / 64 << " secs" << std::endl;
	std::cout << ss.str();
	ss.str(std::string());

	auto now = time(nullptr);
	strftime(buf, 32, "%b-%d_%H-%M", localtime(&now));
	sprintf(file_name, "bench_%s.txt", buf);
	printf("\nresults written to:\n");
	printf("benchmark_%s.txt\n\n", buf);
	bench_log = fopen(file_name, "wt");

	fprintf(bench_log, "%s %s %s\n", engine, version, platform);
	fprintf(bench_log, "Nodes: %lld\n", nodes);
	fprintf(bench_log, "Time : %.2f secs\n", elapsed_time);
	fprintf(bench_log, "NPS  : %.0f\n", nps);
	fprintf(bench_log, "TTD  : %.2f secs\n", elapsed_time / 64);
	fclose(bench_log);

	new_game(board, state);
}
