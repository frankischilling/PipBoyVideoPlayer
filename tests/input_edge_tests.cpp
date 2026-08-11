#include "pbvp/input_edge.hpp"

#include "test_support.hpp"

void RunInputEdgeTests() {
    bool previous_down = false;
    bool armed = false;

    PBVP_CHECK(!pbvp::ConsumeArmedPress(true, previous_down, armed));
    PBVP_CHECK(previous_down);
    PBVP_CHECK(!armed);

    PBVP_CHECK(!pbvp::ConsumeArmedPress(false, previous_down, armed));
    PBVP_CHECK(!previous_down);
    PBVP_CHECK(armed);

    PBVP_CHECK(pbvp::ConsumeArmedPress(true, previous_down, armed));
    PBVP_CHECK(previous_down);
    PBVP_CHECK(!armed);
    PBVP_CHECK(!pbvp::ConsumeArmedPress(true, previous_down, armed));

    PBVP_CHECK(!pbvp::ConsumeArmedPress(false, previous_down, armed));
    PBVP_CHECK(armed);
    PBVP_CHECK(pbvp::ConsumeArmedPress(true, previous_down, armed));
}
