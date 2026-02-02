/*
 * Vail CW Course - Core Definitions
 * Data structures, enums, and state management for CW School training
 *
 * This implements the same curriculum and mastery system as the web app,
 * enabling cross-platform progress sync.
 */

#ifndef TRAINING_VAIL_COURSE_CORE_H
#define TRAINING_VAIL_COURSE_CORE_H

#include <Arduino.h>
#include <Preferences.h>
#include "../core/config.h"

// ============================================
// Module Definitions (matches web definitions.ts)
// ============================================

enum VailCourseModule {
    MODULE_LETTERS_1 = 0,    // E, T
    MODULE_LETTERS_2,        // A, N, I
    MODULE_LETTERS_3,        // R, S, O
    MODULE_LETTERS_4,        // H, D, L
    MODULE_LETTERS_5,        // C, U, M
    MODULE_LETTERS_6,        // W, F, Y
    MODULE_LETTERS_7,        // P, G, K
    MODULE_LETTERS_8,        // V, B, X, J, Q, Z
    MODULE_NUMBERS,          // 0-9
    MODULE_PUNCTUATION,      // . , ? /
    MODULE_WORDS_COMMON,     // Common words
    MODULE_CALLSIGNS,        // Callsign patterns
    MODULE_COUNT             // Total module count (12)
};

// Module names for display
const char* vailCourseModuleNames[] = {
    "Letters 1",
    "Letters 2",
    "Letters 3",
    "Letters 4",
    "Letters 5",
    "Letters 6",
    "Letters 7",
    "Letters 8",
    "Numbers",
    "Punctuation",
    "Common Words",
    "Callsigns"
};

// Module short IDs (for API sync)
const char* vailCourseModuleIds[] = {
    "letters-1",
    "letters-2",
    "letters-3",
    "letters-4",
    "letters-5",
    "letters-6",
    "letters-7",
    "letters-8",
    "numbers",
    "punctuation",
    "words-common",
    "callsigns"
};

// Characters introduced per module
const char* vailCourseModuleChars[] = {
    "ET",           // Letters 1
    "ANI",          // Letters 2
    "RSO",          // Letters 3
    "HDL",          // Letters 4
    "CUM",          // Letters 5
    "WFY",          // Letters 6
    "PGK",          // Letters 7
    "VBXJQZ",       // Letters 8
    "0123456789",   // Numbers
    ".,?/",         // Punctuation
    "",             // Words (uses all learned chars)
    ""              // Callsigns (uses all learned chars)
};

// Lessons per module
const int vailCourseLessonCounts[] = {
    3, 3, 3, 3, 3, 3, 3, 3,   // Letters 1-8: 3 lessons each
    2,                         // Numbers: 2 lessons
    2,                         // Punctuation: 2 lessons
    3,                         // Common Words: 3 lessons
    2                          // Callsigns: 2 lessons
};

// Module prerequisites (which module must be completed first)
const int vailCourseModulePrereqs[] = {
    -1,                     // Letters 1: none
    MODULE_LETTERS_1,       // Letters 2
    MODULE_LETTERS_2,       // Letters 3
    MODULE_LETTERS_3,       // Letters 4
    MODULE_LETTERS_4,       // Letters 5
    MODULE_LETTERS_5,       // Letters 6
    MODULE_LETTERS_6,       // Letters 7
    MODULE_LETTERS_7,       // Letters 8
    MODULE_LETTERS_6,       // Numbers (after Letters 6)
    MODULE_NUMBERS,         // Punctuation (after Numbers)
    MODULE_PUNCTUATION,     // Words (after Punctuation)
    MODULE_WORDS_COMMON     // Callsigns (after Words)
};

// ============================================
// Lesson Phase State Machine
// ============================================

enum VailCoursePhase {
    PHASE_INTRO,          // Show character, play morse 3x
    PHASE_SOLO,           // New character only, immediate feedback
    PHASE_MIXED,          // All known characters
    PHASE_GROUPS,         // 2-5 character groups
    PHASE_RESULT,         // Show accuracy, unlock next if passed
    PHASE_COUNT
};

const char* vailCoursePhaseNames[] = {
    "Introduction",
    "Solo Practice",
    "Mixed Characters",
    "Character Groups",
    "Results"
};

// ============================================
// Character Mastery Structure
// ============================================

// Rolling window size for mastery calculation
#define MASTERY_WINDOW_SIZE 20

// Character index mapping (same as LICW)
// A-Z = 0-25, 0-9 = 26-35, punctuation = 36-39
#define VAIL_CHAR_COUNT 40

struct VailCourseCharMastery {
    int mastery;                          // 0-1000 points (like web app)
    int attempts;                         // Total attempts
    int correct;                          // Total correct
    int windowAttempts[MASTERY_WINDOW_SIZE];  // Rolling window (1=correct, 0=wrong)
    int windowIndex;                      // Current position in window
    unsigned long totalResponseTimeMs;    // Total response time
    int responseTimeCount;                // Count for avg calculation
    char confusedWith[5];                 // Characters commonly confused with
};

