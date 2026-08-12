#include "calendar_math.h"

namespace m5ui {

bool IsLeapYear(int16_t year) {
    return (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
}

uint8_t DaysInMonth(int16_t year, uint8_t month) {
    static const uint8_t kDays[12] = {31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    if (month == 2 && IsLeapYear(year)) return 29;
    return kDays[month - 1];
}

uint8_t DayOfWeek(int16_t year, uint8_t month, uint8_t day) {
    static const uint8_t kOffset[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int32_t y = year;
    if (month < 3) y -= 1;
    return (uint8_t)((y + y / 4 - y / 100 + y / 400 + kOffset[month - 1] + day) % 7);
}

}  // namespace m5ui
