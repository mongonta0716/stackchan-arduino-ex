// Preset motions, ported 1:1 from stackchan-arduino's StackchanSERVO::motion().
#pragma once

#include <string>

#include "SCEX_ServoManager.h"

namespace SCEX {

enum class MotionPreset {
    NoMove,
    Greet,   // wave / nod hello
    Laugh,
    Nod,
    Refuse,  // shake head "no"
    Test,
};

// Plays a built-in preset on the given pan/tilt axis pair (looked up by
// name; defaults to "x"/"y", matching the two-axis SCEX_BasicConfig.yaml
// default). No-op if either axis name is not registered on `manager`.
void playMotion(ServoManager& manager, MotionPreset preset, const std::string& axis_x_name = "x",
                 const std::string& axis_y_name = "y");

}  // namespace SCEX
