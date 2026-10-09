#include <charconv>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string_view>
#include <unordered_set>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "headers/AIPlayer.hpp"

const std::array<std::string_view, 5> phases{ "opening", "middle", "captures", "kings", "endgame" };
const std::array<std::string_view, 5> stages{ "tt", "tactical", "killers", "history", "lmr" };

double cpuMilliseconds() {
#ifdef _WIN32
    FILETIME created, exited, kernel, user;
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user))
        throw std::runtime_error("Cannot read benchmark thread CPU time");
    ULARGE_INTEGER kernelTime, userTime;
    kernelTime.LowPart = kernel.dwLowDateTime; kernelTime.HighPart = kernel.dwHighDateTime;
    userTime.LowPart = user.dwLowDateTime; userTime.HighPart = user.dwHighDateTime;
    return static_cast<double>(kernelTime.QuadPart + userTime.QuadPart) / 10000.0;
#else
    const auto time = std::clock();
    if (time == static_cast<std::clock_t>(-1)) throw std::runtime_error("Cannot read benchmark CPU time");
    return 1000.0 * time / CLOCKS_PER_SEC;
#endif
}

struct Position {
    std::string phase;
    std::vector<std::uint16_t> moves;
};

std::string_view phaseOf(const Checkers& board) {
    const int pieces = std::popcount(board.getDarkPieces() | board.getLightPieces());
    if (pieces <= 5) return "endgame";
    if (board.isCaptureMove(board.getMoves()[0])) return "captures";
    if (board.getKingPieces()) return "kings";
    return pieces >= 20 ? "opening" : "middle";
}

Position parsePosition(const std::string& line) {
    std::istringstream stream(line);
    Position position;
    stream >> position.phase;
    if (std::find(phases.begin(), phases.end(), position.phase) == phases.end())
        throw std::runtime_error("Unknown position phase");
    std::string token;
    while (stream >> token) {
        unsigned move;
        const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), move, 16);
        if (error != std::errc{} || end != token.data() + token.size() || move > 0x1fff)
            throw std::runtime_error("Invalid encoded move: " + token);
        position.moves.push_back(static_cast<std::uint16_t>(move));
    }
    return position;
}

void replay(Checkers& board, const Position& position) {
    board.reset();
    for (const auto move : position.moves) {
        if (board.isDraw()) throw std::runtime_error("Move after drawn position");
        const auto& moves = board.getMoves();
        const auto end = moves.begin() + board.getNumMoves();
        const auto found = std::find(moves.begin(), end, move);
        if (found == end) throw std::runtime_error("Illegal move in position sequence");
        board.makeMove(static_cast<int>(found - moves.begin()));
    }
    if (board.isDraw() || board.getNumMoves() == 0 || board.isMidCapture())
        throw std::runtime_error("Suite positions must be playable, complete-turn positions");
    if (position.phase != phaseOf(board)) throw std::runtime_error("Position phase does not match board");
}

std::vector<Position> loadPositions(const std::filesystem::path& path, Checkers& board) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open position corpus");
    std::vector<Position> positions;
    std::unordered_set<std::uint64_t> seen;
    std::string line;
    while (std::getline(input, line)) {
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#') continue;
        positions.push_back(parsePosition(line));
        replay(board, positions.back());
        if (!seen.insert(board.hash()).second) throw std::runtime_error("Duplicate board in corpus");
    }
    if (!input.eof() || positions.empty()) throw std::runtime_error("Empty or unreadable corpus");
    return positions;
}

double geometricMean(const std::vector<double>& ratios) {
    if (ratios.empty()) throw std::runtime_error("No node ratios");
    double sum = 0;
    for (double ratio : ratios) {
        if (!std::isfinite(ratio) || ratio <= 0) throw std::runtime_error("Invalid node ratio");
        sum += std::log(ratio);
    }
    return std::exp(sum / ratios.size());
}