// ============================================
// Progress Structure
// ============================================

struct VailCourseProgress {
    // Current position
    VailCourseModule currentModule;
    int currentLesson;
    VailCoursePhase currentPhase;

    // Module unlock state (bitfield)
    uint32_t modulesUnlocked;       // Bit set = module unlocked
    uint32_t modulesCompleted;      // Bit set = module completed

    // Per-module lessons completed (packed: 4 bits per module)
    uint32_t lessonsCompleted[MODULE_COUNT];

    // Character mastery (indexed by character code)
    VailCourseCharMastery charMastery[VAIL_CHAR_COUNT];

    // Settings
    int characterWPM;              // Character speed (default 20)
    int effectiveWPM;              // Effective/Farnsworth speed (default 10)
    bool autoAdvance;              // Auto-advance on pass

    // Session stats
    int sessionCorrect;
    int sessionTotal;
    unsigned long sessionStartTime;

    // Sync metadata
    unsigned long lastSyncTimestamp;
};

// Global progress state
static VailCourseProgress vailCourseProgress = {
    .currentModule = MODULE_LETTERS_1,
    .currentLesson = 1,
    .currentPhase = PHASE_INTRO,
    .modulesUnlocked = 0x01,        // First module unlocked by default
    .modulesCompleted = 0,
    .characterWPM = 20,
    .effectiveWPM = 10,
    .autoAdvance = true,
    .sessionCorrect = 0,
    .sessionTotal = 0,
    .sessionStartTime = 0,
    .lastSyncTimestamp = 0
};

static Preferences vailCoursePrefs;

// ============================================
// Character Index Helpers
// ============================================

// Get index for a character (0-39)
int getVailCourseCharIndex(char c) {
    c = toupper(c);
    if (c >= 'A' && c <= 'Z') return c - 'A';           // 0-25
    if (c >= '0' && c <= '9') return 26 + (c - '0');    // 26-35
    switch (c) {
        case '.': return 36;
        case ',': return 37;
        case '?': return 38;
        case '/': return 39;
        default: return -1;
    }
}

// Get character from index
char getVailCourseCharFromIndex(int idx) {
    if (idx < 0 || idx >= VAIL_CHAR_COUNT) return '?';
    if (idx < 26) return 'A' + idx;
    if (idx < 36) return '0' + (idx - 26);
    switch (idx) {
        case 36: return '.';
        case 37: return ',';
        case 38: return '?';
        case 39: return '/';
        default: return '?';
    }
}

// ============================================
// Module/Lesson Helpers
// ============================================

// Check if a module is unlocked
bool isVailCourseModuleUnlocked(VailCourseModule module) {
    return (vailCourseProgress.modulesUnlocked & (1 << module)) != 0;
}

// Check if a module is completed
bool isVailCourseModuleCompleted(VailCourseModule module) {
    return (vailCourseProgress.modulesCompleted & (1 << module)) != 0;
}

// Unlock a module
void unlockVailCourseModule(VailCourseModule module) {
    vailCourseProgress.modulesUnlocked |= (1 << module);
}

// Complete a module
void completeVailCourseModule(VailCourseModule module) {
    vailCourseProgress.modulesCompleted |= (1 << module);

    // Unlock next module if prerequisite is met
    for (int i = 0; i < MODULE_COUNT; i++) {
        if (vailCourseModulePrereqs[i] == module) {
            unlockVailCourseModule((VailCourseModule)i);
        }
    }
}

// Get number of lessons completed in a module
int getVailCourseLessonsCompleted(VailCourseModule module) {
    return vailCourseProgress.lessonsCompleted[module];
}

// Complete a lesson
void completeVailCourseLesson(VailCourseModule module, int lesson) {
    if (lesson > (int)vailCourseProgress.lessonsCompleted[module]) {
        vailCourseProgress.lessonsCompleted[module] = lesson;
    }

    // Check if module is complete
    if ((int)vailCourseProgress.lessonsCompleted[module] >= vailCourseLessonCounts[module]) {
        completeVailCourseModule(module);
    }
}

// Get all characters learned up to a module
String getVailCourseCumulativeChars(VailCourseModule upToModule) {
    String chars = "";
    for (int i = 0; i <= upToModule && i < MODULE_COUNT; i++) {
        chars += vailCourseModuleChars[i];
    }
    return chars;
}

// ============================================
// Persistence Functions
// ============================================

