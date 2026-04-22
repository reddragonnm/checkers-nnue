#pragma once

#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>
#include <map>

#include "../headers/Checkers.hpp"
#include "../headers/EGTB.hpp"
#include "../headers/NNUE.hpp"
#include "../headers/NNUEInference.hpp"

namespace v2 {
    constexpr int infinity{ 300 };
    constexpr int infinityThreshold{ 50 };
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
        NNUEInference& m_nnue;

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
            // int dark{ std::popcount(board.getDarkPieces()) +
            //          std::popcount(board.getDarkPieces() & board.getKingPieces()) };
            // int light{ std::popcount(board.getLightPieces()) +
            //           std::popcount(board.getLightPieces() & board.getKingPieces()) };

            // return board.isDarkTurn() ? (dark - light) : (light - dark);

            float output{ m_nnue.forwardAccumulator(!board.isDarkTurn()) };
            return std::clamp(static_cast<int>(output * infinity), -infinity + infinityThreshold, infinity - infinityThreshold);
        }

        int probeTablebaseScore(Checkers& board) {
            if (std::popcount(board.getDarkPieces()) + std::popcount(board.getLightPieces()) > 5)
                return searchAborted;

            WDL result{ m_egtb.probe(board) };
            if (result == WDL::UNKNOWN)
                return searchAborted;

            m_egtbHits++;

            if (result == WDL::DRAW)
                return 0;

            int dtz{ m_egtb.probeDTZ(board) };
            int distance{ dtz >= 0 ? dtz : (infinityThreshold - 1) };
            distance = std::clamp(distance, 1, infinityThreshold - 1);

            if (result == WDL::WIN)
                return infinity - distance;

            return -infinity + distance;
        }

        int quiscence(int alpha, int beta, Checkers& board, int ply) {
            if (shouldStop())
                return searchAborted;

            int tbScore{ probeTablebaseScore(board) };
            if (tbScore != searchAborted)
                return tbScore;

            int eval{ evaluate(board) };
            if (eval >= beta)
                return beta;
            alpha = std::max(alpha, eval);

            const int numMoves{ board.getNumMoves() };
            if (!board.isCaptureMove(board.getMoves()[0]))
                return eval;

            if (board.isDraw())
                return 0;
            if (numMoves == 0)
                return -infinity + ply;

            for (int i{ 0 }; i < numMoves; i++) {
                int score;
                if (board.makeMove(i)) {
                    score = -quiscence(-beta, -alpha, board, ply + 1);
                }
                else {
                    score = quiscence(alpha, beta, board, ply);
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

        int negamax(int alpha, int beta, int depth, Checkers& board, std::vector<int>& pv, int ply = 0) {
            pv.clear();

            if (shouldStop())
                return searchAborted;

            if (board.isDraw())
                return 0;

            if (ply > 0) {
                int tbScore{ probeTablebaseScore(board) };
                if (tbScore != searchAborted)
                    return tbScore;
            }

            std::uint64_t hash{ board.hash() };
            TTEntry& entry{ tt[hash & (ttSize - 1)] };

            if (entry.key != 0 && entry.key != hash)
                m_hashCollisions++;

            if (entry.key == hash && entry.depth >= depth) {
                int score{ entry.score };
                if (score > infinity - infinityThreshold) score -= ply;
                else if (score < -infinity + infinityThreshold) score += ply;

                if (entry.flag == TTExact)
                    return score;
                if (entry.flag == TTLower && score >= beta)
                    return score;
                if (entry.flag == TTUpper && score <= alpha)
                    return score;
            }

            int hashMove{ -1 };
            if (entry.key == hash)
                hashMove = entry.move;

            const int numMoves{ board.getNumMoves() };

            if (numMoves == 0)
                return -infinity + ply;
            if (depth == 0)
                return quiscence(alpha, beta, board, ply);

            int alphaOrg{ alpha };
            int bestVal{ -infinity };
            int bestMove{ -1 };

            for (int pass{ 0 }; pass < 2; pass++) { // try best move from last depth first
                for (int i{ 0 }; i < numMoves; i++) {
                    if (pass == 0 && i != hashMove)
                        continue;
                    if (pass == 1 && i == hashMove)
                        continue;

                    std::vector<int> childPV;
                    int score;
                    bool turnSwitched{ board.makeMove(i) };

                    if (turnSwitched) {
                        m_nodesHit++;
                        score = -negamax(-beta, -alpha, depth - 1, board, childPV, ply + 1);
                    }
                    else {
                        score = negamax(alpha, beta, depth, board, childPV, ply);
                    }

                    board.undoMove();

                    if (score == searchAborted)
                        return searchAborted;

                    if (score > bestVal) {
                        bestVal = score;
                        bestMove = i;
                        pv = { i };
                        if (!turnSwitched)
                            pv.insert(pv.end(), childPV.begin(), childPV.end());
                    }

                    alpha = std::max(alpha, score);
                    if (alpha >= beta)
                        break;
                }
            }

            int val{ bestVal };
            if (val > infinity - infinityThreshold) val += ply;
            else if (val < -infinity + infinityThreshold) val -= ply;

            if (entry.key != hash || entry.depth <= depth) {
                entry.key = hash;
                entry.depth = depth;
                entry.score = val;
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

        void ensureCompleteTurn(std::vector<int>& pv) {
            bool turnBefore = m_board.isDarkTurn();
            int applied = 0;
            bool switched = false;

            for (int m : pv) {
                switched = m_board.makeMove(m);
                applied++;
                if (switched) break;
            }

            while (!switched && m_board.getNumMoves() > 0) {
                int move = 0;
                TTEntry& e = tt[m_board.hash() & (ttSize - 1)];
                if (e.key == m_board.hash() && e.move >= 0 && e.move < m_board.getNumMoves())
                    move = e.move;
                pv.push_back(move);
                switched = m_board.makeMove(move);
                applied++;
            }

            for (int i = 0; i < applied; i++) m_board.undoMove();
        }

        bool aspirationWindowSearch(int depth, int& curScore, std::vector<int>& pv) {
            int delta = 50;
            int alpha = curScore - delta;
            int beta = curScore + delta;

            m_nodesHit = 0;
            m_egtbHits = 0;
            m_hashCollisions = 0;

            if (depth < 4) {
                alpha = -2 * infinity;
                beta = 2 * infinity;
            }

            while (true) {
                std::vector<int> localPV;
                int result = negamax(alpha, beta, depth, m_board, localPV);

                if (result == searchAborted) return false;

                if (result <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(result - delta, -2 * infinity);
                }
                else if (result >= beta) {
                    alpha = (alpha + beta) / 2;
                    beta = std::min(result + delta, 2 * infinity);
                }
                else {
                    curScore = result;
                    pv = localPV;
                    return true;
                }

                delta += delta / 2;
            }
        }

    public:
        AIPlayer(Checkers& board, EGTB& egtb, NNUEInference& nnue) : m_board(board), m_egtb(egtb), m_nnue(nnue), m_nodesHit(0), tt(ttSize, { 0, -1, 0, -1, 0 }) {}

        std::pair<int, std::vector<int>> search(int input = 10, bool depthInput = true, bool printInfo = false) {
            int score{ 0 };
            int d{ 1 };
            int completedDepth{ 0 };
            int stoppedDepth{ 0 };
            std::vector<int> completedPV;

            m_stopSearch = false;
            m_hasDeadline = !depthInput;

            if (depthInput) {
                for (; d <= input; d++) {
                    if (!aspirationWindowSearch(d, score, completedPV))
                        break;

                    completedDepth = d;
                }
            }
            else {
                m_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(input);
                while (!shouldStop() && d <= 200) {
                    stoppedDepth = d;
                    if (!aspirationWindowSearch(d, score, completedPV))
                        break;

                    completedDepth = d;
                    d++;
                }
            }

            m_hasDeadline = false;

            if (completedPV.empty() && m_board.getNumMoves() > 0)
                completedPV.push_back(0);

            ensureCompleteTurn(completedPV);

            if (printInfo) {
                std::cout << "Evaluation: " << score << " Depth: " << completedDepth;
                if (!depthInput && m_stopSearch && stoppedDepth > completedDepth)
                    std::cout << " StoppedAt: " << stoppedDepth;
                std::cout << " EGTB Hits: " << m_egtbHits << '\n';
            }

            return { score, completedPV };
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

}
