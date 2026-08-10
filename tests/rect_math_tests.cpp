#include "pbvp/rect_math.hpp"

#include "test_support.hpp"

#include <array>
#include <cmath>
#include <limits>

namespace {

bool Near(const float left, const float right) {
    return std::fabs(left - right) < 0.001f;
}

} // namespace

void RunRectMathTests() {
    using namespace pbvp;
    FloatRect output{};
    PBVP_CHECK(ConvertUiRectToPixels({100.0f, 50.0f, 500.0f, 350.0f}, {1280.0f, 720.0f},
                                     {1920.0f, 1080.0f}, output));
    PBVP_CHECK(Near(output.left, 150.0f));
    PBVP_CHECK(Near(output.top, 75.0f));
    PBVP_CHECK(Near(output.right, 750.0f));
    PBVP_CHECK(Near(output.bottom, 525.0f));

    PBVP_CHECK(ConvertUiRectToPixels({-10.0f, -20.0f, 1400.0f, 800.0f}, {1280.0f, 720.0f},
                                     {1280.0f, 720.0f}, output));
    PBVP_CHECK(Near(output.left, 0.0f));
    PBVP_CHECK(Near(output.top, 0.0f));
    PBVP_CHECK(Near(output.right, 1280.0f));
    PBVP_CHECK(Near(output.bottom, 720.0f));

    PBVP_CHECK(!ConvertUiRectToPixels({10.0f, 10.0f, 5.0f, 20.0f}, {1280.0f, 720.0f},
                                      {1920.0f, 1080.0f}, output));
    PBVP_CHECK(!ConvertUiRectToPixels({0.0f, 0.0f, 10.0f, 10.0f}, {0.0f, 720.0f},
                                      {1920.0f, 1080.0f}, output));

    struct MatrixCase {
        PixelExtent ui_extent;
        PixelExtent backbuffer_extent;
        FloatRect expected;
    };
    constexpr FloatRect accepted_ui_rect{42.0f, 323.0f, 426.0f, 539.0f};
    constexpr std::array<MatrixCase, 6> matrix{{
        {{1280.0f, 960.0f}, {1280.0f, 960.0f}, {42.0f, 323.0f, 426.0f, 539.0f}},
        {{1706.666667f, 960.0f}, {1280.0f, 720.0f}, {31.5f, 242.25f, 319.5f, 404.25f}},
        {{1706.666667f, 960.0f}, {1920.0f, 1080.0f}, {47.25f, 363.375f, 479.25f, 606.375f}},
        {{1706.666667f, 960.0f}, {2560.0f, 1440.0f}, {63.0f, 484.5f, 639.0f, 808.5f}},
        {{1536.0f, 960.0f}, {1920.0f, 1200.0f}, {52.5f, 403.75f, 532.5f, 673.75f}},
        {{2293.333333f, 960.0f}, {3440.0f, 1440.0f}, {63.0f, 484.5f, 639.0f, 808.5f}},
    }};
    for (const auto& test_case : matrix) {
        PBVP_CHECK(ConvertUiRectToPixels(
            accepted_ui_rect, test_case.ui_extent, test_case.backbuffer_extent, output));
        PBVP_CHECK(Near(output.left, test_case.expected.left));
        PBVP_CHECK(Near(output.top, test_case.expected.top));
        PBVP_CHECK(Near(output.right, test_case.expected.right));
        PBVP_CHECK(Near(output.bottom, test_case.expected.bottom));
    }

    output = {1.0f, 2.0f, 3.0f, 4.0f};
    const float infinity = std::numeric_limits<float>::infinity();
    PBVP_CHECK(!ConvertUiRectToPixels(
        {0.0f, 0.0f, infinity, 100.0f}, {1280.0f, 720.0f}, {1920.0f, 1080.0f}, output));
    PBVP_CHECK(Near(output.left, 0.0f));
    PBVP_CHECK(Near(output.top, 0.0f));
    PBVP_CHECK(Near(output.right, 0.0f));
    PBVP_CHECK(Near(output.bottom, 0.0f));
}
