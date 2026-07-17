#pragma once

#include <Arduino.h>

#include "GTX335State.h"

namespace gtx335
{
class GTX335Renderer
{
public:
    void begin();
    void render(DeviceState &state, uint32_t nowMs);
    void blank();

private:
    bool displayAwake_ = false;
    int8_t appliedBacklight_ = -1;
    int8_t appliedContrast_ = -1;

    void applyPowerAndLevels(const DeviceState &state);
};
}
