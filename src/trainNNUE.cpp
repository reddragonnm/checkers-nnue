#include <iostream>
#include <random>
#include <bitset>

#include "headers/NNUE.hpp"
#include "headers/NNUEInference.hpp"
#include "headers/Checkers.hpp"
#include "headers/AIPlayer.hpp"

constexpr int bufferCapacity{ 200000 };
constexpr int warmupSize{ 20000 };

constexpr int batchSize{ 256 };
constexpr int trainStepsPerGame{ 32 };

constexpr int warmupSearchDepth{ 4 };
constexpr int trainSearchDepth{ 6 };

constexpr float exploreRate{ 0.15f };

constexpr float lrInitial{ 0.001f };
constexpr int lrDecayEvery{ 10000 };
constexpr float lrDecayFactor{ 0.9f };

constexpr int checkpointEvery{ 1000 };
constexpr int saveLatestEvery{ 100 };
constexpr int eloEvalGames{ 100 };

std::mt19937_64 rng{ 42 };

struct Data {
    std::bitset<128> features;
    float target;
};

struct Buffer {
    std::vector<Data> data;

    Buffer() {
        data.reserve(bufferCapacity);
    }

    void add(const std::bitset<128>& features, float target) {
        if (data.size() < bufferCapacity) {
            data.push_back({ features, target });
        }
        else {
            std::uniform_int_distribution<int> dist(0, bufferCapacity - 1);
            int idx{ dist(rng) };
            data[idx] = { features, target };
        }
    };

    const Data& sample() const {
        std::uniform_int_distribution<int> dist(0, data.size() - 1);
        return data[dist(rng)];
    }

    std::pair<Matrix, Matrix> sampleBatch(int batchSize) const {
        Matrix features(batchSize, 128);
        Matrix targets(batchSize, 1);

        for (int i{ 0 }; i < batchSize; i++) {
            const auto& [feat, target] { sample() };
            for (int j{ 0 }; j < 128; j++) {
                features(i, j) = feat[j] ? 1.f : 0.f;
            }
            targets(i, 0) = target;
        }

        return { features, targets };
    }

    int size() const {
        return data.size();
    }
};

NNUE loadNNUE(const std::string& dir) {
    std::ifstream infile{ dir + "/nnue_latest.bin", std::ios::binary };
    if (infile.good()) {
        NNUE nnue{ {128, 256, 32, 1} };
        nnue.load(dir + "/nnue_latest.bin");
        return nnue;
    }
    return NNUE{ {128, 256, 32, 1} };
}

std::bitset<128> encodeBoard(const Checkers& board) {
    std::bitset<128> features;

    if (board.isDarkTurn())
        features = NNUEInference::encodeBoard(board.getDarkPieces(), board.getLightPieces(), board.getKingPieces());
    else
        features = NNUEInference::encodeBoard(Checkers::flipBoard(board.getLightPieces()), Checkers::flipBoard(board.getDarkPieces()), Checkers::flipBoard(board.getKingPieces()));

    return features;
}

