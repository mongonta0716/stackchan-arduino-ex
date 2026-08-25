// Maps SCEX_BasicConfig.yaml's `driver:` string onto a concrete ServoDriver.
// Adding a new servo driver type: implement SCEX_ServoDriver, then add one
// line here -- nothing in ServoAxis/ServoManager/config-parsing changes.
#pragma once

#include <memory>
#include <string>

#include "SCEX_ServoDriver.h"

namespace SCEX {

// Returns nullptr if driver_type is not one of DriverType::kPwm / kScs / kM5Scs.
std::unique_ptr<ServoDriver> createServoDriver(const std::string& driver_type);

}  // namespace SCEX
