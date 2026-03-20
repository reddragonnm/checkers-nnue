#pragma once 

#include <iostream>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>
#include <array>
#include <filesystem>
#include <stack>
#include <fstream>

#include "Checkers.hpp"
#include <functional>

enum WDL : uint8_t { UNKNOWN = 0, DRAW = 1, WIN = 2, LOSS = 3 };

class EGTB {
private:
    std::array<std::array<std::vector<std::uint8_t>, 6>, 6> m_tables;
    std::array<std::array<int, 33>, 33> C{}; // zero initialised

    int getKingManIndex(const std::uint64_t& darkPieces, const std::uint64_t& lightPieces, const std::uint64_t& kingPieces) const {
        // get king man value for pieces present and then king present or not

        int idx{ 0 };
        int i{ 0 };

        std::uint64_t start{ darkPieces | lightPieces };

        while (start) {
            int sq{ __builtin_ctzll(start) };
            start &= start - 1;

            if ((kingPieces >> sq) & 1)
                idx |= (1 << i);
            i++;
        }

        return idx;
    }

    int getColorSubPosIndex(const std::uint64_t& darkPieces, const std::uint64_t& lightPieces) const {
        // get color based sub position index (keeping everything dark based)

        int idx{ 0 };
        int i{ 0 };
        int found{ 1 };

        std::uint64_t start{ darkPieces | lightPieces };

        while (start) {
            int sq{ __builtin_ctzll(start) };
            start &= start - 1;

            if ((darkPieces >> sq) & 1) {
                idx += C[i][found];
                found++;
            }
            i++;
        }

        return idx;
    }

    int getPosIndex(const std::uint64_t& darkPieces, const std::uint64_t& lightPieces) const {
        // get actual position based index from 32 valid squares
        int idx{ 0 };
        int found{ 1 };

        std::uint64_t start{ darkPieces | lightPieces };

        while (start) {
            int sq{ __builtin_ctzll(start) };
            start &= start - 1;

            idx += C[sq / 2][found]; // map 64 to 32 squares
            found++;
        }

        return idx;
    }

    int getIndex(int a, int b, const Checkers& board) const {
        // built for maximum 5 pieces on board or int will probably overflow

        const auto& darkPieces{ board.getDarkPieces() };
        const auto& lightPieces{ board.getLightPieces() };
        const auto& kingPieces{ board.getKingPieces() };

        int kingManIndex{ getKingManIndex(darkPieces, lightPieces, kingPieces) };
        int colorSubPosIndex{ getColorSubPosIndex(darkPieces, lightPieces) };
        int posIndex{ getPosIndex(darkPieces, lightPieces) };

        int power2{ 1 << (a + b) }; // 2^(a + b)
        return kingManIndex + power2 * colorSubPosIndex + power2 * C[a + b][a] * posIndex;
    }

    std::uint64_t flipBoard(std::uint64_t board) const {
        // flip board for light pieces to reuse dark move generation
        std::uint64_t flipped{ 0 };
        for (int i{ 0 }; i < 64; i++) {
            if ((board >> i) & 1)
                flipped |= (1ULL << (63 - i));
        }
        return flipped;
    }

    int getIndexLTM(int a, int b, const Checkers& board) const {
        // made for light to move postions
        const auto& darkPieces{ flipBoard(board.getLightPieces()) };
        const auto& lightPieces{ flipBoard(board.getDarkPieces()) };
        const auto& kingPieces{ flipBoard(board.getKingPieces()) };

        int kingManIndex{ getKingManIndex(darkPieces, lightPieces, kingPieces) };
        int colorSubPosIndex{ getColorSubPosIndex(darkPieces, lightPieces) };
        int posIndex{ getPosIndex(darkPieces, lightPieces) };

        int power2{ 1 << (a + b) }; // 2^(a + b)
        return kingManIndex + power2 * colorSubPosIndex + power2 * C[a + b][a] * posIndex;
    }

    void setTableIndex(int a, int b, int index, WDL value) {
        if (a < 0 || a > 5 || b < 0 || b > 5 || index < 0) { // safety checks because errors
            return;
        }

        int pos{ index / 4 };
        int offset{ (index % 4) * 2 };
        if (pos < 0 || pos >= static_cast<int>(m_tables[a][b].size())) {
            return;
        }

        m_tables[a][b][pos] &= ~(0b11 << offset);
        m_tables[a][b][pos] |= (static_cast<std::uint8_t>(value) << offset);
    }

