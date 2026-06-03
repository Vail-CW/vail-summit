/*
 * Vail Summit Practice Time Tracking
 * Tracks practice time across all training modes for CW School sync
 */

#ifndef SETTINGS_PRACTICE_TIME_H
#define SETTINGS_PRACTICE_TIME_H

#include <Preferences.h>
#include <Arduino.h>
#include <time.h>

// ============================================
// Inactivity Detection Constants
// ============================================

#define INACTIVITY_THRESHOLD_MS 30000    // 30 seconds - pause counting after this much idle time
#define ACTIVITY_CHECK_INTERVAL_MS 1000  // Check activity state every second

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
    String sessionMode;                  // Which training mode (for reporting)

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
};

static PracticeTimeData practiceTime = {
    .totalPracticeSec = 0,
    .todayPracticeSec = 0,
    .todayDate = 0,
    .sessionStartTime = 0,
    .sessionActive = false,
    .sessionMode = "",
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

// Get current date as YYYYMMDD integer
int getCurrentDateInt() {
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);

    // If time is not set (before 2024), return 0
    if (timeinfo->tm_year < 124) {  // 124 = 2024 - 1900
        return 0;
    }

    return (timeinfo->tm_year + 1900) * 10000 +
           (timeinfo->tm_mon + 1) * 100 +
           timeinfo->tm_mday;
}

// Check if two dates are consecutive
bool areDatesConsecutive(int date1, int date2) {
    if (date1 == 0 || date2 == 0) return false;

    // Extract components
    int y1 = date1 / 10000;
    int m1 = (date1 / 100) % 100;
    int d1 = date1 % 100;

    int y2 = date2 / 10000;
    int m2 = (date2 / 100) % 100;
    int d2 = date2 % 100;

    // Create tm structures
    struct tm tm1 = {0};
    tm1.tm_year = y1 - 1900;
    tm1.tm_mon = m1 - 1;
    tm1.tm_mday = d1;

    struct tm tm2 = {0};
    tm2.tm_year = y2 - 1900;
    tm2.tm_mon = m2 - 1;
    tm2.tm_mday = d2;

    // Convert to time_t and check difference
    time_t t1 = mktime(&tm1);
    time_t t2 = mktime(&tm2);

    // Check if date2 is exactly 1 day after date1
    return (difftime(t2, t1) >= 86400 - 3600 && difftime(t2, t1) <= 86400 + 3600);
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

    // Check for date change and reset today's count if needed
    int currentDate = getCurrentDateInt();
    if (currentDate > 0 && currentDate != practiceTime.todayDate) {
        // New day - archive yesterday's data and reset today
        if (practiceTime.todayDate > 0 && practiceTime.todayPracticeSec > 0) {
            // Shift history and add yesterday
            for (int i = 6; i > 0; i--) {
                practiceTime.historyDates[i] = practiceTime.historyDates[i-1];
                practiceTime.historySeconds[i] = practiceTime.historySeconds[i-1];
            }
            practiceTime.historyDates[0] = practiceTime.todayDate;
            practiceTime.historySeconds[0] = practiceTime.todayPracticeSec;
        }

        // Reset today
        practiceTime.todayPracticeSec = 0;
        practiceTime.todayDate = currentDate;

        // Update streak
        if (practiceTime.lastPracticeDate > 0) {
            if (!areDatesConsecutive(practiceTime.lastPracticeDate, currentDate)) {
                // Streak broken if we didn't practice yesterday
                if (practiceTime.historySeconds[0] < 900) {  // Less than 15 min yesterday
                    practiceTime.currentStreak = 0;
                }
            }
        }

        // Save the date change
        savePracticeTimeData();
    }

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
    if (!practiceTime.sessionActive) return;

    unsigned long now = millis();

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
void startPracticeSession(const String& mode) {
    if (practiceTime.sessionActive) {
        // End previous session first
        endPracticeSession();
    }

    unsigned long now = millis();
    practiceTime.sessionStartTime = now;
    practiceTime.sessionActive = true;
    practiceTime.sessionMode = mode;

    // Initialize activity tracking
    practiceTime.lastActivityTime = now;
    practiceTime.accumulatedActiveMs = 0;
    practiceTime.lastActivityCheck = now;
    practiceTime.wasActive = true;

    // Update today's date if needed
    int currentDate = getCurrentDateInt();
    if (currentDate > 0 && currentDate != practiceTime.todayDate) {
        practiceTime.todayDate = currentDate;
    }

    Serial.printf("[Practice] Session started: %s\n", mode.c_str());
}

// End a practice session (call when exiting a training mode)
// Returns session duration in seconds (only active time, not idle time)
unsigned long endPracticeSession() {
    if (!practiceTime.sessionActive) {
        return 0;
    }

    // Do one final activity check to capture any remaining active time
    updateActivityAccumulator();

    // Use accumulated active time instead of wall-clock time
    unsigned long sessionDuration = practiceTime.accumulatedActiveMs / 1000;
    unsigned long wallClockDuration = (millis() - practiceTime.sessionStartTime) / 1000;

    Serial.printf("[Practice] Session ended: active=%lu sec, wall=%lu sec (%.0f%% active)\n",
                  sessionDuration, wallClockDuration,
                  wallClockDuration > 0 ? (sessionDuration * 100.0 / wallClockDuration) : 0);

    // Add to totals
    practiceTime.totalPracticeSec += sessionDuration;
    practiceTime.todayPracticeSec += sessionDuration;

    // Update streak if today's practice >= 15 minutes (900 seconds)
    int currentDate = getCurrentDateInt();
    if (currentDate > 0 && practiceTime.todayPracticeSec >= 900) {
        if (practiceTime.lastPracticeDate != currentDate) {
            // First time hitting 15 min today
            if (areDatesConsecutive(practiceTime.lastPracticeDate, currentDate) ||
                practiceTime.lastPracticeDate == currentDate) {
                practiceTime.currentStreak++;
            } else {
                practiceTime.currentStreak = 1;  // Start new streak
            }

            if (practiceTime.currentStreak > practiceTime.longestStreak) {
                practiceTime.longestStreak = practiceTime.currentStreak;
            }

            practiceTime.lastPracticeDate = currentDate;
        }
    }

    practiceTime.sessionActive = false;
    practiceTime.sessionMode = "";

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
    // Include current session if active
    unsigned long total = practiceTime.todayPracticeSec;
    if (practiceTime.sessionActive) {
        total += (millis() - practiceTime.sessionStartTime) / 1000;
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
    // Return accumulated active time (activity check will update it)
    return practiceTime.accumulatedActiveMs / 1000;
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
    practiceTime.sessionMode = "";
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
