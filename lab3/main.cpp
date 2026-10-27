#include "algorithms.hpp"

#include <cctype>
#include <cstring>
#include <exception>
#include <iostream>

namespace {
// Parse one input line using integer arithmetic, without a digit string.
bool read_number(std::istream& input, lab3::Number& value) {
    value = 0;
    bool found = false;
    bool finished = false;
    char ch = 0;
    while (input.get(ch)) {
        if (ch == '\n' || ch == '\r') {
            return found;
        }
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (found) {
                finished = true;
            }
            continue;
        }
        if (finished || ch < '1' || ch > '9') {
            return false;
        }
        const auto digit = static_cast<lab3::Number>(ch - '0');
        if (value > (lab3::max_number - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
        found = true;
    }
    return found && !input.bad();
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2 || (std::strcmp(argv[1], "iterative") != 0 &&
                      std::strcmp(argv[1], "recursive") != 0 &&
                      std::strcmp(argv[1], "both") != 0)) {
        std::cerr << "Usage: reverse_number.exe iterative|recursive|both\n"
                     "Read one positive integer (up to 19 digits, no zeros) from stdin.\n";
        return 2;
    }
    lab3::Number n = 0;
    if (!read_number(std::cin, n)) {
        std::cerr << "Invalid input: expected one integer, 1..9999999999999999999, no zeros.\n";
        return 1;
    }
    try {
        if (std::strcmp(argv[1], "iterative") == 0) {
            std::cout << lab3::reverse_iterative(n) << '\n';
        } else if (std::strcmp(argv[1], "recursive") == 0) {
            std::cout << lab3::reverse_recursive(n) << '\n';
        } else {
            const auto iterative = lab3::reverse_iterative(n);
            const auto recursive = lab3::reverse_recursive(n);
            if (iterative != recursive) {
                std::cerr << "Algorithms disagree.\n";
                return 3;
            }
            std::cout << "iterative: " << iterative << '\n'
                      << "recursive: " << recursive << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
