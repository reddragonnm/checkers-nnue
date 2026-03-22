#pragma once

#include <bitset>
#include <cstring>

#include "NNUE.hpp"
#include "Matrix.hpp"

constexpr int h1{ 256 };
constexpr int h2{ 32 };

class NNUEInference {
private:
    NNUE& m_nnue;

    alignas(64) float W1[128][h1];
    alignas(64) float b1[h1];

    alignas(64) float W2[h1][h2];
    alignas(64) float b2[h2];

    alignas(64) float W3[h2];
    alignas(64) float b3;

    alignas(64) mutable float a1[h1];
    alignas(64) mutable float a2[h2];

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

    float forward(const std::bitset<128>& input) const {
        std::memcpy(a1, b1, h1 * sizeof(float));

        int setIndices[128];
        int numSet{ 0 };
        for (int i{ 0 }; i < 128; i++)
            if (input[i]) setIndices[numSet++] = i;

        for (int s{ 0 }; s < numSet; s++) {
            const float* row{ W1[setIndices[s]] };
            for (int j{ 0 }; j < h1; j++)
                a1[j] += row[j];
        }

        for (int j{ 0 }; j < h1; j++)
            a1[j] = a1[j] < 0.f ? 0.f : a1[j] > 1.f ? 1.f : a1[j];

        std::memcpy(a2, b2, h2 * sizeof(float));

        for (int i{ 0 }; i < h1; i++) {
            float ai{ a1[i] };
            const float* row{ W2[i] };
            for (int j{ 0 }; j < h2; j++)
                a2[j] += ai * row[j];
        }

        for (int j{ 0 }; j < h2; j++)
            a2[j] = a2[j] < 0.f ? 0.f : a2[j] > 1.f ? 1.f : a2[j];

        float out{ b3 };
        for (int i{ 0 }; i < h2; i++)
            out += a2[i] * W3[i];
        return out;
    }
};
