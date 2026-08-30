#include "scex_test.h"

#include <climits>
#include <cmath>
#include <cstdint>
#include <vector>

#include "SCEX_Easing.h"
#include "SCEX_ServoAxis.h"
#include "SCEX_ServoDriver.h"
#include "SCEX_ServoTypes.h"

using namespace SCEX;

namespace {

struct TimedWrite {
    float degree;
    uint32_t duration_ms;
};

// Records what ServoAxis hands to the driver so the two move strategies
// (per-tick writeAngle vs. native firmware-timed segments) can be checked
// without an ESP32.
class FakeDriver : public ServoDriver {
public:
    explicit FakeDriver(bool timed, float resolution_deg = 300.0f / 1023.0f)
        : timed_(timed), resolution_deg_(resolution_deg) {}

    bool attach(const ServoAxisConfig&) override { return true; }
    void writeAngle(float degree) override { angle_writes.push_back(degree); }
    bool supportsTimedMove() const override { return timed_; }
    void writeTimedMove(float degree, uint32_t duration_ms) override {
        timed_writes.push_back({degree, duration_ms});
    }
    float positionResolutionDeg() const override { return resolution_deg_; }

    std::vector<float> angle_writes;
    std::vector<TimedWrite> timed_writes;

private:
    bool timed_;
    float resolution_deg_;
};

ServoAxisConfig makeConfig() {
    ServoAxisConfig cfg;
    cfg.name = "y";
    cfg.lower_limit = 0;
    cfg.upper_limit = 180;
    cfg.start_degree = 90;
    cfg.offset = 0;
    cfg.easing = EasingType::QuadInOut;
    cfg.native_timed_move = true;
    return cfg;
}

// Drives update() on a 20ms tick from a non-zero base clock (0 is the
// "not started yet" sentinel inside ServoAxis) until the axis stops or a
// generous tick budget runs out.
void runToCompletion(ServoAxis& axis) {
    uint32_t now = 1000;
    for (int i = 0; i < 4000 && axis.isMoving(); i++) {
        axis.update(now);
        now += 20;
    }
}

uint32_t totalTimedMs(const FakeDriver& d) {
    uint32_t sum = 0;
    for (const auto& w : d.timed_writes) sum += w.duration_ms;
    return sum;
}

// Common invariants for any completed native-timed move from `start` to
// `target` (+`offset`) over `duration_ms`.
void assertTimedMoveWellFormed(const FakeDriver& d, float start, float target, float offset,
                                uint32_t duration_ms) {
    SCEX_ASSERT_TRUE(!d.timed_writes.empty());
    SCEX_ASSERT_TRUE(d.angle_writes.empty());  // no per-tick flooding

    // Waypoints climb (or fall) monotonically toward the target and land
    // exactly on it (ease(1.0) == 1.0, and the final segment is never
    // dropped by the deadband).
    float prev = start + offset;
    bool ascending = target >= start;
    for (const auto& w : d.timed_writes) {
        if (ascending) {
            SCEX_ASSERT_TRUE(w.degree >= prev - 0.001f);
        } else {
            SCEX_ASSERT_TRUE(w.degree <= prev + 0.001f);
        }
        SCEX_ASSERT_TRUE(w.duration_ms > 0);
        prev = w.degree;
    }
    SCEX_ASSERT_NEAR(target + offset, d.timed_writes.back().degree, 0.02);

    // Goal times telescope back to cover the whole move (integer division
    // loses at most one base segment).
    uint32_t total = totalTimedMs(d);
    SCEX_ASSERT_TRUE(total <= duration_ms);
    SCEX_ASSERT_TRUE(total + 150 >= duration_ms);
}

}  // namespace

