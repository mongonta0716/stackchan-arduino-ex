#include "SCEX_ServoDriverFactory.h"

// The concrete drivers this factory instantiates only build on an actual
// ESP32 target; guarded out on host builds (no ESP_PLATFORM) -- see
// drivers/SCEX_ServoDriver_Pwm.h for the rationale. createServoDriver()
// itself stays declared (see the header) so portable code can still be
// written against it; it is simply unavailable to link on host builds,
// which never call it (SCEX_Easing/SCEX_Yaml are the only pieces of this
// library exercised there).
#ifdef ESP_PLATFORM

#include "drivers/SCEX_ServoDriver_Feetech.h"
#include "drivers/SCEX_ServoDriver_Pwm.h"

namespace SCEX {

std::unique_ptr<ServoDriver> createServoDriver(const std::string& driver_type) {
    if (driver_type == DriverType::kPwm) {
        return std::make_unique<PwmServoDriver>();
    }
    if (driver_type == DriverType::kScs || driver_type == DriverType::kM5Scs) {
        return std::make_unique<FeetechServoDriver>();
    }
    return nullptr;
}

}  // namespace SCEX

#endif  // ESP_PLATFORM
