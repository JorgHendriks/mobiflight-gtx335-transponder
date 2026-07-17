#pragma once

#include <Arduino.h>

namespace gtx335::messages
{
constexpr int16_t Stop = -1;
constexpr int16_t PowerSaving = -2;

constexpr int16_t TransponderCode = 0;
constexpr int16_t FlightId = 1;
constexpr int16_t PressureAltitude = 2;
constexpr int16_t StaticAirTemperature = 3;
constexpr int16_t ReplyIndicator = 4;
constexpr int16_t DensityAltitude = 5;
constexpr int16_t SimOnGround = 6;
constexpr int16_t BacklightPwm = 7;
constexpr int16_t OledContrast = 8;
}
