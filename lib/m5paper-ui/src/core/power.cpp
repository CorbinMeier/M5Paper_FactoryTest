#include "power.h"

#include <Arduino.h>
#include <M5EPD.h>
#include <esp_sleep.h>

namespace m5ui {

void PowerManager::Begin() {
    if (_begun) return;
    M5.BatteryADCBegin();

    // The first conversion after the ADC is powered up is unreliable. Discard
    // it here so no caller has to know that -- this is the only reason Begin()
    // touches the ADC at all; nothing is retained.
    (void)ReadMillivoltsRaw();
    _begun = true;
}

uint32_t PowerManager::ReadMillivoltsRaw() {
    // M5EPD already applies the 1:2 divider correction and returns millivolts.
    return M5.getBatteryVoltage();
}

uint32_t PowerManager::MeasureMillivolts() {
    if (!_begun) Begin();

    uint32_t samples[kBurstSamples];
    for (uint8_t i = 0; i < kBurstSamples; ++i) {
        samples[i] = ReadMillivoltsRaw();
    }
    return MedianMillivolts(samples, kBurstSamples);
}

BatteryState PowerManager::GetBattery() {
    const uint32_t mv = MeasureMillivolts();

    BatteryState s;
    s.millivolts = mv;
    s.percent = BatteryPercentFromMillivolts(mv);
    s.usb_present = mv >= kUsbPresentMv;
    s.sampled_at_ms = millis();

    if (s.usb_present) {
        s.charge = s.percent >= 99 ? ChargeState::Full : ChargeState::Charging;
    } else if (mv < kBatteryEmptyMv / 2) {
        // Implausibly low for a live cell -- almost certainly no battery fitted.
        s.charge = ChargeState::UsbOnly;
    } else {
        s.charge = ChargeState::Discharging;
    }

    s.is_low = !s.usb_present && s.percent <= kLowPercent;
    s.is_critical = !s.usb_present && s.percent <= kCriticalPercent;
    return s;
}

// Single-field pulls. Each is a full measurement -- deliberately not sharing a
// hidden last-value, because that would be the cache this design removed.

uint8_t PowerManager::Percent() {
    return BatteryPercentFromMillivolts(MeasureMillivolts());
}

uint32_t PowerManager::Millivolts() {
    return MeasureMillivolts();
}

bool PowerManager::IsCharging() {
    return GetBattery().IsCharging();
}

bool PowerManager::IsLow() {
    return GetBattery().is_low;
}

bool PowerManager::IsUsbPresent() {
    // No dedicated VBUS sense line is exposed, so USB is inferred: the charger
    // holds the rail above what the cell alone can supply.
    return MeasureMillivolts() >= kUsbPresentMv;
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
