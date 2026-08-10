#include "pbvp/recreate_result.hpp"

namespace pbvp {

RecreateResult ClassifyRecreateResult(const std::uint32_t value) noexcept {
    switch (value) {
        case 0u:
            return RecreateResult::failed;
        case 1u:
            return RecreateResult::recovered;
        case 2u:
            return RecreateResult::requested;
        default:
            return RecreateResult::unknown;
    }
}

} // namespace pbvp
