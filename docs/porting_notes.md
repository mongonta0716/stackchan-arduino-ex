# Porting notes: why each stackchan-arduino dependency was replaced (or not)

`stackchan-arduino`'s `library.json` depends on seven libraries. This
library (`stackchan-arduino-ex`) has **zero** third-party dependencies --
everything is implemented directly on ESP-IDF APIs (`driver/ledc.h`,
`driver/uart.h`, `driver/i2c_master.h`, FreeRTOS, `esp_timer.h`,
`esp_log.h`, `<cstdio>` `fopen`) that are available identically whether the
project is built via Arduino/PlatformIO (`framework=arduino`,
`espressif32`) or via a plain `idf.py` build. This note explains what
happened to each original dependency and, for requirement (3), which ones
would be hardest to port if you ever need the pieces this library doesn't
cover.

## Replaced in this library (in scope)

| Library | Why it was Arduino-only | What replaced it |
|---|---|---|
| **ServoEasing** (arminjo) | Built on Arduino `Timer`/ISR APIs and a custom Ticker abstraction. | `SCEX_Easing.h/.cpp` -- a from-scratch implementation of the [easings.net](https://easings.net/) curve family (31 types), driven by a plain FreeRTOS task in `SCEX_ServoManager` instead of a hardware-timer ISR. |
| **ESP32Servo** (madhephaestus) | Thin Arduino wrapper around `ledcWrite`. | `SCEX_ServoDriver_Pwm` calls `driver/ledc.h` directly. |
| **YAMLDuino** (tobozo) | Wraps `libyaml` behind Arduino `Stream`/`File` APIs. | `SCEX_Yaml.h/.cpp` -- a small dependency-free parser for the YAML subset actually needed by a config file (see its header comment for exactly what's supported/unsupported). |
| **ArduinoJson** (bblanchon) | Not actually Arduino-only (portable C++), but no longer needed once config parsing produces `SCEX::YamlValue` trees directly instead of round-tripping through a JSON document. | `SCEX::YamlValue` (part of `SCEX_Yaml.h`). |
| **SCServo** (mongonta0716 fork) | Arduino `Stream`-based wrapper, but the underlying Feetech SCS(CL) protocol is a simple, well-documented byte protocol. | `SCEX_ServoDriver_Feetech` talks the protocol directly over `driver/uart.h` (see that file's header comment for the register-address provenance). |
| **M5Unified** (I2C piece only) | `PY32IOExpander_Class` (used for the `m5_scs`/M5StackChan servo-power rail) took an `m5::I2C_Class*`. | `SCEX_I2CBus` (own thin wrapper over `driver/i2c_master.h`) + `SCEX_IOExpander` (re-implementation of the same register map, see its header comment). |

## Deferred (not in this v1) -- hard to port

- **Dynamixel2Arduino** (robotis-git). This is the dependency most worth
  flagging for requirement (3): its API is built directly on Arduino's
  `HardwareSerial`/`Stream` classes (the constructor takes a
  `HardwareSerial&`), so it can't be dropped into an ESP-IDF-native build
  at all, Arduino or not. The underlying protocol (Dynamixel Protocol 2.0)
  is publicly documented by Robotis, so the fix is the same recipe used
  for `SCServo` above: a from-scratch driver talking the protocol over
  `driver/uart.h`. That's a reasonable follow-up (`DYN_XL330` /
  `RT_DYN_XL330` support), but it's a second protocol implementation's
  worth of work, so it was deliberately left out of this first pass, which
  covers `pwm`, `scs`, and `m5_scs` only.

## Out of scope for this library, but worth knowing about

- **M5Unified / M5GFX** (beyond the I2C piece above). The full library
  assumes the Arduino framework throughout (Arduino `String`, `millis()`,
  etc.), and while M5GFX has some ESP-IDF-native support, it's partial and
  not something this servo/config library needs to take a position on.
  If a future goal is running the *avatar face* (`m5avatar`/`Avatar`,
  which itself depends on M5Unified/M5GFX) on plain ESP-IDF too, that is a
  substantially larger, separate effort from what's covered here.
