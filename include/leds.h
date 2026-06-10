// File: include/leds.h
#pragma once
#include <Arduino.h>
#include <vector>
#include "config.h"

namespace Leds {
    bool supportsDoubleBuffering();
    void tryWaitForRenderer();
    void applyLedConfig();
    bool restartRequired();
    int getLedsNumber();
    int segmentSupported();
    void checkDelayedRender();
    void testRawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint8_t ww = 0, uint8_t cw = 0);
    void testCorrectedColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w, const LedConfig::OutputCorrection& output, uint8_t ww = 0, uint8_t cw = 0);
    void renderLed(bool isNewFrame);
    void synchronizeLedsToVolatileStateBeforeDelayedRender();

    template<bool applyBrightness>
    void setLed(int index, uint8_t r, uint8_t g, uint8_t b);

    template<bool applyBrightness>
    void setLedW(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t w);

    // 16-bit per channel (DDP 16-bit input). Full precision on the analog PWM
    // pipeline; downscaled to the top 8 bits for digital strips.
    template<bool applyBrightness>
    void setLed16(int index, uint16_t r, uint16_t g, uint16_t b);

    template<bool applyBrightness>
    void setLedW16(int index, uint16_t r, uint16_t g, uint16_t b, uint16_t w);
};

