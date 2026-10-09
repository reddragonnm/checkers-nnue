#include <charconv>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "headers/AIPlayer.hpp"

std::vector<std::vector<std::uint16_t>> loadOpenings(const char* path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open match openings");
    std::vector<std::vector<std::uint16_t>> openings;
    std::unordered_set<std::uint64_t> seen;
    Checkers board;
    std::string line;
    while (std::getline(input, line)) {
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#') continue;
        std::istringstream stream(line);
        std::string token;
        std::vector<std::uint16_t> opening;
        board.reset();
        while (stream >> token) {
            unsigned encoded;
            const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), encoded, 16);
            if (error != std::errc{} || end != token.data() + token.size() || encoded > 0x1fff || board.isDraw())
                throw std::runtime_error("Invalid opening move");
            const auto movesEnd = board.getMoves().begin() + board.getNumMoves();
            const auto found = std::find(board.getMoves().begin(), movesEnd, encoded);
            if (found == movesEnd) throw std::runtime_error("Illegal opening move");
            opening.push_back(static_cast<std::uint16_t>(encoded));
            board.makeMove(static_cast<int>(found - board.getMoves().begin()));
        }
        if (opening.empty() || board.isDraw() || board.getNumMoves() == 0 || board.isMidCapture())
            throw std::runtime_error("Opening must end at a playable complete turn");
        if (!seen.insert(board.hash()).second) throw std::runtime_error("Duplicate match opening");
        openings.push_back(std::move(opening));
    }
    if (!input.eof() || openings.empty()) throw std::runtime_error("Empty or unreadable match openings");
    return openings;
}

