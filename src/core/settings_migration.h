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
// version N. None exist yet (we are at schema v1, the baseline). Examples:
//
//   static void migrateToV2() {
//     // A renamed namespace whose old data is meaningless under the new code:
//     const char* dead[] = { "oldns" };
//     clearNvsNamespaces(dead, 1);
//   }
// ---------------------------------------------------------------------------

/*
 * Run any needed settings migrations. Call ONCE in setup(), early, before the
 * individual loadXxx() settings functions run, so stale data is fixed before it
 * is read.
 */
inline void runSettingsMigrations() {
  int stored = getStoredSettingsVersion();

  if (stored == SETTINGS_SCHEMA_VERSION) {
    return;  // up to date, nothing to do
  }

  if (stored == 0) {
    // Unversioned NVS: either a fresh flash, or an existing in-field/dev unit
    // whose data predates this framework. The current per-key schema reads such
    // data compatibly (missing keys -> defaults, blobs are size-guarded), so we
    // must NOT wipe it - just stamp the current version going forward.
    Serial.println("[Settings] Unversioned NVS - stamping current schema (no data wipe)");
  } else if (stored < SETTINGS_SCHEMA_VERSION) {
    Serial.printf("[Settings] Migrating NVS schema v%d -> v%d\n",
                  stored, SETTINGS_SCHEMA_VERSION);
    // Run each step whose target version is ahead of the stored version, in
    // order. Add steps here as the schema evolves:
    //   if (stored < 2) migrateToV2();
    //   if (stored < 3) migrateToV3();
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
