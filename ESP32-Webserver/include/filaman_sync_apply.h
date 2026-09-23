// filaman_sync_apply.h / .cpp would normally be split, kept together here
// for easy review — split into a header if you prefer.
#pragma once
#include <Arduino.h>
#include <vector>
#include "FilamanClient.h" // for FilamentSyncEntry
#include "filament_db.h"
#include "filehandling.h"  // saveFilamentsToFile()
#include "config.h"
#include "globals.h"        // g_reloadFilamentsPending

// Result of merging one sync run into the local FilamentDB — lets the caller
// (e.g. a WebSocket broadcast to the WebIF) report what actually happened,
// not just that a sync ran.
struct FilamanSyncApplyResult {
  int updated = 0;
  int added = 0;
  int skipped = 0; // local DB was at its 150-entry cap
};

// Merges FilaMan sync results into the local FilamentDB:
// - known UIDs are updated (vendor/type/color/ledIndex/info1/info2)
// - `storage` is deliberately left untouched — it's looked up live via
//   FilaMan instead, never synced into the local cache (see the AMS/Bambuddy
//   location-drift discussion)
// - unknown UIDs are added as new entries
// - entries NOT present in the sync (e.g. purely local/manual ones) are left
//   alone — this is additive/merge, never a full replace
inline FilamanSyncApplyResult applyFilamanSyncToLocalDb(const std::vector<FilamentSyncEntry>& syncEntries)
{
  FilamanSyncApplyResult result;

  for (const auto &s : syncEntries)
  {
    FilamentEntry entry;
    bool exists = FilamentDB::findByUID(s.uid, entry);

    entry.uid = s.uid;
    entry.filamentId = s.filamentId;
    entry.vendor = s.vendor;
    entry.type = s.type;
    entry.color = s.color;
    if (s.ledIndex >= 0)
    {
      entry.ledIndex = (uint16_t)s.ledIndex;
    }
    entry.info1 = String(s.spoolCount) + " Spulen, " + String(s.totalRemainingWeightG, 0) + "g";
    entry.info2 = s.shopUrl;
    // entry.storage: untouched on update (kept from the existing entry),
    // stays empty on a brand-new entry — the live FilaMan lookup covers it.

    if (exists)
    {
      FilamentDB::update(entry);
      result.updated++;
    }
    else
    {
      if (FilamentDB::add(entry))
      {
        result.added++;
      }
      else
      {
        result.skipped++; // local DB is at its 150-entry cap
      }
    }
  }

  saveFilamentsToFile();
  g_reloadFilamentsPending = true;

  if (CONFIGV2.system.debugMode)
  {
    Serial.printf("[FILAMAN] sync applied: %d updated, %d added, %d skipped (DB full), total now %d\n",
                   result.updated, result.added, result.skipped, FilamentDB::getAllCount());
  }

  return result;
}