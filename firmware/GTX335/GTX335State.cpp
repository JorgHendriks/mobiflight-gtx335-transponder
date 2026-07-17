#include "GTX335State.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "GTX335Messages.h"

namespace gtx335
{
namespace
{
constexpr uint8_t kXpdrPageCount = 1;
constexpr uint8_t kTimerPageCount = 4;
constexpr uint8_t kAltitudePageCount = 3;
constexpr uint8_t kSystemPageCount = 5;

constexpr uint8_t functionIndex(Function function)
{
    return static_cast<uint8_t>(function);
}

bool parseStrictNumber(const char *text, double &result)
{
    if (text == nullptr) return false;

    while (isspace(static_cast<unsigned char>(*text))) ++text;
    if (*text == '\0') return false;

    char *end = nullptr;
    const double value = strtod(text, &end);
    if (end == text || !isfinite(value)) return false;

    while (isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end != '\0') return false;

    result = value;
    return true;
}

bool parseStrictInteger(const char *text, int32_t &result)
{
    double value = 0.0;
    if (!parseStrictNumber(text, value)) return false;
    if (value > static_cast<double>(INT32_MAX) || value < static_cast<double>(INT32_MIN)) return false;
    result = static_cast<int32_t>(lround(value));
    return true;
}

bool parseStrictBoolean(const char *text, bool &result)
{
    double value = 0.0;
    if (!parseStrictNumber(text, value) || (value != 0.0 && value != 1.0)) return false;
    result = value == 1.0;
    return true;
}

int8_t clampPercent(int32_t value)
{
    if (value < 0) return 0;
    if (value > 100) return 100;
    return static_cast<int8_t>(value);
}

int8_t clampOffset(int32_t value)
{
    if (value < -50) return -50;
    if (value > 50) return 50;
    return static_cast<int8_t>(value);
}

void resetForPowerOn(DeviceState &state, Mode requestedMode, uint32_t nowMs)
{
    // Every OFF-to-active transition is a cold local start; simulator values return only when resent.
    state = DeviceState{};
    state.mode = requestedMode;
    state.phaseStartedMs = nowMs;
    state.lastTimerTickMs = nowMs;
    state.dirty = true;
}

void setMode(DeviceState &state, Mode mode, uint32_t nowMs)
{
    if (state.mode == Mode::Off && mode != Mode::Off) {
        resetForPowerOn(state, mode, nowMs);
        return;
    }

    state.mode = mode;
    state.suspended = false;
    state.connectorStopped = false;
    state.dirty = true;
}

void cancelSquawkEdit(DeviceState &state)
{
    state.squawkEditor = SquawkEditor{};
    state.dirty = true;
}

void beginSquawkEdit(DeviceState &state)
{
    state.squawkEditor = SquawkEditor{};
    state.squawkEditor.active = true;
}

void appendSquawkDigit(DeviceState &state, uint8_t digit)
{
    if (digit > 7) return;
    if (!state.squawkEditor.active) beginSquawkEdit(state);
    if (state.squawkEditor.count >= 4) return;

    state.squawkEditor.digits[state.squawkEditor.count++] = static_cast<char>('0' + digit);
    if (state.squawkEditor.count == 4) {
        memcpy(state.squawk, state.squawkEditor.digits, 4);
        state.squawk[4] = '\0';
        state.squawkEditor = SquawkEditor{};
    }
    state.dirty = true;
}

void deleteSquawkDigit(DeviceState &state)
{
    if (!state.squawkEditor.active || state.squawkEditor.count == 0) return;
    --state.squawkEditor.count;
    state.squawkEditor.digits[state.squawkEditor.count] = '\0';
    state.dirty = true;
}

void normalizeSquawk(const char *value, char output[5])
{
    char octalDigits[4] = {};
    uint8_t count = 0;
    if (value != nullptr) {
        for (const char *cursor = value; *cursor != '\0'; ++cursor) {
            if (*cursor < '0' || *cursor > '7') continue;
            if (count < sizeof(octalDigits)) {
                octalDigits[count++] = *cursor;
            } else {
                memmove(octalDigits, octalDigits + 1, sizeof(octalDigits) - 1);
                octalDigits[sizeof(octalDigits) - 1] = *cursor;
            }
        }
    }

    output[0] = output[1] = output[2] = output[3] = '0';
    output[4] = '\0';
    if (count > 0) memcpy(output + (4 - count), octalDigits, count);
}

void normalizeFlightId(const char *value, char output[9])
{
    output[0] = '\0';
    if (value == nullptr) value = "";

    while (isspace(static_cast<unsigned char>(*value))) ++value;
    const char *end = value + strlen(value);
    while (end > value && isspace(static_cast<unsigned char>(end[-1]))) --end;

    const size_t length = static_cast<size_t>(end - value);
    if (length == 0) {
        memcpy(output, "------", 7);
        return;
    }

    const size_t copyLength = length > 8 ? 8 : length;
    memcpy(output, value, copyLength);
    output[copyLength] = '\0';
}

void updateAutomaticTimers(DeviceState &state, bool onGround)
{
    if (state.simOnGround == onGround) return;

    if (state.simOnGround && !onGround) {
        state.flightTimer.seconds = 0;
        state.flightTimer.running = true;
        state.tripTimer.running = true;
    } else if (!state.simOnGround && onGround) {
        state.flightTimer.running = false;
        state.tripTimer.running = false;
    }
    state.simOnGround = onGround;
    state.dirty = true;
}

void advanceCounter(CounterState &counter, uint32_t ticks, bool down)
{
    if (!counter.running || ticks == 0) return;
    if (!down) {
        counter.seconds += ticks;
        return;
    }

    if (ticks >= counter.seconds) {
        counter.seconds = 0;
        counter.running = false;
    } else {
        counter.seconds -= ticks;
    }
}

void navigatePage(DeviceState &state, int8_t direction)
{
    const uint8_t count = pageCount(state.function);
    uint8_t &page = state.pageByFunction[functionIndex(state.function)];
    if (direction < 0 && page > 0) {
        --page;
    } else if (direction > 0 && page + 1 < count) {
        ++page;
    }
    state.systemSelection = SystemSelection::None;
    state.dirty = true;
}

void cycleFunction(DeviceState &state, uint32_t nowMs)
{
    state.function = static_cast<Function>((functionIndex(state.function) + 1) % 4);
    state.systemSelection = SystemSelection::None;
    if (state.downCounterEditor.active) {
        state.downCounter.seconds = state.downCounterEditor.originalSeconds;
        state.downCounterEditor = DownCounterEditor{};
    }
    cancelSquawkEdit(state);
    state.functionLabelUntilMs = nowMs + FunctionLabelDurationMs;
    state.dirty = true;
}

void beginDownCounterEdit(DeviceState &state)
{
    DownCounterEditor &editor = state.downCounterEditor;
    editor = DownCounterEditor{};
    editor.active = true;
    editor.originalSeconds = state.downCounter.seconds;
    state.dirty = true;
}

void cancelDownCounterEdit(DeviceState &state)
{
    state.downCounter.seconds = state.downCounterEditor.originalSeconds;
    state.downCounterEditor = DownCounterEditor{};
    state.dirty = true;
}

bool downCounterDigitAllowed(uint8_t position, uint8_t digit)
{
    return digit <= 9 && ((position != 2 && position != 4) || digit <= 5);
}

void commitDownCounterEdit(DeviceState &state)
{
    const DownCounterEditor &editor = state.downCounterEditor;
    const uint8_t hours = static_cast<uint8_t>((editor.digits[0] - '0') * 10 + (editor.digits[1] - '0'));
    const uint8_t minutes = static_cast<uint8_t>((editor.digits[2] - '0') * 10 + (editor.digits[3] - '0'));
    const uint8_t seconds = static_cast<uint8_t>((editor.digits[4] - '0') * 10 + (editor.digits[5] - '0'));
    state.downCounter.seconds = static_cast<uint32_t>(hours) * 3600U +
                                static_cast<uint32_t>(minutes) * 60U + seconds;
    state.downCounterEditor = DownCounterEditor{};
    state.dirty = true;
}

void enterDownCounterDigit(DeviceState &state, uint8_t digit)
{
    DownCounterEditor &editor = state.downCounterEditor;
    if (!editor.active || editor.digitCount >= 6 || !downCounterDigitAllowed(editor.digitCount, digit)) return;

    editor.digits[editor.digitCount++] = static_cast<char>('0' + digit);
    editor.digits[editor.digitCount] = '\0';
    if (editor.digitCount == 6) commitDownCounterEdit(state);
    else state.dirty = true;
}

void cycleSystemSelection(DeviceState &state)
{
    if (state.function != Function::System || currentPage(state) > 1) return;
    state.systemSelection = static_cast<SystemSelection>((static_cast<uint8_t>(state.systemSelection) + 1) % 3);
    state.dirty = true;
}

void adjustSystemValue(DeviceState &state, int8_t direction)
{
    if (state.function != Function::System || currentPage(state) > 1 ||
        state.systemSelection == SystemSelection::None) {
        return;
    }

    const bool backlightPage = currentPage(state) == 0;
    if (state.systemSelection == SystemSelection::Value) {
        int8_t &base = backlightPage ? state.backlightBasePercent : state.contrastBasePercent;
        base = clampPercent(base + direction * 5);
    } else {
        int8_t &offset = backlightPage ? state.backlightOffsetPercent : state.contrastOffsetPercent;
        offset = clampOffset(offset + direction);
    }
    state.dirty = true;
}

bool keyIsDigit(Key key, uint8_t &digit)
{
    const uint8_t raw = static_cast<uint8_t>(key);
    if (raw > static_cast<uint8_t>(Key::Digit9)) return false;
    digit = raw;
    return true;
}

void handleNavigationKey(DeviceState &state, Key key)
{
    if (key != Key::Digit8 && key != Key::Digit9) return;
    if (state.function == Function::System && state.systemSelection != SystemSelection::None) {
        adjustSystemValue(state, key == Key::Digit8 ? 1 : -1);
    } else {
        navigatePage(state, key == Key::Digit8 ? -1 : 1);
    }
}

void handleEnter(DeviceState &state)
{
    switch (state.function) {
    case Function::Timer:
        switch (currentPage(state)) {
        case 0:
            state.upCounter.running = !state.upCounter.running;
            break;
        case 1:
            if (!isDownCounterEditing(state)) state.downCounter.running = !state.downCounter.running;
            break;
        default:
            break;
        }
        break;
    case Function::Altitude:
        if (currentPage(state) == 1 && state.pressureAltitudeValid) {
            state.altitudeMonitorReferenceFeet = state.pressureAltitudeFeet;
            state.altitudeMonitorArmed = true;
            state.altitudeMonitorOutside = false;
            state.altitudeWarningUntilMs = 0;
        }
        break;
    default:
        break;
    }
    state.dirty = true;
}

void handleShortClear(DeviceState &state)
{
    if (state.squawkEditor.active) {
        deleteSquawkDigit(state);
        return;
    }

    switch (state.function) {
    case Function::Timer:
        switch (currentPage(state)) {
        case 0:
            state.upCounter.seconds = 0;
            break;
        case 1:
            if (isDownCounterEditing(state)) {
                cancelDownCounterEdit(state);
                return;
            }
            state.downCounter.seconds = DefaultDownCounterSeconds;
            state.downCounter.running = false;
            break;
        case 3:
            state.tripTimer.seconds = 0;
            break;
        default:
            break;
        }
        break;
    case Function::Altitude:
        if (currentPage(state) == 1) {
            state.altitudeMonitorArmed = false;
            state.altitudeMonitorOutside = false;
            state.altitudeWarningUntilMs = 0;
        }
        break;
    default:
        break;
    }
    state.dirty = true;
}

}

void initializeState(DeviceState &state, uint32_t nowMs)
{
    resetForPowerOn(state, Mode::Alt, nowMs);
}

void tickState(DeviceState &state, uint32_t nowMs)
{
    if (state.mode != Mode::Off) {
        if (state.startupPhase == StartupPhase::Logo && nowMs - state.phaseStartedMs >= LogoDurationMs) {
            state.startupPhase = StartupPhase::SelfTest;
            state.phaseStartedMs += LogoDurationMs;
            state.dirty = true;
        }
        if (state.startupPhase == StartupPhase::SelfTest && nowMs - state.phaseStartedMs >= SelfTestDurationMs) {
            state.startupPhase = StartupPhase::Normal;
            state.phaseStartedMs += SelfTestDurationMs;
            state.dirty = true;
        }
    }

    const uint32_t elapsed = nowMs - state.lastTimerTickMs;
    if (elapsed >= 1000U) {
        const uint32_t ticks = elapsed / 1000U;
        state.lastTimerTickMs += ticks * 1000U;
        const uint32_t beforeDown = state.downCounter.seconds;
        const uint32_t beforeUp = state.upCounter.seconds;
        const uint32_t beforeFlight = state.flightTimer.seconds;
        const uint32_t beforeTrip = state.tripTimer.seconds;
        advanceCounter(state.upCounter, ticks, false);
        advanceCounter(state.downCounter, ticks, true);
        advanceCounter(state.flightTimer, ticks, false);
        advanceCounter(state.tripTimer, ticks, false);
        if (beforeDown != state.downCounter.seconds || beforeUp != state.upCounter.seconds ||
            beforeFlight != state.flightTimer.seconds || beforeTrip != state.tripTimer.seconds) {
            state.dirty = true;
        }
    }

    if (state.functionLabelUntilMs != 0 && static_cast<int32_t>(nowMs - state.functionLabelUntilMs) >= 0) {
        state.functionLabelUntilMs = 0;
        state.dirty = true;
    }

    if (state.altitudeWarningUntilMs != 0) {
        if (static_cast<int32_t>(nowMs - state.altitudeWarningUntilMs) >= 0) {
            state.altitudeWarningUntilMs = 0;
            state.dirty = true;
        } else {
            // Redraw at the flashing cadence while the warning is active.
            state.dirty = true;
        }
    }
}

void applyMessage(DeviceState &state, int16_t messageId, const char *value)
{
    if (messageId != messages::Stop && messageId != messages::PowerSaving && state.connectorStopped) {
        state.connectorStopped = false;
        state.dirty = true;
    }

    int32_t numeric = 0;
    bool boolean = false;
    switch (messageId) {
    case messages::Stop:
        state.connectorStopped = true;
        state.dirty = true;
        return;
    case messages::PowerSaving:
        if (parseStrictBoolean(value, boolean)) {
            state.suspended = boolean;
            if (!boolean) state.connectorStopped = false;
            state.dirty = true;
        }
        return;
    case messages::TransponderCode:
        normalizeSquawk(value, state.squawk);
        cancelSquawkEdit(state);
        return;
    case messages::FlightId:
        normalizeFlightId(value, state.flightId);
        state.dirty = true;
        return;
    case messages::PressureAltitude:
        state.pressureAltitudeValid = parseStrictInteger(value, numeric);
        if (state.pressureAltitudeValid) {
            state.pressureAltitudeFeet = numeric;
            if (state.altitudeMonitorArmed) {
                const int64_t delta = static_cast<int64_t>(numeric) - state.altitudeMonitorReferenceFeet;
                const bool outside = delta >= AltitudeMonitorThresholdFeet ||
                                     delta <= -AltitudeMonitorThresholdFeet;
                if (outside && !state.altitudeMonitorOutside) {
                    state.altitudeWarningUntilMs = millis() + AltitudeWarningDurationMs;
                }
                state.altitudeMonitorOutside = outside;
            }
        }
        state.dirty = true;
        return;
    case messages::StaticAirTemperature:
        state.staticAirTemperatureValid = parseStrictInteger(value, numeric);
        if (state.staticAirTemperatureValid) state.staticAirTemperatureCelsius = numeric;
        state.dirty = true;
        return;
    case messages::ReplyIndicator:
        if (parseStrictBoolean(value, boolean)) {
            state.replyVisible = boolean;
            state.dirty = true;
        }
        return;
    case messages::DensityAltitude:
        state.densityAltitudeValid = parseStrictInteger(value, numeric);
        if (state.densityAltitudeValid) state.densityAltitudeFeet = numeric;
        state.dirty = true;
        return;
    case messages::SimOnGround:
        if (parseStrictBoolean(value, boolean)) updateAutomaticTimers(state, boolean);
        return;
    case messages::BacklightPwm:
        if (parseStrictInteger(value, numeric)) {
            if (numeric < 0) numeric = 0;
            if (numeric > 255) numeric = 255;
            state.backlightBasePercent = static_cast<int8_t>(lround(static_cast<double>(numeric) * 100.0 / 255.0));
            state.dirty = true;
        }
        return;
    case messages::OledContrast:
        if (parseStrictInteger(value, numeric)) {
            state.contrastBasePercent = clampPercent(numeric);
            state.dirty = true;
        }
        return;
    default:
        return;
    }
}

bool isDownCounterPage(const DeviceState &state)
{
    return state.function == Function::Timer && currentPage(state) == 1;
}

bool isDownCounterEditing(const DeviceState &state)
{
    return state.downCounterEditor.active;
}

bool shouldSuppressPress(const DeviceState &state, Key key)
{
    return isDownCounterEditing(state) || (isDownCounterPage(state) && key == Key::Cursor);
}

void wakeSuspended(DeviceState &state)
{
    if (!state.suspended) return;
    state.suspended = false;
    state.connectorStopped = false;
    state.dirty = true;
}

void handleKeyPress(DeviceState &state, Key key, uint32_t nowMs)
{
    switch (key) {
    case Key::Off:
        // OFF is local long-press only; normal press/release events still reach MobiFlight.
        state.offButtonPressed = true;
        state.dirty = true;
        return;
    case Key::On:
        setMode(state, Mode::On, nowMs);
        return;
    case Key::Standby:
        setMode(state, Mode::Standby, nowMs);
        return;
    case Key::Alt:
        setMode(state, Mode::Alt, nowMs);
        return;
    default:
        break;
    }

    if (state.mode == Mode::Off) return;

    uint8_t digit = 0;
    if (isDownCounterEditing(state)) {
        if (keyIsDigit(key, digit)) enterDownCounterDigit(state, digit);
        else if (key == Key::Cursor) cancelDownCounterEdit(state);
        return;
    }

    if (keyIsDigit(key, digit)) {
        if (digit <= 7) appendSquawkDigit(state, digit);
        else handleNavigationKey(state, key);
        return;
    }

    switch (key) {
    case Key::Function:
        cycleFunction(state, nowMs);
        break;
    case Key::Cursor:
        if (state.squawkEditor.active) cancelSquawkEdit(state);
        else if (isDownCounterPage(state)) beginDownCounterEdit(state);
        else cycleSystemSelection(state);
        break;
    case Key::Enter:
        handleEnter(state);
        break;
    case Key::Vfr:
    case Key::Ident:
    case Key::Clear:
    default:
        break;
    }
}

void handleKeyRelease(DeviceState &state, Key key, bool longPressHandled, uint32_t nowMs)
{
    (void)nowMs;
    if (key == Key::Off) {
        state.offButtonPressed = false;
        state.dirty = true;
    }
    if (state.mode == Mode::Off) return;
    if (key == Key::Clear && !longPressHandled) handleShortClear(state);
}

void handleKeyLongPress(DeviceState &state, Key key, uint32_t nowMs)
{
    (void)nowMs;
    if (state.mode == Mode::Off) return;

    if (key == Key::Off) {
        state.offButtonPressed = false;
        state.mode = Mode::Off;
        state.suspended = false;
        state.dirty = true;
        return;
    }

    if (key != Key::Clear) return;
    if (isDownCounterEditing(state)) cancelDownCounterEdit(state);
    else if (state.squawkEditor.active) cancelSquawkEdit(state);
}

void handleKeyRepeat(DeviceState &state, Key key, uint32_t nowMs)
{
    (void)nowMs;
    if (state.mode == Mode::Off || isDownCounterEditing(state)) return;
    handleNavigationKey(state, key);
}

uint8_t currentPage(const DeviceState &state)
{
    return state.pageByFunction[functionIndex(state.function)];
}

uint8_t pageCount(Function function)
{
    switch (function) {
    case Function::Xpdr:
        return kXpdrPageCount;
    case Function::Timer:
        return kTimerPageCount;
    case Function::Altitude:
        return kAltitudePageCount;
    case Function::System:
        return kSystemPageCount;
    }
    return 1;
}

int8_t effectiveBacklightPercent(const DeviceState &state)
{
    return clampPercent(state.backlightBasePercent + state.backlightOffsetPercent);
}

int8_t effectiveContrastPercent(const DeviceState &state)
{
    return clampPercent(state.contrastBasePercent + state.contrastOffsetPercent);
}

const char *modeLabel(Mode mode)
{
    switch (mode) {
    case Mode::On:
        return "ON";
    case Mode::Standby:
        return "SBY";
    case Mode::Alt:
        return "ALT";
    case Mode::Off:
        return "";
    }
    return "";
}

const char *functionLabel(Function function)
{
    switch (function) {
    case Function::Xpdr:
        return "XPDR";
    case Function::Timer:
        return "TMR";
    case Function::Altitude:
        return "ALT";
    case Function::System:
        return "SYS";
    }
    return "";
}

}
