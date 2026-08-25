#include "SCEX_ServoAxis.h"

#include <algorithm>

namespace SCEX {

ServoAxis::ServoAxis(ServoAxisConfig cfg, ServoDriver* driver)
    : cfg_(std::move(cfg)), driver_(driver), easing_(cfg_.easing) {}

bool ServoAxis::attach() {
    if (!driver_->attach(cfg_)) {
        return false;
    }
    current_degree_ = static_cast<float>(cfg_.start_degree);
    target_degree_ = current_degree_;
    return true;
}

void ServoAxis::startMove(float target_degree, uint32_t duration_ms) {
    target_degree_ = std::clamp(target_degree, static_cast<float>(cfg_.lower_limit),
                                 static_cast<float>(cfg_.upper_limit));
    start_degree_ = current_degree_;
    move_duration_ms_ = duration_ms;
    move_start_ms_ = 0;  // set on the first update() tick (see below)
    moving_ = true;

    if (duration_ms == 0) {
        current_degree_ = target_degree_;
        driver_->writeAngle(current_degree_ + cfg_.offset);
        moving_ = false;
    }
}

void ServoAxis::update(uint32_t now_ms) {
    if (!moving_) return;
    if (move_start_ms_ == 0) {
        move_start_ms_ = now_ms;
    }
    uint32_t elapsed = now_ms - move_start_ms_;
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
