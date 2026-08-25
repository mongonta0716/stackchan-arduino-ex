// Thin wrapper over ESP-IDF's driver/i2c_master.h (the modern I2C master
// driver, available since ESP-IDF v5.2 -- used instead of M5Unified's
// m5::I2C_Class so this code has no Arduino/M5Unified dependency).
#pragma once

// Guarded out on host builds (no ESP_PLATFORM) -- see
// drivers/SCEX_ServoDriver_Pwm.h for the rationale.
#ifdef ESP_PLATFORM

#include <cstdint>

#include "driver/i2c_master.h"

namespace SCEX {

class I2CBus {
public:
    bool begin(int sda, int scl, uint32_t freq_hz = 100000);

    // Attaches a 7-bit-address device to this bus. Returns nullptr on failure.
    i2c_master_dev_handle_t addDevice(uint8_t addr7);

    bool writeRegister8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value);
    // Returns -1 on failure, otherwise the register value (0-255).
    int readRegister8(i2c_master_dev_handle_t dev, uint8_t reg);

    int sda() const { return sda_; }
    int scl() const { return scl_; }

private:
    i2c_master_bus_handle_t bus_handle_ = nullptr;
    int sda_ = -1;
    int scl_ = -1;
};

// Looks up (or creates) the I2CBus for the given SDA/SCL pin pair.
I2CBus* getOrCreateI2CBus(int sda, int scl);

}  // namespace SCEX

#endif  // ESP_PLATFORM
