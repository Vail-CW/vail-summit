/*
 * VAIL SUMMIT - Satellite Catalog & TLE Management
 * Fetches TLE orbital elements from Celestrak, caches them on the SD card,
 * and holds the parsed catalog in PSRAM for the satellite tracker screens.
 */

#ifndef SAT_DATA_H
#define SAT_DATA_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include "../storage/sd_card.h"

// From ntp_time.h (included earlier in the translation unit)
extern bool ntpSynced;

// ============================================
// Configuration
// ============================================

#define MAX_SATELLITES 160
#define SAT_TLE_TIMEOUT 20000

// Celestrak amateur group + ISS (ISS lives in the stations group, not amateur,
// and is the main SSTV bird - ARISS events transmit on 145.800 FM)
#define SAT_TLE_URL_AMATEUR "https://celestrak.org/NORAD/elements/gp.php?GROUP=amateur&FORMAT=tle"
#define SAT_TLE_URL_ISS     "https://celestrak.org/NORAD/elements/gp.php?CATNR=25544&FORMAT=tle"

#define SAT_SD_DIR       "/satellites"
#define SAT_SD_TLE_FILE  "/satellites/tle.txt"
#define SAT_SD_META_FILE "/satellites/tle_meta.txt"

// TLE age thresholds (days)
#define SAT_TLE_WARN_DAYS  7
#define SAT_TLE_STALE_DAYS 14

// ============================================
// Data Structures
// ============================================

struct SatEntry {
    char name[25];      // satellite name from TLE line 0
    char line1[71];     // TLE line 1
    char line2[71];     // TLE line 2
    uint32_t norad;     // NORAD catalog number
};

struct SatCatalog {
    SatEntry* sats;         // PSRAM array
    int count;
    bool initialized;       // allocation done
    bool valid;             // holds parsed TLEs
    time_t tleFetchUnix;    // unix time of last fetch (0 = unknown)
};

static SatCatalog satCatalog = { nullptr, 0, false, false, 0 };

// ============================================
// PSRAM Allocation
// ============================================

bool initSatCatalog() {
    if (satCatalog.initialized) return true;

    size_t totalSize = sizeof(SatEntry) * MAX_SATELLITES;
    if (psramFound()) {
        satCatalog.sats = (SatEntry*)ps_malloc(totalSize);
    }
    if (!satCatalog.sats) {
        satCatalog.sats = (SatEntry*)malloc(totalSize);
    }
    if (!satCatalog.sats) {
        Serial.println("[SAT] ERROR: catalog allocation failed");
        return false;
    }
    memset(satCatalog.sats, 0, totalSize);
    satCatalog.initialized = true;
    Serial.printf("[SAT] Catalog allocated: %u bytes\n", (unsigned)totalSize);
    return true;
}

// ============================================
// TLE Parsing
// ============================================

