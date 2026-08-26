// Native ESP-IDF example (`idf.py build/flash/monitor`, no Arduino
// framework involved). Same StackchanServoEx.h API as
// examples/arduino_basic/src/main.cpp -- proof the library is genuinely
// dual-target rather than "ESP-IDF via arduino-esp32".
#include "esp_log.h"
#include "esp_spiffs.h"

#include "StackchanServoEx.h"

using namespace SCEX;

namespace {
constexpr char kTag[] = "scex_example";
SystemConfig config;
ServoManager servos;
}  // namespace

extern "C" void app_main(void) {
    esp_vfs_spiffs_conf_t conf = {};
    conf.base_path = "/spiffs";
    conf.partition_label = "storage";
    conf.max_files = 5;
    conf.format_if_mount_failed = true;

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    config.loadConfig("/spiffs/yaml/SCEX_BasicConfig.yaml");

    for (const ServoAxisConfig& axis_cfg : config.servoAxes()) {
        auto driver = createServoDriver(axis_cfg.driver_type);
        if (!driver) {
            ESP_LOGE(kTag, "unknown servo driver type: %s", axis_cfg.driver_type.c_str());
            continue;
        }
        servos.addAxis(axis_cfg, std::move(driver));
    }
    servos.begin();

    ServoAxisHandle x = servos.findAxis("x");
    ServoAxisHandle y = servos.findAxis("y");

    // setEasingType(axis, EasingType) -- one of the easings.net curves
    // (SCEX_Easing.h); default is QuadInOut.
    servos.setEasingType(x, EasingType::BackInOut);
    servos.setEasingType(y, EasingType::SineOut);

    servos.moveTo(x, 45.0f, y, 60.0f, 1000);
    playMotion(servos, MotionPreset::Greet);
}
