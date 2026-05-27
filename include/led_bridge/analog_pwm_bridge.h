/* analog_pwm_bridge.h
*
*  MIT License
*
*  Copyright (c) 2026 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/Hyperk
 */

#pragma once

#include "driver/ledc.h"
#include "led_bridge.h"

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
    uint8_t averageChannel(uint32_t sum) const
    {
        if (_sampleCount == 0) {
            return 0;
        }
        return static_cast<uint8_t>((sum + (_sampleCount / 2)) / _sampleCount);
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

    static uint32_t dutyFrom8Bit(uint8_t value)
    {
        return (static_cast<uint32_t>(value) * PWM_MAX_DUTY + 127) / 255;
    }

    void setDuty(ledc_channel_t channel, uint8_t value)
    {
        ledc_set_duty(PWM_MODE, channel, dutyFrom8Bit(value));
        ledc_update_duty(PWM_MODE, channel);
    }

    void writeChannels(uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw)
    {
        if (!_initialized) {
            return;
        }

        setDuty(_channels[0].channel, r);
        setDuty(_channels[1].channel, g);
        setDuty(_channels[2].channel, b);
        setDuty(_channels[3].channel, ww);
        setDuty(_channels[4].channel, cw);
    }
};