// Trim trailing CR/whitespace in place
static void satTrimRight(char* s) {
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == ' ' || s[len - 1] == '\n' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

static int satFindByNorad(uint32_t norad) {
    for (int i = 0; i < satCatalog.count; i++) {
        if (satCatalog.sats[i].norad == norad) return i;
    }
    return -1;
}

// Feed one line of a TLE stream into the catalog. Maintains a tiny state
// machine across calls: name line -> line 1 -> line 2 -> commit entry.
struct SatTLEParseState {
    char name[25];
    char line1[71];
    bool haveName;
    bool haveLine1;
};

static void satParseTLELine(SatTLEParseState& st, char* line) {
    satTrimRight(line);
    if (line[0] == '\0') return;

    if (strncmp(line, "1 ", 2) == 0 && strlen(line) >= 69) {
        strlcpy(st.line1, line, sizeof(st.line1));
        st.haveLine1 = true;
        return;
    }
    if (strncmp(line, "2 ", 2) == 0 && strlen(line) >= 69) {
        if (!st.haveName || !st.haveLine1) { st.haveLine1 = false; return; }
        uint32_t norad = (uint32_t)strtoul(st.line1 + 2, NULL, 10);
        int idx = satFindByNorad(norad);
        if (idx < 0) {
            if (satCatalog.count >= MAX_SATELLITES) { st.haveName = st.haveLine1 = false; return; }
            idx = satCatalog.count++;
        }
        SatEntry& e = satCatalog.sats[idx];
        memset(&e, 0, sizeof(SatEntry));
        strlcpy(e.name, st.name, sizeof(e.name));
        strlcpy(e.line1, st.line1, sizeof(e.line1));
        strlcpy(e.line2, line, sizeof(e.line2));
        e.norad = norad;
        st.haveName = false;
        st.haveLine1 = false;
        return;
    }
    // Anything else is a name line
    strlcpy(st.name, line, sizeof(st.name));
    satTrimRight(st.name);
    st.haveName = true;
    st.haveLine1 = false;
}

// ============================================
// SD Card Cache
// ============================================

bool satSaveTLEsToSD() {
    if (!sdCardAvailable) return false;
    if (!satCatalog.valid || satCatalog.count == 0) return false;

    if (!SD.exists(SAT_SD_DIR)) SD.mkdir(SAT_SD_DIR);

    File f = SD.open(SAT_SD_TLE_FILE, FILE_WRITE);
    if (!f) {
        Serial.println("[SAT] ERROR: cannot write TLE cache");
        return false;
    }
    for (int i = 0; i < satCatalog.count; i++) {
        f.println(satCatalog.sats[i].name);
        f.println(satCatalog.sats[i].line1);
        f.println(satCatalog.sats[i].line2);
    }
    f.close();

    File m = SD.open(SAT_SD_META_FILE, FILE_WRITE);
    if (m) {
        m.printf("%lu\n", (unsigned long)satCatalog.tleFetchUnix);
        m.close();
    }
    Serial.printf("[SAT] Saved %d TLEs to SD\n", satCatalog.count);
    return true;
}

bool satLoadTLEsFromSD() {
    if (!sdCardAvailable) initSDCard();
    if (!sdCardAvailable) return false;
    if (!initSatCatalog()) return false;
    if (!SD.exists(SAT_SD_TLE_FILE)) return false;

    File f = SD.open(SAT_SD_TLE_FILE);
    if (!f) return false;

    satCatalog.count = 0;
    SatTLEParseState st;
    memset(&st, 0, sizeof(st));
    char line[96];
    while (f.available()) {
        int n = f.readBytesUntil('\n', line, sizeof(line) - 1);
        line[n] = '\0';
        satParseTLELine(st, line);
    }
    f.close();

    satCatalog.tleFetchUnix = 0;
    File m = SD.open(SAT_SD_META_FILE);
    if (m) {
        char meta[24];
        int n = m.readBytesUntil('\n', meta, sizeof(meta) - 1);
        meta[n] = '\0';
        satCatalog.tleFetchUnix = (time_t)strtoul(meta, NULL, 10);
        m.close();
    }

    satCatalog.valid = (satCatalog.count > 0);
    Serial.printf("[SAT] Loaded %d TLEs from SD cache\n", satCatalog.count);
    return satCatalog.valid;
}

// ============================================
// Celestrak Fetch
// ============================================

// Stream one Celestrak URL into the catalog. Returns satellites added/updated
// (>=0) or -1 on HTTP failure.
static int satFetchTLEUrl(const char* url) {
    HTTPClient http;
    http.setTimeout(SAT_TLE_TIMEOUT);
    http.setReuse(false);  // server closes the socket at end of body -> clean loop exit
    http.begin(url);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[SAT] TLE fetch failed: HTTP %d (%s)\n", code, url);
        http.end();
        return -1;
    }

    int before = satCatalog.count;
    WiFiClient* stream = http.getStreamPtr();
    SatTLEParseState st;
    memset(&st, 0, sizeof(st));
    char line[96];
    unsigned long lastData = millis();
    while ((http.connected() || stream->available()) && (millis() - lastData) < 3000) {
        if (!stream->available()) {
            delay(1);
            continue;
        }
        int n = stream->readBytesUntil('\n', line, sizeof(line) - 1);
        line[n] = '\0';
        satParseTLELine(st, line);
        lastData = millis();
    }
    http.end();
    return satCatalog.count - before;
}

// Fetch amateur group + ISS. Blocking (call behind a loading overlay, same as
// the POTA fetch). Returns true if the catalog holds data afterwards.
bool satFetchTLEs() {
    if (WiFi.status() != WL_CONNECTED) return false;
    if (!initSatCatalog()) return false;

    // Rebuild from scratch so decayed/renamed birds don't linger
    satCatalog.count = 0;
    satCatalog.valid = false;

    int amateur = satFetchTLEUrl(SAT_TLE_URL_AMATEUR);
    int iss = satFetchTLEUrl(SAT_TLE_URL_ISS);
    Serial.printf("[SAT] Fetch results: amateur=%d iss=%d\n", amateur, iss);

    if (satCatalog.count == 0) {
        // Fetch failed entirely - try to restore the SD cache
        satLoadTLEsFromSD();
        return false;
    }

    satCatalog.valid = true;
    satCatalog.tleFetchUnix = ntpSynced ? time(nullptr) : 0;
    satSaveTLEsToSD();
    return true;
}

// TLE age in days: -1 unknown, otherwise whole days since fetch
int satTLEAgeDays() {
    if (!satCatalog.valid) return -1;
    if (satCatalog.tleFetchUnix == 0 || !ntpSynced) return -1;
    time_t now = time(nullptr);
    if (now <= satCatalog.tleFetchUnix) return 0;
    return (int)((now - satCatalog.tleFetchUnix) / 86400);
}

// ============================================
// Favorites (CSV of NORAD ids in Preferences)
// ============================================

#include <Preferences.h>

static char satFavCsv[160] = "";
static bool satFavsLoaded = false;

static void saveSatFavorites() {
    Preferences p;
    p.begin("sat", false);
    p.putString("favs", satFavCsv);
    p.end();
}

