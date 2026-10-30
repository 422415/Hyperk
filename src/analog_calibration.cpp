#include "analog_calibration.h"
#include "analog_demo.h"
#include "config.h"
#include "leds.h"
#include <atomic>
#include <mutex>

namespace {
    struct Preview {
        AnalogColor::Profile profile;
        uint8_t pattern = 0;
        float level = 0.1f;
        uint16_t r = 65535, g = 65535, b = 65535;
        uint32_t receivedAt = 0;
        uint32_t revision = 0;
    } pending;
    std::mutex previewMutex;
    std::atomic<bool> previewActive{false};
    bool rendering = false;
    uint32_t renderedRevision = 0;
    uint32_t lastRampFrame = 0;
    uint32_t rampStarted = 0;
    uint8_t renderedPattern = 255;
}

namespace AnalogCalibration {
    void preview(const AnalogColor::Profile& profile, uint8_t pattern, float level,
                 uint16_t r, uint16_t g, uint16_t b) {
        const std::lock_guard<std::mutex> lock(previewMutex);
        pending.profile = profile;
        pending.pattern = pattern;
        pending.level = level;
        pending.r = r; pending.g = g; pending.b = b;
        pending.receivedAt = millis();
        ++pending.revision;
        previewActive = true;
    }

    void stop() {
        const std::lock_guard<std::mutex> lock(previewMutex);
        previewActive = false;
    }

    bool active() { return previewActive.load(); }

    void loop() {
        Preview current;
        bool isActive;
        uint32_t now;
        {
            const std::lock_guard<std::mutex> lock(previewMutex);
            // Sample the clock after locking: a newer request timestamp must
            // not look like an expired lease through unsigned subtraction.
            now = millis();
            if (previewActive && (now - pending.receivedAt >= 15000U ||
                                 Config::cfg.led.type != LedType::ANALOG_RGBCCT)) {
                previewActive = false;
            }
            isActive = previewActive;
            current = pending;
        }
        if (!isActive) {
            if (rendering) {
                rendering = false;
                Leds::applyLedConfig();
            }
            return;
        }
        if (!rendering) {
            AnalogDemo::stop();
            Leds::applyLedConfig();
            rendering = true;
            rampStarted = now;
            renderedRevision = 0;
            renderedPattern = 255;
        }
        if (current.pattern != renderedPattern) {
            rampStarted = now;
            renderedPattern = current.pattern;
        }
        const bool ramp = current.pattern == 8;
        if (current.revision == renderedRevision && (!ramp || now - lastRampFrame < 8U)) return;
        renderedRevision = current.revision;
        lastRampFrame = now;
        auto p = current.profile;
        const float level = current.level;
        AnalogColor::Channels channels{};
        switch (current.pattern) {
            case 1: // RGB-only neutral reference using the saved draft trims.
                p.whiteMix = 0.0f;
                channels = AnalogColor::mixLinear(level, level, level, 0, p);
                break;
            case 2:
                channels = AnalogColor::mixLinear(0, 0, 0, level, p);
                break;
            case 3: channels.r = AnalogColor::quantize(level); break;
            case 4: channels.g = AnalogColor::quantize(level); break;
            case 5: channels.b = AnalogColor::quantize(level); break;
            case 6: channels.ww = AnalogColor::quantize(level); break;
            case 7: channels.cw = AnalogColor::quantize(level); break;
            case 8: {
                // 8-second smooth ramp from black to the selected linear level.
                const float phase = ((now - rampStarted) % 8000U) / 8000.0f;
                const float triangle = phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;
                const float v = level * triangle * triangle * (3.0f - 2.0f * triangle);
                channels = AnalogColor::mixLinear(v, v, v, 0, p);
                break;
            }
            default:
                channels = AnalogColor::mix(current.r, current.g, current.b, 0, p, level);
                break;
        }
        Leds::testAnalogLinear(channels);
    }
}
