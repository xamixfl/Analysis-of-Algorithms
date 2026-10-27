#pragma once

#include <cstdint>

namespace lab3 {
using Number = std::uint64_t;
constexpr Number max_number = 9999999999999999999ULL;

bool is_valid_number(Number n);
Number reverse_iterative(Number n);
Number reverse_recursive(Number n);
}  // namespace lab3
