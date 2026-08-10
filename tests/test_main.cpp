#include "test_support.hpp"

#include <cstdio>

void RunHookProbeTests();
void RunRecreateGateTests();
void RunRecreateResultTests();
void RunRectMathTests();

int main() {
    RunHookProbeTests();
    RunRecreateGateTests();
    RunRecreateResultTests();
    RunRectMathTests();
    if (pbvp::test::failures != 0) {
        std::fprintf(stderr, "%d test check(s) failed\n", pbvp::test::failures);
        return 1;
    }
    std::puts("all checks passed");
    return 0;
}
