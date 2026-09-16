// filaman_manager.cpp
#include "filaman_manager.h"
#include "FilamanClient.h"
#include "config.h"

namespace FilamanManager {

static FilamanClient s_client;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

// Everything below is guarded by s_mux — single background task (producer)
// vs. loop() (consumer), so a spinlock is enough, no need for a full mutex.
static volatile bool s_busy = false;
static volatile bool s_syncBusy = false;
static volatile bool s_syncReady = false;
static std::vector<FilamentSyncEntry> s_syncResult;
static bool s_syncSuccess = false;
static volatile bool s_resultReady = false;
static String s_resultUid;
static bool s_resultFound = false;
static String s_resultLocationName;

void applyConfig() {
  s_client.configure(
    CONFIGV2.filamanConfig.enabled,
    CONFIGV2.filamanConfig.server,
    (uint16_t)CONFIGV2.filamanConfig.port,
    CONFIGV2.filamanConfig.user,
    CONFIGV2.filamanConfig.password
  );
}

bool isBusy() {
  portENTER_CRITICAL(&s_mux);
  bool busy = s_busy;
  portEXIT_CRITICAL(&s_mux);
  return busy;
}

// Runs on its own task/stack. Does the blocking login+lookup+resolve work,
// then hands the result back to loop() via the guarded result fields above.
static void lookupTask(void* param) {
  String* uidPtr = reinterpret_cast<String*>(param);
  String uid = *uidPtr;
  delete uidPtr;

  FilamentLocationResult result = s_client.lookupLocationsByUid(uid);

  String displayName;
  if (result.found) {
    // result.locations is sorted by remaining weight (most stock first).
    // Resolve the primary location's name, just count the rest.
    String primaryName;
    if (s_client.resolveLocationName(result.locations[0].locationId, primaryName)) {
      displayName = primaryName;
      if (result.locations.size() > 1) {
        displayName += " (+" + String(result.locations.size() - 1) + " weitere)";
      }
    } else {
      result.found = false; // name couldn't be resolved -> treat as not found
    }
  }

  if (CONFIGV2.system.debugMode) {
    Serial.printf("[FILAMAN] lookup result for %s: found=%d, display='%s'\n",
                   uid.c_str(), result.found, displayName.c_str());
  }

  portENTER_CRITICAL(&s_mux);
  s_resultUid = uid;
  s_resultFound = result.found;
  s_resultLocationName = displayName;
  s_resultReady = true;
  s_busy = false;
  portEXIT_CRITICAL(&s_mux);

  vTaskDelete(nullptr);
}

static void warmupTask(void* param) {
  (void)param;
  s_client.warmup();

  portENTER_CRITICAL(&s_mux);
  s_busy = false;
  portEXIT_CRITICAL(&s_mux);

  vTaskDelete(nullptr);
}

bool requestWarmup() {
  if (!CONFIGV2.filamanConfig.enabled) {
    if (CONFIGV2.system.debugMode) {
      Serial.println("[FILAMAN] requestWarmup: skipped (FilaMan disabled)");
    }
    return false;
  }

  portENTER_CRITICAL(&s_mux);
  bool alreadyBusy = s_busy || s_syncBusy;
  if (!alreadyBusy) s_busy = true;
  portEXIT_CRITICAL(&s_mux);

  if (alreadyBusy) {
    if (CONFIGV2.system.debugMode) {
      Serial.println("[FILAMAN] requestWarmup: skipped (lookup/warmup/sync already in flight)");
    }
    return false; // a real lookup or sync is running, don't compete with it
  }

  TaskHandle_t taskHandle = nullptr;
  BaseType_t ok = xTaskCreatePinnedToCore(
    warmupTask, "filaman_warmup", 8192, nullptr, 1, &taskHandle, 0
  );

  if (ok != pdPASS) {
    portENTER_CRITICAL(&s_mux);
    s_busy = false;
    portEXIT_CRITICAL(&s_mux);
    return false;
  }
  return true;
}

static void syncTask(void* param) {
  (void)param;
  std::vector<FilamentSyncEntry> result;
  bool ok = s_client.fetchAllTaggedFilaments(result);

  portENTER_CRITICAL(&s_mux);
  s_syncResult = std::move(result); // move, not copy — cheap even under a spinlock
  s_syncSuccess = ok;
  s_syncReady = true;
  s_syncBusy = false;
  portEXIT_CRITICAL(&s_mux);

  vTaskDelete(nullptr);
}

bool isSyncBusy() {
  portENTER_CRITICAL(&s_mux);
  bool busy = s_syncBusy;
  portEXIT_CRITICAL(&s_mux);
  return busy;
}

bool requestSync() {
  if (!CONFIGV2.filamanConfig.enabled) {
    if (CONFIGV2.system.debugMode) {
      Serial.println("[FILAMAN] requestSync: skipped (FilaMan disabled)");
    }
    return false;
  }

  // s_client is a single shared instance used by lookup/warmup/sync alike —
  // only one of them may touch it at a time, so all three share this check.
  portENTER_CRITICAL(&s_mux);
  bool alreadyBusy = s_busy || s_syncBusy;
  if (!alreadyBusy) s_syncBusy = true;
  portEXIT_CRITICAL(&s_mux);

  if (alreadyBusy) {
    if (CONFIGV2.system.debugMode) {
      Serial.println("[FILAMAN] requestSync: skipped (lookup/warmup/sync already in flight)");
    }
    return false;
  }

  if (CONFIGV2.system.debugMode) {
    Serial.println("[FILAMAN] requestSync: starting background sync task");
  }

  // Bigger stack than lookup/warmup: pagination + JSON parsing over up to
  // ~1300 filaments, plus a nested spool-aggregation request per tagged one.
  TaskHandle_t taskHandle = nullptr;
  BaseType_t ok = xTaskCreatePinnedToCore(
    syncTask, "filaman_sync", 12288, nullptr, 1, &taskHandle, 0
  );

  if (ok != pdPASS) {
    portENTER_CRITICAL(&s_mux);
    s_syncBusy = false;
    portEXIT_CRITICAL(&s_mux);
    return false;
  }
  return true;
}

bool pollSyncResult(std::vector<FilamentSyncEntry>& entries, bool& success) {
  portENTER_CRITICAL(&s_mux);
  bool ready = s_syncReady;
  if (ready) {
    entries = std::move(s_syncResult);
    success = s_syncSuccess;
    s_syncReady = false;
  }
  portEXIT_CRITICAL(&s_mux);
  return ready;
}

bool requestLookup(const String& uid) {
  if (!CONFIGV2.filamanConfig.enabled) return false;

  portENTER_CRITICAL(&s_mux);
  bool alreadyBusy = s_busy || s_syncBusy;
  if (!alreadyBusy) s_busy = true;
  portEXIT_CRITICAL(&s_mux);

  if (alreadyBusy) return false; // keep it simple: one thing using s_client at a time

  // Heap-allocated copy; the task takes ownership and frees it itself.
  String* uidCopy = new String(uid);

  TaskHandle_t taskHandle = nullptr;
  BaseType_t ok = xTaskCreatePinnedToCore(
    lookupTask, "filaman_lookup", 8192, uidCopy, 1, &taskHandle, 0 /* core 0, alongside the WiFi/TCP stack */
  );

  if (ok != pdPASS) {
    delete uidCopy;
    portENTER_CRITICAL(&s_mux);
    s_busy = false;
    portEXIT_CRITICAL(&s_mux);
    return false;
  }
  return true;
}

bool pollResult(String& uid, bool& found, String& locationName) {
  portENTER_CRITICAL(&s_mux);
  bool ready = s_resultReady;
  if (ready) {
    uid = s_resultUid;
    found = s_resultFound;
    locationName = s_resultLocationName;
    s_resultReady = false;
  }
  portEXIT_CRITICAL(&s_mux);
  return ready;
}

} // namespace FilamanManager