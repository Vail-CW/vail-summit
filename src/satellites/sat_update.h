/*
 * VAIL SUMMIT - Satellite Data Update Job
 * Non-blocking, staged download of TLEs (Celestrak) + transmitters (SatNOGS).
 * Driven by an LVGL timer tick (satUpdateJobTick) so the UI keeps animating
 * and the loop task keeps feeding its watchdog - the earlier blocking
 * implementation froze the screen for ~40s and tripped the loop-hang WDT.
 */

#ifndef SAT_UPDATE_H
#define SAT_UPDATE_H

#include <Arduino.h>
#include <HTTPClient.h>
#include "sat_data.h"
#include "sat_xmtrs.h"

enum SatUpdStage : uint8_t {
    SATUPD_IDLE = 0,
    SATUPD_TLE_AMATEUR,   // step 1/3
    SATUPD_TLE_ISS,       // step 2/3
    SATUPD_XMTRS,         // step 3/3
};

struct SatUpdateJob {
    SatUpdStage stage;
    bool httpOpen;
    uint32_t stageBytes;
    int stageTotal;          // content-length, -1 when chunked/unknown
    uint32_t lastData;
    // TLE line assembly
    char line[96];
    int linePos;
    SatTLEParseState tleSt;
    // transmitter JSON object splitter
    int oPos;                // -1 = between objects
    int depth;
    bool inStr, esc;
    // result
    bool finishedOk;
};

static SatUpdateJob satUpd = {};
static HTTPClient satUpdHttp;        // reused across stages; too big for stack
static char satUpdObj[2048];
static char satUpdChunk[512];

bool satUpdateJobActive() { return satUpd.stage != SATUPD_IDLE; }

static void satUpdResetStageState() {
    satUpd.stageBytes = 0;
    satUpd.stageTotal = -1;
    satUpd.linePos = 0;
    memset(&satUpd.tleSt, 0, sizeof(satUpd.tleSt));
    satUpd.oPos = -1;
    satUpd.depth = 0;
    satUpd.inStr = false;
    satUpd.esc = false;
}

// Begin the staged update. Caller must have WiFi up.
bool satUpdateJobStart() {
    if (satUpdateJobActive()) return true;
    if (WiFi.status() != WL_CONNECTED) return false;
    if (!initSatCatalog() || !initSatXmtrs()) return false;

    memset(&satUpd, 0, sizeof(satUpd));
    // Rebuild from scratch so decayed/renamed birds don't linger
    satCatalog.count = 0;
    satCatalog.valid = false;
    satXmtrCount = 0;
    satUpd.stage = SATUPD_TLE_AMATEUR;
    satUpdResetStageState();
    return true;
}

static const char* satUpdStageUrl() {
    switch (satUpd.stage) {
        case SATUPD_TLE_AMATEUR: return SAT_TLE_URL_AMATEUR;
        case SATUPD_TLE_ISS:     return SAT_TLE_URL_ISS;
        case SATUPD_XMTRS:       return SAT_XMTR_URL;
        default:                 return NULL;
    }
}

static void satUpdAdvanceStage() {
    if (satUpd.httpOpen) {
        satUpdHttp.end();
        satUpd.httpOpen = false;
    }
    switch (satUpd.stage) {
        case SATUPD_TLE_AMATEUR:
            satUpd.stage = SATUPD_TLE_ISS;
            break;
        case SATUPD_TLE_ISS:
            if (satCatalog.count > 0) {
                satCatalog.valid = true;
                satCatalog.tleFetchUnix = ntpSynced ? time(nullptr) : 0;
                satUpd.stage = SATUPD_XMTRS;
            } else {
                // Both TLE fetches failed - restore the cached catalog
                satLoadTLEsFromStorage();
                satUpd.finishedOk = false;
                satUpd.stage = SATUPD_IDLE;
            }
            break;
        case SATUPD_XMTRS:
            satSaveTLEs();
            if (satXmtrCount > 0) {
                satXmtrsLoaded = true;
                satSaveXmtrs();
            }
            satUpd.finishedOk = true;   // TLEs are in; missing freqs tolerated
            satUpd.stage = SATUPD_IDLE;
            break;
        default:
            satUpd.stage = SATUPD_IDLE;
            break;
    }
    satUpdResetStageState();
}

