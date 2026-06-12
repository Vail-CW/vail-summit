/*
 * SD-Card NVS Backup / Restore
 *
 * Mirrors the ENTIRE app NVS partition (settings AND training/game progress -
 * every namespace, every key) to a JSON-lines file on the SD card, and
 * restores it at boot when NVS comes up empty (wiped, corrupt, or erased).
 * Belt-and-braces against silent NVS write failures and flash corruption.
 *
 * Files:
 *   /nvs_backup.jsonl        rolling mirror, rewritten on triggers below
 *   /nvs_premigration.jsonl  one-shot snapshot taken right before a schema
 *                            migration runs (disaster recovery for migrations)
 *
 * Triggers (all polled from loop(), skipped during audio-critical modes at the
 * call site so SD writes never crunch active audio):
 *   - ~60s after boot (captures the previous session's progress)
 *   - 5s after any core setting stops changing (snapshot diff)
 *   - every 10 minutes (catch-all for progress saved by games/training)
 *
 * Format: one JSON object per line: {"ns":..,"k":..,"t":<nvs_type>,"v":..}
 * (blobs hex-encoded). Streamed line-by-line in both directions, so neither
 * dump nor restore needs a large in-RAM document.
 *
 * Excluded from the dump:
 *   - "wifi" and "webpw" namespaces - credentials would sit in plaintext on a
 *     removable card; the user re-enters them after a restore
 *   - radio-stack namespaces (nvs.net80211, phy*) - device-generated, they
 *     regenerate on their own and restoring them can confuse the drivers.
 */

#ifndef SETTINGS_SD_BACKUP_H
#define SETTINGS_SD_BACKUP_H

#include <ArduinoJson.h>
#include <nvs.h>
#include <nvs_flash.h>
#include "../core/config.h"
#include "../core/settings_migration.h"
#include "../settings/settings_decoder.h"
#include "../storage/sd_card.h"

#define NVS_BACKUP_PATH             "/nvs_backup.jsonl"
#define NVS_PREMIGRATION_PATH       "/nvs_premigration.jsonl"
#define LEGACY_SETTINGS_BACKUP_PATH "/settings_backup.json"
#define SETTINGS_BACKUP_DEBOUNCE_MS 5000
#define NVS_BACKUP_PERIOD_MS        (10UL * 60UL * 1000UL)
#define NVS_BACKUP_BOOT_DELAY_MS    60000UL

// Core-settings globals used only as the fast change-detection trigger
// (the dump itself reads NVS directly and covers everything)
extern int cwSpeed;
extern int cwTone;
int getCwKeyTypeAsInt();
int getVolume();
int getBrightness();
extern String vailCallsign;

// Namespaces that must never be dumped or restored: radio-stack internals
// (regenerate on their own) and credentials (plaintext on a removable card).
static bool isSystemNvsNamespace(const char* ns) {
  return strcmp(ns, "nvs.net80211") == 0 || strncmp(ns, "phy", 3) == 0 ||
         strcmp(ns, "wifi") == 0 || strcmp(ns, "webpw") == 0;
}

// ---------------------------------------------------------------------------
// Dump: NVS -> SD
// ---------------------------------------------------------------------------

