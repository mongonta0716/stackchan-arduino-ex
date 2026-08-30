# stackchan-arduino-ex API documentation

## Overview

`stackchan-arduino-ex` is a servo-control / config-loading library modeled on
[stackchan-arduino](https://github.com/stack-chan/stackchan-arduino).

- No dependency on ServoEasing / YAMLDuino / ESP32Servo / ArduinoJson /
  SCServo -- everything is either implemented from scratch (`SCEX_Easing`,
  `SCEX_Yaml`) or calls ESP-IDF's `driver/ledc.h` / `driver/uart.h` /
  `driver/i2c_master.h` directly.
- The same source builds unmodified under Arduino (PlatformIO,
  `framework=arduino`) and under a plain `idf.py` build (see
  `examples/arduino_basic` and `examples/esp-idf_basic`).
- Servo axes are a variable-length list defined by `SCEX_BasicConfig.yaml`'s
  `servo.axes`, not a hard-coded X/Y pair.
- See also [porting_notes.md](porting_notes.md) for which upstream
  dependencies were replaced vs. deliberately deferred.

Every public symbol lives under `namespace SCEX`.

---

## Classes

### 1. `SCEX::SystemConfig` (`SCEX_Config.h`)

Loads `SCEX_BasicConfig.yaml` (the successor to `StackchanSystemConfig`).

```cpp
void loadConfig(const std::string& basic_yaml_path,
                const std::string& secret_yaml_path = "",
                const std::string& extend_yaml_path = "");
```

- All arguments are plain **filesystem paths** (no `fs::FS&` needed). Files
  are read with `fopen()`, so mount SD/SPIFFS/LittleFS yourself first --
  `SD.begin()`/`SPIFFS.begin()` on Arduino, `esp_vfs_spiffs_register()` etc.
  on native ESP-IDF.
- If `basic_yaml_path` can't be read, falls back to a generic two-axis PWM
  default and calls `basicConfigNotFoundCallback()`.
- `secret_yaml_path` / `extend_yaml_path` are skipped entirely when empty.

Key accessors:

| Method | What |
|---|---|
| `servoAxes()` | `std::vector<ServoAxisConfig>` (straight from yaml's `servo.axes`) |
| `servoInterval(mode_name)` | `servo.speed.<mode_name>` (`nullptr` if absent) |
| `bluetooth()` / `wifi()` / `apiKeys()` / `secret()` | assorted settings |
| `lyricsCount()` / `lyric(i)` | speech-balloon lines |
| `autoPowerOffTime()` / `fontLanguage()` / `ledLr()` / `ledPin()` / `useTakaoBase()` | misc settings |

Extension points (same pattern as `StackchanExConfig`):

```cpp
virtual void loadExtendConfig(const std::string& yaml_path);
virtual void setExtendSettings(const YamlValue& doc);
virtual void printExtParameters() const;
virtual void basicConfigNotFoundCallback();
virtual void secretConfigNotFoundCallback();
```

### 2. `SCEX::ServoManager` (`SCEX_ServoManager.h`)

Owns a variable-length set of servo axes and advances easing interpolation
from a background FreeRTOS task (the successor to `StackchanSERVO`).

```cpp
ServoAxisHandle addAxis(const ServoAxisConfig& cfg, std::unique_ptr<ServoDriver> driver);
ServoAxisHandle findAxis(const std::string& name) const;
void begin(uint32_t tick_hz = 50);

void setEasingType(ServoAxisHandle axis, EasingType type);  // the API requested in item (2)
void setNativeTimedMove(ServoAxisHandle axis, bool on);     // serial servos only

void moveTo(ServoAxisHandle axis, float degree, uint32_t duration_ms = 0,
            bool wait_for_completion = true);
void moveTo(ServoAxisHandle axis_a, float degree_a, ServoAxisHandle axis_b, float degree_b,
            uint32_t duration_ms, bool wait_for_completion = true);

void setTorque(ServoAxisHandle axis, bool on);
bool isMoving(ServoAxisHandle axis) const;
bool isMoving() const;  // true if any axis is moving
float currentDegree(ServoAxisHandle axis) const;
```

Adding axes (item 1 -- restructured so ServoAxis is easy to extend):

```cpp
for (const auto& axis_cfg : config.servoAxes()) {
    auto driver = createServoDriver(axis_cfg.driver_type);  // "pwm"/"scs"/"m5_scs"
    servos.addAxis(axis_cfg, std::move(driver));
}
servos.begin();
```

Adding one entry to `SCEX_BasicConfig.yaml`'s `servo.axes` (e.g. a `jaw`
axis) is enough -- no code changes.

### 3. `SCEX::EasingType` / `SCEX::ease()` (`SCEX_Easing.h`)

The 24 supported [easings.net](https://easings.net/) curves plus `Linear` = 25 total.

```
Linear
SineIn / SineOut / SineInOut
QuadIn / QuadOut / QuadInOut        <- default (equivalent to quadraticEaseInOut)
CubicIn / CubicOut / CubicInOut
QuartIn / QuartOut / QuartInOut
QuintIn / QuintOut / QuintInOut
ExpoIn / ExpoOut / ExpoInOut
CircIn / CircOut / CircInOut
BackIn / BackOut / BackInOut
```

Set per-axis with `ServoManager::setEasingType(axis, EasingType::BackInOut)`.
Falls back to `kDefaultEasingType` (`QuadInOut`) when never set.

In yaml, use the snake_case name (`quad_in_out`, `back_out`, ...) under
`easing:`; `easingTypeFromName()` converts it.

#### Native timed move (anti-stutter, Feetech SCS only)

For a small-distance, long-duration move the per-20ms-tick step falls below the
servo's position resolution (~0.29 deg on the SCS0009), so it repeatedly
"sits still, then jumps one step" -- visible stutter.

With `native_timed_move` (on by default), `ServoAxis` splits the move into
easing-curve waypoints and lets the servo interpolate each segment with its
"goal position + goal time" feature. The servo paces its own coarse steps evenly
across each segment, so the motion reads smoothly and the redundant bus writes
disappear.

**The segment count is adaptive:**

- Target is roughly one waypoint per servo resolution step
  (`ServoDriver::positionResolutionDeg()`, ~0.29 deg on the SCS0009), so a
  smaller-distance / longer-duration move gets more, finer segments.
- Each segment is still bounded to 35-150 ms (never flood the bus on a fast
  move, never leave a slow move without a fresh goal).
- Deadband: a waypoint whose quantized position equals the previous one is not
  sent; the next real write's goal time covers the skipped span (write only
  when the servo actually moves).

- Ignored on PWM axes (`ServoDriver::supportsTimedMove()` is `false`).
- yaml: `servo.axes[].native_timed_move: true|false`; runtime:
  `setNativeTimedMove(axis, on)`.
- Set it `false` to fall back to per-tick interpolation if the polygonal
  approximation of the curve matters for your use.

### 4. `SCEX::ServoDriver` (`SCEX_ServoDriver.h`) / `createServoDriver()`

The extension point for adding a new servo type. Implement this interface
and `ServoAxis`/`ServoManager` need no changes at all:

```cpp
class ServoDriver {
    virtual bool attach(const ServoAxisConfig& cfg) = 0;
    virtual void writeAngle(float degree) = 0;
    virtual float readAngle() { return NAN; }
    virtual void setTorque(bool on) {}
    // Servos that interpolate in firmware (Feetech SCS) override these.
    // Default false => ServoAxis calls writeAngle() every tick.
    virtual bool supportsTimedMove() const { return false; }
    virtual void writeTimedMove(float degree, uint32_t duration_ms) { writeAngle(degree); }
    virtual float positionResolutionDeg() const { return 0.1f; }  // sizes the segment count
};
```

v1 ships `pwm` (`driver/ledc.h`) and `scs` / `m5_scs` (Feetech SCS protocol
directly on `driver/uart.h`). `createServoDriver(driver_type)` builds an
instance from the yaml's `driver:` string.

### 5. `SCEX::playMotion()` (`SCEX_Motion.h`)

Ports the original `StackchanSERVO::motion()` presets
(`greet`/`laugh`/`nod`/`refuse`/`test`).

```cpp
void playMotion(ServoManager& manager, MotionPreset preset,
                 const std::string& axis_x_name = "x", const std::string& axis_y_name = "y");
```

### 6. `SCEX::YamlValue` / `SCEX::YamlParser` (`SCEX_Yaml.h`)

The dependency-free YAML-subset parser. See the header comment for exactly
what's supported.

```cpp
YamlValue root;
std::string error;
YamlParser::parse(text, &root, &error);
root["servo"]["axes"][0]["name"].asString();
```

---

## SCEX_BasicConfig.yaml

See `data/yaml/SCEX_BasicConfig.yaml`. Main differences from the original
`SC_BasicConfig.yaml`:

- The hard-coded two-axis `servo.pin.x`/`servo.pin.y`-style structure is
  replaced by a `servo.axes` list (each entry self-contained: `name`,
  `driver`, `pin_tx`, `pin_rx`, `servo_id`, `offset`, `start_degree`,
  `lower_limit`, `upper_limit`, `easing`).
- `servo.baud` selects the shared bus speed for all serial servos and defaults
  to `1000000`.
- `extend_config_filesize`/`secret_config_filesize` are gone -- the custom
  YAML parser reads the whole file, so ArduinoJson's fixed buffer-size
  hint is no longer needed.

---

## Usage

```cpp
#include "StackchanServoEx.h"
using namespace SCEX;

SystemConfig config;
ServoManager servos;

void setup() {
    config.loadConfig("/spiffs/yaml/SCEX_BasicConfig.yaml");
    for (const auto& axis_cfg : config.servoAxes()) {
        servos.addAxis(axis_cfg, createServoDriver(axis_cfg.driver_type));
    }
    servos.begin();

    ServoAxisHandle x = servos.findAxis("x");
    servos.setEasingType(x, EasingType::BackInOut);
    servos.moveTo(x, 45.0f, 1000);
}
```

See `examples/arduino_basic` (PlatformIO) and `examples/esp-idf_basic`
(idf.py) for complete, verified-building examples.
