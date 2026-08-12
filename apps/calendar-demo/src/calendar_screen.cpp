#include "calendar_screen.h"

using namespace m5ui;

namespace calendar {

namespace {

const char* const kMonthNames[12] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};

const char* const kWeekdayAbbrev[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

// A day cell, sized to share the row evenly with its six siblings. Today gets
// an outline -- not a filled background -- so the page stays white with black
// text throughout (issue #110).
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
        label->SetColor(tok::kFg);
        label->SetBold(is_today);
        label->SetAlign(TextAlign::Center);
        cell->Add(label);
    }
    return cell;
}

}  // namespace

CalendarScreen::CalendarScreen() : Screen("calendar") {}

void CalendarScreen::Build() {
    Column* root = new Column(tok::kSpaceMd);
    root->SetPadding(EdgeInsets::All(tok::kSpaceLg));
    _root = root;

    Clock& clock = Dev().Time();

    int year = 2026, month = 1, day = 1;
    sscanf(clock.DateString().c_str(), "%d-%d-%d", &year, &month, &day);

    Heading* month_heading =
        new Heading(String(kMonthNames[month - 1]) + " " + String(year), 1);
    month_heading->SetColor(tok::kFg);
    month_heading->SetAlign(TextAlign::Center);
    root->Add(month_heading);

    Label* clock_label = new Label(clock.TimeString(/*with_seconds=*/true),
                                    tok::kText2Xl);
    clock_label->SetColor(tok::kFg);
    clock_label->SetBold(true);
    clock_label->SetAlign(TextAlign::Center);
    root->Add(clock_label);

    root->Add(new Divider());

    BuildMonthGrid(*root, (int16_t)year, (uint8_t)month, (uint8_t)day);
}

void CalendarScreen::BuildMonthGrid(Column& column, int16_t year,
                                    uint8_t month, uint8_t today_day) {
    Row* header = new Row(0);
    for (const char* abbrev : kWeekdayAbbrev) {
        Label* label = new Label(abbrev, tok::kTextSm);
        label->SetColor(tok::kFg);
        label->SetBold(true);
        label->SetAlign(TextAlign::Center);
        label->SetFlex(1);
        header->Add(label);
    }
    column.Add(header);

    const uint8_t first_weekday = DayOfWeek(year, month, 1); // 0 = Sunday
    const uint8_t days_in_month = DaysInMonth(year, month);
    const uint16_t total_cells = (uint16_t)first_weekday + days_in_month;
    const uint8_t weeks = (uint8_t)((total_cells + 6) / 7);

    uint8_t day = 1;
    for (uint8_t w = 0; w < weeks; ++w) {
        Row* week = new Row(0);
        for (uint8_t col = 0; col < 7; ++col) {
            const uint16_t position = (uint16_t)(w * 7 + col);
            const bool in_month =
                position >= first_weekday && day <= days_in_month;
            if (in_month) {
                week->Add(MakeDayCell(String(day), day == today_day));
                day++;
            } else {
                week->Add(MakeDayCell("", false));
            }
        }
        column.Add(week);
    }
}

}  // namespace calendar
