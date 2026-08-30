#pragma once

#include <cmath>
#include <cstdint>

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

    // True if the servo interpolates a move to a target angle over a given
    // time in its own firmware (Feetech SCS "goal position + goal time").
    // When true, ServoAxis stops feeding tiny per-tick writeAngle() steps and
    // instead sends a handful of writeTimedMove() waypoints along the easing
    // curve, letting the servo pace its own (coarse) position steps evenly
    // over each segment -- this is what stops a short, slow move from looking
    // stuttery. Default false: ServoAxis uses per-tick interpolation.
    virtual bool supportsTimedMove() const { return false; }

    // Only called when supportsTimedMove() is true. Command a hardware-
    // interpolated move to `degree` (already offset-adjusted) that should
    // complete in `duration_ms` (0 = as fast as the servo can). The default
    // just forwards to writeAngle() so a driver can opt in by overriding
    // supportsTimedMove() alone once it has nothing extra to do here.
    virtual void writeTimedMove(float degree, uint32_t /*duration_ms*/) { writeAngle(degree); }

    // Smallest angle (degrees) the servo can actually resolve -- one unit of
    // its position register. ServoAxis uses this to size a timed move's
    // segment count so each waypoint advances roughly one real step (a
    // shorter/slower move then gets more, finer segments). The 0.1 default is
    // a "fine servo" guess for drivers that don't report a real number.
    virtual float positionResolutionDeg() const { return 0.1f; }

    // Only meaningful for servos that report their own position (currently
    // none in the v1 driver set). Returns NAN when unsupported.
    virtual float readAngle() { return NAN; }

    // Only meaningful for servos with a torque-disable feature (serial
    // servos). No-op by default.
    virtual void setTorque(bool /*on*/) {}
};

}  // namespace SCEX
