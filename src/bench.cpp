#include <iostream>
#include <chrono>
#include <iomanip>
#include <cstring>

#include "headers/NNUE.hpp"
#include "headers/Checkers.hpp"
#include "headers/AIPlayer.hpp"
#include "headers/EGTB.hpp"
#include "headers/NNUE.hpp"
#include "headers/NNUEInference.hpp"

int main() {
    int maxDepth{ 20 };

    EGTB egtb;
    egtb.buildOrLoad("egtb.bin", "egtb_dtz.bin");

    NNUE nnue{ {128, 256, 32, 1} }; nnue.load("nnue_best.bin");
    NNUEInference nnueInference{ nnue };

    Checkers board{ &nnueInference };
    AIPlayer ai{ board, egtb, nnueInference };

    std::cout << std::left << std::setw(8) << "Depth" << std::setw(15) << "Nodes" << std::setw(15)
        << "Time(ms)" << std::setw(15) << "NPS" << std::setw(15) << "Collisions" << std::setw(15) << "EGTB Hits"
        << "\n";
    std::cout << "------------------------------------------------------------\n";

    for (int depth{ 1 }; depth <= maxDepth; depth++) {
        ai.resetTT();
        auto start{ std::chrono::high_resolution_clock::now() };
        ai.search(depth);
        auto end{ std::chrono::high_resolution_clock::now() };
        double time{ std::chrono::duration<double, std::milli>(end - start).count() };
        int nodes{ ai.getNodesHit() };
        double nps{ nodes / (time / 1000.0) };
        std::cout << std::left << std::setw(8) << depth << std::setw(15) << nodes << std::setw(15)
            << time << std::setw(15) << static_cast<uint64_t>(nps) << std::setw(15)
            << ai.getHashCollisions() << std::setw(15) << ai.getEgtbHits() << "\n";
    }
}
