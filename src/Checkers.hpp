#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <random>
#include <cassert>

constexpr int maxMovesSize{48};

constexpr std::uint64_t rt{0xfefefefefefefe};
constexpr std::uint64_t rt2{0xfcfcfcfcfcfc};

constexpr std::uint64_t lt{0x7f7f7f7f7f7f7f};
constexpr std::uint64_t lt2{0x3f3f3f3f3f3f};

constexpr std::uint64_t rb{0xfefefefefefefe00};
constexpr std::uint64_t rb2{0xfcfcfcfcfcfc0000};

constexpr std::uint64_t lb{0x7f7f7f7f7f7f7f00};
constexpr std::uint64_t lb2{0x3f3f3f3f3f3f0000};

using ZobristTable = std::array<std::array<std::uint64_t, 64>, 4>;

struct State {
    std::uint64_t dark;
    std::uint64_t light;
    std::uint64_t king;

    std::array<std::uint16_t, maxMovesSize> moves;
    int moveCounter;
    int drawCounter;
    bool darkTurn;
    bool midCapture;
    std::uint64_t hash;
};

class Checkers {
  private:
    std::uint64_t m_darkPieces{};
    std::uint64_t m_lightPieces{};
    std::uint64_t m_kingPieces{};

    // move/capture-x, from - xxxxxx (0 to 63), to - xxxxxx (0 to 63)
    std::array<std::uint16_t, maxMovesSize> m_moves{};
    int m_moveCounter{0};

    int m_drawCounter{0};
    bool m_darkTurn{true};
    bool m_midCapture{false};

    ZobristTable m_zobrist;
    std::uint64_t m_zobristSide;
    std::uint64_t m_zobristMidCapture;

    std::uint64_t m_hash;

    std::uint64_t m_unoccupied;
    std::uint64_t m_oppPieces;

    std::vector<State> m_history;

    void initZobrist() {
        std::mt19937_64 rng(42);

        for (int piece = 0; piece < 4; piece++) {
            for (int sq = 0; sq < 64; sq++) {
                m_zobrist[piece][sq] = rng();
            }
        }

        m_zobristSide = rng();
        m_zobristMidCapture = rng();
    }

    std::uint64_t computeHash() {
        std::uint64_t h = 0;
        for (int i = 0; i < 64; i++) {
            std::uint64_t mask{static_cast<std::uint64_t>(1) << i};

            if (mask & m_darkPieces) {
                if (mask & m_kingPieces)
                    h ^= m_zobrist[1][i];
                else
                    h ^= m_zobrist[0][i];
            }

            if (mask & m_lightPieces) {
                if (mask & m_kingPieces)
                    h ^= m_zobrist[3][i];
                else
                    h ^= m_zobrist[2][i];
            }
        }

        if (m_darkTurn)
            h ^= m_zobristSide;

        if (m_midCapture)
            h ^= m_zobristMidCapture;

        return h;
    }

    void generateRTMoves(std::uint64_t pieces) {
        std::uint64_t moves{(rt & pieces) << 7};
        moves &= m_unoccupied;

        while (moves) {
            int i{__builtin_ctzll(moves)};
            moves &= moves - 1;

            m_moves[m_moveCounter++] = i | ((i - 7) << 6);
        }
    }

    void generateLTMoves(std::uint64_t pieces) {
        std::uint64_t moves{(lt & pieces) << 9};
        moves &= m_unoccupied;

        while (moves) {
            int i{__builtin_ctzll(moves)};
            moves &= moves - 1;

            m_moves[m_moveCounter++] = i | ((i - 9) << 6);
        }
    }

    void generateRBMoves(std::uint64_t pieces) {
        std::uint64_t moves{(rb & pieces) >> 9};
        moves &= m_unoccupied;

        while (moves) {
            int i{__builtin_ctzll(moves)};
            moves &= moves - 1;

            m_moves[m_moveCounter++] = i | ((i + 9) << 6);
        }
    }

    void generateLBMoves(std::uint64_t pieces) {
        std::uint64_t moves{(lb & pieces) >> 7};
        moves &= m_unoccupied;

        while (moves) {
            int i{__builtin_ctzll(moves)};
            moves &= moves - 1;

            m_moves[m_moveCounter++] = i | ((i + 7) << 6);
        }
    }

    void generateRTCaptures(std::uint64_t pieces) {
        // TODO: check double rt instead of rt2 speed/space
        std::uint64_t check1{((rt2 & pieces) << 7) &
                             m_oppPieces}; // check opp piece is present after one diagonal
        std::uint64_t moves{(check1 << 7) &
                            m_unoccupied}; // then valid only after empty at 2 diagonal

        while (moves) {
            int i{__builtin_ctzll(moves)};
            moves &= moves - 1;

            m_moves[m_moveCounter++] = i | ((i - 14) << 6) | (1 << 12);
        }
    }

