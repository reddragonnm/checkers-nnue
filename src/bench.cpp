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

    NNUE nnue{ {128, 256, 32, 1} };
    nnue.load("nnue_best_v2.bin");

    NNUEInference nnueInference{ nnue };

    Checkers board{ &nnueInference };
    AIPlayer ai{ board, egtb, nnueInference };

    std::cout
        << std::left
        << std::setw(8) << "Depth"
        << std::setw(15) << "Nodes"
        << std::setw(15) << "Time(ms)"
        << std::setw(15) << "NPS"
        << std::setw(15) << "TT Probes"
        << std::setw(15) << "TT Hits"
        << std::setw(15) << "TT Cutoffs"
        << std::setw(15) << "Collisions"
        << std::setw(15) << "EGTB Hits"
        << '\n';

    std::cout
        << "-----------------------------------------------------------------------------------------------------------------\n";

    for (int depth{ 1 }; depth <= maxDepth; depth++) {
        ai.resetTT();

        auto start{
            std::chrono::high_resolution_clock::now()
        };

        ai.search(depth);

        auto end{
            std::chrono::high_resolution_clock::now()
        };

        double time{
            std::chrono::duration<double, std::milli>(end - start).count()
        };

        int nodes{ ai.getNodesHit() };
        double nps{ nodes / (time / 1000.0) };

        std::cout
            << std::left
            << std::setw(8) << depth
            << std::setw(15) << nodes
            << std::setw(15) << time
            << std::setw(15) << static_cast<uint64_t>(nps)
            << std::setw(15) << ai.getTTProbes()
            << std::setw(15) << ai.getTTUsefulHits()
            << std::setw(15) << ai.getTTCutoffs()
            << std::setw(15) << ai.getHashCollisions()
            << std::setw(15) << ai.getEgtbHits()
            << '\n';

        if (depth == 16) {
            std::cout << "\n=== Depth 16 Detailed Statistics ===\n";

            std::cout
                << "Nodes searched      : "
                << nodes << '\n';

            std::cout
                << "TT useful hits      : "
                << ai.getTTUsefulHits()
                << " ("
                << std::fixed << std::setprecision(2)
                << (100.0 * ai.getTTUsefulHits() / nodes)
                << "%)\n";

            std::cout
                << "TT probes           : "
                << ai.getTTProbes()
                << " ("
                << (ai.getTTProbes() > 0
                    ? 100.0 * ai.getTTUsefulHits() / ai.getTTProbes()
                    : 0.0)
                << "% useful)\n";

            std::cout
                << "TT cutoffs          : "
                << ai.getTTCutoffs()
                << " ("
                << (100.0 * ai.getTTCutoffs() / nodes)
                << "%)\n";

            std::cout
                << "Hash collisions     : "
                << ai.getHashCollisions()
                << " ("
                << (100.0 * ai.getHashCollisions() / nodes)
                << "%)\n";

            std::cout
                << "NPS                 : "
                << static_cast<uint64_t>(nps)
                << '\n';

            std::cout << '\n';
        }
    }

    return 0;
}
