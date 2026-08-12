# m5paper-ui

A component library, device wrapper and application framework for the M5Paper
(ESP32-D0WDQ6-V3, 4.7" 540×960 16-grey e-ink, GT911 touch, 8 MB PSRAM).

**Status: skeleton.** The structure and the interfaces are settled; the bodies
are written but have never been compiled — there is no PlatformIO toolchain
installed yet (issue #1). Treat every implementation as a first draft.

## Shape

```
src/
  core/          display, input, focus, layout, text, memory, power, device
  components/    label, button, container, scrollview, listview, textinput, …
  modules/       storage, keyboard          (opt-in, one #include each)
  m5paper_ui.h   umbrella header
```

## The Device wrapper

One object answers everything about the hardware and owns the application loop.

```cpp
#include <m5paper_ui.h>

void setup() {
    auto& dev = m5ui::Device::Get();
    dev.Begin();

    dev.GetBattery().percent;              // 74
    dev.Specs().soc.model;                 // "ESP32-D0WDQ6"
    dev.Specs().memory.psram_bytes;        // 8388608
    dev.Memory().LargestFreeInternalBlock();
    dev.Files().WriteString("notes/a.txt", "hello");
    dev.Keyboard().Show();

    static MyApp app;
    dev.Run(app);                          // never returns
}
```

`Begin()` replaces the whole of the old `systeminit.cpp`: power rails, panel
rotation, touch rotation, battery ADC, storage mount, RTC, font load, spec
probe.

`Specs()` is **probed at runtime**, never hardcoded — chip model and revision,
core count, CPU and crystal frequency, radio features, MAC, flash size and
speed, sketch size and OTA headroom, PSRAM and internal DRAM totals, panel
geometry and greyscale depth, touch controller, SD presence and capacity,
SPIFFS usage.

## Battery

`Power::GetBattery()` returns a `BatteryState`: millivolts, percent, charge
state, USB presence, low and critical flags. The voltage→percent mapping is one
pure function (`BatteryPercentFromMillivolts`) over a piecewise-linear LiPo
curve, so it is unit-testable off-device — the stock firmware inlined a linear
3300–4350 mV map inside a status-bar draw call, which reports ~50% for most of
the usable life and then falls off a cliff.

Readings are smoothed over an 8-sample window and cached, so a status bar can
call it every frame. The boot reading is taken inside `Begin()`, so the value is
real from the moment the device is up rather than after the first refresh
period elapses. Cache lifetime defaults to 60 s and is settable at runtime:

```cpp
dev.Power().SetCacheRefresh(5000);  // 5 s, to watch a load step
dev.Power().SetCacheRefresh(0);     // no cache; sample on every call
```

or at boot via `DeviceConfig::battery_cache_refresh_ms`.

## Two memory pools, never summed

Internal DRAM (~290–320 KB) is the contended pool: BLE, WiFi, TLS, task stacks.
PSRAM (8 MB) holds canvases and parse buffers and is effectively plentiful. An
indicator reporting "8 MB free" tells you nothing about whether BLE can start.

`MemoryMonitor` reports them separately and exposes **largest free block**
alongside total free — the internal heap fragments under repeated subsystem
init/deinit, and a total-free check will green-light an allocation that then
panics.

## Design rules that keep selective compilation working

These are load-bearing (issue #87). Violating one silently defeats the
compile-time module selection the monorepo depends on.

1. **No self-registration.** A component must never insert itself into a global
   registry at static-init time. The linker cannot drop a translation unit that
   registers itself. Apps wire components up explicitly.
2. **No file-scope objects with non-trivial constructors.** They run before
   `main()`, force the linker to keep the TU, and here would touch `M5.EPD`
   before `M5.begin()`. Singletons are function-local statics — see
   `Device::Get()` and `Text()`.
3. **Assets are per-component and opt-in.** No library-wide asset blob. The old
   `ImageResource.h` was 89,886 lines pulled in transitively by nearly every
   frame.
4. **One component per translation unit**, so the LDF and `--gc-sections` have
   something to work with.

## Rendering model

Widgets draw into a shadow framebuffer and declare what they dirtied. `Display`
unions the dirty regions, picks an update mode from the caller's `DrawIntent`
plus per-tile accumulated ghost debt, and pushes once per frame.

The debt ledger is a 9×16 grid of tile counters. Fast modes (A2, DU, DU4) add
debt; GC16 clears it. A tile at the limit forces a clean pass regardless of what
the caller asked for. `RefreshFull()` and `RefreshGhostOnly()` are the manual
escape hatches, and `FlushTextRegion()` is the fast path for a clock or counter.

## What is not here yet

BLE HID host (#60–#64), HTML reader (#65–#71), networking and TLS (#72–#75),
Table/DataGrid, date/time pickers, modal overlays and toasts, form validation.
See issue #91 for the sequencing.
