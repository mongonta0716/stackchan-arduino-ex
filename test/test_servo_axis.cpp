#include "scex_test.h"

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
    explicit FakeDriver(bool timed) : timed_(timed) {}

    bool attach(const ServoAxisConfig&) override { return true; }
    void writeAngle(float degree) override { angle_writes.push_back(degree); }
    bool supportsTimedMove() const override { return timed_; }
    void writeTimedMove(float degree, uint32_t duration_ms) override {
        timed_writes.push_back({degree, duration_ms});
    }

    std::vector<float> angle_writes;
    std::vector<TimedWrite> timed_writes;

private:
    bool timed_;
};

ServoAxisConfig makeConfig() {
    ServoAxisConfig cfg;
    cfg.name = "x";
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
    for (int i = 0; i < 2000 && axis.isMoving(); i++) {
        axis.update(now);
        now += 20;
    }
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

    axis.startMove(100.0f, 1000);  // 10 degree move -> 6 segments of 166ms
    runToCompletion(axis);

    SCEX_ASSERT_FALSE(axis.isMoving());
    SCEX_ASSERT_TRUE(driver.angle_writes.empty());  // no per-tick flooding
    SCEX_ASSERT_EQ_INT(6, driver.timed_writes.size());

    // Every segment carries the same goal time, and the waypoints climb
    // monotonically to exactly the target (ease(1.0) == 1.0).
    float prev = 90.0f;
    for (const auto& w : driver.timed_writes) {
        SCEX_ASSERT_EQ_INT(166, w.duration_ms);
        SCEX_ASSERT_TRUE(w.degree >= prev - 0.001f);
        prev = w.degree;
    }
    SCEX_ASSERT_NEAR(100.0, driver.timed_writes.back().degree, 0.01);
    SCEX_ASSERT_NEAR(100.0, axis.currentDegree(), 0.01);

    // First waypoint is the eased position at t = 1/6, not a linear guess.
    float expected_first = 90.0f + 10.0f * ease(EasingType::QuadInOut, 1.0f / 6.0f);
    SCEX_ASSERT_NEAR(expected_first, driver.timed_writes.front().degree, 0.01);
}

void test_native_timed_move_applies_offset() {
    ServoAxisConfig cfg = makeConfig();
    cfg.offset = 5;
    FakeDriver driver(/*timed=*/true);
    ServoAxis axis(cfg, &driver);
    SCEX_ASSERT_TRUE(axis.attach());

    axis.startMove(100.0f, 1000);
    runToCompletion(axis);

    SCEX_ASSERT_NEAR(105.0, driver.timed_writes.back().degree, 0.01);
}

void test_short_move_collapses_to_single_timed_segment() {
    FakeDriver driver(/*timed=*/true);
    ServoAxis axis(makeConfig(), &driver);
    SCEX_ASSERT_TRUE(axis.attach());
    driver.angle_writes.clear();

    axis.startMove(95.0f, 100);  // below one segment target -> N == 1
    runToCompletion(axis);

    SCEX_ASSERT_EQ_INT(1, driver.timed_writes.size());
    SCEX_ASSERT_EQ_INT(100, driver.timed_writes.front().duration_ms);
    SCEX_ASSERT_NEAR(95.0, driver.timed_writes.front().degree, 0.01);
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
    SCEX_RUN(test_native_timed_move_applies_offset);
    SCEX_RUN(test_short_move_collapses_to_single_timed_segment);
    SCEX_RUN(test_zero_duration_snaps_even_with_timed_driver);
    SCEX_RUN(test_setNativeTimedMove_false_forces_per_tick_path);
    return scex_test::finish();
}