// Serialize one NVS entry as a JSON line. Returns false (and writes nothing)
// if the value can't be read.
static bool writeNvsEntryLine(File& f, const nvs_entry_info_t& info) {
  nvs_handle_t h;
  if (nvs_open(info.namespace_name, NVS_READONLY, &h) != ESP_OK) return false;

  JsonDocument doc;
  doc["ns"] = info.namespace_name;
  doc["k"]  = info.key;
  doc["t"]  = (int)info.type;

  bool ok = true;
  switch (info.type) {
    case NVS_TYPE_U8:  { uint8_t  v; ok = nvs_get_u8(h, info.key, &v)  == ESP_OK; if (ok) doc["v"] = v; break; }
    case NVS_TYPE_I8:  { int8_t   v; ok = nvs_get_i8(h, info.key, &v)  == ESP_OK; if (ok) doc["v"] = v; break; }
    case NVS_TYPE_U16: { uint16_t v; ok = nvs_get_u16(h, info.key, &v) == ESP_OK; if (ok) doc["v"] = v; break; }
    case NVS_TYPE_I16: { int16_t  v; ok = nvs_get_i16(h, info.key, &v) == ESP_OK; if (ok) doc["v"] = v; break; }
    case NVS_TYPE_U32: { uint32_t v; ok = nvs_get_u32(h, info.key, &v) == ESP_OK; if (ok) doc["v"] = v; break; }
    case NVS_TYPE_I32: { int32_t  v; ok = nvs_get_i32(h, info.key, &v) == ESP_OK; if (ok) doc["v"] = v; break; }
    case NVS_TYPE_U64: { uint64_t v; ok = nvs_get_u64(h, info.key, &v) == ESP_OK; if (ok) doc["v"] = v; break; }
    case NVS_TYPE_I64: { int64_t  v; ok = nvs_get_i64(h, info.key, &v) == ESP_OK; if (ok) doc["v"] = v; break; }
    case NVS_TYPE_STR: {
      size_t len = 0;
      ok = (nvs_get_str(h, info.key, NULL, &len) == ESP_OK) && len > 0;
      if (ok) {
        char* buf = (char*)malloc(len);
        ok = (buf != NULL) && (nvs_get_str(h, info.key, buf, &len) == ESP_OK);
        if (ok) doc["v"] = buf;  // ArduinoJson copies
        free(buf);
      }
      break;
    }
    case NVS_TYPE_BLOB: {
      size_t len = 0;
      ok = (nvs_get_blob(h, info.key, NULL, &len) == ESP_OK) && len > 0;
      if (ok) {
        uint8_t* buf = (uint8_t*)malloc(len);
        ok = (buf != NULL) && (nvs_get_blob(h, info.key, buf, &len) == ESP_OK);
        if (ok) {
          String hex;
          hex.reserve(len * 2);
          for (size_t i = 0; i < len; i++) {
            char b[3];
            snprintf(b, sizeof(b), "%02x", buf[i]);
            hex += b;
          }
          doc["v"] = hex;  // copied
        }
        free(buf);
      }
      break;
    }
    default:
      ok = false;
      break;
  }
  nvs_close(h);
  if (!ok) return false;

  serializeJson(doc, f);
  f.print('\n');
  return true;
}

// Dump every app NVS entry to a JSONL file on SD. Writes to a .tmp file and
// renames, so a power cut mid-dump can't destroy the previous good backup.
bool dumpAllNvsToSD(const char* path) {
  static bool triedInit = false;
  if (!sdCardAvailable) {
    if (triedInit) return false;
    triedInit = true;  // one init attempt per boot - no card means no backups
    if (!initSDCard()) return false;
  }

  char tmpPath[48];
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);
  File f = SD.open(tmpPath, FILE_WRITE);
  if (!f) return false;

  int count = 0;
  nvs_iterator_t it = nvs_entry_find("nvs", NULL, NVS_TYPE_ANY);
  while (it != NULL) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    if (!isSystemNvsNamespace(info.namespace_name) && writeNvsEntryLine(f, info)) {
      count++;
    }
    it = nvs_entry_next(it);  // releases the iterator when exhausted
  }
  f.close();

  if (count == 0) {
    SD.remove(tmpPath);
    return false;
  }
  SD.remove(path);
  if (!SD.rename(tmpPath, path)) return false;

  Serial.printf("[SDBackup] NVS backup: %d entries -> %s\n", count, path);
  return true;
}

// ---------------------------------------------------------------------------
// Restore: SD -> NVS
// ---------------------------------------------------------------------------

