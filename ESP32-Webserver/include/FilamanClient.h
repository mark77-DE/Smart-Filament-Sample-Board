// FilamanClient.h
// Talks to the FilaMan-System core API using cookie-based session auth
// (Viewer-role account, read-only usage — no CSRF token needed since we only do GET)
#pragma once
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>
#include "config.h"

struct FilamanLocation {
  int id;
  String name;
};

// One spool of a given filament, at a given location, with its remaining weight.
// Used to aggregate "where can I find filament X" across all its spools.
struct FilamentSpoolLocation {
  int locationId;
  float remainingWeightG;
};

// Result of a full sampleboard_uid -> filament -> locations lookup.
struct FilamentLocationResult {
  bool found = false;
  int filamentId = -1;
  // Sorted descending by remainingWeightG (most stock first) — change the
  // sort in findLocationsByFilamentId() if you'd rather prioritize by
  // last_used_at or show them in a different order.
  std::vector<FilamentSpoolLocation> locations;
};


// One filament tagged with sampleboard_uid, ready to merge into the local
// FilamentDB. Deliberately carries no storage/location info — that stays
// live via lookupLocationsByUid(), never synced into the local cache.
struct FilamentSyncEntry {
  String uid;              // sampleboard_uid
  String vendor;
  String type;              // material_type
  String color;             // manufacturer_color_name
  int ledIndex = -1;        // parsed from sampleboard_led custom field, -1 if missing/invalid
  String shopUrl;
  int spoolCount = 0;
  float totalRemainingWeightG = 0.0f;
};

class FilamanClient {
public:
  // Configured lazily via configure() so it can be re-applied whenever
  // the settings page saves a changed filamanConfig, without needing a reboot.
  FilamanClient() : _enabled(false), _port(0) {}

  void configure(bool enabled, const String& host, uint16_t port,
                 const String& email, const String& password) {
    _enabled = enabled;
    _host = host;
    _port = port;
    _email = email;
    _password = password;
    _sessionCookie = ""; // force re-login under the (possibly new) credentials
  }

  bool isEnabled() const { return _enabled; }

  // Logs in and stores the session_id cookie in RAM.
  // Uses a raw WiFiClient so we can read every Set-Cookie header line ourselves,
  // instead of relying on HTTPClient's header collection (unreliable with duplicate headers).
  bool login() {
    if (!_enabled || _host.isEmpty()) {
      if (CONFIGV2.system.debugMode) {
        Serial.println("[FILAMAN] login: skipped (disabled or host empty)");
      }
      return false;
    }

    if (CONFIGV2.system.debugMode) {
      Serial.printf("[FILAMAN] login: WiFi status=%d (3=connected), connecting to %s:%d\n",
                     (int)WiFi.status(), _host.c_str(), _port);
    }

    WiFiClient client;
    if (!client.connect(_host.c_str(), _port)) {
      if (CONFIGV2.system.debugMode) {
        Serial.println("[FILAMAN] login: TCP connect failed");
      }
      return false;
    }

    JsonDocument doc;
    doc["email"] = _email;
    doc["password"] = _password;
    String body;
    serializeJson(doc, body);

    client.print(
      "POST /auth/login HTTP/1.1\r\n"
      "Host: " + _host + "\r\n" +
      "Content-Type: application/json\r\n" +
      "Content-Length: " + String(body.length()) + "\r\n" +
      "Connection: close\r\n\r\n" +
      body
    );

    String statusLine = client.readStringUntil('\n');
    bool ok = statusLine.indexOf("200") > 0;

    String line;
    _sessionCookie = "";
    while (client.connected() || client.available()) {
      line = client.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) break; // end of headers

      String lowerLine = line;
      lowerLine.toLowerCase();
      if (lowerLine.startsWith("set-cookie:") && line.indexOf("session_id=") >= 0) {
        int start = line.indexOf("session_id=");
        int end = line.indexOf(';', start);
        if (end < 0) end = line.length();
        _sessionCookie = line.substring(start, end); // e.g. "session_id=sess.18.xxx"
      }
    }
    client.stop();

