#include "GTX335Formatting.h"

#include <stdio.h>
#include <string.h>

namespace gtx335::formatting
{
void timer(uint32_t seconds, char output[9])
{
    const uint32_t hours = (seconds / 3600U) % 100U;
    const uint32_t minutes = (seconds / 60U) % 60U;
    const uint32_t remainingSeconds = seconds % 60U;
    snprintf(output, 9, "%02lu:%02lu:%02lu", static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes), static_cast<unsigned long>(remainingSeconds));
}

void downCounter(const DeviceState &state, char output[9])
{
    const DownCounterEditor &editor = state.downCounterEditor;
    if (!editor.active) {
        timer(state.downCounter.seconds, output);
        return;
    }

    memcpy(output, "__:__:__", 9);
    constexpr uint8_t positions[6] = {0, 1, 3, 4, 6, 7};
    for (uint8_t index = 0; index < editor.digitCount; ++index) {
        output[positions[index]] = editor.digits[index];
    }
}


void feet(int32_t value, bool valid, char *output, size_t outputSize)
{
    if (!valid) {
        snprintf(output, outputSize, "----");
        return;
    }
    snprintf(output, outputSize, "%ld#", static_cast<long>(value));
}

void temperature(int32_t value, bool valid, char *output, size_t outputSize)
{
    if (!valid) {
        snprintf(output, outputSize, "----");
        return;
    }
    snprintf(output, outputSize, "%ld@", static_cast<long>(value));
}

void percent(int8_t value, char *output, size_t outputSize)
{
    snprintf(output, outputSize, "%d", static_cast<int>(value));
}

void offset(int8_t value, char *output, size_t outputSize)
{
    snprintf(output, outputSize, "%+d", static_cast<int>(value));
}

}
