#pragma once

#include <Arduino.h>

#include "GTX335State.h"

namespace gtx335::transport
{
void sendButtonChange(Key key, bool pressed);
const char *buttonName(Key key);
}
