/* analog_pwm_bridge.h
*
*  MIT License
*
*  Copyright (c) 2026 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/Hyperk
 */

#pragma once

#include <math.h>
#include "driver/ledc.h"
#include "led_bridge.h"

#if defined(CONFIG_IDF_TARGET_ESP32)
    // Direct duty register access for hardware duty dithering (classic ESP32:
    // duty[3:0] is a fractional part the LEDC peripheral dithers at PWM rate).
    #include "soc/ledc_struct.h"
    #define HYPERK_ANALOG_HW_DITHER 1
#endif

#ifndef HYPERK_ANALOG_PWM_FREQ_HZ
    #define HYPERK_ANALOG_PWM_FREQ_HZ 19531
#endif

#ifndef HYPERK_ANALOG_PWM_RESOLUTION_BITS
    #define HYPERK_ANALOG_PWM_RESOLUTION_BITS 12
#endif

#ifndef HYPERK_ANALOG_GPIO_R
    #define HYPERK_ANALOG_GPIO_R 2
#endif

#ifndef HYPERK_ANALOG_GPIO_G
    #define HYPERK_ANALOG_GPIO_G 4
#endif

#ifndef HYPERK_ANALOG_GPIO_B
    #define HYPERK_ANALOG_GPIO_B 12
#endif

#ifndef HYPERK_ANALOG_GPIO_WW
    #define HYPERK_ANALOG_GPIO_WW 32
#endif

#ifndef HYPERK_ANALOG_GPIO_CW
    #define HYPERK_ANALOG_GPIO_CW 33
#endif

struct analog_pwm_bridge : public led_bridge
{
    struct AnalogChannel {
        int gpio;
        ledc_channel_t channel;
    };

    static constexpr ledc_mode_t PWM_MODE = LEDC_HIGH_SPEED_MODE;
    static constexpr ledc_timer_t PWM_TIMER = LEDC_TIMER_0;
    static constexpr uint32_t PWM_MAX_DUTY = (1UL << HYPERK_ANALOG_PWM_RESOLUTION_BITS) - 1;

    const AnalogChannel _channels[5] = {
        { HYPERK_ANALOG_GPIO_R,  LEDC_CHANNEL_0 },
        { HYPERK_ANALOG_GPIO_G,  LEDC_CHANNEL_1 },
        { HYPERK_ANALOG_GPIO_B,  LEDC_CHANNEL_2 },
        { HYPERK_ANALOG_GPIO_WW, LEDC_CHANNEL_3 },
        { HYPERK_ANALOG_GPIO_CW, LEDC_CHANNEL_4 }
    };

    uint16_t _totalLedsNumber = 1;
    bool _initialized = false;
    uint32_t _sumR = 0;
    uint32_t _sumG = 0;
    uint32_t _sumB = 0;
    uint32_t _sumWw = 0;
    uint32_t _sumCw = 0;
    uint16_t _sampleCount = 0;

    bool supportsDoubleBuffering() override
    {
        return false;
    }

    int getLedsNumber() override
    {
        return _totalLedsNumber;
    }

    void clearAll() override
    {
        resetFrame();
        writeChannels(0, 0, 0, 0, 0);
    }

    bool canRender() override
    {
        return true;
    }

    int segmentSupported() override
    {
        return 0;
    }

    void executeRenderLed() override
    {
        if (_sampleCount == 0) {
            return;
        }

        writeChannels(
            averageChannel(_sumR),
            averageChannel(_sumG),
            averageChannel(_sumB),
            averageChannel(_sumWw),
            averageChannel(_sumCw));
        resetFrame();
    }

    void releaseDriverResources() override
    {
        if (!_initialized) {
            return;
        }

        for (const auto& channel : _channels) {
            ledc_stop(PWM_MODE, channel.channel, 0);
        }

        resetFrame();
        _initialized = false;
    }

    void initializeLedDriver(LedType, uint16_t cfgLedNumLeds, const std::vector<LedConfig::Segment>&,
                            uint8_t, uint8_t, uint8_t, uint8_t) override
    {
        releaseDriverResources();

        _totalLedsNumber = std::max<uint16_t>(cfgLedNumLeds, 1);

        ledc_timer_config_t timerConfig = {};
        timerConfig.speed_mode = PWM_MODE;
        timerConfig.duty_resolution = dutyResolution();
        timerConfig.timer_num = PWM_TIMER;
        timerConfig.freq_hz = HYPERK_ANALOG_PWM_FREQ_HZ;
        timerConfig.clk_cfg = LEDC_AUTO_CLK;

        if (ledc_timer_config(&timerConfig) != ESP_OK) {
            Log::debug("Analog PWM: ledc_timer_config failed");
            return;
        }

        for (const auto& channel : _channels) {
            ledc_channel_config_t channelConfig = {};
            channelConfig.gpio_num = channel.gpio;
            channelConfig.speed_mode = PWM_MODE;
            channelConfig.channel = channel.channel;
            channelConfig.intr_type = LEDC_INTR_DISABLE;
            channelConfig.timer_sel = PWM_TIMER;
            channelConfig.duty = 0;
            channelConfig.hpoint = 0;
            channelConfig.flags.output_invert = 0;

            if (ledc_channel_config(&channelConfig) != ESP_OK) {
                Log::debug("Analog PWM: ledc_channel_config failed for GPIO ", channel.gpio);
                releaseDriverResources();
                return;
            }
        }

        _initialized = true;
        Log::debug("Created Analog RGBCCT output at ", HYPERK_ANALOG_PWM_FREQ_HZ, " Hz, ", HYPERK_ANALOG_PWM_RESOLUTION_BITS, "-bit");
    }

