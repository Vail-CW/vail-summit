/*
 * Device Self Check - automatic checks, results, and the serial report
 *
 * The checks run as a non-blocking step list: diagRunPoll() runs one step per
 * call (each step is at most ~1 s, the SD mount on a missing card being the
 * slowest). Results are kept for the results screen and are also sent out over
 * USB serial as machine-readable lines:
 *
 *     @vail {"t":"check","id":"sd_card","name":"SD card","st":"fail",...}
 *
 * The Get Help page on the updater site parses those lines; the rest of the
 * serial log flows around them and goes into the copied report as-is.
 *
 * Also hosts diagMonitorPoll(), the cheap loop() hook that turns WiFi and
 * Bluetooth keyboard state changes into event-history entries.
 */

#ifndef DIAG_CHECKS_H
#define DIAG_CHECKS_H

#include <Preferences.h>
#include <nvs.h>
#include <esp_heap_caps.h>
#include "diag_events.h"
#include "diag_sd_test.h"

#define DIAG_PROTOCOL_VERSION 1

// ============================================
// Serial output
// ============================================

// A computer that sent us a command is reading, so our own "@vail" lines may
// wait briefly for the USB buffer to drain instead of being dropped. Only
// these lines get the wait: everything else keeps setup()'s 0 ms timeout, so a
// connected-but-idle host can never stall the keyer through ordinary logging.
#define DIAG_SESSION_TX_TIMEOUT_MS 50
#define DIAG_SESSION_IDLE_MS       (5UL * 60UL * 1000UL)
static bool diagSessionActive = false;
static unsigned long diagSessionLastMs = 0;

void diagSessionTouch() {
  diagSessionLastMs = millis();
  diagSessionActive = true;
}

void diagSessionPoll() {
  if (diagSessionActive && millis() - diagSessionLastMs > DIAG_SESSION_IDLE_MS) {
    diagSessionActive = false;
  }
}

// Copy src into dst as JSON string content (no surrounding quotes)
static void diagJsonEscape(char* dst, size_t len, const char* src) {
  size_t o = 0;
  for (; src && *src && o + 7 < len; src++) {
    unsigned char c = (unsigned char)*src;
    if (c == '"' || c == '\\') { dst[o++] = '\\'; dst[o++] = c; }
    else if (c == '\n') { dst[o++] = '\\'; dst[o++] = 'n'; }
    else if (c < 0x20) { o += snprintf(dst + o, len - o, "\\u%04x", c); }
    else dst[o++] = c;
  }
  dst[o] = '\0';
}

// Send one "@vail {json}" line. fmt builds the JSON object.
void diagEmit(const char* fmt, ...) {
  char line[400];
  int n = snprintf(line, sizeof(line), "@vail ");
  va_list args;
  va_start(args, fmt);
  vsnprintf(line + n, sizeof(line) - n - 2, fmt, args);
  va_end(args);
  strlcat(line, "\n", sizeof(line));
  bool wait = diagSessionActive && (bool)Serial;
  if (wait) Serial.setTxTimeoutMs(DIAG_SESSION_TX_TIMEOUT_MS);
  Serial.write((const uint8_t*)line, strlen(line));
  if (wait) Serial.setTxTimeoutMs(0);
}

// ============================================
// Results
// ============================================

enum DiagStatus : uint8_t { DIAG_PASS = 0, DIAG_WARN, DIAG_FAIL, DIAG_INFO };

static const char* diagStatusName(DiagStatus s) {
  switch (s) {
    case DIAG_PASS: return "pass";
    case DIAG_WARN: return "warn";
    case DIAG_FAIL: return "fail";
    default:        return "info";
  }
}

struct DiagResult {
  const char* id;
  const char* name;
  const char* advice;    // plain-English next step, or nullptr
  DiagStatus st;
  char detail[100];
};