    WDL getTableVal(int a, int b, int index) const {
        if (a < 0 || a > 5 || b < 0 || b > 5 || index < 0) { // safety checks because errors
            return UNKNOWN;
        }

        int pos{ index / 4 };
        int offset{ (index % 4) * 2 };
        if (pos < 0 || pos >= static_cast<int>(m_tables[a][b].size())) {
            return UNKNOWN;
        }

        return static_cast<WDL>((m_tables[a][b][pos] >> offset) & 0b11);
    }

    int expandBoard(int i) const {
        // 32 to 64 square mapping
        if ((i / 4) % 2 == 0)
            return (i * 2 + 1);
        else
            return (i * 2);
    }

    std::uint32_t decodeCombinadic(int k, int rank) const {
        // made it safer ig, idk
        std::uint32_t mask{ 0 };
        int n{ 32 };

        for (int r{ k }; r >= 1; --r) {
            int x{ n - 1 };
            while (x >= r && C[x][r] > rank) {
                --x;
            }

            if (x < r - 1) {
                return 0;
            }

            mask |= (std::uint32_t{ 1 } << x);
            rank -= C[x][r];
            n = x;
        }

        return mask;
    }

    Checkers decodeFromIndex(int idx, int a, int b) const {
        int colorComb{ C[a + b][a] };
        int kingComb{ 1 << (a + b) };

        int kingManIndex{ idx % kingComb };
        idx /= kingComb;
        int colorSubPosIndex{ idx % colorComb };
        idx /= colorComb;
        int posIndex{ idx };

        std::uint32_t posLevel{ decodeCombinadic(a + b, posIndex) };
        std::uint32_t colorLevel{ decodeCombinadic(a, colorSubPosIndex) };

        std::uint64_t darkPieces{ 0 };
        std::uint64_t lightPieces{ 0 };
        std::uint64_t kingPieces{ 0 };

        for (int j{ 0 }; j < a + b; j++) {
            std::uint64_t sq{ std::uint64_t{1} << expandBoard(__builtin_ctz(posLevel)) };
            posLevel &= posLevel - 1;

            if ((colorLevel >> j) & 1)
                darkPieces |= sq;
            else
                lightPieces |= sq;

            if ((kingManIndex >> j) & 1)
                kingPieces |= sq;
        }

        return Checkers{ darkPieces, lightPieces, kingPieces, true };
    }

    bool evaluateBoard(int i, int a, int b, Checkers& board) {
        auto moves{ board.getMoves() };
        auto numMoves{ board.getNumMoves() };

        if (numMoves == 0) {
            setTableIndex(a, b, i, LOSS);
            return true;
        }

        std::stack<std::pair<int, int>> s;
        for (int m{ numMoves - 1 }; m >= 0; m--)
            s.push({ 0, m });

        bool allWin{ true };
        int curDepth{ 0 };

        while (!s.empty()) {
            auto [depth, moveIdx] = s.top();
            s.pop();

            while (curDepth > depth) {
                board.undoMove();
                curDepth--;
            }

            if (board.makeMove(moveIdx)) { // turn switched
                int childA{ std::popcount(board.getDarkPieces()) };
                int childB{ std::popcount(board.getLightPieces()) };

                // no light pieces left
                if (childB == 0) {
                    setTableIndex(a, b, i, WIN);
                    return true;
                }

                if (childA < 0 || childA > 5 || childB < 0 || childB > 5
                    || m_tables[childB][childA].empty()) {
                    std::cerr << "This should not happen\n";
                    allWin = false;
                    board.undoMove();
                    continue;
                }

                int idx{ getIndexLTM(childB, childA, board) };
                WDL result{ getTableVal(childB, childA, idx) };

                if (result == LOSS) {
                    setTableIndex(a, b, i, WIN);
                    return true;
                }
                else if (result != WIN) {
                    allWin = false;
                }

                board.undoMove();
            }
            else {
                curDepth++;
                for (int m{ board.getNumMoves() - 1 }; m >= 0; m--)
                    s.push({ depth + 1, m });
            }
        }

        if (allWin) {
            setTableIndex(a, b, i, LOSS);
            return true;
        }

        return false;
    }

