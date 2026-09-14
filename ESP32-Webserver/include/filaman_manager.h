// filaman_manager.h
// Non-blocking wrapper around FilamanClient: runs the lookup on a background
// FreeRTOS task so handleUID()/loop() never wait on the network round trip.
#pragma once
#include <Arduino.h>

namespace FilamanManager {

  // Call once after CONFIGV2 is loaded/changed (e.g. from applyConfigV2()),
  // so the client picks up the current filamanConfig without a reboot.
  void applyConfig();

  // Starts a background lookup for `uid`. Returns immediately (non-blocking).
  // Returns false without doing anything if FilaMan is disabled in the config
  // or a lookup is already in flight (kept deliberately simple: one at a time).
  bool requestLookup(const String& uid);

  // Call once per loop() iteration. Returns true exactly once when a result
  // becomes available. `uid` tells you which tag this result belongs to —
  // always compare it against whatever tag is currently being displayed
  // before acting on it, since the tag may have changed/been removed while
  // the lookup was still running in the background.
  bool pollResult(String& uid, bool& found, String& locationName);

  // True while a lookup is currently running (e.g. to show a "searching..." state).
  bool isBusy();

} // namespace FilamanManager