#define DIAG_MAX_RESULTS 24   // 14 automatic + 5 guided, with headroom
static DiagResult* diagResults = nullptr;   // allocated on first use (PSRAM when fitted)
static int diagResultCountV = 0;
static uint16_t diagResultsReplaced = 0;    // bumps when an entry is overwritten in place

static bool diagEnsureResults() {
  if (diagResults) return true;
  size_t bytes = sizeof(DiagResult) * DIAG_MAX_RESULTS;
  diagResults = (DiagResult*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!diagResults) diagResults = (DiagResult*)malloc(bytes);
  if (diagResults) memset(diagResults, 0, bytes);
  return diagResults != nullptr;
}

int diagResultCount() { return diagResultCountV; }
const DiagResult* diagResultAt(int i) {
  return (i >= 0 && i < diagResultCountV) ? &diagResults[i] : nullptr;
}

void diagEmitResult(const DiagResult* r) {
  char detail[160];
  char advice[300];
  diagJsonEscape(detail, sizeof(detail), r->detail);
  diagJsonEscape(advice, sizeof(advice), r->advice ? r->advice : "");
  diagEmit("{\"t\":\"check\",\"id\":\"%s\",\"name\":\"%s\",\"st\":\"%s\",\"detail\":\"%s\",\"advice\":\"%s\"}",
           r->id, r->name, diagStatusName(r->st), detail, advice);
}

static void diagAdd(const char* id, const char* name, DiagStatus st, const char* advice,
                    const char* fmt, ...) {
  if (!diagEnsureResults()) return;
  // A repeated test (e.g. the sound test run twice) replaces its earlier result
  DiagResult* r = nullptr;
  for (int i = 0; i < diagResultCountV; i++) {
    if (strcmp(diagResults[i].id, id) == 0) { r = &diagResults[i]; diagResultsReplaced++; break; }
  }
  if (!r) {
    if (diagResultCountV >= DIAG_MAX_RESULTS) return;
    r = &diagResults[diagResultCountV++];
  }
  r->id = id;
  r->name = name;
  r->st = st;
  r->advice = advice;
  va_list args;
  va_start(args, fmt);
  vsnprintf(r->detail, sizeof(r->detail), fmt, args);
  va_end(args);
  diagEmitResult(r);
}

// ============================================
// Individual checks
// ============================================

static void diagCheckFirmware() {
  diagAdd("firmware", "Firmware", DIAG_INFO, nullptr, "v%s (%s), %s, PSRAM %s",
          FIRMWARE_VERSION, FIRMWARE_DATE, ESP.getChipModel(),
          psramFound() ? "yes" : "no");
}

static void diagCheckReset() {
  esp_reset_reason_t r = esp_reset_reason();
  uint8_t bleCrashes = 0;
  {
    Preferences p;
    if (p.begin("btkeyboard", true)) {
      bleCrashes = p.getUChar("crashCnt", 0);
      p.end();
    }
  }
  if (diagResetWasCrash(r)) {
    diagAdd("reset", "Last restart", DIAG_WARN,
            "The Summit restarted itself because of a problem. If it keeps happening, send this report so it can be fixed.",
            "%s (boot %u since power-on)", diagResetReasonName(r), (unsigned)diagBootCount());
  } else if (bleCrashes > 0) {
    diagAdd("reset", "Last restart", DIAG_WARN,
            "Crashes happened while connecting a Bluetooth keyboard. Try removing the keyboard in Settings > Bluetooth and pairing again.",
            "%s; %u Bluetooth keyboard crash(es)", diagResetReasonName(r), (unsigned)bleCrashes);
  } else {
    diagAdd("reset", "Last restart", DIAG_PASS, nullptr, "%s (boot %u since power-on)",
            diagResetReasonName(r), (unsigned)diagBootCount());
  }
}