    void generateLTCaptures(std::uint64_t pieces) {
        std::uint64_t check1{((lt2 & pieces) << 9) & m_oppPieces};
        std::uint64_t moves{(check1 << 9) & m_unoccupied};

        while (moves) {
            int i{__builtin_ctzll(moves)};
            moves &= moves - 1;

            m_moves[m_moveCounter++] = i | ((i - 18) << 6) | (1 << 12);
        }
    }

    void generateRBCaptures(std::uint64_t pieces) {
        std::uint64_t check1{((rb2 & pieces) >> 9) & m_oppPieces};
        std::uint64_t moves{(check1 >> 9) & m_unoccupied};

        while (moves) {
            int i{__builtin_ctzll(moves)};
            moves &= moves - 1;

            m_moves[m_moveCounter++] = i | ((i + 18) << 6) | (1 << 12);
        }
    }

    void generateLBCaptures(std::uint64_t pieces) {
        std::uint64_t check1{((lb2 & pieces) >> 7) & m_oppPieces};
        std::uint64_t moves{(check1 >> 7) & m_unoccupied};

        while (moves) {
            int i{__builtin_ctzll(moves)};
            moves &= moves - 1;

            m_moves[m_moveCounter++] = i | ((i + 14) << 6) | (1 << 12);
        }
    }

    void regenCache() {
        m_unoccupied = ~(m_lightPieces | m_darkPieces);
        m_oppPieces = m_darkTurn ? m_lightPieces : m_darkPieces;
    }

    void generateMoves() {
        assert(m_hash == computeHash());
        regenCache();

        if (m_darkTurn) {
            generateRTCaptures(m_darkPieces);
            generateLTCaptures(m_darkPieces);

            generateLBCaptures(m_darkPieces & m_kingPieces);
            generateRBCaptures(m_darkPieces & m_kingPieces);

            if (m_moveCounter == 0) {
                generateRTMoves(m_darkPieces);
                generateLTMoves(m_darkPieces);

                generateLBMoves(m_darkPieces & m_kingPieces);
                generateRBMoves(m_darkPieces & m_kingPieces);
            }

        } else {
            generateLBCaptures(m_lightPieces);
            generateRBCaptures(m_lightPieces);

            generateRTCaptures(m_lightPieces & m_kingPieces);
            generateLTCaptures(m_lightPieces & m_kingPieces);

            if (m_moveCounter == 0) {
                generateLBMoves(m_lightPieces);
                generateRBMoves(m_lightPieces);

                generateRTMoves(m_lightPieces & m_kingPieces);
                generateLTMoves(m_lightPieces & m_kingPieces);
            }
        }
    }

  public:
    Checkers() {
        // initialise board
        m_darkPieces = 0xaa55aa;
        m_lightPieces = static_cast<std::uint64_t>(0x55aa55) << 40;

        initZobrist();
        m_hash = computeHash();
        generateMoves();

        m_history.reserve(256);
    }

    void undoMove() {
        if (!m_history.empty()) {
            const State& top{m_history.back()};

            m_darkPieces = top.dark;
            m_lightPieces = top.light;
            m_kingPieces = top.king;
            m_moves = top.moves;
            m_moveCounter = top.moveCounter;
            m_drawCounter = top.drawCounter;
            m_darkTurn = top.darkTurn;
            m_midCapture = top.midCapture;
            m_hash = top.hash;

            m_history.pop_back();
        }
    }

