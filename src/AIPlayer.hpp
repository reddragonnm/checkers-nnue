#pragma once

#include <stack>
#include "Checkers.hpp"

constexpr int infinity{ 10000 };

class AIPlayer {
private:
    bool m_darkPlayer;
    Checkers& m_board;
    std::stack<int> m_moves;

public:
    AIPlayer(Checkers& board, bool darkPlayer) : m_board(board), m_darkPlayer(darkPlayer) {

    }

    void makeMove() {
        int maxDepth{ 5 };
        int bestMoveIdx{ 0 };
        int bestScore{ -infinity };

        Checkers board{ m_board };
        const auto moves{ board.getMoves() };


        for (int i{ 0 }; i < moves.size(); i++) {
            int score;
            if (board.makeMove(i)) {
                score = -negamax(-infinity, infinity, maxDepth - 1, board);
            }
            else {
                score = negamax(-infinity, infinity, maxDepth, board);
            }

            board = m_board;

            if (score > bestScore) {
                bestScore = score;
                bestMoveIdx = i;
            }
        }

        m_board.makeMove(bestMoveIdx);
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
                score = -negamax(-beta, -alpha, depth - 1, board);
            }
            else {
                score = negamax(alpha, beta, depth, board);
            }

            board = curBoard;

            bestVal = std::max(bestVal, score);
            if (score > alpha) alpha = score;
            if (alpha >= beta) break;
        }
        return bestVal;
    }
};
