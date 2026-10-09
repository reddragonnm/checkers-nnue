#include <sstream>
#include "headers/AIPlayer.hpp"

struct AIPlayerTest {
    static void run(AIPlayer& ai) {
        Checkers quiet;
        const auto first = quiet.getMoves()[0];
        const auto second = quiet.getMoves()[1];
        assert(!ai.shouldReduce(quiet, { 0, 0 }, 4, 4, 1));
        ai.m_useLMR = true;
        assert(ai.shouldReduce(quiet, { 0, 0 }, 4, 4, 1));
        ai.m_lmrPolicy = LMRPolicy::Late;
        assert(!ai.shouldReduce(quiet, { 0, 0 }, 5, 4, 1));
        assert(ai.shouldReduce(quiet, { 0, 0 }, 6, 4, 1));
        ai.m_lmrPolicy = LMRPolicy::EndgameGuard;
        assert(ai.shouldReduce(quiet, { 0, 0 }, 4, 4, 1));
        ai.m_lmrPolicy = LMRPolicy::Conservative;
        assert(!ai.shouldReduce(quiet, { 0, 0 }, 0, 4, 1));
        assert(!ai.shouldReduce(quiet, { 0, 0 }, 4, 3, 1));
        assert(!ai.shouldReduce(quiet, { 0, 0 }, 4, 4, 0));
        for (int priority : { 1024, 20000, 29000, 30000, 1000000 })
            assert(!ai.shouldReduce(quiet, { 0, priority }, 4, 4, 1));
        ai.recordQuietCutoff(quiet, second, 4, 3);
        assert(ai.m_history[true][quiet.getFromSquare(second)][quiet.getToSquare(second)] == 16);
        assert(ai.m_history[false][quiet.getFromSquare(second)][quiet.getToSquare(second)] == 0);
        assert(ai.orderMoves(quiet, -1, 3)[0].index == 1);
        assert(ai.orderMoves(quiet, 0, 3)[0].index == 0);
        ai.recordQuietCutoff(quiet, first, 3, 3);
        assert(ai.m_killers[3][0] == first && ai.m_killers[3][1] == second);
        ai.recordQuietCutoff(quiet, first, 3, 3);
        assert(ai.m_killers[3][1] == second);
        assert(ai.orderMoves(quiet, -1, 256)[0].index == 0); // history, no killer slot
        for (int i = 0; i < 100000; ++i) ai.recordQuietCutoff(quiet, second, 200, 300);
        const int value = ai.m_history[true][quiet.getFromSquare(second)][quiet.getToSquare(second)];
        assert(value >= 0 && value <= AIPlayer::historyLimit);
        ai.resetTT();
        assert(ai.m_killers[3][0] == 0 && ai.m_killers[3][1] == 0);
        assert(ai.m_history[true][quiet.getFromSquare(second)][quiet.getToSquare(second)] == 0);

        Checkers captures{ 1ULL << 19, (1ULL << 26) | (1ULL << 28), 1ULL << 26, true };
        assert(captures.getNumMoves() == 2);
        const auto ranked = ai.orderMoves(captures, -1, 0);
        const auto capture = captures.getMoves()[ranked[0].index];
        assert(!ai.shouldReduce(captures, { ranked[0].index, 0 }, 4, 4, 1));
        assert(captures.getToSquare(capture) == 33); // jump over the king on 26
        ai.recordQuietCutoff(captures, capture, 4, 0);
        assert(ai.m_killers[0][0] == 0);
        assert(ai.m_history[true][19][33] == 0);

        for (bool dark : { false, true }) {
            Checkers promotions{ dark ? (1ULL << 49) | (1ULL << 26) : 1ULL << 60,
                dark ? 1ULL << 3 : (1ULL << 10) | (1ULL << 26), 0, dark };
            const auto moves = ai.orderMoves(promotions, -1, 0);
            const int to = promotions.getToSquare(promotions.getMoves()[moves[0].index]);
            assert(dark ? to >= 56 : to < 8);
            assert(!ai.shouldReduce(promotions, { moves[0].index, 0 }, 4, 4, 1));
        }

        // A king on the back rank is not a new promotion.
        Checkers kings{ 1ULL << 49, 1ULL << 3, 1ULL << 49, true };
        const auto kingMoves = ai.orderMoves(kings, -1, 0);
        for (int i = 0; i < kings.getNumMoves(); ++i) assert(kingMoves[i].score == 0);
        assert(ai.shouldReduce(kings, { kingMoves[0].index, 0 }, 4, 4, 1));
        ai.m_lmrPolicy = LMRPolicy::EndgameGuard;
        assert(!ai.shouldReduce(kings, { kingMoves[0].index, 0 }, 4, 4, 1));
        Checkers six{ (1ULL << 19) | (1ULL << 1) | (1ULL << 3),
            (1ULL << 58) | (1ULL << 60) | (1ULL << 62), 0, true };
        assert(ai.shouldReduce(six, { 0, 0 }, 4, 4, 1));
        Checkers five{ (1ULL << 19) | (1ULL << 1),
            (1ULL << 58) | (1ULL << 60) | (1ULL << 62), 0, true };
        assert(!ai.shouldReduce(five, { 0, 0 }, 4, 4, 1));
        ai.m_lmrPolicy = LMRPolicy::Conservative;
        ai.m_useLMR = false;
    }
};

