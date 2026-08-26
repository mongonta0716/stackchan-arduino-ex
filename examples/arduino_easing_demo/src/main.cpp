// Easing pattern demo. Button A plays the SCEX_Easing.h curve currently
// shown on the display. Button C advances to the next curve (all 31: Linear
// + the 30 easings.net types) without playing it. Holding Button B for two
// seconds plays every curve from the current one through the last one.
#include <M5Unified.h>
#include <SD.h>
#include <SPIFFS.h>

#include "StackchanServoEx.h"

using namespace SCEX;

namespace {
constexpr int kSdCsPin = 4;  // CoreS3 (and Core/Core2) SD card CS pin

// ESP_LOGI is compiled out by some Arduino-ESP32 builds, so print the
// values needed while checking a config directly to the USB serial port.
void printLoadedParameters(const SystemConfig& loaded_config) {
    Serial.println("=== SCEX loaded parameters ===");
    for (const ServoAxisConfig& axis : loaded_config.servoAxes()) {
        Serial.printf("axis '%s': driver=%s tx=%d rx=%d id=%d baud=%d offset=%d "
                      "limits=[%d,%d] start=%d easing=%s i2c_sda=%d i2c_scl=%d\n",
                      axis.name.c_str(), axis.driver_type.c_str(), axis.pin_tx, axis.pin_rx,
                      axis.servo_id, axis.baud, axis.offset, axis.lower_limit, axis.upper_limit,
                      axis.start_degree, easingTypeName(axis.easing), axis.i2c_sda, axis.i2c_scl);
    }
    Serial.printf("auto_power_off_time=%u font_language=%s led_lr=%u led_pin=%d takao_base=%d\n",
                  loaded_config.autoPowerOffTime(), loaded_config.fontLanguage().c_str(),
                  loaded_config.ledLr(), loaded_config.ledPin(), loaded_config.useTakaoBase());
    Serial.println("=== end SCEX parameters ===");
}

SystemConfig config;
ServoManager servos;
std::vector<ServoAxisHandle> demo_axes;
int easing_index = 0;

void showEasingName(EasingType type, bool auto_running = false) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(1);
    if (auto_running) {
        M5.Display.drawString("AUTO - Touch screen to stop", M5.Display.width() / 2, 24);
    } else {
        M5.Display.drawString("BtnA: Play   BtnC: Next", M5.Display.width() / 2, 16);
        M5.Display.drawString("Hold BtnB 2 sec: Auto", M5.Display.width() / 2, 32);
    }

    M5.Display.setTextSize(3);
    M5.Display.drawString(easingTypeName(type), M5.Display.width() / 2, M5.Display.height() / 2);

    M5.Display.setTextSize(1);
    char label[32];
    snprintf(label, sizeof(label), "%d / %d", easing_index + 1, kEasingTypeCount);
    M5.Display.drawString(label, M5.Display.width() / 2, M5.Display.height() - 24);
}

void stopCurrentMove() {
    // Capture every position before cancelling any axis so the whole group
    // stops at the same point in its interpolation.
    std::vector<float> current_degrees;
    current_degrees.reserve(demo_axes.size());
    for (ServoAxisHandle axis : demo_axes) {
        current_degrees.push_back(servos.currentDegree(axis));
    }
    for (size_t i = 0; i < demo_axes.size(); ++i) {
        servos.moveTo(demo_axes[i], current_degrees[i], 0, false);
    }
}

// Wait for an asynchronous move while continuing to update touch input.
// Returns false after stopping the move when the screen is touched.
bool waitForMove(bool stop_on_touch) {
    while (servos.isMoving()) {
        M5.update();
        if (stop_on_touch && M5.Touch.getCount() > 0) {
            stopCurrentMove();
            return false;
        }
        delay(10);
    }
    return true;
}

enum class DemoPosition { Upper, Lower, Start };

bool moveAll(DemoPosition position, uint32_t duration_ms, bool stop_on_touch) {
    // Start every axis without waiting. Their start times differ only by the
    // few calls needed to enqueue the moves, then the common wait begins.
    for (ServoAxisHandle axis : demo_axes) {
        const ServoAxisConfig& cfg = servos.axisConfig(axis);
        float degree = static_cast<float>(cfg.start_degree);
        if (position == DemoPosition::Upper) {
            degree = static_cast<float>(cfg.upper_limit);
        } else if (position == DemoPosition::Lower) {
            degree = static_cast<float>(cfg.lower_limit);
        }
        servos.moveTo(axis, degree, duration_ms, false);
    }
    return waitForMove(stop_on_touch);
}

// Move every configured axis through upper_limit -> lower_limit ->
// start_degree in parallel, using whatever easing type is selected. During
// an automatic run, a screen touch stops the current move and remaining demos.
bool playCurrentEasing(bool stop_on_touch = false) {
    constexpr uint32_t kSwingMs = 1500;
    return moveAll(DemoPosition::Upper, kSwingMs, stop_on_touch) &&
           moveAll(DemoPosition::Lower, kSwingMs, stop_on_touch) &&
           moveAll(DemoPosition::Start, kSwingMs, stop_on_touch);
}

