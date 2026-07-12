/*
 * VAIL SUMMIT - Satellite Transmitter Database
 * Fetches the SatNOGS DB transmitter list (community-maintained), keeps only
 * active entries for satellites present in the TLE catalog, and caches them
 * beside the TLEs (SD preferred, internal SPIFFS fallback) so all frequency
 * data is available offline in the field.
 */

#ifndef SAT_XMTRS_H
#define SAT_XMTRS_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include "sat_data.h"

#define SAT_XMTR_URL      "https://db.satnogs.org/api/transmitters/?format=json"
#define SAT_XMTR_FILE     "/sat/freqs.txt"
#define SAT_XMTR_MAX      800
#define SAT_XMTR_PER_SAT  8
#define SAT_XMTR_TIMEOUT  30000

struct SatTransmitter {
    uint32_t norad;
    uint32_t downHz;      // 0 = none
    uint32_t upHz;        // 0 = none / receive-only
    uint8_t invert;       // inverting transponder
    char mode[14];        // "FM", "SSB", "CW", "GMSK9k6", ...
    char desc[40];        // "Mode U/V FM Voice Repeater"
};

static SatTransmitter* satXmtrs = NULL;
static int satXmtrCount = 0;
static bool satXmtrsLoaded = false;   // load-from-storage attempted

static bool initSatXmtrs() {
    if (satXmtrs) return true;
    size_t totalSize = sizeof(SatTransmitter) * SAT_XMTR_MAX;
    if (psramFound()) {
        satXmtrs = (SatTransmitter*)ps_malloc(totalSize);
    }
    if (!satXmtrs) {
        satXmtrs = (SatTransmitter*)malloc(totalSize);
    }
    if (!satXmtrs) {
        Serial.println("[SAT] ERROR: transmitter table allocation failed");
        return false;
    }
    memset(satXmtrs, 0, totalSize);
    satXmtrCount = 0;
    return true;
}

static bool satCatalogHasNorad(uint32_t norad) {
    for (int i = 0; i < satCatalog.count; i++) {
        if (satCatalog.sats[i].norad == norad) return true;
    }
    return false;
}

// Number of stored transmitters for a satellite
int satXmtrCountFor(uint32_t norad) {
    int n = 0;
    for (int i = 0; i < satXmtrCount; i++) {
        if (satXmtrs[i].norad == norad) n++;
    }
    return n;
}

// nth (0-based) transmitter for a satellite, or NULL
const SatTransmitter* satXmtrFor(uint32_t norad, int nth) {
    for (int i = 0; i < satXmtrCount; i++) {
        if (satXmtrs[i].norad != norad) continue;
        if (nth-- == 0) return &satXmtrs[i];
    }
    return NULL;
}

// Best single entry for compact displays: prefer two-way voice (repeater /
// transponder, i.e. has both uplink and downlink), else anything with a
// downlink, else the first entry.
const SatTransmitter* satBestXmtr(uint32_t norad) {
    const SatTransmitter* withBoth = NULL;
    const SatTransmitter* withDown = NULL;
    const SatTransmitter* any = NULL;
    for (int i = 0; i < satXmtrCount; i++) {
        SatTransmitter& x = satXmtrs[i];
        if (x.norad != norad) continue;
        if (!any) any = &x;
        if (x.downHz && !withDown) withDown = &x;
        if (x.downHz && x.upHz && !withBoth) withBoth = &x;
    }
    if (withBoth) return withBoth;
    if (withDown) return withDown;
    return any;
}

// "437.200" from Hz; "-" when absent
void satFmtMHz(uint32_t hz, char* buf, size_t n) {
    if (hz == 0) { strlcpy(buf, "-", n); return; }
    snprintf(buf, n, "%lu.%03lu", (unsigned long)(hz / 1000000UL),
             (unsigned long)((hz % 1000000UL) / 1000UL));
}

