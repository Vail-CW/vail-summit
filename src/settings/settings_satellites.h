/*
 * VAIL SUMMIT - Satellite Tracker Settings
 * Min elevation / lookahead / grid override in the "sat" namespace.
 * The UTC offset lives in its own "clock" namespace so other features
 * (QSO logger display, future clock screens) can share it.
 */

#ifndef SETTINGS_SATELLITES_H
#define SETTINGS_SATELLITES_H

#include <Arduino.h>
#include <Preferences.h>

// Operator grid from the QSO logger (qso_logger.h) - used as the default site
extern char operatorGrid[9];

struct SatSettings {
    int minElevation;    // degrees: passes peaking below this are hidden (0-45)
    int lookaheadHours;  // pass search window: 12/24/48/72
    char grid[9];        // satellite-specific grid override ("" = use operatorGrid)
};

static SatSettings satSettings = { 10, 48, "" };
static bool satSettingsLoaded = false;

// Shared clock offset, minutes east of UTC (-720..+840, 30-min steps)
static int utcOffsetMinutes = 0;

void loadSatSettings() {
    if (satSettingsLoaded) return;
    Preferences p;
    p.begin("sat", true);
    satSettings.minElevation = p.getInt("minel", 10);
    satSettings.lookaheadHours = p.getInt("lookh", 48);
    p.getString("grid", satSettings.grid, sizeof(satSettings.grid));
    p.end();

    p.begin("clock", true);
    utcOffsetMinutes = p.getInt("utcoffmin", 0);
    p.end();
    satSettingsLoaded = true;
}

void saveSatSettings() {
    Preferences p;
    p.begin("sat", false);
    p.putInt("minel", satSettings.minElevation);
    p.putInt("lookh", satSettings.lookaheadHours);
    p.putString("grid", satSettings.grid);
    p.end();
}

void saveClockSettings() {
    Preferences p;
    p.begin("clock", false);
    p.putInt("utcoffmin", utcOffsetMinutes);
    p.end();
}

// Effective observer grid: satellite override first, QSO logger grid second
const char* satEffectiveGrid() {
    loadSatSettings();
    if (satSettings.grid[0] != '\0') return satSettings.grid;
    return operatorGrid;
}

// "+HH:MM" / "-HH:MM" / "UTC"
void formatUtcOffset(char* out, size_t n) {
    if (utcOffsetMinutes == 0) {
        strlcpy(out, "UTC", n);
        return;
    }
    int m = utcOffsetMinutes < 0 ? -utcOffsetMinutes : utcOffsetMinutes;
    snprintf(out, n, "%c%02d:%02d", utcOffsetMinutes < 0 ? '-' : '+', m / 60, m % 60);
}

#endif // SETTINGS_SATELLITES_H
