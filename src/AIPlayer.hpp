#pragma once

#include <cassert>
#include <chrono>

#include "Checkers.hpp"

constexpr int infinity{10000};

enum { TTExact, TTUpper, TTLower };

struct TTEntry {
    std::uint64_t key;
    int depth;
    int score;
    int move;
    std::uint8_t flag;
};

constexpr int ttSize{1 << 22};
std::vector<TTEntry> tt(ttSize, {0, -1, 0, -1, 0});

class AIPlayer {
  private:
    Checkers& m_board;
    int m_nodesHit;

    int evaluate(Checkers& board) {
        int dark{std::popcount(board.getDarkPieces()) +
                 std::popcount(board.getDarkPieces() & board.getKingPieces())};
        int light{std::popcount(board.getLightPieces()) +
                  std::popcount(board.getLightPieces() & board.getKingPieces())};

        return board.isDarkTurn() ? (dark - light) : (light - dark);
    }

    int quiscence(int alpha, int beta, Checkers& board) {
        int eval{evaluate(board)};

        if (eval >= beta)
            return beta;
        alpha = std::max(alpha, eval);

        const int numMoves{board.getNumMoves()};
        if (numMoves == 0 || !board.isCaptureMove(board.getMoves()[0]))
            return eval;

        for (int i{0}; i < numMoves; i++) {
            int score;
            if (board.makeMove(i)) {
                score = -quiscence(-beta, -alpha, board);
            } else {
                score = quiscence(alpha, beta, board);
            }
            board.undoMove();

            if (score >= beta)
                return score;
            eval = std::max(eval, score);
            alpha = std::max(alpha, score);
        }
        return eval;
    }

    int negamax(int alpha, int beta, int depth, Checkers& board) {
        std::uint64_t hash{board.hash()};
        TTEntry& entry{tt[hash & (ttSize - 1)]};

        if (entry.key == hash && entry.depth >= depth) {
            if (entry.flag == TTExact)
                return entry.score;
            if (entry.flag == TTLower && entry.score >= beta)
                return entry.score;
            if (entry.flag == TTUpper && entry.score <= alpha)
                return entry.score;
        }

        int hashMove{-1};
        if (entry.key == hash)
            hashMove = entry.move;

        const int numMoves{board.getNumMoves()};

        if (board.isDraw())
            return 0;
        if (numMoves == 0)
            return -infinity + depth;
        if (depth == 0)
            return quiscence(alpha, beta, board);

        int alphaOrg{alpha};
        int bestVal{-infinity};
        int bestMove{-1};

        for (int pass{0}; pass < 2; pass++) { // try best move from last depth first
            for (int i{0}; i < numMoves; i++) {
                if (pass == 0 && i != hashMove)
                    continue;
                if (pass == 1 && i == hashMove)
                    continue;

                int score;
                if (board.makeMove(i)) { // turn switched
                    m_nodesHit++;
                    score = -negamax(-beta, -alpha, depth - 1, board);
                } else {
                    score = negamax(alpha, beta, depth - 1, board);
                }

                board.undoMove();

                if (score > bestVal) {
                    bestVal = score;
                    bestMove = i;
                }

                alpha = std::max(alpha, score);
                if (alpha >= beta)
                    break;
            }
        }

        if (entry.key != hash || entry.depth <= depth) {
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
        }
        return bestVal;
    }

    std::vector<int> extractPV() {
        std::vector<int> pv;
        int movesMade{0};

        while (true) {
            TTEntry& entry = tt[m_board.hash() & (ttSize - 1)];
            if (entry.key != m_board.hash() || entry.move == -1)
                break;

            int move = entry.move;
            if (move >= m_board.getNumMoves())
                break;

            pv.push_back(move);
            movesMade++;
            if (m_board.makeMove(move)) // if turn over
                break;
        }

        for (int i = 0; i < movesMade; i++)
            m_board.undoMove();

        return pv;
    }

    void aspirationWindowSearch(int depth, int& curScore) {
        int delta{50};
        int alpha{-infinity};
        int beta{-infinity};

        if (depth >= 4) {
            alpha = curScore - delta;
            beta = curScore + delta;
        }

        while (true) {
            m_nodesHit = 0;
            int result{negamax(alpha, beta, depth, m_board)};

            if (result <= alpha) {
                alpha -= delta;
                delta *= 2;
            } else if (result >= beta) {
                beta += delta;
                delta *= 2;
            } else {
                curScore = result;
                break;
            }
        }
    }

  public:
    AIPlayer(Checkers& board) : m_board(board), m_nodesHit(0) {}

    std::vector<int> search(int input = 10, bool depthInput = true) {
        int score{0};

        if (depthInput) {
            for (int d{1}; d <= input; d++)
                aspirationWindowSearch(d, score);
        } else {
            int d{1};
            auto start{std::chrono::high_resolution_clock::now()};
            while (std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::high_resolution_clock::now() - start)
                       .count() < input)
                aspirationWindowSearch(d++, score);
        }

        return extractPV();
    }

    int getNodesHit() {
        return m_nodesHit;
    }
};
