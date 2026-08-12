#pragma once
// Memory introspection (issue #77).
//
// Internal DRAM and PSRAM are separate pools with separate consumers and must
// never be summed into one "free memory" figure. Internal DRAM (~290-320 KB
// usable) is what BLE, WiFi, TLS and task stacks contend for; PSRAM (8 MB)
// holds canvases and parse buffers and is effectively plentiful.
//
// LargestFreeBlock is the metric most likely to be omitted and most likely to
// matter: the internal heap fragments under repeated subsystem init/deinit, so
// total-free can read 80 KB while no single 20 KB allocation will succeed.

#include <stdint.h>

namespace m5ui {

struct HeapStats {
    uint32_t free_bytes = 0;
    uint32_t largest_free_block = 0;
    uint32_t min_free_ever = 0; // low-water mark -- catches slow leaks
    uint32_t total_bytes = 0;

    uint8_t UsedPercent() const {
        if (total_bytes == 0) return 0;
        return (uint8_t)(((total_bytes - free_bytes) * 100) / total_bytes);
    }

    // How badly the pool is fragmented, 0 (contiguous) to 100 (shattered).
    uint8_t FragmentationPercent() const {
        if (free_bytes == 0) return 0;
        return (uint8_t)(100 - ((largest_free_block * 100) / free_bytes));
    }
};

struct MemorySnapshot {
    HeapStats internal; // MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
    HeapStats psram;    // MALLOC_CAP_SPIRAM
    uint32_t taken_at_ms = 0;
};

// Named DRAM costs used by the admission check. Calibrate on-device (#79) and
// update -- these are starting estimates, not measurements.
enum class Subsystem : uint8_t { WiFi, BleHid, Tls, HttpClient, SdCard };

uint32_t EstimatedDramCost(Subsystem s);

class MemoryMonitor {
   public:
    static constexpr uint8_t kHistoryLen = 32;

    MemorySnapshot Snapshot() const;

    // Free internal DRAM only -- the scarce pool. Cheap enough for a status bar.
    uint32_t FreeInternal() const;
    uint32_t LargestFreeInternalBlock() const;
    uint32_t FreePsram() const;

    // Would starting `s` leave the headroom margin intact, given a contiguous
    // block is required? Checks largest-free-block, not total free, because a
    // total-free check green-lights an allocation that then panics.
    bool CanAfford(Subsystem s, uint32_t headroom_bytes = 24 * 1024) const;

    // Records one sample into the ring for the sparkline on the detail screen.
    void Tick();

    uint8_t HistoryCount() const {
        return _count;
    }
    // Oldest-first; index >= HistoryCount() returns 0.
    uint32_t HistoryAt(uint8_t index) const;

    // Human-readable multi-line dump for the serial log and the info screen.
    void DumpTo(class Print& out) const;

   private:
    uint32_t _history[kHistoryLen] = {0}; // free internal DRAM, KB
    uint8_t _count = 0;
    uint8_t _head = 0;
    uint32_t _last_tick_ms = 0;
};

}  // namespace m5ui
