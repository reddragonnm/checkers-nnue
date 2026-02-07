#pragma once

#include <cstdint>
#include <vector>

constexpr std::uint64_t rt{ 0xfefefefefefefe };
constexpr std::uint64_t rt2{ 0xfcfcfcfcfcfc };

constexpr std::uint64_t lt{ 0x7f7f7f7f7f7f7f };
constexpr std::uint64_t lt2{ 0x3f3f3f3f3f3f };

constexpr std::uint64_t rb{ 0xfefefefefefefe00 };
constexpr std::uint64_t rb2{ 0xfcfcfcfcfcfc0000 };

constexpr std::uint64_t lb{ 0x7f7f7f7f7f7f7f00 };
constexpr std::uint64_t lb2{ 0x3f3f3f3f3f3f0000 };

class Checkers {
private:
    std::uint64_t m_darkPieces{};
    std::uint64_t m_lightPieces{};
    std::uint64_t m_kingPieces{  };

    // move/capture-x, from - xxxxxx (0 to 63), to - xxxxxx (0 to 63)
    std::vector<std::uint16_t> m_moves{};

    bool m_darkTurn{ true };

public:
    Checkers() {
        // initialise board
        m_darkPieces = 0xaa55aa;
        m_lightPieces = static_cast<std::uint64_t>(0x55aa55) << 40;
    }

    void generateRTMoves(std::uint64_t pieces) {
        std::uint64_t moves{ (rt & pieces) << 7 };
        std::uint64_t validMoves{ moves & ~(m_lightPieces | m_darkPieces) };

        for (int i{ 0 }; i < 64; ++i) {
            if ((static_cast<std::uint64_t>(1) << i) & validMoves) {
                m_moves.push_back(i | (static_cast<std::uint64_t>(i - 7) << 6));
            }
        }
    }

    void generateLTMoves(std::uint64_t pieces) {
        std::uint64_t moves{ (lt & pieces) << 9 };
        std::uint64_t validMoves{ moves & ~(m_lightPieces | m_darkPieces) };

        for (int i{ 0 }; i < 64; i++) {
            if ((static_cast<std::uint64_t>(1) << i) & validMoves) {
                m_moves.push_back(i | (static_cast<std::uint64_t>(i - 9) << 6));
            }
        }
    }


    void generateRBMoves(std::uint64_t pieces) {
        std::uint64_t moves{ (rb & pieces) >> 9 };
        std::uint64_t validMoves{ moves & ~(m_lightPieces | m_darkPieces) };

        for (int i{ 0 }; i < 64; i++) {
            if ((static_cast<std::uint64_t>(1) << i) & validMoves) {
                m_moves.push_back(i | (static_cast<std::uint64_t>(i + 9) << 6));
            }
        }
    }

    void generateLBMoves(std::uint64_t pieces) {
        std::uint64_t moves{ (lb & pieces) >> 7 };
        std::uint64_t validMoves{ moves & ~(m_lightPieces | m_darkPieces) };

        for (int i{ 0 }; i < 64; i++) {
            if ((static_cast<std::uint64_t>(1) << i) & validMoves) {
                m_moves.push_back(i | (static_cast<std::uint64_t>(i + 7) << 6));
            }
        }
    }

    void generateMoves() {
        m_moves.clear();

        // generate movement moves
        if (m_darkTurn) {
            m_kingPieces |= m_darkPieces & 0xff00000000000000; // last row

            generateRTMoves(m_darkPieces);
            generateLTMoves(m_darkPieces);

            generateLBMoves(m_darkPieces & m_kingPieces);
            generateRBMoves(m_darkPieces & m_kingPieces);

        }
        else {
            m_kingPieces |= m_lightPieces & 0xff;

            generateLBMoves(m_lightPieces);
            generateRBMoves(m_lightPieces);

            generateRTMoves(m_lightPieces & m_kingPieces);
            generateLTMoves(m_lightPieces & m_kingPieces);

        }
    }

    void makeMove(int moveIdx) {
        if (isCaptureMove(m_moves[moveIdx])) {

        }
        else {
            auto frSq{ getFromSquare(m_moves[moveIdx]) };
            auto toSq{ getToSquare(m_moves[moveIdx]) };

            auto idx{ static_cast<std::uint64_t>(1) };

            if (m_darkTurn) {
                m_darkPieces &= ~(idx << frSq);
                m_darkPieces |= (idx << toSq);
            }
            else {
                m_lightPieces &= ~(idx << frSq);
                m_lightPieces |= (idx << toSq);
            }

            if (m_kingPieces & (idx << frSq)) {
                m_kingPieces &= ~(idx << frSq);
                m_kingPieces |= (idx << toSq);
            }

            m_darkTurn = !m_darkTurn;
            generateMoves();
        }
    }

    bool isCaptureMove(std::uint16_t move) const {
        return move & (1 << 13);
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

    const std::vector<std::uint16_t>& getMoves() const {
        return m_moves;
    }

    const bool& isDarkTurn() const {
        return m_darkTurn;
    }
};