    void setLedRgb(int index, uint8_t r, uint8_t g, uint8_t b) override
    {
        setLedRgbcct(index, r, g, b, 0, 0);
    }

    void setLedRgbw(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w) override
    {
        setLedRgbcct(index, r, g, b, 0, w);
    }

    void setLedRgbcct(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw) override
    {
        // 8-bit callers join the 16-bit pipeline at equivalent levels (v * 257).
        setLedRgbcct16(index, r * 257, g * 257, b * 257, ww * 257, cw * 257);
    }

    void setLedRgbcct16(int index, uint16_t r, uint16_t g, uint16_t b, uint16_t ww, uint16_t cw) override
    {
        if (index < 0 || index >= _totalLedsNumber) {
            return;
        }

        _sumR += r;
        _sumG += g;
        _sumB += b;
        _sumWw += ww;
        _sumCw += cw;
        _sampleCount++;
    }

private:
    uint16_t averageChannel(uint32_t sum) const
    {
        if (_sampleCount == 0) {
            return 0;
        }
        return static_cast<uint16_t>((sum + (_sampleCount / 2)) / _sampleCount);
    }

    void resetFrame()
    {
        _sumR = 0;
        _sumG = 0;
        _sumB = 0;
        _sumWw = 0;
        _sumCw = 0;
        _sampleCount = 0;
    }

    static ledc_timer_bit_t dutyResolution()
    {
        switch (HYPERK_ANALOG_PWM_RESOLUTION_BITS) {
            case 10: return LEDC_TIMER_10_BIT;
            case 11: return LEDC_TIMER_11_BIT;
            case 12: return LEDC_TIMER_12_BIT;
            case 13: return LEDC_TIMER_13_BIT;
            case 14: return LEDC_TIMER_14_BIT;
            default: return LEDC_TIMER_12_BIT;
        }
    }

    // 16-bit input -> 12.4 fixed-point duty (12 integer bits for the PWM timer,
    // 4 fractional bits for the LEDC hardware dither). Gamma shapes the linear
    // 16-bit pipeline onto the duty range perceptually; 1.0 = legacy linear.
    static uint32_t dutyFrom16Bit(uint16_t value, float gamma)
    {
        if (value == 0) {
            return 0;
        }

        float n = value / 65535.0f;
        if (gamma > 1.001f || gamma < 0.999f) {
            n = powf(n, gamma);
        }
        return static_cast<uint32_t>(lroundf(n * static_cast<float>(PWM_MAX_DUTY << 4)));
    }

    void setDuty(ledc_channel_t channel, uint16_t value, float gamma, bool dither)
    {
        const uint32_t duty = dutyFrom16Bit(value, gamma);

        #if defined(HYPERK_ANALOG_HW_DITHER)
            if (dither) {
                // Mirror IDF's static duty update (duty_num=1, duty_cycle=1,
                // duty_scale=0, duty_start), but keep the fractional bits the
                // driver API drops: duty[3:0] is hardware-dithered at PWM rate.
                auto& ch = LEDC.channel_group[static_cast<int>(PWM_MODE)].channel[static_cast<int>(channel)];
                ch.duty.duty = duty;
                ch.conf1.val = (1UL << 31) | (1UL << 30) | (1UL << 20) | (1UL << 10);
                return;
            }
        #endif

        ledc_set_duty(PWM_MODE, channel, (duty + 8) >> 4);
        ledc_update_duty(PWM_MODE, channel);
    }

    void writeChannels(uint16_t r, uint16_t g, uint16_t b, uint16_t ww, uint16_t cw)
    {
        if (!_initialized) {
            return;
        }

        const float gamma = Config::cfg.led.output.analogGamma;
        const bool dither = Config::cfg.led.output.analogDither;

        setDuty(_channels[0].channel, r, gamma, dither);
        setDuty(_channels[1].channel, g, gamma, dither);
        setDuty(_channels[2].channel, b, gamma, dither);
        setDuty(_channels[3].channel, ww, gamma, dither);
        setDuty(_channels[4].channel, cw, gamma, dither);
    }
};
