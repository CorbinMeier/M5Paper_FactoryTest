// calendar-demo -- draws the current month, clock and weather, then
// deep-sleeps.
//
// Two time/data sources, not one:
//   - The M5Paper's BM8563 RTC (m5ui::Device::Time(), core/device.h) runs on
//     its own coin-cell backup, so it keeps correct time across deep sleep
//     and power loss on its own -- that's the baseline "stored somehow"
//     (issue #110).
//   - A short BLE advertising window on every wake (issue #112) lets a phone
//     app push a time correction and hourly weather, which the RTC cannot
//     provide by itself. This is opportunistic: if no phone is in range, the
//     window just times out and the screen still paints from the RTC alone.
//
// This app does not call Device::Run() -- that loop is for apps that stay
// awake handling input. Here there is nothing to wait for once the BLE
// window closes: paint once, let the panel finish, then power down until the
// next wake.

#include <m5paper_ui.h>

#include "calendar_app.h"

namespace {

// How long to sleep before waking to redraw the clock. A full e-ink refresh
// is visible on every wake, so this trades clock accuracy against how often
// the page flashes -- one minute is a compromise, not a measurement.
constexpr uint32_t kWakeAfterSeconds = 60;

// How long to advertise for a phone to connect and push data. Every wake
// pays this whether or not a phone shows up, so it is a battery-life knob,
// not just a UX one -- exits early once both a time and weather write have
// landed, so a phone already in range does not cost the full window.
constexpr uint32_t kBleWindowMs = 8000;

m5ui::DeviceConfig MakeConfig() {
    m5ui::DeviceConfig config;
    config.print_spec_banner = true;
    config.auto_power_save = false; // this app manages its own sleep cycle
    config.storage_root = "/m5paper-calendar";
    return config;
}

// Advertises for up to kBleWindowMs, applying any time sync straight to the
// RTC and returning the last weather snapshot received (default-constructed,
// HasData() == false, if nothing arrived).
m5ui::WeatherSnapshot RunBleWindow(m5ui::Device& device) {
    m5ui::BleCompanion ble;
    ble.Begin("M5Paper-Calendar");

    m5ui::WeatherSnapshot weather;
    bool time_synced = false;

    const uint32_t deadline = millis() + kBleWindowMs;
    while (millis() < deadline) {
        uint32_t epoch;
        int16_t tz_minutes;
        if (ble.TakeTimeSync(epoch, tz_minutes)) {
            device.Time().SetEpochSeconds(epoch, tz_minutes);
            time_synced = true;
        }

        m5ui::WeatherSnapshot received;
        if (ble.TakeWeatherUpdate(received)) {
            weather = received;
        }

        if (time_synced && weather.HasData()) break;
        delay(50);
    }

    ble.Stop();
    return weather;
}

}  // namespace

void setup() {
    m5ui::Device& device = m5ui::Device::Get();
    device.Begin(MakeConfig());

    const m5ui::WeatherSnapshot weather = RunBleWindow(device);

    static calendar::CalendarApp app;
    app.SetWeather(weather);
    device.SetApp(&app); // builds and enters the home screen

    // One paint is enough: the screen is fully dirty on first build, so the
    // first Step() both lays it out and flushes it to the panel. A second
    // call would find nothing dirty and return immediately.
    device.Step();
    device.Step();

    device.Sleep(kWakeAfterSeconds);
}

void loop() {
    // Unreachable -- Sleep() does not return.
}