void test_per_tick_path_when_driver_has_no_timed_move() {
    FakeDriver driver(/*timed=*/false);
    ServoAxis axis(makeConfig(), &driver);
    SCEX_ASSERT_TRUE(axis.attach());
    driver.angle_writes.clear();

    axis.startMove(150.0f, 1000);
    runToCompletion(axis);

    SCEX_ASSERT_FALSE(axis.isMoving());
    SCEX_ASSERT_TRUE(driver.timed_writes.empty());
    SCEX_ASSERT_TRUE(driver.angle_writes.size() > 10);  // one per tick
    SCEX_ASSERT_NEAR(150.0, driver.angle_writes.back(), 0.01);
    SCEX_ASSERT_NEAR(150.0, axis.currentDegree(), 0.01);
}

void test_native_timed_move_sends_easing_waypoints() {
    FakeDriver driver(/*timed=*/true);
    ServoAxis axis(makeConfig(), &driver);
    SCEX_ASSERT_TRUE(axis.attach());
    driver.angle_writes.clear();

    axis.startMove(100.0f, 1000);  // 10 degree move
    runToCompletion(axis);

    SCEX_ASSERT_FALSE(axis.isMoving());
    assertTimedMoveWellFormed(driver, 90.0f, 100.0f, 0.0f, 1000);
    SCEX_ASSERT_NEAR(100.0, axis.currentDegree(), 0.01);
    SCEX_ASSERT_TRUE(driver.timed_writes.front().degree > 90.0f);
    SCEX_ASSERT_TRUE(driver.timed_writes.front().degree < 100.0f);
}

void test_slower_move_gets_more_segments() {
    FakeDriver fast(/*timed=*/true);
    FakeDriver slow(/*timed=*/true);
    ServoAxis fast_axis(makeConfig(), &fast);
    ServoAxis slow_axis(makeConfig(), &slow);
    SCEX_ASSERT_TRUE(fast_axis.attach());
    SCEX_ASSERT_TRUE(slow_axis.attach());
    fast.angle_writes.clear();
    slow.angle_writes.clear();

    fast_axis.startMove(110.0f, 400);   // same 20 degree span...
    slow_axis.startMove(110.0f, 3000);  // ...much longer time
    runToCompletion(fast_axis);
    runToCompletion(slow_axis);

    assertTimedMoveWellFormed(fast, 90.0f, 110.0f, 0.0f, 400);
    assertTimedMoveWellFormed(slow, 90.0f, 110.0f, 0.0f, 3000);
    SCEX_ASSERT_TRUE(slow.timed_writes.size() > fast.timed_writes.size());
}

void test_bigger_span_gets_more_segments() {
    FakeDriver small(/*timed=*/true);
    FakeDriver big(/*timed=*/true);
    ServoAxis small_axis(makeConfig(), &small);
    ServoAxis big_axis(makeConfig(), &big);
    SCEX_ASSERT_TRUE(small_axis.attach());
    SCEX_ASSERT_TRUE(big_axis.attach());
    small.angle_writes.clear();
    big.angle_writes.clear();

    small_axis.startMove(95.0f, 2000);   // 5 degrees
    big_axis.startMove(140.0f, 2000);    // 50 degrees, same duration
    runToCompletion(small_axis);
    runToCompletion(big_axis);

    SCEX_ASSERT_TRUE(big.timed_writes.size() > small.timed_writes.size());
}

void test_tiny_slow_move_spreads_steps_over_whole_duration() {
    FakeDriver driver(/*timed=*/true);  // ~0.293 deg resolution
    ServoAxis axis(makeConfig(), &driver);
    SCEX_ASSERT_TRUE(axis.attach());
    driver.angle_writes.clear();

    axis.startMove(93.0f, 3000);  // only ~10 real servo steps exist
    runToCompletion(axis);

    assertTimedMoveWellFormed(driver, 90.0f, 93.0f, 0.0f, 3000);
    // Roughly span/resolution distinct steps, not one big jump and not
    // hundreds of redundant writes.
    SCEX_ASSERT_TRUE(driver.timed_writes.size() >= 4);
    SCEX_ASSERT_TRUE(driver.timed_writes.size() <= 20);
    // Deadband guarantee: every emitted waypoint but the last advances the
    // servo's own position register by at least one unit.
    const float res = 300.0f / 1023.0f;
    long prev_q = LONG_MIN;
    for (size_t i = 0; i < driver.timed_writes.size(); i++) {
        long q = std::lround(driver.timed_writes[i].degree / res);
        if (i + 1 < driver.timed_writes.size()) {
            SCEX_ASSERT_TRUE(q > prev_q);
        } else {
            SCEX_ASSERT_TRUE(q >= prev_q);
        }
        prev_q = q;
    }
}