static bool hexDecode(const char* hex, uint8_t* out, size_t outLen) {
  for (size_t i = 0; i < outLen; i++) {
    char hi = hex[i * 2], lo = hex[i * 2 + 1];
    int h = (hi >= '0' && hi <= '9') ? hi - '0' : (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10 : -1;
    int l = (lo >= '0' && lo <= '9') ? lo - '0' : (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10 : -1;
    if (h < 0 || l < 0) return false;
    out[i] = (uint8_t)((h << 4) | l);
  }
  return true;
}

static bool applyNvsEntryLine(const char* line) {
  JsonDocument doc;
  if (deserializeJson(doc, line) != DeserializationError::Ok) return false;
  const char* ns = doc["ns"] | (const char*)NULL;
  const char* k  = doc["k"]  | (const char*)NULL;
  int t = doc["t"] | -1;
  if (ns == NULL || k == NULL || t < 0 || isSystemNvsNamespace(ns)) return false;

  nvs_handle_t h;
  if (nvs_open(ns, NVS_READWRITE, &h) != ESP_OK) return false;

  esp_err_t err = ESP_FAIL;
  switch ((nvs_type_t)t) {
    case NVS_TYPE_U8:  err = nvs_set_u8(h, k,  (uint8_t)(doc["v"]  | 0u));        break;
    case NVS_TYPE_I8:  err = nvs_set_i8(h, k,  (int8_t)(doc["v"]   | 0));         break;
    case NVS_TYPE_U16: err = nvs_set_u16(h, k, (uint16_t)(doc["v"] | 0u));        break;
    case NVS_TYPE_I16: err = nvs_set_i16(h, k, (int16_t)(doc["v"]  | 0));         break;
    case NVS_TYPE_U32: err = nvs_set_u32(h, k, (uint32_t)(doc["v"] | 0u));        break;
    case NVS_TYPE_I32: err = nvs_set_i32(h, k, (int32_t)(doc["v"]  | 0));         break;
    case NVS_TYPE_U64: err = nvs_set_u64(h, k, (uint64_t)(doc["v"] | (uint64_t)0)); break;
    case NVS_TYPE_I64: err = nvs_set_i64(h, k, (int64_t)(doc["v"]  | (int64_t)0)); break;
    case NVS_TYPE_STR: {
      const char* v = doc["v"] | (const char*)NULL;
      if (v != NULL) err = nvs_set_str(h, k, v);
      break;
    }
    case NVS_TYPE_BLOB: {
      const char* hexv = doc["v"] | (const char*)NULL;
      size_t hl = (hexv != NULL) ? strlen(hexv) : 0;
      if (hl > 0 && hl % 2 == 0) {
        size_t bl = hl / 2;
        uint8_t* buf = (uint8_t*)malloc(bl);
        if (buf != NULL) {
          if (hexDecode(hexv, buf, bl)) err = nvs_set_blob(h, k, buf, bl);
          free(buf);
        }
      }
      break;
    }
    default:
      break;
  }
  if (err == ESP_OK) nvs_commit(h);
  nvs_close(h);
  return err == ESP_OK;
}

// Restore every entry from a JSONL backup file. Returns entries restored.
int restoreAllNvsFromSD(const char* path) {
  if (!sdCardAvailable && !initSDCard()) return 0;
  if (!SD.exists(path)) return 0;
  File f = SD.open(path);
  if (!f) return 0;

  int ok = 0, fail = 0;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() < 2) continue;
    if (applyNvsEntryLine(line.c_str())) ok++;
    else fail++;
  }
  f.close();
  Serial.printf("[SDBackup] NVS restore from %s: %d entries ok, %d failed\n", path, ok, fail);
  return ok;
}

/*
 * Called once in setup(), right after runSettingsMigrations(). If boot found
 * an unversioned (empty/wiped) NVS and a full backup exists on SD, restore
 * everything and reboot so the normal boot path (migrations included) runs
 * against the restored data.
 */
