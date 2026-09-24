// filaman_manager.cpp
#include "filaman_manager.h"
#include "FilamanClient.h"
#include "config.h"
#include <algorithm>
#include "i18n/i18n.h"
#include "ledctrl_filament.h"
#include "ledctrl_nfc.h"
#include "display/display_anim.h"

namespace FilamanManager {

static FilamanClient s_client;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

// Everything below is guarded by s_mux — single background task (producer)
// vs. loop() (consumer), so a spinlock is enough, no need for a full mutex.
static volatile bool s_busy = false;
static volatile bool s_resultReady = false;
static String s_resultUid;
static bool s_resultFound = false;
static String s_resultLocationName;
static std::vector<ResolvedLocation> s_resultLocations;

static volatile bool s_syncBusy = false;
static volatile bool s_syncReady = false;
static std::vector<FilamentSyncEntry> s_syncResult;
static bool s_syncSuccess = false;
static FilamentSyncSummary s_syncSummary;

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
  bool busy = s_busy || s_syncBusy;
  portEXIT_CRITICAL(&s_mux);
  return busy;
}

// --- Live location lookup for an already-known filament_id -----------------

struct LookupTaskParams {
  int filamentId;
  String uid;
};

static void lookupTask(void* param) {
  LookupTaskParams* p = reinterpret_cast<LookupTaskParams*>(param);
  int filamentId = p->filamentId;
  String uid = p->uid;
  delete p;

  std::vector<FilamentSpoolLocation> locations = s_client.findLocationsByFilamentId(filamentId);

  bool found = !locations.empty();
  String displayName;
  std::vector<ResolvedLocation> resolvedList;

  if (found) {
    // locations is sorted by remaining weight (most stock first).
    for (auto& loc : locations) {
      String name;
      if (s_client.resolveLocationName(loc.locationId, name)) {
        resolvedList.push_back({name, loc.remainingWeightG});
      }
      // Name couldn't be resolved -> that single entry is skipped rather
      // than discarding the whole result.
    }

    found = !resolvedList.empty(); // if ALL names failed to resolve, treat as not found

    if (found) {
      displayName = resolvedList[0].name;
      if (resolvedList.size() > 1) {
        displayName += " (+" + String(resolvedList.size() - 1) + " " + I18N::get("txt_more") + ")";
      }
    }
  }

  if (CONFIGV2.system.debugMode) {
    Serial.printf("[FILAMAN] location lookup for filament_id=%d: found=%d, display='%s'\n",
                   filamentId, found, displayName.c_str());
  }

  portENTER_CRITICAL(&s_mux);
  s_resultUid = uid;
  s_resultFound = found;
  s_resultLocationName = displayName;
  s_resultLocations = std::move(resolvedList);
  s_resultReady = true;
  s_busy = false;
  portEXIT_CRITICAL(&s_mux);

  vTaskDelete(nullptr);
}

bool requestLocationLookup(int filamentId, const String& uid) {
  if (!CONFIGV2.filamanConfig.enabled) {
    if (CONFIGV2.system.debugMode) {
      Serial.println("[FILAMAN] requestLocationLookup: skipped (FilaMan disabled)");
    }
    return false;
  }
  if (filamentId < 0) {
    if (CONFIGV2.system.debugMode) {
      Serial.println("[FILAMAN] requestLocationLookup: skipped (invalid filamentId)");
    }
    return false;
  }

  portENTER_CRITICAL(&s_mux);
  bool alreadyBusy = s_busy || s_syncBusy;
  if (!alreadyBusy) s_busy = true;
  portEXIT_CRITICAL(&s_mux);

  if (alreadyBusy) {
    if (CONFIGV2.system.debugMode) {
      Serial.println("[FILAMAN] requestLocationLookup: skipped (lookup/warmup/sync already in flight)");
    }
    return false;
  }

  LookupTaskParams* params = new LookupTaskParams{filamentId, uid};

  TaskHandle_t taskHandle = nullptr;
  BaseType_t ok = xTaskCreatePinnedToCore(
    lookupTask, "filaman_lookup", 8192, params, 1, &taskHandle, 0 /* core 0, alongside the WiFi/TCP stack */
  );

  if (ok != pdPASS) {
    delete params;
    portENTER_CRITICAL(&s_mux);
    s_busy = false;
    portEXIT_CRITICAL(&s_mux);
    return false;
  }
  return true;
}

bool pollResult(String& uid, bool& found, String& locationName, std::vector<ResolvedLocation>& locations) {
  portENTER_CRITICAL(&s_mux);
  bool ready = s_resultReady;
  if (ready) {
    uid = s_resultUid;
    found = s_resultFound;
    locationName = s_resultLocationName;
    locations = std::move(s_resultLocations);
    s_resultReady = false;
  }
  portEXIT_CRITICAL(&s_mux);
  return ready;
}

// --- Warmup ------------------------------------------------------------

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
    return false;
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

// --- Sync ----------------------------------------------------------------

static void syncTask(void* param) {
  (void)param;

  // Pause display/LED activity during FilaMan network operation
  DisplayAnim::stop();
  MYDISPLAY::clear();
  LEDCTRL_FILAMENT::standBy(true);
  LEDCTRL_NFC::standBy(true);
  MYDISPLAY::showCentered("FilaMan sync");

  std::vector<FilamentSyncEntry> result;
  FilamentSyncSummary summary;

  bool ok = s_client.fetchAllTaggedFilaments(result, summary);

  // Resume display/LED activity after FilaMan network operation
  LEDCTRL_FILAMENT::standBy(false);
  LEDCTRL_NFC::standBy(false);
  DisplayAnim::startIdle(millis());

  portENTER_CRITICAL(&s_mux);
  s_syncResult = std::move(result);
  s_syncSummary = std::move(summary);
  s_syncSuccess = ok;
  s_syncReady = true;
  s_syncBusy = false;
  portEXIT_CRITICAL(&s_mux);

  vTaskDelete(nullptr);
}

bool isSyncBusy() {
  portENTER_CRITICAL(&s_mux);
  bool busy = s_syncBusy || s_syncReady;
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

bool pollSyncResult(std::vector<FilamentSyncEntry>& entries, bool& success, FilamentSyncSummary& summary) {
  portENTER_CRITICAL(&s_mux);
  bool ready = s_syncReady;
  if (ready) {
    entries = std::move(s_syncResult);
    summary = std::move(s_syncSummary);
    success = s_syncSuccess;
    s_syncReady = false;
  }
  portEXIT_CRITICAL(&s_mux);
  return ready;
}

} // namespace FilamanManager