#include "SCEX_Easing.h"

#include <cmath>
#include <cstring>

namespace SCEX {

namespace {

constexpr float kPi = 3.14159265358979323846f;

float sineIn(float t) { return 1.0f - std::cos((t * kPi) / 2.0f); }
float sineOut(float t) { return std::sin((t * kPi) / 2.0f); }
float sineInOut(float t) { return -(std::cos(kPi * t) - 1.0f) / 2.0f; }

float quadIn(float t) { return t * t; }
float quadOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
float quadInOut(float t) {
    return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

float cubicIn(float t) { return t * t * t; }
float cubicOut(float t) { return 1.0f - std::pow(1.0f - t, 3.0f); }
float cubicInOut(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

float quartIn(float t) { return t * t * t * t; }
float quartOut(float t) { return 1.0f - std::pow(1.0f - t, 4.0f); }
float quartInOut(float t) {
    return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 4.0f) / 2.0f;
}

float quintIn(float t) { return t * t * t * t * t; }
float quintOut(float t) { return 1.0f - std::pow(1.0f - t, 5.0f); }
float quintInOut(float t) {
    return t < 0.5f ? 16.0f * t * t * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 5.0f) / 2.0f;
}

float expoIn(float t) { return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f); }
float expoOut(float t) { return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t); }
float expoInOut(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    return t < 0.5f ? std::pow(2.0f, 20.0f * t - 10.0f) / 2.0f
                     : (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) / 2.0f;
}

float circIn(float t) { return 1.0f - std::sqrt(1.0f - std::pow(t, 2.0f)); }
float circOut(float t) { return std::sqrt(1.0f - std::pow(t - 1.0f, 2.0f)); }
float circInOut(float t) {
    return t < 0.5f ? (1.0f - std::sqrt(1.0f - std::pow(2.0f * t, 2.0f))) / 2.0f
                     : (std::sqrt(1.0f - std::pow(-2.0f * t + 2.0f, 2.0f)) + 1.0f) / 2.0f;
}

float backIn(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    return c3 * t * t * t - c1 * t * t;
}
float backOut(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}
float backInOut(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c2 = c1 * 1.525f;
    return t < 0.5f
               ? (std::pow(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f
               : (std::pow(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) /
                     2.0f;
}

struct NamedEasingType {
    const char* name;
    EasingType type;
};
constexpr NamedEasingType kNamedEasingTypes[] = {
    {"linear", EasingType::Linear},
    {"sine_in", EasingType::SineIn}, {"sine_out", EasingType::SineOut}, {"sine_in_out", EasingType::SineInOut},
    {"quad_in", EasingType::QuadIn}, {"quad_out", EasingType::QuadOut}, {"quad_in_out", EasingType::QuadInOut},
    {"cubic_in", EasingType::CubicIn}, {"cubic_out", EasingType::CubicOut}, {"cubic_in_out", EasingType::CubicInOut},
    {"quart_in", EasingType::QuartIn}, {"quart_out", EasingType::QuartOut}, {"quart_in_out", EasingType::QuartInOut},
    {"quint_in", EasingType::QuintIn}, {"quint_out", EasingType::QuintOut}, {"quint_in_out", EasingType::QuintInOut},
    {"expo_in", EasingType::ExpoIn}, {"expo_out", EasingType::ExpoOut}, {"expo_in_out", EasingType::ExpoInOut},
    {"circ_in", EasingType::CircIn}, {"circ_out", EasingType::CircOut}, {"circ_in_out", EasingType::CircInOut},
    {"back_in", EasingType::BackIn}, {"back_out", EasingType::BackOut}, {"back_in_out", EasingType::BackInOut},
};
static_assert(sizeof(kNamedEasingTypes) / sizeof(kNamedEasingTypes[0]) == kEasingTypeCount,
              "kNamedEasingTypes must list every EasingType exactly once");

}  // namespace

const EasingType kAllEasingTypes[kEasingTypeCount] = {
    EasingType::Linear,
    EasingType::SineIn, EasingType::SineOut, EasingType::SineInOut,
    EasingType::QuadIn, EasingType::QuadOut, EasingType::QuadInOut,
    EasingType::CubicIn, EasingType::CubicOut, EasingType::CubicInOut,
    EasingType::QuartIn, EasingType::QuartOut, EasingType::QuartInOut,
    EasingType::QuintIn, EasingType::QuintOut, EasingType::QuintInOut,
    EasingType::ExpoIn, EasingType::ExpoOut, EasingType::ExpoInOut,
    EasingType::CircIn, EasingType::CircOut, EasingType::CircInOut,
    EasingType::BackIn, EasingType::BackOut, EasingType::BackInOut,
};

float ease(EasingType type, float t) {
    switch (type) {
        case EasingType::Linear: return t;
        case EasingType::SineIn: return sineIn(t);
        case EasingType::SineOut: return sineOut(t);
        case EasingType::SineInOut: return sineInOut(t);
        case EasingType::QuadIn: return quadIn(t);
        case EasingType::QuadOut: return quadOut(t);
        case EasingType::QuadInOut: return quadInOut(t);
        case EasingType::CubicIn: return cubicIn(t);
        case EasingType::CubicOut: return cubicOut(t);
        case EasingType::CubicInOut: return cubicInOut(t);
        case EasingType::QuartIn: return quartIn(t);
        case EasingType::QuartOut: return quartOut(t);
        case EasingType::QuartInOut: return quartInOut(t);
        case EasingType::QuintIn: return quintIn(t);
        case EasingType::QuintOut: return quintOut(t);
        case EasingType::QuintInOut: return quintInOut(t);
        case EasingType::ExpoIn: return expoIn(t);
        case EasingType::ExpoOut: return expoOut(t);
        case EasingType::ExpoInOut: return expoInOut(t);
        case EasingType::CircIn: return circIn(t);
        case EasingType::CircOut: return circOut(t);
        case EasingType::CircInOut: return circInOut(t);
        case EasingType::BackIn: return backIn(t);
        case EasingType::BackOut: return backOut(t);
        case EasingType::BackInOut: return backInOut(t);
    }
    return t;
}

EasingType easingTypeFromName(const char* name) {
    if (name == nullptr) return kDefaultEasingType;
    for (const auto& entry : kNamedEasingTypes) {
        if (std::strcmp(name, entry.name) == 0) {
            return entry.type;
        }
    }
    return kDefaultEasingType;
}

const char* easingTypeName(EasingType type) {
    for (const auto& entry : kNamedEasingTypes) {
        if (entry.type == type) {
            return entry.name;
        }
    }
    return easingTypeName(kDefaultEasingType);
}

}  // namespace SCEX
