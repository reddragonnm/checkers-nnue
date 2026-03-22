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

inline Matrix MSE(const Matrix& output, const Matrix& target) {
    assert(output.numCols() == target.numCols() && output.numRows() == target.numRows());

    Matrix diff{ output + (target * -1.f) };
    float scale{ 2.f / output.numRows() };
    return diff * scale;
}

template <typename Activation>
class Layer {
private:
    int m_numInputs;
    int m_numOutputs;

    Matrix m_lastInput;
    Matrix m_lastPreActivation;

    Matrix m_weights;
    Matrix m_bias;

public:
    Layer(int numInputs, int numOutputs) :
        m_numInputs(numInputs),
        m_numOutputs(numOutputs),
        m_weights(numInputs, numOutputs),
        m_bias(1, numOutputs) { // going to need broadcast add
        m_weights.randomInit();
    }

    Matrix forward(const Matrix& input) {
        m_lastInput = input;
        m_lastPreActivation = (input * m_weights) + m_bias;
        Matrix output{ m_lastPreActivation };

        output.map(Activation::apply);
        return output;
    }

    Matrix backward(const Matrix& error, float lr) {
        Matrix delta{ m_lastPreActivation };
        delta.map(Activation::derivative);
        delta.hadamard(error);

        Matrix dW{ m_lastInput.transpose() * delta };
        Matrix dB{ delta.columnSum() };

        Matrix inputError{ delta * m_weights.transpose() };

        m_weights = m_weights + (dW * (-lr));
        m_bias = m_bias + (dB * (-lr));
        return inputError;
    }

    const Matrix& getWeights() const {
        return m_weights;
    }

    const Matrix& getBias() const {
        return m_bias;
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

    Matrix forward(const Matrix& input) {
        Matrix x{ m_accumulator.forward(input) };
        for (auto& layer : m_hiddenLayers) {
            x = layer.forward(x);
        }
        return m_outputLayer.forward(x);
    }

    void backward(const Matrix& error, float lr) {
        Matrix grad{ m_outputLayer.backward(error, lr) };
        for (int i{ static_cast<int>(m_hiddenLayers.size()) - 1 }; i >= 0; i--) {
            grad = m_hiddenLayers[i].backward(grad, lr);
        }
        m_accumulator.backward(grad, lr);
    }

    const Layer<ClampedRelu>& getAccumulator() const {
        return m_accumulator;
    }

    const Layer<ClampedRelu>& getHiddenLayer(int i) const {
        return m_hiddenLayers[i];
    }

    const Layer<Linear>& getOutputLayer() const {
        return m_outputLayer;
    }
};