static void diagCheckMemory() {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  char ps[24] = "none";
  if (psramFound()) snprintf(ps, sizeof(ps), "%u KB free", (unsigned)(ESP.getFreePsram() / 1024));
  if (freeHeap < 20000) {
    diagAdd("memory", "Memory", DIAG_FAIL, "Memory is very low. Restart the Summit and run the check again.",
            "%u KB free, largest %u KB, PSRAM %s", (unsigned)(freeHeap / 1024), (unsigned)(largest / 1024), ps);
  } else if (largest < TLS_INTERNAL_BLOCK_NEEDED) {
    diagAdd("memory", "Memory", DIAG_WARN, "Online features may fail until the Summit is restarted.",
            "%u KB free, largest %u KB (lowest %u KB), PSRAM %s", (unsigned)(freeHeap / 1024),
            (unsigned)(largest / 1024), (unsigned)(lowestMaxInternalBlock / 1024), ps);
  } else {
    diagAdd("memory", "Memory", DIAG_PASS, nullptr, "%u KB free, largest %u KB, PSRAM %s",
            (unsigned)(freeHeap / 1024), (unsigned)(largest / 1024), ps);
  }
}

static void diagCheckSettingsStore() {
  nvs_stats_t stats;
  if (nvs_get_stats(NULL, &stats) != ESP_OK) {
    diagAdd("nvs", "Settings storage", DIAG_FAIL,
            "Settings storage can't be read. A factory reset (Settings > Device Settings) usually fixes this.",
            "nvs_get_stats failed");
    return;
  }
  bool nearlyFull = stats.free_entries < 32;
  diagAdd("nvs", "Settings storage", nearlyFull ? DIAG_WARN : DIAG_PASS,
          nearlyFull ? "Settings storage is almost full, so new settings may not save." : nullptr,
          "%u of %u entries used", (unsigned)stats.used_entries, (unsigned)stats.total_entries);
}

// Shown wherever MISO stuck low is the cause
#define DIAG_ADVICE_MISO_LOW   "The SD data line (MISO) is held low, so the card can't be read reliably. Usually the display board "   "is doing it and its MISO pin needs to be disconnected: email Brett at ke9bos@pigletradio.org for how. "   "Otherwise look for solder shorting MISO to ground."

static bool diagSdMisoHeld = false;

static void diagCheckSDWiring() {
  DiagSDWiring w;
  diagSDWiringTest(&w);
  diagSdMisoHeld = w.misoLow;

  char pulled[32] = "";
  for (int i = 0; i < DIAG_SD_PIN_COUNT; i++) {
    if (w.pullupMask & (1 << i)) {
      if (pulled[0]) strlcat(pulled, " ", sizeof(pulled));
      strlcat(pulled, diagSdPinNames[i], sizeof(pulled));
    }
  }
  if (!pulled[0]) strlcpy(pulled, "none", sizeof(pulled));

  if (w.bridgeMask) {
    diagAdd("sd_wiring", "SD wiring", DIAG_FAIL,
            "Two SD pins on the display board header are touching. Look at the SD header solder joints and remove the solder bridge.",
            "solder bridge: %s", w.bridges);
    return;
  }
  if (w.shortGndMask) {
    char shorted[32] = "";
    for (int i = 0; i < DIAG_SD_PIN_COUNT; i++) {
      if (w.shortGndMask & (1 << i)) {
        if (shorted[0]) strlcat(shorted, " ", sizeof(shorted));
        strlcat(shorted, diagSdPinNames[i], sizeof(shorted));
      }
    }
    diagAdd("sd_wiring", "SD wiring", DIAG_FAIL,
            "An SD pin is shorted to ground. Look at the SD header solder joints for solder touching a neighboring pin or pad.",
            "shorted to ground: %s", shorted);
    return;
  }

  if (w.misoLow) {
    diagAdd("sd_wiring", "SD wiring", DIAG_FAIL, DIAG_ADVICE_MISO_LOW,
            "MISO held low with the card deselected");
    return;
  }

  // Only CS is the SD slot's own pin; an open joint on any of them shows up
  // in the card check instead
  diagAdd("sd_wiring", "SD wiring", DIAG_PASS, nullptr, "no shorts or bridges on CS, MOSI, SCK, MISO free (pull-ups: %s)", pulled);
}

