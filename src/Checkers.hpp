#pragma once

#include <cstdint>
#include <vector>

class Checkers {
private:
    std::uint64_t m_darkPieces{};
    std::uint64_t m_lightPieces{};
    std::uint64_t m_kingPieces{};

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

            std::uint64_t leftMoves{ (leftExMask & m_darkPieces) << 9 };
            std::uint64_t validLeftMoves{ leftMoves & ~(m_darkPieces | m_lightPieces) };

            for (int i{ 0 }; i < 64; i++) {
                if ((static_cast<std::uint64_t>(1) << i) & validLeftMoves) {
                    m_moves.push_back(i | (static_cast<std::uint64_t>(i - 9) << 6));
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

            std::uint64_t leftMoves{ (leftExMask & m_lightPieces) >> 7 };
            std::uint64_t validLeftMoves{ leftMoves & ~(m_darkPieces | m_lightPieces) };

            for (int i{ 0 }; i < 64; i++) {
                if ((static_cast<std::uint64_t>(1) << i) & validLeftMoves) {
                    m_moves.push_back(i | (static_cast<std::uint64_t>(i + 7) << 6));
                }
            }
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

    const std::vector<std::uint16_t>& getMoves() const {
        return m_moves;
    }

    const bool& isDarkTurn() const {
        return m_darkTurn;
    }
};
