#include "SCEX_ServoAxis.h"

#include <algorithm>
#include <cmath>

namespace SCEX {

namespace {
// Native firmware-timed moves (Feetech SCS): the segment count is adaptive
// -- ideally one easing-curve waypoint per servo resolution step -- but each
// segment is kept inside [min, max] ms so a fast move does not flood the bus
// and a slow move never leaves the servo without a fresh goal for long.
constexpr uint32_t kTimedMinSegmentMs = 35;
constexpr uint32_t kTimedMaxSegmentMs = 150;
constexpr uint8_t kTimedMaxSegments = 128;
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
    seg_emitted_ = 0;
    seg_duration_ms_ = 0;
    seg_last_quant_ = 0;
    moving_ = true;

    if (duration_ms == 0) {
        current_degree_ = target_degree_;
        driver_->writeAngle(current_degree_ + cfg_.offset);
        moving_ = false;
        return;
    }

    if (native_timed_move_ && driver_->supportsTimedMove()) {
        seg_res_deg_ = driver_->positionResolutionDeg();
        if (!(seg_res_deg_ > 0.0f)) seg_res_deg_ = 0.1f;

        // Aim for ~one servo step per waypoint...
        float span = std::fabs(target_degree_ - start_degree_);
        uint32_t n = static_cast<uint32_t>(std::ceil(span / seg_res_deg_));
        // ...then bound the per-segment time so neither end gets pathological.
        uint32_t n_lo = (duration_ms + kTimedMaxSegmentMs - 1) / kTimedMaxSegmentMs;
        uint32_t n_hi = duration_ms / kTimedMinSegmentMs;
        if (n_lo < 1) n_lo = 1;
        if (n_hi < 1) n_hi = 1;
        if (n < n_lo) n = n_lo;
        if (n > n_hi) n = n_hi;
        if (n > kTimedMaxSegments) n = kTimedMaxSegments;
        if (n < 1) n = 1;

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
        // so it always has a fresh goal + time to interpolate toward. A
        // deadband skips a waypoint the servo could not resolve from the
        // previous one; the next real write then carries a goal time
        // spanning every skipped segment.
        uint32_t want = elapsed / seg_duration_ms_ + 1;
        if (want > seg_count_) want = seg_count_;
        while (seg_sent_ < want) {
            seg_sent_++;
            float seg_t = static_cast<float>(seg_sent_) / static_cast<float>(seg_count_);
            float deg = start_degree_ + (target_degree_ - start_degree_) * ease(easing_, seg_t);
            int32_t q = static_cast<int32_t>(std::lround(deg / seg_res_deg_));
            bool is_final = (seg_sent_ == seg_count_);
            if (seg_emitted_ != 0 && q == seg_last_quant_ && !is_final) {
                continue;
            }
            uint32_t span_ms = static_cast<uint32_t>(seg_sent_ - seg_emitted_) * seg_duration_ms_;
            driver_->writeTimedMove(deg + cfg_.offset, span_ms);
            seg_emitted_ = seg_sent_;
            seg_last_quant_ = q;
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