static bool diagSdMountedOk = false;

static void diagCheckSDCard() {
  DiagSDProbe p;
  diagSDCardProbe(&p);

  // Highest clock that read block 0 with a good CRC
  int best = -1;
  for (size_t s = 0; s < DIAG_SD_SPEED_COUNT; s++) {
    if (p.speedOkMask & (1 << s)) best = s;
  }
  // The SD library runs at 4 MHz (index 2); a failure only at 10 MHz is noted
  // but not treated as a fault
  bool marginal = best >= 0 && (p.speedOkMask & 0x07) != (p.speedTriedMask & 0x07);
  bool fastFail = best >= 0 && !marginal && p.speedOkMask != p.speedTriedMask;

  // Always try a real mount (skipping the 30 s "no card" back-off), so a
  // working card is never left unmounted by the probe
  sdInitLastFailMs = 0;
  diagSdMountedOk = initSDCard();

  if (!diagSdMountedOk) {
    if (diagSdMisoHeld) {
      diagAdd("sd_card", "SD card", DIAG_FAIL, DIAG_ADVICE_MISO_LOW,
              "can't read the card while MISO is held low (reset reply 0x%02X)", p.cmd0);
    } else if (!p.responded) {
      diagAdd("sd_card", "SD card", DIAG_WARN,
              "No SD card found. If a card is in the slot, push it in until it clicks, or try another card.",
              "no response from card");
    } else if (p.garbled) {
      diagAdd("sd_card", "SD card", DIAG_FAIL,
              "The card's answer was garbled, which usually means a bad MISO solder joint or a bridge on the SD header.",
              "bad reply 0x%02X to reset command", p.cmd0);
    } else if (!p.initOk) {
      diagAdd("sd_card", "SD card", DIAG_FAIL,
              "The card answered but would not start. Try a different card (4 to 32 GB works best).",
              "card did not finish start-up (%s)", p.v2 ? "SD v2" : "SD v1");
    } else if (best < 0) {
      diagAdd("sd_card", "SD card", DIAG_FAIL,
              "The card starts but every read comes back corrupted. Reflow the SD header solder joints, or try another card.",
              "reads fail at every speed (R1 0x%02X, token 0x%02X%s)", p.readR1, p.readToken,
              p.readCrcBad ? ", CRC mismatch" : "");
    } else {
      diagAdd("sd_card", "SD card", DIAG_FAIL,
              "The card works but isn't formatted the way the Summit needs. Format it as FAT32 on a computer "
              "(cards over 32 GB come as exFAT and need reformatting).",
              "%s card OK, but no FAT32 filesystem", p.sdhc ? "SDHC" : "SD");
    }
    return;
  }

  char size[16], freeBuf[16];
  formatBytes(SD.cardSize(), size, sizeof(size));
  uint64_t freeBytes = getSDFreeBytes();
  formatBytes(freeBytes, freeBuf, sizeof(freeBuf));
  const char* type = SD.cardType() == CARD_SDHC ? "SDHC" : "SD";

  if (marginal) {
    diagAdd("sd_card", "SD card", DIAG_WARN,
            "Reads fail at normal speed, a sign of a weak solder joint on the SD header. Reflowing the 4 SD pins should fix it.",
            "%s %s, reads OK only up to %u kHz", type, size, (unsigned)(diagSdSpeeds[best] / 1000));
  } else if (freeBytes < 50ULL * 1024 * 1024) {
    diagAdd("sd_card", "SD card", DIAG_WARN, "The card is almost full. Delete some files or use a bigger card.",
            "%s %s, %s free", type, size, freeBuf);
  } else {
    diagAdd("sd_card", "SD card", DIAG_PASS, nullptr, "%s %s, %s free%s", type, size, freeBuf,
            !p.initOk || best < 0 ? " (raw probe inconclusive)" : (fastFail ? " (10 MHz reads fail)" : ""));
  }
}