    bool success = ok && _sessionCookie.length() > 0;
    if (CONFIGV2.system.debugMode) {
      Serial.printf("[FILAMAN] login: httpOk=%d, sessionCookie=%s -> %s\n",
                     ok, _sessionCookie.length() ? "present" : "MISSING",
                     success ? "success" : "FAILED");
    }
    return success;
  }

  // Two-stage lookup, interim workaround until FilaMan's custom-field search
  // works on /api/v1/filaments directly (tracked upstream on GitHub):
  //   1) find the filament_id via the spool(s) tagged with
  //      custom_fields.sampleboard_uid == uid (copies of the same spool are
  //      fine and expected; different filaments sharing the UID are not)
  //   2) fetch ALL non-archived spools of that filament_id and aggregate
  //      their locations + remaining weight
  // Once the upstream fix lands, step 1 can search /api/v1/filaments directly
  // by custom field, and the tag can move from a representative spool onto
  // the filament itself — step 2 stays exactly the same.
  FilamentLocationResult lookupLocationsByUid(const String& uid) {
    FilamentLocationResult result;

    int filamentId = findFilamentIdByTaggedSpool(uid);
    if (filamentId < 0) return result; // uid not found (or conflicting) -> result.found stays false

    result.filamentId = filamentId;
    result.locations = findLocationsByFilamentId(filamentId);
    result.found = !result.locations.empty();
    return result;
  }

  // Warms up the client: logs in and pre-fetches the location list.
  // Call once after WiFi comes up (a cold login can otherwise silently fail
  // if it races with WiFi/DNS not being fully ready yet), and optionally
  // periodically afterwards to keep the session and location cache fresh.
  bool warmup() {
    if (!_enabled) return false;
    if (!login()) return false;
    return refreshLocations();
  }


  // Fetches every filament that has a sampleboard_uid tag, paginating through
  // the FULL filament catalog. Interim workaround until FilaMan's custom-field
  // search works on /api/v1/filaments (same upstream issue as
  // lookupLocationsByUid() above). Meant to run rarely (e.g. a user- or
  // button-triggered sync), NOT per scan — with ~1300 filaments this issues
  // many requests and can take a while.
  bool fetchAllTaggedFilaments(std::vector<FilamentSyncEntry>& out) {
    out.clear();
    if (!_enabled) return false;

    if (_sessionCookie.length() == 0) {
      if (!login()) return false;
    }

    const int pageSize = 100;
    int page = 1;
    int total = -1;

    while (total < 0 || (page - 1) * pageSize < total) {
      String url = "http://" + _host + ":" + String(_port) +
                   "/api/v1/filaments?page=" + String(page) + "&page_size=" + String(pageSize);

      int code = doGet(url);
      if (code == 401) {
        if (!login()) return false;
        code = doGet(url);
      }
      if (code != 200) return false;

      JsonDocument jsonDoc; // ArduinoJson v7
      if (deserializeJson(jsonDoc, _lastBody) != DeserializationError::Ok) return false;

      total = jsonDoc["total"] | 0;

      for (JsonObject item : jsonDoc["items"].as<JsonArray>()) {
        const char* uid = item["custom_fields"]["sampleboard_uid"] | "";
        if (strlen(uid) == 0) continue; // not tagged, skip

        FilamentSyncEntry entry;
        entry.uid = uid;
        entry.vendor = item["manufacturer"]["name"] | "";
        entry.type = item["material_type"] | "";
        entry.color = item["manufacturer_color_name"] | "";
        entry.shopUrl = item["shop_url"] | "";

        const char* ledStr = item["custom_fields"]["sampleboard_led"] | "";
        entry.ledIndex = (strlen(ledStr) > 0) ? atoi(ledStr) : -1;

        int filamentId = item["id"] | -1;
        if (filamentId >= 0) {
          aggregateSpoolStock(filamentId, entry.spoolCount, entry.totalRemainingWeightG);
        }

        if (CONFIGV2.system.debugMode) {
          Serial.printf("[FILAMAN] sync: tagged filament uid=%s vendor=%s type=%s spools=%d weight=%.0fg\n",
                         entry.uid.c_str(), entry.vendor.c_str(), entry.type.c_str(),
                         entry.spoolCount, entry.totalRemainingWeightG);
        }

        out.push_back(entry);
      }

      page++;
    }

    return true;
  }

  // Resolves a location_id to its display name (e.g. 5 -> "B1").
  // On a cache miss, refreshes once from the server and retries — locations
  // are still being added while the shelving is being built out, so a miss
  // is the natural signal that the list needs reloading, no fixed TTL needed.
  bool resolveLocationName(int locationId, String& outName) {
    if (!_enabled) return false;

    if (findCachedLocation(locationId, outName)) return true;

    if (!refreshLocations()) {
      return false;
    }

    return findCachedLocation(locationId, outName);
  }

