#include "SCEX_ServoDriver_Feetech.h"

#ifdef ESP_PLATFORM

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "SCEX_IOExpander.h"

namespace SCEX {

namespace {
constexpr char kTag[] = "SCEX_Feetech";

// Feetech SCS(CL) memory table (see header comment for provenance).
constexpr uint8_t kAddrTorqueEnable = 40;
constexpr uint8_t kAddrGoalPositionL = 42;
constexpr uint8_t kAddrGoalTimeL = 44;
constexpr uint8_t kAddrGoalSpeedL = 46;
constexpr uint8_t kAddrPresentPositionL = 56;

constexpr uint8_t kInstrPing = 0x01;
constexpr uint8_t kInstrRead = 0x02;
constexpr uint8_t kInstrWrite = 0x03;

// SCS0009 is mounted in the M5StackChan with its raw direction reversed:
// 0 degrees = 1023, 300 degrees = 0.  This matches stackchan-arduino's
// proven convertSCS0009Pos() implementation.
uint16_t degreeToRaw(float degree) {
    float clamped = std::clamp(degree, 0.0f, 300.0f);
    long pos = std::lround((300.0f - clamped) * 1023.0 / 300.0);
    return static_cast<uint16_t>(std::clamp<long>(pos, 0, 1023));
}

float rawToDegree(int raw) {
    return 300.0f - static_cast<float>(raw) * 300.0f / 1023.0f;
}

}  // namespace

bool FeetechBus::begin(int pin_tx, int pin_rx, int baud) {
    if (initialized_) {
        return true;
    }
    pin_tx_ = pin_tx;
    pin_rx_ = pin_rx;
    baud_ = baud;

    uart_config_t cfg = {};
    cfg.baud_rate = baud;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    if (uart_driver_install(port_, 1024, 1024, 0, nullptr, 0) != ESP_OK) {
        ESP_LOGE(kTag, "uart_driver_install failed on port %d", port_);
        return false;
    }
    if (uart_param_config(port_, &cfg) != ESP_OK ||
        uart_set_pin(port_, pin_tx_, pin_rx_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(kTag, "uart_param_config/uart_set_pin failed on port %d", port_);
        return false;
    }
    initialized_ = true;
    return true;
}

bool FeetechBus::sendPacket(uint8_t id, uint8_t instruction, const uint8_t* params, int param_len) {
    if (!initialized_) return false;
    // A level-1 SCS servo acknowledges write commands.  Discard the ACK
    // left by the previous command before starting the next transaction;
    // otherwise the receive buffer eventually fills and a later ping/read
    // can consume a stale response.
    uart_flush_input(port_);
    uint8_t buf[8 + 32];
    int idx = 0;
    buf[idx++] = 0xFF;
    buf[idx++] = 0xFF;
    buf[idx++] = id;
    uint8_t len = static_cast<uint8_t>(param_len + 2);
    buf[idx++] = len;
    buf[idx++] = instruction;
    uint16_t sum = static_cast<uint16_t>(id) + len + instruction;
    for (int i = 0; i < param_len; i++) {
        buf[idx++] = params[i];
        sum += params[i];
    }
    buf[idx++] = static_cast<uint8_t>(~sum & 0xFF);
    if (uart_write_bytes(port_, reinterpret_cast<const char*>(buf), idx) != idx) {
        return false;
    }
    return uart_wait_tx_done(port_, pdMS_TO_TICKS(20)) == ESP_OK;
}

int FeetechBus::recvPacket(uint8_t expected_id, uint8_t* out_params, int max_params) {
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(50);
    int header_matches = 0;
    while (xTaskGetTickCount() < deadline) {
        uint8_t b;
        if (uart_read_bytes(port_, &b, 1, pdMS_TO_TICKS(10)) <= 0) continue;
        if (header_matches < 2) {
            header_matches = (b == 0xFF) ? header_matches + 1 : 0;
            continue;
        }
        header_matches = 0;
        uint8_t id = b;
        uint8_t len, error;
        if (uart_read_bytes(port_, &len, 1, pdMS_TO_TICKS(20)) <= 0) continue;
        if (uart_read_bytes(port_, &error, 1, pdMS_TO_TICKS(20)) <= 0) continue;
        int param_len = static_cast<int>(len) - 2;
        if (param_len < 0 || param_len > 32) continue;
        uint8_t params[32] = {};
        if (param_len > 0 && uart_read_bytes(port_, params, param_len, pdMS_TO_TICKS(20)) != param_len) {
            continue;
        }
        uint8_t checksum;
        if (uart_read_bytes(port_, &checksum, 1, pdMS_TO_TICKS(20)) <= 0) continue;
        uint16_t sum = static_cast<uint16_t>(id) + len + error;
        for (int i = 0; i < param_len; i++) sum += params[i];
        if (static_cast<uint8_t>(~sum & 0xFF) != checksum) continue;
        if (id != expected_id) continue;
        if (out_params && param_len > 0) {
            std::memcpy(out_params, params, std::min(param_len, max_params));
        }
        return param_len;
    }
    return -1;
}

bool FeetechBus::writeByte(uint8_t id, uint8_t addr, uint8_t value) {
    uint8_t params[2] = {addr, value};
    if (!sendPacket(id, kInstrWrite, params, 2)) return false;
    // Level-1 SCS servos return a zero-parameter status packet.  Consume it
    // before another axis writes to the shared half-duplex bus; otherwise
    // the ACK can collide with or be flushed by the next command.
    return recvPacket(id, nullptr, 0) == 0;
}

bool FeetechBus::writeWords(uint8_t id, uint8_t addr, const uint16_t* values, int count) {
    uint8_t params[1 + 2 * 8];
    params[0] = addr;
    for (int i = 0; i < count; i++) {
        // SCSCL uses big-endian words even though the first register is
        // named *_L in the memory table (the official SCSCL implementation
        // is constructed with End=1).
        params[1 + 2 * i] = static_cast<uint8_t>((values[i] >> 8) & 0xFF);
        params[2 + 2 * i] = static_cast<uint8_t>(values[i] & 0xFF);
    }
    if (!sendPacket(id, kInstrWrite, params, 1 + 2 * count)) return false;
    return recvPacket(id, nullptr, 0) == 0;
}

int FeetechBus::readWord(uint8_t id, uint8_t addr) {
    uint8_t params[2] = {addr, 2};
    if (!sendPacket(id, kInstrRead, params, 2)) return -1;
    uint8_t out[2];
    if (recvPacket(id, out, 2) != 2) return -1;
    return (out[0] << 8) | out[1];
}

bool FeetechBus::ping(uint8_t id) {
    if (!sendPacket(id, kInstrPing, nullptr, 0)) return false;
    return recvPacket(id, nullptr, 0) >= 0;
}

bool FeetechBus::writePosition(uint8_t id, uint16_t position, uint16_t time_ms, uint16_t speed) {
    uint16_t values[3] = {position, time_ms, speed};
    return writeWords(id, kAddrGoalPositionL, values, 3);
}

bool FeetechBus::enableTorque(uint8_t id, bool on) {
    return writeByte(id, kAddrTorqueEnable, on ? 1 : 0);
}

int FeetechBus::readPosition(uint8_t id) {
    return readWord(id, kAddrPresentPositionL);
}

namespace {
std::vector<std::unique_ptr<FeetechBus>>& busRegistry() {
    static std::vector<std::unique_ptr<FeetechBus>> registry;
    return registry;
}
}  // namespace

FeetechBus* getOrCreateFeetechBus(int pin_tx, int pin_rx, int baud) {
    auto& registry = busRegistry();
    for (auto& bus : registry) {
        if (bus->pinTx() == pin_tx && bus->pinRx() == pin_rx) {
            if (bus->baud() != baud) {
                ESP_LOGE(kTag, "shared bus tx=%d rx=%d has conflicting baud rates: %d and %d",
                         pin_tx, pin_rx, bus->baud(), baud);
                return nullptr;
            }
            return bus.get();
        }
    }
    auto bus = std::make_unique<FeetechBus>();
    if (!bus->begin(pin_tx, pin_rx, baud)) {
        ESP_LOGE(kTag, "failed to start bus on tx=%d rx=%d baud=%d", pin_tx, pin_rx, baud);
        return nullptr;
    }
    registry.push_back(std::move(bus));
    return registry.back().get();
}

bool FeetechServoDriver::attach(const ServoAxisConfig& cfg) {
    if (cfg.use_io_expander) {
        IOExpander* expander = getOrCreateIOExpander(cfg.i2c_sda, cfg.i2c_scl, cfg.io_expander_addr);
        if (expander == nullptr || !expander->setServoPower(true)) {
            ESP_LOGE(kTag, "axis '%s': failed to power on servo rail via IOExpander", cfg.name.c_str());
            return false;
        }
        // The servo rail and SCS0009 need a short stabilization period
        // before the first UART transaction.
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    bus_ = getOrCreateFeetechBus(cfg.pin_tx, cfg.pin_rx, cfg.baud);
    if (bus_ == nullptr) {
        return false;
    }
    // The working Arduino implementation waits after Serial2.begin().
    // Without this, the first transactions can be lost on M5StackChan.
    vTaskDelay(pdMS_TO_TICKS(500));
    id_ = static_cast<uint8_t>(cfg.servo_id);
    lower_limit_ = cfg.lower_limit;
    upper_limit_ = cfg.upper_limit;

    bool responded = false;
    for (int attempt = 0; attempt < 3 && !responded; ++attempt) {
        responded = bus_->ping(id_);
        if (!responded) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    if (!responded) {
        // Arduino-ESP32 is commonly built with ERROR as its maximum log
        // level, so this must be an error (not a hidden warning).  Without
        // a ping response the axis must not be reported as attached.
        ESP_LOGE(kTag, "axis '%s': servo id %d did not respond on tx=%d rx=%d baud=%d "
                       "(check servo power, wiring, id and baud rate)",
                 cfg.name.c_str(), id_, cfg.pin_tx, cfg.pin_rx, cfg.baud);
        return false;
    }

    // SCS servos may start with torque disabled after power-up.  Enable it
    // before ServoAxis sends start_degree and verify that reads work too.
    if (!bus_->enableTorque(id_, true)) {
        ESP_LOGE(kTag, "axis '%s': failed to send torque-enable command", cfg.name.c_str());
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    if (bus_->readPosition(id_) < 0) {
        ESP_LOGE(kTag, "axis '%s': servo responded to ping but position read failed", cfg.name.c_str());
        return false;
    }
    // ServoAxis::attach() drives the initial move to start_degree (+offset)
    // right after this returns -- no need to duplicate it here.
    return true;
}

void FeetechServoDriver::writeAngle(float degree) {
    if (bus_ == nullptr) return;
    degree = std::clamp(degree, static_cast<float>(lower_limit_), static_cast<float>(upper_limit_));
    bus_->writePosition(id_, degreeToRaw(degree), 0);
}

float FeetechServoDriver::readAngle() {
    if (bus_ == nullptr) return NAN;
    int raw = bus_->readPosition(id_);
    return raw < 0 ? NAN : rawToDegree(raw);
}

void FeetechServoDriver::setTorque(bool on) {
    if (bus_ != nullptr) {
        bus_->enableTorque(id_, on);
    }
}

}  // namespace SCEX

#endif  // ESP_PLATFORM