#define DIAG_SD_TEST_FILE "/.vail_selftest.bin"
#define DIAG_SD_TEST_BYTES 4096

static void diagCheckSDReadWrite() {
  if (!diagSdMountedOk) return;
  uint8_t chunk[256];
  unsigned long t0 = millis();
  bool ok = true;
  File f = SD.open(DIAG_SD_TEST_FILE, FILE_WRITE);
  if (!f) ok = false;
  for (int off = 0; ok && off < DIAG_SD_TEST_BYTES; off += sizeof(chunk)) {
    for (size_t i = 0; i < sizeof(chunk); i++) chunk[i] = (uint8_t)((off + i) * 31 + 7);
    if (f.write(chunk, sizeof(chunk)) != sizeof(chunk)) ok = false;
  }
  if (f) f.close();
  unsigned long tWrite = millis() - t0;

  if (!ok) {
    SD.remove(DIAG_SD_TEST_FILE);
    diagAdd("sd_rw", "SD read/write", DIAG_FAIL,
            "Files can't be saved to the card. It may be locked (check the switch on an adapter) or worn out. Try another card.",
            "write failed");
    return;
  }

  bool match = true;
  f = SD.open(DIAG_SD_TEST_FILE, FILE_READ);
  if (!f) match = false;
  for (int off = 0; match && off < DIAG_SD_TEST_BYTES; off += sizeof(chunk)) {
    if (f.read(chunk, sizeof(chunk)) != (int)sizeof(chunk)) { match = false; break; }
    for (size_t i = 0; i < sizeof(chunk); i++) {
      if (chunk[i] != (uint8_t)((off + i) * 31 + 7)) { match = false; break; }
    }
  }
  if (f) f.close();
  SD.remove(DIAG_SD_TEST_FILE);
  unsigned long tTotal = millis() - t0;

  if (!match) {
    diagAdd("sd_rw", "SD read/write", DIAG_FAIL,
            "Data read back from the card didn't match what was written. Try another card; if that also fails, reflow the SD header joints.",
            "read-back mismatch");
  } else {
    diagAdd("sd_rw", "SD read/write", DIAG_PASS, nullptr, "4 KB written and verified (%lu ms write, %lu ms total)",
            tWrite, tTotal);
  }
}

static void diagCheckWebFiles() {
  if (!diagSdMountedOk) return;
  char ver[16] = "";
  File f = SD.open(WEB_FILES_PATH "version.txt", FILE_READ);
  if (f) {
    int n = f.read((uint8_t*)ver, sizeof(ver) - 1);
    f.close();
    ver[n > 0 ? n : 0] = '\0';
    for (int i = strlen(ver) - 1; i >= 0 && (ver[i] == '\n' || ver[i] == '\r' || ver[i] == ' '); i--) ver[i] = '\0';
  }
  if (!ver[0]) {
    diagAdd("web_files", "Web files", DIAG_WARN,
            "The web pages aren't on the card. Connect to WiFi, then go to Settings > Device Settings > WiFi > Web Files.",
            "not installed");
  } else if (strcmp(ver, WEB_FILES_VERSION) != 0) {
    diagAdd("web_files", "Web files", DIAG_WARN,
            "The web pages on the card are out of date. Connect to WiFi, then go to Settings > Device Settings > WiFi > Web Files.",
            "v%s on card, firmware expects v%s", ver, WEB_FILES_VERSION);
  } else {
    diagAdd("web_files", "Web files", DIAG_PASS, nullptr, "v%s", ver);
  }
}

