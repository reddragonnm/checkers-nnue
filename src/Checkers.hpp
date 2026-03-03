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
    std::uint64_t m_kingPieces{};

    // move/capture-x, from - xxxxxx (0 to 63), to - xxxxxx (0 to 63)
    std::vector<std::uint16_t> m_moves{};

    int m_drawCounter{ 0 };
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

    void generateRTCaptures(std::uint64_t pieces) {
        // TODO: check double rt instead of rt2 speed/space
        std::uint64_t oppPieces{ (pieces & m_darkPieces) ? m_lightPieces : m_darkPieces }; // pieces is subset of darkPieces (might be king)
        std::uint64_t check1{ ((rt2 & pieces) << 7) & oppPieces }; // check opp piece is present after one diagonal
        std::uint64_t validMoves{ (check1 << 7) & ~(m_lightPieces | m_darkPieces) }; // then valid only after empty at 2 diagonal

        for (int i{ 0 }; i < 64; i++) {
            if ((static_cast<std::uint64_t>(1) << i) & validMoves) {
                m_moves.push_back(i | (static_cast<std::uint64_t>(i - 14) << 6 | (1 << 12)));
            }
        }
    }

    void generateLTCaptures(std::uint64_t pieces) {
        std::uint64_t oppPieces{ (pieces & m_darkPieces) ? m_lightPieces : m_darkPieces };
        std::uint64_t check1{ ((lt2 & pieces) << 9) & oppPieces };
        std::uint64_t validMoves{ (check1 << 9) & ~(m_lightPieces | m_darkPieces) };

        for (int i{ 0 }; i < 64; i++) {
            if ((static_cast<std::uint64_t>(1) << i) & validMoves) {
                m_moves.push_back(i | (static_cast<std::uint64_t>(i - 18) << 6 | (1 << 12)));
            }
        }
    }

    void generateRBCaptures(std::uint64_t pieces) {
        std::uint64_t oppPieces{ (pieces & m_darkPieces) ? m_lightPieces : m_darkPieces };
        std::uint64_t check1{ ((rb2 & pieces) >> 9) & oppPieces };
        std::uint64_t validMoves{ (check1 >> 9) & ~(m_lightPieces | m_darkPieces) };

        for (int i{ 0 }; i < 64; i++) {
            if ((static_cast<std::uint64_t>(1) << i) & validMoves) {
                m_moves.push_back(i | (static_cast<std::uint64_t>(i + 18) << 6 | (1 << 12)));
            }
        }
    }

    void generateLBCaptures(std::uint64_t pieces) {
        std::uint64_t oppPieces{ (pieces & m_darkPieces) ? m_lightPieces : m_darkPieces };
        std::uint64_t check1{ ((lb2 & pieces) >> 7) & oppPieces };
        std::uint64_t validMoves{ (check1 >> 7) & ~(m_lightPieces | m_darkPieces) };

        for (int i{ 0 }; i < 64; i++) {
            if ((static_cast<std::uint64_t>(1) << i) & validMoves) {
                m_moves.push_back(i | (static_cast<std::uint64_t>(i + 14) << 6 | (1 << 12)));
            }
        }
    }

    void generateMoves() {
        m_kingPieces |= m_darkPieces & 0xff00000000000000; // last row
        m_kingPieces |= m_lightPieces & 0xff; // first row

        if (m_darkTurn) {
            generateRTCaptures(m_darkPieces);
            generateLTCaptures(m_darkPieces);

            generateLBCaptures(m_darkPieces & m_kingPieces);
            generateRBCaptures(m_darkPieces & m_kingPieces);


            if (m_moves.empty()) {
                generateRTMoves(m_darkPieces);
                generateLTMoves(m_darkPieces);

                generateLBMoves(m_darkPieces & m_kingPieces);
                generateRBMoves(m_darkPieces & m_kingPieces);
            }

        }
        else {
            generateLBCaptures(m_lightPieces);
            generateRBCaptures(m_lightPieces);

            generateRTCaptures(m_lightPieces & m_kingPieces);
            generateLTCaptures(m_lightPieces & m_kingPieces);

            if (m_moves.empty()) {
                generateLBMoves(m_lightPieces);
                generateRBMoves(m_lightPieces);

                generateRTMoves(m_lightPieces & m_kingPieces);
                generateLTMoves(m_lightPieces & m_kingPieces);
            }
        }
    }

    bool makeMove(int moveIdx) {
        // TODO: make this cleaner
        auto idx{ static_cast<std::uint64_t>(1) };
        auto move{ m_moves[moveIdx] };

        auto f{ getFromSquare(move) };
        auto t{ getToSquare(move) };

        auto frSq{ idx << f };
        auto toSq{ idx << t };
        auto midSq{ idx << ((f + t) / 2) };

        m_drawCounter++;

        m_moves.clear();

        if (isCaptureMove(move)) {
            m_drawCounter = 0;
            if (m_darkTurn) {
                m_darkPieces &= ~frSq;
                m_darkPieces |= toSq;

                m_lightPieces &= ~midSq; // remove captured piece

                // captures to last row (making it king) and not already king (as toSq is not updated yet for king)
                if ((toSq & 0xff00000000000000) && !(frSq & m_kingPieces)) {
                    m_darkTurn = !m_darkTurn;
                    generateMoves();
                    return true; // if king is made capture chain is broken
                }

                // now check if this piece can capture any further (assuming not king - handled separately)
                generateRTCaptures(toSq);
                generateLTCaptures(toSq);
            }
            else {
                m_lightPieces &= ~frSq;
                m_lightPieces |= toSq;

                m_darkPieces &= ~midSq;

                if ((toSq & 0xff) && !(frSq & m_kingPieces)) {// first row
                    m_darkTurn = !m_darkTurn;
                    generateMoves();
                    return true;
                }

                generateLBCaptures(toSq);
                generateRBCaptures(toSq);
            }

            if (m_kingPieces & frSq) {
                m_kingPieces &= ~frSq;
                m_kingPieces &= ~midSq;
                m_kingPieces |= toSq;

                if (m_darkTurn) {
                    generateLBCaptures(toSq);
                    generateRBCaptures(toSq);
                }
                else {
                    generateRTCaptures(toSq);
                    generateLTCaptures(toSq);
                }
            }

            if (m_moves.empty()) { // no more captures left
                m_darkTurn = !m_darkTurn;
                generateMoves();
            }
        }
        else {
            if (m_darkTurn) {
                m_darkPieces &= ~frSq;
                m_darkPieces |= toSq;

                if ((toSq & 0xff00000000000000) && !(frSq & m_kingPieces)) {
                    m_drawCounter = 0;
                }
            }
            else {
                m_lightPieces &= ~frSq;
                m_lightPieces |= toSq;

                if ((toSq & 0xff) && !(frSq & m_kingPieces)) {
                    m_drawCounter = 0;
                }
            }

            if (m_kingPieces & frSq) {
                m_kingPieces &= ~frSq;
                m_kingPieces |= toSq;
            }

            m_darkTurn = !m_darkTurn;
            generateMoves();
            return true;
        }

        return false;
    }

    bool isDraw() {
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

    const std::vector<std::uint16_t>& getMoves() const {
        return m_moves;
    }

    const bool& isDarkTurn() const {
        return m_darkTurn;
    }
};
