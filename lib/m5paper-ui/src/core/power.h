#pragma once
// Battery and power state (issue #92, replaces the inline curve noted in #50).
//
// The voltage-to-percent mapping and the burst median live in pure functions
// so they can be unit tested off-device (issue #17) instead of being buried in
// a status bar draw call.

#include <stdint.h>

#include "battery_curve.h"

namespace m5ui {

enum class ChargeState : uint8_t {
    Unknown = 0,
    Discharging, // running from the cell
    Charging,    // USB present, cell below full
    Full,        // USB present, cell topped off
    UsbOnly      // USB present, no cell detected
};

// An immutable snapshot. Every field comes from one measurement, so a caller
// rendering "74%, 3810 mV, charging" cannot show three fields from three
// different instants.
struct BatteryState {
    uint32_t millivolts = 0;   // median of the burst
    uint8_t percent = 0;       // 0-100, from the discharge curve
    ChargeState charge = ChargeState::Unknown;
    bool usb_present = false;
    bool is_low = false;      // <= kLowPercent, warn the user
    bool is_critical = false; // <= kCriticalPercent, refuse heavy subsystems
    uint32_t sampled_at_ms = 0;

    bool IsCharging() const {
        return charge == ChargeState::Charging;
    }
};

// Pull-only. Nothing here samples on a timer, keeps a cache, or runs in the
// background -- the ADC is touched if and only if a caller asks. A clock that
// wants a percentage every second gets one measurement per second; a screen
// that never asks costs nothing.
//
// Each call is self-contained: it takes a burst of kBurstSamples conversions
// back to back and takes their median. That is what makes dropping the cache
// safe -- a single raw conversion carries enough noise to swing the reading by
// several percent, and previously a rolling window across calls hid it. Now
// the rejection happens inside the call, so the answer does not depend on how
// often you ask.
//
// Cost of one GetBattery(): 8 conversions, on the order of a millisecond.
// Calling it per frame would be wasteful but not harmful; per second is free.
class PowerManager {
   public:
    // Conversions per measurement. Odd counts would avoid the averaging step,
    // but 8 keeps the burst short and the median stable.
    static constexpr uint8_t kBurstSamples = 8;

    // Enables the battery ADC and throws away one conversion -- the first read
    // after the ADC is powered up is unreliable, and discarding it here means
    // no caller ever has to know that. Keeps no state beyond the enable.
    void Begin();

    // THE call. One measurement, all fields coherent.
    BatteryState GetBattery();

    // Single-field pulls, for a caller that wants exactly one number. Each is
    // a full measurement -- if you need two or more fields, call GetBattery()
    // once instead, both to halve the cost and to keep the fields consistent.
    uint8_t Percent();
    uint32_t Millivolts();
    bool IsCharging();
    bool IsLow();
    bool IsUsbPresent();

    // One unfiltered conversion, no burst, no median. For calibration work
    // (issue #79); app code wants GetBattery().
    uint32_t ReadMillivoltsRaw();

    // Cuts main power. Does not return.
    [[noreturn]] void Shutdown();

    // Deep sleep, waking on the given delay (0 = wake on touch/button only).
    void Sleep(uint32_t wake_after_seconds = 0);

    // Reduces CPU frequency when idle. Returns the frequency actually applied.
    uint32_t SetCpuFrequencyMhz(uint32_t mhz);

   private:
    // Burst plus median. The only place the ADC is read in bulk.
    uint32_t MeasureMillivolts();

    bool _begun = false;
};

}  // namespace m5ui