// All SD steps run back to back in one call: the wiring test takes the SPI
// pins away from the SD library, so nothing else may touch the card until the
// card is mounted again. The web server runs on its own task and can read the
// SD card at any time, so it is stopped first; its lifecycle restarts it on a
// later loop pass if it is still wanted.
static void diagCheckSDAll() {
  if (webServerRunning) {
    Serial.println("[Diag] Stopping web server for the SD tests");
    stopWebServer();
  }
  diagCheckSDWiring();
  diagCheckSDCard();
  diagCheckSDReadWrite();
  diagCheckWebFiles();
}

static bool diagI2CPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static void diagCheckKeyboard() {
  bool cardkb = diagI2CPresent(CARDKB_ADDR);
  bool bleKb = isBLEKeyboardConnected();
  if (cardkb) {
    diagAdd("keyboard", "Keyboard", DIAG_PASS, nullptr, "CardKB found%s", bleKb ? ", Bluetooth keyboard connected" : "");
  } else if (bleKb) {
    diagAdd("keyboard", "Keyboard", DIAG_WARN,
            "The built-in keyboard isn't detected (a Bluetooth keyboard is working). Check the keyboard cable is pushed in at both ends.",
            "CardKB not found at 0x%02X", CARDKB_ADDR);
  } else {
    diagAdd("keyboard", "Keyboard", DIAG_FAIL,
            "The keyboard isn't detected. Check its cable is pushed in firmly at both ends.",
            "CardKB not found at 0x%02X", CARDKB_ADDR);
  }
}

static void diagCheckBattery() {
  if (hasMAX17048) {
    float v = maxlipo.cellVoltage();
    float pct = maxlipo.cellPercent();
    bool low = v > 0.5f && v < 3.5f;
    diagAdd("battery", "Battery", low ? DIAG_WARN : DIAG_PASS, low ? "The battery is low. Charge it over USB." : nullptr,
            "%.2f V, %.0f%% (MAX17048)", v, pct);
  } else if (hasLC709203) {
    float v = lc.cellVoltage();
    float pct = lc.cellPercent();
    bool low = v > 0.5f && v < 3.5f;
    diagAdd("battery", "Battery", low ? DIAG_WARN : DIAG_PASS, low ? "The battery is low. Charge it over USB." : nullptr,
            "%.2f V, %.0f%% (LC709203F)", v, pct);
  } else {
    diagAdd("battery", "Battery", DIAG_WARN,
            "The battery gauge wasn't found, so battery level can't be shown. The Summit still runs on battery and USB.",
            "no gauge at 0x36 or 0x0B");
  }
}

static void diagCheckAudio() {
  if (!isI2SInitialized()) {
    diagAdd("audio", "Audio", DIAG_FAIL,
            "The audio chip didn't start. Restart the Summit; if it still fails, check the amplifier board solder joints.",
            "I2S driver not running");
    return;
  }
  int vol = getVolume();
  if (vol == 0) {
    diagAdd("audio", "Audio", DIAG_WARN, "Volume is set to 0. Press V on a menu screen to turn it up.",
            "ready, volume 0%%");
  } else {
    diagAdd("audio", "Audio", DIAG_PASS, nullptr, "ready, volume %d%%", vol);
  }
}

static void diagCheckWiFi() {
  int saved = 0;
  {
    Preferences p;
    if (p.begin("wifi", true)) {
      const char* keys[3] = { "ssid1", "ssid2", "ssid3" };
      for (int i = 0; i < 3; i++) {
        char ssid[33] = "";
        if (p.isKey(keys[i])) p.getString(keys[i], ssid, sizeof(ssid));
        if (ssid[0]) saved++;
      }
      p.end();
    }
  }
  if (!WiFi.isConnected()) {
    if (saved == 0) {
      diagAdd("wifi", "WiFi", DIAG_INFO, nullptr, "no networks saved");
    } else {
      diagAdd("wifi", "WiFi", DIAG_WARN,
              "Not connected. Check the WiFi network is on and in range, or re-enter the password in Settings > Device Settings > WiFi.",
              "not connected (%d saved network%s)", saved, saved == 1 ? "" : "s");
    }
    return;
  }
  int rssi = WiFi.RSSI();
  InternetStatus inet = getInternetStatus();
  if (inet == INET_WIFI_ONLY) {
    diagAdd("wifi", "WiFi", DIAG_WARN, "Connected to WiFi but the internet isn't reachable. Check the router's internet connection.",
            "connected, %d dBm, no internet", rssi);
  } else if (rssi < -80) {
    diagAdd("wifi", "WiFi", DIAG_WARN, "The WiFi signal is weak. Move closer to the router.",
            "connected, weak signal %d dBm", rssi);
  } else {
    diagAdd("wifi", "WiFi", DIAG_PASS, nullptr, "connected, %d dBm%s", rssi,
            inet == INET_CONNECTED ? ", internet OK" : "");
  }
}

