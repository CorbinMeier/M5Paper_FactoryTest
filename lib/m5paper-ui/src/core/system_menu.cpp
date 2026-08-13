#include "system_menu.h"

#include "app.h"
#include "calendar_math.h"
#include "device.h"

#include "../components/button.h"
#include "../components/container.h"
#include "../components/label.h"

namespace m5ui {

namespace {

const char* const kMonthNames[12] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};

const char* const kWeekdayAbbrev[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

// Same shape as calendar-demo's cell -- duplicated rather than shared because
// that screen lives in an app, not the library.
Container* MakeDayCell(const String& text, bool is_today) {
    Container* cell = new Container(Axis::Vertical);
    cell->SetFlex(1);
    cell->SetJustify(Justify::Center);
    cell->SetCrossAlign(Align::Center);
    cell->SetPadding(EdgeInsets::All(tok::kSpaceXs));
    if (is_today) {
        cell->SetBorder(tok::kFg, tok::kBorderWidth);
        cell->SetRadius(tok::kRadiusSm);
    }
    if (text.length() > 0) {
        Label* label = new Label(text, tok::kTextMd);
        label->SetBold(is_today);
        label->SetAlign(TextAlign::Center);
        cell->Add(label);
    }
    return cell;
}

Label* SectionLabel(const String& text) {
    Label* label = new Label(text, tok::kTextSm);
    label->SetColor(tok::kFgMuted);
    return label;
}

}  // namespace

SystemMenuScreen::SystemMenuScreen() : Screen(App::kSystemMenuRoute) {}

void SystemMenuScreen::Build() {
    switch (_mode) {
        case Mode::Blank: BuildBlankSplash(); return;
        case Mode::Calendar: BuildCalendarSplash(); return;
        case Mode::Menu: default: BuildMenu(); return;
    }
}

void SystemMenuScreen::BuildMenu() {
    Column* root = new Column(tok::kSpaceMd);
    root->SetPadding(EdgeInsets::All(tok::kSpaceLg));
    // Add to the framework-owned ScreenRoot, don't replace it (issue #111).
    Root()->Add(root);

    Heading* title = new Heading("System Menu", 2);
    title->SetAlign(TextAlign::Center);
    root->Add(title);
    root->Add(new Divider());

    root->Add(SectionLabel("Connectivity"));

    // Stubs: no WiFi module (epic #26) or BLE stack (epic #24, #59) exist
    // yet. Disabled rather than omitted, so the menu shape is already right
    // for when those land.
    Button* wifi = new Button("Wi-Fi -- unavailable", ButtonVariant::Ghost);
    wifi->SetEnabled(false);
    root->Add(wifi);

    Button* bluetooth = new Button("Bluetooth -- unavailable", ButtonVariant::Ghost);
    bluetooth->SetEnabled(false);
    root->Add(bluetooth);

    root->Add(new Divider());
    root->Add(SectionLabel("Power off"));

    root->Add((new Button("Blank screen, then power off", ButtonVariant::Outline))
                  ->OnPress([this] { PowerOffAfter(Mode::Blank); }));
    root->Add((new Button("Power off now (leave screen as-is)", ButtonVariant::Outline))
                  ->OnPress([this] { Dev().Shutdown(); }));
    root->Add((new Button("Show calendar, then power off", ButtonVariant::Outline))
                  ->OnPress([this] { PowerOffAfter(Mode::Calendar); }));

    root->Add(new Spacer());
    root->Add((new Button("Close", ButtonVariant::Filled))->OnPress([this] { Close(); }));
}

void SystemMenuScreen::BuildCalendarSplash() {
    Column* root = new Column(tok::kSpaceMd);
    root->SetPadding(EdgeInsets::All(tok::kSpaceLg));
    Root()->Add(root);

    Clock& clock = Dev().Time();
    int year = 2026, month = 1, day = 1;
    sscanf(clock.DateString().c_str(), "%d-%d-%d", &year, &month, &day);

    Heading* month_heading =
        new Heading(String(kMonthNames[month - 1]) + " " + String(year), 1);
    month_heading->SetAlign(TextAlign::Center);
    root->Add(month_heading);

    Label* clock_label =
        new Label(clock.TimeString(/*with_seconds=*/true), tok::kText2Xl);
    clock_label->SetBold(true);
    clock_label->SetAlign(TextAlign::Center);
    root->Add(clock_label);

    root->Add(new Divider());

    Row* header = new Row(0);
    for (const char* abbrev : kWeekdayAbbrev) {
        Label* label = new Label(abbrev, tok::kTextSm);
        label->SetBold(true);
        label->SetAlign(TextAlign::Center);
        label->SetFlex(1);
        header->Add(label);
    }
    root->Add(header);

    const int16_t y = (int16_t)year;
    const uint8_t m = (uint8_t)month;
    const uint8_t first_weekday = DayOfWeek(y, m, 1); // 0 = Sunday
    const uint8_t days_in_month = DaysInMonth(y, m);
    const uint16_t total_cells = (uint16_t)first_weekday + days_in_month;
    const uint8_t weeks = (uint8_t)((total_cells + 6) / 7);

    uint8_t day_n = 1;
    for (uint8_t w = 0; w < weeks; ++w) {
        Row* week = new Row(0);
        for (uint8_t col = 0; col < 7; ++col) {
            const uint16_t position = (uint16_t)(w * 7 + col);
            const bool in_month = position >= first_weekday && day_n <= days_in_month;
            if (in_month) {
                week->Add(MakeDayCell(String(day_n), day_n == (uint8_t)day));
                day_n++;
            } else {
                week->Add(MakeDayCell("", false));
            }
        }
        root->Add(week);
    }
}

void SystemMenuScreen::PowerOffAfter(Mode mode) {
    _mode = mode;
    Teardown();
    EnsureBuilt();
    InvalidateAll();
    // Shutdown() is [[noreturn]], so the splash must be on the panel before
    // it is called -- Paint() into the shadow canvas, then Flush() it for
    // real, synchronously, rather than waiting for the next Step().
    Paint(Dev().Panel());
    Dev().Panel().Flush(DrawIntent::Clean);
    Dev().Shutdown();
}

void SystemMenuScreen::Close() {
    if (Owner() != nullptr) Owner()->Pop();
}

}  // namespace m5ui