static void satAddXmtr(uint32_t norad, uint32_t down, uint32_t up, bool invert,
                       const char* mode, const char* desc) {
    if (satXmtrCount >= SAT_XMTR_MAX) return;
    if (satXmtrCountFor(norad) >= SAT_XMTR_PER_SAT) return;
    SatTransmitter& x = satXmtrs[satXmtrCount++];
    memset(&x, 0, sizeof(SatTransmitter));
    x.norad = norad;
    x.downHz = down;
    x.upHz = up;
    x.invert = invert ? 1 : 0;
    strlcpy(x.mode, mode ? mode : "", sizeof(x.mode));
    strlcpy(x.desc, desc ? desc : "", sizeof(x.desc));
}

// ============================================
// Storage (mirrors the TLE cache: SD then SPIFFS)
// ============================================

static bool satWriteXmtrFile(fs::FS& fs) {
    File f = fs.open(SAT_XMTR_FILE, FILE_WRITE);
    if (!f) return false;
    for (int i = 0; i < satXmtrCount; i++) {
        SatTransmitter& x = satXmtrs[i];
        // desc is last so embedded commas are harmless
        f.printf("%lu,%lu,%lu,%u,%s,%s\n", (unsigned long)x.norad,
                 (unsigned long)x.downHz, (unsigned long)x.upHz,
                 (unsigned)x.invert, x.mode, x.desc);
    }
    f.close();
    return true;
}

bool satSaveXmtrs() {
    if (satXmtrCount == 0) return false;
    if (!sdCardAvailable) initSDCard();
    if (sdCardAvailable) {
        if (!SD.exists(SAT_SD_DIR)) SD.mkdir(SAT_SD_DIR);
        if (satWriteXmtrFile(SD)) {
            Serial.printf("[SAT] Saved %d transmitters to SD\n", satXmtrCount);
            return true;
        }
    }
    if (satEnsureFlashFS() && satWriteXmtrFile(SPIFFS)) {
        Serial.printf("[SAT] Saved %d transmitters to flash\n", satXmtrCount);
        return true;
    }
    Serial.println("[SAT] ERROR: transmitter cache not saved");
    return false;
}

static bool satReadXmtrFile(fs::FS& fs) {
    File f = fs.open(SAT_XMTR_FILE);
    if (!f) return false;
    satXmtrCount = 0;
    char line[128];
    while (f.available() && satXmtrCount < SAT_XMTR_MAX) {
        int n = f.readBytesUntil('\n', line, sizeof(line) - 1);
        line[n] = '\0';
        // norad,down,up,invert,mode,desc
        char* p = line;
        uint32_t norad = strtoul(p, &p, 10); if (*p != ',') continue; p++;
        uint32_t down = strtoul(p, &p, 10);  if (*p != ',') continue; p++;
        uint32_t up = strtoul(p, &p, 10);    if (*p != ',') continue; p++;
        uint32_t inv = strtoul(p, &p, 10);   if (*p != ',') continue; p++;
        char* modeStart = p;
        char* comma = strchr(p, ',');
        if (!comma) continue;
        *comma = '\0';
        char* desc = comma + 1;
        // strip trailing CR
        char* cr = strchr(desc, '\r');
        if (cr) *cr = '\0';
        if (norad == 0) continue;
        satAddXmtr(norad, down, up, inv != 0, modeStart, desc);
    }
    f.close();
    return satXmtrCount > 0;
}

bool satLoadXmtrsFromStorage() {
    if (satXmtrsLoaded) return satXmtrCount > 0;
    satXmtrsLoaded = true;
    if (!initSatXmtrs()) return false;

    if (!sdCardAvailable) initSDCard();
    if (sdCardAvailable && SD.exists(SAT_XMTR_FILE) && satReadXmtrFile(SD)) {
        Serial.printf("[SAT] Loaded %d transmitters from SD\n", satXmtrCount);
        return true;
    }
    if (satEnsureFlashFS() && SPIFFS.exists(SAT_XMTR_FILE) && satReadXmtrFile(SPIFFS)) {
        Serial.printf("[SAT] Loaded %d transmitters from flash\n", satXmtrCount);
        return true;
    }
    return false;
}

