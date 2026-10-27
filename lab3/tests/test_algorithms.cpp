#include "../algorithms.hpp"

#include <iostream>
#include <random>
#include <stdexcept>

int main() {
    using lab3::Number;
    std::size_t cases = 0;
    auto check = [&](Number n, Number expected) {
        if (lab3::reverse_iterative(n) != expected ||
            lab3::reverse_recursive(n) != expected ||
            lab3::reverse_recursive(expected) != n) {
            throw std::runtime_error("Incorrect reversal");
        }
        ++cases;
    };
    check(1, 1);
    check(9, 9);
    check(12, 21);
    check(179, 971);
    check(11111, 11111);
    check(12321, 12321);
    check(123456789, 987654321);
    check(987654321, 123456789);
    check(1234567891234567891ULL, 1987654321987654321ULL);
    check(lab3::max_number, lab3::max_number);
    for (Number n : {0ULL, 10ULL, 101ULL, 1203ULL, 10000000000000000000ULL,
                     11111111111111111111ULL, 18446744073709551615ULL}) {
        for (auto algorithm : {lab3::reverse_iterative, lab3::reverse_recursive}) {
            bool rejected = false;
            try {
                (void)algorithm(n);
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            if (!rejected) {
                throw std::runtime_error("Invalid number accepted");
            }
        }
        ++cases;
    }
    // Independent oracle: construct n and its expected reverse together.
    // No reversal routine is used to calculate the expected value.
    std::mt19937 engine(20261026);
    for (unsigned length = 1; length <= 19; ++length) {
        for (unsigned sample = 0; sample < 1000; ++sample) {
            Number n = 0;
            Number expected = 0;
            Number place = 1;
            for (unsigned i = 0; i < length; ++i) {
                const Number digit = 1 + engine() % 9;
                n = 10 * n + digit;
                expected += digit * place;
                if (i + 1 < length) {
                    place *= 10;
                }
            }
            check(n, expected);
        }
    }
    std::cout << "PASS: " << cases << " algorithm cases (19010 valid, 7 invalid).\n";
}