    bool makeMove(int moveIdx) {
        // TODO: make this cleaner

        m_history.emplace_back(m_darkPieces, m_lightPieces, m_kingPieces, m_moves, m_moveCounter,
                               m_drawCounter, m_darkTurn, m_midCapture, m_hash);

        if (m_midCapture) {
            m_midCapture = false;
            m_hash ^= m_zobristMidCapture;
        }

        auto idx{static_cast<std::uint64_t>(1)};
        auto move{m_moves[moveIdx]};

        auto f{getFromSquare(move)};
        auto t{getToSquare(move)};
        auto m{(f + t) / 2};

        auto frSq{idx << f};
        auto toSq{idx << t};
        auto midSq{idx << m};

        m_drawCounter++;
        m_moveCounter = 0;

        bool wasKing{static_cast<bool>(frSq & m_kingPieces)};

        if (isCaptureMove(move)) {
            m_drawCounter = 0;
            if (m_darkTurn) {
                m_darkPieces &= ~frSq;
                m_darkPieces |= toSq;

                m_lightPieces &= ~midSq; // remove captured piece

                m_hash ^= wasKing ? m_zobrist[1][f] : m_zobrist[0][f];
                m_hash ^= wasKing ? m_zobrist[1][t] : m_zobrist[0][t];
                m_hash ^= (midSq & m_kingPieces) ? m_zobrist[3][m] : m_zobrist[2][m];

                // captures to last row (making it king) and not already king (as toSq is not
                // updated yet for king)
                if ((toSq & 0xff00000000000000) && !wasKing) {
                    m_hash ^= m_zobrist[0][t];
                    m_hash ^= m_zobrist[1][t];

                    m_kingPieces |= toSq;
                    m_kingPieces &= ~midSq;

                    m_darkTurn = !m_darkTurn;
                    m_hash ^= m_zobristSide;
                    generateMoves();
                    return true; // if king is made capture chain is broken
                }

                // now check if this piece can capture any further (assuming not king - handled
                // separately)
                regenCache();
                generateRTCaptures(toSq);
                generateLTCaptures(toSq);
            } else {
                m_lightPieces &= ~frSq;
                m_lightPieces |= toSq;

                m_darkPieces &= ~midSq;

                m_hash ^= wasKing ? m_zobrist[3][f] : m_zobrist[2][f];
                m_hash ^= wasKing ? m_zobrist[3][t] : m_zobrist[2][t];
                m_hash ^= (midSq & m_kingPieces) ? m_zobrist[1][m] : m_zobrist[0][m];

                if ((toSq & 0xff) && !wasKing) { // first row
                    m_hash ^= m_zobrist[2][t];
                    m_hash ^= m_zobrist[3][t];

                    m_kingPieces |= toSq;
                    m_kingPieces &= ~midSq;

                    m_darkTurn = !m_darkTurn;
                    m_hash ^= m_zobristSide;
                    generateMoves();
                    return true;
                }

                regenCache();
                generateLBCaptures(toSq);
                generateRBCaptures(toSq);
            }

            m_kingPieces &= ~midSq;
            if (m_kingPieces & frSq) {
                m_kingPieces &= ~frSq;
                m_kingPieces |= toSq;

                // no regen cache because it doesn't depend on kingPieces
                if (m_darkTurn) {
                    generateLBCaptures(toSq);
                    generateRBCaptures(toSq);
                } else {
                    generateRTCaptures(toSq);
                    generateLTCaptures(toSq);
                }
            }

            if (m_moveCounter == 0) { // no more captures left
                m_darkTurn = !m_darkTurn;
                m_hash ^= m_zobristSide;
                generateMoves();
                return true;
            }
        } else {
            if (m_darkTurn) {
                m_darkPieces &= ~frSq;
                m_darkPieces |= toSq;

                m_hash ^= wasKing ? m_zobrist[1][f] : m_zobrist[0][f];
                m_hash ^= wasKing ? m_zobrist[1][t] : m_zobrist[0][t];

                if ((toSq & 0xff00000000000000) && !wasKing) {
                    m_drawCounter = 0;
                    m_kingPieces |= toSq;
                    m_hash ^= m_zobrist[0][t];
                    m_hash ^= m_zobrist[1][t];
                }
            } else {
                m_lightPieces &= ~frSq;
                m_lightPieces |= toSq;

                m_hash ^= wasKing ? m_zobrist[3][f] : m_zobrist[2][f];
                m_hash ^= wasKing ? m_zobrist[3][t] : m_zobrist[2][t];

                if ((toSq & 0xff) && !wasKing) {
                    m_drawCounter = 0;
                    m_kingPieces |= toSq;
                    m_hash ^= m_zobrist[2][t];
                    m_hash ^= m_zobrist[3][t];
                }
            }

            if (m_kingPieces & frSq) {
                m_kingPieces &= ~frSq;
                m_kingPieces |= toSq;
            }

            m_darkTurn = !m_darkTurn;
            m_hash ^= m_zobristSide;
            generateMoves();
            return true;
        }

        m_midCapture = true;
        m_hash ^= m_zobristMidCapture;

        assert(m_hash == computeHash());
        return false;
    }

    std::uint64_t hash() const {
        return m_hash;
    }

    bool isDraw() const {
        return m_drawCounter == 80;
    }

    bool isCaptureMove(std::uint16_t move) const {
        return move & (1 << 12);
    }

    int getFromSquare(std::uint16_t move) const {
        return static_cast<int>((move >> 6) & 0x3f);
    }

    int getToSquare(std::uint16_t move) const {
        return static_cast<int>(move & 0x3f);
    }

    const std::uint64_t& getDarkPieces() const {
        return m_darkPieces;
    }

    const std::uint64_t& getLightPieces() const {
        return m_lightPieces;
    }

    const std::uint64_t& getKingPieces() const {
        return m_kingPieces;
    }

    const std::array<std::uint16_t, maxMovesSize>& getMoves() const {
        return m_moves;
    }

    int getNumMoves() const {
        return m_moveCounter;
    }

    bool isDarkTurn() const {
        return m_darkTurn;
    }
};
