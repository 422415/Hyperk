/* hybrid_esp32_bridge.h
*
*  MIT License
*
*  Copyright (c) 2026 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/Hyperk
 */

#pragma once

#include "analog_pwm_bridge.h"
#include "multi_esp32_led_strip_bridge.h"

template<bool DOUBLEBUFFER_SUPPORT>
struct hybrid_esp32_bridge : public led_bridge
{
    multi_esp32_led_strip_bridge<DOUBLEBUFFER_SUPPORT> digital;
    analog_pwm_bridge analog;
    led_bridge* active = nullptr;

    bool restartRequired() override
    {
        return active ? active->restartRequired() : false;
    }

    bool supportsDoubleBuffering() override
    {
        return active ? active->supportsDoubleBuffering() : digital.supportsDoubleBuffering();
    }

    int getLedsNumber() override
    {
        return active ? active->getLedsNumber() : 0;
    }

    int segmentSupported() override
    {
        return active ? active->segmentSupported() : digital.segmentSupported();
    }

    void clearAll() override
    {
        if (active) {
            active->clearAll();
        }
    }

    bool canRender() override
    {
        return active ? active->canRender() : true;
    }

    void executeRenderLed() override
    {
        if (active) {
            active->executeRenderLed();
        }
    }

    void releaseDriverResources() override
    {
        digital.releaseDriverResources();
        analog.releaseDriverResources();
        active = nullptr;
    }

    void initializeLedDriver(LedType cfgLedType, uint16_t cfgLedNumLeds, const std::vector<LedConfig::Segment>& cfgSegments,
                            uint8_t calGain, uint8_t calRed, uint8_t calGreen, uint8_t calBlue) override
    {
        releaseDriverResources();
        active = (cfgLedType == LedType::ANALOG_RGBCCT) ? static_cast<led_bridge*>(&analog)
                                                        : static_cast<led_bridge*>(&digital);
        active->initializeLedDriver(cfgLedType, cfgLedNumLeds, cfgSegments, calGain, calRed, calGreen, calBlue);
    }

    void setLedRgb(int index, uint8_t r, uint8_t g, uint8_t b) override
    {
        if (active) {
            active->setLedRgb(index, r, g, b);
        }
    }

    void setLedRgbw(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w) override
    {
        if (active) {
            active->setLedRgbw(index, r, g, b, w);
        }
    }

    void setLedRgbcct(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t ww, uint8_t cw) override
    {
        if (active) {
            active->setLedRgbcct(index, r, g, b, ww, cw);
        }
    }
};
