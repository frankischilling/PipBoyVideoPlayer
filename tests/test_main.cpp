#include "test_support.hpp"

#include <cstdio>

void RunFrameCadenceTests();
void RunAudioCallbackStateTests();
void RunCheckedMathTests();
void RunMediaLimitTests();
void RunPlaybackClockTests();
void RunBoundedQueueTests();
void RunRectMathTests();
void RunTextureContractTests();

int main() {
    RunAudioCallbackStateTests();
    RunCheckedMathTests();
    RunMediaLimitTests();
    RunPlaybackClockTests();
    RunBoundedQueueTests();
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
