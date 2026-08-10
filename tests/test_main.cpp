#include "test_support.hpp"

#include <cstdio>

void RunFrameCadenceTests();
void RunRectMathTests();
void RunTextureContractTests();

int main() {
    RunFrameCadenceTests();
    RunRectMathTests();
    RunTextureContractTests();
    if (pbvp::test::failures != 0) {
        std::fprintf(stderr, "%d test check(s) failed\n", pbvp::test::failures);
        return 1;
    }
    std::puts("all checks passed");
    return 0;
}
