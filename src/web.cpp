#include <cstdint>
#include <memory>
#include <vector>

#include "headers/AIPlayer.hpp"
#include "headers/Checkers.hpp"
#include "headers/EGTB.hpp"
#include "headers/NNUE.hpp"
#include "headers/NNUEInference.hpp"

namespace {
std::unique_ptr<NNUE> g_nnue;
std::unique_ptr<NNUEInference> g_nnueInference;
std::unique_ptr<Checkers> g_board;
std::unique_ptr<EGTB> g_egtb;
std::unique_ptr<AIPlayer> g_ai;

int g_lastScore{0};
int g_lastDepth{0};
long long g_lastTimeMs{0};

constexpr int ongoingState{0};
constexpr int humanWinState{1};
constexpr int aiWinState{2};
constexpr int drawState{3};

int g_humanSide{0}; // 0 for Dark, 1 for Light

int uiToBoardSquare(int uiSquare) {
    return 63 - uiSquare;
}

int boardToUiSquare(int boardSquare) {
    return 63 - boardSquare;
}

int currentGameState() {
    if (!g_board) {
        return ongoingState;
    }

    if (g_board->isDraw()) {
        return drawState;
    }

    if (g_board->getNumMoves() == 0) {
        // If it's Dark's turn and Dark has no moves, Light wins.
        // If Dark is human (0), then AI (Light) wins.
        // If Dark is AI (1), then human wins.
        if (g_board->isDarkTurn()) {
            return g_humanSide == 0 ? aiWinState : humanWinState;
        } else {
            return g_humanSide == 1 ? aiWinState : humanWinState;
        }
    }

    return ongoingState;
}
}

extern "C" {
int init_engine() {
    try {
        g_nnue = std::make_unique<NNUE>(std::vector<int>{128, 256, 32, 1});
        g_nnue->load("/nnue_best.bin");
        g_nnueInference = std::make_unique<NNUEInference>(*g_nnue);
        g_board = std::make_unique<Checkers>(g_nnueInference.get());
        g_egtb = std::make_unique<EGTB>();
        g_ai = std::make_unique<AIPlayer>(*g_board, *g_egtb, *g_nnueInference);
        return 1;
    } catch (...) {
        g_ai.reset();
        g_egtb.reset();
        g_board.reset();
        g_nnueInference.reset();
        g_nnue.reset();
        return 0;
    }
}

void reset_game(int humanSide) {
    if (!g_board || !g_ai) {
        return;
    }

    g_board->reset();
    g_ai->resetTT();
    g_humanSide = humanSide;
    g_lastScore = 0;
    g_lastDepth = 0;
    g_lastTimeMs = 0;
}

int get_turn() {
    return g_board && g_board->isDarkTurn() ? 0 : 1;
}

int get_human_side() {
    return g_humanSide;
}

int get_game_state() {
    return currentGameState();
}

int get_piece_at(int uiSquare) {
    if (!g_board || uiSquare < 0 || uiSquare >= 64) {
        return 0;
    }

    const std::uint64_t mask{1ULL << uiToBoardSquare(uiSquare)};
    const bool dark{static_cast<bool>(g_board->getDarkPieces() & mask)};
    const bool light{static_cast<bool>(g_board->getLightPieces() & mask)};
    const bool king{static_cast<bool>(g_board->getKingPieces() & mask)};

    if (!dark && !light) {
        return 0;
    }
    if (dark) {
        return king ? 2 : 1;
    }
    return king ? 4 : 3;
}

int get_legal_move_count_from(int uiFromSquare) {
    if (!g_board || uiFromSquare < 0 || uiFromSquare >= 64) {
        return 0;
    }

    int count{0};
    const int fromBoardSquare{uiToBoardSquare(uiFromSquare)};
    const auto& moves{g_board->getMoves()};
    for (int i{0}; i < g_board->getNumMoves(); ++i) {
        if (g_board->getFromSquare(moves[i]) == fromBoardSquare) {
            ++count;
        }
    }
    return count;
}

int get_legal_move_to(int uiFromSquare, int moveIndex) {
    if (!g_board || uiFromSquare < 0 || uiFromSquare >= 64 || moveIndex < 0) {
        return -1;
    }

    int currentIndex{0};
    const int fromBoardSquare{uiToBoardSquare(uiFromSquare)};
    const auto& moves{g_board->getMoves()};
    for (int i{0}; i < g_board->getNumMoves(); ++i) {
        if (g_board->getFromSquare(moves[i]) != fromBoardSquare) {
            continue;
        }
        if (currentIndex == moveIndex) {
            return boardToUiSquare(g_board->getToSquare(moves[i]));
        }
        ++currentIndex;
    }
    return -1;
}

int make_human_move(int uiFromSquare, int uiToSquare) {
    if (!g_board || !g_ai) {
        return 0;
    }

    // Check if it's actually human's turn
    int currentTurn = g_board->isDarkTurn() ? 0 : 1;
    if (currentTurn != g_humanSide) {
        return 0;
    }

    const int fromBoardSquare{uiToBoardSquare(uiFromSquare)};
    const int toBoardSquare{uiToBoardSquare(uiToSquare)};
    const auto& moves{g_board->getMoves()};

    for (int i{0}; i < g_board->getNumMoves(); ++i) {
        if (g_board->getFromSquare(moves[i]) != fromBoardSquare ||
            g_board->getToSquare(moves[i]) != toBoardSquare) {
            continue;
        }

        const bool turnSwitched{g_board->makeMove(i)};
        if (!turnSwitched) {
            return 2;
        }
        return currentGameState() == ongoingState ? 1 : currentGameState();
    }

    return 0;
}

int make_ai_move(int timeLimitMs) {
    if (!g_board || !g_ai || currentGameState() != ongoingState) {
        return currentGameState();
    }

    auto start = std::chrono::steady_clock::now();
    const SearchResult result{g_ai->search(timeLimitMs, false, false)};
    auto end = std::chrono::steady_clock::now();

    g_lastScore = result.score;
    g_lastDepth = result.completedDepth;
    g_lastTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    for (int moveIndex : result.pv) {
        g_board->makeMove(moveIndex);
    }

    return currentGameState();
}

int get_last_score() { return g_lastScore; }
int get_last_depth() { return g_lastDepth; }
int get_last_time() { return (int)g_lastTimeMs; }
int get_nodes_hit() { return g_ai ? g_ai->getNodesHit() : 0; }
}
