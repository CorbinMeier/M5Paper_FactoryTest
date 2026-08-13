#pragma once
// The only screen in calendar-demo: current month grid plus the clock.
// White page, black text throughout -- today's cell gets an outline rather
// than an inverted fill, so nothing on the panel is ever black-on-black or
// relies on a grey fill to read (issue #110).

#include <m5paper_ui.h>

// Not in the umbrella header on purpose (see ble_companion.h).
#include "modules/ble_companion.h"

namespace calendar {

class CalendarScreen : public m5ui::Screen {
   public:
    explicit CalendarScreen(const m5ui::WeatherSnapshot& weather);

    void Build() override;

   private:
    void BuildMonthGrid(m5ui::Column& column, int16_t year, uint8_t month,
                        uint8_t today_day);

    m5ui::WeatherSnapshot _weather;
};

}  // namespace calendar
