/*
 * Bluetooth Keyboard Check (Get Help page)
 *
 * The page walks the user through pairing a keyboard on the Summit's own
 * External Keyboard screen, one step at a time. The Summit does nothing
 * different from normal use; this file only reports what happens:
 *
 *   @vail bt status        snapshot: host state, saved pairing, crash guard,
 *                          heap, WiFi (shares the radio), bond count
 *   @vail bt mark <step>   echoes a step marker so the log groups by step
 *   @vail bt keys [secs]   trace raw key reports for secs (default 30, max 60)
 *   @vail bt keys off      stop tracing key reports
 *
 * While a computer is attached, the keyboard host records each scan result and
 * connect/pair/enumerate phase (ble_kb_trace.h). btCheckPoll() drains those
 * here on the main loop as
 *
 *   @vail {"t":"bt","ms":12345,"ph":"pair_fail","a":1285,"b":0,"s":"confirm value failed"}
 *
 * plus the keyboard's Report Map as "bt_map" hex chunks.
 */

#ifndef BT_CHECK_H
#define BT_CHECK_H

#include "diag_checks.h"
#include "../bluetooth/ble_kb_trace.h"

#define BT_CHECK_DRAIN_PER_POLL  6
#define BT_CHECK_MAP_CHUNK       64     // bytes of Report Map per line
#define BT_CHECK_KEYS_DEFAULT_S  30
#define BT_CHECK_KEYS_MAX_S      60

void btCheckEmitStatus() {
  char name[48], addr[24], err[64];
  diagJsonEscape(name, sizeof(name), bleKBHost.pairedDevice.valid ? bleKBHost.pairedDevice.name : "");
  diagJsonEscape(addr, sizeof(addr), bleKBHost.pairedDevice.valid ? bleKBHost.pairedDevice.address : "");
  diagJsonEscape(err, sizeof(err), bleKBHost.lastError);
  bool up = bleKBHost.state != BLEKB_STATE_IDLE;
  int bonds = up ? NimBLEDevice::getNumBonds() : -1;
  wifi_mode_t wm = WiFi.getMode();
  diagEmit("{\"t\":\"bt_status\",\"fw\":\"%s\",\"state\":\"%s\",\"busy\":%s,"
           "\"paired\":%s,\"name\":\"%s\",\"addr\":\"%s\",\"atype\":%u,"
           "\"auto\":%s,\"tripped\":%s,\"crashes\":%u,\"crashPhase\":\"%s\","
           "\"err\":\"%s\",\"bonds\":%d,\"heap\":%u,\"heapNeed\":%u,"
           "\"wifi\":\"%s\",\"wifiUp\":%s,\"mode\":%d,\"connSecs\":%lu}",
           FIRMWARE_VERSION, diagBleStateName(bleKBHost.state),
           isBLEKeyboardConnectBusy() ? "true" : "false",
           bleKBHost.pairedDevice.valid ? "true" : "false", name, addr,
           (unsigned)bleKBHost.pairedDevice.addrType,
           bleKBHost.autoReconnect ? "true" : "false",
           bleKBHost.autoReconnectTripped ? "true" : "false",
           (unsigned)bleKBHost.crashCount, getBLEKBPhaseString(bleKBHost.lastCrashPhase),
           err, bonds,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)BLE_MIN_FREE_INTERNAL_HEAP,
           wm == WIFI_MODE_NULL ? "off" : (wm == WIFI_MODE_AP ? "ap" : (wm == WIFI_MODE_APSTA ? "ap+sta" : "sta")),
           WiFi.isConnected() ? "true" : "false",
           (int)currentMode,
           isBLEKeyboardConnected() ? (millis() - bleKBHost.connectedSince) / 1000 : 0UL);
}

// Step ids come from the page; keep only safe characters
void btCheckMark(const char* step) {
  char id[24];
  size_t o = 0;
  for (; step && *step && o < sizeof(id) - 1; step++) {
    char c = *step;
    if (isalnum((unsigned char)c) || c == '_' || c == '-') id[o++] = c;
  }
  id[o] = '\0';
  Serial.printf("===== BT CHECK STEP: %s =====\n", id);
  diagEmit("{\"t\":\"bt_mark\",\"step\":\"%s\",\"ms\":%lu}", id, millis());
}