static const char* diagBleStateName(BLEKBHostState s) {
  switch (s) {
    case BLEKB_STATE_IDLE:          return "idle";
    case BLEKB_STATE_READY:         return "ready";
    case BLEKB_STATE_SCANNING:      return "scanning";
    case BLEKB_STATE_SCAN_COMPLETE: return "scan done";
    case BLEKB_STATE_CONNECTING:    return "connecting";
    case BLEKB_STATE_CONNECTED:     return "connected";
    case BLEKB_STATE_DISCONNECTED:  return "disconnected";
    default:                        return "error";
  }
}

static void diagCheckBluetooth() {
  if (!bleKBHost.pairedDevice.valid) {
    diagAdd("bluetooth", "Bluetooth keyboard", DIAG_INFO, nullptr, "none paired");
    return;
  }
  const char* name = bleKBHost.pairedDevice.name;   // escaped when emitted
  if (isBLEKeyboardConnected()) {
    diagAdd("bluetooth", "Bluetooth keyboard", DIAG_PASS, nullptr, "%s connected", name);
  } else {
    diagAdd("bluetooth", "Bluetooth keyboard", DIAG_INFO, nullptr, "%s paired, %s%s%s", name,
            diagBleStateName(bleKBHost.state), bleKBHost.lastError[0] ? ": " : "", bleKBHost.lastError);
  }
}

static void diagCheckHistory() {
  char modes[48];
  diagFormatModeHistory(modes, sizeof(modes));
  diagAdd("history", "Event history", DIAG_INFO, nullptr, "%d event(s) since power-on, boot %u, recent screens %s",
          diagEventCount(), (unsigned)diagBootCount(), modes[0] ? modes : "-");
}

// ============================================
// Runner
// ============================================

typedef void (*DiagStepFn)();
static const DiagStepFn diagSteps[] = {
  diagCheckFirmware,
  diagCheckReset,
  diagCheckMemory,
  diagCheckSettingsStore,
  diagCheckSDAll,
  diagCheckKeyboard,
  diagCheckBattery,
  diagCheckAudio,
  diagCheckWiFi,
  diagCheckBluetooth,
  diagCheckHistory,
};
#define DIAG_STEP_COUNT ((int)(sizeof(diagSteps) / sizeof(diagSteps[0])))

static int diagStepIndex = -1;   // -1 = idle

void diagEmitHello() {
  char call[24];
  diagJsonEscape(call, sizeof(call), strcmp(vailCallsign, "GUEST") == 0 ? "" : vailCallsign);
  diagEmit("{\"t\":\"hello\",\"proto\":%d,\"fw\":\"%s\",\"date\":\"%s\",\"web\":\"%s\",\"call\":\"%s\",\"psram\":%s,\"uptime\":%lu}",
           DIAG_PROTOCOL_VERSION, FIRMWARE_VERSION, FIRMWARE_DATE, WEB_FILES_VERSION, call,
           psramFound() ? "true" : "false", millis() / 1000);
}

bool diagRunActive() { return diagStepIndex >= 0; }
int diagRunStep() { return diagStepIndex; }
int diagRunTotal() { return DIAG_STEP_COUNT; }