void loadVailCourseProgress() {
    vailCoursePrefs.begin("vailcourse", true);  // Read-only

    // Current position
    vailCourseProgress.currentModule = (VailCourseModule)vailCoursePrefs.getInt("module", MODULE_LETTERS_1);
    vailCourseProgress.currentLesson = vailCoursePrefs.getInt("lesson", 1);

    // Unlock state
    vailCourseProgress.modulesUnlocked = vailCoursePrefs.getUInt("unlocked", 0x01);
    vailCourseProgress.modulesCompleted = vailCoursePrefs.getUInt("completed", 0);

    // Settings
    vailCourseProgress.characterWPM = vailCoursePrefs.getInt("charWPM", 20);
    vailCourseProgress.effectiveWPM = vailCoursePrefs.getInt("effWPM", 10);
    vailCourseProgress.autoAdvance = vailCoursePrefs.getBool("autoAdv", true);

    // Sync metadata
    vailCourseProgress.lastSyncTimestamp = vailCoursePrefs.getULong("lastSync", 0);

    // Load lessons completed per module
    for (int i = 0; i < MODULE_COUNT; i++) {
        char key[12];
        snprintf(key, sizeof(key), "lc%d", i);
        vailCourseProgress.lessonsCompleted[i] = vailCoursePrefs.getInt(key, 0);
    }

    vailCoursePrefs.end();

    Serial.printf("[VailCourse] Progress loaded: Module %d, Lesson %d\n",
                  (int)vailCourseProgress.currentModule, vailCourseProgress.currentLesson);
}

void saveVailCourseProgress() {
    vailCoursePrefs.begin("vailcourse", false);  // Read-write

    // Current position
    vailCoursePrefs.putInt("module", (int)vailCourseProgress.currentModule);
    vailCoursePrefs.putInt("lesson", vailCourseProgress.currentLesson);

    // Unlock state
    vailCoursePrefs.putUInt("unlocked", vailCourseProgress.modulesUnlocked);
    vailCoursePrefs.putUInt("completed", vailCourseProgress.modulesCompleted);

    // Settings
    vailCoursePrefs.putInt("charWPM", vailCourseProgress.characterWPM);
    vailCoursePrefs.putInt("effWPM", vailCourseProgress.effectiveWPM);
    vailCoursePrefs.putBool("autoAdv", vailCourseProgress.autoAdvance);

    // Sync metadata
    vailCoursePrefs.putULong("lastSync", vailCourseProgress.lastSyncTimestamp);

    // Save lessons completed per module
    for (int i = 0; i < MODULE_COUNT; i++) {
        char key[12];
        snprintf(key, sizeof(key), "lc%d", i);
        vailCoursePrefs.putInt(key, vailCourseProgress.lessonsCompleted[i]);
    }

    vailCoursePrefs.end();

    Serial.println("[VailCourse] Progress saved");
}

// Load character mastery (separate namespace due to size)
void loadVailCourseMastery() {
    vailCoursePrefs.begin("vcmastery", true);

    for (int i = 0; i < VAIL_CHAR_COUNT; i++) {
        char keyMastery[12], keyAttempts[12], keyCorrect[12];
        snprintf(keyMastery, sizeof(keyMastery), "m%d", i);
        snprintf(keyAttempts, sizeof(keyAttempts), "a%d", i);
        snprintf(keyCorrect, sizeof(keyCorrect), "c%d", i);

        vailCourseProgress.charMastery[i].mastery = vailCoursePrefs.getInt(keyMastery, 0);
        vailCourseProgress.charMastery[i].attempts = vailCoursePrefs.getInt(keyAttempts, 0);
        vailCourseProgress.charMastery[i].correct = vailCoursePrefs.getInt(keyCorrect, 0);
    }

    vailCoursePrefs.end();
    Serial.println("[VailCourse] Mastery loaded");
}

void saveVailCourseMastery() {
    vailCoursePrefs.begin("vcmastery", false);

    for (int i = 0; i < VAIL_CHAR_COUNT; i++) {
        char keyMastery[12], keyAttempts[12], keyCorrect[12];
        snprintf(keyMastery, sizeof(keyMastery), "m%d", i);
        snprintf(keyAttempts, sizeof(keyAttempts), "a%d", i);
        snprintf(keyCorrect, sizeof(keyCorrect), "c%d", i);

        vailCoursePrefs.putInt(keyMastery, vailCourseProgress.charMastery[i].mastery);
        vailCoursePrefs.putInt(keyAttempts, vailCourseProgress.charMastery[i].attempts);
        vailCoursePrefs.putInt(keyCorrect, vailCourseProgress.charMastery[i].correct);
    }

    vailCoursePrefs.end();
    Serial.println("[VailCourse] Mastery saved");
}

// ============================================
// Session Management
// ============================================

void startVailCourseSession() {
    vailCourseProgress.sessionCorrect = 0;
    vailCourseProgress.sessionTotal = 0;
    vailCourseProgress.sessionStartTime = millis();
    Serial.println("[VailCourse] Session started");
}

void endVailCourseSession() {
    // Save progress
    saveVailCourseProgress();
    saveVailCourseMastery();

    unsigned long duration = (millis() - vailCourseProgress.sessionStartTime) / 1000;
    Serial.printf("[VailCourse] Session ended: %d/%d correct (%lu sec)\n",
                  vailCourseProgress.sessionCorrect, vailCourseProgress.sessionTotal, duration);
}

// ============================================
// Initialization
// ============================================

void initVailCourse() {
    loadVailCourseProgress();
    loadVailCourseMastery();
}

#endif // TRAINING_VAIL_COURSE_CORE_H
