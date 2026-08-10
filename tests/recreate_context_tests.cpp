#include "pbvp/recreate_context.hpp"

#include "test_support.hpp"

#include <string_view>

void RunRecreateContextTests() {
    using namespace pbvp;

    RecreateContext context{
        0x12345678u,
        0u,
        0u,
        1920u,
        1080u,
        1920u,
        1080u,
    };
    PBVP_CHECK(ValidateRecreateContext(context) == RecreateContextResult::ready);
    PBVP_CHECK(RecreateContextResultName(RecreateContextResult::ready) ==
               std::string_view("ready"));

    context.renderer = 0u;
    PBVP_CHECK(ValidateRecreateContext(context) ==
               RecreateContextResult::renderer_unavailable);
    context.renderer = 0x12345678u;

    context.requested_width = 1920u;
    PBVP_CHECK(ValidateRecreateContext(context) ==
               RecreateContextResult::request_in_progress);
    context.requested_width = 0u;
    context.requested_height = 1080u;
    PBVP_CHECK(ValidateRecreateContext(context) ==
               RecreateContextResult::request_in_progress);
    context.requested_height = 0u;

    context.active_width = kMinimumRecreateWidth - 1u;
    PBVP_CHECK(ValidateRecreateContext(context) ==
               RecreateContextResult::active_size_out_of_range);
    context.active_width = 1920u;
    context.active_height = kMaximumRecreateExtent + 1u;
    PBVP_CHECK(ValidateRecreateContext(context) ==
               RecreateContextResult::active_size_out_of_range);
    context.active_height = 1080u;

    context.backbuffer_height = kMinimumRecreateHeight - 1u;
    PBVP_CHECK(ValidateRecreateContext(context) ==
               RecreateContextResult::backbuffer_size_out_of_range);
    context.backbuffer_height = 1080u;
    context.backbuffer_width = 2560u;
    PBVP_CHECK(ValidateRecreateContext(context) ==
               RecreateContextResult::size_mismatch);
}
