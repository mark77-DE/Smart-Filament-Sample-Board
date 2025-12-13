#pragma once
#include <Arduino.h>
#include <vector>

struct FilamentEntry {
    uint8_t ledIndex;
    String uid;
    String vendor;
    String type;
    String color;
};

// namespace FilamentDB {
//     bool load();
//     bool save();

//     bool getAll(std::vector<FilamentEntry> &list);
//     bool findByUID(const String &uid, FilamentEntry &entry);

//     bool add(const FilamentEntry &entry);
//     bool update(const FilamentEntry &entry);
//     bool removeByUID(const String &uid);
// }

namespace FilamentDB {
    void load();
    bool findByUID(const String &uid, FilamentEntry &entry);

    void getAll(std::vector<FilamentEntry> &list);

    bool loadFromFile();
    bool saveToFile();
    bool add(const FilamentEntry &entry);
    bool update(const FilamentEntry &entry);
    bool remove(const String &uid);
    bool deleteEntry(int index);

    // neue Methode: Update über Index
    bool updateAtIndex(int idx, const FilamentEntry &entry);
}
