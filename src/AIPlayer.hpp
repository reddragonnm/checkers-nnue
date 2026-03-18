#pragma once

#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>
#include <map>

#include "Checkers.hpp"
#include "EGTB.hpp"

constexpr int infinity{ 10000 };
constexpr int searchAborted{ std::numeric_limits<int>::min() / 2 };

enum { TTExact, TTUpper, TTLower };

struct TTEntry {
    std::uint64_t key;
    int16_t score;
    int8_t move;
    int16_t depth;
    std::uint8_t flag;
};

constexpr int ttSize{ 1 << 24 };

class AIPlayer {
private:
    Checkers& m_board;
    EGTB& m_egtb;

    std::vector<TTEntry> tt;

    int m_nodesHit{ 0 };
    int m_hashCollisions{ 0 };
    int m_egtbHits{ 0 };
    bool m_stopSearch{ false };
    bool m_hasDeadline{ false };
    std::chrono::steady_clock::time_point m_deadline;

    bool shouldStop() {
        if (!m_hasDeadline || m_stopSearch)
            return m_stopSearch;

        if (std::chrono::steady_clock::now() >= m_deadline)
            m_stopSearch = true;

        return m_stopSearch;
    }

    int evaluate(Checkers& board) {
        int dark{ std::popcount(board.getDarkPieces()) +
                 std::popcount(board.getDarkPieces() & board.getKingPieces()) };
        int light{ std::popcount(board.getLightPieces()) +
                  std::popcount(board.getLightPieces() & board.getKingPieces()) };

        return board.isDarkTurn() ? (dark - light) : (light - dark);
    }

    int quiscence(int alpha, int beta, Checkers& board) {
        if (shouldStop())
            return searchAborted;

        int eval{ evaluate(board) };

        if (!board.isMidCapture()) {
            WDL result{ m_egtb.probe(board) };
            if (result != UNKNOWN) {
                eval = result == WIN ? infinity : (result == LOSS ? -infinity : 0);
                m_egtbHits++;
            }

            if (eval >= beta)
                return beta;
            alpha = std::max(alpha, eval);
        }

        const int numMoves{ board.getNumMoves() };
        if (numMoves == 0 || !board.isCaptureMove(board.getMoves()[0]))
            return eval;

        for (int i{ 0 }; i < numMoves; i++) {
            int score;
            if (board.makeMove(i)) {
                score = -quiscence(-beta, -alpha, board);
            }
            else {
                score = quiscence(alpha, beta, board);
            }
            board.undoMove();

            if (score == searchAborted)
                return searchAborted;

            if (score >= beta)
                return score;
            eval = std::max(eval, score);
            alpha = std::max(alpha, score);
        }
        return eval;
    }

    int negamax(int alpha, int beta, int depth, Checkers& board) {
        if (shouldStop())
            return searchAborted;

        std::uint64_t hash{ board.hash() };
        TTEntry& entry{ tt[hash & (ttSize - 1)] };

        if (entry.key != 0 && entry.key != hash)
            m_hashCollisions++;

        if (entry.key == hash && entry.depth >= depth) {
            if (entry.flag == TTExact)
                return entry.score;
            if (entry.flag == TTLower && entry.score >= beta)
                return entry.score;
            if (entry.flag == TTUpper && entry.score <= alpha)
                return entry.score;
        }

        int hashMove{ -1 };
        if (entry.key == hash)
            hashMove = entry.move;

        const int numMoves{ board.getNumMoves() };

        if (board.isDraw())
            return 0;
        if (numMoves == 0)
            return -infinity;
        if (depth == 0)
            return quiscence(alpha, beta, board);

        int alphaOrg{ alpha };
        int bestVal{ -infinity };
        int bestMove{ -1 };

        for (int pass{ 0 }; pass < 2; pass++) { // try best move from last depth first
            for (int i{ 0 }; i < numMoves; i++) {
                if (pass == 0 && i != hashMove)
                    continue;
                if (pass == 1 && i == hashMove)
                    continue;

                int score;
                if (board.makeMove(i)) { // turn switched
                    m_nodesHit++;
                    score = -negamax(-beta, -alpha, depth - 1, board);
                }
                else {
                    score = negamax(alpha, beta, depth, board);
                }

                board.undoMove();

                if (score == searchAborted)
                    return searchAborted;

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
        int movesMade{ 0 };

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

    bool aspirationWindowSearch(int depth, int& curScore) {
        int delta{ 50 };
        int alpha{ -infinity };
        int beta{ infinity };

        // if (depth >= 4 && std::abs(curScore) != infinity) {
        //     alpha = curScore - delta;
        //     beta = curScore + delta;
        // }

        while (true) {
            m_nodesHit = 0;
            m_hashCollisions = 0;
            m_egtbHits = 0;

            int result{ negamax(alpha, beta, depth, m_board) };

            if (result == searchAborted)
                return false;

            if (result <= alpha) {
                alpha -= delta;
                delta *= 2;
                // if (alpha <= -infinity) alpha = -infinity;
            }
            else if (result > beta) {
                beta += delta;
                delta *= 2;
                // if (beta >= infinity) beta = infinity;
            }
            else {
                curScore = result;
                return true;
            }
        }
    }

public:
    AIPlayer(Checkers& board, EGTB& egtb) : m_board(board), m_egtb(egtb), m_nodesHit(0), tt(ttSize, { 0, -1, 0, -1, 0 }) {}

    std::vector<int> search(int input = 10, bool depthInput = true, bool printInfo = false) {
        int score{ 0 };
        int d{ 1 };
        int completedDepth{ 0 };
        int stoppedDepth{ 0 };
        std::vector<int> completedPV;

        m_stopSearch = false;
        m_hasDeadline = !depthInput;

        if (depthInput) {
            for (; d <= input; d++) {
                if (!aspirationWindowSearch(d, score))
                    break;

                completedDepth = d;
                completedPV = extractPV();
            }
        }
        else {
            m_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(input);
            while (!shouldStop()) {
                stoppedDepth = d;
                if (!aspirationWindowSearch(d, score))
                    break;

                completedDepth = d;
                completedPV = extractPV();
                d++;

                // stop if terminal score reached
                if (std::abs(score) == infinity)
                    break;
            }
        }

        m_hasDeadline = false;

        if (completedPV.empty() && m_board.getNumMoves() > 0)
            completedPV.push_back(0);

        if (printInfo) {
            std::cout << "Evaluation: " << score << " Depth: " << completedDepth;
            if (!depthInput && m_stopSearch && stoppedDepth > completedDepth)
                std::cout << " StoppedAt: " << stoppedDepth;
            std::cout << " EGTB Hits: " << m_egtbHits << '\n';
        }

        return completedPV;
    }

    int getNodesHit() {
        return m_nodesHit;
    }

    int getHashCollisions() {
        return m_hashCollisions;
    }

    int getEgtbHits() {
        return m_egtbHits;
    }

    void resetTT() {
        std::fill(tt.begin(), tt.end(), TTEntry{ 0, -1, 0, -1, 0 });
    }
};
