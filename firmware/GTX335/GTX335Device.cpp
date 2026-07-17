#include "GTX335Device.h"

#include <string.h>

#include "GTX335Transport.h"

namespace gtx335
{
void GTX335Device::attach(const uint8_t buttonPins[ButtonCount])
{
    const uint32_t nowMs = millis();
    initializeState(state_, nowMs);
    memset(suppressForward_, 0, sizeof(suppressForward_));
    renderer_.begin();
    input_.attach(buttonPins, nowMs);
    initialized_ = true;
    renderer_.render(state_, nowMs);
}

void GTX335Device::detach()
{
    if (!initialized_) return;
    input_.detach();
    renderer_.blank();
    initialized_ = false;
}

void GTX335Device::set(int16_t messageId, const char *setPoint)
{
    if (!initialized_) return;
    applyMessage(state_, messageId, setPoint);
}

void GTX335Device::update()
{
    if (!initialized_) return;
    const uint32_t nowMs = millis();
    tickState(state_, nowMs);
    input_.poll(nowMs, inputThunk, this);
    if (state_.dirty) renderer_.render(state_, nowMs);
}

void GTX335Device::inputThunk(void *context, Key key, InputAction action, bool longPressHandled, uint32_t nowMs)
{
    static_cast<GTX335Device *>(context)->handleInput(key, action, longPressHandled, nowMs);
}

void GTX335Device::handleInput(Key key, InputAction action, bool longPressHandled, uint32_t nowMs)
{
    const uint8_t index = static_cast<uint8_t>(key);
    if (index >= ButtonCount) return;

    switch (action) {
    case InputAction::Press:
        wakeSuspended(state_);
        // Latch suppression until release so an edit-completing key cannot leak its release event.
        suppressForward_[index] = shouldSuppressPress(state_, key);
        if (!suppressForward_[index]) transport::sendButtonChange(key, true);
        handleKeyPress(state_, key, nowMs);
        break;
    case InputAction::Release:
        if (!suppressForward_[index]) transport::sendButtonChange(key, false);
        handleKeyRelease(state_, key, longPressHandled, nowMs);
        suppressForward_[index] = false;
        break;
    case InputAction::LongPress:
        handleKeyLongPress(state_, key, nowMs);
        break;
    case InputAction::Repeat:
        handleKeyRepeat(state_, key, nowMs);
        break;
    }
}

}
