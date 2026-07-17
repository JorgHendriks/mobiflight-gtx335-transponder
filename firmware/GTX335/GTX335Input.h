#pragma once

#include <Arduino.h>

#include "GTX335State.h"

namespace gtx335
{
enum class InputAction : uint8_t { Press, Release, LongPress, Repeat };
using InputHandler = void (*)(void *context, Key key, InputAction action, bool longPressHandled, uint32_t nowMs);

class GTX335Input
{
public:
    void attach(const uint8_t pins[ButtonCount], uint32_t nowMs);
    void detach();
    void poll(uint32_t nowMs, InputHandler handler, void *context);

private:
    struct ButtonState {
        uint8_t pin = 0;
        bool rawPressed = false;
        bool stablePressed = false;
        bool longPressHandled = false;
        uint32_t rawChangedMs = 0;
        uint32_t pressedMs = 0;
        uint32_t nextRepeatMs = 0;
    };

    bool attached_ = false;
    ButtonState buttons_[ButtonCount];
};
}