int main() {
    EGTB egtb;
    NNUE nnue{ {128, 256, 32, 1} };
    NNUEInference inference{ nnue };
    Checkers board;
    AIPlayer ai{ board, egtb, inference, true };
    AIPlayerTest::run(ai);
    std::array<int, 3> reductions{}, researches{};

    // Compare exact scores and restored state across a deterministic opening.
    for (int position = 0; position < 8; ++position) {
        const auto hash = board.searchHash();
        int expected = 0;
        for (int stage = 0; stage < 7; ++stage) {
            AIPlayer player{ board, egtb, inference, true,
                stage >= 4 ? MoveOrdering::History : static_cast<MoveOrdering>(stage), stage >= 4,
                stage >= 4 ? static_cast<LMRPolicy>(stage - 4) : LMRPolicy::Conservative };
            const int depth = stage >= 4 ? 7 : 5;
            const auto result = player.search(depth);
            if (stage == 0) expected = result.score;
            assert((stage >= 4 || result.score == expected) && result.completedDepth == depth);
            assert(player.getLMRResearches() <= player.getLMRReductions());
            if (stage >= 4) {
                reductions[stage - 4] += player.getLMRReductions();
                researches[stage - 4] += player.getLMRResearches();
            }
            assert(board.searchHash() == hash);
            assert(player.getFirstMoveCutoffs() <= player.getBetaCutoffs());
            assert(player.getTTUsefulHits() <= player.getTTMatches());
            assert(player.getTTMatches() <= player.getTTProbes());
            bool switched = false;
            int applied = 0;
            for (int index : result.pv) {
                assert(!switched && index >= 0 && index < board.getNumMoves());
                switched = board.makeMove(index);
                ++applied;
            }
            assert(switched);
            while (applied-- > 0) board.undoMove();
            assert(board.searchHash() == hash);
        }
        if (board.getNumMoves() == 0) break;
        board.makeMove(position % board.getNumMoves());
    }
    for (int policy = 0; policy < 3; ++policy) assert(reductions[policy] > 0 && researches[policy] > 0);
    Checkers small{ (1ULL << 19) | (1ULL << 1), (1ULL << 60) | (1ULL << 62),
        (1ULL << 19) | (1ULL << 1) | (1ULL << 60) | (1ULL << 62), true };
    const auto smallHash = small.searchHash();
    std::ostringstream missingTableMessages;
    auto* originalOutput = std::cout.rdbuf(missingTableMessages.rdbuf());
    for (const auto policy : { LMRPolicy::Conservative, LMRPolicy::Late, LMRPolicy::EndgameGuard }) {
        AIPlayer player{ small, egtb, inference, true, MoveOrdering::History, true, policy };
        player.search(8);
        const auto stats = player.getLMREndgameStats();
        assert(small.searchHash() == smallHash);
        assert(stats.reductions == player.getLMRReductions());
        assert(stats.researches == player.getLMRResearches());
        assert(stats.reducedCutoffs == 0 && stats.researchCutoffs <= stats.researches);
        assert(stats.reducedNodes >= stats.reductions && stats.researchNodes >= stats.researches);
        if (policy == LMRPolicy::EndgameGuard) assert(stats.reductions == 0);
        else if (policy == LMRPolicy::Conservative) assert(stats.reductions > 0);
        player.resetTT();
        player.search(1);
        assert(player.getLMREndgameStats().reductions == 0 && player.getLMREndgameStats().researchNodes == 0);
    }
    std::cout.rdbuf(originalOutput);
    // Ordering must preserve a complete multi-jump PV at the root.
    Checkers chain{ (1ULL << 19) | (1ULL << 1) | (1ULL << 3),
        (1ULL << 28) | (1ULL << 46) | (1ULL << 58) | (1ULL << 60) | (1ULL << 62), 0, true };
    AIPlayer chainPlayer{ chain, egtb, inference, true, MoveOrdering::History, true };
    const auto hash = chain.searchHash();
    const auto result = chainPlayer.search(1);
    assert(result.pv.size() == 2 && chain.searchHash() == hash);
    assert(!chain.makeMove(result.pv[0]));
    assert(chain.makeMove(result.pv[1]));
    chain.undoMove();
    chain.undoMove();
    assert(chain.searchHash() == hash);
    assert(chainPlayer.getLMRReductions() == 0);
    const auto timedHash = board.searchHash();
    const auto timed = ai.search(1, false);
    assert(board.searchHash() == timedHash);
    assert(board.getNumMoves() == 0 || !timed.pv.empty());
    AIPlayer timedLMR{ board, egtb, inference, true, MoveOrdering::History, true };
    const auto lmrTimed = timedLMR.search(1, false);
    assert(board.searchHash() == timedHash && !lmrTimed.pv.empty());
    std::cout << "Move-ordering checks passed\n";
}
