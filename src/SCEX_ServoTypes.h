#pragma once

#include <cstdint>
#include <string>

#include "SCEX_Easing.h"

namespace SCEX {

// Well-known driver_type strings understood by createServoDriver() (SCEX_ServoManager.cpp).
namespace DriverType {
constexpr const char* kPwm = "pwm";        // SG90-style PWM servo (driver/ledc.h)
constexpr const char* kScs = "scs";        // Feetech SCS0009 on a plain UART
constexpr const char* kM5Scs = "m5_scs";   // Feetech SCS0009 behind PY32IOExpander (M5StackChan)
}  // namespace DriverType

// Self-contained description of a single servo axis. One entry of the
// `servo.axes` list in SCEX_BasicConfig.yaml maps to one ServoAxisConfig.
// Adding a new axis (e.g. a jaw/mouth servo) only means adding one more
// entry here/in the yaml -- nothing else needs to change size.
struct ServoAxisConfig {
    std::string name;         // e.g. "x", "y", "jaw" -- used as the lookup key
    std::string driver_type;  // DriverType::kPwm / kScs / kM5Scs / ...

    // PWM: GPIO the servo signal is attached to.
    // Serial (scs/m5_scs): UART TX pin. Axes that share the same (pin_tx,
    // pin_rx) pair are automatically bound to the same SCEX_FeetechBus.
    int pin_tx = -1;
    int pin_rx = -1;  // Serial only.
    int servo_id = 1; // Serial protocol id (1-based), unused for PWM.

    int16_t offset = 0;
    int16_t lower_limit = 0;
    int16_t upper_limit = 180;
    int16_t start_degree = 90;  // initial position ("center" in stackchan-arduino's yaml)

    EasingType easing = kDefaultEasingType;

    // m5_scs only: PY32IOExpander I2C address + bus pins, used to switch the
    // servo power rail (VM) on/off.
    bool use_io_expander = false;
    uint8_t io_expander_addr = 0x6F;
    int i2c_sda = -1;
    int i2c_scl = -1;
};

}  // namespace SCEX