void eloCheck(const std::string& v1, const std::string& v2) {
    Checkers board{};

    EGTB egtb;
    egtb.buildOrLoad("egtb.bin");

    NNUE nnueV1{ {128, 256, 32, 1} }; nnueV1.load(v1);
    NNUE nnueV2{ {128, 256, 32, 1} }; nnueV2.load(v2);

    NNUEInference nnueInferenceV1{ nnueV1 };
    NNUEInference nnueInferenceV2{ nnueV2 };

    auto v1Player{ AIPlayer(board, egtb, nnueInferenceV1) };
    auto v2Player{ AIPlayer(board, egtb, nnueInferenceV2) };

    int v1Wins{ 0 };
    int v2Wins{ 0 };
    int draws{ 0 };

    for (int i{ 0 }; i < eloEvalGames; i++) {
        bool v1IsDark{ i % 2 == 0 };
        int numMoves{ 0 };

        while (true) {
            int score;
            std::vector<int> moves;
            bool isV1Turn = (board.isDarkTurn() == v1IsDark);

            if (isV1Turn)
                std::tie(score, moves) = v1Player.search(100, false);
            else
                std::tie(score, moves) = v2Player.search(100, false);

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

    float winRate{ (v1Wins + 0.5f * draws) / eloEvalGames };
    float eloDiff{ -400.f * std::log10((1.f / winRate) - 1.f) };
    std::cout << "ELO Difference vs previous checkpoint: " << eloDiff << "\n";
}

int main() {
    EGTB egtb;
    egtb.buildOrLoad("egtb.bin");

    NNUE nnue{ loadNNUE("checkpoints") };
    NNUEInference nnueInference{ nnue };

    Checkers board{};
    AIPlayer ai{ board, egtb, nnueInference };

    float lr{ lrInitial };
    Buffer buffer{}; // dark perspective only

    // warmup
    while (buffer.size() < warmupSize) {
        board.reset();

        while (!board.isDraw() && board.getNumMoves() > 0) {
            auto features{ encodeBoard(board) };

            int dark{ std::popcount(board.getDarkPieces()) +
                 std::popcount(board.getDarkPieces() & board.getKingPieces()) };
            int light{ std::popcount(board.getLightPieces()) +
                      std::popcount(board.getLightPieces() & board.getKingPieces()) };

            float target{ (dark - light) / 24.f };

            buffer.add(features, target);

            std::uniform_int_distribution<int> dist(0, board.getNumMoves() - 1);
            board.makeMove(dist(rng)); // all random moves so we don't care about capture sequences or even color to move
        }
    }
    std::cout << "Warmup complete, buffer: " << buffer.size() << " positions\n";

    int games{ nnue.trainGames };

    // main loop
    std::uniform_real_distribution<float> uniformDist(0.f, 1.f);
    while (true) {
        // self play game
        std::cout << "Starting game " << games + 1 << "\n";
        board.reset();
        ai.resetTT();

        while (board.getNumMoves() > 0 && !board.isDraw()) {
            auto features{ encodeBoard(board) };

            auto [score, pv] { ai.search(trainSearchDepth) };

            buffer.add(features, score / infinity);

            if (uniformDist(rng) < exploreRate) {
                while (true) {
                    std::uniform_int_distribution<int> dist(0, board.getNumMoves() - 1);
                    if (board.makeMove(dist(rng))) {
                        break;
                    }
                }
            }
            else {
                for (const auto& move : pv) {
                    board.makeMove(move);
                }
            }
        }

        std::cout << "Game " << games + 1 << " finished. Result: ";
        if (board.isDraw()) std::cout << "Draw\n";
        else if (board.isDarkTurn()) std::cout << "Light wins\n";
        else std::cout << "Dark wins\n";

        // training
        float avgMSE{ 0.f };
        for (int step{ 0 }; step < trainStepsPerGame; step++) {
            auto [features, targets] { buffer.sampleBatch(batchSize) };
            Matrix output{ nnue.forward(features) };

            auto diff{ output + (targets * -1.f) };
            diff.hadamard(diff);
            float mse{ diff.columnSum().transpose().columnSum()(0, 0) / batchSize };
            avgMSE += mse;

            Matrix error{ mseDiff(output, targets) };
            nnue.backward(error, lr);
        }
        nnue.trainGames++;

        avgMSE /= trainStepsPerGame;
        std::cout << "Training " << games + 1 << " complete. Avg MSE: " << avgMSE << "\n";

        // periodic stuff
        if (games % lrDecayEvery == 0) {
            lr *= lrDecayFactor;
            std::cout << "Step " << games << ", LR decayed to " << lr << "\n";
        }

        if (games % saveLatestEvery == 0) {
            nnue.save("checkpoints/nnue_latest.bin");
        }

        if (games % checkpointEvery == 0) {
            nnue.save("checkpoints/nnue_" + std::to_string(games) + ".bin");
            std::cout << "Checkpoint saved at step " << games << "\n";

            if (games > checkpointEvery) { // skip first
                std::cout << "Starting ELO evaluation\n";
                eloCheck("checkpoints/nnue_" + std::to_string(games) + ".bin", "checkpoints/nnue_" + std::to_string(games - checkpointEvery) + ".bin");
            }
        }

        nnueInference.updateWeights();
        games++;
    }
}
