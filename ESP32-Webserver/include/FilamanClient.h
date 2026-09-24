// FilamanClient.h
// Talks to the FilaMan-System core API using cookie-based session auth
// (Viewer-role account, read-only usage — no CSRF token needed since we only do GET)
//
// Tags (sampleboard_uid, sampleboard_led) live ONLY on the FILAMENT in
// FilaMan, never on a spool. This works because the plain, unfiltered
// pagination over /api/v1/filaments is unaffected by FilaMan's known bug in
// server-side custom-field search/filtering — only search/filter is broken,
// not reading the data. So identity resolution (vendor/type/color/ledIndex)
// happens entirely through the periodic sync (fetchAllTaggedFilaments), and
// this client's only LIVE per-scan job is refreshing the location for an
// already-known filament_id (findLocationsByFilamentId), since locations
// change too often to cache (AMS/Bambuddy slot reassignment etc.).
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
// Used both for the live "where can I find filament X" lookup and for the
// stock-summary shown during sync (see FilamentSyncEntry).
struct FilamentSpoolLocation {
  int locationId;
  float remainingWeightG;
};

// One filament tagged with sampleboard_uid, ready to merge into the local
// FilamentDB. Deliberately carries no storage/location info — that stays
// live via findLocationsByFilamentId(), never synced into the local cache.
struct FilamentSyncEntry {
  String uid;              // sampleboard_uid
  int filamentId = -1;     // FilaMan's filament id — kept locally so the live
                            // location lookup never needs a spool search again
  String vendor;
  String type;              // material_type
  String color;             // manufacturer_color_name
  int ledIndex = -1;        // parsed from sampleboard_led custom field, -1 if missing/invalid
  String shopUrl;
  int spoolCount = 0;
  float totalRemainingWeightG = 0.0f;
};

// Summary of one sync run — lets the caller (or a display/WebIF status line)
// report something like "1300 Filamente gescannt, 63 Spulen gefunden,
// 2 Filamente mit Sample-UID aber ohne passende Spule" instead of having to
// count log lines.
struct FilamentSyncSummary {
  int totalFilamentsScanned = 0;   // "total" from FilaMan's pagination (whole catalog, not just tagged)
  int taggedFilamentsFound = 0;    // filaments with a sampleboard_uid tag
  int totalSpoolsFound = 0;        // summed spoolCount across all tagged filaments
  int pagesFailed = 0;              // pages that failed even after doGet()'s internal retry — a partial sync
  std::vector<String> taggedWithoutSpools;
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

  // Live per-scan call for an ALREADY-KNOWN filament_id (from the local,
  // synced FilamentDB): DISTINCT locations holding this filament, with
  // remaining stock summed across all spools at that location. Two spools
  // of the same filament sitting in the same spot (e.g. both in "B3") show
  // up as ONE entry with combined weight, not two — otherwise a display
  // like "B3 (+1 weitere)" would wrongly imply a second, different place.
  // Sorted descending by remainingWeightG (most stock first) as the default
  // "best place to grab it from" ordering.
  std::vector<FilamentSpoolLocation> findLocationsByFilamentId(int filamentId) {
    std::vector<FilamentSpoolLocation> raw = fetchSpoolsForFilament(filamentId);

    std::vector<FilamentSpoolLocation> merged;
    for (auto& s : raw) {
      bool found = false;
      for (auto& m : merged) {
        if (m.locationId == s.locationId) {
          m.remainingWeightG += s.remainingWeightG;
          found = true;
          break;
        }
      }
      if (!found) merged.push_back(s);
    }

    std::sort(merged.begin(), merged.end(), [](const FilamentSpoolLocation& a, const FilamentSpoolLocation& b) {
      return a.remainingWeightG > b.remainingWeightG;
    });

    return merged;
  }

