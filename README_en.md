# stackchan-arduino-ex

English | [日本語](README.md)

An extended Stack-chan servo/config library modeled on
[stackchan-arduino](https://github.com/stack-chan/stackchan-arduino).

- **Zero dependencies**: no ServoEasing / YAMLDuino / ESP32Servo /
  ArduinoJson / SCServo. A from-scratch Easing engine (`SCEX_Easing`) and
  YAML parser (`SCEX_Yaml`) are included, and servo/UART/I2C access goes
  straight through ESP-IDF's `driver/ledc.h` / `driver/uart.h` /
  `driver/i2c_master.h`.
- **Both Arduino and native ESP-IDF**: the same source builds unmodified
  under PlatformIO (`framework=arduino`) and under a plain `idf.py` build
  (arduino-esp32 is itself built on top of ESP-IDF, and its driver headers
  remain directly callable from Arduino sketches).
- **Servo axes are easy to extend**: no more hard-coded `enum {AXIS_X,
  AXIS_Y}` -- axes are defined by `SCEX_BasicConfig.yaml`'s `servo.axes`
  list. Adding an axis is one more yaml entry.
- **easings.net-style easing**: `ServoManager::setEasingType(axis,
  EasingType)` picks one of 31 curves per axis (default: `QuadInOut`).

See [docs/api_en.md](docs/api_en.md) (English) /
[docs/api.md](docs/api.md) (日本語) for the full API, and
[docs/porting_notes.md](docs/porting_notes.md) for the porting rationale.

## Layout

```
src/                   the library
  SCEX_Easing.*         from-scratch easing engine (easings.net-equivalent)
  SCEX_Yaml.*           dependency-free YAML-subset parser
  SCEX_Config.*         SCEX_BasicConfig.yaml loader
  SCEX_ServoAxis.*      one axis's state + easing application
  SCEX_ServoManager.*   variable-length axis registry + background interpolation task
  SCEX_ServoDriver*.*   per-servo-type drivers (pwm / scs / m5_scs)
  SCEX_Motion.*         preset motions
  SCEX_I2CBus.* / SCEX_IOExpander.*   m5_scs servo-power-rail control
data/yaml/                    sample SCEX_BasicConfig.yaml etc.
examples/arduino_basic/       PlatformIO (Arduino) example
examples/arduino_easing_demo/ play with Button A and select patterns with Button C (uses M5Unified)
examples/esp-idf_basic/       plain ESP-IDF (idf.py) example
test/                         host-side unit tests for Easing/Yaml (test/run_native_tests.sh)
```

## Usage (PlatformIO / Arduino)

```ini
; platformio.ini
; SCEX_BasicConfig.yaml's default axes use m5_scs (M5StackChan, CoreS3/
; ESP32-S3). Any ESP32 board works if you switch to driver: pwm instead.
[env:m5stack-cores3]
platform = espressif32
board = m5stack-cores3
framework = arduino
lib_deps = symlink://../path/to/stackchan-arduino-ex
```

```cpp
#include "StackchanServoEx.h"
using namespace SCEX;

SystemConfig config;
ServoManager servos;

void setup() {
    SPIFFS.begin(true);
    config.loadConfig("/spiffs/yaml/SCEX_BasicConfig.yaml");
    for (const auto& axis_cfg : config.servoAxes()) {
        servos.addAxis(axis_cfg, createServoDriver(axis_cfg.driver_type));
    }
    servos.begin();

    ServoAxisHandle x = servos.findAxis("x");
    servos.setEasingType(x, EasingType::BackInOut);  // pick an easing curve
    servos.moveTo(x, 45.0f, 1000);
}
```

See [examples/arduino_basic](examples/arduino_basic) for the full example.

To compare all 31 easing patterns side by side, use
[examples/arduino_easing_demo](examples/arduino_easing_demo). Button C selects
the pattern shown on the display, and Button A plays the currently displayed
pattern. Every configured axis swings between lower_limit and upper_limit,
then returns to start_degree. Hold Button B for two seconds to play every
remaining pattern from the one currently displayed through the last one.

## Usage (plain ESP-IDF)

```
cd examples/esp-idf_basic
idf.py set-target esp32s3
idf.py build flash monitor
```

Verified building against both ESP-IDF v5.5 and v6.0. See
[examples/esp-idf_basic](examples/esp-idf_basic) for details.

## Tests

```
test/run_native_tests.sh
```

Builds and runs the hardware-independent `SCEX_Easing` / `SCEX_Yaml` tests
with the host g++.

## License

This library is released under the [MIT License](LICENSE).

Copyright (c) 2026 Takao Akaki

## References and acknowledgements

This library draws on the design, APIs, and behavior of the following projects
and resources. It neither links nor bundles these libraries as dependencies;
`SCEX_Easing` and `SCEX_Yaml` are original implementations in this project.

- [stackchan-arduino](https://github.com/stack-chan/stackchan-arduino) —
  Stack-chan servo-control and configuration APIs and the configuration-file
  structure (MIT License)
- [ServoEasing](https://github.com/ArminJo/ServoEasing) —
  servo interpolation APIs and easing behavior (GPL-3.0-or-later)
- [YAMLDuino](https://github.com/tobozo/YAMLDuino) —
  the design of loading YAML configuration in an Arduino environment
  (MIT License)
- [SCServo (mongonta0716 fork)](https://github.com/mongonta0716/SCServo) —
  the Feetech SCS(CL) protocol packet format and register layout (MIT License)
- [easings.net](https://easings.net/) —
  easing-curve names, formulas, and behavior

## Key differences from stackchan-arduino

- `SCEX_BasicConfig.yaml`'s `servo` section moved from the hard-coded
  `pin.x`/`pin.y` shape to a `servo.axes` list (see [docs/api_en.md](docs/api_en.md)).
- `StackchanSERVO`/`StackchanSystemConfig` are replaced by
  `SCEX::ServoManager`/`SCEX::SystemConfig`; file loading takes plain path
  strings (`fopen`) instead of `fs::FS&`.
- `DYN_XL330`/`RT_DYN_XL330` (Dynamixel) are not ported in this version --
  see [docs/porting_notes.md](docs/porting_notes.md) for why.
