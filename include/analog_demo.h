#pragma once
#include <Arduino.h>

namespace AnalogDemo {
    void start(uint8_t mode, uint16_t periodMs);
    void stop();
    bool active();
    uint8_t getMode();
    uint16_t getPeriodMs();
    void loop();
}