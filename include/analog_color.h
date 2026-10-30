#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace AnalogColor {
    // The addon sends gamma-2.2 encoded RGB16. Mixing and emitter trims belong
    // in linear light; quantize only the final five PWM channel values.
    struct Profile {
        float red = 1.0f;
        float green = 1.0f;
        float blue = 1.0f;
        float whiteLevel = 1.0f;
        float warmFraction = 0.0f;
        float whiteMix = 0.0f;
    };

    struct Channels {
        uint16_t r, g, b, ww, cw;
    };

    inline bool valid(const Profile& p) {
        const float values[] = {p.red, p.green, p.blue, p.whiteLevel, p.warmFraction, p.whiteMix};
        for (float v : values) {
            if (!std::isfinite(v) || v < 0.0f || v > 1.0f) return false;
        }
        return true;
    }

    inline uint16_t quantize(float linear) {
        return static_cast<uint16_t>(std::lround(std::max(0.0f, std::min(1.0f, linear)) * 65535.0f));
    }

    inline float decode(uint16_t encoded) {
        return std::pow(encoded / 65535.0f, 2.2f);
    }

    inline Channels mixLinear(float r, float g, float b, float w, const Profile& p) {
        // A matched white pair replaces the shared RGB component continuously.
        // Pure primaries never use white. No absolute threshold that changes
        // the color with brightness, and no encoded-domain subtraction.
        const float shared = w > 0.0f ? 0.0f : std::min(r, std::min(g, b)) * p.whiteMix;
        const float white = (w + shared) * p.whiteLevel;
        return {
            quantize((r - shared) * p.red),
            quantize((g - shared) * p.green),
            quantize((b - shared) * p.blue),
            quantize(white * p.warmFraction),
            quantize(white * (1.0f - p.warmFraction))
        };
    }

    inline Channels mix(uint16_t r, uint16_t g, uint16_t b, uint16_t w,
                        const Profile& p, float brightness = 1.0f) {
        return mixLinear(decode(r) * brightness, decode(g) * brightness,
                         decode(b) * brightness, decode(w) * brightness, p);
    }

    inline uint32_t duty12_4(uint16_t linear) {
        return (static_cast<uint32_t>(linear) * (4095U << 4) + 32767U) / 65535U;
    }

    // Kept for existing saved profiles until the user saves the new calibration.
    // Keep the original ratio until the final division. Quantizing the ratio
    // to an 8-bit weight would introduce steps into otherwise 16-bit colors.
    inline uint16_t extractWhite(uint16_t minimum, uint16_t difference,
                                 uint8_t neutralThreshold, uint8_t neutralFeather)
    {
        const uint32_t threshold = static_cast<uint32_t>(neutralThreshold) * 257;
        const uint32_t feather = static_cast<uint32_t>(neutralFeather) * 257;

        if (difference <= threshold) {
            return minimum;
        }
        if (feather == 0 || difference >= threshold + feather) {
            return 0;
        }

        return static_cast<uint16_t>(
            (static_cast<uint32_t>(minimum) * (threshold + feather - difference)
             + feather / 2) / feather);
    }

    // Temperatures have already been clamped by the caller, with warm < cold.
    // Use the complementary remainder for cold white so the split preserves
    // the input value exactly before the independent channel gains.
    inline void splitWhite(uint16_t white, uint16_t warmKelvin, uint16_t coldKelvin,
                           uint16_t targetKelvin, uint16_t& warm, uint16_t& cold)
    {
        const uint32_t span = coldKelvin - warmKelvin;
        warm = static_cast<uint16_t>(
            (static_cast<uint32_t>(white) * (coldKelvin - targetKelvin) + span / 2) / span);
        cold = white - warm;
    }
}
