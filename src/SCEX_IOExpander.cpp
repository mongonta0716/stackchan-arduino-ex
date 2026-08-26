#include "SCEX_IOExpander.h"

#ifdef ESP_PLATFORM

#include <map>
#include <memory>
#include <utility>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace SCEX {

namespace {
constexpr char kTag[] = "SCEX_IOExpander";
constexpr uint8_t kRegVersion = 0x02;
constexpr uint8_t kRegGpioModeL = 0x03;
constexpr uint8_t kRegGpioModeH = 0x04;
constexpr uint8_t kRegGpioOutL = 0x05;
constexpr uint8_t kRegGpioOutH = 0x06;
constexpr uint8_t kRegGpioPullUpL = 0x09;
constexpr uint8_t kRegGpioPullUpH = 0x0A;
constexpr uint8_t kRegGpioPullDownL = 0x0B;
constexpr uint8_t kRegGpioPullDownH = 0x0C;
}  // namespace

bool IOExpander::begin(I2CBus* bus, uint8_t addr) {
    bus_ = bus;
    if (bus_ == nullptr) return false;
    dev_ = bus_->addDevice(addr);
    if (dev_ == nullptr) return false;

    // PY32 may still be booting when the ESP32 reaches setup().  Match the
    // reference board support and give it up to about 1.2 seconds.
    for (int attempt = 0; attempt < 6; ++attempt) {
        int version = bus_->readRegister8(dev_, kRegVersion);
        if (version > 0 && version != 0xFF) {
            ready_ = true;
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    ESP_LOGE(kTag, "IOExpander at 0x%02X did not become ready", addr);
    return false;
}

bool IOExpander::writeBit(uint8_t reg_l, uint8_t reg_h, uint8_t pin, bool value) {
    if (!ready_) return false;
    uint8_t reg = pin < 8 ? reg_l : reg_h;
    uint8_t bit = 1u << (pin < 8 ? pin : pin - 8);
    int current = bus_->readRegister8(dev_, reg);
    if (current < 0) return false;
    uint8_t updated = value ? (current | bit) : (current & ~bit);
    return bus_->writeRegister8(dev_, reg, updated);
}

void IOExpander::setDirection(uint8_t pin, bool output) {
    writeBit(kRegGpioModeL, kRegGpioModeH, pin, output);
}

void IOExpander::setPullMode(uint8_t pin, bool pull_up) {
    if (pull_up) {
        writeBit(kRegGpioPullDownL, kRegGpioPullDownH, pin, false);
        writeBit(kRegGpioPullUpL, kRegGpioPullUpH, pin, true);
    } else {
        writeBit(kRegGpioPullUpL, kRegGpioPullUpH, pin, false);
        writeBit(kRegGpioPullDownL, kRegGpioPullDownH, pin, true);
    }
}

void IOExpander::digitalWrite(uint8_t pin, bool level) {
    writeBit(kRegGpioOutL, kRegGpioOutH, pin, level);
}

bool IOExpander::setServoPower(bool on) {
    if (!ready_) return false;
    const bool direction_ok =
        writeBit(kRegGpioModeL, kRegGpioModeH, kServoPowerPin, true);
    const bool pull_down_ok =
        writeBit(kRegGpioPullDownL, kRegGpioPullDownH, kServoPowerPin, false);
    const bool pull_up_ok =
        writeBit(kRegGpioPullUpL, kRegGpioPullUpH, kServoPowerPin, true);
    const bool output_ok =
        writeBit(kRegGpioOutL, kRegGpioOutH, kServoPowerPin, on);
    return direction_ok && pull_down_ok && pull_up_ok && output_ok;
}

namespace {
std::map<std::pair<int, int>, std::unique_ptr<IOExpander>>& expanderRegistry() {
    static std::map<std::pair<int, int>, std::unique_ptr<IOExpander>> registry;
    return registry;
}
}  // namespace

IOExpander* getOrCreateIOExpander(int sda, int scl, uint8_t addr) {
    auto& registry = expanderRegistry();
    auto key = std::make_pair(sda, scl);
    auto it = registry.find(key);
    if (it != registry.end()) {
        return it->second.get();
    }
    I2CBus* bus = getOrCreateI2CBus(sda, scl);
    if (bus == nullptr) return nullptr;
    auto expander = std::make_unique<IOExpander>();
    if (!expander->begin(bus, addr)) {
        ESP_LOGE(kTag, "failed to init IOExpander at sda=%d scl=%d", sda, scl);
        return nullptr;
    }
    IOExpander* raw = expander.get();
    registry.emplace(key, std::move(expander));
    return raw;
}

}  // namespace SCEX

#endif  // ESP_PLATFORM
