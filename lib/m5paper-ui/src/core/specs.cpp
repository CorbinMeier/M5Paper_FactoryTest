#include "specs.h"

#include <SD.h>
#include <SPIFFS.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

namespace m5ui {

namespace {

String ChipModelName(const esp_chip_info_t& info) {
    switch (info.model) {
        case CHIP_ESP32:   return "ESP32-D0WDQ6";
        case CHIP_ESP32S2: return "ESP32-S2";
        case CHIP_ESP32S3: return "ESP32-S3";
        case CHIP_ESP32C3: return "ESP32-C3";
        default:           return "ESP32 (unknown)";
    }
}

}  // namespace

String FormatBytes(uint64_t bytes) {
    char buf[24];
    if (bytes >= 1024ULL * 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024 * 1024));
    } else if (bytes >= 1024ULL * 1024) {
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024));
    } else if (bytes >= 1024ULL) {
        snprintf(buf, sizeof(buf), "%u KB", (unsigned)(bytes / 1024));
    } else {
        snprintf(buf, sizeof(buf), "%u B", (unsigned)bytes);
    }
    return String(buf);
}

void DeviceSpecs::Probe() {
    build_date = String(__DATE__) + " " + String(__TIME__);

    // ------------------------------------------------------------- soc ----
    esp_chip_info_t info;
    esp_chip_info(&info);

    soc.model = ChipModelName(info);
    soc.revision = (uint8_t)info.revision;
    soc.cores = (uint8_t)info.cores;
    soc.cpu_freq_mhz = getCpuFrequencyMhz();
    soc.xtal_mhz = getXtalFrequencyMhz();
    soc.has_wifi = (info.features & CHIP_FEATURE_WIFI_BGN) != 0;
    soc.has_bt_classic = (info.features & CHIP_FEATURE_BT) != 0;
    soc.has_ble = (info.features & CHIP_FEATURE_BLE) != 0;
    soc.has_embedded_flash = (info.features & CHIP_FEATURE_EMB_FLASH) != 0;

    // ESP.getEfuseMac() rather than esp_efuse_mac_get_default(): the Arduino
    // wrapper is stable across IDF versions, where the header the IDF call
    // lives in moved between 4.4 and 5.x.
    const uint64_t efuse = ESP.getEfuseMac();
    uint8_t mac[6];
    for (int i = 0; i < 6; ++i) mac[i] = (uint8_t)((efuse >> (8 * i)) & 0xFF);

    char macbuf[18];
    snprintf(macbuf, sizeof(macbuf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],
             mac[1], mac[2], mac[3], mac[4], mac[5]);
    soc.mac = String(macbuf);

    soc.chip_id = 0;
    for (int i = 0; i < 6; ++i) {
        soc.chip_id = (soc.chip_id << 8) | mac[i];
    }

    // ---------------------------------------------------------- memory ----
    memory.flash_bytes = ESP.getFlashChipSize();
    memory.flash_speed_hz = ESP.getFlashChipSpeed();
    memory.sketch_bytes = ESP.getSketchSize();
    memory.sketch_free_bytes = ESP.getFreeSketchSpace();
    memory.psram_bytes = ESP.getPsramSize();
    memory.psram_present = memory.psram_bytes > 0;
    memory.internal_dram_bytes = (uint32_t)heap_caps_get_total_size(
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    // --------------------------------------------------------- display ----
    display.width = kDisplayW;
    display.height = kDisplayH;
    display.grey_bits = kGreyBits;
    display.grey_levels = kGreyLevels;
    display.framebuffer_bytes = kFramebufferBytes;
    display.touch_controller = "GT911";
    // Set by Device::Begin() once the touch driver reports in -- probing the
    // I2C bus from here would duplicate that init.

    // --------------------------------------------------------- storage ----
    const uint8_t card = SD.cardType();
    storage.sd_type = card;
    storage.sd_present = card != CARD_NONE;
    storage.sd_bytes = storage.sd_present ? SD.cardSize() : 0;
    storage.spiffs_bytes = (uint32_t)SPIFFS.totalBytes();
    storage.spiffs_used_bytes = (uint32_t)SPIFFS.usedBytes();
}

String DeviceSpecs::ToString() const {
    String s;
    s.reserve(768);
    s += product + " firmware " + firmware_version + "\n";
    s += "built      " + build_date + "\n";
    s += "soc        " + soc.model + " rev" + String(soc.revision) + ", " +
         String(soc.cores) + " cores @ " + String(soc.cpu_freq_mhz) + " MHz\n";
    s += "mac        " + soc.mac + "\n";
    s += "radios     ";
    s += soc.has_wifi ? "WiFi " : "";
    s += soc.has_ble ? "BLE " : "";
    s += soc.has_bt_classic ? "BT-Classic" : "";
    s += "\n";
    s += "flash      " + FormatBytes(memory.flash_bytes) + " @ " +
         String(memory.flash_speed_hz / 1000000) + " MHz\n";
    s += "psram      " + FormatBytes(memory.psram_bytes) + "\n";
    s += "dram       " + FormatBytes(memory.internal_dram_bytes) + "\n";
    s += "sketch     " + FormatBytes(memory.sketch_bytes) + " used, " +
         FormatBytes(memory.sketch_free_bytes) + " free\n";
    s += "display    " + String(display.width) + "x" + String(display.height) +
         " @ " + String(display.grey_levels) + " greys, " +
         String(display.ppi) + " ppi\n";
    s += "touch      " + display.touch_controller +
         (display.touch_present ? " (ok)" : " (absent)") + "\n";
    s += "sd         ";
    s += storage.sd_present ? FormatBytes(storage.sd_bytes) : String("none");
    s += "\n";
    s += "spiffs     " + FormatBytes(storage.spiffs_used_bytes) + " / " +
         FormatBytes(storage.spiffs_bytes) + "\n";
    return s;
}

}  // namespace m5ui
