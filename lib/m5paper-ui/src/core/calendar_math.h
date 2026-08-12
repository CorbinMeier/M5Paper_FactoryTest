#pragma once
// Pure Gregorian calendar math, split out so it compiles and tests on the
// host with no Arduino or M5EPD dependency -- same pattern as
// battery_curve.h and geometry.h (issue #17).

#include <stdint.h>

namespace m5ui {

// True for years divisible by 4, except centuries not divisible by 400.
bool IsLeapYear(int16_t year);

// 28-31. `month` is 1-12.
uint8_t DaysInMonth(int16_t year, uint8_t month);

// 0 (Sunday) - 6 (Saturday). `month` is 1-12, `day` is 1-31. Sakamoto's
// algorithm, valid for the Gregorian calendar (year >= 1583).
uint8_t DayOfWeek(int16_t year, uint8_t month, uint8_t day);

}  // namespace m5ui
