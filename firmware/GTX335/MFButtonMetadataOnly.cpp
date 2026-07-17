#include <Arduino.h>

#include "Button.h"

// Connector needs Button records for input discovery, but GTX335 owns the GPIO.
// These no-ops satisfy core config loading without a second poller or event stream.
namespace Button
{
bool setupArray(uint16_t)
{
    return true;
}

void Add(uint8_t, char const *)
{
}

void Clear()
{
}

void read()
{
}

void OnTrigger()
{
}
}
