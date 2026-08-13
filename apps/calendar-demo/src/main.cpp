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
// Writing to that BLE service requires pairing first (issue #114) -- holding
// the Push button through boot enters an interactive pairing screen instead
// of the normal quick window, showing the numeric-comparison code and
// on-screen Confirm/Reject buttons. This is the one place the app stays
// awake handling touch input; everywhere else there is nothing to wait for,
// so it just paints once and sleeps.

#include <m5paper_ui.h>

#include "calendar_app.h"
#include "pairing_screen.h"

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

// Pairing needs a human to read a 6-digit code off this screen, glance at
// their phone, and tap two buttons -- 8 seconds is not enough for that.
constexpr uint32_t kPairingWindowMs = 90000;

m5ui::DeviceConfig MakeConfig() {
    m5ui::DeviceConfig config;
    config.print_spec_banner = true;
    config.auto_power_save = false; // this app manages its own sleep cycle
    config.storage_root = "/m5paper-calendar";
    return config;
}

// The Push button still held right after boot -- works whether this wake
// came from the periodic timer or the user power-cycling the device by hand,
// with no extra wiring beyond what PowerManager::Sleep() already sets up.
bool PairingRequested() {
    return M5.BtnP.isPressed();
}

// Advertises for up to kBleWindowMs, applying any time sync straight to the
// RTC and returning the last weather snapshot received (default-constructed,
// HasData() == false, if nothing arrived).
m5ui::WeatherSnapshot RunBleWindow(m5ui::Device& device, calendar::CalendarApp& app) {
    m5ui::BleCompanion& ble = app.Ble();
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

// Runs the interactive pairing screen: polls touch/buttons and steps the
// UI loop itself (Device::Run() is [[noreturn]], not usable here -- this
// needs to give up and fall through to the normal calendar paint+sleep
// either on success, rejection, or kPairingWindowMs elapsing). Also drains
// any time/weather writes that happen to land in the same window, same as
// RunBleWindow, since a phone that just paired may push data right away.
m5ui::WeatherSnapshot RunPairingWindow(m5ui::Device& device, calendar::CalendarApp& app) {
    m5ui::BleCompanion& ble = app.Ble();
    ble.Begin("M5Paper-Calendar");

    m5ui::WeatherSnapshot weather;

    const uint32_t deadline = millis() + kPairingWindowMs;
    while (millis() < deadline) {
        device.Touch().Poll(device.Input());
        device.Buttons().Poll(device.Input());
        device.Step();

        uint32_t epoch;
        int16_t tz_minutes;
        if (ble.TakeTimeSync(epoch, tz_minutes)) {
            device.Time().SetEpochSeconds(epoch, tz_minutes);
        }
        m5ui::WeatherSnapshot received;
        if (ble.TakeWeatherUpdate(received)) {
            weather = received;
        }

        auto* screen = static_cast<calendar::PairingScreen*>(app.Top());
        if (screen != nullptr && screen->IsDone()) {
            delay(2000); // leave "Paired!" / "Pairing failed." visible briefly
            device.Step();
            break;
        }
        delay(20);
    }

    ble.Stop();
    return weather;
}

}  // namespace

void setup() {
    m5ui::Device& device = m5ui::Device::Get();
    device.Begin(MakeConfig());

    static calendar::CalendarApp app;

    m5ui::WeatherSnapshot weather;
    if (PairingRequested()) {
        app.RequestPairingOnStart();
        device.SetApp(&app); // enters "pairing" -- OnStart() made it home
        weather = RunPairingWindow(device, app);
        app.SetWeather(weather); // before Replace(), so the rebuilt screen sees it
        app.Replace("calendar");
    } else {
        weather = RunBleWindow(device, app);
        app.SetWeather(weather); // before SetApp(), so Start()'s Push() sees it
        device.SetApp(&app);     // enters "calendar" -- the default home
    }

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
