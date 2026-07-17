#pragma once

#include <Arduino.h>

namespace gtx335
{
constexpr uint8_t ButtonCount = 20;
constexpr uint32_t LogoDurationMs = 2500;
constexpr uint32_t SelfTestDurationMs = 3000;
constexpr uint32_t FunctionLabelDurationMs = 1000;
constexpr uint32_t AltitudeWarningDurationMs = 2000;
constexpr int32_t AltitudeMonitorThresholdFeet = 250;
constexpr uint32_t DefaultDownCounterSeconds = 60U * 60U;

enum class Key : uint8_t {
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    On,
    Standby,
    Off,
    Vfr,
    Alt,
    Ident,
    Function,
    Cursor,
    Clear,
    Enter,
};

enum class Mode : uint8_t { Off, On, Standby, Alt };
enum class Function : uint8_t { Xpdr, Timer, Altitude, System };
enum class StartupPhase : uint8_t { Logo, SelfTest, Normal };
enum class SystemSelection : uint8_t { None, Value, Offset };

struct CounterState {
    uint32_t seconds = 0;
    bool running = false;
};

struct SquawkEditor {
    bool active = false;
    uint8_t count = 0;
    char digits[5] = {'\0', '\0', '\0', '\0', '\0'};
};

struct DownCounterEditor {
    bool active = false;
    uint32_t originalSeconds = DefaultDownCounterSeconds;
    uint8_t digitCount = 0;
    char digits[7] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0'};
};

struct DeviceState {
    Mode mode = Mode::Alt;
    Function function = Function::Xpdr;
    StartupPhase startupPhase = StartupPhase::Logo;
    uint32_t phaseStartedMs = 0;
    uint32_t functionLabelUntilMs = 0;

    bool suspended = false;
    bool connectorStopped = false;
    bool dirty = true;

    char squawk[5] = {'0', '0', '0', '0', '\0'};
    char flightId[9] = {'-', '-', '-', '-', '-', '-', '\0', '\0', '\0'};
    SquawkEditor squawkEditor;

    bool pressureAltitudeValid = false;
    int32_t pressureAltitudeFeet = 0;
    bool staticAirTemperatureValid = false;
    int32_t staticAirTemperatureCelsius = 0;
    bool densityAltitudeValid = false;
    int32_t densityAltitudeFeet = 0;
    bool replyVisible = false;

    bool simOnGround = true;
    CounterState upCounter;
    CounterState downCounter = {DefaultDownCounterSeconds, false};
    CounterState flightTimer;
    CounterState tripTimer;
    uint32_t lastTimerTickMs = 0;
    DownCounterEditor downCounterEditor;

    bool altitudeMonitorArmed = false;
    int32_t altitudeMonitorReferenceFeet = 0;
    bool altitudeMonitorOutside = false;
    uint32_t altitudeWarningUntilMs = 0;
    bool offButtonPressed = false;

    uint8_t pageByFunction[4] = {0, 0, 0, 0};
    SystemSelection systemSelection = SystemSelection::None;

    int8_t backlightBasePercent = 25;
    int8_t backlightOffsetPercent = 0;
    int8_t contrastBasePercent = 25;
    int8_t contrastOffsetPercent = 0;
};

void initializeState(DeviceState &state, uint32_t nowMs);
void tickState(DeviceState &state, uint32_t nowMs);
void applyMessage(DeviceState &state, int16_t messageId, const char *value);

bool isDownCounterPage(const DeviceState &state);
bool isDownCounterEditing(const DeviceState &state);
bool shouldSuppressPress(const DeviceState &state, Key key);

void wakeSuspended(DeviceState &state);
void handleKeyPress(DeviceState &state, Key key, uint32_t nowMs);
void handleKeyRelease(DeviceState &state, Key key, bool longPressHandled, uint32_t nowMs);
void handleKeyLongPress(DeviceState &state, Key key, uint32_t nowMs);
void handleKeyRepeat(DeviceState &state, Key key, uint32_t nowMs);

uint8_t currentPage(const DeviceState &state);
uint8_t pageCount(Function function);
int8_t effectiveBacklightPercent(const DeviceState &state);
int8_t effectiveContrastPercent(const DeviceState &state);

const char *modeLabel(Mode mode);
const char *functionLabel(Function function);

}