void selfTest(const char* corpus) {
    if (std::abs(geometricMean({ 2.0, 0.5 }) - 1.0) > 1e-12
        || std::abs(geometricMean({ 4.0, 1.0 }) - 2.0) > 1e-12)
        throw std::runtime_error("Geometric mean self-check failed");
    Checkers board;
    const auto hash = board.searchHash();
    replay(board, parsePosition("opening"));
    if (board.searchHash() != hash) throw std::runtime_error("Replay self-check failed");
    const auto move = board.getMoves()[0];
    std::ostringstream line;
    line << "opening " << std::hex << move;
    board.makeMove(0);
    const auto movedHash = board.searchHash();
    replay(board, parsePosition(line.str()));
    if (board.searchHash() != movedHash) throw std::runtime_error("Move replay self-check failed");
    for (const auto* bad : { "unknown", "opening z", "opening 2000", "opening 0" }) {
        bool rejected = false;
        try { replay(board, parsePosition(bad)); }
        catch (const std::runtime_error&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Invalid corpus entry was accepted");
    }
    if (corpus) {
        const auto positions = loadPositions(corpus, board);
        if (positions.size() != 100) throw std::runtime_error("Fixed corpus must contain 100 positions");
        for (const auto phase : phases)
            if (std::count_if(positions.begin(), positions.end(), [phase](const auto& p) { return p.phase == phase; }) != 20)
                throw std::runtime_error("Fixed corpus stratum must contain 20 positions");
    }
    std::cout << "Suite self-checks passed\n";
}

void generate(const std::filesystem::path& path, Checkers& board, AIPlayer& ai) {
    // Freeze a small stratified corpus; generation never runs during measurements.
    std::array<int, 5> counts{};
    std::vector<Position> positions;
    std::unordered_set<std::uint64_t> seen;
    std::mt19937 rng(42);
    for (int game = 0; game < 1000 && positions.size() < 100; ++game) {
        board.reset();
        ai.resetTT();
        std::vector<std::uint16_t> pathMoves;
        for (int turn = 0; turn < 160 && !board.isDraw() && board.getNumMoves() > 0; ++turn) {
            const auto phase = phaseOf(board);
            const auto category = std::find(phases.begin(), phases.end(), phase) - phases.begin();
            if (turn >= 4 && turn % 3 == 0 && counts[category] < 20 && seen.insert(board.hash()).second) {
                positions.push_back({ std::string(phase), pathMoves });
                ++counts[category];
                if (positions.size() == 100) break;
            }
            if (turn < 8 || rng() % 5 == 0) {
                bool switched;
                do {
                    const int index = rng() % board.getNumMoves();
                    pathMoves.push_back(board.getMoves()[index]);
                    switched = board.makeMove(index);
                } while (!switched);
            }
            else {
                const auto result = ai.search(4);
                for (int index : result.pv) {
                    pathMoves.push_back(board.getMoves()[index]);
                    board.makeMove(index);
                }
            }
        }
    }
    if (positions.size() != 100) throw std::runtime_error("Could not fill all five position strata");
    // Verify the entire corpus before creating the output file.
    for (const auto& position : positions) replay(board, position);
    std::ofstream output(path);
    if (!output) throw std::runtime_error("Cannot create corpus file");
    output << "# Seed 42; depth-4 TT self-play; random first 8 turns and 20% exploration thereafter.\n"
           << "# 20 unique boards each: opening, middle, captures, kings, endgame.\n"
           << "# Each line: phase followed by hexadecimal encoded moves from the initial board.\n";
    for (const auto& position : positions) {
        output << position.phase;
        for (auto move : position.moves) output << ' ' << std::hex << move;
        output << '\n';
    }
    output.close();
    if (!output) throw std::runtime_error("Failed writing corpus");
    std::cout << "Saved 100 verified positions to " << path << '\n';
}

int main(int argc, char** argv) try {
    if ((argc == 2 || argc == 3) && std::string_view(argv[1]) == "--self-test") {
        selfTest(argc == 3 ? argv[2] : nullptr);
        return 0;
    }
    const bool generating = argc == 3 && std::string_view(argv[1]) == "generate";
    const std::string_view lmrFlag = argc == 5 ? argv[4] : "";
    const bool singlePolicy = lmrFlag == "--only-history" || lmrFlag == "--only-lmr" || lmrFlag == "--only-lmr-endgame";
    const bool comparingLMR = lmrFlag == "--lmr" || lmrFlag == "--lmr-late" || lmrFlag == "--lmr-endgame" || singlePolicy;
    const LMRPolicy policy = lmrFlag == "--lmr-late" ? LMRPolicy::Late
        : lmrFlag == "--lmr-endgame" || lmrFlag == "--only-lmr-endgame" ? LMRPolicy::EndgameGuard : LMRPolicy::Conservative;
    const std::string_view candidate = lmrFlag == "--lmr-late" ? "lmr_late"
        : lmrFlag == "--lmr-endgame" || lmrFlag == "--only-lmr-endgame" ? "lmr_endgame" : "lmr";
    int depth = 0;
    if (!generating) {
        if (argc != 4 && !comparingLMR) throw std::runtime_error("Usage: suiteBench <depth> <positions.txt> <results.csv> [--lmr|--lmr-late|--lmr-endgame|--only-history|--only-lmr|--only-lmr-endgame] | generate <positions.txt> | --self-test");
        const std::string_view arg = argv[1];
        const auto [end, error] = std::from_chars(arg.data(), arg.data() + arg.size(), depth);
        if (error != std::errc{} || end != arg.data() + arg.size() || depth < 1 || depth > 200)
            throw std::runtime_error("Depth must be an integer from 1 to 200");
        if (std::filesystem::exists(argv[2]) && std::filesystem::exists(argv[3])
            && std::filesystem::equivalent(argv[2], argv[3]))
            throw std::runtime_error("Results file must differ from corpus file");
    }
    for (const auto* file : { "egtb.bin", "egtb_dtz.bin", "nnue_best_v2.bin" })
        if (!std::filesystem::exists(file)) throw std::runtime_error(std::string("Missing ") + file + "; run from build/bin");
    EGTB egtb;
    egtb.buildOrLoad("egtb.bin", "egtb_dtz.bin");
    NNUE nnue{ {128, 256, 32, 1} };
    nnue.load("nnue_best_v2.bin");
    NNUEInference inference{ nnue };
    Checkers board{ &inference };
    if (generating) {
        if (std::filesystem::exists(argv[2])) throw std::runtime_error("Corpus already exists; choose a new path");
        AIPlayer ai{ board, egtb, inference, false, MoveOrdering::TT };
        generate(argv[2], board, ai);
        return 0;
    }
    const auto positions = loadPositions(argv[2], board);
    std::ofstream output(argv[3]);
    if (!output) throw std::runtime_error("Cannot create results CSV");
    output << "position,phase,stage,depth,pieces,kings,root_moves,capture,draw_counter,score,nodes,time_ms,tt_probes,tt_matches,tt_hits,tt_cutoffs,egtb_hits,beta_cutoffs,first_move_cutoffs,"
        << (singlePolicy ? "ratio_vs_self" : comparingLMR ? "ratio_vs_history" : "ratio_vs_tt") << ",score_delta,lmr_reductions,lmr_researches,"
        << "endgame_reductions,endgame_researches,endgame_reduced_cutoffs,endgame_research_cutoffs,endgame_reduced_nodes,endgame_research_nodes,nps,cpu_ms\n";
    std::vector<int> baselineNodes(positions.size()), baselineScores(positions.size());
    const int firstStage = singlePolicy && lmrFlag != "--only-history" ? 4 : comparingLMR ? 3 : 0;
    const int endStage = singlePolicy ? firstStage + 1 : comparingLMR ? 5 : 4;
    for (int stage = firstStage; stage < endStage; ++stage) {
        AIPlayer ai{ board, egtb, inference, false, stage == 4 ? MoveOrdering::History : static_cast<MoveOrdering>(stage), stage == 4, policy };
        const auto stageName = stage == 4 ? candidate : stages[stage];
        std::uint64_t totalNodes = 0;
        int wins = 0, ties = 0, losses = 0;
        int scoreDifferences = 0;
        double totalMs = 0;
        std::vector<double> ratios;
        for (std::size_t p = 0; p < positions.size(); ++p) {
            replay(board, positions[p]);
            ai.resetTT();
            const auto hash = board.searchHash();
            const double cpuStart = cpuMilliseconds();
            const auto start = std::chrono::steady_clock::now();
            const auto result = ai.search(depth);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            const double cpuMs = cpuMilliseconds() - cpuStart;
            const int nodes = ai.getNodesHit();
            if (nodes <= 0 || board.searchHash() != hash || result.completedDepth != depth)
                throw std::runtime_error("Search failed at position " + std::to_string(p + 1));
            if (stage == firstStage) {
                baselineNodes[p] = nodes;
                baselineScores[p] = result.score;
            }
            else if (!comparingLMR && result.score != baselineScores[p])
                throw std::runtime_error("Score mismatch at position " + std::to_string(p + 1) + " / " + std::string(stages[stage])
                    + ": TT=" + std::to_string(baselineScores[p]) + ", stage=" + std::to_string(result.score));
            if (result.score != baselineScores[p]) ++scoreDifferences;
            const double ratio = static_cast<double>(baselineNodes[p]) / nodes;
            ratios.push_back(ratio);
            totalNodes += nodes;
            totalMs += ms;
            if (nodes < baselineNodes[p]) ++wins;
            else if (nodes == baselineNodes[p]) ++ties;
            else ++losses;
            const auto& endgame = ai.getLMREndgameStats();
            output << std::fixed << std::setprecision(6) << p + 1 << ',' << positions[p].phase << ',' << stageName << ',' << depth << ','
                << std::popcount(board.getDarkPieces() | board.getLightPieces()) << ',' << std::popcount(board.getKingPieces()) << ','
                << board.getNumMoves() << ',' << board.isCaptureMove(board.getMoves()[0]) << ',' << board.getDrawCounter() << ','
                << result.score << ',' << nodes << ',' << ms << ',' << ai.getTTProbes() << ',' << ai.getTTMatches() << ','
                << ai.getTTUsefulHits() << ',' << ai.getTTCutoffs() << ',' << ai.getEgtbHits() << ',' << ai.getBetaCutoffs() << ','
                << ai.getFirstMoveCutoffs() << ',' << ratio << ',' << result.score - baselineScores[p] << ','
                << ai.getLMRReductions() << ',' << ai.getLMRResearches() << ',' << endgame.reductions << ',' << endgame.researches << ','
                << endgame.reducedCutoffs << ',' << endgame.researchCutoffs << ',' << endgame.reducedNodes << ',' << endgame.researchNodes << ','
                << (ms > 0 ? 1000.0 * nodes / ms : 0) << ',' << cpuMs << '\n';
            if ((p + 1) % 20 == 0) std::cout << stageName << ": " << p + 1 << '/' << positions.size() << " positions\n" << std::flush;
        }
        std::cout << stageName << ": total nodes=" << totalNodes << ", time(ms)=" << totalMs
            << ", geometric mean " << (singlePolicy ? "self" : comparingLMR ? "history" : "TT") << "/new=" << geometricMean(ratios)
            << ", fewer/equal/more nodes=" << wins << '/' << ties << '/' << losses
            << ", score differences=" << scoreDifferences << '\n' << std::flush;
    }
    output.close();
    if (!output) throw std::runtime_error("Failed writing results CSV");
    std::cout << "Results saved to " << argv[3] << '\n';
}
catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
