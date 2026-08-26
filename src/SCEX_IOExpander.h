// Re-implementation of the register-level subset of PY32IOExpander_Class
// (from stackchan-arduino) needed to switch the "m5_scs" servo power rail
// (VM) on/off, built on SCEX_I2CBus instead of m5::I2C_Class. Register
// addresses (REG_VERSION=0x02, REG_GPIO_M_*=0x03/0x04, REG_GPIO_O_*=0x05/0x06,
// REG_GPIO_PU_*=0x09/0x0A) are taken directly from the reference driver.
#pragma once

// Guarded out on host builds (no ESP_PLATFORM) -- see
// drivers/SCEX_ServoDriver_Pwm.h for the rationale.
#ifdef ESP_PLATFORM

#include <cstdint>

#include "SCEX_I2CBus.h"

namespace SCEX {

class IOExpander {
public:
    static constexpr uint8_t kDefaultAddress = 0x6F;
    static constexpr uint8_t kServoPowerPin = 0;  // VM enable pin on the expander

    bool begin(I2CBus* bus, uint8_t addr = kDefaultAddress);

    void setDirection(uint8_t pin, bool output);
    void setPullMode(uint8_t pin, bool pull_up);
    void digitalWrite(uint8_t pin, bool level);

    // Convenience used by FeetechServoDriver: configures pin 0 as an
    // output, pulled up, and drives it to switch the servo power rail.
    bool setServoPower(bool on);

private:
    bool writeBit(uint8_t reg_l, uint8_t reg_h, uint8_t pin, bool value);

    I2CBus* bus_ = nullptr;
    i2c_master_dev_handle_t dev_ = nullptr;
    bool ready_ = false;
};

// Looks up (or creates + begins) the IOExpander behind the given I2C pins.
IOExpander* getOrCreateIOExpander(int sda, int scl, uint8_t addr = IOExpander::kDefaultAddress);

}  // namespace SCEX

#endif  // ESP_PLATFORM
