#include "SCEX_ServoAxis.h"

#include <algorithm>

namespace SCEX {

namespace {
// Native firmware-timed moves (Feetech SCS): aim for roughly one easing-curve
// waypoint every this many milliseconds, clamped to [min, max] segments. A few
// coarse segments are enough -- the servo interpolates *within* each one at its
// own internal rate, which is finer and steadier than anything the UART tick
// can feed.
constexpr uint32_t kTimedSegmentTargetMs = 150;
constexpr uint8_t kTimedMinSegments = 1;
constexpr uint8_t kTimedMaxSegments = 16;
}  // namespace

ServoAxis::ServoAxis(ServoAxisConfig cfg, ServoDriver* driver)
    : cfg_(std::move(cfg)),
      driver_(driver),
      easing_(cfg_.easing),
      native_timed_move_(cfg_.native_timed_move) {}

bool ServoAxis::attach() {
    if (!driver_->attach(cfg_)) {
        return false;
    }
    current_degree_ = static_cast<float>(cfg_.start_degree);
    target_degree_ = current_degree_;
    driver_->writeAngle(current_degree_ + cfg_.offset);
    return true;
}

void ServoAxis::startMove(float target_degree, uint32_t duration_ms) {
    target_degree_ = std::clamp(target_degree, static_cast<float>(cfg_.lower_limit),
                                 static_cast<float>(cfg_.upper_limit));
    start_degree_ = current_degree_;
    move_duration_ms_ = duration_ms;
    move_start_ms_ = 0;  // set on the first update() tick (see below)
    timed_mode_ = false;
    seg_count_ = 0;
    seg_sent_ = 0;
    seg_duration_ms_ = 0;
    moving_ = true;

    if (duration_ms == 0) {
        current_degree_ = target_degree_;
        driver_->writeAngle(current_degree_ + cfg_.offset);
        moving_ = false;
        return;
    }

    if (native_timed_move_ && driver_->supportsTimedMove()) {
        uint32_t n = duration_ms / kTimedSegmentTargetMs;
        if (n < kTimedMinSegments) n = kTimedMinSegments;
        if (n > kTimedMaxSegments) n = kTimedMaxSegments;
        seg_count_ = static_cast<uint8_t>(n);
        seg_duration_ms_ = duration_ms / seg_count_;
        if (seg_duration_ms_ == 0) seg_duration_ms_ = 1;
        timed_mode_ = true;
        // Waypoints are emitted from update() so every bus transaction stays
        // on the ServoManager task, same as writeAngle().
    }
}

void ServoAxis::update(uint32_t now_ms) {
    if (!moving_) return;
    if (move_start_ms_ == 0) {
        move_start_ms_ = now_ms;
    }
    uint32_t elapsed = now_ms - move_start_ms_;

    if (timed_mode_) {
        // currentDegree() should still report a smooth estimate along the
        // real easing curve even though the driver only gets waypoints.
        float t = std::clamp(static_cast<float>(elapsed) / static_cast<float>(move_duration_ms_),
                              0.0f, 1.0f);
        current_degree_ = start_degree_ + (target_degree_ - start_degree_) * ease(easing_, t);

        // Hand the servo the next waypoint as the current segment runs out,
        // so it always has a fresh goal + time to interpolate toward.
        uint32_t want = elapsed / seg_duration_ms_ + 1;
        if (want > seg_count_) want = seg_count_;
        while (seg_sent_ < want) {
            seg_sent_++;
            float seg_t = static_cast<float>(seg_sent_) / static_cast<float>(seg_count_);
            float deg = start_degree_ + (target_degree_ - start_degree_) * ease(easing_, seg_t);
            driver_->writeTimedMove(deg + cfg_.offset, seg_duration_ms_);
        }

        if (elapsed >= move_duration_ms_ && seg_sent_ >= seg_count_) {
            current_degree_ = target_degree_;
            moving_ = false;
            timed_mode_ = false;
        }
        return;
    }

    float t = move_duration_ms_ == 0 ? 1.0f
                                      : std::clamp(static_cast<float>(elapsed) /
                                                        static_cast<float>(move_duration_ms_),
                                                    0.0f, 1.0f);
    float eased = ease(easing_, t);
    current_degree_ = start_degree_ + (target_degree_ - start_degree_) * eased;
    driver_->writeAngle(current_degree_ + cfg_.offset);

    if (t >= 1.0f) {
        current_degree_ = target_degree_;
        moving_ = false;
    }
}

}  // namespace SCEX
