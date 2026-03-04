#pragma once

#include <stack>
#include <iostream>

#include "Checkers.hpp"

constexpr int infinity{ 10000 };

enum {
    TTExact,
    TTUpper,
    TTLower
};

struct TTEntry {
    std::uint64_t key;
    int depth;
    int score;
    std::uint16_t move;
    std::uint8_t flag;
};

constexpr int ttSize{ 1 << 22 };
std::vector<TTEntry> tt(ttSize);

class AIPlayer {
private:
    Checkers& m_board;
    int m_nodesHit;

public:
    AIPlayer(Checkers& board) : m_board(board), m_nodesHit(0) {

    }

    std::vector<int> search(int maxDepth = 10) {
        m_nodesHit = 0;
        std::vector<int> bestPath;

        int bestScore{ -infinity };
        const auto moves{ m_board.getMoves() };

        std::stack<std::tuple<Checkers, int, std::vector<int>>> s;
        for (int i = moves.size() - 1; i >= 0; i--) {
            s.push({ m_board, i , {} });
        }

        int alpha{ -infinity };

        while (!s.empty()) {
            auto [board, moveIdx, curPath] = s.top();
            s.pop();

            curPath.push_back(moveIdx);

            if (board.makeMove(moveIdx)) {
                m_nodesHit++;
                int score{ -negamax(alpha, infinity, maxDepth - 1, board) };

                if (score > bestScore) {
                    bestScore = score;
                    bestPath = curPath;
                }

                alpha = std::max(alpha, score);
            }
            else {
                for (int i = board.getMoves().size() - 1; i >= 0; i--) {
                    s.push({ board, i, curPath });
                }
            }
        }

        return bestPath;
    }

    int evaluate(Checkers& board) {
        int dark{ std::popcount(board.getDarkPieces()) + std::popcount(board.getDarkPieces() & board.getKingPieces()) };
        int light{ std::popcount(board.getLightPieces()) + std::popcount(board.getLightPieces() & board.getKingPieces()) };

        return board.isDarkTurn() ? (dark - light) : (light - dark);
    }

    int negamax(int alpha, int beta, int depth, Checkers& curBoard) {
        Checkers board{ curBoard };
        const auto moves{ board.getMoves() };

        if (board.isDraw()) return 0;
        if (moves.empty()) return -infinity + depth;
        if (depth == 0) return evaluate(board);

        int bestVal{ -infinity };

        for (int i{ 0 }; i < moves.size(); i++) {
            int score;
            if (board.makeMove(i)) { // turn switched
                m_nodesHit++;
                score = -negamax(-beta, -alpha, depth - 1, board);
            }
            else {
                score = negamax(alpha, beta, depth, board);
            }

            board = curBoard;

            bestVal = std::max(bestVal, score);
            alpha = std::max(alpha, score);
            if (alpha >= beta) break;
        }
        return bestVal;
    }

    int getNodesHit() {
        return m_nodesHit;
    }
};
