#pragma once

#include <cstdio>

namespace pbvp::test {

inline int failures = 0;

inline void Check(const bool condition, const char* expression, const char* file, const int line) {
    if (!condition) {
        std::fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
        ++failures;
    }
}

} // namespace pbvp::test

#define PBVP_CHECK(expression) ::pbvp::test::Check((expression), #expression, __FILE__, __LINE__)
