/*
 * Persisted-settings (NVS) schema versioning + migration
 *
 * Purpose: make firmware updates safe for data already stored on a customer's
 * device. Without a version hook, a future change that alters how a value is
 * stored (a key's type/meaning, a putBytes() struct layout, or a namespace's
 * purpose) would silently misread old data -> corrupted settings or boot loops.
 *
 * How it works:
 *   - SETTINGS_SCHEMA_VERSION (config.h) is the current schema version.
 *   - The device stores the version it last wrote in the "sysmeta" namespace.
 *   - runSettingsMigrations() runs once at boot, BEFORE any settings load. If
 *     the stored version is behind, it runs the registered migration steps in
 *     order, then stamps the current version.
 *
 * When do you need a migration?  Most settings here use per-key typed gets with
 * defaults (e.g. getInt("k", default)), which read missing keys as the default,
 * and the few putBytes() blobs size-guard their loads. So ADDING keys or new
 * namespaces never needs a migration. You only need one for a BREAKING change:
 *   - a key changes type (was putInt, now putString) or meaning (units, range)
 *   - a putBytes() struct changes layout but keeps the same size (size guard
 *     can't catch it)
 *   - a namespace is renamed or repurposed and stale data must be cleared
 *
 * To add a migration:
 *   1. Bump SETTINGS_SCHEMA_VERSION in config.h (e.g. 1 -> 2).
 *   2. Write a migrateToV2() that transforms or clears the affected data.
 *   3. Add `if (stored < 2) migrateToV2();` to the ladder in runSettingsMigrations().
 *   Keep steps small and idempotent.
 */

#ifndef SETTINGS_MIGRATION_H
#define SETTINGS_MIGRATION_H

#include <Arduino.h>
#include <Preferences.h>
#include <nvs_flash.h>
#include "config.h"

#define SETTINGS_META_NS  "sysmeta"
#define SETTINGS_VER_KEY  "schemaVer"

// Clear (wipe all keys in) a set of NVS namespaces. Shared by migrations that
// need to discard stale data and by the factory-reset path. Safe to call on a
// namespace that doesn't exist yet.
inline void clearNvsNamespaces(const char* const* namespaces, int count) {
  Preferences p;
  for (int i = 0; i < count; i++) {
    if (p.begin(namespaces[i], false)) {  // read-write
      p.clear();
      p.end();
    }
  }
}

// Full factory reset: erase the ENTIRE NVS partition - every settings/progress
// namespace plus the WiFi credentials the ESP32 stores there - and reboot to an
// out-of-box state. Does NOT touch SD-card web assets (those re-download). This
// is irreversible; gate it behind a confirmation in the UI. Also reachable as a
// recovery gesture (hold both paddles at power-on) so a misconfigured device can
// be recovered even if the UI won't load.
inline void factoryReset() {
  Serial.println("[Settings] FACTORY RESET - erasing all NVS, rebooting...");
  delay(100);             // let the serial line flush
  nvs_flash_deinit();     // release NVS handles so the erase proceeds cleanly
  esp_err_t err = nvs_flash_erase();
  Serial.printf("[Settings] nvs_flash_erase result: %d\n", (int)err);
  delay(100);
  ESP.restart();
}

// Recovery gesture: if BOTH paddles are held at power-on, hold them for 3s to
// trigger a factory reset. Display-independent so it works even when normal boot
// fails. Call very early in setup(), before heavy init. No-op if paddles aren't
// both held at entry. Returns true if a reset was triggered (never returns then,
// since it reboots).
inline void checkFactoryResetGesture(int ditPin, int dahPin, int activeLevel) {
  pinMode(ditPin, INPUT_PULLUP);
  pinMode(dahPin, INPUT_PULLUP);
  if (digitalRead(ditPin) != activeLevel || digitalRead(dahPin) != activeLevel) return;

  Serial.println("[Boot] Both paddles held - keep holding 3s for FACTORY RESET...");
  unsigned long start = millis();
  while (millis() - start < 3000) {
    if (digitalRead(ditPin) != activeLevel || digitalRead(dahPin) != activeLevel) {
      Serial.println("[Boot] Paddles released - factory reset cancelled");
      return;
    }
    delay(50);
  }
  Serial.println("[Boot] Factory reset confirmed by paddle hold");
  factoryReset();  // erases NVS and reboots; does not return
}

// Log NVS partition health at boot. The Preferences API swallows write errors,
// so a full or degraded NVS partition silently stops settings from persisting -
// this report makes that failure mode visible in any tester's serial log.
inline void logNvsStats() {
  nvs_stats_t stats;
  esp_err_t err = nvs_get_stats(NULL, &stats);
  if (err != ESP_OK) {
    Serial.printf("[Settings] WARNING: nvs_get_stats failed: %d\n", (int)err);
    return;
  }
  Serial.printf("[Settings] NVS: %d/%d entries used (%d free), %d namespaces\n",
                stats.used_entries, stats.total_entries, stats.free_entries,
                stats.namespace_count);
  if (stats.free_entries < 32) {
    Serial.println("[Settings] WARNING: NVS nearly full - settings writes may start failing!");
  }
}

// Read the schema version currently stored on the device (0 = unversioned).
inline int getStoredSettingsVersion() {
  Preferences meta;
  meta.begin(SETTINGS_META_NS, true);  // read-only
  int v = meta.getInt(SETTINGS_VER_KEY, 0);
  meta.end();
  return v;
}

// Stamp the current schema version into NVS.
inline void writeSettingsVersion(int version) {
  Preferences meta;
  meta.begin(SETTINGS_META_NS, false);  // read-write
  meta.putInt(SETTINGS_VER_KEY, version);
  meta.end();
}

