/*
 * Diagnostic Event History
 *
 * A small ring of notable events (SD mount results, audio init failures, WiFi
 * connect failures, low heap, BLE keyboard state changes, boots) kept in
 * RTC_NOINIT memory. That memory survives panics, watchdog resets and software
 * restarts, so after a crash the self check can still show what happened just
 * before it. A full power-off clears it.
 *
 * Included early (right after config.h) so any header can call diagEvent().
 * Writes are rare and cheap: no flash, no heap.
 */

#ifndef DIAG_EVENTS_H
#define DIAG_EVENTS_H

#include <Arduino.h>
#include <esp_system.h>
#include <stdarg.h>

#define DIAG_EVENT_MAGIC     0x56444731UL   // "VDG1"
#define DIAG_EVENT_CAPACITY  32
#define DIAG_EVENT_TAG_LEN   6
#define DIAG_EVENT_MSG_LEN   40
#define DIAG_MODE_HISTORY    8

struct DiagEventRec {
  uint32_t ms;                       // millis() when recorded
  uint16_t boot;                     // boot number since power-on
  char tag[DIAG_EVENT_TAG_LEN];      // short subsystem tag: "SD", "WIFI", ...
  char msg[DIAG_EVENT_MSG_LEN];
};

struct DiagEventStore {
  uint32_t magic;
  uint16_t bootCount;                // boots since the last power-on
  uint16_t head;                     // next write slot
  uint16_t count;                    // valid entries (<= capacity)
  uint16_t modeHead;
  int16_t  modes[DIAG_MODE_HISTORY]; // most recent screens, oldest first via modeHead
  DiagEventRec events[DIAG_EVENT_CAPACITY];
};

static RTC_NOINIT_ATTR DiagEventStore diagStore;
static bool diagEventsReady = false;
// diagEvent() is also reached from the async web server task (SD mounts), so
// slot allocation is guarded
static portMUX_TYPE diagEventMux = portMUX_INITIALIZER_UNLOCKED;

static const char* diagResetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_SW:        return "software restart";
    case ESP_RST_PANIC:     return "crash (panic)";
    case ESP_RST_TASK_WDT:  return "loop watchdog";
    case ESP_RST_INT_WDT:   return "interrupt watchdog";
    case ESP_RST_WDT:       return "watchdog";
    case ESP_RST_BROWNOUT:  return "brownout (low power)";
    case ESP_RST_DEEPSLEEP: return "deep sleep wake";
    case ESP_RST_EXT:       return "reset pin";
    case ESP_RST_USB:       return "USB reset";
    default:                return "other";
  }
}

// True for resets that mean something went wrong
static bool diagResetWasCrash(esp_reset_reason_t r) {
  return r == ESP_RST_PANIC || r == ESP_RST_TASK_WDT || r == ESP_RST_INT_WDT ||
         r == ESP_RST_WDT || r == ESP_RST_BROWNOUT;
}

static bool diagStoreLooksValid() {
  return diagStore.magic == DIAG_EVENT_MAGIC &&
         diagStore.head < DIAG_EVENT_CAPACITY &&
         diagStore.count <= DIAG_EVENT_CAPACITY &&
         diagStore.modeHead < DIAG_MODE_HISTORY;
}

void diagEvent(const char* tag, const char* fmt, ...);

// Call once, early in setup(). Keeps the history across crash/soft resets and
// starts fresh after a power-on (RTC memory holds garbage then).
void diagEventsInit() {
  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_POWERON || !diagStoreLooksValid()) {
    memset(&diagStore, 0, sizeof(diagStore));
    for (int i = 0; i < DIAG_MODE_HISTORY; i++) diagStore.modes[i] = -1;
    diagStore.magic = DIAG_EVENT_MAGIC;
  } else {
    // Kept across a crash: make sure no record can print past its buffers
    for (int i = 0; i < DIAG_EVENT_CAPACITY; i++) {
      diagStore.events[i].tag[DIAG_EVENT_TAG_LEN - 1] = '\0';
      diagStore.events[i].msg[DIAG_EVENT_MSG_LEN - 1] = '\0';
    }
  }
  diagStore.bootCount++;
  diagEventsReady = true;
  diagEvent("BOOT", "v%s, reset: %s", FIRMWARE_VERSION, diagResetReasonName(reason));
}

void diagEvent(const char* tag, const char* fmt, ...) {
  if (!diagEventsReady) return;
  portENTER_CRITICAL(&diagEventMux);
  DiagEventRec* e = &diagStore.events[diagStore.head];
  diagStore.head = (diagStore.head + 1) % DIAG_EVENT_CAPACITY;
  if (diagStore.count < DIAG_EVENT_CAPACITY) diagStore.count++;
  portEXIT_CRITICAL(&diagEventMux);
  e->ms = millis();
  e->boot = diagStore.bootCount;
  strlcpy(e->tag, tag, sizeof(e->tag));
  va_list args;
  va_start(args, fmt);
  vsnprintf(e->msg, sizeof(e->msg), fmt, args);
  va_end(args);
  Serial.printf("[Diag] %s: %s\n", e->tag, e->msg);
}

// Screens are tracked separately so frequent navigation can't push the
// interesting events out of the ring.
void diagRecordMode(int mode) {
  if (!diagEventsReady) return;
  diagStore.modes[diagStore.modeHead] = (int16_t)mode;
  diagStore.modeHead = (diagStore.modeHead + 1) % DIAG_MODE_HISTORY;
}

int diagEventCount() { return diagEventsReady ? diagStore.count : 0; }
uint16_t diagBootCount() { return diagEventsReady ? diagStore.bootCount : 0; }

// idx 0 = oldest
const DiagEventRec* diagEventAt(int idx) {
  if (idx < 0 || idx >= diagEventCount()) return nullptr;
  int start = (diagStore.head + DIAG_EVENT_CAPACITY - diagStore.count) % DIAG_EVENT_CAPACITY;
  return &diagStore.events[(start + idx) % DIAG_EVENT_CAPACITY];
}

// Writes recent screens oldest first as "61,22,3"; empty if none
void diagFormatModeHistory(char* buf, size_t len) {
  buf[0] = '\0';
  if (!diagEventsReady) return;
  size_t used = 0;
  for (int i = 0; i < DIAG_MODE_HISTORY; i++) {
    int16_t m = diagStore.modes[(diagStore.modeHead + i) % DIAG_MODE_HISTORY];
    if (m < 0) continue;
    int n = snprintf(buf + used, len - used, used ? ",%d" : "%d", m);
    if (n < 0 || (size_t)n >= len - used) break;
    used += n;
  }
}

#endif // DIAG_EVENTS_H
