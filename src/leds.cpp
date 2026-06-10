/* leds.cpp
*
*  MIT License
*
*  Copyright (c) 2026 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/Hyperk
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

#include "leds.h"
#include "config.h"
#include "storage.h"
#include "manager.h"
#include "calibration.h"
#include "volatile_state.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(USE_MULTI_ESP32_LED_STRIP) && defined(USE_ANALOG_PWM)
    #include "led_bridge/hybrid_esp32_bridge.h"
    namespace Leds{
        #if defined(CONFIG_IDF_TARGET_ESP32C2)
            hybrid_esp32_bridge<false> renderer;
        #else
            hybrid_esp32_bridge<true> renderer;
        #endif
    }
#elif defined(USE_ANALOG_PWM)
    #include "led_bridge/analog_pwm_bridge.h"
    namespace Leds{ analog_pwm_bridge renderer; }
#elif defined(USE_PICOLADA)
    #include "led_bridge/picolada_bridge.h"
    namespace Leds{ picolada_bridge renderer; }
#elif defined(USE_MULTI_ESP32_LED_STRIP)
    #include "led_bridge/multi_esp32_led_strip_bridge.h"
    namespace Leds{ 
        #if defined(CONFIG_IDF_TARGET_ESP32C2)
            multi_esp32_led_strip_bridge<false> renderer;
        #else
            multi_esp32_led_strip_bridge<true> renderer;
        #endif
    }
#elif defined(USE_FASTLED)
    #include "led_bridge/fastled_bridge.h"
    namespace Leds{ fastled_bridge renderer; }
#elif defined(USE_PARLIO_LED_STRIP)
    #include "led_bridge/parlio_bridge.h"
    namespace Leds{ parlio_bridge<true> renderer; }
#elif defined(USE_ESPRESSIF_LED_STRIP)
    #include "led_bridge/espressif_bridge.h"
    namespace Leds{ espressif_bridge renderer; }
#else
    #include "led_bridge/neopixelbus_bridge.h"
    namespace Leds{ neopixelbus_bridge renderer; }
#endif

namespace Leds{
    bool ledDriverInitialized = false;
    volatile bool delayedRender = false;
    uint16_t briPlus = 256;

    bool restartRequired()
    {
        return renderer.restartRequired();
    }

    int getLedsNumber()
    {
        return renderer.getLedsNumber();
    }

    int segmentSupported()
    {
        return renderer.segmentSupported();
    }

    bool supportsDoubleBuffering()
    {
        return renderer.supportsDoubleBuffering();
    }

    void tryWaitForRenderer()
    {
        const int max_waiting = 80;
        int wait = 0;
        for (wait = 0; !renderer.canRender() && wait < max_waiting; wait++) {
            delay(1);                
        }

        if (wait > 0) {
            Log::debug("Had to wait for LED renderer: ", wait);
        }
    }

    void synchronizeLedsToVolatileStateBeforeDelayedRender()
    {
        if (delayedRender || !renderer.canRender())
            return;

        bool updated = false;
        if (Volatile::clearUpdatedBrightnessState())
        {
            updated = true;
            Log::debug("Updating brightness to: ", Volatile::state.brightness);
            briPlus = Volatile::state.brightness + 1;
        }

        if (Volatile::clearUpdatedPowerOnState())
        {
            updated = true;
            Log::debug("Updating power on to: ", Volatile::state.on);
        }

        if (Volatile::clearUpdatedStaticColorState())
        {
            updated = true;
            Log::debug("Updating static color to: {", Volatile::state.staticColor.red, ", ", Volatile::state.staticColor.green, ", ", Volatile::state.staticColor.blue, "}");
        }

        if (updated)
        {
            tryWaitForRenderer();

            auto r = (Volatile::state.on) ? Volatile::state.staticColor.red : 0;
            auto g = (Volatile::state.on) ? Volatile::state.staticColor.green : 0;
            auto b = (Volatile::state.on) ? Volatile::state.staticColor.blue : 0;

            Volatile::setRelay(r || g || b);
            
            for(int i = 0; i < getLedsNumber(); i++) {
                if (Volatile::state.brightness != 255)
                    setLed<true>(i, r, g, b);
                else
                    setLed<false>(i, r, g, b);
            }

            renderLed(true);
        }
    }

    void initLEDs(LedType cfgLedType, uint16_t cfgLedNumLeds, const std::vector<LedConfig::Segment>& cfgSegments,
                    uint8_t calGain, uint8_t calRed, uint8_t calGreen, uint8_t calBlue) {
        renderer.clearAll();

        if (renderer.restartRequired()){
            if (ledDriverInitialized)
            {
                if (cfgLedType == LedType::SK6812) {
                    setParamsAndPrepareCalibration(calGain, calRed, calGreen, calBlue);
                }
                return;
            }
        }

        renderer.releaseDriverResources();

        if (cfgLedType != LedType::SK6812)
        {
            deleteCalibration();
        }

        delayedRender = false;

        tryWaitForRenderer();

        // LED controller setup
        renderer.initializeLedDriver(cfgLedType, cfgLedNumLeds, cfgSegments, calGain, calRed, calGreen, calBlue);

        renderer.clearAll();

        ledDriverInitialized = true;
    }

    void applyLedConfig()
    {
        const AppConfig& cfg = Config::cfg;
        const bool standbyOn = !cfg.led.standbyOff && (cfg.led.r || cfg.led.g || cfg.led.b);
        const uint8_t standbyR = cfg.led.standbyOff ? 0 : cfg.led.r;
        const uint8_t standbyG = cfg.led.standbyOff ? 0 : cfg.led.g;
        const uint8_t standbyB = cfg.led.standbyOff ? 0 : cfg.led.b;

        Volatile::setRelay(standbyOn);
        initLEDs(cfg.led.type, cfg.led.numLeds, cfg.led.segments, cfg.led.calibration.gain, cfg.led.calibration.red, cfg.led.calibration.green, cfg.led.calibration.blue);
        Volatile::updateBrightness(cfg.led.brightness);
        Volatile::updatePowerOn(standbyOn);
        Volatile::updateStaticColor(standbyR, standbyG, standbyB);
    }

    inline uint8_t scaleGain(uint8_t v, uint8_t gain)
    {
        return (static_cast<uint16_t>(v) * gain + 127) / 255;
    }

    inline uint8_t scaleBri(uint8_t v)
    {
        return (static_cast<uint16_t>(v) * briPlus) >> 8;
    }

    inline uint8_t clampChannel(uint16_t v)
    {
        return static_cast<uint8_t>(v > 255 ? 255 : v);
    }

    // ------- 16-bit pipeline (ANALOG_RGBCCT) -------
    // The analog PWM output has 12(+4 dither) bits of duty resolution, so the
    // whole mix runs at 16 bits per channel; 8-bit callers are widened with
    // v * 257 so full scale maps to full scale.

    inline uint16_t to16(uint8_t v)
    {
        return static_cast<uint16_t>(v) * 257;
    }

    inline uint16_t scaleGain16(uint16_t v, uint8_t gain)
    {
        return (static_cast<uint32_t>(v) * gain + 127) / 255;
    }

    inline uint16_t scaleBri16(uint16_t v)
    {
        return (static_cast<uint32_t>(v) * briPlus) >> 8;
    }

    inline uint16_t clampChannel16(uint32_t v)
    {
        return static_cast<uint16_t>(v > 65535 ? 65535 : v);
    }

    struct ColorRgbw16 {
        uint16_t R;
        uint16_t G;
        uint16_t B;
        uint16_t W;
    };

    struct ColorRgbcct16 {
        uint16_t R;
        uint16_t G;
        uint16_t B;
        uint16_t WW;
        uint16_t CW;
    };

    inline ColorRgbw16 applyOutputMix16(uint16_t r, uint16_t g, uint16_t b, uint16_t w, const LedConfig::OutputCorrection& output)
    {
        return {
            clampChannel16(static_cast<uint32_t>(scaleGain16(r, output.red)) + scaleGain16(g, output.greenToRed) + scaleGain16(b, output.blueToRed)),
            clampChannel16(static_cast<uint32_t>(scaleGain16(g, output.green)) + scaleGain16(r, output.redToGreen) + scaleGain16(b, output.blueToGreen)),
            clampChannel16(static_cast<uint32_t>(scaleGain16(b, output.blue)) + scaleGain16(r, output.redToBlue) + scaleGain16(g, output.greenToBlue)),
            scaleGain16(w, output.white)
        };
    }

    inline void splitWhiteToCct16(uint16_t w, const LedConfig::OutputCorrection& output, uint16_t& ww, uint16_t& cw)
    {
        const uint16_t warmKelvin = constrain(output.cctWarmKelvin, static_cast<uint16_t>(1000), static_cast<uint16_t>(10000));
        const uint16_t coldKelvin = constrain(output.cctColdKelvin, static_cast<uint16_t>(1000), static_cast<uint16_t>(10000));

        if (warmKelvin >= coldKelvin)
        {
            ww = scaleGain16(w, output.warmWhite);
            cw = 0;
            return;
        }

        const uint16_t targetKelvin = constrain(output.cctTargetKelvin, warmKelvin, coldKelvin);
        const uint32_t warmWeight = static_cast<uint32_t>(coldKelvin - targetKelvin) * 255 / (coldKelvin - warmKelvin);
        const uint32_t coldWeight = 255 - warmWeight;

        ww = scaleGain16(static_cast<uint16_t>((static_cast<uint32_t>(w) * warmWeight + 127) / 255), output.warmWhite);
        cw = scaleGain16(static_cast<uint16_t>((static_cast<uint32_t>(w) * coldWeight + 127) / 255), output.coldWhite);
    }

    inline ColorRgbcct16 applyAnalogOutputMix16(uint16_t r, uint16_t g, uint16_t b, uint16_t w, uint16_t ww, uint16_t cw, const LedConfig::OutputCorrection& output)
    {
        ColorRgbw16 mixed = applyOutputMix16(r, g, b, w, output);

        if (output.rgbToWhite && mixed.W == 0 && ww == 0 && cw == 0)
        {
            // Feathered RGB->white extraction: full inside the neutral
            // threshold, fading to none over the feather band above it, so a
            // slowly drifting near-neutral color never JUMPS between the RGB
            // and white channels (the old hard threshold was a visible cliff).
            const uint16_t minRgb = min(mixed.R, min(mixed.G, mixed.B));
            const uint16_t maxRgb = max(mixed.R, max(mixed.G, mixed.B));
            const uint16_t diff = maxRgb - minRgb;
            const uint32_t threshold = to16(output.cctNeutralThreshold);
            const uint32_t feather = to16(output.cctNeutralFeather);

            uint32_t weight = 0;                                  // 0..255
            if (diff <= threshold) {
                weight = 255;
            }
            else if (feather > 0 && diff < threshold + feather) {
                weight = (threshold + feather - diff) * 255 / feather;
            }

            if (weight > 0) {
                const uint16_t extracted = static_cast<uint32_t>(minRgb) * weight / 255;
                mixed.R -= extracted;
                mixed.G -= extracted;
                mixed.B -= extracted;
                mixed.W = scaleGain16(extracted, output.white);
            }
        }

        uint16_t splitWw = 0;
        uint16_t splitCw = 0;
        splitWhiteToCct16(mixed.W, output, splitWw, splitCw);

        return {
            mixed.R,
            mixed.G,
            mixed.B,
            clampChannel16(static_cast<uint32_t>(scaleGain16(ww, output.warmWhite)) + splitWw),
            clampChannel16(static_cast<uint32_t>(scaleGain16(cw, output.coldWhite)) + splitCw)
        };
    }

    template<bool applyBrightness>
    inline void setLedAnalog16(int index, uint16_t r, uint16_t g, uint16_t b, uint16_t w)
    {
        if constexpr (applyBrightness) {
            r = scaleBri16(r);
            g = scaleBri16(g);
            b = scaleBri16(b);
            w = scaleBri16(w);
        }

        const ColorRgbcct16 converted = applyAnalogOutputMix16(r, g, b, w, 0, 0, Config::cfg.led.output);
        renderer.setLedRgbcct16(index, converted.R, converted.G, converted.B, converted.WW, converted.CW);
    }

    inline ColorRgbw applyOutputMix(uint8_t r, uint8_t g, uint8_t b, uint8_t w, const LedConfig::OutputCorrection& output)
    {
        return {
            clampChannel(scaleGain(r, output.red) + scaleGain(g, output.greenToRed) + scaleGain(b, output.blueToRed)),
            clampChannel(scaleGain(g, output.green) + scaleGain(r, output.redToGreen) + scaleGain(b, output.blueToGreen)),
            clampChannel(scaleGain(b, output.blue) + scaleGain(r, output.redToBlue) + scaleGain(g, output.greenToBlue)),
            scaleGain(w, output.white)
        };
    }

    template<bool applyBrightness>
    void setLed(int index, uint8_t r, uint8_t g, uint8_t b)
    {
        const auto& ledCfg = Config::cfg.led;

        if (ledCfg.type == LedType::ANALOG_RGBCCT)
        {
            setLedAnalog16<applyBrightness>(index, to16(r), to16(g), to16(b), 0);
            return;
        }

        if constexpr (applyBrightness) {
            r = scaleBri(r);
            g = scaleBri(g);
            b = scaleBri(b);
        }

        ColorRgbw mixed = applyOutputMix(r, g, b, 0, ledCfg.output);

        if (ledCfg.type == LedType::SK6812)
        {
            if (ledCfg.output.rgbToWhite)
            {
                const ColorRgbw converted = rgb2rgbw(mixed.R, mixed.G, mixed.B);
                renderer.setLedRgbw(index, converted.R, converted.G, converted.B, scaleGain(converted.W, ledCfg.output.white));
            }
            else
            {
                renderer.setLedRgbw(index, mixed.R, mixed.G, mixed.B, 0);
            }
            return;
        }

        renderer.setLedRgb(index, mixed.R, mixed.G, mixed.B);
    }

    template<bool applyBrightness>
    void setLedW(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
    {
        const auto& ledCfg = Config::cfg.led;

        if (ledCfg.type == LedType::ANALOG_RGBCCT)
        {
            setLedAnalog16<applyBrightness>(index, to16(r), to16(g), to16(b), to16(w));
            return;
        }

        if constexpr (applyBrightness) {
            r = scaleBri(r);
            g = scaleBri(g);
            b = scaleBri(b);
            w = scaleBri(w);
        }

        const ColorRgbw mixed = applyOutputMix(r, g, b, w, ledCfg.output);

        renderer.setLedRgbw(index, mixed.R, mixed.G, mixed.B, mixed.W);
    }

    template<bool applyBrightness>
    void setLed16(int index, uint16_t r, uint16_t g, uint16_t b)
    {
        if (Config::cfg.led.type == LedType::ANALOG_RGBCCT)
        {
            setLedAnalog16<applyBrightness>(index, r, g, b, 0);
            return;
        }

        setLed<applyBrightness>(index, r >> 8, g >> 8, b >> 8);
    }

    template<bool applyBrightness>
    void setLedW16(int index, uint16_t r, uint16_t g, uint16_t b, uint16_t w)
    {
        if (Config::cfg.led.type == LedType::ANALOG_RGBCCT)
        {
            setLedAnalog16<applyBrightness>(index, r, g, b, w);
            return;
        }

        setLedW<applyBrightness>(index, r >> 8, g >> 8, b >> 8, w >> 8);
    }

    void testRawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint8_t ww, uint8_t cw)
    {
        tryWaitForRenderer();
        Volatile::setRelay(r || g || b || w || ww || cw);

        uint16_t ww16 = to16(ww);
        uint16_t cw16 = to16(cw);
        if (Config::cfg.led.type == LedType::ANALOG_RGBCCT && w > 0 && ww == 0 && cw == 0)
        {
            splitWhiteToCct16(to16(w), Config::cfg.led.output, ww16, cw16);
        }

        for(int i = 0; i < getLedsNumber(); i++) {
            if (Config::cfg.led.type == LedType::ANALOG_RGBCCT)
            {
                renderer.setLedRgbcct16(i, to16(r), to16(g), to16(b), ww16, cw16);
            }
            else
            {
                renderer.setLedRgbw(i, r, g, b, w);
            }
        }

        renderLed(true);
    }

    void testCorrectedColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w, const LedConfig::OutputCorrection& output, uint8_t ww, uint8_t cw)
    {
        tryWaitForRenderer();

        if (Config::cfg.led.type == LedType::ANALOG_RGBCCT)
        {
            const ColorRgbcct16 mixed = applyAnalogOutputMix16(to16(r), to16(g), to16(b), to16(w), to16(ww), to16(cw), output);
            Volatile::setRelay(mixed.R || mixed.G || mixed.B || mixed.WW || mixed.CW);
            for(int i = 0; i < getLedsNumber(); i++) {
                renderer.setLedRgbcct16(i, mixed.R, mixed.G, mixed.B, mixed.WW, mixed.CW);
            }
            renderLed(true);
            return;
        }

        ColorRgbw mixed = applyOutputMix(r, g, b, w, output);

        if (Config::cfg.led.type == LedType::SK6812 && output.rgbToWhite && mixed.W == 0)
        {
            const ColorRgbw converted = rgb2rgbw(mixed.R, mixed.G, mixed.B);
            mixed.R = converted.R;
            mixed.G = converted.G;
            mixed.B = converted.B;
            mixed.W = scaleGain(converted.W, output.white);
        }

        Volatile::setRelay(mixed.R || mixed.G || mixed.B || mixed.W);

        for(int i = 0; i < getLedsNumber(); i++) {
            if (Config::cfg.led.type == LedType::SK6812)
            {
                renderer.setLedRgbw(i, mixed.R, mixed.G, mixed.B, mixed.W);
            }
            else
            {
                renderer.setLedRgb(i, mixed.R, mixed.G, mixed.B);
            }
        }

        renderLed(true);
    }

    void checkDelayedRender()
    {
        if (delayedRender)
        {
            renderLed(false);
        }
    }

    void queueRender(bool isNewFrame)
    {
        if (isNewFrame && delayedRender)
        {
            stats.skippedFrames = stats.skippedFrames + 1;
        }
        delayedRender = true;
    }

    void renderLed(bool isNewFrame)
    {
        if (!renderer.canRender())
        {
            queueRender(isNewFrame);
        }
        else
        {
            renderer.executeRenderLed();
            delayedRender = false;
            stats.renderedFrames = stats.renderedFrames + 1;
        }
    }

    template void setLed<false>(int, uint8_t, uint8_t, uint8_t);
    template void setLedW<false>(int, uint8_t, uint8_t, uint8_t, uint8_t);
    template void setLed<true>(int, uint8_t, uint8_t, uint8_t);
    template void setLedW<true>(int, uint8_t, uint8_t, uint8_t, uint8_t);
    template void setLed16<false>(int, uint16_t, uint16_t, uint16_t);
    template void setLedW16<false>(int, uint16_t, uint16_t, uint16_t, uint16_t);
    template void setLed16<true>(int, uint16_t, uint16_t, uint16_t);
    template void setLedW16<true>(int, uint16_t, uint16_t, uint16_t, uint16_t);
}
