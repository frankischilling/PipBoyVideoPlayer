#include "pbvp/recreate_result.hpp"

#include "test_support.hpp"

void RunRecreateResultTests() {
    using namespace pbvp;
    PBVP_CHECK(ClassifyRecreateResult(0u) == RecreateResult::failed);
    PBVP_CHECK(ClassifyRecreateResult(1u) == RecreateResult::recovered);
    PBVP_CHECK(ClassifyRecreateResult(2u) == RecreateResult::requested);
    PBVP_CHECK(ClassifyRecreateResult(3u) == RecreateResult::unknown);
    PBVP_CHECK(ClassifyRecreateResult(0xFFFFFFFFu) == RecreateResult::unknown);
}
