// Host-side tests for the hardware-independent logic (issue #17).
//
// Only code that compiles without Arduino or M5EPD can live here -- currently
// the battery discharge curve, the rect algebra, and the calendar math. As
// more logic is split out of the hardware-touching translation units, add it.

#include <unity.h>

#include "../../lib/m5paper-ui/src/core/battery_curve.h"
#include "../../lib/m5paper-ui/src/core/calendar_math.h"
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

// ---------------------------------------------------------- burst median ----

void test_median_of_empty_is_zero(void) {
    uint32_t none[1] = {0};
    TEST_ASSERT_EQUAL_UINT32(0, MedianMillivolts(none, 0));
    TEST_ASSERT_EQUAL_UINT32(0, MedianMillivolts(nullptr, 8));
}

void test_median_of_one_is_itself(void) {
    uint32_t one[1] = {3812};
    TEST_ASSERT_EQUAL_UINT32(3812, MedianMillivolts(one, 1));
}

void test_median_of_odd_count(void) {
    uint32_t s[5] = {3800, 3700, 3900, 3750, 3850};
    TEST_ASSERT_EQUAL_UINT32(3800, MedianMillivolts(s, 5));
}

void test_median_of_even_count_averages_the_middle(void) {
    // Sorted: 3700 3750 3800 3900 -> (3750 + 3800 + 1) / 2
    uint32_t s[4] = {3800, 3700, 3900, 3750};
    TEST_ASSERT_EQUAL_UINT32(3775, MedianMillivolts(s, 4));
}

void test_median_rejects_a_wild_outlier(void) {
    // The case this exists for: one bogus SAR conversion among eight good ones.
    uint32_t s[8] = {3800, 3805, 3798, 3802, 65535, 3801, 3799, 3803};
    const uint32_t m = MedianMillivolts(s, 8);
    TEST_ASSERT_TRUE(m >= 3798 && m <= 3805);
}

void test_mean_would_not_have_rejected_it(void) {
    // Same burst through a mean pegs the gauge at 100% -- this is why the
    // rolling average the cache used to carry was not good enough on its own.
    const uint32_t s[8] = {3800, 3805, 3798, 3802, 65535, 3801, 3799, 3803};
    uint32_t sum = 0;
    for (int i = 0; i < 8; ++i) sum += s[i];
    TEST_ASSERT_TRUE((sum / 8) > kBatteryFullMv);
}

void test_median_sorts_in_place(void) {
    uint32_t s[4] = {3900, 3700, 3800, 3750};
    MedianMillivolts(s, 4);
    TEST_ASSERT_EQUAL_UINT32(3700, s[0]);
    TEST_ASSERT_EQUAL_UINT32(3900, s[3]);
}

void test_median_feeds_a_stable_percent(void) {
    // Eight noisy conversions around 3800 mV should land near the 45%
    // breakpoint rather than anywhere in the band a single conversion allows.
    uint32_t s[8] = {3792, 3808, 3801, 3797, 3804, 3799, 3802, 3798};
    const uint8_t pct = BatteryPercentFromMillivolts(MedianMillivolts(s, 8));
    TEST_ASSERT_UINT8_WITHIN(2, 45, pct);
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

// -------------------------------------------------------------- calendar ----

void test_leap_year_divisible_by_four(void) {
    TEST_ASSERT_TRUE(IsLeapYear(2024));
    TEST_ASSERT_TRUE(IsLeapYear(2000));
    TEST_ASSERT_FALSE(IsLeapYear(1900));
    TEST_ASSERT_FALSE(IsLeapYear(2026));
}

void test_days_in_month_handles_february(void) {
    TEST_ASSERT_EQUAL_UINT8(29, DaysInMonth(2024, 2));
    TEST_ASSERT_EQUAL_UINT8(28, DaysInMonth(2026, 2));
    TEST_ASSERT_EQUAL_UINT8(28, DaysInMonth(1900, 2));
}

void test_days_in_month_matches_calendar(void) {
    TEST_ASSERT_EQUAL_UINT8(31, DaysInMonth(2026, 1));
    TEST_ASSERT_EQUAL_UINT8(30, DaysInMonth(2026, 4));
    TEST_ASSERT_EQUAL_UINT8(31, DaysInMonth(2026, 12));
}

// Reference weekdays cross-checked against `date -d <date> +%A`.
void test_day_of_week_known_dates(void) {
    TEST_ASSERT_EQUAL_UINT8(3, DayOfWeek(2026, 8, 12));  // Wednesday
    TEST_ASSERT_EQUAL_UINT8(6, DayOfWeek(2000, 1, 1));   // Saturday
    TEST_ASSERT_EQUAL_UINT8(4, DayOfWeek(2026, 1, 1));   // Thursday
    TEST_ASSERT_EQUAL_UINT8(6, DayOfWeek(2026, 2, 28));  // Saturday
    TEST_ASSERT_EQUAL_UINT8(1, DayOfWeek(1900, 1, 1));   // Monday
}

void test_day_of_week_advances_by_one_each_day(void) {
    uint8_t previous = DayOfWeek(2026, 8, 1);
    for (uint8_t day = 2; day <= 31; ++day) {
        const uint8_t w = DayOfWeek(2026, 8, day);
        TEST_ASSERT_EQUAL_UINT8((previous + 1) % 7, w);
        previous = w;
    }
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_battery_clamps_below_empty);
    RUN_TEST(test_battery_clamps_above_full);
    RUN_TEST(test_battery_hits_breakpoints_exactly);
    RUN_TEST(test_battery_is_monotonic);
    RUN_TEST(test_battery_interpolates_between_breakpoints);
    RUN_TEST(test_battery_curve_beats_linear_in_the_flat_middle);

    RUN_TEST(test_median_of_empty_is_zero);
    RUN_TEST(test_median_of_one_is_itself);
    RUN_TEST(test_median_of_odd_count);
    RUN_TEST(test_median_of_even_count_averages_the_middle);
    RUN_TEST(test_median_rejects_a_wild_outlier);
    RUN_TEST(test_mean_would_not_have_rejected_it);
    RUN_TEST(test_median_sorts_in_place);
    RUN_TEST(test_median_feeds_a_stable_percent);

    RUN_TEST(test_rect_union_ignores_empty);
    RUN_TEST(test_rect_union_covers_both);
    RUN_TEST(test_rect_intersection_is_empty_when_disjoint);
    RUN_TEST(test_rect_contains_excludes_far_edges);
    RUN_TEST(test_rect_aligned_out_snaps_outward);
    RUN_TEST(test_rect_clipped_to_panel);
    RUN_TEST(test_framebuffer_size_matches_geometry);

    RUN_TEST(test_leap_year_divisible_by_four);
    RUN_TEST(test_days_in_month_handles_february);
    RUN_TEST(test_days_in_month_matches_calendar);
    RUN_TEST(test_day_of_week_known_dates);
    RUN_TEST(test_day_of_week_advances_by_one_each_day);

    return UNITY_END();
}

void setUp(void) {}
void tearDown(void) {}
