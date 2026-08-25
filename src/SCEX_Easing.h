// Own Easing engine (no ServoEasing dependency).
// Implements the standard easing family listed on https://easings.net/ .
#pragma once

namespace SCEX {

enum class EasingType {
    Linear,
    SineIn, SineOut, SineInOut,
    QuadIn, QuadOut, QuadInOut,
    CubicIn, CubicOut, CubicInOut,
    QuartIn, QuartOut, QuartInOut,
    QuintIn, QuintOut, QuintInOut,
    ExpoIn, ExpoOut, ExpoInOut,
    CircIn, CircOut, CircInOut,
    BackIn, BackOut, BackInOut,
    ElasticIn, ElasticOut, ElasticInOut,
    BounceIn, BounceOut, BounceInOut,
};

// Default easing type used when a ServoAxis does not specify one.
constexpr EasingType kDefaultEasingType = EasingType::QuadInOut;

// Total number of EasingType values (Linear + the 30 easings.net curves).
constexpr int kEasingTypeCount = 31;

// All EasingType values in declaration order -- e.g. to cycle through them
// one at a time in a demo. Defined in SCEX_Easing.cpp.
extern const EasingType kAllEasingTypes[kEasingTypeCount];

// t must be in [0, 1]. Returns the eased progress; Back/Elastic can
// momentarily leave [0, 1] (overshoot), which is expected.
float ease(EasingType type, float t);

// Parses one of the SCEX_BasicConfig.yaml easing names (e.g. "quad_in_out")
// into an EasingType. Returns kDefaultEasingType if the name is unknown.
EasingType easingTypeFromName(const char* name);

// The inverse of easingTypeFromName(): the yaml-style snake_case name for
// an EasingType (e.g. EasingType::QuadInOut -> "quad_in_out").
const char* easingTypeName(EasingType type);

}  // namespace SCEX
