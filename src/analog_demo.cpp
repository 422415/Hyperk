#include "analog_demo.h"
#include "config.h"
#include "leds.h"

namespace
{
    bool _active = false;
    uint8_t _mode = 1;
    uint16_t _periodMs = 3000;
    uint32_t _startedAt = 0;
    uint32_t _lastRenderAt = 0;

    constexpr uint16_t MIN_PERIOD_MS = 500;
    constexpr uint16_t MAX_PERIOD_MS = 12000;
    constexpr uint16_t UPDATE_INTERVAL_MS = 8;

    uint8_t scaleToByte(uint32_t value, uint32_t maxValue)
    {
        if (maxValue == 0) {
            return 0;
        }

        const uint32_t scaled = (value * 255 + maxValue / 2) / maxValue;
        return static_cast<uint8_t>(scaled > 255 ? 255 : scaled);
    }

    uint8_t smoothStep8(uint8_t x)
    {
        const uint32_t t = x;
        return static_cast<uint8_t>((t * t * (765 - 2 * t) + 32512) / 65025);
    }

    uint8_t easedTriangle(uint32_t elapsedMs)
    {
        const uint16_t period = _periodMs == 0 ? MIN_PERIOD_MS : _periodMs;
        const uint32_t phase = elapsedMs % period;
        const uint32_t half = period / 2;
        const uint32_t position = phase < half ? phase : period - phase;
        return smoothStep8(scaleToByte(position, half));
    }

    void renderWhiteBreath(uint32_t elapsedMs)
    {
        Leds::testRawColor(0, 0, 0, 0, 0, easedTriangle(elapsedMs));
    }

    void renderLowEndCrawl(uint32_t elapsedMs)
    {
        const uint8_t level = static_cast<uint8_t>((static_cast<uint16_t>(easedTriangle(elapsedMs)) * 64 + 127) / 255);
        Leds::testRawColor(0, 0, 0, 0, 0, level);
    }

    void renderRgbCrossfade(uint32_t elapsedMs)
    {
        const uint16_t period = _periodMs == 0 ? MIN_PERIOD_MS : _periodMs;
        const uint32_t colorPhase = (elapsedMs % period) * 3;
        const uint8_t leg = static_cast<uint8_t>(colorPhase / period);
        const uint8_t blend = smoothStep8(scaleToByte(colorPhase % period, period));
        const uint8_t inverse = 255 - blend;

        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;

        switch (leg)
        {
            case 0:
                r = inverse;
                g = blend;
                break;
            case 1:
                g = inverse;
                b = blend;
                break;
            default:
                b = inverse;
                r = blend;
                break;
        }

        Leds::testRawColor(r, g, b, 0, 0, 0);
    }

    void renderStepCompare(uint32_t elapsedMs)
    {
        static constexpr uint8_t levels[] = {0, 1, 2, 4, 8, 16, 32, 64, 128, 255};
        uint16_t stepMs = _periodMs / (sizeof(levels) / sizeof(levels[0]));
        if (stepMs < 100) {
            stepMs = 100;
        }

        const uint8_t index = static_cast<uint8_t>((elapsedMs / stepMs) % (sizeof(levels) / sizeof(levels[0])));
        Leds::testRawColor(0, 0, 0, 0, 0, levels[index]);
    }
}

namespace AnalogDemo
{
    void start(uint8_t mode, uint16_t periodMs)
    {
        if (Config::cfg.led.type != LedType::ANALOG_RGBCCT) {
            return;
        }

        _mode = static_cast<uint8_t>(constrain(static_cast<int>(mode), 1, 4));
        _periodMs = static_cast<uint16_t>(constrain(static_cast<int>(periodMs), MIN_PERIOD_MS, MAX_PERIOD_MS));
        _startedAt = millis();
        _lastRenderAt = 0;
        _active = true;
    }

    void stop()
    {
        if (!_active) {
            return;
        }

        _active = false;
        Leds::applyLedConfig();
    }

    bool active()
    {
        return _active;
    }

    uint8_t getMode()
    {
        return _mode;
    }

    uint16_t getPeriodMs()
    {
        return _periodMs;
    }

    void loop()
    {
        if (!_active) {
            return;
        }

        if (Config::cfg.led.type != LedType::ANALOG_RGBCCT) {
            _active = false;
            return;
        }

        const uint32_t now = millis();
        if (_lastRenderAt != 0 && now - _lastRenderAt < UPDATE_INTERVAL_MS) {
            return;
        }
        _lastRenderAt = now;

        const uint32_t elapsed = now - _startedAt;
        switch (_mode)
        {
            case 1:
                renderWhiteBreath(elapsed);
                break;
            case 2:
                renderLowEndCrawl(elapsed);
                break;
            case 3:
                renderRgbCrossfade(elapsed);
                break;
            case 4:
                renderStepCompare(elapsed);
                break;
            default:
                renderWhiteBreath(elapsed);
                break;
        }
    }
}