void loadSatFavorites() {
    if (satFavsLoaded) return;
    Preferences p;
    p.begin("sat", true);
    bool haveKey = p.isKey("favs");
    p.getString("favs", satFavCsv, sizeof(satFavCsv));
    p.end();
    satFavsLoaded = true;

    // First run: seed the popular starter birds (ISS, SO-50, AO-91, RS-44)
    // so the list opens with something workable pinned on top.
    if (!haveKey) {
        strlcpy(satFavCsv, "25544,27607,43017,44909", sizeof(satFavCsv));
        saveSatFavorites();
    }
}

bool isSatFavorite(uint32_t norad) {
    loadSatFavorites();
    char token[12];
    snprintf(token, sizeof(token), "%lu", (unsigned long)norad);
    const char* pos = satFavCsv;
    size_t tlen = strlen(token);
    while ((pos = strstr(pos, token)) != NULL) {
        bool startOk = (pos == satFavCsv) || (pos[-1] == ',');
        bool endOk = (pos[tlen] == '\0') || (pos[tlen] == ',');
        if (startOk && endOk) return true;
        pos += tlen;
    }
    return false;
}

void toggleSatFavorite(uint32_t norad) {
    loadSatFavorites();
    char token[12];
    snprintf(token, sizeof(token), "%lu", (unsigned long)norad);

    if (isSatFavorite(norad)) {
        // Remove token (and one adjoining comma)
        char out[sizeof(satFavCsv)] = "";
        char work[sizeof(satFavCsv)];
        strlcpy(work, satFavCsv, sizeof(work));
        char* saveptr = NULL;
        for (char* t = strtok_r(work, ",", &saveptr); t; t = strtok_r(NULL, ",", &saveptr)) {
            if (strcmp(t, token) == 0) continue;
            if (out[0] != '\0') strlcat(out, ",", sizeof(out));
            strlcat(out, t, sizeof(out));
        }
        strlcpy(satFavCsv, out, sizeof(satFavCsv));
    } else {
        if (strlen(satFavCsv) + strlen(token) + 2 >= sizeof(satFavCsv)) return;  // full
        if (satFavCsv[0] != '\0') strlcat(satFavCsv, ",", sizeof(satFavCsv));
        strlcat(satFavCsv, token, sizeof(satFavCsv));
    }
    saveSatFavorites();
}

// ============================================
// Display List (favorites first, name filter)
// ============================================

static uint16_t satDisplayIdx[MAX_SATELLITES];
static int satDisplayCount = 0;

// Case-insensitive substring match
static bool satNameMatches(const char* name, const char* filter) {
    if (filter[0] == '\0') return true;
    int nlen = strlen(name);
    int flen = strlen(filter);
    for (int i = 0; i + flen <= nlen; i++) {
        int j = 0;
        while (j < flen && toupper((unsigned char)name[i + j]) == toupper((unsigned char)filter[j])) j++;
        if (j == flen) return true;
    }
    return false;
}

// Sort key for next-pass ordering: soonest AOS first, uncomputed ("...")
// next, known-no-pass ("none") last.
static uint32_t satNextPassSortKey(int idx, const time_t* nextPassAos) {
    time_t a = nextPassAos[idx];
    if (a > 0) return (uint32_t)a;
    if (a == 0) return 0xFFFFFFFEu;
    return 0xFFFFFFFFu;
}

// Rebuild satDisplayIdx from the catalog + name filter.
// nextPassAos == NULL: favorites pinned first, each section name-sorted.
// nextPassAos != NULL: whole list ordered by next pass time (see key above).
void buildSatDisplayList(const char* filter, const time_t* nextPassAos) {
    satDisplayCount = 0;
    if (!satCatalog.valid) return;

    if (nextPassAos) {
        for (int i = 0; i < satCatalog.count; i++) {
            if (!satNameMatches(satCatalog.sats[i].name, filter)) continue;
            uint32_t key = satNextPassSortKey(i, nextPassAos);
            int pos = satDisplayCount;
            while (pos > 0) {
                int prev = satDisplayIdx[pos - 1];
                uint32_t prevKey = satNextPassSortKey(prev, nextPassAos);
                if (prevKey < key) break;
                if (prevKey == key &&
                    strcasecmp(satCatalog.sats[prev].name, satCatalog.sats[i].name) <= 0) break;
                satDisplayIdx[pos] = satDisplayIdx[pos - 1];
                pos--;
            }
            satDisplayIdx[pos] = (uint16_t)i;
            satDisplayCount++;
        }
        return;
    }

    // Two passes: favorites then non-favorites
    for (int fav = 1; fav >= 0; fav--) {
        int sectionStart = satDisplayCount;
        for (int i = 0; i < satCatalog.count; i++) {
            if ((int)isSatFavorite(satCatalog.sats[i].norad) != fav) continue;
            if (!satNameMatches(satCatalog.sats[i].name, filter)) continue;
            // Insertion sort by name within the section
            int pos = satDisplayCount;
            while (pos > sectionStart &&
                   strcasecmp(satCatalog.sats[satDisplayIdx[pos - 1]].name, satCatalog.sats[i].name) > 0) {
                satDisplayIdx[pos] = satDisplayIdx[pos - 1];
                pos--;
            }
            satDisplayIdx[pos] = (uint16_t)i;
            satDisplayCount++;
        }
    }
}

#endif // SAT_DATA_H
