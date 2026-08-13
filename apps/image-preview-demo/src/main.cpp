// image-preview-demo -- full-screen JPEG gallery, G37 (left)/G39 (right)
// side buttons step backward/forward through the array (issue #113).
//
// Stays awake and interactive (Device::Run), unlike calendar-demo's
// paint-once-and-sleep pattern -- there is something to wait for here: the
// next button press.

#include <m5paper_ui.h>

#include "card_app.h"

namespace {

m5ui::DeviceConfig MakeConfig() {
    m5ui::DeviceConfig config;
    config.print_spec_banner = true;
    config.storage_root = "/m5paper-image-preview";
    return config;
}

}  // namespace

void setup() {
    m5ui::Device& device = m5ui::Device::Get();
    device.Begin(MakeConfig());

    static card::CardApp app;
    device.Run(app); // does not return
}

void loop() {
    // Unreachable -- Device::Run() never returns.
}
