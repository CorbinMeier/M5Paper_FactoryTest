// Host-side tests for the hardware-independent logic (issue #17).
//
// Only code that compiles without Arduino or M5EPD can live here -- currently
// the battery discharge curve and the rect algebra. As more logic is split out
// of the hardware-touching translation units, add it.

#include <unity.h>

#include "../../lib/m5paper-ui/src/core/battery_curve.h"
#include "../../lib/m5paper-ui/src/core/geometry.h"

using namespace m5ui;

// --------------------------------------------------------- battery curve ----

void test_battery_clamps_below_empty(void) {
    TEST_ASSERT_EQUAL_UINT8(0, BatteryPercentFromMillivolts(0));
    TEST_ASSERT_EQUAL_UINT8(0, BatteryPercentFromMillivolts(3000));
    TEST_ASSERT_EQUAL_UINT8(0, BatteryPercentFromMillivolts(kBatteryEmptyMv));
}

void test_battery_clamps_above_full(void) {
    TEST_ASSERT_EQUAL_UINT8(100, BatteryPercentFromMillivolts(kBatteryFullMv));
    TEST_ASSERT_EQUAL_UINT8(100, BatteryPercentFromMillivolts(5000));
}

void test_battery_hits_breakpoints_exactly(void) {
    TEST_ASSERT_EQUAL_UINT8(25, BatteryPercentFromMillivolts(3700));
    TEST_ASSERT_EQUAL_UINT8(45, BatteryPercentFromMillivolts(3800));
    TEST_ASSERT_EQUAL_UINT8(78, BatteryPercentFromMillivolts(4000));
    TEST_ASSERT_EQUAL_UINT8(97, BatteryPercentFromMillivolts(4200));
}

void test_battery_is_monotonic(void) {
    uint8_t previous = 0;
    for (uint32_t mv = 3200; mv <= 4400; mv += 5) {
        const uint8_t pct = BatteryPercentFromMillivolts(mv);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT8(previous, pct);
        previous = pct;
    }
}

void test_battery_interpolates_between_breakpoints(void) {
    // Midway between 3700 (25%) and 3800 (45%) should read about 35%.
    const uint8_t pct = BatteryPercentFromMillivolts(3750);
    TEST_ASSERT_UINT8_WITHIN(1, 35, pct);
}

void test_battery_curve_beats_linear_in_the_flat_middle(void) {
    // The whole point of the curve: a linear 3300-4350 map reports ~38% at
    // 3700 mV, where a real cell is closer to a quarter charged.
    const uint8_t linear = (uint8_t)(((3700 - 3300) * 100) / (4350 - 3300));
    TEST_ASSERT_TRUE(BatteryPercentFromMillivolts(3700) < linear);
}

// ------------------------------------------------------------- geometry ----

void test_rect_union_ignores_empty(void) {
    const Rect a{10, 10, 100, 100};
    const Rect empty;
    TEST_ASSERT_EQUAL_INT16(100, a.Union(empty).w);
    TEST_ASSERT_EQUAL_INT16(10, empty.Union(a).x);
}

void test_rect_union_covers_both(void) {
    const Rect u = Rect{0, 0, 10, 10}.Union(Rect{90, 90, 10, 10});
    TEST_ASSERT_EQUAL_INT16(0, u.x);
    TEST_ASSERT_EQUAL_INT16(100, u.w);
    TEST_ASSERT_EQUAL_INT16(100, u.h);
}

void test_rect_intersection_is_empty_when_disjoint(void) {
    // Braced initialisers cannot be written inline inside a Unity assertion --
    // the preprocessor reads their commas as macro argument separators.
    const Rect a{0, 0, 10, 10};
    const Rect b{50, 50, 10, 10};
    TEST_ASSERT_TRUE(a.Intersection(b).IsEmpty());
}

void test_rect_contains_excludes_far_edges(void) {
    const Rect r{10, 10, 20, 20};
    TEST_ASSERT_TRUE(r.Contains(10, 10));
    TEST_ASSERT_TRUE(r.Contains(29, 29));
    TEST_ASSERT_FALSE(r.Contains(30, 30)); // right/bottom are exclusive
    TEST_ASSERT_FALSE(r.Contains(9, 10));
}

void test_rect_aligned_out_snaps_outward(void) {
    const Rect a = Rect{7, 7, 5, 5}.AlignedOut(60);
    TEST_ASSERT_EQUAL_INT16(0, a.x);
    TEST_ASSERT_EQUAL_INT16(0, a.y);
    TEST_ASSERT_EQUAL_INT16(60, a.w);
    TEST_ASSERT_EQUAL_INT16(60, a.h);
}

void test_rect_clipped_to_panel(void) {
    const Rect c = Rect{-50, -50, 200, 200}.Clipped();
    TEST_ASSERT_EQUAL_INT16(0, c.x);
    TEST_ASSERT_EQUAL_INT16(150, c.w);
}

void test_framebuffer_size_matches_geometry(void) {
    TEST_ASSERT_EQUAL_UINT32(259200, kFramebufferBytes);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_battery_clamps_below_empty);
    RUN_TEST(test_battery_clamps_above_full);
    RUN_TEST(test_battery_hits_breakpoints_exactly);
    RUN_TEST(test_battery_is_monotonic);
    RUN_TEST(test_battery_interpolates_between_breakpoints);
    RUN_TEST(test_battery_curve_beats_linear_in_the_flat_middle);

    RUN_TEST(test_rect_union_ignores_empty);
    RUN_TEST(test_rect_union_covers_both);
    RUN_TEST(test_rect_intersection_is_empty_when_disjoint);
    RUN_TEST(test_rect_contains_excludes_far_edges);
    RUN_TEST(test_rect_aligned_out_snaps_outward);
    RUN_TEST(test_rect_clipped_to_panel);
    RUN_TEST(test_framebuffer_size_matches_geometry);

    return UNITY_END();
}

void setUp(void) {}
void tearDown(void) {}
