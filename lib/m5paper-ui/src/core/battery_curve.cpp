// The voltage-to-percent mapping, deliberately in its own translation unit
// with no Arduino or M5EPD dependency, so it can be compiled and tested on the
// host (issue #17). The rest of PowerManager needs hardware; this does not.

#include <stddef.h>
#include <stdint.h>

#include "battery_curve.h"

namespace m5ui {

namespace {

// Breakpoints along a single-cell LiPo discharge curve, low to high. The knees
// matter more than the endpoints: a linear 3300-4350 map reports ~50% for most
// of the usable life and then falls off a cliff.
struct CurvePoint {
    uint32_t mv;
    uint8_t pct;
};

constexpr CurvePoint kCurve[] = {
    {3300, 0},  {3500, 5},  {3600, 10}, {3700, 25},
    {3800, 45}, {3900, 62}, {4000, 78}, {4100, 90},
    {4200, 97}, {4350, 100},
};
constexpr size_t kCurveLen = sizeof(kCurve) / sizeof(kCurve[0]);

}  // namespace

uint8_t BatteryPercentFromMillivolts(uint32_t mv) {
    if (mv <= kCurve[0].mv) return 0;
    if (mv >= kCurve[kCurveLen - 1].mv) return 100;

    for (size_t i = 1; i < kCurveLen; ++i) {
        if (mv > kCurve[i].mv) continue;

        const CurvePoint& lo = kCurve[i - 1];
        const CurvePoint& hi = kCurve[i];
        const uint32_t span_mv = hi.mv - lo.mv;
        const uint32_t span_pct = hi.pct - lo.pct;
        // Round to nearest rather than truncating, so the curve hits its
        // breakpoints exactly.
        return (uint8_t)(lo.pct +
                         ((mv - lo.mv) * span_pct + span_mv / 2) / span_mv);
    }
    return 100;
}

}  // namespace m5ui
