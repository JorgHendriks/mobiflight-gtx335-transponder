#pragma once

#include <Arduino.h>

#include "GTX335Input.h"
#include "GTX335Renderer.h"
#include "GTX335State.h"

namespace gtx335
{
class GTX335Device
{
public:
    GTX335Device() = default;

    void attach(const uint8_t buttonPins[ButtonCount]);
    void detach();
    void set(int16_t messageId, const char *setPoint);
    void update();

private:
    static void inputThunk(void *context, Key key, InputAction action, bool longPressHandled, uint32_t nowMs);
    void handleInput(Key key, InputAction action, bool longPressHandled, uint32_t nowMs);

    bool initialized_ = false;
    DeviceState state_;
    GTX335Input input_;
    GTX335Renderer renderer_;
    bool suppressForward_[ButtonCount] = {};
};
}
