#pragma once

#include <vector>

template <typename T>
class Matrix {
private:
    int m_cols;
    int m_rows;

    std::vector<T> m_data;
public:
    Matrix(int cols, int rows) : m_cols(cols), m_rows(rows) {
        m_data.resize(cols * rows);
    }

    template <typename Func>
    void map(Func f) {
        for (int i{ 0 }; i < m_rows * m_cols; i++) {
            m_data[i] = f(m_data[i]);
        }
    }

    Matrix<T> operator*(const Matrix<T>& other) const {
    }

    Matrix<T> operator+(const Matrix<T>& other) const {
    }
};
