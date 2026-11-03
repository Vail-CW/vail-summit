/*
 * Vail Summit Practice Time Tracking
 * Tracks practice time across all training modes for CW School sync
 */

#ifndef SETTINGS_PRACTICE_TIME_H
#define SETTINGS_PRACTICE_TIME_H

#include <Preferences.h>
#include <Arduino.h>
#include <time.h>
#include "../core/deferred_save.h"
#include "settings_satellites.h"   // utcOffsetMinutes ("clock" namespace)

// ============================================
// Inactivity Detection Constants
// ============================================

#define INACTIVITY_THRESHOLD_MS 30000    // 30 seconds - pause counting after this much idle time
#define ACTIVITY_CHECK_INTERVAL_MS 1000  // Check activity state every second
#define DAY_ROLLOVER_CHECK_MS 60000      // Check for a new practice day once a minute
#define PRACTICE_CHECKPOINT_MS 300000    // Commit + save an active session every 5 minutes
#define CHECKPOINT_QUIET_MS 5000         // ...but only after this long with no tone/keys
#define PRACTICE_SESSION_MODE_LEN 32

// ============================================
// Practice Time Storage Structure
// ============================================

struct PracticeTimeData {
    // Lifetime totals
    unsigned long totalPracticeSec;      // Total practice time ever (seconds)

    // Today's practice
    unsigned long todayPracticeSec;      // Practice time today (seconds)
    int todayDate;                       // YYYYMMDD format, to detect date change

    // Current session
    unsigned long sessionStartTime;      // millis() when session started
    bool sessionActive;                  // Is a session currently active?
    char sessionMode[PRACTICE_SESSION_MODE_LEN];  // Which training mode (for reporting)

    // Inactivity tracking
    unsigned long lastActivityTime;      // millis() of last user activity
    unsigned long accumulatedActiveMs;   // Active practice time this session (ms)
    unsigned long lastActivityCheck;     // millis() of last activity state check
    bool wasActive;                      // Was user active at last check?

    // Streak tracking
    int currentStreak;                   // Consecutive days with 15+ min practice
    int longestStreak;                   // Best streak ever
    int lastPracticeDate;                // YYYYMMDD of last practice

    // History (last 7 days for sync)
    int historyDates[7];                 // YYYYMMDD for each day
    unsigned long historySeconds[7];     // Seconds practiced each day

    // Checkpointing / day attribution (RAM only)
    unsigned long sessionCommittedSec;   // Session seconds already added to today/total
    int sessionStartDate;                // Date the session's uncommitted time belongs to (0 = unknown)
    unsigned long undatedSec;            // Seconds in todayPracticeSec counted while the date was unknown
    unsigned long lastCheckpointMs;      // millis() of last checkpoint (or session start)
};

static PracticeTimeData practiceTime = {
    .totalPracticeSec = 0,
    .todayPracticeSec = 0,
    .todayDate = 0,
    .sessionStartTime = 0,
    .sessionActive = false,
    .sessionMode = {0},
    .lastActivityTime = 0,
    .accumulatedActiveMs = 0,
    .lastActivityCheck = 0,
    .wasActive = false,
    .currentStreak = 0,
    .longestStreak = 0,
    .lastPracticeDate = 0
};

static Preferences practicePrefs;

// ============================================
// Forward Declarations
// ============================================

void savePracticeTimeData();
unsigned long endPracticeSession();
void updateActivityAccumulator();

// ============================================
// Date Utilities
// ============================================

// Get the current local date as YYYYMMDD integer (0 if the clock isn't set).
// The system clock runs in UTC (NTP, no TZ), so apply the user's UTC offset
// from the shared "clock" setting - the practice day follows local midnight.
int getCurrentDateInt() {
    time_t now;
    time(&now);
    if (now < 1704067200) {  // Before 2024-01-01: clock not set yet
        return 0;
    }

    loadSatSettings();  // loads utcOffsetMinutes once (no-op afterwards)
    time_t local = now + (time_t)utcOffsetMinutes * 60;
    struct tm tmv;
    gmtime_r(&local, &tmv);

    return (tmv.tm_year + 1900) * 10000 +
           (tmv.tm_mon + 1) * 100 +
           tmv.tm_mday;
}

