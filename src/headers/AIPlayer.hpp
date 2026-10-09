#pragma once

#include <cassert>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <iostream>
#include <limits>

#include "Checkers.hpp"
#include "EGTB.hpp"
#include "NNUEInference.hpp"

constexpr int infinity{ 300 };
constexpr int infinityThreshold{ 50 };
constexpr int searchAborted{ std::numeric_limits<int>::min() / 2 };

enum { TTExact, TTUpper, TTLower };

enum class MoveOrdering { TT, Tactical, Killers, History };
enum class LMRPolicy { Conservative, Late, EndgameGuard };

struct LMREndgameStats {
    int reductions{ 0 }, researches{ 0 };
    int reducedCutoffs{ 0 }, researchCutoffs{ 0 };
    // Inclusive call work overlaps when reduced searches nest.
    int reducedNodes{ 0 }, researchNodes{ 0 };
};

struct TTEntry {
    std::uint64_t key;
    int16_t score;
    int8_t move;
    int16_t depth;
    std::uint8_t flag;
};

struct SearchResult {
    int score;
    std::vector<int> pv;
    int completedDepth;
};

#ifdef __EMSCRIPTEN__
constexpr int ttSize{ 1 << 20 };
#else
constexpr int ttSize{ 1 << 24 };
#endif

class AIPlayer {
    friend struct AIPlayerTest;
private:
    Checkers& m_board;
    EGTB& m_egtb;
    NNUEInference& m_nnue;
    bool m_pieceCount;
    MoveOrdering m_ordering;
    bool m_useLMR;
    LMRPolicy m_lmrPolicy;
    static constexpr int maxKillerPly{ 256 };
    static constexpr int historyLimit{ 16384 };
    std::array<std::array<std::uint16_t, 2>, maxKillerPly> m_killers{};
    std::array<std::array<std::array<int, 64>, 64>, 2> m_history{};

    std::vector<TTEntry> tt;

    int m_nodesHit{ 0 };
    int m_hashCollisions{ 0 };
    int m_egtbHits{ 0 };

    int m_ttProbes{ 0 };      // table lookups
    int m_ttMatches{ 0 };     // matching hash, including shallow entries
    int m_ttUsefulHits{ 0 };  // depth sufficient
    int m_ttCutoffs{ 0 };     // returned early from TT
    int m_betaCutoffs{ 0 };
    int m_firstMoveCutoffs{ 0 };
    int m_lmrReductions{ 0 };
    int m_lmrResearches{ 0 };
    LMREndgameStats m_lmrEndgame{};

    struct RankedMove {
        int index;
        int score;
    };

    bool isPromotion(const Checkers& board, std::uint16_t move) const {
        return !(board.getKingPieces() & (1ULL << board.getFromSquare(move)))
            && (board.isDarkTurn() ? board.getToSquare(move) >= 56 : board.getToSquare(move) < 8);
    }

    bool shouldReduce(const Checkers& board, RankedMove move, int rank, int depth, int ply) const {
        return m_useLMR && ply > 0 && depth >= 4 && rank >= (m_lmrPolicy == LMRPolicy::Late ? 6 : 4)
            && (m_lmrPolicy != LMRPolicy::EndgameGuard || std::popcount(board.getDarkPieces() | board.getLightPieces()) > 5)
            && move.score < 1024
            && !board.isMidCapture() && !board.isCaptureMove(board.getMoves()[move.index])
            && !isPromotion(board, board.getMoves()[move.index]);
    }

    std::array<RankedMove, maxMovesSize> orderMoves(const Checkers& board, int hashMove, int ply) const {
        std::array<RankedMove, maxMovesSize> moves{};
        for (int i = 0; i < board.getNumMoves(); ++i) {
            const auto move = board.getMoves()[i];
            const int from = board.getFromSquare(move);
            const int to = board.getToSquare(move);
            int score = i == hashMove ? 1000000 : 0;
            if (m_ordering >= MoveOrdering::Tactical) {
                if (isPromotion(board, move))
                    score += 20000;
                if (board.isCaptureMove(move)) {
                    // shortcut: score single jumps; rank whole chains if tactical benchmarks justify it.
                    score += 1000;
                    if (board.getKingPieces() & (1ULL << ((from + to) / 2)))
                        score += 10000;
                }
                else {
                    if (m_ordering >= MoveOrdering::Killers && ply < maxKillerPly) {
                        if (move == m_killers[ply][0]) score += 30000;
                        else if (move == m_killers[ply][1]) score += 29000;
                    }
                    if (m_ordering >= MoveOrdering::History)
                        score += m_history[board.isDarkTurn()][from][to];
                }
            }
            moves[i] = { i, score };
        }
        // Keep generator indices intact for TT entries, PVs, and makeMove().
        std::sort(moves.begin(), moves.begin() + board.getNumMoves(), [](const auto& a, const auto& b) {
            return a.score != b.score ? a.score > b.score : a.index < b.index;
        });
        return moves;
    }

