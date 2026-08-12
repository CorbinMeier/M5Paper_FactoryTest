#include "power.h"

#include <Arduino.h>
#include <M5EPD.h>
#include <esp_sleep.h>

namespace m5ui {

void PowerManager::Begin() {
    if (_begun) return;
    M5.BatteryADCBegin();
    _begun = true;
    SampleNow();
}

uint32_t PowerManager::ReadMillivolts() {
    // M5EPD already applies the 1:2 divider correction and returns millivolts.
    return M5.getBatteryVoltage();
}

bool PowerManager::IsUsbPresent() {
    // No dedicated VBUS sense line is exposed, so USB is inferred: the charger
    // holds the rail above what the cell alone can supply.
    return ReadMillivolts() >= kUsbPresentMv;
}

void PowerManager::PushSample(uint32_t mv) {
    _samples[_sample_head] = mv;
    _sample_head = (uint8_t)((_sample_head + 1) % kSmoothingWindow);
    if (_sample_count < kSmoothingWindow) _sample_count++;
}

BatteryState PowerManager::SampleNow() {
    const uint32_t raw = ReadMillivolts();
    PushSample(raw);

    uint32_t sum = 0;
    for (uint8_t i = 0; i < _sample_count; ++i) sum += _samples[i];
    const uint32_t smoothed = _sample_count ? sum / _sample_count : raw;

    BatteryState s;
    s.millivolts = smoothed;
    s.percent = BatteryPercentFromMillivolts(smoothed);
    s.usb_present = smoothed >= kUsbPresentMv;
    s.sampled_at_ms = millis();

    if (s.usb_present) {
        s.charge = s.percent >= 99 ? ChargeState::Full : ChargeState::Charging;
    } else if (smoothed < kBatteryEmptyMv / 2) {
        // Implausibly low for a live cell -- almost certainly no battery fitted.
        s.charge = ChargeState::UsbOnly;
    } else {
        s.charge = ChargeState::Discharging;
    }

    s.is_low = !s.usb_present && s.percent <= kLowPercent;
    s.is_critical = !s.usb_present && s.percent <= kCriticalPercent;

    _cached = s;
    return s;
}

void PowerManager::SetCacheRefresh(uint32_t milliseconds) {
    _cache_refresh_ms = milliseconds;
}

BatteryState PowerManager::GetBattery() {
    // Begin() takes the boot reading, so the first caller after it gets a real
    // value rather than a zeroed struct. Calling it here as well covers an app
    // that reaches for the battery before Device::Begin().
    if (!_begun) Begin();

    if (_cache_refresh_ms == 0) return SampleNow();

    const uint32_t now = millis();
    const bool stale = (now - _cached.sampled_at_ms) >= _cache_refresh_ms;
    if (stale || _cached.sampled_at_ms == 0) return SampleNow();
    return _cached;
}

void PowerManager::Shutdown() {
    M5.disableMainPower();
    // The RTC cuts the rail a few milliseconds later; park until it does so
    // nothing downstream observes a half-powered device.
    M5.shutdown();
    for (;;) {
        delay(1000);
    }
}

void PowerManager::Sleep(uint32_t wake_after_seconds) {
    M5.disableEPDPower();
    M5.disableEXTPower();
    if (wake_after_seconds > 0) {
        M5.shutdown((int)wake_after_seconds);
    } else {
        esp_sleep_enable_ext0_wakeup((gpio_num_t)M5EPD_KEY_PUSH_PIN, LOW);
        esp_deep_sleep_start();
    }
}

uint32_t PowerManager::SetCpuFrequencyMhz(uint32_t mhz) {
    setCpuFrequencyMhz((uint32_t)mhz);
    return getCpuFrequencyMhz();
}

}  // namespace m5ui
