#pragma once

#include <memory>
#include <string>
#include <vector>

#include "SCEX_ServoAxis.h"

namespace SCEX {

// Owns a variable-length set of ServoAxis instances and a single background
// FreeRTOS task that ticks every active interpolation (replaces the
// ServoEasing Ticker/ISR mechanism from stackchan-arduino, uniformly for
// every driver type -- PWM and serial axes get the same 25 easing curves).
//
// Axes are looked up by name, not by a fixed X/Y enum, so adding a new axis
// (e.g. "jaw") is just one more addAxis() call / one more entry in
// SCEX_BasicConfig.yaml's servo.axes list.
class ServoManager {
public:
    ~ServoManager();

    // Takes ownership of `driver`. Returns kInvalidServoAxis if cfg.name is
    // already registered or if driver->attach() fails.
    ServoAxisHandle addAxis(const ServoAxisConfig& cfg, std::unique_ptr<ServoDriver> driver);

    ServoAxisHandle findAxis(const std::string& name) const;

    // Starts the background interpolation task. Call once after all axes
    // have been added.
    void begin(uint32_t tick_hz = 50);

    // The API requested in the extended-servo design: pick one of the
    // easings.net curves (SCEX_Easing.h) for a given axis. Defaults to
    // kDefaultEasingType (QuadInOut) if never called.
    void setEasingType(ServoAxisHandle axis, EasingType type);

    // Serial (scs/m5_scs) axes only: enable/disable handing short, slow moves
    // to the servo's own timed interpolation instead of per-tick writes (see
    // ServoAxis::setNativeTimedMove). Defaults on, from
    // ServoAxisConfig::native_timed_move. No effect on PWM axes.
    void setNativeTimedMove(ServoAxisHandle axis, bool on);

    void moveTo(ServoAxisHandle axis, float degree, uint32_t duration_ms = 0,
                bool wait_for_completion = true);
    // Starts two axes moving in parallel and (optionally) waits for both.
    void moveTo(ServoAxisHandle axis_a, float degree_a, ServoAxisHandle axis_b, float degree_b,
                uint32_t duration_ms, bool wait_for_completion = true);

    void setTorque(ServoAxisHandle axis, bool on);
    bool isMoving(ServoAxisHandle axis) const;
    bool isMoving() const;  // true if any axis is moving
    float currentDegree(ServoAxisHandle axis) const;
    const ServoAxisConfig& axisConfig(ServoAxisHandle axis) const { return axes_[axis]->config(); }

    void waitForCompletion(ServoAxisHandle axis) const;
    void waitForCompletion() const;  // waits for every axis

private:
    static void taskEntry(void* arg);
    void taskLoop();

    std::vector<std::unique_ptr<ServoAxis>> axes_;
    std::vector<std::unique_ptr<ServoDriver>> drivers_;  // keeps drivers alive alongside axes_
    void* task_handle_ = nullptr;                        // TaskHandle_t, kept opaque
    uint32_t tick_period_ms_ = 20;
    bool running_ = false;
};

}  // namespace SCEX
