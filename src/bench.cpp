#include <iostream>
#include <chrono>
#include <iomanip>
#include <cstring>

#include "NNUE.hpp"
#include "Checkers.hpp"
// #include "AIPlayer.hpp"
#include "EGTB.hpp"

#include "matchmaking/v1.hpp"
#include "matchmaking/v2.hpp"

// int main() {
//     int maxDepth{ 20 };

//     EGTB egtb;
//     egtb.buildOrLoad("egtb.bin");

//     Checkers board{};
//     AIPlayer ai{ board, egtb };

//     std::cout << std::left << std::setw(8) << "Depth" << std::setw(15) << "Nodes" << std::setw(15)
//         << "Time(ms)" << std::setw(15) << "NPS" << std::setw(15) << "Collisions" << std::setw(15) << "EGTB Hits"
//         << "\n";
//     std::cout << "------------------------------------------------------------\n";

//     for (int depth{ 1 }; depth <= maxDepth; depth++) {
//         ai.resetTT();
//         auto start{ std::chrono::high_resolution_clock::now() };
//         ai.search(depth);
//         auto end{ std::chrono::high_resolution_clock::now() };
//         double time{ std::chrono::duration<double, std::milli>(end - start).count() };
//         int nodes{ ai.getNodesHit() };
//         double nps{ nodes / (time / 1000.0) };
//         std::cout << std::left << std::setw(8) << depth << std::setw(15) << nodes << std::setw(15)
//             << time << std::setw(15) << static_cast<uint64_t>(nps) << std::setw(15)
//             << ai.getHashCollisions() << std::setw(15) << ai.getEgtbHits() << "\n";
//     }
// }

int main() {
    Checkers board{};
    EGTB egtb;
    egtb.buildOrLoad("egtb.bin");

    auto v1Player{ v1::AIPlayer(board, egtb) };
    auto v2Player{ v2::AIPlayer(board, egtb) };

    int v1Wins{ 0 };
    int v2Wins{ 0 };
    int draws{ 0 };

    for (int i{ 0 }; i < 1000; i++) {
        bool v1IsDark{ i % 2 == 0 };
        int numMoves{ 0 };

        while (true) {
            std::vector<int> moves;
            bool isV1Turn = (board.isDarkTurn() == v1IsDark);

            if (isV1Turn)
                moves = v1Player.search(100, false);
            else
                moves = v2Player.search(100, false);

            if (moves.empty()) {
                if (isV1Turn) v2Wins++;
                else v1Wins++;
                break;
            }

            for (int m : moves) board.makeMove(m);

            if (board.isDraw()) {
                draws++;
                break;
            }

            numMoves++;
            std::cout << "Game " << i + 1 << ": Move " << numMoves << "\n";
        }

        std::cout << "Game " << i + 1 << ": V1 Wins: " << v1Wins << " V2 Wins: " << v2Wins << " Draws: " << draws << "\n";
        board.reset();
        v1Player.resetTT();
        v2Player.resetTT();
    }
}