// secs <= 0 turns key tracing off
void btCheckKeys(int secs) {
  if (secs <= 0) {
    bleKbTraceKeysUntil = 0;
    diagEmit("{\"t\":\"bt_keys\",\"on\":false}");
    return;
  }
  if (secs > BT_CHECK_KEYS_MAX_S) secs = BT_CHECK_KEYS_MAX_S;
  uint32_t until = millis() + (uint32_t)secs * 1000UL;
  bleKbTraceKeysUntil = until ? until : 1;
  diagEmit("{\"t\":\"bt_keys\",\"on\":true,\"secs\":%d}", secs);
}

static void btCheckEmitReportMap() {
  if (!bleKbReportMapReady) return;
  uint16_t len = bleKbReportMapLen;
  char hex[BT_CHECK_MAP_CHUNK * 2 + 1];
  for (uint16_t off = 0; off < len; off += BT_CHECK_MAP_CHUNK) {
    uint16_t n = (len - off) < BT_CHECK_MAP_CHUNK ? (len - off) : BT_CHECK_MAP_CHUNK;
    for (uint16_t i = 0; i < n; i++) snprintf(hex + i * 2, 3, "%02X", bleKbReportMap[off + i]);
    hex[n * 2] = '\0';
    diagEmit("{\"t\":\"bt_map\",\"len\":%u,\"off\":%u,\"hex\":\"%s\"}", len, off, hex);
  }
  // The worker only writes the buffer while !ready, so it is ours to free
  free(bleKbReportMap);
  bleKbReportMap = nullptr;
  bleKbReportMapLen = 0;
  bleKbReportMapReady = false;
}

// Called every loop from serialConsolePoll()
void btCheckPoll() {
  bool on = diagSessionActive && (bool)Serial;
  if (on != bleKbTraceOn) {
    bleKbTraceOn = on;
    if (!on) {
      // Nobody listening any more: drop whatever is queued
      BleKbTraceRec r;
      while (bleKbTracePop(&r)) {}
      bleKbTraceKeysUntil = 0;
      if (bleKbReportMapReady) {
        free(bleKbReportMap);
        bleKbReportMap = nullptr;
        bleKbReportMapReady = false;
      }
    }
  }
  if (!on) return;

  uint32_t until = bleKbTraceKeysUntil;
  if (until != 0 && (int32_t)(until - millis()) <= 0) {
    bleKbTraceKeysUntil = 0;
    diagEmit("{\"t\":\"bt_keys\",\"on\":false}");
  }

  uint16_t dropped = bleKbTraceTakeDropped();
  if (dropped) diagEmit("{\"t\":\"bt\",\"ms\":%lu,\"ph\":\"dropped\",\"a\":%u,\"b\":0,\"s\":\"\"}", millis(), dropped);

  BleKbTraceRec r;
  for (int i = 0; i < BT_CHECK_DRAIN_PER_POLL && bleKbTracePop(&r); i++) {
    char s[BLEKB_TRACE_TEXT_LEN * 2], ph[BLEKB_TRACE_PH_LEN * 2];
    diagJsonEscape(s, sizeof(s), r.s);
    diagJsonEscape(ph, sizeof(ph), r.ph);
    diagEmit("{\"t\":\"bt\",\"ms\":%lu,\"ph\":\"%s\",\"a\":%ld,\"b\":%ld,\"s\":\"%s\"}",
             (unsigned long)r.ms, ph, (long)r.a, (long)r.b, s);
  }

  btCheckEmitReportMap();
}

// "@vail bt <sub> [arg]". Returns false if sub is not a bt command.
bool btCheckCommand(const char* sub, const char* arg) {
  if (!sub) return false;
  if (strcmp(sub, "status") == 0) { btCheckEmitStatus(); return true; }
  if (strcmp(sub, "mark") == 0) { btCheckMark(arg ? arg : ""); return true; }
  if (strcmp(sub, "keys") == 0) {
    if (arg && strcmp(arg, "off") == 0) btCheckKeys(0);
    else btCheckKeys(arg ? atoi(arg) : BT_CHECK_KEYS_DEFAULT_S);
    return true;
  }
  return false;
}

#endif // BT_CHECK_H
