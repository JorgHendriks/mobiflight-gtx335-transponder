#include "GTX335Transport.h"

#include "commandmessenger.h"

namespace gtx335::transport
{
namespace
{
constexpr uint8_t ButtonOnPress = 0;
constexpr uint8_t ButtonOnRelease = 1;

const char *const ButtonNames[ButtonCount] = {
    "GTX335_0", "GTX335_1", "GTX335_2", "GTX335_3", "GTX335_4",
    "GTX335_5", "GTX335_6", "GTX335_7", "GTX335_8", "GTX335_9",
    "GTX335_ON", "GTX335_SBY", "GTX335_OFF", "GTX335_VFR", "GTX335_ALT",
    "GTX335_IDNT", "GTX335_FUNC", "GTX335_CRSR", "GTX335_CLR", "GTX335_ENT"};
}

const char *buttonName(Key key)
{
    const uint8_t index = static_cast<uint8_t>(key);
    return index < ButtonCount ? ButtonNames[index] : "";
}

void sendButtonChange(Key key, bool pressed)
{
    cmdMessenger.sendCmdStart(kButtonChange);
    cmdMessenger.sendCmdArg(buttonName(key));
    cmdMessenger.sendCmdArg(pressed ? ButtonOnPress : ButtonOnRelease);
    cmdMessenger.sendCmdEnd();
}

}
