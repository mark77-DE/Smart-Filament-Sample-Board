// FilamanClient.h
// Talks to the FilaMan-System core API using cookie-based session auth
// (Viewer-role account, read-only usage — no CSRF token needed since we only do GET)
#pragma once
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include "config.h"

struct FilamanLocation
{
  int id;
  String name;
};

class FilamanClient
{
public:
  // Configured lazily via configure() so it can be re-applied whenever
  // the settings page saves a changed filamanConfig, without needing a reboot.
  FilamanClient() : _enabled(false), _port(0) {}

  void configure(bool enabled, const String &host, uint16_t port,
                 const String &email, const String &password)
  {
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
  bool login()
  {

    if (CONFIGV2.system.debugMode)
    {
      Serial.print("[FILAMAN] login");
    }

    if (!_enabled || _host.isEmpty())
      return false;

    WiFiClient client;
    if (!client.connect(_host.c_str(), _port))
    {
      return false;
    }

    JsonDocument doc;
    doc["email"] = _email;
    doc["password"] = _password;
    String body;
    serializeJson(doc, body);

    client.print(
        "POST /auth/login HTTP/1.1\r\n"
        "Host: " +
        _host + "\r\n" +
        "Content-Type: application/json\r\n" +
        "Content-Length: " + String(body.length()) + "\r\n" +
        "Connection: close\r\n\r\n" +
        body);

    String statusLine = client.readStringUntil('\n');
    bool ok = statusLine.indexOf("200") > 0;

    String line;
    _sessionCookie = "";
    while (client.connected() || client.available())
    {
      line = client.readStringUntil('\n');
      line.trim();
      if (line.length() == 0)
        break; // end of headers

      String lowerLine = line;
      lowerLine.toLowerCase();
      if (lowerLine.startsWith("set-cookie:") && line.indexOf("session_id=") >= 0)
      {
        int start = line.indexOf("session_id=");
        int end = line.indexOf(';', start);
        if (end < 0)
          end = line.length();
        _sessionCookie = line.substring(start, end); // e.g. "session_id=sess.18.xxx"

        if (CONFIGV2.system.debugMode)
        {
          Serial.print("[FILAMAN] session cookie: ");
          Serial.println(_sessionCookie);
        }
      }
    }
    client.stop();

    return ok && _sessionCookie.length() > 0;
  }

  // Looks up a spool by an exact match on custom_fields.sampleboard_uid.
  // The server-side `search` param is a full-text match and could return
  // false positives (e.g. the UID appearing as a substring elsewhere), so
  // every candidate is re-checked client-side before being accepted.
  // Returns true and fills locationId only on exactly one exact match.
  bool lookupByUid(const String &uid, int &locationId)
  {
    if (CONFIGV2.system.debugMode)
    {
      Serial.println("[FILAMAN] lookup by UID");
    }
    if (!_enabled)
      return false;

    if (_sessionCookie.length() == 0)
    {
      if (!login())
        return false;
    }

    String url = "http://" + _host + ":" + String(_port) +
                 "/api/v1/spools?search=" + urlEncode(uid);

    if (CONFIGV2.system.debugMode)
    {
      Serial.print("[FILAMAN] lookup url: ");
      Serial.println(url);
    }

    int code = doGet(url);
    if (code == 401)
    {
      if (!login())
        return false;
      code = doGet(url);
    }

    if (code != 200)
      return false;

    JsonDocument jsonDoc; // ArduinoJson v7
    if (deserializeJson(jsonDoc, _lastBody) != DeserializationError::Ok)
      return false;

    int matchCount = 0;
    int matchedLocationId = -1;

    for (JsonObject item : jsonDoc["items"].as<JsonArray>())
    {
      const char *fieldUid = item["custom_fields"]["sampleboard_uid"] | "";
      if (uid.equals(fieldUid))
      {
        matchCount++;
        matchedLocationId = item["location_id"] | -1;
      }
    }

    if (matchCount == 1 && matchedLocationId >= 0)
    {
      locationId = matchedLocationId;

      if (CONFIGV2.system.debugMode)
      {
        Serial.println("[FILAMAN] spool found");
      }

      return true;
    }
    // matchCount == 0 -> UID not registered on any spool
    // matchCount > 1  -> data issue (same UID used twice) -> caller should treat as error

    if (CONFIGV2.system.debugMode)
    {
      Serial.println("[FILAMAN] spool not found");
    }

    return false;
  }

  // Resolves a location_id to its display name (e.g. 5 -> "B1").
  // On a cache miss, refreshes once from the server and retries — locations
  // are still being added while the shelving is being built out, so a miss
  // is the natural signal that the list needs reloading, no fixed TTL needed.
  bool resolveLocationName(int locationId, String &outName)
  {

    if (CONFIGV2.system.debugMode)
    {
      Serial.println("[FILAMAN] resolve location");
      Serial.printf("  [FILAMAN] ID  = '%d' - name: '%s'\n", locationId, outName.c_str());
    }

    if (!_enabled)
      return false;

    if (findCached(locationId, outName))
      return true;

    if (!refreshLocations())
    {
      return false;
    }

    return findCached(locationId, outName);
  }

private:
  bool findCached(int locationId, String &outName)
  {
    for (auto &loc : _locations)
    {
      if (loc.id == locationId)
      {
        outName = loc.name;
        return true;
      }
    }
    return false;
  }

  bool refreshLocations()
  {
    if (_sessionCookie.length() == 0)
    {
      if (!login())
        return false;
    }

    String url = "http://" + _host + ":" + String(_port) + "/api/v1/locations?page_size=100";
    int code = doGet(url);
    if (code == 401)
    {
      if (!login())
        return false;
      code = doGet(url);
    }
    if (code != 200)
      return false;

    JsonDocument jsonDoc; // ArduinoJson v7
    if (deserializeJson(jsonDoc, _lastBody) != DeserializationError::Ok)
      return false;

    _locations.clear();
    for (JsonObject item : jsonDoc["items"].as<JsonArray>())
    {
      FilamanLocation loc;
      loc.id = item["id"] | -1;
      loc.name = item["name"] | "";
      if (loc.id >= 0)
        _locations.push_back(loc);
    }
    return !_locations.empty();
  }

  int doGet(const String &url)
  {
    HTTPClient http;
    http.begin(url);
    http.addHeader("Cookie", _sessionCookie);
    int code = http.GET();
    _lastBody = (code > 0) ? http.getString() : "";
    http.end();
    return code;
  }

  String urlEncode(const String &s)
  {
    String out;
    for (char c : s)
    {
      if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
      {
        out += c;
      }
      else
      {
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
