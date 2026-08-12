#include "home_screen.h"

using namespace m5ui;

namespace demo {

const HomeScreen::MenuEntry HomeScreen::kMenu[] = {
    {"device", "Device", "Specs, battery, memory -- all live"},
    {"components", "Components", "Every widget in the library"},
    {"notes", "Notes", "Keyboard, storage, text editing"},
    {"render", "Rendering", "Update modes and ghost debt"},
};

const uint32_t HomeScreen::kMenuCount =
    sizeof(HomeScreen::kMenu) / sizeof(HomeScreen::kMenu[0]);

HomeScreen::HomeScreen() : DemoScreen("home", "m5paper-ui", false) {}

void HomeScreen::BuildContent(Column& column) {
    Label* intro = new Label(
        "A component library, device wrapper and application framework for the "
        "M5Paper. Everything below reads from the running hardware.",
        tok::kTextSm);
    intro->SetColor(tok::kFgMuted);
    column.Add(intro);

    VirtualList* menu = new VirtualList();
    menu->SetRowHeight(tok::kListRowH);
    menu->SetPreferredSize(Size{kDisplayW, (int16_t)(tok::kListRowH * kMenuCount)});
    menu->SetDataSource(
        []() { return kMenuCount; },
        [](uint32_t index) {
            ListItem item;
            item.title = kMenu[index].title;
            item.subtitle = kMenu[index].subtitle;
            item.trailing = ">";
            return item;
        });
    menu->OnSelect([this](uint32_t index) {
        if (App* app = Owner()) app->Push(kMenu[index].route);
    });
    column.Add(menu);

    // A quick battery read straight from the wrapper, to show the shortcut.
    const BatteryState battery = Device::Get().GetBattery();
    Label* summary = new Label(
        "Battery " + String(battery.percent) + "% (" +
            String(battery.millivolts) + " mV)  |  Free DRAM " +
            FormatBytes(Device::Get().Memory().FreeInternal()),
        tok::kTextXs);
    summary->SetColor(tok::kFgMuted);
    summary->SetAlign(TextAlign::Center);
    column.Add(summary);
}

}  // namespace demo
