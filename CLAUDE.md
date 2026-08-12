# M5Paper — project notes

Monorepo: a reusable UI/device library plus the applications built on it, for
the M5Paper (ESP32-D0WDQ6-V3, 4.7" 540×960 16-grey e-ink, GT911 touch, 8 MB
PSRAM, 16 MB flash).

Started as a fork of `m5stack/M5Paper_FactoryTest`. The original firmware is
preserved under `src/` and `src/frame/` while the new framework is brought up
alongside it.

## Layout

```
lib/m5paper-ui/       the library (PlatformIO's LDF discovers lib/ automatically)
  src/core/           display, input, focus, layout, text, memory, power, device
  src/components/     label, button, container, scrollview, listview, textinput…
  src/modules/        storage, keyboard — opt-in, one #include each
apps/demo-all/        the reference app; demos every component
src/                  legacy FactoryTest firmware, not yet migrated
platformio.ini        one [env:] per app over a shared [env] base
```

## Build

PlatformIO Core lives in `~/.platformio/penv` and is **not** on PATH by default
— see the README for setup. The project builds as of 2026-08-12 (issue #1).

```
pio run -e demo-all          # build
pio run -e demo-all -t upload
pio test -e native           # host-side tests (issue #17), 21/21 passing
```

Caveats that still bite:

- **Compiles ≠ works.** Nothing has run on real hardware. The compiler has
  checked the implementations; the panel has not.
- **No lint stage.** clang-format/clang-tidy/cppcheck are still absent (issue
  #100), so changes are verified by test + build only. `.clang-format` exists
  but nothing enforces it.
- **Flashing needs `dialout` + udev rules** (issue #93), separate from the
  build toolchain.
- The shared config block is `[esp32_base]`, deliberately not `[env]`: a
  section named `[env]` is the implicit base for *every* environment, and it
  leaked `board = m5stack-fire` into `[env:native]` (issue #98). New app envs
  need `extends = esp32_base`.
- **The library and M5EPD share a global namespace.** M5EPD.h declares a global
  `Button`, which is ambiguous against `m5ui::Button` in any TU that opens with
  `using namespace m5ui;` — qualify as `m5ui::Button` (issue #95). Watch for
  the same collision on other names. Relatedly, never write `class Foo&` as a
  parameter type inside `namespace m5ui` for a global Arduino type: that
  declares a phantom `m5ui::Foo` rather than naming `::Foo` (issue #96).

## Non-obvious facts

- **The panel is rotated.** The M5EPD library's native geometry is landscape;
  the framework applies `ROTATE_90` at init, so everything above the driver is
  portrait 540×960. Use `m5ui::kDisplayW/kDisplayH` — issue #27 exists because
  `540` and `960` appear as bare literals in 28 places across 11 legacy files.
- **Two memory pools, never summed.** Internal DRAM (~290–320 KB) is what BLE,
  WiFi, TLS and task stacks contend for. PSRAM (8 MB) holds canvases. A "free
  memory" figure that adds them is worse than none. And **largest free block**
  matters more than total free: the internal heap fragments, so total-free can
  read 80 KB while no 20 KB allocation will succeed.
- **Canvases live in PSRAM by necessity.** `Frame_Home` in the legacy firmware
  allocates ~331 KB of canvas, which exceeds total internal DRAM — the firmware
  only boots because `createCanvas` falls back to PSRAM.
- **The 3.4 MB `spiffs` partition in `default_16MB.csv` is unused** by the
  legacy firmware. `m5ui::Storage` uses it as the fallback backend when no SD
  card is present.
- **E-ink has no animation.** Anything designed around a blink, a fade or an
  inertial scroll decay will look broken. Scrolling tracks the finger 1:1 with
  A2 updates and settles with a clean pass; the text caret is solid, not
  blinking.

## Library design rules (issue #87)

These are load-bearing. Breaking one silently defeats the compile-time module
selection the monorepo depends on.

1. **No self-registration.** Never insert a component into a global registry at
   static-init time — the linker cannot drop a TU that registers itself, so it
   ends up in every app. Apps wire components up explicitly.
2. **No file-scope objects with non-trivial constructors.** They run before
   `main()`, pin the TU, and here would touch `M5.EPD` before `M5.begin()`.
   Singletons are function-local statics (`Device::Get()`, `Text()`). The
   legacy code violates this at `frame_lifegame.cpp:19-22` and
   `systeminit.cpp:9,11`.
3. **Assets are per-component and opt-in.** No library-wide blob.
   `src/resources/ImageResource.h` is 89,886 lines pulled in transitively by
   nearly every legacy frame — the exact anti-pattern (issue #14).
4. **One component per translation unit**, so the LDF and `--gc-sections` have
   something to work with.

## Conventions

- Namespace `m5ui` for the library, one namespace per app (`demo`).
- `#pragma once` in the new tree; the legacy `src/` keeps its include guards.
- 4-space indent, 80 columns, Google-ish braces — see `.clang-format`.
- Members are `_snake_case`, methods `PascalCase`, constants `kCamelCase`.
- Callbacks are `std::function`, never C function pointers (issue #33).
- Comments explain *why*, not *what*. If a value is a guess rather than a
  measurement, say so at the definition.

## Issue tracking

GitHub is configured (`gh repo view` works). Every fix or feature gets an issue
first. Epics: #19 GUI foundations, #20 rendering, #21 scrolling, #22 components,
#23 forms, #24 Bluetooth, #25 HTML reader, #26 networking, #76 memory, #84
monorepo. **#91 is the sequencing roadmap** — read it before picking up work.
