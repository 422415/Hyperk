#include "analog_calibration.h"
#include "config.h"
#include <cassert>
#include <iostream>

uint32_t clockMs = 100;
uint32_t millis() { return clockMs; }
namespace Config { TestConfig cfg; }
int restores = 0, frames = 0;
AnalogColor::Channels last{};
namespace Leds {
    void applyLedConfig() { ++restores; }
    void testAnalogLinear(const AnalogColor::Channels& channels) { last = channels; ++frames; }
}
namespace AnalogDemo { void stop() {} }

int main() {
    AnalogColor::Profile p;
    assert(!AnalogCalibration::active());
    AnalogCalibration::preview(p, 3, 0.1f, 65535, 65535, 65535);
    assert(AnalogCalibration::active()); // UDP is held off immediately.
    AnalogCalibration::loop();
    assert(restores == 1 && frames == 1 && last.r == 6554 && last.g == 0 && last.cw == 0);

    p.blue = 0.5f;
    AnalogCalibration::preview(p, 0, 0.2f, 0, 0, 65535);
    AnalogCalibration::loop();
    assert(restores == 1 && last.b == 6554 && last.r == 0);
    AnalogCalibration::stop();
    AnalogCalibration::loop();
    assert(!AnalogCalibration::active() && restores == 2);
    AnalogCalibration::loop();
    assert(restores == 2); // Restore once, not on every idle iteration.

    AnalogCalibration::preview(p, 2, 0.1f, 0, 0, 0);
    AnalogCalibration::loop();
    const int beforeExpiry = restores;
    clockMs += 14999;
    AnalogCalibration::loop();
    assert(AnalogCalibration::active());
    ++clockMs;
    AnalogCalibration::loop();
    assert(!AnalogCalibration::active() && restores == beforeExpiry + 1);

    // Renewing the page's lease keeps a static preview alive without flashes.
    AnalogCalibration::preview(p, 1, 0.1f, 0, 0, 0);
    AnalogCalibration::loop();
    const int beforeRenew = restores;
    clockMs += 10000;
    AnalogCalibration::preview(p, 1, 0.1f, 0, 0, 0);
    clockMs += 10000;
    AnalogCalibration::loop();
    assert(AnalogCalibration::active() && restores == beforeRenew);

    // Switching into the ramp begins at black; heartbeats do not restart it.
    AnalogCalibration::preview(p, 8, 0.03f, 65535, 65535, 65535);
    AnalogCalibration::loop();
    assert(last.r == 0);
    clockMs += 2000;
    AnalogCalibration::preview(p, 8, 0.03f, 65535, 65535, 65535);
    AnalogCalibration::loop();
    assert(last.r == 983 && last.g == 983 && last.b == 492);
    AnalogCalibration::stop();
    AnalogCalibration::loop();

    // ESP32 millis rolls over after ~49 days. The preview must still expire.
    clockMs = UINT32_MAX - 10;
    AnalogCalibration::preview(p, 1, 0.1f, 0, 0, 0);
    AnalogCalibration::loop();
    clockMs += 20;
    AnalogCalibration::loop();
    assert(AnalogCalibration::active());
    clockMs += 15000;
    AnalogCalibration::loop();
    assert(!AnalogCalibration::active());
    std::cout << "Calibration preview: drafts, stop, expiry, renewal, ramp, and clock rollover passed\n";
}
