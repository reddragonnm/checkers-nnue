#pragma once

#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

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

inline Matrix mseDiff(const Matrix& output, const Matrix& target) {
    assert(output.numCols() == target.numCols() && output.numRows() == target.numRows());

    Matrix diff{ output + (target * -1.f) };
    float scale{ 2.f / output.numRows() };
    return diff * scale;
}

template <typename Activation>
class Layer {
private:
    static constexpr float b1{ 0.9f };
    static constexpr float b2{ 0.999f };

    int m_numInputs;
    int m_numOutputs;

    Matrix m_lastInput;
    Matrix m_lastPreActivation;

    Matrix m_weights;
    Matrix m_bias;

    Matrix m_momentumW;
    Matrix m_varianceW;
    Matrix m_momentumB;
    Matrix m_varianceB;

    float m_powb1{ 1.f };
    float m_powb2{ 1.f };

    void adamUpdate(Matrix& W, const Matrix& dW, Matrix& m, Matrix& v, float lr, float b1, float b2, float bc1, float bc2, float eps) {
        for (int i{ 0 }; i < W.numRows() * W.numCols(); i++) {
            float g{ dW.data()[i] };
            m.data()[i] = (b1 * m.data()[i]) + ((1.f - b1) * g);
            v.data()[i] = (b2 * v.data()[i]) + ((1.f - b2) * g * g);

            float mHat{ m.data()[i] / bc1 };
            float vHat{ v.data()[i] / bc2 };

            W.data()[i] -= lr * mHat / (std::sqrt(vHat) + eps);
        }
    }

public:
    Layer(int numInputs, int numOutputs) :
        m_numInputs(numInputs),
        m_numOutputs(numOutputs),
        m_weights(numInputs, numOutputs),
        m_bias(1, numOutputs), // going to need broadcast add
        m_momentumW(numInputs, numOutputs),
        m_varianceW(numInputs, numOutputs),
        m_momentumB(1, numOutputs),
        m_varianceB(1, numOutputs)
    {
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
        static constexpr float eps{ 1e-8f };

        Matrix delta{ m_lastPreActivation };
        delta.map(Activation::derivative);
        delta.hadamard(error);

        Matrix dW{ m_lastInput.transpose() * delta };
        Matrix dB{ delta.columnSum() };

        Matrix inputError{ delta * m_weights.transpose() };

        m_powb1 *= b1;
        m_powb2 *= b2;

        float bc1{ 1.0f - m_powb1 };
        float bc2{ 1.0f - m_powb2 };

        adamUpdate(m_weights, dW, m_momentumW, m_varianceW, lr, b1, b2, bc1, bc2, eps);
        adamUpdate(m_bias, dB, m_momentumB, m_varianceB, lr, b1, b2, bc1, bc2, eps);

        return inputError;
    }

    const Matrix& getWeights() const {
        return m_weights;
    }

    const Matrix& getBias() const {
        return m_bias;
    }

    void save(std::ofstream& out) const {
        m_weights.save(out);
        m_bias.save(out);

        m_momentumW.save(out);
        m_varianceW.save(out);
        m_momentumB.save(out);
        m_varianceB.save(out);

        out.write(reinterpret_cast<const char*>(&m_powb1), sizeof(float));
        out.write(reinterpret_cast<const char*>(&m_powb2), sizeof(float));
    }

    void load(std::ifstream& in) {
        m_weights.load(in);
        m_bias.load(in);

        m_momentumW.load(in);
        m_varianceW.load(in);
        m_momentumB.load(in);
        m_varianceB.load(in);

        in.read(reinterpret_cast<char*>(&m_powb1), sizeof(float));
        in.read(reinterpret_cast<char*>(&m_powb2), sizeof(float));

        m_numInputs = m_weights.numRows();
        m_numOutputs = m_weights.numCols();
    }
};

class NNUE {
private:
    Layer<ClampedRelu> m_accumulator;
    std::vector<Layer<ClampedRelu>> m_hiddenLayers;
    Layer<Linear> m_outputLayer;

public:
    int trainGames{ 0 };

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

    void save(const std::string& filename) const {
        fs::path filepath(filename);
        if (filepath.has_parent_path()) {
            fs::create_directories(filepath.parent_path());
        }

        std::ofstream out(filename, std::ios::binary);
        if (!out) throw std::runtime_error("Failed to open file for saving: " + filename);

        out.write(reinterpret_cast<const char*>(&trainGames), sizeof(int));

        m_accumulator.save(out);

        int numHidden{ m_hiddenLayers.size() };
        out.write(reinterpret_cast<const char*>(&numHidden), sizeof(int));
        for (const auto& layer : m_hiddenLayers) {
            layer.save(out);
        }

        m_outputLayer.save(out);

        out.close();
        std::cout << "Checkpoint saved: " << filename << std::endl;
    }

    void load(const std::string& filename) {
        std::ifstream in(filename, std::ios::binary);
        if (!in) throw std::runtime_error("Failed to open file for loading: " + filename);

        in.read(reinterpret_cast<char*>(&trainGames), sizeof(int));

        m_accumulator.load(in);

        int numHidden;
        in.read(reinterpret_cast<char*>(&numHidden), sizeof(int));

        if (numHidden != m_hiddenLayers.size()) {
            throw std::runtime_error("Checkpoint layer count mismatch!");
        }

        for (auto& layer : m_hiddenLayers) {
            layer.load(in);
        }

        m_outputLayer.load(in);

        in.close();
        std::cout << "Checkpoint loaded: " << filename << " (Step: " << trainGames << ")" << std::endl;
    }
};
