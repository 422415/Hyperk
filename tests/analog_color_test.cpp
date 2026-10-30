#include "analog_color.h"

#include <cassert>
#include <iostream>

int main()
{
    // A one-code color change just outside the neutral region must not cause
    // the old 129-code jump from rounding the feather weight down to 254/255.
    constexpr uint16_t threshold = 24 * 257;
    assert(AnalogColor::extractWhite(32768, threshold, 24, 16) == 32768);
    assert(AnalogColor::extractWhite(32768, threshold + 1, 24, 16) == 32760);
    assert(AnalogColor::extractWhite(32768, threshold + 8 * 257, 24, 16) == 16384);
    assert(AnalogColor::extractWhite(32768, 40 * 257, 24, 16) == 0);
    assert(AnalogColor::extractWhite(32768, threshold + 1, 24, 0) == 0);

    // This range used to have only a handful of distinct white values. Each
    // fine input step must now produce the corresponding fine output step.
    uint16_t previous = AnalogColor::extractWhite(32768, threshold, 24, 16);
    for (uint16_t delta = 1; delta <= 64; ++delta) {
        const uint16_t white = AnalogColor::extractWhite(32768, threshold + delta, 24, 16);
        assert(previous > white && previous - white <= 8);
        previous = white;
    }

    // The largest product must fit, and the upper feather boundary can lie
    // beyond 65535 without wrapping back into the visible input range.
    assert(AnalogColor::extractWhite(65535, 1, 0, 255) == 65534);
    assert(AnalogColor::extractWhite(32768, 65535, 254, 255) == 32639);

    uint16_t warm, cold;
    AnalogColor::splitWhite(65535, 2800, 6500, 2800, warm, cold);
    assert(warm == 65535 && cold == 0);
    AnalogColor::splitWhite(65535, 2800, 6500, 6500, warm, cold);
    assert(warm == 0 && cold == 65535);
    AnalogColor::splitWhite(65535, 2800, 6500, 4650, warm, cold);
    assert(warm == 32768 && cold == 32767);

    // A one-kelvin adjustment must survive; the old 8-bit weight discarded it.
    AnalogColor::splitWhite(65535, 2800, 6500, 4651, warm, cold);
    assert(warm == 32750 && cold == 32785);
    AnalogColor::splitWhite(1, 2800, 6500, 4650, warm, cold);
    assert(warm + cold == 1);

    // New engine: wire encoding matches the addon's encode(linear, gamma=2.2).
    AnalogColor::Profile p;
    const uint16_t encodedQuarter = AnalogColor::quantize(std::pow(0.25f, 1.0f / 2.2f));
    const auto quarter = AnalogColor::mix(encodedQuarter, encodedQuarter, encodedQuarter, 0, p);
    assert(std::abs(int(quarter.r) - 16384) <= 1);
    assert(quarter.r == quarter.g && quarter.g == quarter.b && quarter.ww == 0 && quarter.cw == 0);

    // Splitting white in half must preserve linear light, not dim it by gamma.
    p.whiteMix = 1.0f;
    p.warmFraction = 0.5f;
    const auto white = AnalogColor::mix(encodedQuarter, encodedQuarter, encodedQuarter, 0, p);
    assert(white.r == 0 && white.g == 0 && white.b == 0);
    assert(std::abs(int(white.ww + white.cw) - int(quarter.r)) <= 1);

    // The matched neutral contribution stays constant across the entire blend.
    for (int share = 0; share <= 100; ++share) {
        p.whiteMix = share / 100.0f;
        const auto c = AnalogColor::mixLinear(0.2f, 0.3f, 0.5f, 0, p);
        assert(std::abs(int(c.r + c.ww + c.cw) - 13107) <= 2);
        assert(std::abs(int(c.g + c.ww + c.cw) - 19661) <= 2);
        assert(std::abs(int(c.b + c.ww + c.cw) - 32768) <= 2);
    }

    // Hardware trims apply to the residual RGB, not the neutral extraction.
    p.red = 0.5f; p.green = 0.7f; p.blue = 0.2f;
    const auto trimmedWhite = AnalogColor::mixLinear(0.25f, 0.25f, 0.25f, 0, p);
    assert(trimmedWhite.r == 0 && trimmedWhite.g == 0 && trimmedWhite.b == 0);
    assert(std::abs(int(trimmedWhite.ww + trimmedWhite.cw) - 16384) <= 1);
    const auto red = AnalogColor::mix(65535, 0, 0, 0, p, 0.2f);
    assert(std::abs(int(red.r) - 6554) <= 1 && red.g == 0 && red.b == 0 && red.ww == 0 && red.cw == 0);
    const auto black = AnalogColor::mix(0, 0, 0, 0, p);
    assert(black.r == 0 && black.g == 0 && black.b == 0 && black.ww == 0 && black.cw == 0);

    // Preserve precision beyond an 8-bit signal and a 12-bit integer duty.
    uint32_t lastDuty = AnalogColor::duty12_4(32768);
    int distinct = 0;
    for (uint16_t v = 32769; v < 32833; ++v) {
        const uint32_t duty = AnalogColor::duty12_4(v);
        assert(duty >= lastDuty && duty - lastDuty <= 1);
        if (duty > lastDuty) ++distinct;
        lastDuty = duty;
    }
    assert(distinct >= 63);
    assert(AnalogColor::duty12_4(0) == 0 && AnalogColor::duty12_4(65535) == 65520);
    assert(AnalogColor::valid(p));
    p.whiteMix = std::nanf("");
    assert(!AnalogColor::valid(p));

    std::cout << "Analog color: legacy precision, gamma decoding, linear mixing, trims, and PWM checks passed\n";
}