// ---------------------------------------------------------------------------
// Migration steps. Each migrateToVN() upgrades stored data FROM version N-1 TO
// version N.
// ---------------------------------------------------------------------------

// v1 -> v2: consolidate the large per-key stat arrays into single putBytes()
// blobs. The per-key layout burned ~300 of the ~504 usable entries in the 20KB
// NVS partition; a full partition makes every put() fail silently, which users
// saw as "settings don't persist across reboots". Existing data is preserved.
// Idempotent: each block converts only if the old key layout is present.
static void migrateToV2() {
  char key[12];

  // licwstats: cc%d/ct%d/ttr%d/ttrc%d (4 x 44 keys) -> blobs cc/ct/ttr/ttrc
  {
    Preferences p;
    if (p.begin("licwstats", false)) {
      if (p.isKey("cc0")) {
        int32_t cc[44], ct[44], ttrc[44];
        uint32_t ttr[44];
        for (int i = 0; i < 44; i++) {
          snprintf(key, sizeof(key), "cc%d", i);   cc[i]   = p.getInt(key, 0);
          snprintf(key, sizeof(key), "ct%d", i);   ct[i]   = p.getInt(key, 0);
          snprintf(key, sizeof(key), "ttr%d", i);  ttr[i]  = p.getULong(key, 0);
          snprintf(key, sizeof(key), "ttrc%d", i); ttrc[i] = p.getInt(key, 0);
        }
        p.clear();  // namespace holds only these arrays
        p.putBytes("cc", cc, sizeof(cc));
        p.putBytes("ct", ct, sizeof(ct));
        p.putBytes("ttr", ttr, sizeof(ttr));
        p.putBytes("ttrc", ttrc, sizeof(ttrc));
        Serial.println("[Settings] v2: licwstats consolidated to blobs");
      }
      p.end();
    }
  }

  // vcmastery: m%d/a%d/c%d (3 x 40 keys) -> blobs m/a/c
  {
    Preferences p;
    if (p.begin("vcmastery", false)) {
      if (p.isKey("m0")) {
        int32_t m[40], a[40], c[40];
        for (int i = 0; i < 40; i++) {
          snprintf(key, sizeof(key), "m%d", i); m[i] = p.getInt(key, 0);
          snprintf(key, sizeof(key), "a%d", i); a[i] = p.getInt(key, 0);
          snprintf(key, sizeof(key), "c%d", i); c[i] = p.getInt(key, 0);
        }
        p.clear();  // namespace holds only these arrays
        p.putBytes("m", m, sizeof(m));
        p.putBytes("a", a, sizeof(a));
        p.putBytes("c", c, sizeof(c));
        Serial.println("[Settings] v2: vcmastery consolidated to blobs");
      }
      p.end();
    }
  }

  // vailcourse: lc%d (12 keys) -> blob lc. Other keys in this namespace
  // (module/lesson/unlocked/...) are untouched, so remove per-key, no clear().
  {
    Preferences p;
    if (p.begin("vailcourse", false)) {
      if (p.isKey("lc0")) {
        uint32_t lc[12];
        for (int i = 0; i < 12; i++) {
          snprintf(key, sizeof(key), "lc%d", i);
          lc[i] = (uint32_t)p.getInt(key, 0);
          p.remove(key);
        }
        p.putBytes("lc", lc, sizeof(lc));
        Serial.println("[Settings] v2: vailcourse lessons consolidated to blob");
      }
      p.end();
    }
  }
}

/*
 * Run any needed settings migrations. Call ONCE in setup(), early, before the
 * individual loadXxx() settings functions run, so stale data is fixed before it
 * is read.
 */
// Set when boot finds an unversioned (fresh or wiped) NVS - the SD settings
// backup uses this as the signal to offer a restore (see settings_sd_backup.h).
bool g_nvsWasUnversioned = false;

// True when runSettingsMigrations() will change stored data or stamp a new
// version this boot - the caller takes a pre-migration SD snapshot first.
inline bool settingsMigrationPending() {
  return getStoredSettingsVersion() != SETTINGS_SCHEMA_VERSION;
}

inline void runSettingsMigrations() {
  int stored = getStoredSettingsVersion();

  if (stored == SETTINGS_SCHEMA_VERSION) {
    return;  // up to date, nothing to do
  }

  if (stored == 0) {
    // Unversioned NVS: either a fresh flash, or an existing in-field/dev unit
    // whose data predates this framework. Don't wipe - run the (idempotent)
    // migration steps so any pre-framework per-key data is converted, then
    // stamp the current version going forward.
    Serial.println("[Settings] Unversioned NVS - migrating any legacy data, stamping schema");
    g_nvsWasUnversioned = true;
    migrateToV2();
  } else if (stored < SETTINGS_SCHEMA_VERSION) {
    Serial.printf("[Settings] Migrating NVS schema v%d -> v%d\n",
                  stored, SETTINGS_SCHEMA_VERSION);
    // Run each step whose target version is ahead of the stored version, in
    // order. Add steps here as the schema evolves:
    //   if (stored < 3) migrateToV3();
    if (stored < 2) migrateToV2();
  } else {
    // stored > current: the device previously ran NEWER firmware (a downgrade).
    // Leave the data as-is - older code simply ignores keys it doesn't know.
    // Wiping here would punish a user for rolling back, so we don't.
    Serial.printf("[Settings] NVS schema v%d is newer than firmware (v%d) - leaving as-is\n",
                  stored, SETTINGS_SCHEMA_VERSION);
  }

  writeSettingsVersion(SETTINGS_SCHEMA_VERSION);
}

#endif // SETTINGS_MIGRATION_H
