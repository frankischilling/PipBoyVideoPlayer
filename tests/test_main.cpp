#include "test_support.hpp"

#include <cstdio>

void RunFrameCadenceTests();
void RunAudioCallbackStateTests();
void RunCheckedMathTests();
void RunConfigurationTests();
void RunControllerInputTests();
void RunInputEdgeTests();
void RunInputPromptTests();
void RunLogPrivacyTests();
void RunMediaLimitTests();
void RunMediaCatalogTests();
void RunMenuVtableValidationTests();
void RunMenuKeyboardTests();
void RunPlaybackClockTests();
void RunPlaybackStateTests();
void RunBoundedQueueTests();
void RunRectMathTests();
void RunTextureContractTests();
void RunVideoScalerTests();
void RunVideoSchedulerTests();

int main() {
    RunAudioCallbackStateTests();
    RunCheckedMathTests();
    RunConfigurationTests();
    RunControllerInputTests();
    RunInputEdgeTests();
    RunInputPromptTests();
    RunLogPrivacyTests();
    RunMediaLimitTests();
    RunMediaCatalogTests();
    RunMenuKeyboardTests();
    RunMenuVtableValidationTests();
    RunPlaybackClockTests();
    RunPlaybackStateTests();
    RunBoundedQueueTests();
    RunFrameCadenceTests();
    RunRectMathTests();
    RunTextureContractTests();
    RunVideoScalerTests();
    RunVideoSchedulerTests();
    if (pbvp::test::failures != 0) {
        std::fprintf(stderr, "%d test check(s) failed\n", pbvp::test::failures);
        return 1;
    }
    std::puts("all checks passed");
    return 0;
}
