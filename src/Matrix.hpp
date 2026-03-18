#pragma once

#include <vector>
#include <cassert>

template <typename T>
class Matrix {
private:
    int m_cols;
    int m_rows;

    std::vector<T> m_data;
public:
    Matrix(int cols, int rows) : m_cols(cols), m_rows(rows), m_data(cols* rows) {
    }

    void map(auto f) {
        for (int i{ 0 }; i < m_rows * m_cols; i++) {
            m_data[i] = f(m_data[i]);
        }
    }

    T& operator[](int row, int col) {
        assert(row >= 0 && row < m_rows && col >= 0 && col < m_cols);
        return m_data[row * m_cols + col];
    }

    Matrix<T> operator*(const Matrix<T>& other) const {
        assert(m_cols == other.m_rows);
        Matrix<T> result(other.m_cols, m_rows);

        for (int i{ 0 }; i < m_rows; i++) {
            for (int k{ 0 }; k < m_cols; k++) { // cache stuff
                T temp{ m_data[i * m_cols + k] };
                for (int j{ 0 }; j < other.m_cols; j++) {
                    result.m_data[i * other.m_cols + j] += temp * other.m_data[k * other.m_cols + j];
                }
            }
        }

        return result;
    }

    Matrix<T> operator+(const Matrix<T>& other) const {
        Matrix<T> result(m_cols, m_rows);

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
};