void setEasingForAll(EasingType type) {
    for (ServoAxisHandle axis : demo_axes) {
        servos.setEasingType(axis, type);
    }
}

void waitForTouchRelease() {
    do {
        M5.update();
        delay(10);
    } while (M5.Touch.getCount() > 0);
}

void playRemainingEasings() {
    // The virtual BtnB itself is a screen touch. Do not arm the stop gesture
    // until the finger that started automatic playback has been released.
    waitForTouchRelease();

    Serial.printf("BtnB held: auto play starting at %s\n",
                  easingTypeName(kAllEasingTypes[easing_index]));
    while (true) {
        EasingType current = kAllEasingTypes[easing_index];
        setEasingForAll(current);
        showEasingName(current, true);
        Serial.printf("Auto playing %s\n", easingTypeName(current));

        if (!playCurrentEasing(true)) {
            Serial.printf("Auto play stopped at %s\n", easingTypeName(current));
            waitForTouchRelease();
            showEasingName(current);
            return;
        }
        if (easing_index + 1 >= kEasingTypeCount) {
            Serial.println("Auto play completed");
            showEasingName(current);
            return;
        }
        ++easing_index;
    }
}

}  // namespace

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    // CoreS3 has no physical buttons -- turn the bottom 40px of the
    // touchscreen into virtual BtnA/BtnB/BtnC zones so M5.BtnA.wasPressed()
    // below still works via touch.
    M5.setTouchButtonHeight(40);
    M5.BtnB.setHoldThresh(2000);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    Serial.begin(115200);
    delay(500);

    // Mount every filesystem a candidate config path lives on; loadConfig()
    // itself tries the paths in order and uses the first one it can open --
    // an SD card takes priority over the SPIFFS image so re-flashing the SD
    // card doesn't require re-uploading SPIFFS too.
    const bool sd_mounted = SD.begin(kSdCsPin);
    const bool spiffs_mounted = SPIFFS.begin(true);
    Serial.printf("SD: %s, SPIFFS: %s\n", sd_mounted ? "mounted" : "not mounted",
                  spiffs_mounted ? "mounted" : "not mounted");
    if (!spiffs_mounted) {
        Serial.println("SPIFFS mount failed");
    }
    if (sd_mounted && SD.exists("/yaml/SCEX_BasicConfig.yaml")) {
        Serial.println("config source: SD /yaml/SCEX_BasicConfig.yaml");
    } else if (spiffs_mounted && SPIFFS.exists("/yaml/SCEX_BasicConfig.yaml")) {
        Serial.println("config source: SPIFFS /yaml/SCEX_BasicConfig.yaml");
    } else {
        Serial.println("config source: built-in defaults (YAML not found)");
    }
    config.loadConfig(
        std::vector<std::string>{"/sd/yaml/SCEX_BasicConfig.yaml", "/spiffs/yaml/SCEX_BasicConfig.yaml"});
    printLoadedParameters(config);
    for (const ServoAxisConfig& axis_cfg : config.servoAxes()) {
        auto driver = createServoDriver(axis_cfg.driver_type);
        if (!driver) {
            Serial.printf("unknown servo driver type: %s\n", axis_cfg.driver_type.c_str());
            continue;
        }
        ServoAxisHandle handle = servos.addAxis(axis_cfg, std::move(driver));
        if (handle == kInvalidServoAxis) {
            Serial.printf("failed to attach servo axis '%s'\n", axis_cfg.name.c_str());
            continue;
        }
        Serial.printf("attached servo axis '%s' (handle=%u)\n", axis_cfg.name.c_str(), handle);
        demo_axes.push_back(handle);
    }
    servos.begin();

    if (demo_axes.empty()) {
        Serial.println("no servo axes could be attached");
        M5.Display.drawString("no servo axes", M5.Display.width() / 2, M5.Display.height() / 2);
        return;
    }

    EasingType initial = kAllEasingTypes[easing_index];
    setEasingForAll(initial);
    showEasingName(initial);
}

void loop() {
    M5.update();
    if (demo_axes.empty()) {
        return;
    }

    if (M5.BtnA.wasPressed()) {
        EasingType current = kAllEasingTypes[easing_index];
        Serial.printf("BtnA pressed: playing %s\n", easingTypeName(current));
        playCurrentEasing();  // blocks loop() for the swing; button input resumes right after
    } else if (M5.BtnB.wasHold()) {
        playRemainingEasings();
    } else if (M5.BtnC.wasPressed()) {
        easing_index = (easing_index + 1) % kEasingTypeCount;
        EasingType current = kAllEasingTypes[easing_index];
        Serial.printf("BtnC pressed: selected %s\n", easingTypeName(current));
        setEasingForAll(current);
        showEasingName(current);
    }
}
