#include "nfc.h"

static Adafruit_PN532* _nfc = nullptr;

namespace NFC {

void init(Adafruit_PN532 *nfc) {
    _nfc = nfc;
    _nfc->begin();
    _nfc->SAMConfig();
}

String checkTag() {
    if (!_nfc) return "";
    uint8_t uid[7];
    uint8_t uidLength;

    Serial.println("Scanning for tag...");
    if (_nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
        String s = "";
        for (uint8_t i = 0; i < uidLength; i++) {
            if (i > 0) s += ":";
            if (uid[i] < 0x10) s += "0";
            s += String(uid[i], HEX);
        }
        s.toUpperCase();
        Serial.print("Found UID: ");
        Serial.println(s);
        return s;
    }
    return "";
     }
}