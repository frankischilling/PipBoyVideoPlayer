#include "pbvp/recreate_context.hpp"

namespace pbvp {
namespace {

bool IsBoundedExtent(const std::uint32_t width, const std::uint32_t height) noexcept {
    return width >= kMinimumRecreateWidth && width <= kMaximumRecreateExtent &&
           height >= kMinimumRecreateHeight && height <= kMaximumRecreateExtent;
}

} // namespace

RecreateContextResult ValidateRecreateContext(const RecreateContext& context) noexcept {
    if (context.renderer == 0u) {
        return RecreateContextResult::renderer_unavailable;
    }
    if (context.requested_width != 0u || context.requested_height != 0u) {
        return RecreateContextResult::request_in_progress;
    }
    if (!IsBoundedExtent(context.active_width, context.active_height)) {
        return RecreateContextResult::active_size_out_of_range;
    }
    if (!IsBoundedExtent(context.backbuffer_width, context.backbuffer_height)) {
        return RecreateContextResult::backbuffer_size_out_of_range;
    }
    if (context.active_width != context.backbuffer_width ||
        context.active_height != context.backbuffer_height) {
        return RecreateContextResult::size_mismatch;
    }
    return RecreateContextResult::ready;
}

const char* RecreateContextResultName(const RecreateContextResult result) noexcept {
    switch (result) {
        case RecreateContextResult::ready:
            return "ready";
        case RecreateContextResult::renderer_unavailable:
            return "renderer-unavailable";
        case RecreateContextResult::request_in_progress:
            return "request-in-progress";
        case RecreateContextResult::active_size_out_of_range:
            return "active-size-out-of-range";
        case RecreateContextResult::backbuffer_size_out_of_range:
            return "backbuffer-size-out-of-range";
        case RecreateContextResult::size_mismatch:
            return "size-mismatch";
    }
    return "unknown";
}

} // namespace pbvp
