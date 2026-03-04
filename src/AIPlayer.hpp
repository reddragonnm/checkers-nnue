#pragma once

#include <stack>
#include <cassert>

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
        std::uint64_t hash {curBoard.hash()};
        TTEntry& entry {tt[hash & (ttSize - 1)]};

        if (entry.key == hash && entry.depth >= depth) {
            if (entry.flag == TTExact) return entry.score;
            if (entry.flag == TTLower && entry.score >= beta) return entry.score;
            if (entry.flag == TTUpper && entry.score <= alpha) return entry.score;
        }

        Checkers board{ curBoard };
        const auto moves{ board.getMoves() };

        if (board.isDraw()) return 0;
        if (moves.empty()) return -infinity + depth;
        if (depth == 0) return evaluate(board);

        int alphaOrg{ alpha };
        int bestVal{ -infinity };
        int bestMove {-1};

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

            if (score > bestVal) {
                bestVal = score;
                bestMove = i;
            }

            alpha = std::max(alpha, score);
            if (alpha >= beta) break;
        }

        entry.key = hash;
        entry.depth = depth;
        entry.score = bestVal;
        entry.move = bestMove;

        if (bestVal <= alphaOrg)
            entry.flag = TTUpper; // at most this good, could be worse but we cut early
        else if (bestVal >= beta)
            entry.flag = TTLower; // at least this good, could be better but we cut early
        else
            entry.flag = TTExact; // actual evalutation reached

        return bestVal;
    }

    int getNodesHit() {
        return m_nodesHit;
    }
};
