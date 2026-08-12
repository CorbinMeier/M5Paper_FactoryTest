#pragma once
// Split out of power.h so the mapping can be compiled on the host with no
// Arduino or M5EPD dependency (issue #17).

#include <stdint.h>

namespace m5ui {

// The M5Paper carries a single-cell LiPo behind a 1:2 divider on ADC1_CH7.
constexpr uint32_t kBatteryEmptyMv = 3300;
constexpr uint32_t kBatteryFullMv = 4350;
constexpr uint32_t kUsbPresentMv = 4400; // above the cell's own ceiling
constexpr uint8_t kLowPercent = 15;
constexpr uint8_t kCriticalPercent = 5;

// Pure. Piecewise-linear approximation of a LiPo discharge curve -- the flat
// middle is deliberately stretched so the reading does not sit at "70%" for
// hours and then collapse. Clamps outside [empty, full].
uint8_t BatteryPercentFromMillivolts(uint32_t mv);

}  // namespace m5ui
