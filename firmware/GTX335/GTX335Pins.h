#pragma once

#include <Arduino.h>

#include "GTX335State.h"

namespace gtx335::pins
{
constexpr uint8_t OledCs = 17;
constexpr uint8_t OledSck = 18;
constexpr uint8_t OledMosi = 19;
constexpr uint8_t OledDc = 20;
constexpr uint8_t OledReset = 21;
constexpr uint8_t Backlight = 10;

constexpr uint8_t DefaultButtons[ButtonCount] = {
    14, 13, 12, 11, 9, 8, 7, 6, 5, 4, 27, 28, 16, 15, 22, 26, 2, 1, 3, 0};
}