void restoreAllNvsIfWiped() {
  if (!g_nvsWasUnversioned) return;  // NVS had data - nothing to recover
  if (!sdCardAvailable && !initSDCard()) return;
  if (!SD.exists(NVS_BACKUP_PATH)) return;

  Serial.println("[SDBackup] NVS is empty but an SD backup exists - restoring");
  if (restoreAllNvsFromSD(NVS_BACKUP_PATH) > 0) {
    // Ensure a schema version is stamped so this path can't loop; if the
    // backup predates the current schema, migration runs on the next boot.
    if (getStoredSettingsVersion() == 0) writeSettingsVersion(1);
    Serial.println("[SDBackup] Restore complete - rebooting to load restored data");
    delay(300);
    ESP.restart();  // does not return
  }
}

// ---------------------------------------------------------------------------
// Rolling backup triggers (polled from loop())
// ---------------------------------------------------------------------------

// Snapshot of the core settings - cheap change detector so a settings tweak is
// backed up within seconds instead of waiting for the periodic dump.
struct SettingsBackupSnapshot {
  int  cwSpeed;
  int  cwTone;
  int  keyType;
  int  decoder;
  int  volume;
  int  brightness;
  char callsign[16];
};

static SettingsBackupSnapshot sdBackupLast;
static bool sdBackupBaselineSet = false;
static bool sdBackupPendingWrite = false;
static unsigned long sdBackupChangedAt = 0;

static void captureSettingsSnapshot(SettingsBackupSnapshot& s) {
  memset(&s, 0, sizeof(s));  // zero padding so memcmp comparison is valid
  s.cwSpeed    = cwSpeed;
  s.cwTone     = cwTone;
  s.keyType    = getCwKeyTypeAsInt();
  s.decoder    = (int)decoderType;
  s.volume     = getVolume();
  s.brightness = getBrightness();
  strncpy(s.callsign, vailCallsign.c_str(), sizeof(s.callsign) - 1);
}

void updateSettingsBackup() {
  static unsigned long lastPoll = 0;
  unsigned long now = millis();
  if (now - lastPoll < 1000) return;  // 1s cadence is plenty
  lastPoll = now;

  static bool bootDumpDone = false;
  static unsigned long lastDump = 0;
  bool dumpNow = false;

  // Fast trigger: a core setting changed and has been stable for the debounce
  SettingsBackupSnapshot cur;
  captureSettingsSnapshot(cur);
  if (!sdBackupBaselineSet) {
    sdBackupLast = cur;  // first poll after boot: loaded state is the baseline
    sdBackupBaselineSet = true;
    return;
  }
  if (memcmp(&cur, &sdBackupLast, sizeof(cur)) != 0) {
    sdBackupLast = cur;
    sdBackupChangedAt = now;
    sdBackupPendingWrite = true;
    return;
  }
  if (sdBackupPendingWrite && (now - sdBackupChangedAt) >= SETTINGS_BACKUP_DEBOUNCE_MS) {
    sdBackupPendingWrite = false;
    dumpNow = true;
  }

  // Boot dump: capture the previous session's progress shortly after boot
  if (!bootDumpDone && now >= NVS_BACKUP_BOOT_DELAY_MS) {
    bootDumpDone = true;
    dumpNow = true;
  }

  // Periodic catch-all for progress saved outside the snapshot (games, training)
  if (lastDump != 0 && (now - lastDump) >= NVS_BACKUP_PERIOD_MS) {
    dumpNow = true;
  }

  if (dumpNow) {
    dumpAllNvsToSD(NVS_BACKUP_PATH);
    lastDump = now;  // even on failure, don't retry every second
  }
}

// Remove all backup files (called from the factory-reset path so a reset
// device doesn't silently resurrect its old data on next boot).
void removeSettingsBackupFromSD() {
  if (!sdCardAvailable && !initSDCard()) return;
  const char* files[] = { NVS_BACKUP_PATH, NVS_PREMIGRATION_PATH, LEGACY_SETTINGS_BACKUP_PATH };
  for (int i = 0; i < 3; i++) {
    if (SD.exists(files[i])) {
      SD.remove(files[i]);
      Serial.printf("[SDBackup] Removed %s\n", files[i]);
    }
  }
}

#endif // SETTINGS_SD_BACKUP_H