static void satUpdConsumeByte(char c) {
    if (satUpd.stage == SATUPD_XMTRS) {
        // Split the JSON array into per-transmitter objects
        if (satUpd.oPos < 0) {
            if (c == '{') {
                satUpd.depth = 1;
                satUpd.oPos = 0;
                satUpdObj[satUpd.oPos++] = c;
                satUpd.inStr = false;
                satUpd.esc = false;
            }
            return;
        }
        if (satUpd.oPos < (int)sizeof(satUpdObj) - 1) satUpdObj[satUpd.oPos++] = c;
        if (satUpd.esc) { satUpd.esc = false; return; }
        if (satUpd.inStr) {
            if (c == '\\') satUpd.esc = true;
            else if (c == '"') satUpd.inStr = false;
            return;
        }
        if (c == '"') satUpd.inStr = true;
        else if (c == '{') satUpd.depth++;
        else if (c == '}') {
            if (--satUpd.depth == 0) {
                satUpdObj[satUpd.oPos] = '\0';
                satParseXmtrObject(satUpdObj);
                satUpd.oPos = -1;
            }
        }
        return;
    }

    // TLE stages: assemble lines
    if (c == '\n') {
        satUpd.line[satUpd.linePos] = '\0';
        satParseTLELine(satUpd.tleSt, satUpd.line);
        satUpd.linePos = 0;
    } else if (satUpd.linePos < (int)sizeof(satUpd.line) - 1) {
        satUpd.line[satUpd.linePos++] = c;
    }
}

// One tick of work, bounded by budgetMs. Call from an LVGL timer.
void satUpdateJobTick(uint32_t budgetMs) {
    if (!satUpdateJobActive()) return;
    uint32_t t0 = millis();

    if (!satUpd.httpOpen) {
        // Stage kickoff: TLS handshake blocks ~1-3s. One hitch per stage,
        // well under the loop WDT timeout; a frame renders right after.
        const char* url = satUpdStageUrl();
        if (!url) { satUpd.stage = SATUPD_IDLE; return; }
        satUpdHttp.setTimeout(15000);
        satUpdHttp.setReuse(false);
        satUpdHttp.begin(url);
        int code = satUpdHttp.GET();
        if (code != HTTP_CODE_OK) {
            Serial.printf("[SAT] update stage %d failed: HTTP %d\n", satUpd.stage, code);
            satUpdHttp.end();
            satUpdAdvanceStage();
            return;
        }
        satUpd.stageTotal = satUpdHttp.getSize();
        satUpd.lastData = millis();
        satUpd.httpOpen = true;
        return;
    }

    WiFiClient* stream = satUpdHttp.getStreamPtr();
    while ((millis() - t0) < budgetMs) {
        if (!satUpdHttp.connected() && !stream->available()) {
            satUpdAdvanceStage();       // stage body fully consumed
            return;
        }
        int avail = stream->available();
        if (avail <= 0) {
            if (millis() - satUpd.lastData > 15000UL) {
                Serial.printf("[SAT] update stage %d stalled\n", satUpd.stage);
                satUpdAdvanceStage();
                return;
            }
            return;                     // nothing yet - let the UI animate
        }
        satUpd.lastData = millis();
        int n = stream->readBytes(satUpdChunk, min(avail, (int)sizeof(satUpdChunk)));
        satUpd.stageBytes += (uint32_t)n;
        for (int i = 0; i < n; i++) {
            satUpdConsumeByte(satUpdChunk[i]);
        }
    }
}

#endif // SAT_UPDATE_H