  // Fetches every filament that has a sampleboard_uid tag, paginating through
  // the FULL filament catalog. Interim workaround until FilaMan's custom-field
  // search works on /api/v1/filaments (upstream issue) — plain, unfiltered
  // pagination is unaffected by that bug, only search/filter is. Meant to run
  // rarely (a user- or button-triggered sync), NOT per scan — with ~1300
  // filaments this issues many requests and can take a while.
  bool fetchAllTaggedFilaments(std::vector<FilamentSyncEntry>& out, FilamentSyncSummary& summary) {
    out.clear();
    summary = FilamentSyncSummary{};
    if (!_enabled) return false;

    if (_sessionCookie.length() == 0) {
      if (!login()) return false;
    }

    const int pageSize = 15; // filament objects are large (nested manufacturer/colors/custom_fields) — keep pages small
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

      if (code != 200) {
        if (page == 1) {
          // Page 1 gives us `total`, without which we don't know how many
          // pages to expect at all — a real, unrecoverable failure.
          if (CONFIGV2.system.debugMode) {
            Serial.println("[FILAMAN] sync: page 1 failed, aborting (can't determine total)");
          }
          return false;
        }
        // Any later page: skip it and keep going rather than throwing away
        // everything already collected. It'll be picked up on the next sync.
        if (CONFIGV2.system.debugMode) {
          Serial.printf("[FILAMAN] sync: page %d failed (code=%d) even after retry, skipping\n", page, code);
        }
        summary.pagesFailed++;
        page++;
        continue;
      }

      JsonDocument jsonDoc; // ArduinoJson v7
      if (deserializeJson(jsonDoc, _lastBody) != DeserializationError::Ok) {
        if (page == 1) return false;
        if (CONFIGV2.system.debugMode) {
          Serial.printf("[FILAMAN] sync: page %d JSON parse failed, skipping\n", page);
        }
        summary.pagesFailed++;
        page++;
        continue;
      }

      total = jsonDoc["total"] | 0;
      summary.totalFilamentsScanned = total;

      for (JsonObject item : jsonDoc["items"].as<JsonArray>()) {
        const char* uid = item["custom_fields"]["sampleboard_uid"] | "";
        if (strlen(uid) == 0) continue; // not tagged, skip

        FilamentSyncEntry entry;
        entry.uid = uid;
        entry.filamentId = item["id"] | -1;
        entry.vendor = item["manufacturer"]["name"] | "";
        entry.type = item["material_type"] | "";
        entry.color = item["manufacturer_color_name"] | "";
        entry.shopUrl = item["shop_url"] | "";

        const char* ledStr = item["custom_fields"]["sampleboard_led"] | "";
        entry.ledIndex = (strlen(ledStr) > 0) ? atoi(ledStr) : -1;

        if (entry.filamentId >= 0) {
          aggregateSpoolStock(entry.filamentId, entry.spoolCount, entry.totalRemainingWeightG);
        }

        summary.taggedFilamentsFound++;
        summary.totalSpoolsFound += entry.spoolCount;

        if (entry.spoolCount == 0) {
          String desc = entry.vendor + " " + entry.type + " " + entry.color + " (uid=" + entry.uid + ")";
          summary.taggedWithoutSpools.push_back(desc);
        }

        if (CONFIGV2.system.debugMode) {
          Serial.printf("[FILAMAN] sync: tagged filament uid=%s filament_id=%d vendor=%s type=%s spools=%d weight=%.0fg\n",
                         entry.uid.c_str(), entry.filamentId, entry.vendor.c_str(), entry.type.c_str(),
                         entry.spoolCount, entry.totalRemainingWeightG);
        }

        out.push_back(entry);
      }

      page++;
    }

