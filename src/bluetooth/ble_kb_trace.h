/*
 * BLE Keyboard Pairing Trace
 *
 * Structured record of what the keyboard host is doing (scan results, each
 * connect/pair/enumerate phase, disconnect reasons) for the Bluetooth keyboard
 * check on the Get Help page. The page shows these as a timeline and puts them
 * in the report alongside the normal "BLEKB:" log.
 *
 * Records come from the connect worker task and the NimBLE host task, so they
 * go into a small locked ring here; the main loop drains it and sends each one
 * as an "@vail {"t":"bt",...}" line (bt_check.h). Nothing is recorded unless a
 * computer is attached and talking to us (bleKbTraceOn), so normal use pays
 * nothing.
 *
 * Key reports are only traced while the page's key test is armed
 * (bleKbTraceKeysUntil), so the report never holds anything else the user typed.
 * While armed, the keys are also kept out of the UI (see hidReportNotifyCallback)
 * so the typing test can't press buttons on the Summit.
 *
 * bleKbTraceOn is only changed under bleKbTraceMux, and producers re-check it
 * under the same lock before publishing, so nothing from a finished session can
 * land in the queue or the Report Map slot after the main loop has cleaned up.
 */

#ifndef BLE_KB_TRACE_H
#define BLE_KB_TRACE_H

#include <Arduino.h>
#include <stdarg.h>
#include <esp_heap_caps.h>

#define BLEKB_TRACE_CAP       24
#define BLEKB_TRACE_PH_LEN    16
#define BLEKB_TRACE_TEXT_LEN  48
#define BLEKB_REPORT_MAP_MAX  512

struct BleKbTraceRec {
  uint32_t ms;
  char ph[BLEKB_TRACE_PH_LEN];       // phase id, e.g. "link", "encrypt"
  int32_t a;                         // phase-specific numbers (see bt_check.h)
  int32_t b;
  char s[BLEKB_TRACE_TEXT_LEN];      // phase-specific text
};

static BleKbTraceRec bleKbTraceQ[BLEKB_TRACE_CAP];
static uint8_t bleKbTraceHead = 0;
static uint8_t bleKbTraceCount = 0;
static uint16_t bleKbTraceDropped = 0;
static portMUX_TYPE bleKbTraceMux = portMUX_INITIALIZER_UNLOCKED;

// Set by the main loop while a computer session is active
static volatile bool bleKbTraceOn = false;
// millis() deadline for tracing key reports (0 = off)
static volatile uint32_t bleKbTraceKeysUntil = 0;

// Report map captured during enumeration, sent once by the main loop. Only
// allocated while tracing, and freed as soon as it has been sent.
static uint8_t* bleKbReportMap = nullptr;
static volatile uint16_t bleKbReportMapLen = 0;     // bytes held in bleKbReportMap
static volatile uint16_t bleKbReportMapTotal = 0;   // size the keyboard reported
static volatile bool bleKbReportMapReady = false;
static volatile bool bleKbReportMapBusy = false;    // worker is copying into it

void bleKbTrace(const char* ph, int32_t a, int32_t b, const char* fmt = nullptr, ...) {
  if (!bleKbTraceOn) return;
  BleKbTraceRec rec;
  rec.ms = millis();
  strlcpy(rec.ph, ph, sizeof(rec.ph));
  rec.a = a;
  rec.b = b;
  rec.s[0] = '\0';
  if (fmt) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(rec.s, sizeof(rec.s), fmt, args);
    va_end(args);
  }
  portENTER_CRITICAL(&bleKbTraceMux);
  if (!bleKbTraceOn) {
    // The session ended while this record was being built
  } else if (bleKbTraceCount < BLEKB_TRACE_CAP) {
    bleKbTraceQ[(bleKbTraceHead + bleKbTraceCount) % BLEKB_TRACE_CAP] = rec;
    bleKbTraceCount++;
  } else {
    bleKbTraceDropped++;
  }
  portEXIT_CRITICAL(&bleKbTraceMux);
}

// Oldest record out; false when empty
bool bleKbTracePop(BleKbTraceRec* out) {
  bool ok = false;
  portENTER_CRITICAL(&bleKbTraceMux);
  if (bleKbTraceCount > 0) {
    *out = bleKbTraceQ[bleKbTraceHead];
    bleKbTraceHead = (bleKbTraceHead + 1) % BLEKB_TRACE_CAP;
    bleKbTraceCount--;
    ok = true;
  }
  portEXIT_CRITICAL(&bleKbTraceMux);
  return ok;
}

uint16_t bleKbTraceTakeDropped() {
  portENTER_CRITICAL(&bleKbTraceMux);
  uint16_t d = bleKbTraceDropped;
  bleKbTraceDropped = 0;
  portEXIT_CRITICAL(&bleKbTraceMux);
  return d;
}

