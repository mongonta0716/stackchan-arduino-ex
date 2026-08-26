// Feetech SCS-protocol serial servo support (SCS0009 and the M5StackChan
// "m5_scs" variant that additionally powers the servo rail through a
// PY32IOExpander I2C IO expander). Implemented directly on driver/uart.h --
// no SCServo/Dynamixel2Arduino-style Arduino Stream dependency.
//
// Register addresses follow the published Feetech SCS(CL) memory table
// (TORQUE_ENABLE=40, GOAL_POSITION_L=42, GOAL_TIME_L=44, GOAL_SPEED_L=46,
// PRESENT_POSITION_L=56), the same table used by the reference SCServo
// library. Continuous-rotation ("wheel") mode is not ported in this v1 --
// see docs/porting_notes.md.
#pragma once

// Guarded out on host builds (no ESP_PLATFORM) -- see SCEX_ServoDriver_Pwm.h
// for the rationale.
#ifdef ESP_PLATFORM

#include <cstdint>

#include "driver/uart.h"
#include "SCEX_ServoDriver.h"

namespace SCEX {

// One shared half-duplex UART bus. Multiple axes with the same (pin_tx,
// pin_rx) pair reuse a single SCEX_FeetechBus instance (see
// getOrCreateFeetechBus() below) -- this mirrors how the original library
// shared one HardwareSerial across the X/Y servos.
class FeetechBus {
public:
    bool begin(int pin_tx, int pin_rx, int baud = 1000000);

    bool ping(uint8_t id);
    // position: 0-1023 raw servo units. time_ms: 0 = max speed.
    bool writePosition(uint8_t id, uint16_t position, uint16_t time_ms, uint16_t speed = 0);
    bool enableTorque(uint8_t id, bool on);
    // Returns -1 on read failure, otherwise 0-1023 raw servo units.
    int readPosition(uint8_t id);

    int pinTx() const { return pin_tx_; }
    int pinRx() const { return pin_rx_; }
    int baud() const { return baud_; }

private:
    bool writeByte(uint8_t id, uint8_t addr, uint8_t value);
    bool writeWords(uint8_t id, uint8_t addr, const uint16_t* values, int count);
    int readWord(uint8_t id, uint8_t addr);
    bool sendPacket(uint8_t id, uint8_t instruction, const uint8_t* params, int param_len);
    int recvPacket(uint8_t expected_id, uint8_t* out_params, int max_params);

    // Match Arduino's Serial2, which is used by the proven M5StackChan
    // implementation on GPIO6/GPIO7.
    uart_port_t port_ = UART_NUM_2;
    int pin_tx_ = -1;
    int pin_rx_ = -1;
    int baud_ = 0;
    bool initialized_ = false;
};

// Looks up (or creates) the FeetechBus for the given pin pair. Owned
// statically for the lifetime of the program -- servo buses are never torn
// down on an embedded target.
FeetechBus* getOrCreateFeetechBus(int pin_tx, int pin_rx, int baud = 1000000);

class FeetechServoDriver : public ServoDriver {
public:
    // use_io_expander: true for the "m5_scs" driver type (switches the VM
    // servo power rail on via SCEX_IOExpander before talking to the bus).
    bool attach(const ServoAxisConfig& cfg) override;
    void writeAngle(float degree) override;
    float readAngle() override;
    void setTorque(bool on) override;

private:
    FeetechBus* bus_ = nullptr;
    uint8_t id_ = 1;
    int16_t lower_limit_ = 0;
    int16_t upper_limit_ = 300;
};

}  // namespace SCEX

#endif  // ESP_PLATFORM
