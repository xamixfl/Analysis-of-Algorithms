#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

using Value = std::int64_t;
struct Matrix {
    std::size_t rows, cols;
    std::vector<Value> data;
    Matrix(std::size_t r, std::size_t c);
    Value& operator()(std::size_t i, std::size_t j) { return data[i * cols + j]; }
    Value operator()(std::size_t i, std::size_t j) const { return data[i * cols + j]; }
    bool operator==(const Matrix& b) const {
        return rows == b.rows && cols == b.cols && data == b.data;
    }
};
void validate(const Matrix& a, const Matrix& b);
Matrix classical(const Matrix& a, const Matrix& b);
Matrix winograd(const Matrix& a, const Matrix& b);
Matrix optimized(const Matrix& a, const Matrix& b);
