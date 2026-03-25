#pragma once

#include <bitset>
#include <cstring>
#include <array>

#include "NNUE.hpp"
#include "Matrix.hpp"

constexpr int h1{ 256 };
constexpr int h2{ 32 };

using AccumulatorState = std::pair<std::array<float, h1>, std::array<float, h1>>;

class NNUEInference {
private:
    NNUE& m_nnue;

    alignas(64) std::array<std::array<float, h1>, 128> W1;
    alignas(64) std::array<float, h1> b1;

    alignas(64) std::array<std::array<float, h2>, h1> W2;
    alignas(64) std::array<float, h2> b2;

    alignas(64) std::array<float, h2> W3;
    float b3;

    // maintain a flipped perspective feature set to avoid having to recalculate the whole accumulator when flipping sides
    alignas(64) mutable std::array<float, h1> a1;
    alignas(64) mutable std::array<float, h1> a1Flipped;

public:
    NNUEInference(NNUE& nnue) : m_nnue(nnue) {
        updateWeights();
    }

    void updateWeights() {
        const auto& W1mat{ m_nnue.getAccumulator().getWeights() };
        const auto& b1mat{ m_nnue.getAccumulator().getBias() };

        for (int i{ 0 }; i < 128; i++)
            for (int j{ 0 }; j < h1; j++)
                W1[i][j] = W1mat(i, j);
        for (int j{ 0 }; j < h1; j++)
            b1[j] = b1mat(0, j);

        const auto& W2mat{ m_nnue.getHiddenLayer(0).getWeights() };
        const auto& b2mat{ m_nnue.getHiddenLayer(0).getBias() };
        for (int i{ 0 }; i < h1; i++)
            for (int j{ 0 }; j < h2; j++)
                W2[i][j] = W2mat(i, j);
        for (int j{ 0 }; j < h2; j++)
            b2[j] = b2mat(0, j);

        const auto& W3mat{ m_nnue.getOutputLayer().getWeights() };
        const auto& b3mat{ m_nnue.getOutputLayer().getBias() };
        for (int i{ 0 }; i < h2; i++)
            W3[i] = W3mat(i, 0);
        b3 = b3mat(0, 0);

        a1 = b1;
        a1Flipped = b1;
    }

    static std::bitset<128> encodeBoard(std::uint64_t darkPieces, std::uint64_t lightPieces, std::uint64_t kingPieces) {
        // even index bits are playable
        std::bitset<128> features;

        for (int i{ 0 }; i < 64; i++) {
            std::uint64_t mask{ static_cast<std::uint64_t>(1) << i };

            if (!(mask & (darkPieces | lightPieces)))
                continue;

            int idx{ (i / 2) * 4 };

            if (mask & lightPieces) idx += 2;
            if (mask & kingPieces) idx++;
            features[idx] = 1;
        }

        return features;
    }

    float forwardAccumulator(bool flipped) const {
        std::array<float, h1> ac1;
        for (int j{ 0 }; j < h1; j++)
            ac1[j] = std::clamp(flipped ? a1Flipped[j] : a1[j], 0.f, 1.f);

        auto a2{ b2 };
        for (int i{ 0 }; i < h1; i++) {
            float ai{ ac1[i] };
            const auto& row{ W2[i] };
            for (int j{ 0 }; j < h2; j++)
                a2[j] += ai * row[j];
        }

        for (int j{ 0 }; j < h2; j++)
            a2[j] = std::clamp(a2[j], 0.f, 1.f);

        float out{ b3 };
        for (int i{ 0 }; i < h2; i++)
            out += a2[i] * W3[i];
        return out;
    }

    void initialise(std::uint64_t darkPieces, std::uint64_t lightPieces, std::uint64_t kingPieces) {
        a1 = b1;
        a1Flipped = b1;

        for (int i{ 0 }; i < 64; i++) {
            std::uint64_t mask{ static_cast<std::uint64_t>(1) << i };

            if (!(mask & (darkPieces | lightPieces)))
                continue;

            setFeature(i / 2, static_cast<bool>(mask & kingPieces), static_cast<bool>(mask & darkPieces));
        }

    }

    void setFeature(int sq32, bool king, bool dark) {
        int idx{ sq32 * 4 + ((!dark) * 2) + king };
        int idxFlipped{ (31 - sq32) * 4 + dark * 2 + king };

        for (int j{ 0 }; j < h1; j++) {
            a1[j] += W1[idx][j];
            a1Flipped[j] += W1[idxFlipped][j];
        }
    }

    void unsetFeature(int sq32, bool king, bool dark) {
        int idx{ sq32 * 4 + ((!dark) * 2) + king };
        int idxFlipped{ (31 - sq32) * 4 + dark * 2 + king };

        for (int j{ 0 }; j < h1; j++) {
            a1[j] -= W1[idx][j];
            a1Flipped[j] -= W1[idxFlipped][j];
        }
    }

    AccumulatorState getAccumulatorState() const {
        return { a1, a1Flipped };
    }

    void setAccumulatorState(AccumulatorState state) {
        a1 = state.first;
        a1Flipped = state.second;
    }
};
