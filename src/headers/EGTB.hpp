#pragma once 

#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <array>
#include <filesystem>
#include <stack>
#include <fstream>

#include "Checkers.hpp"

enum class WDL : uint8_t { UNKNOWN = 0, DRAW = 1, WIN = 2, LOSS = 3 };
enum class DTZ : uint8_t { UNKNOWN = 0, TERMINAL = 1, NONE = 255 }; // full uint8 used

class EGTB {
private:
    std::array<std::array<std::vector<std::uint8_t>, 6>, 6> m_tables;
    std::array<std::array<std::vector<std::uint8_t>, 6>, 6> m_dtz;

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
            return WDL::UNKNOWN;
        }

        int pos{ index / 4 };
        int offset{ (index % 4) * 2 };
        if (pos < 0 || pos >= static_cast<int>(m_tables[a][b].size())) {
            return WDL::UNKNOWN;
        }

        return static_cast<WDL>((m_tables[a][b][pos] >> offset) & 0b11);
    }

    void setDTZIndex(int a, int b, int index, std::uint8_t val) {
        if (a < 0 || a > 5 || b < 0 || b > 5 || index < 0 || index >= m_dtz[a][b].size()) { // safety checks because errors
            return;
        }

        m_dtz[a][b][index] = val;
    }

    std::uint8_t getDTZVal(int a, int b, int index) const {
        if (a < 0 || a > 5 || b < 0 || b > 5 || index < 0 || index >= m_dtz[a][b].size()) { // safety checks because errors
            return static_cast<std::uint8_t>(DTZ::UNKNOWN);
        }

        return m_dtz[a][b][index];
    }

    bool updateDTZ(int a, int b, int index, std::uint8_t val) {
        if (getDTZVal(a, b, index) == val)
            return false;

        setDTZIndex(a, b, index, val);
        return true;
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

        int curA{ std::popcount(board.getDarkPieces()) };
        int curB{ std::popcount(board.getLightPieces()) };

        if (curB == 0) {
            setTableIndex(a, b, i, WDL::WIN);
            return true;
        }
        if (curA == 0) {
            setTableIndex(a, b, i, WDL::LOSS);
            return true;
        }

        if (numMoves == 0) {
            setTableIndex(a, b, i, WDL::LOSS);
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
                    setTableIndex(a, b, i, WDL::WIN);
                    return true;
                }
                if (childA == 0) {
                    allWin = false;
                    board.undoMove();
                    continue;
                }

                if (childA < 0 || childA > 5 || childB < 0 || childB > 5
                    || m_tables[childB][childA].empty()) {
                    std::cerr << "This should not happen\n";
                    std::abort();
                }

                int idx{ getIndexLTM(childB, childA, board) };
                WDL result{ getTableVal(childB, childA, idx) };

                if (result == WDL::LOSS) {
                    setTableIndex(a, b, i, WDL::WIN);
                    return true;
                }
                else if (result != WDL::WIN) {
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
            setTableIndex(a, b, i, WDL::LOSS);
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
        if (a != b) {
            m_tables[b][a].resize((size + 3) / 4, 0);
        }

        int pass{ 0 };
        int totalChanges{ 0 };
        while (true) {
            int changes{ 0 };

            for (int i{ 0 }; i < size; i++) {
                if (getTableVal(a, b, i) == WDL::UNKNOWN) {
                    Checkers board{ decodeFromIndex(i, a, b) };
                    assert(getIndex(a, b, board) == i);

                    if (evaluateBoard(i, a, b, board))
                        changes++;
                }

                if (a != b && getTableVal(b, a, i) == WDL::UNKNOWN) {
                    Checkers board{ decodeFromIndex(i, b, a) };
                    assert(getIndex(b, a, board) == i);

                    if (evaluateBoard(i, b, a, board))
                        changes++;
                }
            }

            totalChanges += changes;
            std::cout << "Pass " << pass++ << " changes:" << changes << " total changes:" << totalChanges << "/" << (a == b ? size : (size * 2)) << '\n';

            if (changes == 0) {
                int drawSet{ 0 };
                for (int i{ 0 }; i < size; i++) {
                    if (getTableVal(a, b, i) == WDL::UNKNOWN) {
                        setTableIndex(a, b, i, WDL::DRAW); // mark remaining as DRAW
                        drawSet++;
                    }

                    if (a != b && getTableVal(b, a, i) == WDL::UNKNOWN) {
                        setTableIndex(b, a, i, WDL::DRAW); // mark remaining as DRAW
                        drawSet++;
                    }

                }
                std::cout << "Draws set: " << drawSet << " total changes: " << (totalChanges + drawSet) << "/" << (a == b ? size : (size * 2)) << '\n';
                break;
            }
        }

        int WIN{ 0 }, LOSS{ 0 }, DRAW{ 0 };
        for (int i{ 0 }; i < size; i++) {
            WDL result{ getTableVal(a, b, i) };
            if (result == WDL::WIN) WIN++;
            else if (result == WDL::LOSS) LOSS++;
            else if (result == WDL::DRAW) DRAW++;
        }
        std::cout << "Table " << a << "v" << b << ": " << WIN << " wins, " << LOSS << " losses, " << DRAW << " draws\n";

        if (a != b) {
            WIN = 0; LOSS = 0; DRAW = 0;
            for (int i{ 0 }; i < size; i++) {
                WDL result{ getTableVal(b, a, i) };
                if (result == WDL::WIN) WIN++;
                else if (result == WDL::LOSS) LOSS++;
                else if (result == WDL::DRAW) DRAW++;
            }
            std::cout << "Table " << b << "v" << a << ": " << WIN << " wins, " << LOSS << " losses, " << DRAW << " draws\n";
        }
        std::cout << "\n";
    }


    bool evaluateDTZ(int i, int a, int b, Checkers& board) {
        WDL myWDL{ getTableVal(a, b, i) };

        auto none{ static_cast<std::uint8_t>(DTZ::NONE) };
        auto unknown{ static_cast<std::uint8_t>(DTZ::UNKNOWN) };
        auto terminal{ static_cast<std::uint8_t>(DTZ::TERMINAL) };

        if (myWDL == WDL::UNKNOWN || myWDL == WDL::DRAW) {
            return updateDTZ(a, b, i, none);
        }

        int curA{ std::popcount(board.getDarkPieces()) };
        int curB{ std::popcount(board.getLightPieces()) };

        if (curA == 0 || curB == 0 || board.getNumMoves() == 0) {
            return updateDTZ(a, b, i, terminal);
        }

        std::stack<std::pair<int, int>> s;
        for (int m{ board.getNumMoves() - 1 }; m >= 0; m--)
            s.push({ 0, m });

        int curDepth{ 0 };

        std::uint8_t bestVal{ myWDL == WDL::WIN ? none : terminal };
        bool found{ false };
        bool allResolved{ true };

        while (!s.empty()) {
            auto [depth, moveIdx] = s.top();
            s.pop();

            while (curDepth > depth) {
                board.undoMove();
                curDepth--;
            }

            if (board.makeMove(moveIdx)) {
                int childA{ std::popcount(board.getDarkPieces()) };
                int childB{ std::popcount(board.getLightPieces()) };

                if (childA == 0 || childB == 0 || board.getNumMoves() == 0) {
                    if (myWDL == WDL::WIN) {
                        bestVal = std::min(bestVal, static_cast<std::uint8_t>(terminal + 1)); // child is terminal so this dtz is 2
                        found = true;
                    }

                    // child is lost so we dont see parent lost branch

                    board.undoMove();
                    continue;
                }

                if (childA < 0 || childA > 5 || childB < 0 || childB > 5
                    || m_tables[childB][childA].empty() || m_dtz[childB][childA].empty()) {
                    std::cerr << "This should not happen\n";
                    std::abort();
                }

                int childIdx{ getIndexLTM(childB, childA, board) };
                WDL childWDL{ getTableVal(childB, childA, childIdx) };

                // only check relevant positions
                if (!(myWDL == WDL::WIN ? (childWDL == WDL::LOSS) : (childWDL == WDL::WIN))) {
                    board.undoMove();
                    continue;
                }

                auto childStored{ getDTZVal(childB, childA, childIdx) };

                if (childStored == unknown) {
                    allResolved = false;
                    board.undoMove();
                    continue;
                }

                if (childStored == none) {
                    if (myWDL == WDL::LOSS) allResolved = false;

                    board.undoMove();
                    continue;
                }

                std::uint8_t candidate;
                if (board.getDrawCounter() == 0) {
                    candidate = terminal + 1;
                }
                else {
                    if (childStored >= 254u) {
                        if (myWDL == WDL::LOSS) allResolved = false;
                        else found = true;

                        board.undoMove();
                        continue;
                    }

                    candidate = childStored + 1;
                }

                if (myWDL == WDL::WIN)
                    bestVal = std::min(bestVal, candidate);
                else
                    bestVal = std::max(bestVal, candidate);

                found = true;
                board.undoMove();
            }
            else {
                curDepth++;
                for (int m{ board.getNumMoves() - 1 }; m >= 0; m--)
                    s.push({ depth + 1, m });
            }
        }

        // win resolved when loss with known dtz found
        // loss resolved when all win children resolved
        // so if cant resolve then return false
        if (!(myWDL == WDL::WIN ? found : (found && allResolved))) {
            return false;
        }

        return updateDTZ(a, b, i, bestVal);
    }

    void buildDTZ(int a, int b) {
        int size{ C[32][a + b] * C[a + b][a] * (1 << (a + b)) };
        std::cout << "Building DTZ table " << a << "v" << b << " of size " << size << '\n';

        m_dtz[a][b].resize(size, static_cast<std::uint8_t>(DTZ::UNKNOWN));
        if (a != b) m_dtz[b][a].assign(size, static_cast<std::uint8_t>(DTZ::UNKNOWN));

        int pass{ 0 };
        int totalChanges{ 0 };

        std::uint8_t none{ static_cast<std::uint8_t>(DTZ::NONE) };
        std::uint8_t unknown{ static_cast<std::uint8_t>(DTZ::UNKNOWN) };

        while (true) {
            int changes = 0;

            for (int i{ 0 }; i < size; i++) {
                auto v{ getDTZVal(a, b, i) };
                if (v != none) {
                    Checkers board{ decodeFromIndex(i, a, b) };
                    if (evaluateDTZ(i, a, b, board))
                        changes++;
                }

                if (a != b) {
                    auto v2{ getDTZVal(b, a, i) };
                    if (v2 != none) {
                        Checkers board{ decodeFromIndex(i, b, a) };
                        if (evaluateDTZ(i, b, a, board))
                            changes++;
                    }
                }
            }

            totalChanges += changes;
            std::cout << "Pass " << pass++ << " changes:" << changes << " total changes:" << totalChanges << '\n';

            if (changes == 0) {
                int noneSet{ 0 };
                for (int i{ 0 }; i < size; i++) {
                    if (getDTZVal(a, b, i) == unknown) {
                        setDTZIndex(a, b, i, none);
                        noneSet++;
                    }

                    if (a != b && getDTZVal(b, a, i) == unknown) {
                        setDTZIndex(b, a, i, none);
                        noneSet++;
                    }
                }
                std::cout << "Nones set: " << noneSet << " total changes: " << (totalChanges + noneSet) << '\n';
                break;
            }
        }

        int numUnknown{ 0 }, numNone{ 0 }, numTerminal{ 0 };
        for (int i{ 0 }; i < size; i++) {
            auto v{ getDTZVal(a, b, i) };
            if (v == unknown) numUnknown++;
            else if (v == none) numNone++;
            else if (v == static_cast<std::uint8_t>(DTZ::TERMINAL)) numTerminal++;
        }
        std::cout << "DTZ Table " << a << "v" << b << ": " << numUnknown << " unknown, " << numNone << " none, " << numTerminal << " terminal\n";

        if (a != b) {
            numUnknown = 0; numNone = 0; numTerminal = 0;
            for (int i{ 0 }; i < size; i++) {
                auto v{ getDTZVal(b, a, i) };
                if (v == unknown) numUnknown++;
                else if (v == none) numNone++;
                else if (v == static_cast<std::uint8_t>(DTZ::TERMINAL)) numTerminal++;
            }
            std::cout << "DTZ Table " << b << "v" << a << ": " << numUnknown << " unknown, " << numNone << " none, " << numTerminal << " terminal\n";
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

    void buildOrLoad(const std::string& filepath, const std::string& dtzFilepath) {
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

            build(1, 0);
            build(1, 1);
            build(2, 0);
            build(2, 1);
            build(2, 2);
            build(3, 0);
            build(3, 1);
            build(3, 2);
            build(4, 0);
            build(4, 1);
            build(5, 0);

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

        if (fs::exists(dtzFilepath)) {
            std::cout << "Loading DTZ from " << dtzFilepath << "...\n";

            std::ifstream file{ dtzFilepath, std::ios::binary };
            if (!file) {
                std::cerr << "Failed to open " << dtzFilepath << "\n";
                return;
            }

            for (int a{ 0 }; a <= 5; a++) {
                for (int b{ 0 }; b <= 5; b++) {
                    std::size_t size{};
                    file.read(reinterpret_cast<char*>(&size), sizeof(size));
                    m_dtz[a][b].resize(size);
                    if (size > 0)
                        file.read(reinterpret_cast<char*>(m_dtz[a][b].data()), size);
                }
            }

            std::cout << "Loaded DTZ\n";
        }
        else {
            std::cout << "Building DTZ...\n";

            buildDTZ(1, 1);
            buildDTZ(2, 1);
            buildDTZ(2, 2);
            buildDTZ(3, 1);
            buildDTZ(3, 2);
            buildDTZ(4, 1);

            std::ofstream file{ dtzFilepath, std::ios::binary };
            if (!file) {
                std::cerr << "Failed to write " << dtzFilepath << "\n";
                return;
            }

            for (int a{ 0 }; a <= 5; a++) {
                for (int b{ 0 }; b <= 5; b++) {
                    std::size_t size{ m_dtz[a][b].size() };
                    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
                    if (size > 0)
                        file.write(reinterpret_cast<const char*>(m_dtz[a][b].data()), size);
                }
            }

            std::cout << "Saved DTZ to " << dtzFilepath << "\n";
        }
    }

    WDL probe(const Checkers& board) const {
        if (board.isMidCapture())
            return WDL::UNKNOWN;

        int a{ std::popcount(board.getDarkPieces()) };
        int b{ std::popcount(board.getLightPieces()) };

        if (b == 0) return (board.isDarkTurn() ? WDL::WIN : WDL::LOSS);   // dark won, no light pieces
        if (a == 0) return (board.isDarkTurn() ? WDL::LOSS : WDL::WIN);  // dark lost, no dark pieces

        if (a + b > 5)
            return WDL::UNKNOWN;

        if (m_tables[a][b].empty()) {
            std::cerr << "Table for " << a << "v" << b << " not built or loaded!\n";
            return WDL::UNKNOWN;
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

    int probeDTZ(const Checkers& board) const {
        if (board.isMidCapture()) return -1;

        int a{ std::popcount(board.getDarkPieces()) };
        int b{ std::popcount(board.getLightPieces()) };

        if (a + b > 5) return -1;
        if (a == 0 || b == 0) return -1;
        if (board.getNumMoves() == 0) return 0;

        int idx{ board.isDarkTurn() ? getIndex(a, b, board) : getIndexLTM(b, a, board) };
        auto stored{ board.isDarkTurn()
            ? getDTZVal(a, b, idx)
            : getDTZVal(b, a, idx) };

        if (stored == static_cast<std::uint8_t>(DTZ::NONE) || stored == static_cast<std::uint8_t>(DTZ::UNKNOWN)) return -1;

        return static_cast<int>(stored) - static_cast<int>(DTZ::TERMINAL);
    }
};
