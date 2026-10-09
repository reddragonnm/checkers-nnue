#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string_view>

#include "headers/AIPlayer.hpp"

int main(int argc, char** argv) {
    int maxDepth{ 20 };
    const std::array<std::string_view, 4> names{ "tt", "tactical", "killers", "history" };
    const std::string_view stage = argc > 2 ? argv[2] : "history";
    if (argc > 1) {
        const std::string_view arg = argv[1];
        const auto [ptr, error] = std::from_chars(arg.data(), arg.data() + arg.size(), maxDepth);
        if (error != std::errc{} || ptr != arg.data() + arg.size() || maxDepth < 1 || maxDepth > 200) {
            std::cerr << "Depth must be an integer from 1 to 200\n";
            return 1;
        }
    }
    if (argc > 3 || (stage != "all" && std::find(names.begin(), names.end(), stage) == names.end())) {
        std::cerr << "Usage: bench [max-depth=20] [tt|tactical|killers|history|all]\n";
        return 1;
    }
    // Fail before buildOrLoad() can start generating missing tablebases.
    for (const auto* file : { "egtb.bin", "egtb_dtz.bin", "nnue_best_v2.bin" }) {
        if (!std::filesystem::exists(file)) {
            std::cerr << "Missing " << file << "; run bench from build/bin\n";
            return 1;
        }
    }
    EGTB egtb;
    egtb.buildOrLoad("egtb.bin", "egtb_dtz.bin");
    NNUE nnue{ {128, 256, 32, 1} };
    nnue.load("nnue_best_v2.bin");
    NNUEInference inference{ nnue };
    Checkers board{ &inference };
    std::vector<int> baselineScores(maxDepth + 1);

    std::cout << std::left
        << std::setw(10) << "Stage" << std::setw(7) << "Depth"
        << std::setw(13) << "Nodes" << std::setw(12) << "Time(ms)"
        << std::setw(12) << "NPS" << std::setw(8) << "Score"
        << std::setw(12) << "TT Probes" << std::setw(12) << "TT Matches"
        << std::setw(12) << "TT Hits" << std::setw(12) << "TT Cutoffs"
        << std::setw(12) << "Collisions" << std::setw(12) << "EGTB Hits"
        << std::setw(12) << "Beta Cuts" << std::setw(12) << "First Cuts"
        << std::setw(10) << "First %" << "TT Hit %\n";

    for (int s = 0; s < 4; ++s) {
        if (stage != "all" && stage != names[s]) continue;
        AIPlayer ai{ board, egtb, inference, false, static_cast<MoveOrdering>(s) };
        for (int depth = 1; depth <= maxDepth; ++depth) {
            ai.resetTT();
            const auto hash = board.searchHash();
            const auto start = std::chrono::steady_clock::now();
            const auto result = ai.search(depth);
            const double ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();
            if (board.searchHash() != hash || result.completedDepth != depth) {
                std::cerr << "Search did not complete or restore the board\n";
                return 1;
            }
            if (stage == "all") {
                if (s == 0) baselineScores[depth] = result.score;
                else if (result.score != baselineScores[depth]) {
                    std::cerr << "Score mismatch for " << names[s] << " at depth " << depth << '\n';
                    return 1;
                }
            }
            std::cout << std::left << std::fixed << std::setprecision(2)
                << std::setw(10) << names[s] << std::setw(7) << depth
                << std::setw(13) << ai.getNodesHit() << std::setw(12) << ms
                << std::setw(12) << static_cast<std::uint64_t>(ms > 0 ? ai.getNodesHit() * 1000.0 / ms : 0)
                << std::setw(8) << result.score
                << std::setw(12) << ai.getTTProbes() << std::setw(12) << ai.getTTMatches()
                << std::setw(12) << ai.getTTUsefulHits() << std::setw(12) << ai.getTTCutoffs()
                << std::setw(12) << ai.getHashCollisions() << std::setw(12) << ai.getEgtbHits()
                << std::setw(12) << ai.getBetaCutoffs() << std::setw(12) << ai.getFirstMoveCutoffs()
                << std::setw(10) << (ai.getBetaCutoffs() ? 100.0 * ai.getFirstMoveCutoffs() / ai.getBetaCutoffs() : 0)
                << (ai.getTTProbes() ? 100.0 * ai.getTTMatches() / ai.getTTProbes() : 0) << std::endl;
        }
    }
}
