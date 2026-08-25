// Easing pattern demo. Each Button A press advances to the next
// SCEX_Easing.h curve (all 31: Linear + the 30 easings.net types), applies
// it to axis "x" via ServoManager::setEasingType(), and swings that axis
// from lower_limit to upper_limit and back once so you can see the curve.
// The curve currently playing is shown centered on the display.
#include <M5Unified.h>
#include <SD.h>
#include <SPIFFS.h>

#include "StackchanServoEx.h"

using namespace SCEX;

namespace {
constexpr int kSdCsPin = 4;  // CoreS3 (and Core/Core2) SD card CS pin

SystemConfig config;
ServoManager servos;
ServoAxisHandle axis_x = kInvalidServoAxis;
int easing_index = 0;

void showEasingName(EasingType type) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(1);
    M5.Display.drawString("Press BtnA for next pattern", M5.Display.width() / 2, 24);

    M5.Display.setTextSize(3);
    M5.Display.drawString(easingTypeName(type), M5.Display.width() / 2, M5.Display.height() / 2);

    M5.Display.setTextSize(1);
    char label[32];
    snprintf(label, sizeof(label), "%d / %d", easing_index + 1, kEasingTypeCount);
    M5.Display.drawString(label, M5.Display.width() / 2, M5.Display.height() - 24);
}

// One full lower_limit -> upper_limit -> lower_limit swing on axis "x",
// using whatever easing type was just set on it.
void playCurrentEasing() {
    const ServoAxisConfig& cfg = servos.axisConfig(axis_x);
    constexpr uint32_t kSwingMs = 1200;
    servos.moveTo(axis_x, static_cast<float>(cfg.upper_limit), kSwingMs);
    servos.moveTo(axis_x, static_cast<float>(cfg.lower_limit), kSwingMs);
}

}  // namespace

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    // CoreS3 has no physical buttons -- turn the bottom 40px of the
    // touchscreen into virtual BtnA/BtnB/BtnC zones so M5.BtnA.wasPressed()
    // below still works via touch.
    M5.setTouchButtonHeight(40);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    Serial.begin(115200);

    // Mount every filesystem a candidate config path lives on; loadConfig()
    // itself tries the paths in order and uses the first one it can open --
    // an SD card takes priority over the SPIFFS image so re-flashing the SD
    // card doesn't require re-uploading SPIFFS too.
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
        ServoAxisHandle handle = servos.addAxis(axis_cfg, std::move(driver));
        if (axis_cfg.name == "x") {
            axis_x = handle;
        }
    }
    servos.begin();

    if (axis_x == kInvalidServoAxis) {
        Serial.println("SCEX_BasicConfig.yaml has no axis named 'x'");
        M5.Display.drawString("axis 'x' not found", M5.Display.width() / 2, M5.Display.height() / 2);
        return;
    }

    EasingType initial = kAllEasingTypes[easing_index];
    servos.setEasingType(axis_x, initial);
    showEasingName(initial);
}

void loop() {
    M5.update();
    if (axis_x == kInvalidServoAxis) {
        return;
    }

    if (M5.BtnA.wasPressed()) {
        easing_index = (easing_index + 1) % kEasingTypeCount;
        EasingType current = kAllEasingTypes[easing_index];
        servos.setEasingType(axis_x, current);
        showEasingName(current);
        playCurrentEasing();  // blocks loop() for the swing; button input resumes right after
    }
}