// Days since 1970-01-01 for a YYYYMMDD date (proleptic Gregorian)
long practiceDateToDays(int date) {
    long y = date / 10000;
    long m = (date / 100) % 100;
    long d = date % 100;
    y -= (m <= 2) ? 1 : 0;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153L * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

// Check if date2 is exactly one day after date1
bool areDatesConsecutive(int date1, int date2) {
    if (date1 == 0 || date2 == 0) return false;
    return practiceDateToDays(date2) - practiceDateToDays(date1) == 1;
}

// ============================================
// Day Rollover / Streak
// ============================================

// Credit the streak for 'date' once today's total reaches 15 minutes.
// Only increments once per day (lastPracticeDate guard).
static void updatePracticeStreak(int date) {
    if (date <= 0 || practiceTime.todayPracticeSec < 900) return;
    if (practiceTime.lastPracticeDate == date) return;

    if (areDatesConsecutive(practiceTime.lastPracticeDate, date)) {
        practiceTime.currentStreak++;
    } else {
        practiceTime.currentStreak = 1;  // Start new streak
    }
    if (practiceTime.currentStreak > practiceTime.longestStreak) {
        practiceTime.longestStreak = practiceTime.currentStreak;
    }
    practiceTime.lastPracticeDate = date;
}

// Move the active session's whole seconds into today/total. The remainder
// stays in accumulatedActiveMs, so nothing is counted twice when the session
// later ends. streakDate is the day the time belongs to (0 = unknown).
static void commitActiveSessionTime(int streakDate) {
    if (!practiceTime.sessionActive) return;
    unsigned long secs = practiceTime.accumulatedActiveMs / 1000;
    if (secs == 0) return;

    practiceTime.accumulatedActiveMs -= secs * 1000;
    practiceTime.sessionCommittedSec += secs;
    practiceTime.totalPracticeSec += secs;
    practiceTime.todayPracticeSec += secs;
    if (streakDate <= 0) {
        practiceTime.undatedSec += secs;
    }
    updatePracticeStreak(streakDate);
}

/*
 * Start a new practice day if the (known) local date differs from todayDate:
 * archive today into history, zero today, and break the streak if a day was
 * skipped. Safe to call often - it's a clock read when nothing changed.
 * Returns true if a rollover happened (the save is deferred).
 */
bool checkPracticeDayRollover() {
    int currentDate = getCurrentDateInt();
    if (currentDate == 0) return false;

    if (currentDate == practiceTime.todayDate) {
        // Date confirmed: anything counted while it was unknown was today
        practiceTime.undatedSec = 0;
        if (practiceTime.sessionActive && practiceTime.sessionStartDate == 0) {
            practiceTime.sessionStartDate = currentDate;
        }
        return false;
    }

    // Ignore a small backwards step (UTC offset edited across midnight) so
    // the day doesn't flip-flop and zero itself
    if (practiceTime.todayDate > 0 && currentDate < practiceTime.todayDate &&
        practiceDateToDays(practiceTime.todayDate) - practiceDateToDays(currentDate) <= 1) {
        return false;
    }

    if (practiceTime.todayDate == 0) {
        // No day recorded yet (first use): adopt the date, keep the time
        practiceTime.todayDate = currentDate;
        practiceTime.undatedSec = 0;
        if (practiceTime.sessionActive) practiceTime.sessionStartDate = currentDate;
        updatePracticeStreak(currentDate);
        markDeferredSave(savePracticeTimeData);
        return true;
    }

    int oldDate = practiceTime.todayDate;

    // A session that spans midnight: time so far belongs to the old day, if
    // the session started while the date was known
    if (practiceTime.sessionActive && practiceTime.sessionStartDate == oldDate) {
        commitActiveSessionTime(oldDate);
    }

    // Time counted while the clock was unknown (boot before NTP) happened
    // today, not on the stored day
    unsigned long carry = practiceTime.undatedSec;
    if (carry > practiceTime.todayPracticeSec) carry = practiceTime.todayPracticeSec;
    unsigned long oldDaySec = practiceTime.todayPracticeSec - carry;

    if (oldDaySec > 0) {
        for (int i = 6; i > 0; i--) {
            practiceTime.historyDates[i] = practiceTime.historyDates[i-1];
            practiceTime.historySeconds[i] = practiceTime.historySeconds[i-1];
        }
        practiceTime.historyDates[0] = oldDate;
        practiceTime.historySeconds[0] = oldDaySec;
    }

    practiceTime.todayDate = currentDate;
    practiceTime.todayPracticeSec = carry;
    practiceTime.undatedSec = 0;
    if (practiceTime.sessionActive) practiceTime.sessionStartDate = currentDate;

    // Streak survives only if the last 15-minute day was yesterday (or today)
    if (practiceTime.lastPracticeDate > 0 &&
        practiceTime.lastPracticeDate != currentDate &&
        !areDatesConsecutive(practiceTime.lastPracticeDate, currentDate)) {
        practiceTime.currentStreak = 0;
    }
    updatePracticeStreak(currentDate);

    Serial.printf("[Practice] New day %d (archived %d: %lu sec)\n",
                  currentDate, oldDate, oldDaySec);
    markDeferredSave(savePracticeTimeData);
    return true;
}

// ============================================
// Persistence Functions
// ============================================

// Load practice time data from flash
void loadPracticeTimeData() {
    practicePrefs.begin("practice", true);  // Read-only

    practiceTime.totalPracticeSec = practicePrefs.getULong("total_sec", 0);
    practiceTime.todayPracticeSec = practicePrefs.getULong("today_sec", 0);
    practiceTime.todayDate = practicePrefs.getInt("today_date", 0);
    practiceTime.currentStreak = practicePrefs.getInt("streak", 0);
    practiceTime.longestStreak = practicePrefs.getInt("best_streak", 0);
    practiceTime.lastPracticeDate = practicePrefs.getInt("last_date", 0);

    // Load history
    for (int i = 0; i < 7; i++) {
        char keyDate[12], keySec[12];
        snprintf(keyDate, sizeof(keyDate), "hd%d", i);
        snprintf(keySec, sizeof(keySec), "hs%d", i);
        practiceTime.historyDates[i] = practicePrefs.getInt(keyDate, 0);
        practiceTime.historySeconds[i] = practicePrefs.getULong(keySec, 0);
    }

    practicePrefs.end();

    // Day change (only possible here if the clock is already set; otherwise
    // the periodic check in updateActivityAccumulator() catches it later)
    checkPracticeDayRollover();

    Serial.printf("[Practice] Loaded: total=%lu sec, today=%lu sec, streak=%d\n",
                  practiceTime.totalPracticeSec, practiceTime.todayPracticeSec,
                  practiceTime.currentStreak);
}

// Save practice time data to flash
void savePracticeTimeData() {
    practicePrefs.begin("practice", false);  // Read-write

    practicePrefs.putULong("total_sec", practiceTime.totalPracticeSec);
    practicePrefs.putULong("today_sec", practiceTime.todayPracticeSec);
    practicePrefs.putInt("today_date", practiceTime.todayDate);
    practicePrefs.putInt("streak", practiceTime.currentStreak);
    practicePrefs.putInt("best_streak", practiceTime.longestStreak);
    practicePrefs.putInt("last_date", practiceTime.lastPracticeDate);

    // Save history
    for (int i = 0; i < 7; i++) {
        char keyDate[12], keySec[12];
        snprintf(keyDate, sizeof(keyDate), "hd%d", i);
        snprintf(keySec, sizeof(keySec), "hs%d", i);
        practicePrefs.putInt(keyDate, practiceTime.historyDates[i]);
        practicePrefs.putULong(keySec, practiceTime.historySeconds[i]);
    }

    practicePrefs.end();

    Serial.println("[Practice] Data saved");
}

// ============================================
// Activity Tracking Functions
// ============================================

/*
 * Record user activity (call on any user input during training)
 * This keeps the session "active" for practice time counting
 */
void recordPracticeActivity() {
    if (!practiceTime.sessionActive) return;

    unsigned long now = millis();

    // If we were inactive and just became active again, update the check time
    if (!practiceTime.wasActive) {
        practiceTime.lastActivityCheck = now;
        practiceTime.wasActive = true;
    }

    practiceTime.lastActivityTime = now;
}

/*
 * Update the activity accumulator (call periodically from main loop)
 * This accumulates active time and pauses when user is idle
 */
void updateActivityAccumulator() {
    unsigned long now = millis();

    // Cheap periodic day check, so "today" resets at local midnight even if
    // the device stays on (the clock usually isn't set yet at boot)
    static unsigned long lastDayCheckMs = 0;
    if (now - lastDayCheckMs >= DAY_ROLLOVER_CHECK_MS) {
        lastDayCheckMs = now;
        checkPracticeDayRollover();
    }

    if (!practiceTime.sessionActive) return;

    // Checkpoint: commit the session so far and save, so a power-off doesn't
    // lose it. Waits for a quiet moment (no tone or keys) because an NVS
    // commit stalls the flash cache and would glitch audio.
    if (now - practiceTime.lastCheckpointMs >= PRACTICE_CHECKPOINT_MS &&
        (now - practiceTime.lastActivityTime) >= CHECKPOINT_QUIET_MS &&
        !isTonePlaying()) {
        practiceTime.lastCheckpointMs = now;
        checkPracticeDayRollover();
        int d = getCurrentDateInt();
        commitActiveSessionTime(d > 0 && d == practiceTime.todayDate ? d : 0);
        savePracticeTimeData();
    }

    // Only check once per interval
    if (now - practiceTime.lastActivityCheck < ACTIVITY_CHECK_INTERVAL_MS) return;

    unsigned long elapsed = now - practiceTime.lastActivityCheck;
    practiceTime.lastActivityCheck = now;

    // Check if user is currently active (had activity within threshold)
    bool isActive = (now - practiceTime.lastActivityTime) < INACTIVITY_THRESHOLD_MS;

    if (isActive) {
        // User is active - accumulate this time
        practiceTime.accumulatedActiveMs += elapsed;
        practiceTime.wasActive = true;
    } else if (practiceTime.wasActive) {
        // User just became inactive - log it
        Serial.println("[Practice] User inactive - pausing time accumulation");
        practiceTime.wasActive = false;
    }
}

/*
 * Check if user is currently active
 */
bool isPracticeActive() {
    if (!practiceTime.sessionActive) return false;
    return (millis() - practiceTime.lastActivityTime) < INACTIVITY_THRESHOLD_MS;
}

// ============================================
// Session Management
// ============================================

// Start a practice session (call when entering a training mode)
void startPracticeSession(const char* mode) {
    if (practiceTime.sessionActive) {
        // End previous session first
        endPracticeSession();
    }

    unsigned long now = millis();
    practiceTime.sessionStartTime = now;
    practiceTime.sessionActive = true;
    strlcpy(practiceTime.sessionMode, mode ? mode : "", sizeof(practiceTime.sessionMode));

    // Initialize activity tracking
    practiceTime.lastActivityTime = now;
    practiceTime.accumulatedActiveMs = 0;
    practiceTime.lastActivityCheck = now;
    practiceTime.wasActive = true;
    practiceTime.sessionCommittedSec = 0;
    practiceTime.lastCheckpointMs = now;
    practiceTime.sessionStartDate = 0;

    // Start a new day if the date moved on (archives yesterday properly)
    checkPracticeDayRollover();
    int currentDate = getCurrentDateInt();
    if (currentDate > 0 && currentDate == practiceTime.todayDate) {
        practiceTime.sessionStartDate = currentDate;
    }

    Serial.printf("[Practice] Session started: %s\n", practiceTime.sessionMode);
}

// End a practice session (call when exiting a training mode)
// Returns session duration in seconds (only active time, not idle time)
unsigned long endPracticeSession() {
    if (!practiceTime.sessionActive) {
        return 0;
    }

    // Do one final activity check to capture any remaining active time
    updateActivityAccumulator();

    // Archive yesterday first if the session ran past midnight
    checkPracticeDayRollover();

    unsigned long wallClockDuration = (millis() - practiceTime.sessionStartTime) / 1000;

    // Add remaining active time to totals (checkpoints already added the rest)
    int currentDate = getCurrentDateInt();
    commitActiveSessionTime(currentDate > 0 && currentDate == practiceTime.todayDate ? currentDate : 0);
    unsigned long sessionDuration = practiceTime.sessionCommittedSec;

    Serial.printf("[Practice] Session ended: active=%lu sec, wall=%lu sec (%.0f%% active)\n",
                  sessionDuration, wallClockDuration,
                  wallClockDuration > 0 ? (sessionDuration * 100.0 / wallClockDuration) : 0);

    practiceTime.sessionActive = false;
    practiceTime.sessionMode[0] = '\0';
    practiceTime.sessionCommittedSec = 0;
    practiceTime.sessionStartDate = 0;

    // Reset activity tracking
    practiceTime.lastActivityTime = 0;
    practiceTime.accumulatedActiveMs = 0;
    practiceTime.lastActivityCheck = 0;
    practiceTime.wasActive = false;

    // Save after each session
    savePracticeTimeData();

    Serial.printf("[Practice] Today total: %lu sec, lifetime: %lu sec\n",
                  practiceTime.todayPracticeSec, practiceTime.totalPracticeSec);

    return sessionDuration;
}

// ============================================
// Getters for UI/Sync
// ============================================

// Get total practice time in seconds
unsigned long getTotalPracticeSeconds() {
    return practiceTime.totalPracticeSec;
}

// Get today's practice time in seconds
unsigned long getTodayPracticeSeconds() {
    // Roll over to a new day first so the home screen never shows yesterday
    checkPracticeDayRollover();

    // Include current session if active. Count active time only, the same
    // number endPracticeSession() commits, so the total doesn't drop on exit.
    unsigned long total = practiceTime.todayPracticeSec;
    if (practiceTime.sessionActive) {
        total += practiceTime.accumulatedActiveMs / 1000;
    }
    return total;
}

// Get current streak
int getPracticeStreak() {
    return practiceTime.currentStreak;
}

// Get longest streak
int getLongestPracticeStreak() {
    return practiceTime.longestStreak;
}

// Check if a session is currently active
bool isPracticeSessionActive() {
    return practiceTime.sessionActive;
}

// Get current session duration in active seconds (while active)
unsigned long getCurrentSessionSeconds() {
    if (!practiceTime.sessionActive) return 0;
    // Committed (checkpointed) time plus accumulated active time
    return practiceTime.sessionCommittedSec + practiceTime.accumulatedActiveMs / 1000;
}

// Get wall-clock session duration (for display purposes)
unsigned long getCurrentSessionWallSeconds() {
    if (!practiceTime.sessionActive) return 0;
    return (millis() - practiceTime.sessionStartTime) / 1000;
}

// ============================================
// Formatting Helpers
// ============================================

// Format seconds as "Xh Ym" or "Ym Zs"
String formatPracticeTime(unsigned long seconds) {
    if (seconds >= 3600) {
        unsigned long hours = seconds / 3600;
        unsigned long mins = (seconds % 3600) / 60;
        return String(hours) + "h " + String(mins) + "m";
    } else if (seconds >= 60) {
        unsigned long mins = seconds / 60;
        unsigned long secs = seconds % 60;
        return String(mins) + "m " + String(secs) + "s";
    } else {
        return String(seconds) + "s";
    }
}

// Get practice history for sync (returns JSON-like structure)
// Format: {"YYYY-MM-DD": seconds, ...}
String getPracticeHistoryJson() {
    String json = "{";
    bool first = true;

    for (int i = 0; i < 7; i++) {
        if (practiceTime.historyDates[i] > 0 && practiceTime.historySeconds[i] > 0) {
            if (!first) json += ",";
            first = false;

            // Format date as YYYY-MM-DD
            int y = practiceTime.historyDates[i] / 10000;
            int m = (practiceTime.historyDates[i] / 100) % 100;
            int d = practiceTime.historyDates[i] % 100;

            char dateBuf[16];
            snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d", y, m, d);

            json += "\"" + String(dateBuf) + "\":" + String(practiceTime.historySeconds[i]);
        }
    }

    // Add today if we have practice time
    if (practiceTime.todayDate > 0 && getTodayPracticeSeconds() > 0) {
        if (!first) json += ",";

        int y = practiceTime.todayDate / 10000;
        int m = (practiceTime.todayDate / 100) % 100;
        int d = practiceTime.todayDate % 100;

        char dateBuf[16];
        snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d", y, m, d);

        json += "\"" + String(dateBuf) + "\":" + String(getTodayPracticeSeconds());
    }

    json += "}";
    return json;
}

// ============================================
// Reset Functions
// ============================================

// Reset all practice time data (for testing/factory reset)
void resetPracticeTimeData() {
    practiceTime.totalPracticeSec = 0;
    practiceTime.todayPracticeSec = 0;
    practiceTime.todayDate = 0;
    practiceTime.sessionStartTime = 0;
    practiceTime.sessionActive = false;
    practiceTime.sessionMode[0] = '\0';
    practiceTime.accumulatedActiveMs = 0;
    practiceTime.sessionCommittedSec = 0;
    practiceTime.sessionStartDate = 0;
    practiceTime.undatedSec = 0;
    practiceTime.currentStreak = 0;
    practiceTime.longestStreak = 0;
    practiceTime.lastPracticeDate = 0;

    for (int i = 0; i < 7; i++) {
        practiceTime.historyDates[i] = 0;
        practiceTime.historySeconds[i] = 0;
    }

    practicePrefs.begin("practice", false);
    practicePrefs.clear();
    practicePrefs.end();

    Serial.println("[Practice] All data reset");
}

#endif // SETTINGS_PRACTICE_TIME_H
