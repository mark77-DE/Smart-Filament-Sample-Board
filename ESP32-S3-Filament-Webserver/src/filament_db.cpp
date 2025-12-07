#include "filament_db.h"

static FilamentEntry db[3];  // 3 Tags

namespace FilamentDB {

void load() {
    // Tag 1
    db[0].uid = "04:D3:4F:51:6F:61:81";
    db[0].vendor = "HerstellerA";
    db[0].type   = "PLA";
    db[0].color  = "Rot";
    db[0].ledIndex = 0; // LED 0

    // Tag 2
    db[1].uid = "04:21:87:51:6F:61:80";
    db[1].vendor = "HerstellerB";
    db[1].type   = "PETG";
    db[1].color  = "Blau";
    db[1].ledIndex = 3; // LED 3

    // Tag 3
    db[2].uid = "04:92:89:51:6F:61:80";
    db[2].vendor = "HerstellerC";
    db[2].type   = "ABS";
    db[2].color  = "Grün";
    db[2].ledIndex = 7; // LED 7
}

bool findByUID(const String &uid, FilamentEntry &entry) {
    for (int i = 0; i < 3; i++) {
        if (db[i].uid == uid) {
            entry = db[i];
            return true;
        }
    }
    return false;
}

} // namespace
