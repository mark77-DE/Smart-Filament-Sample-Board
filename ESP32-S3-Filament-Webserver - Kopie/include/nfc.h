#pragma once
#include <Arduino.h>
#include <Adafruit_PN532.h>
#include <String.h>

namespace NFC {
    void init(Adafruit_PN532 *nfc);
    String checkTag();
}