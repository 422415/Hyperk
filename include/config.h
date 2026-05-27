// File: include/config.h
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "logger.h"

#define CONFIG_FILE "/config.json"

#ifndef POWER_RELAY_GPIO
    #define POWER_RELAY_GPIO -1 
#endif

#ifdef POWER_RELAY_INVERT
    #define POWER_RELAY_INVERT_BOOL true
#else
    #define POWER_RELAY_INVERT_BOOL false
#endif

#ifndef HYPERK_DEFAULT_LED_TYPE
    #define HYPERK_DEFAULT_LED_TYPE 0
#endif

#ifndef HYPERK_DEFAULT_RGBW_ORDER
    #define HYPERK_DEFAULT_RGBW_ORDER 0
#endif

#ifndef HYPERK_DEFAULT_LED_DATA_GPIO
    #define HYPERK_DEFAULT_LED_DATA_GPIO 2
#endif

#ifndef HYPERK_DEFAULT_LED_CLOCK_GPIO
    #define HYPERK_DEFAULT_LED_CLOCK_GPIO 4
#endif

#ifndef HYPERK_DEFAULT_LED_COUNT
    #define HYPERK_DEFAULT_LED_COUNT 16
#endif

#ifndef HYPERK_DEFAULT_OUTPUT_RED_GAIN
    #define HYPERK_DEFAULT_OUTPUT_RED_GAIN 255
#endif

#ifndef HYPERK_DEFAULT_OUTPUT_GREEN_GAIN
    #define HYPERK_DEFAULT_OUTPUT_GREEN_GAIN 255
#endif

#ifndef HYPERK_DEFAULT_OUTPUT_BLUE_GAIN
    #define HYPERK_DEFAULT_OUTPUT_BLUE_GAIN 255
#endif

#ifndef HYPERK_DEFAULT_OUTPUT_WHITE_GAIN
    #define HYPERK_DEFAULT_OUTPUT_WHITE_GAIN 255
#endif

#ifndef HYPERK_DEFAULT_OUTPUT_WARM_WHITE_GAIN
    #define HYPERK_DEFAULT_OUTPUT_WARM_WHITE_GAIN 0
#endif

#ifndef HYPERK_DEFAULT_OUTPUT_COLD_WHITE_GAIN
    #define HYPERK_DEFAULT_OUTPUT_COLD_WHITE_GAIN 255
#endif

#ifndef HYPERK_DEFAULT_RGB_TO_WHITE_CONVERSION
    #define HYPERK_DEFAULT_RGB_TO_WHITE_CONVERSION 1
#endif

#ifndef HYPERK_DEFAULT_CCT_WARM_KELVIN
    #define HYPERK_DEFAULT_CCT_WARM_KELVIN 3000
#endif

#ifndef HYPERK_DEFAULT_CCT_COLD_KELVIN
    #define HYPERK_DEFAULT_CCT_COLD_KELVIN 6500
#endif

#ifndef HYPERK_DEFAULT_CCT_TARGET_KELVIN
    #define HYPERK_DEFAULT_CCT_TARGET_KELVIN 6500
#endif

#ifndef HYPERK_DEFAULT_CCT_NEUTRAL_THRESHOLD
    #define HYPERK_DEFAULT_CCT_NEUTRAL_THRESHOLD 24
#endif

enum class LedType : uint8_t {
    WS2812 = 0,
    SK6812 = 1,
    APA102 = 2,
    ANALOG_RGBCCT = 3
};

struct LedConfig {
    enum class RgbwOrder : uint8_t {
        GRBW = 0,
        WGRB = 1,
        RGBW = 2,
        RBGW = 3,
        GBRW = 4,
        BRGW = 5,
        BGRW = 6,
        RGWB = 7,
        RBWG = 8,
        GRWB = 9,
        GBWR = 10,
        BRWG = 11,
        BGWR = 12,
        RWGB = 13,
        RWBG = 14,
        GWRB = 15,
        GWBR = 16,
        BWRG = 17,
        BWGR = 18,
        WRGB = 19,
        WRBG = 20,
        WGBR = 21,
        WBRG = 22,
        WBGR = 23
    };

    struct Segment {
        uint8_t data;
        uint8_t clock;
        uint16_t startIndex;

        bool operator==(const Segment& other) const {
            return data == other.data && clock == other.clock && startIndex == other.startIndex;
        }

        bool operator!=(const Segment& other) const {
            return !(*this == other);
        }
    };

    struct Relay {
        int8_t gpio;
        bool inverted;
    };

    struct OutputCorrection {
        uint8_t red   = HYPERK_DEFAULT_OUTPUT_RED_GAIN;
        uint8_t green = HYPERK_DEFAULT_OUTPUT_GREEN_GAIN;
        uint8_t blue  = HYPERK_DEFAULT_OUTPUT_BLUE_GAIN;
        uint8_t white = HYPERK_DEFAULT_OUTPUT_WHITE_GAIN;
        uint8_t redToGreen = 0;
        uint8_t redToBlue = 0;
        uint8_t greenToRed = 0;
        uint8_t greenToBlue = 0;
        uint8_t blueToRed = 0;
        uint8_t blueToGreen = 0;
        uint8_t warmWhite = HYPERK_DEFAULT_OUTPUT_WARM_WHITE_GAIN;
        uint8_t coldWhite = HYPERK_DEFAULT_OUTPUT_COLD_WHITE_GAIN;
        uint8_t cctNeutralThreshold = HYPERK_DEFAULT_CCT_NEUTRAL_THRESHOLD;
        uint16_t cctWarmKelvin = HYPERK_DEFAULT_CCT_WARM_KELVIN;
        uint16_t cctColdKelvin = HYPERK_DEFAULT_CCT_COLD_KELVIN;
        uint16_t cctTargetKelvin = HYPERK_DEFAULT_CCT_TARGET_KELVIN;
        bool rgbToWhite = HYPERK_DEFAULT_RGB_TO_WHITE_CONVERSION != 0;
    };

    LedType  type       = static_cast<LedType>(HYPERK_DEFAULT_LED_TYPE);
    RgbwOrder rgbwOrder = static_cast<RgbwOrder>(HYPERK_DEFAULT_RGBW_ORDER);
    std::vector<Segment> segments = {{HYPERK_DEFAULT_LED_DATA_GPIO, HYPERK_DEFAULT_LED_CLOCK_GPIO, 0}};
    Relay  relay = {POWER_RELAY_GPIO, POWER_RELAY_INVERT_BOOL};
    uint16_t numLeds    = HYPERK_DEFAULT_LED_COUNT;
    uint8_t  brightness = 255;
    uint8_t  r = 196, g = 32, b = 8;
    bool     standbyOff = false;
    OutputCorrection output;
    uint8_t  effect     = 0;

    struct Calibration {
	    uint8_t gain  = 0xFF;
	    uint8_t red   = 0xA0;
	    uint8_t green = 0xA0;
	    uint8_t blue  = 0xA0;
    } calibration;

    void deserializeSegments(const JsonArray& jsonArray);
    bool deserializeSegments(const String& rawValues);
    void serializeSegments(JsonArray& jsonArray) const;
};

struct AppConfig {
    struct WifiConfig {
        String ssid;
        String password;
        bool ethernet = false;
    } wifi;
    LedConfig  led;
    String     deviceName = "hyperk";
    String     extraMdnsTag = "wled";
};

namespace Config {
    extern const AppConfig& cfg;

    bool loadConfig();
    bool saveConfig(const AppConfig &cfg);
};
