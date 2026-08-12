#pragma once
// Runtime device specification (issue #92).
//
// Everything here is *queried*, not hardcoded, so the answer stays true across
// board revisions and flash configurations. Populated once at Device::Begin()
// and then read freely -- none of it changes after boot.

#include <Arduino.h>
#include <stdint.h>

#include "geometry.h"

namespace m5ui {

struct SocSpecs {
    String model;        // "ESP32-D0WDQ6-V3"
    uint8_t revision = 0;
    uint8_t cores = 0;
    uint32_t cpu_freq_mhz = 0;
    uint32_t xtal_mhz = 0;
    bool has_wifi = false;
    bool has_bt_classic = false;
    bool has_ble = false;
    bool has_embedded_flash = false;
    uint64_t chip_id = 0; // derived from the factory MAC
    String mac;           // "AA:BB:CC:DD:EE:FF"
};

struct MemorySpecs {
    uint32_t flash_bytes = 0;
    uint32_t flash_speed_hz = 0;
    uint32_t sketch_bytes = 0;      // this firmware image
    uint32_t sketch_free_bytes = 0; // remaining OTA room
    uint32_t psram_bytes = 0;
    uint32_t internal_dram_bytes = 0;
    bool psram_present = false;
};

struct DisplaySpecs {
    int16_t width = kDisplayW;
    int16_t height = kDisplayH;
    uint8_t grey_bits = kGreyBits;
    uint8_t grey_levels = kGreyLevels;
    uint32_t framebuffer_bytes = kFramebufferBytes;
    float diagonal_inches = 4.7f;
    uint16_t ppi = 235;
    bool touch_present = false;
    String touch_controller; // "GT911"
};

struct StorageSpecs {
    bool sd_present = false;
    uint64_t sd_bytes = 0;
    uint8_t sd_type = 0; // sdcard_type_t
    uint32_t spiffs_bytes = 0;
    uint32_t spiffs_used_bytes = 0;
};

struct DeviceSpecs {
    String product = "M5Paper";
    String firmware_version = "0.1.0";
    String build_date; // __DATE__ " " __TIME__ captured at compile time
    SocSpecs soc;
    MemorySpecs memory;
    DisplaySpecs display;
    StorageSpecs storage;

    // Fills every field from the running system. Idempotent.
    void Probe();

    // Human-readable spec sheet -- what the demo's device-info screen and the
    // serial banner both render.
    String ToString() const;
};

// Formats a byte count as "8.0 MB" / "312 KB". Shared by the info screens.
String FormatBytes(uint64_t bytes);

}  // namespace m5ui