bool diagRunStart() {
  if (diagRunActive()) return false;
  if (!diagEnsureResults()) return false;
  memset(diagResults, 0, sizeof(DiagResult) * DIAG_MAX_RESULTS);
  diagResultCountV = 0;
  diagStepIndex = 0;
  diagEmitHello();
  diagEmit("{\"t\":\"diag_start\",\"steps\":%d}", DIAG_STEP_COUNT);
  return true;
}

void diagEmitSummary() {
  int counts[4] = { 0, 0, 0, 0 };
  for (int i = 0; i < diagResultCountV; i++) counts[diagResults[i].st]++;
  diagEmit("{\"t\":\"diag_end\",\"pass\":%d,\"warn\":%d,\"fail\":%d,\"info\":%d}",
           counts[DIAG_PASS], counts[DIAG_WARN], counts[DIAG_FAIL], counts[DIAG_INFO]);
}

// Runs one check per call. Returns true when the run just finished.
bool diagRunPoll() {
  if (!diagRunActive()) return false;
  diagSteps[diagStepIndex]();
  diagStepIndex++;
  if (diagStepIndex >= DIAG_STEP_COUNT) {
    diagStepIndex = -1;
    diagEmitSummary();
    return true;
  }
  return false;
}

// Re-send the last run's results (the page asks for this after connecting
// to a Summit that already ran its check on-screen)
void diagEmitLastResults() {
  diagEmitHello();
  if (diagResultCountV == 0) {
    diagEmit("{\"t\":\"no_results\"}");
    return;
  }
  diagEmit("{\"t\":\"diag_start\",\"steps\":%d}", diagResultCountV);
  for (int i = 0; i < diagResultCountV; i++) diagEmitResult(&diagResults[i]);
  diagEmitSummary();
}

void diagEmitEvents() {
  int n = diagEventCount();
  for (int i = 0; i < n; i++) {
    const DiagEventRec* e = diagEventAt(i);
    if (!e) break;
    char msg[96], tag[16];
    diagJsonEscape(msg, sizeof(msg), e->msg);
    diagJsonEscape(tag, sizeof(tag), e->tag);
    diagEmit("{\"t\":\"event\",\"boot\":%u,\"ms\":%lu,\"tag\":\"%s\",\"msg\":\"%s\"}",
             (unsigned)e->boot, (unsigned long)e->ms, tag, msg);
  }
  char modes[48];
  diagFormatModeHistory(modes, sizeof(modes));
  diagEmit("{\"t\":\"events_end\",\"count\":%d,\"boot\":%u,\"modes\":\"%s\"}", n, (unsigned)diagBootCount(), modes);
}

// ============================================
// Background monitor (called from loop)
// ============================================

void diagMonitorPoll() {
  static unsigned long last = 0;
  unsigned long now = millis();
  if (now - last < 250) return;
  last = now;

  static int lastMode = -1;
  if ((int)currentMode != lastMode) {
    lastMode = (int)currentMode;
    diagRecordMode(lastMode);
  }

  static bool lastWifi = false;
  bool wifi = WiFi.isConnected();
  if (wifi != lastWifi) {
    lastWifi = wifi;
    if (wifi) diagEvent("WIFI", "connected, %d dBm", WiFi.RSSI());
    else diagEvent("WIFI", "disconnected");
  }

  static BLEKBHostState lastBle = BLEKB_STATE_IDLE;
  if (bleKBHost.state != lastBle) {
    BLEKBHostState s = bleKBHost.state;
    lastBle = s;
    if (s == BLEKB_STATE_ERROR) {
      diagEvent("BTKB", "error: %s", bleKBHost.lastError);
    } else if (s == BLEKB_STATE_CONNECTED || s == BLEKB_STATE_DISCONNECTED) {
      diagEvent("BTKB", "%s", diagBleStateName(s));
    }
  }

  diagSessionPoll();
}

#endif // DIAG_CHECKS_H