// Called from the connect worker after reading the Report Map. Maps longer
// than BLEKB_REPORT_MAP_MAX are cut, and bleKbReportMapTotal keeps the real
// size so the report can say so.
void bleKbTraceReportMap(const uint8_t* data, size_t len) {
  if (len == 0) return;
  portENTER_CRITICAL(&bleKbTraceMux);
  bool take = bleKbTraceOn && !bleKbReportMapReady && !bleKbReportMapBusy;
  if (take) bleKbReportMapBusy = true;
  portEXIT_CRITICAL(&bleKbTraceMux);
  if (!take) return;

  // The buffer is only freed by the main loop while neither busy nor ready,
  // so it is ours to fill now
  if (bleKbReportMap == nullptr) {
    bleKbReportMap = (uint8_t*)heap_caps_malloc(BLEKB_REPORT_MAP_MAX, MALLOC_CAP_8BIT);
  }
  size_t n = len < BLEKB_REPORT_MAP_MAX ? len : BLEKB_REPORT_MAP_MAX;
  if (bleKbReportMap != nullptr) memcpy(bleKbReportMap, data, n);

  portENTER_CRITICAL(&bleKbTraceMux);
  if (bleKbReportMap != nullptr && bleKbTraceOn) {
    bleKbReportMapLen = (uint16_t)n;
    bleKbReportMapTotal = (uint16_t)(len > 0xFFFF ? 0xFFFF : len);
    bleKbReportMapReady = true;
  }
  bleKbReportMapBusy = false;
  portEXIT_CRITICAL(&bleKbTraceMux);
}

// Main loop: start or end a trace session. Ending one drops everything queued
// and hands back the Report Map buffer (nullptr if a worker still holds it).
uint8_t* bleKbTraceSetOn(bool on) {
  uint8_t* toFree = nullptr;
  portENTER_CRITICAL(&bleKbTraceMux);
  bleKbTraceOn = on;
  if (!on) {
    bleKbTraceHead = 0;
    bleKbTraceCount = 0;
    bleKbTraceDropped = 0;
    bleKbTraceKeysUntil = 0;
    if (!bleKbReportMapBusy) {
      toFree = bleKbReportMap;
      bleKbReportMap = nullptr;
      bleKbReportMapReady = false;
      bleKbReportMapLen = 0;
      bleKbReportMapTotal = 0;
    }
  }
  portEXIT_CRITICAL(&bleKbTraceMux);
  return toFree;
}

bool bleKbTraceKeysArmed() {
  uint32_t until = bleKbTraceKeysUntil;
  return bleKbTraceOn && until != 0 && (int32_t)(until - millis()) > 0;
}

// Plain-English name for a NimBLE connect/disconnect/pairing error code.
// HCI reasons are 0x200 + the Bluetooth spec code, Security Manager errors are
// 0x400 (ours) / 0x500 (the keyboard's) + the SM code.
const char* bleKbReasonName(int reason) {
  if (reason == 0) return "ok";
  if (reason >= 0x200 && reason < 0x300) {
    switch (reason - 0x200) {
      case 0x05: return "authentication failure";
      case 0x06: return "keyboard lost its pairing key";
      case 0x08: return "connection timeout (out of range or keyboard slept)";
      case 0x13: return "keyboard ended the connection";
      case 0x14: return "keyboard low on resources";
      case 0x15: return "keyboard powering off";
      case 0x16: return "Summit ended the connection";
      case 0x1A: return "unsupported feature";
      case 0x22: return "link layer response timeout";
      case 0x28: return "instant passed";
      case 0x3B: return "unacceptable connection parameters";
      case 0x3D: return "encryption check failed (stale pairing)";
      case 0x3E: return "connection failed to establish";
      default:   return "radio error";
    }
  }
  if (reason >= 0x400 && reason < 0x600) {
    switch (reason & 0xFF) {
      case 0x01: return "passkey entry failed";
      case 0x02: return "OOB not available";
      case 0x03: return "authentication requirements not met";
      case 0x04: return "confirm value failed (wrong PIN?)";
      case 0x05: return "pairing not supported";
      case 0x06: return "encryption key too short";
      case 0x07: return "command not supported";
      case 0x08: return "unspecified pairing failure";
      case 0x09: return "too many pairing attempts";
      case 0x0B: return "DHKey check failed";
      case 0x0C: return "numeric comparison failed";
      default:   return reason >= 0x500 ? "keyboard refused pairing" : "pairing error";
    }
  }
  if (reason >= 0x100 && reason < 0x200) return "GATT error";
  switch (reason) {
    case 7:  return "not connected";
    case 6:  return "out of memory";
    case 13: return "timed out";
    case 14: return "already done";
    case 15: return "radio busy";
    case 16: return "rejected";
    default: return "error";
  }
}

#endif // BLE_KB_TRACE_H
