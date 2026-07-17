#pragma once

#include <Arduino.h>

#include "GTX335State.h"

namespace gtx335::formatting
{
void timer(uint32_t seconds, char output[9]);
void downCounter(const DeviceState &state, char output[9]);
void feet(int32_t value, bool valid, char *output, size_t outputSize);
void temperature(int32_t value, bool valid, char *output, size_t outputSize);
void percent(int8_t value, char *output, size_t outputSize);
void offset(int8_t value, char *output, size_t outputSize);
}
