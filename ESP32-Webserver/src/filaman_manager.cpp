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

  int locationId = -1;
  String locationName;
  bool found = false;

  if (s_client.lookupByUid(uid, locationId)) {
    found = s_client.resolveLocationName(locationId, locationName);
  }

  portENTER_CRITICAL(&s_mux);
  s_resultUid = uid;
  s_resultFound = found;
  s_resultLocationName = locationName;
  s_resultReady = true;
  s_busy = false;
  portEXIT_CRITICAL(&s_mux);

  vTaskDelete(nullptr);
}

bool requestLookup(const String& uid) {

  if(CONFIGV2.system.debugMode) {
    Serial.print("[FILAMAN] request lookup uid: ");
    Serial.println(uid);
  }

  if (!CONFIGV2.filamanConfig.enabled) return false;

  portENTER_CRITICAL(&s_mux);
  bool alreadyBusy = s_busy;
  if (!alreadyBusy) s_busy = true;
  portEXIT_CRITICAL(&s_mux);

  if (alreadyBusy) return false; // keep it simple: one lookup in flight at a time

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
