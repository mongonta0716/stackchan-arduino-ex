#pragma once

#include <cstdint>

#include "SCEX_Easing.h"
#include "SCEX_ServoDriver.h"
#include "SCEX_ServoTypes.h"

namespace SCEX {

// Opaque handle returned by ServoManager::addAxis()/findAxis(). Just an
// index -- axes themselves are looked up by name (see ServoAxisConfig::name)
// so callers never need to know the handle's numeric value.
using ServoAxisHandle = uint8_t;
constexpr ServoAxisHandle kInvalidServoAxis = 0xFF;

// Runtime state + easing-driven motion for a single servo axis. Adding a new
// axis to a robot means constructing one more ServoAxis (see
// ServoManager::addAxis) -- this class has no notion of "how many axes
// exist" or "which index is X/Y".
class ServoAxis {
public:
    ServoAxis(ServoAxisConfig cfg, ServoDriver* driver);

    bool attach();

    // Begins interpolating from the current angle to target_degree (clamped
    // to [lower_limit, upper_limit]) over duration_ms. duration_ms == 0
    // snaps to the target on the next update() tick.
    void startMove(float target_degree, uint32_t duration_ms);

    // Advances the interpolation given the current tick time (milliseconds,
    // any monotonic clock). Called by ServoManager's background task.
    void update(uint32_t now_ms);

    bool isMoving() const { return moving_; }
    float currentDegree() const { return current_degree_; }

    void setEasingType(EasingType type) { easing_ = type; }
    EasingType easingType() const { return easing_; }

    // When the driver supports firmware-side timed moves (Feetech SCS), a
    // move is handed to the servo as a few easing-curve waypoints, each with
    // its own goal time, instead of many sub-resolution per-tick writes. This
    // stops a short, slow move from stuttering. No effect on PWM axes.
    // Defaults to ServoAxisConfig::native_timed_move (true).
    void setNativeTimedMove(bool on) { native_timed_move_ = on; }
    bool nativeTimedMove() const { return native_timed_move_; }

    void setTorque(bool on) { driver_->setTorque(on); }

    const std::string& name() const { return cfg_.name; }
    const ServoAxisConfig& config() const { return cfg_; }

private:
    ServoAxisConfig cfg_;
    ServoDriver* driver_;
    EasingType easing_;
    bool native_timed_move_;

    float start_degree_ = 0.0f;
    float target_degree_ = 0.0f;
    float current_degree_ = 0.0f;
    uint32_t move_start_ms_ = 0;
    uint32_t move_duration_ms_ = 0;
    bool moving_ = false;

    // Set while the current move is running as native firmware-timed
    // segments (see setNativeTimedMove). seg_sent_ counts the waypoints
    // already handed to the driver; each covers seg_duration_ms_.
    bool timed_mode_ = false;
    uint8_t seg_count_ = 0;
    uint8_t seg_sent_ = 0;
    uint32_t seg_duration_ms_ = 0;
};

}  // namespace SCEX