    void recordQuietCutoff(const Checkers& board, std::uint16_t move, int depth, int ply) {
        if (board.isCaptureMove(move)) return;
        if (m_ordering >= MoveOrdering::Killers && ply < maxKillerPly && m_killers[ply][0] != move) {
            m_killers[ply][1] = m_killers[ply][0];
            m_killers[ply][0] = move;
        }
        if (m_ordering >= MoveOrdering::History) {
            int& history = m_history[board.isDarkTurn()][board.getFromSquare(move)][board.getToSquare(move)];
            const int bonus = std::min(depth, 128) * std::min(depth, 128);
            // Bounded gravity prevents overflow and keeps learning after many cutoffs.
            history += bonus - history * bonus / historyLimit;
        }
    }

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
        if (m_pieceCount) {
            int dark{ std::popcount(board.getDarkPieces()) +
                      std::popcount(board.getDarkPieces() & board.getKingPieces()) };
            int light{ std::popcount(board.getLightPieces()) +
                       std::popcount(board.getLightPieces() & board.getKingPieces()) };
            return board.isDarkTurn() ? dark - light : light - dark;
        }
        float output{ m_nnue.forwardAccumulator(!board.isDarkTurn()) };
        return std::clamp(static_cast<int>(output * infinity), -infinity + infinityThreshold, infinity - infinityThreshold);
    }

    int probeTablebaseScore(Checkers& board) {
        if (board.hasRepeatedPosition())
            return searchAborted;

        if (std::popcount(board.getDarkPieces()) + std::popcount(board.getLightPieces()) > 5)
            return searchAborted;

        WDL result{ m_egtb.probe(board) };
        if (result == WDL::UNKNOWN)
            return searchAborted;

        m_egtbHits++;

        if (result == WDL::DRAW)
            return 0;

        int dtz{ m_egtb.probeDTZ(board) };
        if (dtz >= 0 && board.getDrawCounter() + dtz >= 80)
            return searchAborted;
        // After a capture or promotion, the table records distance to the next reset.
        int distance{ board.getDrawCounter() == 0 ? 1 : (dtz >= 0 ? dtz : (infinityThreshold - 1)) };
        distance = std::clamp(distance, 1, infinityThreshold - 1);

        if (result == WDL::WIN)
            return infinity - distance;

        return -infinity + distance;
    }

    int quiscence(int alpha, int beta, Checkers& board, int ply) {
        if (shouldStop())
            return searchAborted;

        if (board.isDraw())
            return 0;
        const int numMoves{ board.getNumMoves() };
        if (numMoves == 0)
            return -infinity + ply;

        int tbScore{ probeTablebaseScore(board) };
        if (tbScore != searchAborted)
            return tbScore;

        if (!board.isCaptureMove(board.getMoves()[0]))
            return evaluate(board);

        int eval{ -infinity };
        const auto moves = orderMoves(board, -1, ply);

        for (int rank{ 0 }; rank < numMoves; rank++) {
            const int i = moves[rank].index;
            int score;
            if (board.makeMove(i)) {
                m_nodesHit++;
                score = quiscence(-beta, -alpha, board, ply + 1);
                if (score != searchAborted) score = -score;
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
        // m_nodesHit++;
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

        std::uint64_t hash{ board.searchHash() };
        TTEntry& entry{ tt[hash & (ttSize - 1)] };
        m_ttProbes++;

        if (entry.key != 0 && entry.key != hash)
            m_hashCollisions++;

        if (entry.key == hash) {
            m_ttMatches++;
        }

        if (entry.key == hash && entry.depth >= depth) {
            m_ttUsefulHits++;

            int score{ entry.score };
            if (score > infinity - infinityThreshold) score -= ply;
            else if (score < -infinity + infinityThreshold) score += ply;

            if (entry.flag == TTExact || (entry.flag == TTLower && score >= beta)
                || (entry.flag == TTUpper && score <= alpha)) {
                m_ttCutoffs++;
                if (entry.move >= 0 && entry.move < board.getNumMoves())
                    pv.push_back(entry.move);
                return score;
            }
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

        const auto moves = orderMoves(board, hashMove, ply);
        for (int rank{ 0 }; rank < numMoves; rank++) {
            const int i = moves[rank].index;
            const auto move = board.getMoves()[i];
            const bool reduce = shouldReduce(board, moves[rank], rank, depth, ply);
            const bool endgameReduction = reduce && std::popcount(board.getDarkPieces() | board.getLightPieces()) <= 5;
            bool researched = false;

            std::vector<int> childPV;
            int score;
            bool turnSwitched{ board.makeMove(i) };

            if (turnSwitched) {
                m_nodesHit++;
                if (reduce) {
                    m_lmrReductions++;
                    const int reducedStart = m_nodesHit - 1;
                    score = negamax(-alpha - 1, -alpha, depth - 2, board, childPV, ply + 1);
                    if (score != searchAborted) score = -score;
                    if (endgameReduction) {
                        ++m_lmrEndgame.reductions;
                        m_lmrEndgame.reducedNodes += m_nodesHit - reducedStart;
                    }
                    if (score != searchAborted && score > alpha) {
                        researched = true;
                        m_lmrResearches++;
                        const int researchStart = m_nodesHit;
                        m_nodesHit++;
                        score = negamax(-beta, -alpha, depth - 1, board, childPV, ply + 1);
                        if (score != searchAborted) score = -score;
                        if (endgameReduction) {
                            ++m_lmrEndgame.researches;
                            m_lmrEndgame.researchNodes += m_nodesHit - researchStart;
                        }
                    }
                }
                else {
                    score = negamax(-beta, -alpha, depth - 1, board, childPV, ply + 1);
                    if (score != searchAborted) score = -score;
                }
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
            if (alpha >= beta) {
                if (endgameReduction) {
                    if (researched) ++m_lmrEndgame.researchCutoffs;
                    else ++m_lmrEndgame.reducedCutoffs;
                }
                m_betaCutoffs++;
                if (rank == 0) m_firstMoveCutoffs++;
                recordQuietCutoff(board, move, depth, ply);
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
        int applied = 0;
        bool switched = false;

        for (int m : pv) {
            switched = m_board.makeMove(m);
            applied++;
            if (switched) break;
        }

        while (!switched && m_board.getNumMoves() > 0) {
            int move = 0;
            std::uint64_t hash = m_board.searchHash();
            TTEntry& e = tt[hash & (ttSize - 1)];
            if (e.key == hash && e.move >= 0 && e.move < m_board.getNumMoves())
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
    AIPlayer(Checkers& board, EGTB& egtb, NNUEInference& nnue, bool pieceCount = false,
        MoveOrdering ordering = MoveOrdering::History, bool useLMR = false,
        LMRPolicy lmrPolicy = LMRPolicy::Conservative) : m_board(board), m_egtb(egtb), m_nnue(nnue),
        m_pieceCount(pieceCount), m_ordering(ordering), m_useLMR(useLMR), m_lmrPolicy(lmrPolicy), tt(ttSize, { 0, -1, 0, -1, 0 }) {}

    SearchResult search(int input = 10, bool depthInput = true, bool printInfo = false) {
        m_nodesHit = 0;
        m_egtbHits = 0;
        m_hashCollisions = 0;
        m_ttProbes = 0;
        m_ttMatches = 0;
        m_ttUsefulHits = 0;
        m_ttCutoffs = 0;
        m_betaCutoffs = 0;
        m_firstMoveCutoffs = 0;
        m_lmrReductions = 0;
        m_lmrResearches = 0;
        m_lmrEndgame = {};
        for (auto& side : m_history)
            for (auto& from : side)
                for (auto& value : from) value /= 2;

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

        return { score, completedPV, completedDepth };
    }

    int getNodesHit() const {
        return m_nodesHit;
    }

    int getHashCollisions() const {
        return m_hashCollisions;
    }

    int getEgtbHits() const {
        return m_egtbHits;
    }

    void resetTT() {
        std::fill(tt.begin(), tt.end(), TTEntry{ 0, -1, 0, -1, 0 });
        m_killers = {};
        m_history = {};
    }

    int getTTProbes() const {
        return m_ttProbes;
    }

    int getTTUsefulHits() const {
        return m_ttUsefulHits;
    }

    int getTTMatches() const { return m_ttMatches; }
    int getBetaCutoffs() const { return m_betaCutoffs; }
    int getFirstMoveCutoffs() const { return m_firstMoveCutoffs; }
    int getLMRReductions() const { return m_lmrReductions; }
    int getLMRResearches() const { return m_lmrResearches; }
    const LMREndgameStats& getLMREndgameStats() const { return m_lmrEndgame; }

    int getTTCutoffs() const {
        return m_ttCutoffs;
    }
};
