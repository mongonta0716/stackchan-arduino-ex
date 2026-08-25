#include "SCEX_ServoManager.h"

// The background task in this file only makes sense on an actual ESP32
// target; guarded out on host builds (no ESP_PLATFORM) -- see
// drivers/SCEX_ServoDriver_Pwm.h for the rationale.
#ifdef ESP_PLATFORM

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace SCEX {

namespace {
constexpr char kTag[] = "SCEX_ServoManager";

uint32_t nowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}
}  // namespace

ServoManager::~ServoManager() {
    if (task_handle_ != nullptr) {
        vTaskDelete(static_cast<TaskHandle_t>(task_handle_));
    }
}

ServoAxisHandle ServoManager::addAxis(const ServoAxisConfig& cfg, std::unique_ptr<ServoDriver> driver) {
    if (findAxis(cfg.name) != kInvalidServoAxis) {
        ESP_LOGE(kTag, "axis '%s' already registered", cfg.name.c_str());
        return kInvalidServoAxis;
    }
    if (axes_.size() >= kInvalidServoAxis) {
        ESP_LOGE(kTag, "too many axes registered");
        return kInvalidServoAxis;
    }
    auto axis = std::make_unique<ServoAxis>(cfg, driver.get());
    if (!axis->attach()) {
        ESP_LOGE(kTag, "axis '%s': attach() failed", cfg.name.c_str());
        return kInvalidServoAxis;
    }
    drivers_.push_back(std::move(driver));
    axes_.push_back(std::move(axis));
    return static_cast<ServoAxisHandle>(axes_.size() - 1);
}

ServoAxisHandle ServoManager::findAxis(const std::string& name) const {
    for (size_t i = 0; i < axes_.size(); i++) {
        if (axes_[i]->name() == name) {
            return static_cast<ServoAxisHandle>(i);
        }
    }
    return kInvalidServoAxis;
}

void ServoManager::begin(uint32_t tick_hz) {
    if (running_) return;
    tick_period_ms_ = tick_hz == 0 ? 20 : (1000 / tick_hz);
    running_ = true;
    TaskHandle_t handle = nullptr;
    xTaskCreate(&ServoManager::taskEntry, "scex_servo", 4096, this, tskIDLE_PRIORITY + 2, &handle);
    task_handle_ = handle;
}

void ServoManager::taskEntry(void* arg) {
    static_cast<ServoManager*>(arg)->taskLoop();
}

void ServoManager::taskLoop() {
    const TickType_t period = pdMS_TO_TICKS(tick_period_ms_ == 0 ? 1 : tick_period_ms_);
    for (;;) {
        uint32_t now = nowMs();
        for (auto& axis : axes_) {
            axis->update(now);
        }
        vTaskDelay(period);
    }
}

void ServoManager::setEasingType(ServoAxisHandle axis, EasingType type) {
    if (axis >= axes_.size()) return;
    axes_[axis]->setEasingType(type);
}

void ServoManager::moveTo(ServoAxisHandle axis, float degree, uint32_t duration_ms,
                           bool wait_for_completion) {
    if (axis >= axes_.size()) return;
    axes_[axis]->startMove(degree, duration_ms);
    if (wait_for_completion) {
        waitForCompletion(axis);
    }
}

void ServoManager::moveTo(ServoAxisHandle axis_a, float degree_a, ServoAxisHandle axis_b,
                           float degree_b, uint32_t duration_ms, bool wait_for_completion) {
    if (axis_a < axes_.size()) axes_[axis_a]->startMove(degree_a, duration_ms);
    if (axis_b < axes_.size()) axes_[axis_b]->startMove(degree_b, duration_ms);
    if (wait_for_completion) {
        waitForCompletion(axis_a);
        waitForCompletion(axis_b);
    }
}

void ServoManager::setTorque(ServoAxisHandle axis, bool on) {
    if (axis >= axes_.size()) return;
    axes_[axis]->setTorque(on);
}

bool ServoManager::isMoving(ServoAxisHandle axis) const {
    if (axis >= axes_.size()) return false;
    return axes_[axis]->isMoving();
}

bool ServoManager::isMoving() const {
    for (auto& axis : axes_) {
        if (axis->isMoving()) return true;
    }
    return false;
}

float ServoManager::currentDegree(ServoAxisHandle axis) const {
    if (axis >= axes_.size()) return 0.0f;
    return axes_[axis]->currentDegree();
}

void ServoManager::waitForCompletion(ServoAxisHandle axis) const {
    if (axis >= axes_.size()) return;
    while (axes_[axis]->isMoving()) {
        vTaskDelay(pdMS_TO_TICKS(tick_period_ms_ == 0 ? 1 : tick_period_ms_));
    }
}

void ServoManager::waitForCompletion() const {
    while (isMoving()) {
        vTaskDelay(pdMS_TO_TICKS(tick_period_ms_ == 0 ? 1 : tick_period_ms_));
    }
}

}  // namespace SCEX

#endif  // ESP_PLATFORM
