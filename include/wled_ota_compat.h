#pragma once

#include <cstddef>
#include <cstdint>

#if (defined(ESP32) || defined(ARDUINO_ARCH_ESP32)) && defined(WLED_OTA_COMPAT_RELEASE_NAME)

#ifndef WLED_OTA_COMPAT_VERSION
#define WLED_OTA_COMPAT_VERSION "16.0.0"
#endif

namespace WledOtaCompat {
    constexpr uint32_t Magic = 0x57535453; // "WSTS"
    constexpr uint32_t DescVersion = 1;
    constexpr size_t VersionMaxLen = 48;
    constexpr size_t ReleaseNameMaxLen = 48;

    struct __attribute__((packed)) Metadata {
        uint32_t magic;
        uint32_t descVersion;
        char wledVersion[VersionMaxLen];
        char releaseName[ReleaseNameMaxLen];
        uint32_t hash;
        uint8_t safeUpdateVersion[3];
    };

    constexpr uint32_t djb2(const char* text, uint32_t hash = 5381) {
        return (*text == '\0')
            ? hash
            : djb2(text + 1, ((hash << 5) + hash) + static_cast<uint8_t>(*text));
    }

    static_assert(sizeof(WLED_OTA_COMPAT_VERSION) <= VersionMaxLen,
        "WLED_OTA_COMPAT_VERSION must fit WLED metadata");
    static_assert(sizeof(WLED_OTA_COMPAT_RELEASE_NAME) <= ReleaseNameMaxLen,
        "WLED_OTA_COMPAT_RELEASE_NAME must fit WLED metadata");
}

// WLED's OTA validator scans the uploaded ESP32 app image for this structure.
const WledOtaCompat::Metadata __attribute__((used, section(".rodata_custom_desc")))
HYPERK_WLED_OTA_COMPAT_DESCRIPTION = {
    WledOtaCompat::Magic,
    WledOtaCompat::DescVersion,
    WLED_OTA_COMPAT_VERSION,
    WLED_OTA_COMPAT_RELEASE_NAME,
    WledOtaCompat::djb2(WLED_OTA_COMPAT_RELEASE_NAME),
    {0, 0, 0},
};

#endif
