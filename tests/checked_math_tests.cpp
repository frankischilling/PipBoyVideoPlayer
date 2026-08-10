#include "pbvp/checked_math.hpp"

#include "test_support.hpp"

#include <limits>

void RunCheckedMathTests() {
    std::size_t output = 99u;
    PBVP_CHECK(pbvp::CheckedAddSize(10u, 20u, output));
    PBVP_CHECK(output == 30u);
    PBVP_CHECK(!pbvp::CheckedAddSize((std::numeric_limits<std::size_t>::max)(), 1u, output));
    PBVP_CHECK(output == 0u);

    PBVP_CHECK(pbvp::CheckedMultiplySize(1920u, 4u, output));
    PBVP_CHECK(output == 7680u);
    PBVP_CHECK(!pbvp::CheckedMultiplySize((std::numeric_limits<std::size_t>::max)(), 2u, output));
    PBVP_CHECK(output == 0u);

    PBVP_CHECK(pbvp::CheckedAlignSize(17u, 16u, output));
    PBVP_CHECK(output == 32u);
    PBVP_CHECK(pbvp::CheckedAlignSize(32u, 16u, output));
    PBVP_CHECK(output == 32u);
    PBVP_CHECK(!pbvp::CheckedAlignSize(1u, 0u, output));
    PBVP_CHECK(!pbvp::CheckedAlignSize((std::numeric_limits<std::size_t>::max)(), 2u, output));

    PBVP_CHECK(pbvp::CheckedUint64ToSize(1024u, output));
    PBVP_CHECK(output == 1024u);
    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        PBVP_CHECK(!pbvp::CheckedUint64ToSize(
            static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()) + 1u, output));
    }
}