private:
  // Stage 1: finds the filament_id via the spool(s) carrying the sampleboard_uid tag.
  // Multiple spools with the SAME uid pointing at the SAME filament_id are fine
  // (e.g. a copied spool entry) — only a mismatch across filaments is treated
  // as a real data problem and rejected.
  int findFilamentIdByTaggedSpool(const String& uid) {
    if (CONFIGV2.system.debugMode) {
      Serial.println("[FILAMAN] lookup by UID");
    }
    if (!_enabled) return -1;

    if (_sessionCookie.length() == 0) {
      if (!login()) return -1;
    }

    String url = "http://" + _host + ":" + String(_port) +
                 "/api/v1/spools?search=" + urlEncode(uid);

    if (CONFIGV2.system.debugMode) {
      Serial.print("[FILAMAN] lookup url: ");
      Serial.println(url);
    }

    int code = doGet(url);
    if (code == 401) {
      if (!login()) return -1;
      code = doGet(url);
    }
    if (code != 200) return -1;

    JsonDocument jsonDoc; // ArduinoJson v7
    if (deserializeJson(jsonDoc, _lastBody) != DeserializationError::Ok) return -1;

    int matchCount = 0;
    int matchedFilamentId = -1;
    bool conflicting = false;

    for (JsonObject item : jsonDoc["items"].as<JsonArray>()) {
      const char* fieldUid = item["custom_fields"]["sampleboard_uid"] | "";
      if (uid.equals(fieldUid)) {
        int fid = item["filament_id"] | -1;
        if (matchCount == 0) {
          matchedFilamentId = fid;
        } else if (fid != matchedFilamentId) {
          // Same UID on spools of DIFFERENT filaments — that's a real data
          // problem, not just a copied spool. Refuse rather than guess.
          conflicting = true;
        }
        matchCount++;
      }
    }

    if (conflicting || matchCount == 0 || matchedFilamentId < 0) {
      if (CONFIGV2.system.debugMode) {
        Serial.println("[FILAMAN] spool not found (or conflicting filament ids)");
      }
      return -1;
    }

    if (CONFIGV2.system.debugMode) {
      Serial.printf("[FILAMAN] spool found, filament_id=%d (%d Spule(n) mit dieser UID)\n",
                     matchedFilamentId, matchCount);
    }

    return matchedFilamentId;
  }

  // Stage 2: all non-archived spools of this filament, with remaining stock.
  // Sorted descending by remainingWeightG (most stock first) as the default
  // "best place to grab it from" ordering.
  std::vector<FilamentSpoolLocation> findLocationsByFilamentId(int filamentId) {
    std::vector<FilamentSpoolLocation> out;
    if (!_enabled) return out;

    if (_sessionCookie.length() == 0) {
      if (!login()) return out;
    }

    String url = "http://" + _host + ":" + String(_port) +
                 "/api/v1/spools?filament_id=" + String(filamentId) +
                 "&include_archived=false&page_size=100";

    if (CONFIGV2.system.debugMode) {
      Serial.print("[FILAMAN] locations url: ");
      Serial.println(url);
    }

    int code = doGet(url);
    if (code == 401) {
      if (!login()) return out;
      code = doGet(url);
    }
    if (code != 200) return out;

    JsonDocument jsonDoc; // ArduinoJson v7
    if (deserializeJson(jsonDoc, _lastBody) != DeserializationError::Ok) return out;

    for (JsonObject item : jsonDoc["items"].as<JsonArray>()) {
      float remaining = item["remaining_weight_g"] | 0.0f;
      // Skip spools with (essentially) nothing left — no point sending
      // someone to an empty spool. Adjust the threshold to taste.
      if (remaining <= 1.0f) continue;

      int locId = item["location_id"] | -1;
      if (locId < 0) continue;

      out.push_back({locId, remaining});
    }

    std::sort(out.begin(), out.end(), [](const FilamentSpoolLocation& a, const FilamentSpoolLocation& b) {
      return a.remainingWeightG > b.remainingWeightG;
    });

    if (CONFIGV2.system.debugMode) {
      Serial.printf("[FILAMAN] %d Lagerort(e) mit Bestand gefunden\n", (int)out.size());
    }

    return out;
  }


  // Sums spool count and remaining weight across all (non-empty, non-archived)
  // spools of a filament — reuses the same data findLocationsByFilamentId()
  // already fetches for the live location lookup.
  void aggregateSpoolStock(int filamentId, int& outCount, float& outTotalWeight) {
    std::vector<FilamentSpoolLocation> spools = findLocationsByFilamentId(filamentId);
    outCount = (int)spools.size();
    outTotalWeight = 0.0f;
    for (auto& s : spools) outTotalWeight += s.remainingWeightG;
  }

  bool findCachedLocation(int locationId, String& outName) {
    for (auto& loc : _locations) {
      if (loc.id == locationId) {
        outName = loc.name;
        return true;
      }
    }
    return false;
  }

  bool refreshLocations() {
    if (_sessionCookie.length() == 0) {
      if (!login()) return false;
    }

    String url = "http://" + _host + ":" + String(_port) + "/api/v1/locations?page_size=100";
    int code = doGet(url);
    if (code == 401) {
      if (!login()) return false;
      code = doGet(url);
    }
    if (code != 200) return false;

    JsonDocument jsonDoc; // ArduinoJson v7
    if (deserializeJson(jsonDoc, _lastBody) != DeserializationError::Ok) return false;

    _locations.clear();
    for (JsonObject item : jsonDoc["items"].as<JsonArray>()) {
      FilamanLocation loc;
      loc.id = item["id"] | -1;
      loc.name = item["name"] | "";
      if (loc.id >= 0) _locations.push_back(loc);
    }
    return !_locations.empty();
  }

  int doGet(const String& url) {
    HTTPClient http;
    http.begin(url);
    http.useHTTP10(true); // avoids chunked transfer-encoding, which getString() can return empty for on larger responses
    http.addHeader("Cookie", _sessionCookie);
    int code = http.GET();
    _lastBody = (code > 0) ? http.getString() : "";
    http.end();
    if (CONFIGV2.system.debugMode) {
      Serial.printf("[FILAMAN] GET %s -> code=%d, bodyLen=%d\n", url.c_str(), code, _lastBody.length());
    }
    return code;
  }

  String urlEncode(const String& s) {
    String out;
    for (char c : s) {
      if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
        out += c;
      } else {
        char buf[4];
        snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
        out += buf;
      }
    }
    return out;
  }

  bool _enabled;
  String _host;
  uint16_t _port;
  String _email, _password;
  String _sessionCookie;
  String _lastBody;
  std::vector<FilamanLocation> _locations;
};
