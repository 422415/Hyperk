#pragma once
#include <Arduino.h>
#include "analog_color.h"

namespace AnalogCalibration {
    // 0: mixed RGB, 1: RGB reference white, 2: white pair,
    // 3..7: raw R/G/B/WW/CW wiring tests, 8: low-light neutral ramp.
    void preview(const AnalogColor::Profile& profile, uint8_t pattern, float level,
                 uint16_t r, uint16_t g, uint16_t b);
    void stop();
    bool active();
    void loop();
}
