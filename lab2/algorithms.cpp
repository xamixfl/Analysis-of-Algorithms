#include "algorithms.hpp"
#include <stdexcept>

Matrix::Matrix(std::size_t r, std::size_t c) : rows(r), cols(c) {
    if (r == 0 || c == 0 || r > 2000 || c > 2000)
        throw std::invalid_argument("Dimensions must be in [1, 2000]");
    data.resize(r * c, 0);
}
void validate(const Matrix& a, const Matrix& b) {
    if (a.cols != b.rows) throw std::invalid_argument("Incompatible dimensions");
    for (const Matrix* m : {&a, &b}) {
        if (!m->rows || !m->cols || m->rows > 2000 || m->cols > 2000 ||
            m->data.size() != m->rows * m->cols)
            throw std::invalid_argument("Invalid matrix storage");
        for (Value x : m->data)
            if (x < -1000000 || x > 1000000)
                throw std::invalid_argument("Elements must be in [-1000000, 1000000]");
    }
}

Matrix classical(const Matrix& a, const Matrix& b) {
    validate(a, b);
    const auto m = a.rows, n = a.cols, q = b.cols;
    Matrix c(m, q);
    for (std::size_t i = 0; i < m; ++i)
        for (std::size_t j = 0; j < q; ++j)
            for (std::size_t k = 0; k < n; ++k)
                c(i, j) += a(i, k) * b(k, j);
    return c;
}

Matrix winograd(const Matrix& a, const Matrix& b) {
    validate(a, b);
    const auto m = a.rows, n = a.cols, q = b.cols;
    Matrix c(m, q);
    std::vector<Value> row(m, 0), col(q, 0);
    for (std::size_t i = 0; i < m; ++i)
        for (std::size_t k = 0; k < n / 2; ++k)
            row[i] = row[i] + a(i, 2*k) * a(i, 2*k+1);
    for (std::size_t j = 0; j < q; ++j)
        for (std::size_t k = 0; k < n / 2; ++k)
            col[j] = col[j] + b(2*k, j) * b(2*k+1, j);
    for (std::size_t i = 0; i < m; ++i)
        for (std::size_t j = 0; j < q; ++j) {
            c(i, j) = -row[i] - col[j];
            for (std::size_t k = 0; k < n / 2; ++k)
                c(i, j) = c(i, j) +
                    (a(i, 2*k) + b(2*k+1, j)) *
                    (a(i, 2*k+1) + b(2*k, j));
        }
    if (n % 2 != 0)
        for (std::size_t i = 0; i < m; ++i)
            for (std::size_t j = 0; j < q; ++j)
                c(i, j) += a(i, n-1) * b(n-1, j);
    return c;
}

Matrix optimized(const Matrix& a, const Matrix& b) {
    validate(a, b);
    const auto m = a.rows, n = a.cols, q = b.cols;
    const auto even = n - n % 2;
    const bool odd = n % 2 != 0;
    Matrix c(m, q);
    std::vector<Value> row(m, 0), col(q, 0);
    for (std::size_t i = 0; i < m; ++i) {
        Value factor = 0;
        for (std::size_t k = 0; k < even; k += 2)
            factor += a(i, k) * a(i, k+1);
        row[i] = factor;
    }
    for (std::size_t j = 0; j < q; ++j) {
        Value factor = 0;
        for (std::size_t k = 0; k < even; k += 2)
            factor += b(k, j) * b(k+1, j);
        col[j] = factor;
    }
    for (std::size_t i = 0; i < m; ++i)
        for (std::size_t j = 0; j < q; ++j) {
            Value sum = -row[i] - col[j];
            for (std::size_t k = 0; k < even; k += 2)
                sum += (a(i, k) + b(k+1, j)) *
                       (a(i, k+1) + b(k, j));
            if (odd) sum += a(i, n-1) * b(n-1, j);
            c(i, j) = sum;
        }
    return c;
}