    void build(int a, int b) {
        // built from perspective of dark side, only dark moves
        // total = 32C(a + b) * (a + b)C(a) * 2^(a + b)

        int size{ C[32][a + b] * C[a + b][a] * (1 << (a + b)) };

        std::cout << "Building table " << a << "v" << b << " of size " << size << '\n';

        m_tables[a][b].resize((size + 3) / 4, 0);

        bool changed{ true };
        int pass{ 0 };

        while (changed) {
            changed = false;

            for (int i{ 0 }; i < size; i++) {
                if (getTableVal(a, b, i) != UNKNOWN)
                    continue;

                Checkers board{ decodeFromIndex(i, a, b) };
                assert(getIndex(a, b, board) == i);

                if (evaluateBoard(i, a, b, board))
                    changed = true;
            }

            if (!changed) {
                for (int i{ 0 }; i < size; i++) {
                    if (getTableVal(a, b, i) == UNKNOWN) {
                        setTableIndex(a, b, i, DRAW); // mark remaining as draw
                    }
                }
            }
        }
    }

public:
    EGTB() {
        for (int i{ 0 }; i <= 32; i++) {
            C[i][0] = C[i][i] = 1;
            for (int j{ 1 }; j < i; j++) {
                C[i][j] = C[i - 1][j - 1] + C[i - 1][j];
            }
        }
    }

    void buildOrLoad(const std::string& filepath) {
        namespace fs = std::filesystem;

        if (fs::exists(filepath)) {
            std::cout << "Loading EGTB from " << filepath << "...\n";

            std::ifstream file{ filepath, std::ios::binary };
            if (!file) {
                std::cerr << "Failed to open " << filepath << "\n";
                return;
            }

            for (int a{ 0 }; a <= 5; a++) {
                for (int b{ 0 }; b <= 5; b++) {
                    std::size_t size{};
                    file.read(reinterpret_cast<char*>(&size), sizeof(size));
                    m_tables[a][b].resize(size);
                    if (size > 0)
                        file.read(reinterpret_cast<char*>(m_tables[a][b].data()), size);
                }
            }

            std::cout << "Loaded EGTB\n";
        }
        else {
            std::cout << "Building EGTB...\n";

            build(1, 0); build(0, 1);
            build(1, 1);
            build(2, 0); build(0, 2); build(2, 1); build(1, 2);
            build(2, 2);
            build(3, 0); build(0, 3); build(3, 1); build(1, 3); build(3, 2); build(2, 3);
            build(4, 0); build(0, 4); build(4, 1); build(1, 4);
            build(5, 0); build(0, 5);

            std::ofstream file{ filepath, std::ios::binary };
            if (!file) {
                std::cerr << "Failed to write " << filepath << "\n";
                return;
            }

            for (int a{ 0 }; a <= 5; a++) {
                for (int b{ 0 }; b <= 5; b++) {
                    std::size_t size{ m_tables[a][b].size() };
                    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
                    if (size > 0)
                        file.write(reinterpret_cast<const char*>(m_tables[a][b].data()), size);
                }
            }

            std::cout << "Saved EGTB to " << filepath << "\n";
        }
    }

    WDL probe(const Checkers& board) const {
        if (board.isMidCapture())
            return UNKNOWN;

        int a{ std::popcount(board.getDarkPieces()) };
        int b{ std::popcount(board.getLightPieces()) };

        if (b == 0) return WIN;   // dark won, no light pieces
        if (a == 0) return LOSS;  // dark lost, no dark pieces

        if (a + b > 5)
            return UNKNOWN;

        if (m_tables[a][b].empty()) {
            std::cerr << "Table for " << a << "v" << b << " not built or loaded!\n";
            return UNKNOWN;
        }

        int idx;
        if (board.isDarkTurn()) {
            idx = getIndex(a, b, board);
            return getTableVal(a, b, idx);
        }
        else {
            idx = getIndexLTM(b, a, board);
            return getTableVal(b, a, idx);
        }
    }
};
