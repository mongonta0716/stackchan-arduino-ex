// Arduino/PlatformIO example. Ports stackchan-arduino's examples/Basic to
// the new API: axes come from SCEX_BasicConfig.yaml's servo.axes list
// (createServoDriver() maps each entry's `driver:` string to a concrete
// driver), and each axis can pick its own easings.net curve.
#include <Arduino.h>
#include <SD.h>
#include <SPIFFS.h>

#include "StackchanServoEx.h"

using namespace SCEX;

namespace {
constexpr int kSdCsPin = 4;  // CoreS3 (and Core/Core2) SD card CS pin
}  // namespace

SystemConfig config;
ServoManager servos;

void setup() {
    Serial.begin(115200);
    delay(1000);

    // SystemConfig reads plain filesystem paths via fopen(); SD.begin() /
    // SPIFFS.begin() below is what makes "/sd/..." / "/spiffs/..." resolve
    // through the Arduino-ESP32 VFS. (On native ESP-IDF, mount SD/SPIFFS/
    // LittleFS via the IDF VFS APIs instead -- see
    // examples/esp-idf_basic/main/main.cpp.) loadConfig() itself tries each
    // candidate path in order and uses the first one it can open, so an SD
    // card takes priority over the SPIFFS image without any code here
    // needing to know which one actually has the file.
    SD.begin(kSdCsPin);  // best-effort: fine if no card is inserted
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed");
    }
    config.loadConfig({"/sd/yaml/SCEX_BasicConfig.yaml", "/spiffs/yaml/SCEX_BasicConfig.yaml"});

    for (const ServoAxisConfig& axis_cfg : config.servoAxes()) {
        auto driver = createServoDriver(axis_cfg.driver_type);
        if (!driver) {
            Serial.printf("unknown servo driver type: %s\n", axis_cfg.driver_type.c_str());
            continue;
        }
        servos.addAxis(axis_cfg, std::move(driver));
    }
    servos.begin();

    ServoAxisHandle x = servos.findAxis("x");
    ServoAxisHandle y = servos.findAxis("y");

    // The API requested for this library: setEasingType(axis, EasingType),
    // one of the easings.net curves (SCEX_Easing.h). Default is QuadInOut.
    servos.setEasingType(x, EasingType::BackInOut);
    servos.setEasingType(y, EasingType::BounceOut);

    servos.moveTo(x, 45.0f, y, 60.0f, 1000);
    playMotion(servos, MotionPreset::Greet);
}

void loop() {}
