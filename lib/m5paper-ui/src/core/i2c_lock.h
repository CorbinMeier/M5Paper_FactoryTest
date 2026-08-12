#pragma once
// Serialises access to I2C0 (issue #107).
//
// The GT911 touch controller and the BM8563 RTC sit on the same bus, and after
// the render/input split they are read from different tasks: touch from the
// input task on core 0, the clock from the UI task on core 1. Arduino's Wire is
// not thread-safe -- two concurrent transactions interleave and both come back
// wrong -- so every I2C user takes this lock first.
//
// The panel is on SPI and is touched only by the UI task, so it needs no
// equivalent.

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace m5ui {

// Function-local static rather than a file-scope object: the library forbids
// non-trivial constructors at static-init time, and this must not run before
// M5.begin() (see rule 2 in issue #87).
inline SemaphoreHandle_t I2CMutex() {
    static SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    return mutex;
}

// RAII guard. Transactions here are microseconds, so blocking indefinitely is
// correct -- a timeout would only convert a bus stall into a silent bad read.
class I2CLock {
   public:
    I2CLock() {
        xSemaphoreTake(I2CMutex(), portMAX_DELAY);
    }
    ~I2CLock() {
        xSemaphoreGive(I2CMutex());
    }

    I2CLock(const I2CLock&) = delete;
    I2CLock& operator=(const I2CLock&) = delete;
};

}  // namespace m5ui
