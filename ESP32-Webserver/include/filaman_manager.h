// filaman_manager.h
// Non-blocking wrapper around FilamanClient: runs the lookup on a background
// FreeRTOS task so handleUID()/loop() never wait on the network round trip.
#pragma once
#include <Arduino.h>
#include <vector>
#include "FilamanClient.h" // for FilamentSyncEntry

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

  // Non-blocking: logs in and pre-fetches the location cache in the background.
  // Call once after WiFi connects, and optionally on a periodic timer
  // afterwards. Shares the same "one background task at a time" slot as
  // requestLookup(), so it's simply skipped if a real lookup is in flight.
  bool requestWarmup();

  // True while a lookup or warmup is currently running (e.g. to show a "searching..." state).
  bool isBusy();

  // Non-blocking: syncs all filaments tagged with sampleboard_uid from FilaMan
  // into a background-collected list. Meant to be triggered rarely (WebIF
  // button / hardware button), not automatically — pulling ~1300 filaments
  // takes many requests. Runs on its own task, independent of the lookup/
  // warmup busy flag (but they share the same FilamanClient instance
  // internally, so a sync won't start while a lookup/warmup is in flight,
  // and vice versa).
  bool requestSync();

  // True while a sync is currently running.
  bool isSyncBusy();

  // Call once per loop() iteration. Returns true exactly once when a sync
  // run has finished. `entries` holds everything found (already includes
  // per-filament spool count/weight); merging them into FilamentDB is left
  // to the caller (single-threaded in loop(), to avoid touching FilamentDB
  // from a background task).
  bool pollSyncResult(std::vector<FilamentSyncEntry>& entries, bool& success);

} // namespace FilamanManager