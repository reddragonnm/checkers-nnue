#pragma once

#include <vector>

#include "Matrix.hpp"

struct ClampedRelu {
    static float apply(float x) {
        return std::max(0.f, std::min(x, static_cast<float>(1)));
    }

    static float derivative(float x) {
        return (x > 0.f && x < 1.f) ? 1.f : 0.f;
    }
};

struct Linear {
    static float apply(float x) {
        return x;
    }

    static float derivative(float x) {
        return 1.0f;
    }
};

template <typename Activation>
class Layer {
private:
    int m_numInputs;
    int m_numOutputs;

    Matrix m_lastInput;

    Matrix m_weights;
    Matrix m_bias;

public:
    Layer(int numInputs, int numOutputs) :
        m_numInputs(numInputs),
        m_numOutputs(numOutputs),
        m_weights(numInputs, numOutputs),
        m_bias(1, numOutputs) { // going to need broadcast add
    }

    Matrix forward(Matrix input) {
        m_lastInput = input;
        Matrix output = (input * m_weights) + m_bias;

        output.map(Activation::apply);
        return output;
    }

    Matrix backward(Matrix error, double lr) {

    }
};

class NNUE {
private:
    Layer<ClampedRelu> m_accumulator;
    std::vector<Layer<ClampedRelu>> m_hiddenLayers;
    Layer<Linear> m_outputLayer;

public:
    NNUE(std::vector<int> layerSizes) : m_accumulator(layerSizes[0], layerSizes[1]), m_outputLayer(layerSizes[layerSizes.size() - 2], layerSizes.back()) {
        for (int i{ 2 }; i < layerSizes.size() - 1; i++) {
            m_hiddenLayers.emplace_back(layerSizes[i - 1], layerSizes[i]);
        }
    }

    Matrix forward(Matrix input) {

    }
};
