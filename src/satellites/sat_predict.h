/*
 * VAIL SUMMIT - Satellite Pass Prediction
 * Chunked SGP4 pass search + live look-angle tracking, built on the vendored
 * propagator in src/thirdparty/sgp4/. All heavy math runs in small time
 * budgets from an LVGL timer so the UI stays responsive (no blocking loops).
 */

#ifndef SAT_PREDICT_H
#define SAT_PREDICT_H

#include <Arduino.h>
#include <time.h>
#include "../thirdparty/sgp4/sgp4.h"
#include "../settings/settings_satellites.h"
#include "sat_data.h"

// ============================================
// Maidenhead Grid -> Lat/Lon
// ============================================

// Accepts 4/6/8-char grids; returns the square/subsquare center.
bool gridToLatLon(const char* grid, double* lat, double* lon) {
    if (!grid) return false;
    int len = strlen(grid);
    if (len < 4) return false;

    char g[9];
    strlcpy(g, grid, sizeof(g));
    for (int i = 0; i < (int)strlen(g); i++) g[i] = toupper((unsigned char)g[i]);

    if (g[0] < 'A' || g[0] > 'R' || g[1] < 'A' || g[1] > 'R') return false;
    if (g[2] < '0' || g[2] > '9' || g[3] < '0' || g[3] > '9') return false;

    double lonV = (g[0] - 'A') * 20.0 - 180.0 + (g[2] - '0') * 2.0;
    double latV = (g[1] - 'A') * 10.0 - 90.0 + (g[3] - '0') * 1.0;

    if (len >= 6 && g[4] >= 'A' && g[4] <= 'X' && g[5] >= 'A' && g[5] <= 'X') {
        lonV += (g[4] - 'A') * (2.0 / 24.0) + (1.0 / 24.0);
        latV += (g[5] - 'A') * (1.0 / 24.0) + (0.5 / 24.0);
    } else {
        lonV += 1.0;   // center of 2 deg square
        latV += 0.5;
    }

    *lat = latV;
    *lon = lonV;
    return true;
}

// ============================================
// Formatting Helpers
// ============================================

// 16-point compass direction for an azimuth in degrees
const char* azToCompass(float az) {
    static const char* dirs[16] = {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
    };
    int idx = (int)((az + 11.25f) / 22.5f) & 15;
    return dirs[idx];
}

