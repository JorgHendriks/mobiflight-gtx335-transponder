#pragma once

#include <Arduino.h>

#include "GTX335Device.h"

class MFCustomDevice
{
public:
    MFCustomDevice();
    void attach(uint16_t adrPin, uint16_t adrType, uint16_t adrConfig, bool configFromFlash = false);
    void detach();
    void update();
    void set(int16_t messageId, char *setPoint);

private:
    bool getStringFromMem(uint16_t address, char *buffer, size_t bufferSize, bool configFromFlash);

    bool initialized_ = false;
    gtx335::GTX335Device *device_ = nullptr;
};
