#pragma once

#include <cmath>

#include "SCEX_ServoTypes.h"

namespace SCEX {

// Strategy interface for one physical servo actuator technology.
// Adding a new ServoAxis "kind" (a new motor/protocol) means writing one
// class that implements this interface -- ServoAxis/ServoManager/the Easing
// engine never need to change.
class ServoDriver {
public:
    virtual ~ServoDriver() = default;

    // Called once when the axis is registered. Should perform whatever bus/
    // peripheral init is required and move the servo to cfg.start_degree.
    virtual bool attach(const ServoAxisConfig& cfg) = 0;

    // Write an absolute angle in degrees (already offset-adjusted by the
    // caller). Called at the interpolation tick rate while an axis is moving.
    virtual void writeAngle(float degree) = 0;

    // Only meaningful for servos that report their own position (currently
    // none in the v1 driver set). Returns NAN when unsupported.
    virtual float readAngle() { return NAN; }

    // Only meaningful for servos with a torque-disable feature (serial
    // servos). No-op by default.
    virtual void setTorque(bool /*on*/) {}
};

}  // namespace SCEX