int main(int argc, char** argv) try {
    if (argc == 3 && std::string_view(argv[1]) == "--check-openings") {
        const auto count = loadOpenings(argv[2]).size();
        std::cout << "Verified " << count << " unique, playable match openings\n";
        return 0;
    }
    const std::string_view lmrFlag = argc >= 5 ? argv[4] : "";
    const bool originalB = lmrFlag == "--lmr-vs-original";
    const bool lmrA = lmrFlag == "--lmr" || lmrFlag == "--lmr-late" || lmrFlag == "--lmr-endgame" || originalB;
    const LMRPolicy policy = lmrFlag == "--lmr-late" ? LMRPolicy::Late
        : lmrFlag == "--lmr-endgame" || originalB ? LMRPolicy::EndgameGuard : LMRPolicy::Conservative;
    if (argc < 3 || argc > 7 || (argc >= 5 && !lmrA)) {
        std::cerr << "Usage: headlessMatch <model-A.bin> <model-B.bin|piececount> [games] [--lmr|--lmr-late|--lmr-endgame|--lmr-vs-original] [openings.txt] [moves.csv]\n";
        return 1;
    }

    int games{ 200 };
    if (argc >= 4) {
        const char* end{ argv[3] + std::strlen(argv[3]) };
        auto [ptr, error] { std::from_chars(argv[3], end, games) };
        if (error != std::errc{} || ptr != end || games <= 0 || (lmrA && games % 2 != 0)) {
            std::cerr << "Games must be a positive integer (even for paired LMR matches)\n";
            return 1;
        }
    }
    constexpr int moveTimeMs{ 100 };
    constexpr int openingTurns{ 8 };
    const auto fixedOpenings = argc >= 6 ? loadOpenings(argv[5]) : std::vector<std::vector<std::uint16_t>>{};
    if (!fixedOpenings.empty() && static_cast<std::size_t>(games) != 2 * fixedOpenings.size())
        throw std::runtime_error("Games must equal twice the number of fixed openings");
    std::ofstream moveTimings;
    if (argc == 7) {
        for (const auto* input : std::array<const char*, 5>{ argv[1], argv[2], argv[5], "egtb.bin", "egtb_dtz.bin" })
            if (std::filesystem::exists(input) && std::filesystem::exists(argv[6])
                && std::filesystem::equivalent(input, argv[6]))
                throw std::runtime_error("Move CSV must differ from input files");
        moveTimings.open(argv[6]);
        if (!moveTimings) throw std::runtime_error("Cannot create move timings CSV");
        moveTimings << "game,opening,player,variant,dark,pieces,kings,capture,root_moves,draw_counter,depth,nodes,time_ms,budget_ms\n";
    }

    EGTB egtb;
    egtb.buildOrLoad("egtb.bin", "egtb_dtz.bin");

    const bool pieceCountB{ std::string_view(argv[2]) == "piececount" };
    NNUE nnueA{ { 128, 256, 32, 1 } };
    NNUE nnueB{ { 128, 256, 32, 1 } };
    nnueA.load(argv[1]);
    if (!pieceCountB) nnueB.load(argv[2]);
    NNUEInference inferenceA{ nnueA };
    NNUEInference inferenceB{ nnueB };
    Checkers boardA{ &inferenceA };
    Checkers boardB{ pieceCountB ? nullptr : &inferenceB };
    AIPlayer playerA{ boardA, egtb, inferenceA, false, MoveOrdering::History, lmrA, policy };
    AIPlayer playerB{ boardB, egtb, inferenceB, pieceCountB, MoveOrdering::History, originalB };
    const std::string_view variantA = !lmrA ? "history" : policy == LMRPolicy::EndgameGuard ? "lmr_endgame"
        : policy == LMRPolicy::Late ? "lmr_late" : "lmr";
    const std::string_view variantB = originalB ? "lmr" : "history";

    std::mt19937_64 openingRng{ 42 };
    std::vector<int> opening;
    int aWins{ 0 }, bWins{ 0 }, draws{ 0 };
    std::uint64_t nodesA = 0, nodesB = 0, lmrReductions = 0, lmrResearches = 0;
    int movesA = 0, movesB = 0, depthsA = 0, depthsB = 0;
    std::vector<double> elapsedA, elapsedB;

    std::cout << "A: " << argv[1] << " (" << variantA << ")\nB: " << argv[2] << " (" << variantB << ")"
              << "\nGames: " << games << "  Time per move: " << moveTimeMs << " ms"
              << "  Fixed openings: " << fixedOpenings.size() << '\n';

    for (int game{ 0 }; game < games; game++) {
        bool aIsDark{ game % 2 == 0 };
        if (!fixedOpenings.empty()) {
            for (auto encoded : fixedOpenings[game / 2]) {
                const auto end = boardA.getMoves().begin() + boardA.getNumMoves();
                const auto found = std::find(boardA.getMoves().begin(), end, encoded);
                if (found == end) throw std::runtime_error("Fixed opening replay failed");
                const int move = static_cast<int>(found - boardA.getMoves().begin());
                if (boardA.makeMove(move) != boardB.makeMove(move) || boardA.searchHash() != boardB.searchHash())
                    throw std::runtime_error("Opening boards diverged");
            }
        }
        else if (aIsDark) {
            opening.clear();
            for (int turn{ 0 }; turn < openingTurns && boardA.getNumMoves() > 0 && !boardA.isDraw(); turn++) {
                bool switched;
                do {
                    std::uniform_int_distribution<int> dist(0, boardA.getNumMoves() - 1);
                    int move{ dist(openingRng) };
                    opening.push_back(move);
                    switched = boardA.makeMove(move);
                    boardB.makeMove(move);
                } while (!switched);
            }
        }
        else {
            for (int move : opening) {
                boardA.makeMove(move);
                boardB.makeMove(move);
            }
        }

        char result{ 'D' };
        while (!boardA.isDraw()) {
            bool aTurn{ boardA.isDarkTurn() == aIsDark };
            if (boardA.getNumMoves() == 0) {
                result = aTurn ? 'B' : 'A';
                break;
            }

            auto& player = aTurn ? playerA : playerB;
            const auto rootHash = boardA.searchHash();
            const auto searchStart = std::chrono::steady_clock::now();
            auto search{ player.search(moveTimeMs, false) };
            const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - searchStart).count();
            if (boardA.searchHash() != rootHash || boardB.searchHash() != rootHash)
                throw std::runtime_error("Search failed to restore match boards");
            (aTurn ? elapsedA : elapsedB).push_back(elapsed);
            if (moveTimings.is_open()) {
                moveTimings << std::fixed << std::setprecision(6) << game + 1 << ',' << game / 2 + 1 << ','
                    << (aTurn ? 'A' : 'B') << ',' << (aTurn ? variantA : variantB) << ',' << boardA.isDarkTurn() << ','
                    << std::popcount(boardA.getDarkPieces() | boardA.getLightPieces()) << ',' << std::popcount(boardA.getKingPieces()) << ','
                    << boardA.isCaptureMove(boardA.getMoves()[0]) << ',' << boardA.getNumMoves() << ',' << boardA.getDrawCounter() << ','
                    << search.completedDepth << ',' << player.getNodesHit() << ',' << elapsed << ',' << moveTimeMs << '\n';
            }
            if (aTurn) {
                nodesA += player.getNodesHit();
                depthsA += search.completedDepth;
                ++movesA;
                lmrReductions += player.getLMRReductions();
                lmrResearches += player.getLMRResearches();
            }
            else {
                nodesB += player.getNodesHit();
                depthsB += search.completedDepth;
                ++movesB;
            }
            if (search.pv.empty()) {
                std::cerr << "Empty move at game " << game + 1 << '\n';
                return 1;
            }

            bool switched{ false };
            for (int move : search.pv) {
                if (move < 0 || move >= boardA.getNumMoves() || move >= boardB.getNumMoves()) {
                    std::cerr << "Invalid move at game " << game + 1 << '\n';
                    return 1;
                }
                switched = boardA.makeMove(move);
                if (switched != boardB.makeMove(move) || boardA.hash() != boardB.hash()) {
                    std::cerr << "Boards diverged at game " << game + 1 << '\n';
                    return 1;
                }
            }
            if (!switched) {
                std::cerr << "Incomplete turn at game " << game + 1 << '\n';
                return 1;
            }
        }

        if (result == 'A') aWins++;
        else if (result == 'B') bWins++;
        else draws++;

        std::cout << "Game " << game + 1 << ": " << result
                  << "  A " << aWins << "  B " << bWins << "  Draw " << draws << std::endl;
        boardA.reset();
        boardB.reset();
        playerA.resetTT();
        playerB.resetTT();
    }

    double score{ (aWins + 0.5 * draws) / games };
    std::cout << "Final: A " << aWins << ", B " << bWins << ", draws " << draws
              << ", A score " << score * 100 << "%";
    if (score > 0 && score < 1)
        std::cout << ", observed Elo difference " << 400 * std::log10(score / (1 - score));
    std::cout << std::endl;
    std::cout << "Search totals: A nodes " << nodesA << ", B nodes " << nodesB
              << ", A mean depth " << (movesA ? static_cast<double>(depthsA) / movesA : 0)
              << ", B mean depth " << (movesB ? static_cast<double>(depthsB) / movesB : 0)
              << ", A LMR reductions " << lmrReductions << ", re-searches " << lmrResearches << '\n';
    for (auto* times : { &elapsedA, &elapsedB }) {
        if (times->empty()) continue;
        const double total = std::accumulate(times->begin(), times->end(), 0.0);
        std::sort(times->begin(), times->end());
        std::cout << "Elapsed search ms: " << (times == &elapsedA ? 'A' : 'B') << " total " << total
            << ", mean " << total / times->size() << ", median " << (*times)[(times->size() - 1) / 2]
            << ", p95 " << (*times)[static_cast<std::size_t>(std::ceil(0.95 * times->size())) - 1]
            << ", max " << times->back() << '\n';
    }
    if (moveTimings.is_open()) {
        moveTimings.close();
        if (!moveTimings) throw std::runtime_error("Failed writing move timing CSV");
    }
}
catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
