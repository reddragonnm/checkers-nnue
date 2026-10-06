#include <iostream>
#include <random>
#include <bitset>
#include <cmath>
#include <fstream>
#include <optional>
#include <regex>
#include <string>

#include "headers/NNUE.hpp"
#include "headers/NNUEInference.hpp"
#include "headers/Checkers.hpp"
#include "headers/AIPlayer.hpp"

constexpr int bufferCapacity{ 200000 };
constexpr int warmupSize{ 20000 };

constexpr int batchSize{ 256 };
constexpr int trainStepsPerGame{ 32 };

constexpr int trainSearchDepth{ 6 };

constexpr float exploreRate{ 0.15f };
constexpr float gameResultWeight{ 0.25f };

constexpr float lr{ 0.001f };

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
    int index{ 0 };

    Buffer() {
        data.reserve(bufferCapacity);
    }

    int add(const std::bitset<128>& features, float target) {
        if (data.size() < bufferCapacity) {
            data.push_back({ features, target });
            return static_cast<int>(data.size()) - 1;
        }
        else {
            const int slot{ index };
            data[slot] = { features, target };
            index = (index + 1) % bufferCapacity;
            return slot;
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

void blendGameResult(Buffer& buffer, const std::vector<std::pair<int, bool>>& positions, float darkResult) {
    for (const auto& [slot, darkToMove] : positions) {
        float result{ darkToMove ? darkResult : -darkResult };
        buffer.data[slot].target = (1.f - gameResultWeight) * buffer.data[slot].target
            + gameResultWeight * result;
    }
}

NNUE loadNNUE(const std::string& dir, std::ostream* log = nullptr) {
    const fs::path latest{ fs::path(dir) / "nnue_latest.bin" };
    auto loadIfFinite = [log](const fs::path& path) -> std::optional<NNUE> {
        NNUE nnue{ { 128, 256, 32, 1 } };
        std::string reason{ "non-finite model" };
        try {
            nnue.load(path.string());
            if (nnue.isFinite()) return nnue;
        }
        catch (const std::runtime_error& e) { reason = e.what(); }
        std::cerr << "Skipping invalid checkpoint: " << path << " (" << reason << ")\n";
        if (log) *log << "invalid_checkpoint path=" << path << " reason=" << reason << std::endl;
        return std::nullopt;
    };
    if (fs::exists(latest)) {
        if (auto nnue = loadIfFinite(latest)) return std::move(*nnue);
    }
    std::vector<std::pair<int, fs::path>> checkpoints;
    const std::regex numbered{ R"(nnue_([0-9]+)\.bin)" };
    if (fs::exists(dir)) for (const auto& entry : fs::directory_iterator(dir)) {
        std::smatch match;
        const std::string name{ entry.path().filename().string() };
        if (std::regex_match(name, match, numbered))
            checkpoints.emplace_back(std::stoi(match[1].str()), entry.path());
    }
    std::sort(checkpoints.rbegin(), checkpoints.rend());
    for (const auto& [step, path] : checkpoints)
        if (auto nnue = loadIfFinite(path)) return std::move(*nnue);
    if (fs::exists(latest) || !checkpoints.empty())
        throw std::runtime_error("No finite NNUE checkpoint is available");
    return NNUE{ { 128, 256, 32, 1 } };
}

std::bitset<128> encodeBoard(const Checkers& board) {
    std::bitset<128> features;

    if (board.isDarkTurn())
        features = NNUEInference::encodeBoard(board.getDarkPieces(), board.getLightPieces(), board.getKingPieces());
    else
        features = NNUEInference::encodeBoard(Checkers::flipBoard(board.getLightPieces()), Checkers::flipBoard(board.getDarkPieces()), Checkers::flipBoard(board.getKingPieces()));

    return features;
}

void eloCheck(EGTB& egtb, const std::string& v1, const std::string& v2) {
    NNUE nnueV1{ { 128, 256, 32, 1 } }; nnueV1.load(v1);
    NNUE nnueV2{ { 128, 256, 32, 1 } }; nnueV2.load(v2);

    NNUEInference nnueInferenceV1{ nnueV1 };
    NNUEInference nnueInferenceV2{ nnueV2 };

    Checkers board{ &nnueInferenceV1 };
    Checkers board2{ &nnueInferenceV2 };
    auto v1Player{ AIPlayer(board, egtb, nnueInferenceV1) };
    auto v2Player{ AIPlayer(board2, egtb, nnueInferenceV2) };

    int v1Wins{ 0 };
    int v2Wins{ 0 };
    int draws{ 0 };
    std::mt19937_64 openingRng{ 42 };
    std::vector<int> opening;

    for (int i{ 0 }; i < eloEvalGames; i++) {
        bool v1IsDark{ i % 2 == 0 };
        int numMoves{ 0 };

        if (v1IsDark) {
            opening.clear();
            for (int turn{ 0 }; turn < 8 && board.getNumMoves() > 0 && !board.isDraw(); turn++) {
                bool switched;
                do {
                    std::uniform_int_distribution<int> dist(0, board.getNumMoves() - 1);
                    int move{ dist(openingRng) };
                    opening.push_back(move);
                    switched = board.makeMove(move);
                    board2.makeMove(move);
                } while (!switched);
            }
        }
        else {
            for (int move : opening) {
                board.makeMove(move);
                board2.makeMove(move);
            }
        }

        while (true) {
            if (board.isDraw()) {
                draws++;
                break;
            }
            bool isV1Turn = (board.isDarkTurn() == v1IsDark);

            SearchResult res;
            if (isV1Turn)
                res = v1Player.search(100, false);
            else
                res = v2Player.search(100, false);

            if (res.pv.empty()) {
                if (isV1Turn) v2Wins++;
                else v1Wins++;
                break;
            }

            for (int m : res.pv) {
                board.makeMove(m);
                board2.makeMove(m);
            }

            if (board.isDraw()) {
                draws++;
                break;
            }

            numMoves++;
            std::cout << "Game " << i + 1 << ": Move " << numMoves << "\n";
        }

        std::cout << "Game " << i + 1 << ": V1 Wins: " << v1Wins << " V2 Wins: " << v2Wins << " Draws: " << draws << "\n";
        board.reset();
        board2.reset();
        v1Player.resetTT();
        v2Player.resetTT();
    }

    float epsilon{ 1e-8f };
    float winRate{ (v1Wins + 0.5f * draws) / eloEvalGames };
    float eloDiff{ -400.f * std::log10((1.f / (winRate + epsilon)) - 1.f) };
    std::cout << "ELO Difference vs previous checkpoint: " << eloDiff << "\n";
}

float calculateMSE(const Matrix& output, const Matrix& target) {
    float totalError{ 0.f };
    int n = output.numRows();

    for (int i = 0; i < n; i++) {
        float error = output(i, 0) - target(i, 0);
        totalError += (error * error);
    }

    return totalError / n;
}

int main(int argc, char* argv[]) {
    if (argc != 3 || std::string(argv[1]) != "--run-dir") {
        std::cerr << "Usage: trainNNUE --run-dir <directory>\n";
        return 2;
    }
    const fs::path runDir{ fs::absolute(argv[2]) };
    const fs::path checkpointDir{ runDir / "checkpoints" };
    fs::create_directories(runDir);
    std::ofstream trainLog(runDir / "train.log", std::ios::app);
    if (!trainLog) throw std::runtime_error("Cannot open training log");
    auto logLine = [&](const std::string& line) {
        std::cout << line << '\n';
        trainLog << line << std::endl;
    };
    logLine("run_dir=" + runDir.string() + " selfplay_rng_seed=42 initial_weights=random_device architecture=128,256,32,1"
            + " depth=" + std::to_string(trainSearchDepth) + " lr=" + std::to_string(lr)
            + " batch=" + std::to_string(batchSize) + " steps_per_game=" + std::to_string(trainStepsPerGame)
            + " game_result_weight=" + std::to_string(gameResultWeight));

    EGTB egtb;
    egtb.buildOrLoad("egtb.bin", "egtb_dtz.bin");

    const bool freshRun{ !fs::exists(checkpointDir) || fs::is_empty(checkpointDir) };
    NNUE nnue{ loadNNUE(checkpointDir.string(), &trainLog) };
    if (freshRun) {
        nnue.save((checkpointDir / "nnue_0.bin").string());
        logLine("initial_checkpoint=" + (checkpointDir / "nnue_0.bin").string());
    }
    logLine("starting_step=" + std::to_string(nnue.trainGames));
    NNUEInference nnueInference{ nnue };

    Checkers board{ &nnueInference };
    AIPlayer ai{ board, egtb, nnueInference };

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

            float target{ board.isDarkTurn() ? (dark - light) / 24.f : (light - dark) / 24.f };

            buffer.add(features, target);

            std::uniform_int_distribution<int> dist(0, board.getNumMoves() - 1);
            board.makeMove(dist(rng)); // all random moves so we don't care about capture sequences or even color to move
        }
    }
    logLine("warmup_complete buffer=" + std::to_string(buffer.size()));

    // main loop
    std::uniform_real_distribution<float> uniformDist(0.f, 1.f);
    while (true) {
        // self play game
        std::cout << "Starting game " << nnue.trainGames << "\n";
        board.reset();
        ai.resetTT();
        std::vector<std::pair<int, bool>> gamePositions;

        while (board.getNumMoves() > 0 && !board.isDraw()) {
            auto features{ encodeBoard(board) };

            auto [score, pv, _] { ai.search(trainSearchDepth) };

            gamePositions.emplace_back(buffer.add(features, static_cast<float>(score) / infinity), board.isDarkTurn());

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

        std::cout << "Game " << nnue.trainGames << " finished. Result: ";
        if (board.isDraw()) std::cout << "Draw\n";
        else if (board.isDarkTurn()) std::cout << "Light wins\n";
        else std::cout << "Dark wins\n";
        std::cout << "Buffer size: " << buffer.size() << "\n";

        const float darkResult{ board.isDraw() ? 0.f : (board.isDarkTurn() ? -1.f : 1.f) };
        blendGameResult(buffer, gamePositions, darkResult);

        // training
        float avgMSE{ 0.f };
        int completedSteps{ 0 };
        for (int step{ 0 }; step < trainStepsPerGame; step++) {
            auto [features, targets] { buffer.sampleBatch(batchSize) };
            Matrix output;
            try {
                output = nnue.forward(features);
            }
            catch (const std::runtime_error& e) {
                logLine("numerical_issue game=" + std::to_string(nnue.trainGames)
                    + " step=" + std::to_string(step) + " stage=forward " + e.what());
                continue;
            }

            float mse{ calculateMSE(output, targets) };
            if (!std::isfinite(mse)) {
                int badOutput{ -1 }, badTarget{ -1 };
                float maxOutput{ 0.f }, maxTarget{ 0.f };
                for (int row{ 0 }; row < batchSize; row++) {
                    if (!std::isfinite(output(row, 0)) && badOutput < 0) badOutput = row;
                    else maxOutput = std::max(maxOutput, std::abs(output(row, 0)));
                    if (!std::isfinite(targets(row, 0)) && badTarget < 0) badTarget = row;
                    else maxTarget = std::max(maxTarget, std::abs(targets(row, 0)));
                }
                logLine("numerical_issue game=" + std::to_string(nnue.trainGames)
                    + " step=" + std::to_string(step) + " stage=loss mse=" + std::to_string(mse)
                    + " first_bad_output_row=" + std::to_string(badOutput)
                    + " first_bad_target_row=" + std::to_string(badTarget)
                    + " max_abs_output=" + std::to_string(maxOutput)
                    + " max_abs_target=" + std::to_string(maxTarget));
                continue;
            }

            Matrix error{ mseDiff(output, targets) };
            std::string failure;
            if (!nnue.backward(error, lr, &failure)) {
                logLine("numerical_issue game=" + std::to_string(nnue.trainGames)
                    + " step=" + std::to_string(step) + " stage=optimizer " + failure);
                continue;
            }
            avgMSE += mse;
            completedSteps++;
        }

        logLine("training game=" + std::to_string(nnue.trainGames)
            + " result=" + (board.isDraw() ? "draw" : (board.isDarkTurn() ? "light_win" : "dark_win"))
            + " avg_mse=" + (completedSteps ? std::to_string(avgMSE / completedSteps) : "n/a")
            + " completed_steps=" + std::to_string(completedSteps)
            + " skipped_steps=" + std::to_string(trainStepsPerGame - completedSteps)
            + " buffer=" + std::to_string(buffer.size()));

        // periodic stuff
        if (nnue.trainGames % saveLatestEvery == 0) {
            nnue.save((checkpointDir / "nnue_latest.bin").string());
            logLine("latest_checkpoint game=" + std::to_string(nnue.trainGames));
        }

        if (nnue.trainGames != 0 && nnue.trainGames % checkpointEvery == 0) {
            nnue.save((checkpointDir / ("nnue_" + std::to_string(nnue.trainGames) + ".bin")).string());
            logLine("numbered_checkpoint game=" + std::to_string(nnue.trainGames));
            std::cout << "Checkpoint saved at step " << nnue.trainGames << "\n";

            if (nnue.trainGames != checkpointEvery) { // skip first
                std::cout << "Starting ELO evaluation\n";
                eloCheck(egtb, (checkpointDir / ("nnue_" + std::to_string(nnue.trainGames) + ".bin")).string(),
                    (checkpointDir / ("nnue_" + std::to_string(nnue.trainGames - checkpointEvery) + ".bin")).string());
            }
        }
        std::cout << "\n";

        nnueInference.updateWeights();
        nnue.trainGames++;
    }
}