    // Always printed (not gated by debugMode) — a sync is a rare, deliberately
    // triggered action, and its outcome is worth seeing regardless of debug mode.
    Serial.println(F("[FILAMAN] --- Sync Summary ---"));
    Serial.printf("[FILAMAN] %d Filamente gescannt, %d mit Sample-UID getaggt, %d Spulen insgesamt gefunden\n",
                   summary.totalFilamentsScanned, summary.taggedFilamentsFound, summary.totalSpoolsFound);
    if (summary.pagesFailed > 0) {
      Serial.printf("[FILAMAN] ACHTUNG: %d Seite(n) trotz Retry fehlgeschlagen -> Sync unvollstaendig, ggf. erneut ausfuehren\n",
                     summary.pagesFailed);
    }
    if (!summary.taggedWithoutSpools.empty()) {
      Serial.printf("[FILAMAN] %d getaggte(s) Filament(e) OHNE passende Spule (evtl. falscher Katalogeintrag getaggt?):\n",
                     (int)summary.taggedWithoutSpools.size());
      for (auto& desc : summary.taggedWithoutSpools) {
        Serial.printf("[FILAMAN]   - %s\n", desc.c_str());
      }
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

  // Warms up the client: logs in and pre-fetches the location list.
  // Call once after WiFi comes up (a cold login can otherwise silently fail
  // if it races with WiFi/DNS not being fully ready yet), and optionally
  // periodically afterwards to keep the session and location cache fresh.
  bool warmup() {
    if (!_enabled) return false;
    if (!login()) return false;
    return refreshLocations();
  }

private:
  // Raw, one-entry-per-spool fetch (NOT merged by location) — used by
  // findLocationsByFilamentId() (which merges by location) and
  // aggregateSpoolStock() (which needs the true spool count) alike, so the
  // request itself only lives in one place.
  std::vector<FilamentSpoolLocation> fetchSpoolsForFilament(int filamentId) {
    std::vector<FilamentSpoolLocation> out;
    if (!_enabled) return out;

    if (_sessionCookie.length() == 0) {
      if (!login()) return out;
    }

    String url = "http://" + _host + ":" + String(_port) +
                 "/api/v1/spools?filament_id=" + String(filamentId) +
                 "&include_archived=false&page_size=100";

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
      // someone to an empty spool, and no point counting it as stock.
      if (remaining <= 1.0f) continue;

      int locId = item["location_id"] | -1;
      if (locId < 0) continue;

      out.push_back({locId, remaining});
    }

    return out;
  }

  // Sums spool count and remaining weight across all (non-empty, non-archived)
  // spools of a filament. Uses the RAW per-spool list (not merged by
  // location) since "2 Spulen, 2000g" should count actual spools, even if
  // several happen to sit at the same location.
  void aggregateSpoolStock(int filamentId, int& outCount, float& outTotalWeight) {
    std::vector<FilamentSpoolLocation> spools = fetchSpoolsForFilament(filamentId);
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
    int code = doGetOnce(url);

    // code=200 with an empty body usually means a transient hiccup (chunked
    // encoding quirk already handled via useHTTP10, or memory pressure from
    // something else running concurrently, e.g. an HTTPS update check) rather
    // than a real server error. One retry after a short pause is cheap
    // insurance against treating that as a hard failure.
    if (code == 200 && _lastBody.length() == 0) {
      if (CONFIGV2.system.debugMode) {
        Serial.println("[FILAMAN] GET returned 200 with empty body, retrying once...");
      }
      delay(300);
      code = doGetOnce(url);
    }

    return code;
  }

  int doGetOnce(const String& url) {
    HTTPClient http;
    http.begin(url);
    http.useHTTP10(true); // avoids chunked transfer-encoding, which getString() can return empty for on larger responses
    http.addHeader("Cookie", _sessionCookie);
    int code = http.GET();
    _lastBody = (code > 0) ? http.getString() : "";
    http.end();
    
    if (CONFIGV2.system.debugMode) {
      Serial.printf("[FILAMAN] GET %s -> code=%d, bodyLen=%d, free heap=%u bytes\n",
                     url.c_str(), code, _lastBody.length(), ESP.getFreeHeap());
    }
    return code;
  }

  bool _enabled;
  String _host;
  uint16_t _port;
  String _email, _password;
  String _sessionCookie;
  String _lastBody;
  std::vector<FilamanLocation> _locations;
};