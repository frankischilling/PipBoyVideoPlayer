#include "test_support.hpp"

#include <cstdio>

void RunHookProbeTests();
void RunFrameCadenceTests();
void RunRecreateGateTests();
void RunRecreateContextTests();
void RunRecreateObserverTests();
void RunRecreateResultTests();
void RunRectMathTests();

int main() {
    RunFrameCadenceTests();
    RunHookProbeTests();
    RunRecreateContextTests();
    RunRecreateGateTests();
    RunRecreateObserverTests();
    RunRecreateResultTests();
    RunRectMathTests();
    if (pbvp::test::failures != 0) {
        std::fprintf(stderr, "%d test check(s) failed\n", pbvp::test::failures);
        return 1;
    }
    std::puts("all checks passed");
    return 0;
}
