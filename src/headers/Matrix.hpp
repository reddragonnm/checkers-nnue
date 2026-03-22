#pragma once

#include <vector>
#include <cassert>
#include <random>

class Matrix {
private:
    int m_rows{};
    int m_cols{};

    std::vector<float> m_data{};
public:
    Matrix() = default;

    Matrix(int rows, int cols) : m_rows(rows), m_cols(cols), m_data(cols* rows, 0.f) {}

    int numRows() const {
        return m_rows;
    }

    int numCols() const {
        return m_cols;
    }

    void randomInit(unsigned int seed = std::random_device{}()) {
        std::mt19937 rng{ seed };
        std::normal_distribution<float> dist{ 0.f, std::sqrt(2.f / m_rows) };
        for (int i{ 0 }; i < m_rows * m_cols; i++)
            m_data[i] = dist(rng);
    }

    void map(auto f) {
        for (int i{ 0 }; i < m_rows * m_cols; i++) {
            m_data[i] = f(m_data[i]);
        }
    }

    float operator()(int row, int col) const {
        assert(row >= 0 && row < m_rows && col >= 0 && col < m_cols);
        return m_data[row * m_cols + col];
    }

    float& operator()(int row, int col) {
        assert(row >= 0 && row < m_rows && col >= 0 && col < m_cols);
        return m_data[row * m_cols + col];
    }

    Matrix operator*(const Matrix& other) const {
        assert(m_cols == other.m_rows);
        Matrix result(m_rows, other.m_cols);

        for (int i{ 0 }; i < m_rows; i++) {
            for (int k{ 0 }; k < m_cols; k++) { // cache stuff
                float temp{ m_data[i * m_cols + k] };
                for (int j{ 0 }; j < other.m_cols; j++) {
                    result.m_data[i * other.m_cols + j] += temp * other.m_data[k * other.m_cols + j];
                }
            }
        }

        return result;
    }

    Matrix operator+(const Matrix& other) const {
        Matrix result{ m_rows, m_cols };

        if (m_cols == other.m_cols && m_rows == other.m_rows) {
            for (int i{ 0 }; i < m_rows * m_cols; i++) {
                result.m_data[i] = m_data[i] + other.m_data[i];
            }
        }
        else if (other.m_rows == 1 && other.m_cols == m_cols) { // broadcast add
            for (int i{ 0 }; i < m_rows; i++) {
                for (int j{ 0 }; j < m_cols; j++) {
                    result.m_data[i * m_cols + j] = m_data[i * m_cols + j] + other.m_data[j];
                }
            }
        }
        else {
            throw std::invalid_argument("Matrix dimensions do not match for addition");
        }

        return result;
    }

    Matrix operator*(float scalar) const {
        Matrix result(m_rows, m_cols);

        for (int i{ 0 }; i < m_rows * m_cols; i++) {
            result.m_data[i] = m_data[i] * scalar;
        }

        return result;
    }

    void hadamard(const Matrix& other) {
        assert(m_cols == other.m_cols && m_rows == other.m_rows);

        for (int i{ 0 }; i < m_rows * m_cols; i++) {
            m_data[i] *= other.m_data[i];
        }
    }

    Matrix transpose() const {
        Matrix result{ m_cols, m_rows };

        for (int i{ 0 }; i < m_rows; i++) {
            for (int j{ 0 }; j < m_cols; j++) {
                result.m_data[j * m_rows + i] = m_data[i * m_cols + j];
            }
        }

        return result;
    }

    Matrix columnSum() const {
        Matrix result{ 1, m_cols };

        for (int i{ 0 }; i < m_rows; i++) {
            for (int j{ 0 }; j < m_cols; j++) {
                result.m_data[j] += m_data[i * m_cols + j];
            }
        }

        return result;
    };
};
