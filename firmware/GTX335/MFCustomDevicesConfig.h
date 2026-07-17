#pragma once

#include <Arduino.h>

// Flash configuration boots the fixed device immediately and advertises its
// twenty inputs to Connector. MFButtonMetadataOnly disables stock polling, so
// GTX335Input remains the sole GPIO owner and button-event source.
// Records: custom device `17.<type>.<pins>.<config>.<name>:`; button `1.<pin>.<name>:`.
const char CustomDeviceConfig[] PROGMEM =
    "17.GTX335."
    "14|13|12|11|9|8|7|6|5|4|27|28|16|15|22|26|2|1|3|0."
    "."
    "GTX335:"
    "1.14.GTX335_0:"
    "1.13.GTX335_1:"
    "1.12.GTX335_2:"
    "1.11.GTX335_3:"
    "1.9.GTX335_4:"
    "1.8.GTX335_5:"
    "1.7.GTX335_6:"
    "1.6.GTX335_7:"
    "1.5.GTX335_8:"
    "1.4.GTX335_9:"
    "1.27.GTX335_ON:"
    "1.28.GTX335_SBY:"
    "1.16.GTX335_OFF:"
    "1.15.GTX335_VFR:"
    "1.22.GTX335_ALT:"
    "1.26.GTX335_IDNT:"
    "1.2.GTX335_FUNC:"
    "1.1.GTX335_CRSR:"
    "1.3.GTX335_CLR:"
    "1.0.GTX335_ENT:";
