#pragma once
#include <Arduino.h>
#include <vector>

struct FilamentEntry {
    uint16_t ledIndex;   // statt uint8_t (Überlauf vermeiden)
    String uid;
    String vendor;
    String type;
    String color;
};

namespace FilamentDB {

    // Lebenszyklus
    void load();

    // Lesen
    void getAll(std::vector<FilamentEntry> &list);
    bool findByUID(const String &uid, FilamentEntry &entry);

    // Schreiben (UID = Primärschlüssel)
    bool add(const FilamentEntry &entry);
    bool update(const FilamentEntry &entry);
    bool remove(const String &uid);

    // Persistenz (intern, aber ok im Header)
    bool loadFromFile();
    bool saveToFile();

    // OPTIONAL / INTERN:
    // Nur behalten, wenn du sie wirklich noch brauchst
    // (nicht über HTTP verwenden!)
    bool updateAtIndex(int idx, const FilamentEntry &entry);
}
