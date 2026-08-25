#include "SCEX_ServoDriver_Pwm.h"

#ifdef ESP_PLATFORM

#include <algorithm>

#include "esp_log.h"

namespace SCEX {

namespace {
constexpr char kTag[] = "SCEX_PWM";
constexpr ledc_timer_t kTimer = LEDC_TIMER_0;
constexpr ledc_timer_bit_t kResolution = LEDC_TIMER_14_BIT;
constexpr uint32_t kFrequencyHz = 50;
constexpr ledc_mode_t kSpeedMode = LEDC_LOW_SPEED_MODE;

// One shared 50Hz timer is enough to drive every PWM axis; only the channel
// differs per axis. LEDC_CHANNEL_MAX is 8 on ESP32 -- more than the number
// of servo axes a Stack-chan realistically has.
int nextChannel() {
    static int channel = 0;
    return channel++;
}

bool timerInitialized() {
    static bool initialized = false;
    if (!initialized) {
        ledc_timer_config_t timer_cfg = {};
        timer_cfg.speed_mode = kSpeedMode;
        timer_cfg.timer_num = kTimer;
        timer_cfg.duty_resolution = kResolution;
        timer_cfg.freq_hz = kFrequencyHz;
        timer_cfg.clk_cfg = LEDC_AUTO_CLK;
        if (ledc_timer_config(&timer_cfg) != ESP_OK) {
            ESP_LOGE(kTag, "ledc_timer_config failed");
            return false;
        }
        initialized = true;
    }
    return true;
}
}  // namespace

PwmServoDriver::PwmServoDriver(uint32_t min_us, uint32_t max_us)
    : channel_(LEDC_CHANNEL_0), min_us_(min_us), max_us_(max_us), duty_max_((1u << kResolution) - 1) {}

bool PwmServoDriver::attach(const ServoAxisConfig& cfg) {
    pin_ = cfg.pin_tx;
    if (pin_ < 0) {
        ESP_LOGE(kTag, "axis '%s': pin_tx is not set", cfg.name.c_str());
        return false;
    }
    if (!timerInitialized()) {
        return false;
    }
    int channel_num = nextChannel();
    if (channel_num >= LEDC_CHANNEL_MAX) {
        ESP_LOGE(kTag, "axis '%s': no free LEDC channel left", cfg.name.c_str());
        return false;
    }
    channel_ = static_cast<ledc_channel_t>(channel_num);

    ledc_channel_config_t ch_cfg = {};
    ch_cfg.gpio_num = pin_;
    ch_cfg.speed_mode = kSpeedMode;
    ch_cfg.channel = channel_;
    ch_cfg.timer_sel = kTimer;
    ch_cfg.duty = 0;
    ch_cfg.hpoint = 0;
    if (ledc_channel_config(&ch_cfg) != ESP_OK) {
        ESP_LOGE(kTag, "axis '%s': ledc_channel_config failed", cfg.name.c_str());
        return false;
    }

    writeAngle(static_cast<float>(cfg.start_degree));
    return true;
}

void PwmServoDriver::writeAngle(float degree) {
    degree = std::clamp(degree, 0.0f, 180.0f);
    uint32_t us = min_us_ + static_cast<uint32_t>((max_us_ - min_us_) * (degree / 180.0f));
    writeMicroseconds(us);
}

void PwmServoDriver::writeMicroseconds(uint32_t us) {
    constexpr uint32_t kPeriodUs = 1000000 / kFrequencyHz;  // 20000us @ 50Hz
    uint32_t duty = static_cast<uint32_t>((static_cast<uint64_t>(us) * duty_max_) / kPeriodUs);
    ledc_set_duty(kSpeedMode, channel_, duty);
    ledc_update_duty(kSpeedMode, channel_);
}

}  // namespace SCEX

#endif  // ESP_PLATFORM