void test_native_timed_move_applies_offset() {
    ServoAxisConfig cfg = makeConfig();
    cfg.offset = 5;
    FakeDriver driver(/*timed=*/true);
    ServoAxis axis(cfg, &driver);
    SCEX_ASSERT_TRUE(axis.attach());
    driver.angle_writes.clear();

    axis.startMove(100.0f, 1000);
    runToCompletion(axis);

    SCEX_ASSERT_NEAR(105.0, driver.timed_writes.back().degree, 0.02);
}

void test_very_short_move_uses_few_segments() {
    FakeDriver driver(/*timed=*/true);
    ServoAxis axis(makeConfig(), &driver);
    SCEX_ASSERT_TRUE(axis.attach());
    driver.angle_writes.clear();

    axis.startMove(95.0f, 100);
    runToCompletion(axis);

    SCEX_ASSERT_TRUE(!driver.timed_writes.empty());
    SCEX_ASSERT_TRUE(driver.timed_writes.size() <= 3);
    SCEX_ASSERT_NEAR(95.0, driver.timed_writes.back().degree, 0.05);
    SCEX_ASSERT_TRUE(totalTimedMs(driver) <= 100);
}

void test_zero_duration_snaps_even_with_timed_driver() {
    FakeDriver driver(/*timed=*/true);
    ServoAxis axis(makeConfig(), &driver);
    SCEX_ASSERT_TRUE(axis.attach());
    driver.angle_writes.clear();

    axis.startMove(120.0f, 0);

    SCEX_ASSERT_FALSE(axis.isMoving());
    SCEX_ASSERT_TRUE(driver.timed_writes.empty());
    SCEX_ASSERT_EQ_INT(1, driver.angle_writes.size());
    SCEX_ASSERT_NEAR(120.0, driver.angle_writes.front(), 0.01);
}

void test_setNativeTimedMove_false_forces_per_tick_path() {
    FakeDriver driver(/*timed=*/true);
    ServoAxis axis(makeConfig(), &driver);
    SCEX_ASSERT_TRUE(axis.attach());
    driver.angle_writes.clear();
    axis.setNativeTimedMove(false);

    axis.startMove(100.0f, 1000);
    runToCompletion(axis);

    SCEX_ASSERT_TRUE(driver.timed_writes.empty());
    SCEX_ASSERT_TRUE(driver.angle_writes.size() > 10);
    SCEX_ASSERT_NEAR(100.0, driver.angle_writes.back(), 0.01);
}

int main() {
    SCEX_RUN(test_per_tick_path_when_driver_has_no_timed_move);
    SCEX_RUN(test_native_timed_move_sends_easing_waypoints);
    SCEX_RUN(test_slower_move_gets_more_segments);
    SCEX_RUN(test_bigger_span_gets_more_segments);
    SCEX_RUN(test_tiny_slow_move_spreads_steps_over_whole_duration);
    SCEX_RUN(test_native_timed_move_applies_offset);
    SCEX_RUN(test_very_short_move_uses_few_segments);
    SCEX_RUN(test_zero_duration_snaps_even_with_timed_driver);
    SCEX_RUN(test_setNativeTimedMove_false_forces_per_tick_path);
    return scex_test::finish();
}
