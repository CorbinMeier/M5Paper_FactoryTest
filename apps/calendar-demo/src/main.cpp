// calendar-demo -- draws the current month and clock, then deep-sleeps.
//
// The current time comes from the M5Paper's BM8563 RTC (m5ui::Device::Time(),
// lib/m5paper-ui/src/core/device.h): it runs on its own coin-cell backup, so
// it keeps correct time across deep sleep and power loss without this app
// doing anything -- that's the "stored somehow" (issue #110). Set it once via
// Clock::SyncFromNtp() (needs WiFi) or M5Burner before relying on it here.
//
// This app does not call Device::Run() -- that loop is for apps that stay
// awake handling input. Here there is nothing to wait for: paint once, let
// the panel finish, then power down until the next wake.

#include <m5paper_ui.h>

#include "calendar_app.h"

namespace {

// How long to sleep before waking to redraw the clock. A full e-ink refresh
// is visible on every wake, so this trades clock accuracy against how often
// the page flashes -- one minute is a compromise, not a measurement.
constexpr uint32_t kWakeAfterSeconds = 60;

m5ui::DeviceConfig MakeConfig() {
    m5ui::DeviceConfig config;
    config.print_spec_banner = true;
    config.auto_power_save = false; // this app manages its own sleep cycle
    config.storage_root = "/m5paper-calendar";
    return config;
}

}  // namespace

void setup() {
    m5ui::Device& device = m5ui::Device::Get();
    device.Begin(MakeConfig());

    static calendar::CalendarApp app;
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
