#include "GTX335Renderer.h"

#include <SPI.h>
#include <U8g2lib.h>
#include <string.h>

#include "GTX335Formatting.h"
#include "GTX335Pins.h"
#include "assets/GarminBootLogo.h"
#include "assets/u8g2_font_logisoso_gtx20_tr.h"
#include "assets/u8g2_font_logisoso_gtx38_tr.h"

namespace gtx335
{
namespace
{
U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI Display(U8G2_R2, pins::OledCs, pins::OledDc, pins::OledReset);

// The panel bezel masks the outer OLED rows. Layout anchors intentionally target
// the visible y=2..56 window rather than the full 64-row controller area.
constexpr uint8_t FunctionLeft = 148;
constexpr uint8_t FunctionRight = 238;
constexpr uint8_t FunctionWidth = FunctionRight - FunctionLeft + 1;
constexpr uint8_t FunctionTitleBaseline = 17;
constexpr uint8_t FunctionValueBaseline = 46;
constexpr uint8_t TimerX = 144;
constexpr uint8_t SquawkX = 30;
constexpr uint8_t SquawkBaseline = 44;

uint8_t percentToByte(int8_t percent)
{
    return static_cast<uint8_t>((static_cast<uint16_t>(percent) * 255U + 50U) / 100U);
}

int16_t centeredX(const char *text)
{
    const int16_t width = Display.getStrWidth(text);
    return FunctionLeft + (static_cast<int16_t>(FunctionWidth) - width) / 2;
}

void drawCenteredSmall(const char *text, uint8_t baseline)
{
    Display.setFont(u8g2_font_7x13_tf);
    const int16_t width = Display.getStrWidth(text);
    Display.drawStr((256 - width) / 2, baseline, text);
}

void drawCenteredMessage(const char *text)
{
    Display.setFont(u8g2_font_logisoso_gtx20_tr);
    const int16_t width = Display.getStrWidth(text);
    Display.drawStr((256 - width) / 2, 36, text);
}

void drawFunctionTitle(const char *title)
{
    Display.setFont(u8g2_font_7x13_tf);
    Display.drawStr(centeredX(title), FunctionTitleBaseline, title);
}

void drawFunctionValue(const char *value)
{
    Display.setFont(u8g2_font_logisoso_gtx20_tr);
    Display.drawStr(centeredX(value), FunctionValueBaseline, value);
}

void drawRightAligned(const char *value, int16_t rightEdge, int16_t baseline)
{
    const int16_t x = rightEdge - Display.getStrWidth(value) + 1;
    Display.drawStr(x, baseline, value);
}

void drawReverseText(int16_t x, int16_t baseline, const char *text)
{
    const int16_t width = Display.getStrWidth(text);
    const int16_t top = baseline - Display.getAscent() - 1;
    const int16_t bottom = baseline - Display.getDescent() - 3;
    const int16_t height = bottom >= top ? bottom - top + 1 : 1;
    Display.drawBox(x - 1, top, width + 2, height);
    Display.setDrawColor(0);
    Display.drawStr(x, baseline, text);
    Display.setDrawColor(1);
}

int16_t glyphAdvance(char glyph)
{
    // Match U8g2 glyph advances; bounding widths would shift digits on commit.
    return static_cast<int16_t>(
        u8g2_GetGlyphWidth(Display.getU8g2(), static_cast<uint8_t>(glyph)));
}

void drawSquawk(const DeviceState &state)
{
    Display.setFont(u8g2_font_logisoso_gtx38_tr);
    if (!state.squawkEditor.active) {
        Display.drawStr(SquawkX, SquawkBaseline, state.squawk);
        return;
    }

    char shown[5] = {'_', '_', '_', '_', '\0'};
    for (uint8_t index = 0; index < state.squawkEditor.count; ++index) shown[index] = state.squawkEditor.digits[index];

    // Keep one four-glyph layout in both states; only overpaint the active slot.
    Display.drawStr(SquawkX, SquawkBaseline, shown);

    if (state.squawkEditor.count >= 4) return;

    int16_t cursorX = SquawkX;
    for (uint8_t index = 0; index < state.squawkEditor.count; ++index) {
        cursorX += glyphAdvance(shown[index]);
    }

    const char selected[2] = {shown[state.squawkEditor.count], '\0'};
    drawReverseText(cursorX, SquawkBaseline, selected);
}

void drawTimerValue(const DeviceState &state, const CounterState &counter, bool downCounter)
{
    char value[9] = {};
    if (downCounter) formatting::downCounter(state, value);
    else formatting::timer(counter.seconds, value);

    Display.setFont(u8g2_font_logisoso_gtx20_tr);
    if (downCounter && isDownCounterEditing(state)) {
        drawReverseText(TimerX, FunctionValueBaseline, value);
    } else {
        Display.drawStr(TimerX, FunctionValueBaseline, value);
    }
}

void drawXpdrPage(const DeviceState &state)
{
    drawFunctionTitle("FLIGHT ID");
    drawFunctionValue(state.flightId);
}

void drawTimerPage(const DeviceState &state)
{
    switch (currentPage(state)) {
    case 0:
        drawFunctionTitle("UP COUNTER");
        drawTimerValue(state, state.upCounter, false);
        break;
    case 1:
        drawFunctionTitle("DOWN COUNTER");
        drawTimerValue(state, state.downCounter, true);
        break;
    case 2:
        drawFunctionTitle("FLIGHT TIMER");
        drawTimerValue(state, state.flightTimer, false);
        break;
    case 3:
        drawFunctionTitle("TRIP TIMER");
        drawTimerValue(state, state.tripTimer, false);
        break;
    }
}

void drawAltitudePage(const DeviceState &state)
{
    char value[20] = {};
    switch (currentPage(state)) {
    case 0:
        drawFunctionTitle("PRESSURE ALT");
        formatting::feet(state.pressureAltitudeFeet, state.pressureAltitudeValid, value, sizeof(value));
        drawFunctionValue(value);
        break;
    case 1: {
        drawFunctionTitle("ALT MONITOR");
        const bool valid = state.altitudeMonitorArmed && state.pressureAltitudeValid;
        const int32_t delta = state.pressureAltitudeFeet - state.altitudeMonitorReferenceFeet;
        formatting::feet(delta, valid, value, sizeof(value));
        drawFunctionValue(value);
        break;
    }
    case 2: {
        char sat[20] = {};
        char dalt[20] = {};
        formatting::temperature(state.staticAirTemperatureCelsius, state.staticAirTemperatureValid, sat, sizeof(sat));
        formatting::feet(state.densityAltitudeFeet, state.densityAltitudeValid, dalt, sizeof(dalt));
        Display.setFont(u8g2_font_7x13_tf);
        Display.drawStr(142, 26, "SAT");
        Display.drawStr(135, 54, "DALT");
        Display.setFont(u8g2_font_logisoso_gtx20_tr);
        drawRightAligned(sat, 243, 26);
        drawRightAligned(dalt, 243, 54);
        break;
    }
    }
}

void drawSystemEditablePage(const DeviceState &state, bool backlight)
{
    char topValue[8] = {};
    char offsetValue[8] = {};
    const int8_t base = backlight ? state.backlightBasePercent : state.contrastBasePercent;
    const int8_t offset = backlight ? state.backlightOffsetPercent : state.contrastOffsetPercent;
    formatting::percent(base, topValue, sizeof(topValue));
    formatting::offset(offset, offsetValue, sizeof(offsetValue));

    Display.setFont(u8g2_font_7x13_tf);
    Display.drawStr(backlight ? 139 : 146, 26, backlight ? "BACKLIGHT" : "CONTRAST");
    Display.drawStr(160, 54, "OFFSET");

    Display.setFont(u8g2_font_logisoso_gtx20_tr);
    const int16_t topX = 242 - Display.getStrWidth(topValue) + 1;
    if (state.systemSelection == SystemSelection::Value) drawReverseText(topX, 26, topValue);
    else Display.drawStr(topX, 26, topValue);

    const int16_t offsetX = 244 - Display.getStrWidth(offsetValue) + 1;
    if (state.systemSelection == SystemSelection::Offset) drawReverseText(offsetX, 54, offsetValue);
    else Display.drawStr(offsetX, 54, offsetValue);
}

void drawSystemPage(const DeviceState &state)
{
    switch (currentPage(state)) {
    case 0:
        drawSystemEditablePage(state, true);
        break;
    case 1:
        drawSystemEditablePage(state, false);
        break;
    case 2:
        drawFunctionTitle("MESSAGES");
        drawFunctionValue("0");
        break;
    case 3:
        drawFunctionTitle("BLUETOOTH");
        drawFunctionValue("OFF");
        break;
    case 4:
        drawFunctionTitle("GPS STATUS");
        drawFunctionValue("3D DIFF");
        break;
    }
}

void drawScrollbar(const DeviceState &state)
{
    Display.drawPixel(251, 4);
    Display.drawHLine(250, 5, 3);
    Display.drawHLine(249, 6, 5);
    Display.drawHLine(249, 52, 5);
    Display.drawHLine(250, 53, 3);
    Display.drawPixel(251, 54);

    const uint8_t count = pageCount(state.function);
    const uint8_t page = currentPage(state);
    const uint8_t blockHeight = count <= 1 ? 43 : (43 / count < 4 ? 4 : 43 / count);
    const uint8_t range = 43 - blockHeight;
    const uint8_t y = count <= 1 ? 8 : static_cast<uint8_t>(8 + (static_cast<uint16_t>(range) * page) / (count - 1));
    Display.drawBox(250, y, 3, blockHeight);
}

void drawVerticalFunctionLabel(Function function)
{
    const char *label = functionLabel(function);
    const size_t length = strlen(label);
    const uint8_t firstBaseline = length == 4 ? 16 : 22;
    Display.setFont(u8g2_font_7x13_tf);
    for (size_t index = 0; index < length; ++index) Display.drawGlyph(248, firstBaseline + index * 11, label[index]);
}

void drawNormalScreen(const DeviceState &state, uint32_t nowMs)
{
    Display.setFont(u8g2_font_9x15_mf);
    Display.drawStr(0, 39, modeLabel(state.mode));
    drawSquawk(state);

    if (state.replyVisible) {
        Display.setFont(u8g2_font_7x13_tf);
        Display.drawStr(127, 44, "R");
    }

    switch (state.function) {
    case Function::Xpdr:
        drawXpdrPage(state);
        break;
    case Function::Timer:
        drawTimerPage(state);
        break;
    case Function::Altitude:
        drawAltitudePage(state);
        break;
    case Function::System:
        drawSystemPage(state);
        break;
    }

    if (state.functionLabelUntilMs != 0 && static_cast<int32_t>(state.functionLabelUntilMs - nowMs) > 0) {
        drawVerticalFunctionLabel(state.function);
    } else {
        drawScrollbar(state);
    }
}

}

void GTX335Renderer::begin()
{
    SPI.setSCK(pins::OledSck);
    SPI.setTX(pins::OledMosi);
    pinMode(pins::Backlight, OUTPUT);
    analogWrite(pins::Backlight, 0);

    // Activate the remapped Arduino-Pico SPI pins before U8g2 claims hardware SPI.
    SPI.begin();
    delay(20);
    Display.setBusClock(8000000UL);
    Display.begin();
    Display.setFontMode(1);
    Display.setDrawColor(1);
    Display.setPowerSave(0);
    displayAwake_ = true;
}

void GTX335Renderer::applyPowerAndLevels(const DeviceState &state)
{
    const bool shouldWake = state.mode != Mode::Off && !state.suspended && !state.connectorStopped;
    if (shouldWake != displayAwake_) {
        Display.setPowerSave(shouldWake ? 0 : 1);
        displayAwake_ = shouldWake;
    }

    const int8_t backlight = shouldWake ? effectiveBacklightPercent(state) : 0;
    if (backlight != appliedBacklight_) {
        analogWrite(pins::Backlight, percentToByte(backlight));
        appliedBacklight_ = backlight;
    }

    const int8_t contrast = effectiveContrastPercent(state);
    if (shouldWake && contrast != appliedContrast_) {
        Display.setContrast(percentToByte(contrast));
        appliedContrast_ = contrast;
    }
}

void GTX335Renderer::render(DeviceState &state, uint32_t nowMs)
{
    applyPowerAndLevels(state);
    if (!displayAwake_) {
        state.dirty = false;
        return;
    }

    Display.clearBuffer();
    if (state.offButtonPressed) {
        drawCenteredMessage("HOLD TO POWER OFF");
    } else if (state.altitudeWarningUntilMs != 0 && ((nowMs / 250U) & 1U) == 0U) {
        drawCenteredMessage("LEAVING ALTITUDE");
    } else if (state.altitudeWarningUntilMs != 0) {
        // Blank phase of the flashing warning.
    } else if (state.startupPhase == StartupPhase::Logo) {
        Display.drawXBMP(58, 12, 140, 36, GarminBootLogoBitmap);
    } else if (state.startupPhase == StartupPhase::SelfTest) {
        drawCenteredSmall("GTX335    (C) 2015-26 JORG HENDRIKS", 21);
        drawCenteredSmall("PRESS ENT FOR PRODUCT DATA", 36);
        drawCenteredSmall("SELF TEST IN PROGRESS", 51);
    } else {
        drawNormalScreen(state, nowMs);
    }
    Display.sendBuffer();
    state.dirty = false;
}

void GTX335Renderer::blank()
{
    analogWrite(pins::Backlight, 0);
    Display.clearBuffer();
    Display.sendBuffer();
    Display.setPowerSave(1);
    displayAwake_ = false;
    appliedBacklight_ = 0;
}

}
