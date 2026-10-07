#include <charconv>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include <string_view>
#include <vector>

#include "headers/AIPlayer.hpp"

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: headlessMatch <model-A.bin> <model-B.bin|piececount> [games]\n";
        return 1;
    }

    int games{ 200 };
    if (argc == 4) {
        const char* end{ argv[3] + std::strlen(argv[3]) };
        auto [ptr, error] { std::from_chars(argv[3], end, games) };
        if (error != std::errc{} || ptr != end || games <= 0) {
            std::cerr << "Games must be a positive integer\n";
            return 1;
        }
    }
    constexpr int moveTimeMs{ 100 };
    constexpr int openingTurns{ 8 };

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
    AIPlayer playerA{ boardA, egtb, inferenceA };
    AIPlayer playerB{ boardB, egtb, inferenceB, pieceCountB };

    std::mt19937_64 openingRng{ 42 };
    std::vector<int> opening;
    int aWins{ 0 }, bWins{ 0 }, draws{ 0 };

    std::cout << "A: " << argv[1] << "\nB: " << argv[2]
              << "\nGames: " << games << "  Time per move: " << moveTimeMs << " ms\n";

    for (int game{ 0 }; game < games; game++) {
        bool aIsDark{ game % 2 == 0 };
        if (aIsDark) {
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

            auto search{ aTurn ? playerA.search(moveTimeMs, false) : playerB.search(moveTimeMs, false) };
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
}
