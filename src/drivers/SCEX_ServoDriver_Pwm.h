// PWM servo driver (e.g. SG90) implemented directly on top of ESP-IDF's
// driver/ledc.h. No ServoEasing / ESP32Servo dependency: this compiles and
// runs identically under Arduino (framework=arduino, espressif32) and under
// a plain `idf.py` build, because arduino-esp32 exposes the same ESP-IDF
// driver headers.
#pragma once

// Only meaningful on an actual ESP32 target (Arduino-ESP32 or native
// ESP-IDF both define ESP_PLATFORM). Guarded out on host builds (e.g. the
// `native` PlatformIO test env used for SCEX_Easing/SCEX_Yaml) where
// driver/ledc.h does not exist.
#ifdef ESP_PLATFORM

#include "driver/ledc.h"
#include "SCEX_ServoDriver.h"

namespace SCEX {

class PwmServoDriver : public ServoDriver {
public:
    // us range for 0deg/180deg. SG90-class servos are commonly 500-2400us;
    // override in code if a specific servo's datasheet differs.
    PwmServoDriver(uint32_t min_us = 500, uint32_t max_us = 2400);

    bool attach(const ServoAxisConfig& cfg) override;
    void writeAngle(float degree) override;

private:
    void writeMicroseconds(uint32_t us);

    int pin_ = -1;
    ledc_channel_t channel_;
    uint32_t min_us_;
    uint32_t max_us_;
    uint32_t duty_max_;  // (1 << LEDC_TIMER_RESOLUTION) - 1
};

}  // namespace SCEX

#endif  // ESP_PLATFORM
