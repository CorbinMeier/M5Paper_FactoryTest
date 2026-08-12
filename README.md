# M5Paper

A UI framework and application platform for the [M5Paper](https://docs.m5stack.com/en/core/m5paper)
— ESP32-D0WDQ6-V3, 4.7" 540×960 16-grey e-ink, GT911 touch, 8 MB PSRAM.

Forked from `m5stack/M5Paper_FactoryTest` and being rebuilt as a monorepo: a
reusable library under `lib/m5paper-ui/`, applications under `apps/`.

> **Status: skeleton, but it builds.** The structure and interfaces are
> settled. As of 2026-08-12 the firmware compiles and the host test suite
> passes 21/21 ([#1](../../issues/1)) — but *compiles* is not *works*: none of
> it has run on real glass yet. Treat the implementations as a first draft that
> the compiler has now checked.

## What is here

| | |
|---|---|
| `lib/m5paper-ui/` | The library. [Its README](lib/m5paper-ui/README.md) is the API tour. |
| `apps/demo-all/` | Reference app — device info, component gallery, notes, render diagnostics. |
| `src/` | The original FactoryTest firmware, not yet migrated. |

## Hello, M5Paper

```cpp
#include <m5paper_ui.h>

class HelloScreen : public m5ui::Screen {
   public:
    HelloScreen() : Screen("hello") {}

    void Build() override {
        auto* column = new m5ui::Column(m5ui::tok::kSpaceMd);
        column->SetPadding(m5ui::EdgeInsets::All(m5ui::tok::kSpaceLg));
        Root()->Add(column);

        column->Add(new m5ui::Heading("Hello", 1));

        auto* battery = new m5ui::Label("", m5ui::tok::kTextMd);
        battery->SetText("Battery " +
                         String(m5ui::Device::Get().GetBattery().percent) + "%");
        column->Add(battery);

        auto* button = new m5ui::Button("Refresh", m5ui::ButtonVariant::Filled);
        button->OnPress([battery]() {
            battery->SetText("Battery " +
                             String(m5ui::GetBattery().percent) + "%");
        });
        column->Add(button);
    }
};

class HelloApp : public m5ui::App {
   public:
    HelloApp() {
        Register("hello", []() -> m5ui::Screen* { return new HelloScreen(); });
    }
};

void setup() {
    m5ui::Device::Get().Begin();
    static HelloApp app;
    m5ui::Device::Get().Run(app);
}

void loop() {}
```

## Installing the toolchain

This is a **PlatformIO** project. You do not need the Arduino IDE — the Arduino
*framework* is used, but PlatformIO fetches it for you. Installing Arduino IDE
plus the M5Stack board manager URL gives you a second, unrelated toolchain that
cannot build this tree.

Linux instructions below; the PlatformIO steps are identical on macOS and
Windows, only the serial-port setup differs.

### 1. Prerequisites

Python 3.6+, `git`, and a host C++ compiler for the native tests. On Ubuntu
these are present by default apart from the compiler:

```
sudo apt install build-essential git python3 python3-venv
```

### 2. PlatformIO Core

Use the official installer, **not** `pip`:

```
curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
python3 get-platformio.py
```

`pip install --user platformio` fails on Ubuntu 24.04 and other modern distros,
which mark the system Python `EXTERNALLY-MANAGED`. The installer sidesteps this
by building its own virtualenv under `~/.platformio/penv`, and needs no `sudo`.

Then put it on your PATH — the installer does not do this for you. Add to
`~/.bashrc`, or `~/.zshrc` if you use zsh:

```
export PATH="$PATH:$HOME/.platformio/penv/bin"
```

Verify with `pio --version`.

### 3. Serial port access (only needed to flash)

Two steps, both requiring `sudo`. Add yourself to the `dialout` group:

```
sudo usermod -aG dialout $USER
```

This does **not** take effect until you log out and back in — a fresh terminal
is not enough. Then install PlatformIO's udev rules:

```
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules \
  | sudo tee /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

The M5Paper's USB-serial bridge is covered by in-tree kernel modules, so no
driver package is needed. Plug the device in and confirm a `/dev/ttyUSB0` or
`/dev/ttyACM0` appears; `dmesg | tail` shows what enumerated.

### 4. First build

```
pio run -e demo-all
```

The first run downloads roughly 1 GB of ESP32 toolchain, the Arduino-ESP32
framework and the M5EPD library into `~/.platformio`. Later builds take about
ten seconds. Expect:

```
RAM:   [          ]   0.5% (used 21032 bytes from 4521984 bytes)
Flash: [=         ]   8.4% (used 550285 bytes from 6553600 bytes)
```

### Optional: linting

Not required to build, and not currently installed ([#100](../../issues/100)):

```
sudo apt install clang-format clang-tidy
```

## Build

```
pio run -e demo-all              # build
pio run -e demo-all-debug        # same, with -Og and verbose logging
pio run -e demo-all -t upload    # flash
pio device monitor               # 115200 baud; prints the spec sheet at boot
pio test -e native               # host-side logic tests, no hardware needed
```

## Roadmap

Issue [#91](../../issues/91) holds the sequencing. In short: foundations
(input queue, focus, layout, text measurement) → rendering pipeline → the
components the Notes app needs → BLE HID → the Notes app. The HTML reader and
networking epics are deferred until after that ships.

## License

MIT. See [LICENSE](LICENSE).
