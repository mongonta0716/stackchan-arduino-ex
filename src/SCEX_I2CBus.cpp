#include "SCEX_I2CBus.h"

#ifdef ESP_PLATFORM

#include <memory>
#include <vector>

#include "esp_log.h"

namespace SCEX {

namespace {
constexpr char kTag[] = "SCEX_I2C";
constexpr uint32_t kTimeoutMs = 100;
}  // namespace

bool I2CBus::begin(int sda, int scl, uint32_t freq_hz) {
    if (bus_handle_ != nullptr) {
        return true;
    }
    sda_ = sda;
    scl_ = scl;

    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = -1;  // auto-select a free port
    bus_cfg.sda_io_num = static_cast<gpio_num_t>(sda);
    bus_cfg.scl_io_num = static_cast<gpio_num_t>(scl);
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    if (i2c_new_master_bus(&bus_cfg, &bus_handle_) != ESP_OK) {
        ESP_LOGE(kTag, "i2c_new_master_bus failed (sda=%d scl=%d)", sda, scl);
        return false;
    }
    (void)freq_hz;  // stored per-device below, not per-bus
    return true;
}

i2c_master_dev_handle_t I2CBus::addDevice(uint8_t addr7) {
    if (bus_handle_ == nullptr) return nullptr;
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = addr7;
    dev_cfg.scl_speed_hz = 100000;

    i2c_master_dev_handle_t dev = nullptr;
    if (i2c_master_bus_add_device(bus_handle_, &dev_cfg, &dev) != ESP_OK) {
        ESP_LOGE(kTag, "i2c_master_bus_add_device failed (addr=0x%02X)", addr7);
        return nullptr;
    }
    return dev;
}

bool I2CBus::writeRegister8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value) {
    if (dev == nullptr) return false;
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(dev, buf, sizeof(buf), kTimeoutMs) == ESP_OK;
}

int I2CBus::readRegister8(i2c_master_dev_handle_t dev, uint8_t reg) {
    if (dev == nullptr) return -1;
    uint8_t value = 0;
    if (i2c_master_transmit_receive(dev, &reg, 1, &value, 1, kTimeoutMs) != ESP_OK) {
        return -1;
    }
    return value;
}

namespace {
std::vector<std::unique_ptr<I2CBus>>& busRegistry() {
    static std::vector<std::unique_ptr<I2CBus>> registry;
    return registry;
}
}  // namespace

I2CBus* getOrCreateI2CBus(int sda, int scl) {
    auto& registry = busRegistry();
    for (auto& bus : registry) {
        if (bus->sda() == sda && bus->scl() == scl) {
            return bus.get();
        }
    }
    auto bus = std::make_unique<I2CBus>();
    if (!bus->begin(sda, scl)) {
        return nullptr;
    }
    registry.push_back(std::move(bus));
    return registry.back().get();
}

}  // namespace SCEX

#endif  // ESP_PLATFORM
