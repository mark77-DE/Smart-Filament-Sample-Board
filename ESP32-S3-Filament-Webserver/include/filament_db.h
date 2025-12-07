#pragma once
#include <Arduino.h>

struct FilamentEntry {
    uint8_t ledIndex;   // Welche LED aufleuchtet
    String uid;         // NFC Tag UID
    String vendor;      // Hersteller
    String type;        // Typ
    String color;       // Farbe
};

namespace FilamentDB {
    void load();
    bool findByUID(const String &uid, FilamentEntry &entry);
}