#include "scex_test.h"

#include "SCEX_Easing.h"

using namespace SCEX;

void test_boundaries_are_0_and_1() {
    EasingType types[] = {
        EasingType::Linear, EasingType::SineInOut, EasingType::QuadInOut, EasingType::CubicInOut,
        EasingType::QuartInOut, EasingType::QuintInOut, EasingType::ExpoInOut, EasingType::CircInOut,
        EasingType::BackInOut,
    };
    for (EasingType t : types) {
        SCEX_ASSERT_NEAR(0.0, ease(t, 0.0f), 0.001);
        SCEX_ASSERT_NEAR(1.0, ease(t, 1.0f), 0.001);
    }
}

void test_quad_in_out_matches_original_formula() {
    // Ported from the original stackchan-arduino quadraticEaseInOut().
    SCEX_ASSERT_NEAR(0.125, ease(EasingType::QuadInOut, 0.25f), 0.0001);
    SCEX_ASSERT_NEAR(0.5, ease(EasingType::QuadInOut, 0.5f), 0.0001);
    SCEX_ASSERT_NEAR(0.875, ease(EasingType::QuadInOut, 0.75f), 0.0001);
}

void test_default_easing_is_quad_in_out() {
    SCEX_ASSERT_TRUE(kDefaultEasingType == EasingType::QuadInOut);
}

void test_easing_type_from_name() {
    SCEX_ASSERT_TRUE(easingTypeFromName("quad_in_out") == EasingType::QuadInOut);
    SCEX_ASSERT_TRUE(easingTypeFromName("back_out") == EasingType::BackOut);
    SCEX_ASSERT_TRUE(easingTypeFromName("bounce_out") == kDefaultEasingType);
    SCEX_ASSERT_TRUE(easingTypeFromName("elastic_in") == kDefaultEasingType);
    SCEX_ASSERT_TRUE(easingTypeFromName("does_not_exist") == kDefaultEasingType);
    SCEX_ASSERT_TRUE(easingTypeFromName(nullptr) == kDefaultEasingType);
}

void test_easing_type_name_round_trips_through_all_types() {
    SCEX_ASSERT_EQ_INT(25, kEasingTypeCount);
    for (int i = 0; i < kEasingTypeCount; i++) {
        EasingType type = kAllEasingTypes[i];
        const char* name = easingTypeName(type);
        SCEX_ASSERT_TRUE(easingTypeFromName(name) == type);
    }
    SCEX_ASSERT_EQ_STR("quad_in_out", easingTypeName(EasingType::QuadInOut));
    SCEX_ASSERT_EQ_STR("back_out", easingTypeName(EasingType::BackOut));
}

int main() {
    SCEX_RUN(test_boundaries_are_0_and_1);
    SCEX_RUN(test_quad_in_out_matches_original_formula);
    SCEX_RUN(test_default_easing_is_quad_in_out);
    SCEX_RUN(test_easing_type_from_name);
    SCEX_RUN(test_easing_type_name_round_trips_through_all_types);
    return scex_test::finish();
}