// ============================================
// SatNOGS Fetch (streaming)
// ============================================

// Parse one JSON object from the stream and keep it if relevant.
// One reused document - allocating a fresh 2KB doc for each of the ~3200
// stream objects churns the heap for no benefit.
static void satParseXmtrObject(const char* obj) {
    static DynamicJsonDocument doc(2048);
    doc.clear();
    if (deserializeJson(doc, obj) != DeserializationError::Ok) return;

    const char* status = doc["status"] | "";
    if (strcmp(status, "active") != 0) return;

    uint32_t norad = doc["norad_cat_id"] | 0UL;
    if (norad == 0 || !satCatalogHasNorad(norad)) return;

    uint32_t down = doc["downlink_low"] | 0UL;
    uint32_t up = doc["uplink_low"] | 0UL;
    if (down == 0 && up == 0) return;

    satAddXmtr(norad, down, up, doc["invert"] | false,
               doc["mode"] | "", doc["description"] | "");
}

// Download the full SatNOGS transmitter list (~2-3MB JSON), stream-splitting
// it into per-transmitter objects so only ~2KB is ever held at once. Only
// entries for catalog satellites survive the filter (~few hundred records).
// Blocking - run behind a loading overlay like the TLE fetch.
bool satFetchTransmitters() {
    if (!satCatalog.valid || WiFi.status() != WL_CONNECTED) return false;
    if (!initSatXmtrs()) return false;

    HTTPClient http;
    http.setTimeout(SAT_XMTR_TIMEOUT);
    http.setReuse(false);
    http.begin(SAT_XMTR_URL);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[SAT] Transmitter fetch failed: HTTP %d\n", code);
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    satXmtrCount = 0;

    static char obj[2048];
    static char chunk[512];
    int oPos = -1;             // -1 = between objects
    int depth = 0;
    bool inStr = false, esc = false;
    uint32_t lastData = millis();
    uint32_t started = millis();
    uint32_t chunks = 0;

    while (http.connected() || stream->available()) {
        if (millis() - started > 120000UL) break;       // hard cap: 2 minutes
        // loopTask is subscribed to the task WDT (see vail-summit.ino setup)
        // and only loop() feeds it - delay() does NOT. This blocking fetch
        // runs far past the timeout on the ~2-3MB stream, so it must feed
        // the watchdog itself (same as the BLE host's long operations).
        esp_task_wdt_reset();
        int avail = stream->available();
        if (avail <= 0) {
            if (millis() - lastData > 15000UL) break;   // stalled
            delay(1);
            continue;
        }
        if ((++chunks & 0x03) == 0) delay(1);           // let WiFi breathe
        lastData = millis();
        int n = stream->readBytes(chunk, min(avail, (int)sizeof(chunk)));
        for (int i = 0; i < n; i++) {
            char c = chunk[i];
            if (oPos < 0) {
                if (c == '{') { depth = 1; oPos = 0; obj[oPos++] = c; inStr = false; esc = false; }
                continue;
            }
            if (oPos < (int)sizeof(obj) - 1) obj[oPos++] = c;
            if (esc) { esc = false; continue; }
            if (inStr) {
                if (c == '\\') esc = true;
                else if (c == '"') inStr = false;
                continue;
            }
            if (c == '"') inStr = true;
            else if (c == '{') depth++;
            else if (c == '}') {
                if (--depth == 0) {
                    obj[oPos] = '\0';
                    satParseXmtrObject(obj);
                    oPos = -1;
                }
            }
        }
    }
    http.end();

    Serial.printf("[SAT] Kept %d transmitters for %d catalog sats\n",
                  satXmtrCount, satCatalog.count);
    if (satXmtrCount > 0) {
        satXmtrsLoaded = true;
        satSaveXmtrs();
        return true;
    }
    return false;
}

#endif // SAT_XMTRS_H
