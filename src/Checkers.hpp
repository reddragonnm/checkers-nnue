#pragma once

#include <cstdint>
#include <vector>

class Checkers {
private:
    std::uint64_t m_darkPieces{};
    std::uint64_t m_lightPieces{};

    // move/capture-x, from - xxxxxx (0 to 63), to - xxxxxx (0 to 63)
    std::vector<std::uint16_t> m_moves{};

    bool m_darkTurn{ true };

public:
    Checkers() {
        // initialise board
        m_darkPieces = 0xaa55aa;
        m_lightPieces = static_cast<std::uint64_t>(0x55aa55) << 40;
    }

    void generateMoves() {
        m_moves.clear();

        std::uint64_t rightExMask{ 0xfefefefefefefefe };
        std::uint64_t leftExMask{ 0x7f7f7f7f7f7f7f7f };

        if (m_darkTurn) {
            std::uint64_t rightMoves{ (rightExMask & m_darkPieces) << 7 };
            std::uint64_t validRightMoves{ rightMoves & ~(m_darkPieces | m_lightPieces) };

            for (int i{ 0 }; i < 64; i++) {
                if ((static_cast<std::uint64_t>(1) << i) & validRightMoves) {
                    m_moves.push_back(i | (static_cast<std::uint64_t>(i - 7) << 6));
                }
            }
        }
        else {
            std::uint64_t rightMoves{ (rightExMask & m_lightPieces) >> 9 };
            std::uint64_t validRightMoves{ rightMoves & ~(m_darkPieces | m_lightPieces) };

            for (int i{ 0 }; i < 64; i++) {
                if ((static_cast<std::uint64_t>(1) << i) & validRightMoves) {
                    m_moves.push_back(i | (static_cast<std::uint64_t>(i + 9) << 6));
                }
            }
        }
    }

    bool makeMove(int moveIdx) {
        return true;
    }

    const std::uint64_t& getDarkPieces() const {
        return m_darkPieces;
    }

    const std::uint64_t& getLightPieces() const {
        return m_lightPieces;
    }

    const std::vector<std::uint16_t>& getMoves() const {
        return m_moves;
    }

    const bool& isDarkTurn() const {
        return m_darkTurn;
    }
};
