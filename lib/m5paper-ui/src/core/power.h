#pragma once
// Battery and power state (issue #92, replaces the inline curve noted in #50).
//
// The voltage-to-percent mapping lives in one pure function so it can be unit
// tested off-device (issue #17) instead of being buried in a status bar draw
// call.

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

struct BatteryState {
    uint32_t millivolts = 0;   // raw, as sampled
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

// Owns ADC sampling and its cadence. Reads are cheap because they are served
// from a smoothed cache; the ADC itself is only hit every kSamplePeriodMs.
class PowerManager {
   public:
    static constexpr uint32_t kSamplePeriodMs = 5000;
    static constexpr uint8_t kSmoothingWindow = 8;

    // Enables the battery ADC. Safe to call once, from Device::Begin().
    void Begin();

    // THE call most app code wants. Returns the cached state, resampling first
    // if the cache is stale.
    BatteryState GetBattery();

    // Forces an immediate ADC read, bypassing the cache. Use sparingly -- it
    // blocks for the sample and defeats the smoothing window.
    BatteryState SampleNow();

    // Raw millivolts, already divider-corrected.
    uint32_t ReadMillivolts();

    bool IsUsbPresent();

    // Cuts main power. Does not return.
    [[noreturn]] void Shutdown();

    // Deep sleep, waking on the given delay (0 = wake on touch/button only).
    void Sleep(uint32_t wake_after_seconds = 0);

    // Reduces CPU frequency when idle. Returns the frequency actually applied.
    uint32_t SetCpuFrequencyMhz(uint32_t mhz);

   private:
    void PushSample(uint32_t mv);

    bool _begun = false;
    uint32_t _samples[kSmoothingWindow] = {0};
    uint8_t _sample_count = 0;
    uint8_t _sample_head = 0;
    BatteryState _cached;
};

}  // namespace m5ui
