# M5Paper

A UI framework and application platform for the [M5Paper](https://docs.m5stack.com/en/core/m5paper)
— ESP32-D0WDQ6-V3, 4.7" 540×960 16-grey e-ink, GT911 touch, 8 MB PSRAM.

Forked from `m5stack/M5Paper_FactoryTest` and being rebuilt as a monorepo: a
reusable library under `lib/m5paper-ui/`, applications under `apps/`.

> **Status: skeleton.** The structure and interfaces are settled; the
> implementations are written but have never been compiled — there is no
> PlatformIO toolchain installed yet ([#1](../../issues/1)). Treat everything as
> a first draft.

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

## Build

```
pio run -e demo-all              # build
pio run -e demo-all -t upload    # flash
pio device monitor               # 115200 baud; prints the spec sheet at boot
```

## Roadmap

Issue [#91](../../issues/91) holds the sequencing. In short: foundations
(input queue, focus, layout, text measurement) → rendering pipeline → the
components the Notes app needs → BLE HID → the Notes app. The HTML reader and
networking epics are deferred until after that ships.

## License

MIT. See [LICENSE](LICENSE).
