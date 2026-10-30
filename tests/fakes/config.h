#pragma once
enum class LedType { SK6812, ANALOG_RGBCCT };
struct TestConfig { struct { LedType type = LedType::ANALOG_RGBCCT; } led; };
namespace Config { extern TestConfig cfg; }
