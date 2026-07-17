#include "GTX335Input.h"

namespace gtx335
{
namespace
{
constexpr uint32_t DebounceMs = 30;
constexpr uint32_t HoldDelayMs = 800;
constexpr uint32_t RepeatIntervalMs = 100;

bool supportsLongPress(Key key)
{
    return key == Key::Clear || key == Key::Off;
}

bool supportsRepeat(Key key)
{
    return key == Key::Digit8 || key == Key::Digit9;
}
}

void GTX335Input::attach(const uint8_t pins[ButtonCount], uint32_t nowMs)
{
    for (uint8_t index = 0; index < ButtonCount; ++index) {
        ButtonState &button = buttons_[index];
        button = ButtonState{};
        button.pin = pins[index];
        pinMode(button.pin, INPUT_PULLUP);
        const bool pressed = digitalRead(button.pin) == LOW;
        button.rawPressed = pressed;
        button.stablePressed = pressed;
        button.rawChangedMs = nowMs;
        button.pressedMs = nowMs;
        button.nextRepeatMs = nowMs + HoldDelayMs;
    }
    attached_ = true;
}

void GTX335Input::detach()
{
    attached_ = false;
}

void GTX335Input::poll(uint32_t nowMs, InputHandler handler, void *context)
{
    if (!attached_ || handler == nullptr) return;

    for (uint8_t index = 0; index < ButtonCount; ++index) {
        ButtonState &button = buttons_[index];
        const Key key = static_cast<Key>(index);
        const bool rawPressed = digitalRead(button.pin) == LOW;
        if (rawPressed != button.rawPressed) {
            button.rawPressed = rawPressed;
            button.rawChangedMs = nowMs;
        }

        if (button.rawPressed != button.stablePressed && nowMs - button.rawChangedMs >= DebounceMs) {
            button.stablePressed = button.rawPressed;
            if (button.stablePressed) {
                button.pressedMs = nowMs;
                button.nextRepeatMs = nowMs + HoldDelayMs;
                button.longPressHandled = false;
                handler(context, key, InputAction::Press, false, nowMs);
            } else {
                handler(context, key, InputAction::Release, button.longPressHandled, nowMs);
            }
        }

        if (!button.stablePressed) continue;

        if (supportsLongPress(key) && !button.longPressHandled && nowMs - button.pressedMs >= HoldDelayMs) {
            button.longPressHandled = true;
            handler(context, key, InputAction::LongPress, true, nowMs);
        }

        if (supportsRepeat(key) && static_cast<int32_t>(nowMs - button.nextRepeatMs) >= 0) {
            button.nextRepeatMs += RepeatIntervalMs;
            if (static_cast<int32_t>(nowMs - button.nextRepeatMs) >= 0) button.nextRepeatMs = nowMs + RepeatIntervalMs;
            handler(context, key, InputAction::Repeat, false, nowMs);
        }
    }
}

}
