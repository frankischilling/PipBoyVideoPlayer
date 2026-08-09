#include "pbvp/rect_math.hpp"

#include "test_support.hpp"

#include <cmath>

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
}
