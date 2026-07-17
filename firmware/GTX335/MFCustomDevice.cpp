#include "MFCustomDevice.h"

#include <new>
#include <stdlib.h>
#include <string.h>

#include "GTX335Pins.h"
#include "MFEEPROM.h"
#include "allocateMem.h"
#include "commandmessenger.h"

#if defined(MF_MAX_DEVICEMEM)
static_assert(sizeof(gtx335::GTX335Device) <= MF_MAX_DEVICEMEM,
              "GTX335Device exceeds the configured MobiFlight custom-device memory pool");
#endif

#if defined(HAS_CONFIG_IN_FLASH)
#include "MFCustomDevicesConfig.h"
#else
const char CustomDeviceConfig[] PROGMEM = {0};
#endif

extern MFEEPROM MFeeprom;

namespace
{
constexpr size_t StringBufferSize = 96;

bool parsePins(char *text, uint8_t output[gtx335::ButtonCount])
{
    memcpy(output, gtx335::pins::DefaultButtons, gtx335::ButtonCount);
    char *save = nullptr;
    char *token = strtok_r(text, "|", &save);
    for (uint8_t index = 0; index < gtx335::ButtonCount; ++index) {
        if (token == nullptr || *token == '\0') return false;
        char *end = nullptr;
        const long value = strtol(token, &end, 10);
        if (*end != '\0' || value < 0 || value > 29) return false;
        output[index] = static_cast<uint8_t>(value);
        token = strtok_r(nullptr, "|", &save);
    }
    return token == nullptr;
}
}

MFCustomDevice::MFCustomDevice() = default;

bool MFCustomDevice::getStringFromMem(uint16_t address, char *buffer, size_t bufferSize, bool configFromFlash)
{
    if (buffer == nullptr || bufferSize == 0) return false;
    const uint16_t memoryLength = MFeeprom.get_length();
    size_t index = 0;
    char value = 0;
    do {
        if (index + 1 >= bufferSize) return false;
        if (configFromFlash) {
            if (address >= sizeof(CustomDeviceConfig)) return false;
            value = static_cast<char>(pgm_read_byte_near(CustomDeviceConfig + address++));
        } else {
            if (address > memoryLength) return false;
            value = static_cast<char>(MFeeprom.read_byte(address++));
        }
        buffer[index++] = value;
    } while (value != '.');
    buffer[index - 1] = '\0';
    return true;
}

void MFCustomDevice::attach(uint16_t adrPin, uint16_t adrType, uint16_t adrConfig, bool configFromFlash)
{
    (void)adrConfig;
    if (adrPin == 0 || initialized_) return;

    char buffer[StringBufferSize] = {};
    if (!getStringFromMem(adrType, buffer, sizeof(buffer), configFromFlash) || strcmp(buffer, "GTX335") != 0) {
        cmdMessenger.sendCmd(kStatus, F("Custom Device is not supported by this firmware version"));
        return;
    }

    if (!getStringFromMem(adrPin, buffer, sizeof(buffer), configFromFlash)) {
        cmdMessenger.sendCmd(kStatus, F("Invalid GTX335 pin configuration"));
        return;
    }

    uint8_t buttonPins[gtx335::ButtonCount] = {};
    if (!parsePins(buffer, buttonPins)) {
        cmdMessenger.sendCmd(kStatus, F("Invalid GTX335 pin configuration"));
        return;
    }

    if (!FitInMemory(sizeof(gtx335::GTX335Device))) {
        cmdMessenger.sendCmd(kStatus, F("Custom Device does not fit in Memory"));
        return;
    }

    device_ = new (allocateMemory(sizeof(gtx335::GTX335Device))) gtx335::GTX335Device();
    device_->attach(buttonPins);
    initialized_ = true;
}

void MFCustomDevice::detach()
{
    if (!initialized_ || device_ == nullptr) return;
    device_->detach();
    initialized_ = false;
}

void MFCustomDevice::update()
{
    if (initialized_ && device_ != nullptr) device_->update();
}

void MFCustomDevice::set(int16_t messageId, char *setPoint)
{
    if (initialized_ && device_ != nullptr) device_->set(messageId, setPoint);
}
