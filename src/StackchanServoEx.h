// stackchan-arduino-ex: extended Stack-chan servo/config library.
//
// - No ServoEasing / YAMLDuino / ESP32Servo / ArduinoJson / SCServo /
//   Dynamixel2Arduino dependency (see docs/porting_notes.md for why each
//   was replaced or deferred).
// - Runs unmodified under Arduino (framework=arduino, espressif32) and
//   under a plain `idf.py` ESP-IDF build: the driver layer talks directly
//   to ESP-IDF's driver/ledc.h, driver/uart.h and driver/i2c_master.h.
// - Servo axes are data-driven (SCEX_BasicConfig.yaml's `servo.axes` list)
//   instead of a fixed X/Y pair -- add an axis by adding a yaml entry.
// - Every axis gets the full easings.net curve set; pick one per axis with
//   ServoManager::setEasingType(axis, EasingType) (default: QuadInOut).
#pragma once

#include "SCEX_Config.h"
#include "SCEX_Easing.h"
#include "SCEX_Motion.h"
#include "SCEX_ServoAxis.h"
#include "SCEX_ServoDriver.h"
#include "SCEX_ServoDriverFactory.h"
#include "SCEX_ServoManager.h"
#include "SCEX_ServoTypes.h"
#include "SCEX_Yaml.h"
