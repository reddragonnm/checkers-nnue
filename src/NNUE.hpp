#pragma once

#include <vector>

#include "Matrix.hpp"

template <typename T>
struct ClampedRelu {
    static T apply(T x) {
        return std::max(static_cast<T>(0), std::min(x, static_cast<T>(1)));
    }

    static T derivative(T x) {
        return (x > static_cast<T>(0) && x < static_cast<T>(1)) ? static_cast<T>(1) : static_cast<T>(0);
    }
};

template <typename T>
struct Linear {
    static T apply(T x) {
        return x;
    }

    static T derivative(T x) {
        return static_cast<T>(1);
    }
};

template <typename T, typename Activation>
class Layer {
private:
    int m_numInputs;
    int m_numOutputs;

    Matrix<T> m_lastInput;

    Matrix<T> m_weights;
    Matrix<T> m_bias;

public:
    Layer(int numInputs, int numOutputs) :
        m_numInputs(numInputs),
        m_numOutputs(numOutputs),
        m_weights(numInputs, numOutputs),
        m_bias(1, numOutputs) { // going to need broadcast add
    }

    Matrix<T> forward(Matrix<T> input) {
        m_lastInput = input;
        Matrix<T> output = (input * m_weights) + m_bias;

        output.map(Activation::apply);
        return output;
    }

    Matrix<T> backward(Matrix<T> error, double lr) {

    }
};

template <typename T>
class NNUE {
private:
    Layer<T, ClampedRelu<T>> m_accumulator;
    std::vector<Layer<T, ClampedRelu<T>>> m_hiddenLayers;
    Layer<T, Linear<T>> m_outputLayer;

public:
    NNUE(std::vector<int> layerSizes) : m_accumulator(layerSizes[0], layerSizes[1]), m_outputLayer(layerSizes[layerSizes.size() - 2], layerSizes.back()) {
        for (int i{ 2 }; i < layerSizes.size() - 1; i++) {
            m_hiddenLayers.emplace_back(layerSizes[i - 1], layerSizes[i]);
        }
    }

    Matrix<T> forward(Matrix<T> input) {

    }
};
