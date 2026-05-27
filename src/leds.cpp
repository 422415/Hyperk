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

    struct ColorRgbcct {
        uint8_t R;
        uint8_t G;
        uint8_t B;
        uint8_t WW;
        uint8_t CW;
    };

    inline ColorRgbw applyOutputMix(uint8_t r, uint8_t g, uint8_t b, uint8_t w, const LedConfig::OutputCorrection& output)
    {
        return {
            clampChannel(scaleGain(r, output.red) + scaleGain(g, output.greenToRed) + scaleGain(b, output.blueToRed)),
            clampChannel(scaleGain(g, output.green) + scaleGain(r, output.redToGreen) + scaleGain(b, output.blueToGreen)),
            clampChannel(scaleGain(b, output.blue) + scaleGain(r, output.redToBlue) + scaleGain(g, output.greenToBlue)),
            scaleGain(w, output.white)
        };
    }

    inline void splitWhiteToCct(uint8_t w, const LedConfig::OutputCorrection& output, uint8_t& ww, uint8_t& cw)
    {
        const uint16_t warmKelvin = constrain(output.cctWarmKelvin, static_cast<uint16_t>(1000), static_cast<uint16_t>(10000));
        const uint16_t coldKelvin = constrain(output.cctColdKelvin, static_cast<uint16_t>(1000), static_cast<uint16_t>(10000));

        if (warmKelvin >= coldKelvin)
        {
            ww = scaleGain(w, output.warmWhite);
            cw = 0;
            return;
        }

        const uint16_t targetKelvin = constrain(output.cctTargetKelvin, warmKelvin, coldKelvin);
        const uint32_t warmWeight = static_cast<uint32_t>(coldKelvin - targetKelvin) * 255 / (coldKelvin - warmKelvin);
        const uint32_t coldWeight = 255 - warmWeight;

        ww = scaleGain(static_cast<uint8_t>((static_cast<uint16_t>(w) * warmWeight + 127) / 255), output.warmWhite);
        cw = scaleGain(static_cast<uint8_t>((static_cast<uint16_t>(w) * coldWeight + 127) / 255), output.coldWhite);
    }

    inline ColorRgbcct applyAnalogOutputMix(uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint8_t ww, uint8_t cw, const LedConfig::OutputCorrection& output)
    {
        ColorRgbw mixed = applyOutputMix(r, g, b, w, output);

        if (output.rgbToWhite && mixed.W == 0 && ww == 0 && cw == 0)
        {
            const uint8_t minRgb = min(mixed.R, min(mixed.G, mixed.B));
            const uint8_t maxRgb = max(mixed.R, max(mixed.G, mixed.B));
            if ((maxRgb - minRgb) <= output.cctNeutralThreshold)
            {
                mixed.R -= minRgb;
                mixed.G -= minRgb;
                mixed.B -= minRgb;
                mixed.W = scaleGain(minRgb, output.white);
            }
        }

        uint8_t splitWw = 0;
        uint8_t splitCw = 0;
        splitWhiteToCct(mixed.W, output, splitWw, splitCw);

        return {
            mixed.R,
            mixed.G,
            mixed.B,
            clampChannel(scaleGain(ww, output.warmWhite) + splitWw),
            clampChannel(scaleGain(cw, output.coldWhite) + splitCw)
        };
    }

    template<bool applyBrightness>
    void setLed(int index, uint8_t r, uint8_t g, uint8_t b)
    {
        if constexpr (applyBrightness) {
            r = scaleBri(r);
            g = scaleBri(g);
            b = scaleBri(b);
        }

        const auto& ledCfg = Config::cfg.led;
        ColorRgbw mixed = applyOutputMix(r, g, b, 0, ledCfg.output);

        if (ledCfg.type == LedType::ANALOG_RGBCCT)
        {
            const ColorRgbcct converted = applyAnalogOutputMix(r, g, b, 0, 0, 0, ledCfg.output);
            renderer.setLedRgbcct(index, converted.R, converted.G, converted.B, converted.WW, converted.CW);
            return;
        }

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
        if constexpr (applyBrightness) {
            r = scaleBri(r);
            g = scaleBri(g);
            b = scaleBri(b);
            w = scaleBri(w);
        }

        const auto& ledCfg = Config::cfg.led;

        if (ledCfg.type == LedType::ANALOG_RGBCCT)
        {
            const ColorRgbcct converted = applyAnalogOutputMix(r, g, b, w, 0, 0, ledCfg.output);
            renderer.setLedRgbcct(index, converted.R, converted.G, converted.B, converted.WW, converted.CW);
            return;
        }

        const ColorRgbw mixed = applyOutputMix(r, g, b, w, ledCfg.output);

        renderer.setLedRgbw(index, mixed.R, mixed.G, mixed.B, mixed.W);
    }

    void testRawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint8_t ww, uint8_t cw)
    {
        tryWaitForRenderer();
        Volatile::setRelay(r || g || b || w || ww || cw);

        for(int i = 0; i < getLedsNumber(); i++) {
            if (Config::cfg.led.type == LedType::ANALOG_RGBCCT)
            {
                if (w > 0 && ww == 0 && cw == 0)
                {
                    splitWhiteToCct(w, Config::cfg.led.output, ww, cw);
                }
                renderer.setLedRgbcct(i, r, g, b, ww, cw);
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
            const ColorRgbcct mixed = applyAnalogOutputMix(r, g, b, w, ww, cw, output);
            Volatile::setRelay(mixed.R || mixed.G || mixed.B || mixed.WW || mixed.CW);
            for(int i = 0; i < getLedsNumber(); i++) {
                renderer.setLedRgbcct(i, mixed.R, mixed.G, mixed.B, mixed.WW, mixed.CW);
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
}
