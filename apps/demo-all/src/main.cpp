// demo-all -- the reference application for m5paper-ui.
//
// The entire boot sequence is three statements. Everything the old
// systeminit.cpp did by hand (power rails, panel rotation, touch rotation,
// battery ADC, SD mount, RTC) is inside Device::Begin().

#include <m5paper_ui.h>

#include "demo_app.h"

namespace {

m5ui::DeviceConfig MakeConfig() {
    m5ui::DeviceConfig config;
    config.print_spec_banner = true; // dump the spec sheet to serial at boot
    config.auto_power_save = true;
    config.idle_prompt_ms = 5 * 60 * 1000;
    config.idle_shutdown_ms = 6 * 60 * 1000;
    config.storage_root = "/m5paper-demo";
    return config;
}

}  // namespace

void setup() {
    m5ui::Device& device = m5ui::Device::Get();
    device.Begin(MakeConfig());

    // Static, not heap: the app outlives setup() and is never destroyed.
    static demo::DemoApp app;

    // Does not return -- Run() owns the loop from here.
    device.Run(app);
}

void loop() {
    // Unreachable. Device::Run() never returns; Arduino requires the symbol.
}
