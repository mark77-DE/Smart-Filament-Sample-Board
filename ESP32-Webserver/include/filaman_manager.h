// filaman_manager.h
// Non-blocking wrapper around FilamanClient: runs lookups/sync on a
// background FreeRTOS task so handleUID()/loop() never wait on the network
// round trip. Identity (vendor/type/color/ledIndex) comes entirely from the
// synced local FilamentDB — this manager's live job is only refreshing the
// location for an already-known filament_id, plus the periodic/manual sync
// itself.
#pragma once
#include <Arduino.h>
#include <vector>
#include "FilamanClient.h" // for FilamentSyncEntry

namespace FilamanManager {

  // Call once after CONFIGV2 is loaded/changed (e.g. from applyConfigV2()),
  // so the client picks up the current filamanConfig without a reboot.
  void applyConfig();

  // Starts a background live location lookup for a filament that's already
  // known locally (filamentId comes from FilamentDB, populated during sync).
  // `uid` is only carried through so the caller can later check the result
  // still belongs to the tag currently being displayed (NFC::currentHoldUid()).
  // Returns immediately (non-blocking). Returns false without doing anything
  // if FilaMan is disabled, filamentId is invalid, or a lookup/warmup/sync is
  // already in flight (kept deliberately simple: one thing at a time).
  bool requestLocationLookup(int filamentId, const String& uid);

  // Call once per loop() iteration. Returns true exactly once when a result
  // becomes available. `uid` tells you which tag this result belongs to —
  // always compare it against whatever tag is currently being displayed
  // before acting on it, since the tag may have changed/been removed while
  // the lookup was still running in the background.
  bool pollResult(String& uid, bool& found, String& locationName);

  // Non-blocking: logs in and pre-fetches the location cache in the background.
  // Call once after WiFi connects, and optionally on a periodic timer
  // afterwards. Shares the same "one background task at a time" slot as
  // requestLocationLookup(), so it's simply skipped if a real lookup is in flight.
  bool requestWarmup();

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
  // from a background task). `summary` gives the headline numbers (scanned/
  // tagged/spools found/tagged-without-spools) for a status line or log,
  // without having to derive them from `entries` yourself.
  bool pollSyncResult(std::vector<FilamentSyncEntry>& entries, bool& success, FilamentSyncSummary& summary);

  // True while a lookup, warmup, or sync is currently running (e.g. to show
  // a "busy" state).
  bool isBusy();

} // namespace FilamanManager