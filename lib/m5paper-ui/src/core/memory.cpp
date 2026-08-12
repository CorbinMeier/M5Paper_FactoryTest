#include "memory.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

namespace m5ui {

namespace {

constexpr uint32_t kInternalCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
constexpr uint32_t kSamplePeriodMs = 1000;

HeapStats ReadPool(uint32_t caps) {
    HeapStats s;
    s.free_bytes = (uint32_t)heap_caps_get_free_size(caps);
    s.largest_free_block = (uint32_t)heap_caps_get_largest_free_block(caps);
    s.min_free_ever = (uint32_t)heap_caps_get_minimum_free_size(caps);
    s.total_bytes = (uint32_t)heap_caps_get_total_size(caps);
    return s;
}

}  // namespace

uint32_t EstimatedDramCost(Subsystem s) {
    // Starting estimates. #79 replaces these with measured figures.
    switch (s) {
        case Subsystem::WiFi:       return 62 * 1024;
        case Subsystem::BleHid:     return 78 * 1024;
        case Subsystem::Tls:        return 44 * 1024; // handshake peak
        case Subsystem::HttpClient: return 12 * 1024;
        case Subsystem::SdCard:     return 8 * 1024;
    }
    return 0;
}

MemorySnapshot MemoryMonitor::Snapshot() const {
    MemorySnapshot snap;
    snap.internal = ReadPool(kInternalCaps);
    snap.psram = ReadPool(MALLOC_CAP_SPIRAM);
    snap.taken_at_ms = millis();
    return snap;
}

uint32_t MemoryMonitor::FreeInternal() const {
    return (uint32_t)heap_caps_get_free_size(kInternalCaps);
}

uint32_t MemoryMonitor::LargestFreeInternalBlock() const {
    return (uint32_t)heap_caps_get_largest_free_block(kInternalCaps);
}

uint32_t MemoryMonitor::FreePsram() const {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

bool MemoryMonitor::CanAfford(Subsystem s, uint32_t headroom_bytes) const {
    const uint32_t need = EstimatedDramCost(s) + headroom_bytes;
    // Both must hold: enough total, and enough in one contiguous block.
    return FreeInternal() >= need && LargestFreeInternalBlock() >= need / 2;
}

void MemoryMonitor::Tick() {
    const uint32_t now = millis();
    if (_last_tick_ms != 0 && (now - _last_tick_ms) < kSamplePeriodMs) return;
    _last_tick_ms = now;

    _history[_head] = FreeInternal() / 1024;
    _head = (uint8_t)((_head + 1) % kHistoryLen);
    if (_count < kHistoryLen) _count++;
}

uint32_t MemoryMonitor::HistoryAt(uint8_t index) const {
    if (index >= _count) return 0;
    const uint8_t oldest = (uint8_t)((_head + kHistoryLen - _count) % kHistoryLen);
    return _history[(uint8_t)((oldest + index) % kHistoryLen)];
}

void MemoryMonitor::DumpTo(Print& out) const {
    const MemorySnapshot s = Snapshot();
    out.println(F("-- memory ------------------------------------"));
    out.printf("internal free      : %7u B\n", s.internal.free_bytes);
    out.printf("internal largest   : %7u B\n", s.internal.largest_free_block);
    out.printf("internal low-water : %7u B\n", s.internal.min_free_ever);
    out.printf("internal frag      : %7u %%\n", s.internal.FragmentationPercent());
    out.printf("psram free         : %7u B\n", s.psram.free_bytes);
    out.printf("psram largest      : %7u B\n", s.psram.largest_free_block);
}

}  // namespace m5ui
