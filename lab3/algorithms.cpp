#include "algorithms.hpp"

#include <stdexcept>

namespace lab3 {
namespace {
bool nonzero_digits(Number n) {
    if (n == 0) {
        return true;
    }
    return n % 10 != 0 && nonzero_digits(n / 10);
}

Number reverse_iterative_core(Number n) {
    Number a = 0;
    while (n != 0) {
        const Number d = n % 10;
        const Number q = n / 10;
        a = 10 * a + d;
        n = q;
    }
    return a;
}

Number reverse_recursive_core(Number n, Number a) {
    if (n == 0) {
        return a;
    }
    const Number d = n % 10;
    const Number q = n / 10;
    const Number b = 10 * a + d;
    const Number t = reverse_recursive_core(q, b);
    return t;
}

void require_valid(Number n) {
    if (!is_valid_number(n)) {
        throw std::invalid_argument("Expected 1..9999999999999999999 without zero digits");
    }
}

void require_valid_iterative(Number n) {
    if (n == 0 || n > max_number) {
        throw std::invalid_argument("Number outside the supported range");
    }
    while (n != 0) {
        if (n % 10 == 0) {
            throw std::invalid_argument("Zero digit is not allowed");
        }
        n /= 10;
    }
}
}  // namespace

bool is_valid_number(Number n) {
    return n >= 1 && n <= max_number && nonzero_digits(n);
}

Number reverse_iterative(Number n) {
    require_valid_iterative(n);
    return reverse_iterative_core(n);
}

Number reverse_recursive(Number n) {
    require_valid(n);
    return reverse_recursive_core(n, 0);
}
}  // namespace lab3
