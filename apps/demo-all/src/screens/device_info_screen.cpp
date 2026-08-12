#include "device_info_screen.h"

using namespace m5ui;

namespace demo {

namespace {

constexpr uint32_t kLiveRefreshMs = 5000;

const char* ChargeStateName(ChargeState s) {
    switch (s) {
        case ChargeState::Charging:    return "charging";
        case ChargeState::Full:        return "full";
        case ChargeState::Discharging: return "on battery";
        case ChargeState::UsbOnly:     return "usb, no cell";
        default:                       return "unknown";
    }
}

Heading* SectionHeading(const String& text) {
    Heading* h = new Heading(text, 3);
    h->SetMargin(EdgeInsets::Only(tok::kSpaceSm, 0, 0, 0));
    return h;
}

}  // namespace

DeviceInfoScreen::DeviceInfoScreen() : DemoScreen("device-info", "Device") {}

void DeviceInfoScreen::BuildActions(AppBar& bar) {
    bar.AddAction("Refresh", [this]() {
        Device::Get().RefreshSpecs();
        _last_live_refresh_ms = 0;
        RefreshLiveValues();
        // A spec refresh is a good moment to clear accumulated residue.
        Device::Get().Panel().RefreshFull();
    });
}

void DeviceInfoScreen::BuildContent(Column& column) {
    BuildBatteryCard(column);
    BuildMemoryCard(column);
    BuildSpecCards(column);
    RefreshLiveValues();
}

// -------------------------------------------------------------- battery ----

void DeviceInfoScreen::BuildBatteryCard(Column& column) {
    Card* card = new Card();
    column.Add(card);

    card->Add(new Heading("Battery", 3));

    _battery_meter = new Meter("Charge");
    _battery_meter->SetWarnBelow(0.15f); // hatches below 15%
    card->Add(_battery_meter);

    _battery_detail = new Label("", tok::kTextSm);
    _battery_detail->SetColor(tok::kFgMuted);
    card->Add(_battery_detail);
}

// --------------------------------------------------------------- memory ----

void DeviceInfoScreen::BuildMemoryCard(Column& column) {
    Card* card = new Card();
    column.Add(card);

    card->Add(new Heading("Memory", 3));

    // Two pools, two meters, never summed -- an "8 MB free" reading says
    // nothing about whether BLE can start (issue #77).
    _dram_meter = new Meter("Internal DRAM free");
    _dram_meter->SetWarnBelow(0.20f);
    card->Add(_dram_meter);

    _dram_detail = new Label("", tok::kTextXs);
    _dram_detail->SetColor(tok::kFgMuted);
    card->Add(_dram_detail);

    _psram_meter = new Meter("PSRAM free");
    card->Add(_psram_meter);
}

// ---------------------------------------------------------------- specs ----

void DeviceInfoScreen::BuildSpecCards(Column& column) {
    const DeviceSpecs& s = Device::Get().Specs();

    // ---- SoC -------------------------------------------------------------
    Card* soc = new Card();
    column.Add(soc);
    soc->Add(SectionHeading("SoC"));
    soc->Add(InfoRow("Model", s.soc.model));
    soc->Add(InfoRow("Revision", "rev " + String(s.soc.revision)));
    soc->Add(InfoRow("Cores", String(s.soc.cores)));
    soc->Add(InfoRow("CPU", String(s.soc.cpu_freq_mhz) + " MHz"));
    soc->Add(InfoRow("Crystal", String(s.soc.xtal_mhz) + " MHz"));
    soc->Add(new Divider());
    soc->Add(InfoRow("MAC", s.soc.mac));

    String radios;
    if (s.soc.has_wifi) radios += "WiFi ";
    if (s.soc.has_ble) radios += "BLE ";
    if (s.soc.has_bt_classic) radios += "BT";
    soc->Add(InfoRow("Radios", radios.length() ? radios : String("none")));

    // ---- flash and storage ----------------------------------------------
    Card* flash = new Card();
    column.Add(flash);
    flash->Add(SectionHeading("Flash & storage"));
    flash->Add(InfoRow("Flash", FormatBytes(s.memory.flash_bytes) + " @ " +
                                    String(s.memory.flash_speed_hz / 1000000) +
                                    " MHz"));
    flash->Add(InfoRow("Sketch", FormatBytes(s.memory.sketch_bytes)));
    flash->Add(InfoRow("OTA free", FormatBytes(s.memory.sketch_free_bytes)));
    flash->Add(new Divider());

    Storage& files = Device::Get().Files();
    const char* backend = files.Backend() == StorageBackend::SdCard ? "SD card"
                          : files.Backend() == StorageBackend::Spiffs ? "SPIFFS"
                                                                      : "none";
    flash->Add(InfoRow("Backend", backend));
    flash->Add(InfoRow("Capacity", FormatBytes(files.TotalBytes())));
    flash->Add(InfoRow("Free", FormatBytes(files.FreeBytes())));

    // ---- display ---------------------------------------------------------
    Card* display = new Card();
    column.Add(display);
    display->Add(SectionHeading("Display"));
    display->Add(InfoRow("Resolution", String(s.display.width) + " x " +
                                           String(s.display.height)));
    display->Add(InfoRow("Greyscale", String(s.display.grey_levels) +
                                          " levels (" +
                                          String(s.display.grey_bits) + " bpp)"));
    display->Add(InfoRow("Panel", String(s.display.diagonal_inches, 1) + "\", " +
                                      String(s.display.ppi) + " ppi"));
    display->Add(InfoRow("Framebuffer", FormatBytes(s.display.framebuffer_bytes)));
    display->Add(InfoRow("Touch", s.display.touch_present
                                      ? s.display.touch_controller
                                      : String("absent")));

    // ---- firmware --------------------------------------------------------
    Card* fw = new Card();
    column.Add(fw);
    fw->Add(SectionHeading("Firmware"));
    fw->Add(InfoRow("Version", s.firmware_version));
    fw->Add(InfoRow("Built", s.build_date));
    fw->Add(InfoRow("UI library", M5PAPER_UI_VERSION));

    _uptime = new Label("", tok::kTextSm);
    _uptime->SetColor(tok::kFgMuted);
    fw->Add(_uptime);
}

// ----------------------------------------------------------------- live ----

void DeviceInfoScreen::RefreshLiveValues() {
    Device& dev = Device::Get();

    // ---- battery ---------------------------------------------------------
    if (_battery_meter != nullptr) {
        const BatteryState b = dev.GetBattery();
        _battery_meter->SetValue(b.percent / 100.0f);
        _battery_meter->SetValueText(String(b.percent) + "%");

        String detail = String(b.millivolts) + " mV, " +
                        String(ChargeStateName(b.charge));
        if (b.is_critical) {
            detail += " -- critical";
        } else if (b.is_low) {
            detail += " -- low";
        }
        _battery_detail->SetText(detail);
    }

    // ---- memory ----------------------------------------------------------
    const MemorySnapshot mem = dev.Memory().Snapshot();

    if (_dram_meter != nullptr && mem.internal.total_bytes > 0) {
        _dram_meter->SetValue((float)mem.internal.free_bytes /
                              (float)mem.internal.total_bytes);
        _dram_meter->SetValueText(FormatBytes(mem.internal.free_bytes));
    }
    if (_dram_detail != nullptr) {
        // Largest free block and the low-water mark are the two figures a bare
        // "free" number hides.
        _dram_detail->SetText(
            "largest block " + FormatBytes(mem.internal.largest_free_block) +
            "  |  low-water " + FormatBytes(mem.internal.min_free_ever) +
            "  |  frag " + String(mem.internal.FragmentationPercent()) + "%");
    }
    if (_psram_meter != nullptr && mem.psram.total_bytes > 0) {
        _psram_meter->SetValue((float)mem.psram.free_bytes /
                               (float)mem.psram.total_bytes);
        _psram_meter->SetValueText(FormatBytes(mem.psram.free_bytes));
    }

    // ---- uptime ----------------------------------------------------------
    if (_uptime != nullptr) {
        const uint32_t s = millis() / 1000;
        char buf[48];
        snprintf(buf, sizeof(buf), "Uptime %luh %02lum %02lus, %lu frames",
                 (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60),
                 (unsigned long)(s % 60), (unsigned long)dev.LoopCount());
        _uptime->SetText(String(buf));
    }
}

void DeviceInfoScreen::Tick() {
    DemoScreen::Tick();

    const uint32_t now = millis();
    if (now - _last_live_refresh_ms < kLiveRefreshMs) return;
    _last_live_refresh_ms = now;
    RefreshLiveValues();
}

}  // namespace demo
