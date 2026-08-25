#include "SCEX_Motion.h"

// Guarded out on host builds (no ESP_PLATFORM) -- see
// drivers/SCEX_ServoDriver_Pwm.h for the rationale.
#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace SCEX {

void playMotion(ServoManager& manager, MotionPreset preset, const std::string& axis_x_name,
                 const std::string& axis_y_name) {
    if (preset == MotionPreset::NoMove) return;

    ServoAxisHandle x = manager.findAxis(axis_x_name);
    ServoAxisHandle y = manager.findAxis(axis_y_name);
    if (x == kInvalidServoAxis || y == kInvalidServoAxis) return;

    manager.moveTo(x, 90, y, 75, 500);
    switch (preset) {
        case MotionPreset::Greet:
            manager.moveTo(y, 90, 1000);
            manager.moveTo(y, 75, 1000);
            break;
        case MotionPreset::Laugh:
            for (int i = 0; i < 5; i++) {
                manager.moveTo(y, 80, 500);
                manager.moveTo(y, 60, 500);
            }
            break;
        case MotionPreset::Nod:
            for (int i = 0; i < 5; i++) {
                manager.moveTo(y, 90, 1000);
                manager.moveTo(y, 60, 1000);
            }
            break;
        case MotionPreset::Refuse:
            for (int i = 0; i < 2; i++) {
                manager.moveTo(x, 70, 500);
                manager.moveTo(x, 110, 500);
            }
            break;
        case MotionPreset::Test:
            manager.moveTo(x, 45, 1000);
            manager.moveTo(x, 135, 1000);
            manager.moveTo(x, 90, 1000);
            manager.moveTo(y, 50, 1000);
            manager.moveTo(y, 90, 1000);
            break;
        default:
            break;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    float start_x = static_cast<float>(manager.axisConfig(x).start_degree);
    float start_y = static_cast<float>(manager.axisConfig(y).start_degree);
    manager.moveTo(x, start_x, y, start_y, 1000);
}

}  // namespace SCEX

#endif  // ESP_PLATFORM