// Local wall-clock from UTC using the shared UTC offset ("14:32")
void satFmtLocalTime(time_t utc, char* out, size_t n) {
    time_t local = utc + (time_t)utcOffsetMinutes * 60;
    struct tm tmv;
    gmtime_r(&local, &tmv);
    snprintf(out, n, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
}

// Local date ("Sat 07/12")
void satFmtLocalDate(time_t utc, char* out, size_t n) {
    static const char* days[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    time_t local = utc + (time_t)utcOffsetMinutes * 60;
    struct tm tmv;
    gmtime_r(&local, &tmv);
    snprintf(out, n, "%s %02d/%02d", days[tmv.tm_wday], tmv.tm_mon + 1, tmv.tm_mday);
}

// Zulu time ("19:32z")
void satFmtZuluTime(time_t utc, char* out, size_t n) {
    struct tm tmv;
    gmtime_r(&utc, &tmv);
    snprintf(out, n, "%02d:%02dz", tmv.tm_hour, tmv.tm_min);
}

// Countdown "1h 02m 33s" / "02m 33s"
void satFmtCountdown(long secs, char* out, size_t n) {
    if (secs < 0) secs = 0;
    long h = secs / 3600, m = (secs % 3600) / 60, s = secs % 60;
    if (h > 0) snprintf(out, n, "%ldh %02ldm %02lds", h, m, s);
    else snprintf(out, n, "%02ldm %02lds", m, s);
}

// ============================================
// Pass Search State Machine
// ============================================

#define SAT_MAX_PASSES 20

struct SatPass {
    time_t aos;
    time_t los;
    time_t tmax;
    float aosAz;
    float losAz;
    float maxAz;
    float maxEl;
};

struct SatPassSearch {
    bool active;         // search in progress
    bool done;           // search complete, passes[] valid
    int count;
    SatPass passes[SAT_MAX_PASSES];
    double startJd;
    double endJd;
    int catalogIdx;      // catalog index being searched
};

static SatPassSearch satSearch = { false, false, 0, {}, 0.0, 0.0, -1 };

// One propagator instance shared by search + live view (elsetrec is large;
// keep it out of stack frames).
static vsgp4::Sgp4 satPredictor;

// Initialize the predictor for a catalog entry + current site. Returns false
// on bad grid or missing TLE.
static bool satPredictorSetup(int catalogIdx) {
    if (!satCatalog.valid || catalogIdx < 0 || catalogIdx >= satCatalog.count) return false;

    double lat, lon;
    if (!gridToLatLon(satEffectiveGrid(), &lat, &lon)) return false;

    SatEntry& e = satCatalog.sats[catalogIdx];
    // twoline2rv() mutates the TLE strings while parsing, so hand the
    // propagator scratch copies - the catalog entries must stay pristine
    // (they get re-saved to SD and re-parsed on every screen entry).
    static char l1[130], l2[130];
    strlcpy(l1, e.line1, sizeof(l1));
    strlcpy(l2, e.line2, sizeof(l2));
    // init() no-ops when the same TLE is already loaded - that's fine
    satPredictor.init(e.name, l1, l2);
    satPredictor.site(lat, lon, 100.0);  // altitude barely matters for pass times
    return true;
}

// Begin a pass search for a catalog entry over an arbitrary window.
bool satStartPassSearchAt(int catalogIdx, time_t startUnix, double windowHours) {
    memset(&satSearch, 0, sizeof(satSearch));
    satSearch.catalogIdx = catalogIdx;

    if (!ntpSynced) return false;
    if (!satPredictorSetup(catalogIdx)) return false;

    satSearch.startJd = vsgp4::getJulianFromUnix((double)startUnix);
    satSearch.endJd = satSearch.startJd + windowHours / 24.0;

    if (!satPredictor.initpredpoint((unsigned long)startUnix, 0.0)) {
        Serial.println("[SAT] initpredpoint failed");
        return false;
    }
    satSearch.active = true;
    return true;
}

// Begin a pass search over the configured lookahead window starting now.
bool satStartPassSearch(int catalogIdx) {
    return satStartPassSearchAt(catalogIdx, time(nullptr), (double)satSettings.lookaheadHours);
}

// Run search iterations for up to budgetMs. Each nextpass(1 iteration) call
// advances the predictor by roughly one orbit. Returns true when finished.
bool satPassSearchStep(uint32_t budgetMs) {
    if (!satSearch.active) return true;

    uint32_t t0 = millis();
    vsgp4::passinfo p;

    while ((millis() - t0) < budgetMs) {
        if (satSearch.count >= SAT_MAX_PASSES ||
            satPredictor.getpredpoint() > satSearch.endJd) {
            satSearch.active = false;
            satSearch.done = true;
            return true;
        }

        bool found = satPredictor.nextpass(&p, 1, false, (double)satSettings.minElevation);
        if (!found) continue;  // this orbit peaked below min elevation - keep going

        if (p.jdstop < satSearch.startJd) continue;      // pass already over
        if (p.jdstart > satSearch.endJd) {               // beyond the window
            satSearch.active = false;
            satSearch.done = true;
            return true;
        }

        SatPass& sp = satSearch.passes[satSearch.count++];
        sp.aos = (time_t)vsgp4::getUnixFromJulian(p.jdstart);
        sp.los = (time_t)vsgp4::getUnixFromJulian(p.jdstop);
        sp.tmax = (time_t)vsgp4::getUnixFromJulian(p.jdmax);
        sp.aosAz = (float)p.azstart;
        sp.losAz = (float)p.azstop;
        sp.maxAz = (float)p.azmax;
        sp.maxEl = (float)p.maxelevation;
    }
    return false;  // budget exhausted, more to do
}

// Search progress 0-100 for the progress label
int satPassSearchProgress() {
    if (satSearch.done) return 100;
    if (!satSearch.active) return 0;
    double span = satSearch.endJd - satSearch.startJd;
    if (span <= 0.0) return 100;
    double frac = (satPredictor.getpredpoint() - satSearch.startJd) / span;
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;
    return (int)(frac * 100.0);
}

// ============================================
// Live Tracking
// ============================================

struct SatLiveState {
    bool valid;
    float az;
    float el;
    float distKm;
};

// Compute current look angles for a catalog entry. Cheap enough for a 1 Hz
// LVGL timer (single SGP4 evaluation).
bool satLiveUpdate(int catalogIdx, SatLiveState* out) {
    memset(out, 0, sizeof(SatLiveState));
    if (!ntpSynced) return false;
    if (!satPredictorSetup(catalogIdx)) return false;

    satPredictor.findsat((unsigned long)time(nullptr));
    out->az = (float)satPredictor.satAz;
    out->el = (float)satPredictor.satEl;
    out->distKm = (float)satPredictor.satDist;
    out->valid = true;
    return true;
}

#endif // SAT_PREDICT_H
