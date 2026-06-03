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

// Curriculum re-derived from the web app's src/lib/modules/definitions.ts (Koch
// method, true Koch order). 12 modules; Numbers sits at slot 6 to match the web
// progression. Bump VAIL_CURRICULUM_VERSION below when this changes so on-device
// progress from the old (stale) curriculum is reset rather than misinterpreted.
#define VAIL_CURRICULUM_VERSION 2

enum VailCourseModule {
    MODULE_LETTERS_1 = 0,    // K, M
    MODULE_LETTERS_2,        // R, S
    MODULE_LETTERS_3,        // U, A
    MODULE_LETTERS_4,        // P, T, L
    MODULE_LETTERS_5,        // O, W, I
    MODULE_LETTERS_6,        // N, J, E, F
    MODULE_NUMBERS,          // 0-9
    MODULE_LETTERS_7,        // Y, V, G, Q
    MODULE_LETTERS_8,        // Z, H, B, C, D, X
    MODULE_PUNCTUATION,      // . , ? /
    MODULE_WORDS_COMMON,     // Common words
    MODULE_CALLSIGNS,        // Callsign patterns
    MODULE_COUNT             // Total module count (12)
};

// Module names for display (web titles, shortened where needed to fit cards)
const char* vailCourseModuleNames[] = {
    "First Steps",       // letters-1: K, M
    "Building Blocks",   // letters-2: R, S
    "Vowels Begin",      // letters-3: U, A
    "Common Sounds",     // letters-4: P, T, L
    "More Vowels",       // letters-5: O, W, I
    "Essential",         // letters-6: N, J, E, F
    "Numbers",           // numbers: 0-9
    "Y V G Q",           // letters-7
    "Z H B C D X",       // letters-8
    "Punctuation",       // punctuation: . , ? /
    "Common Words",      // words-common
    "Callsigns"          // callsigns
};

// Module short IDs (for API sync — must match web module ids)
const char* vailCourseModuleIds[] = {
    "letters-1",
    "letters-2",
    "letters-3",
    "letters-4",
    "letters-5",
    "letters-6",
    "numbers",
    "letters-7",
    "letters-8",
    "punctuation",
    "words-common",
    "callsigns"
};

// Characters introduced per module (Koch order)
const char* vailCourseModuleChars[] = {
    "KM",           // letters-1
    "RS",           // letters-2
    "UA",           // letters-3
    "PTL",          // letters-4
    "OWI",          // letters-5
    "NJEF",         // letters-6
    "0123456789",   // numbers
    "YVGQ",         // letters-7
    "ZHBCDX",       // letters-8
    ".,?/",         // punctuation
    "",             // words (uses all learned chars)
    ""              // callsigns (uses all learned chars)
};

// Lesson character definitions: [moduleIdx][lessonIdx] -> NEW characters for that
// lesson. Up to 5 lessons/module (letters-6 has 5: N,J,E,F,review). Review lessons
// introduce no new chars (""). Mirrors web definitions.ts newCharacters per lesson.
const char* vailCourseLessonChars[MODULE_COUNT][5] = {
    {"K", "M", "", "", ""},          // letters-1:  K / M / review
    {"R", "S", "", "", ""},          // letters-2:  R / S / review
    {"U", "A", "", "", ""},          // letters-3:  U / A / review
    {"P", "T", "L", "", ""},         // letters-4:  P / T / L / review
    {"O", "W", "I", "", ""},         // letters-5:  O / W / I / review
    {"N", "J", "E", "F", ""},        // letters-6:  N / J / E / F / review
    {"05", "1234", "6789", "", ""},  // numbers:    0&5 / 1-4 / 6-9 / review
    {"Y", "V", "G", "Q", ""},        // letters-7:  Y / V / G / Q (no review)
    {"ZH", "BC", "DX", "", ""},      // letters-8:  ZH / BC / DX / review
    {".,", "?", "/", "", ""},        // punctuation: period+comma / question / slash
    {"", "", "", "", ""},            // words: uses all learned chars
    {"", "", "", "", ""}             // callsigns: uses all learned chars
};

// Lessons per module
const int vailCourseLessonCounts[] = {
    3,   // letters-1
    3,   // letters-2
    3,   // letters-3
    4,   // letters-4
    4,   // letters-5
    5,   // letters-6
    4,   // numbers
    4,   // letters-7
    4,   // letters-8
    3,   // punctuation
    3,   // words-common
    2    // callsigns
};

// Module prerequisites (which module must be completed first)
const int vailCourseModulePrereqs[] = {
    -1,                     // letters-1: none
    MODULE_LETTERS_1,       // letters-2
    MODULE_LETTERS_2,       // letters-3
    MODULE_LETTERS_3,       // letters-4
    MODULE_LETTERS_4,       // letters-5
    MODULE_LETTERS_5,       // letters-6
    MODULE_LETTERS_6,       // numbers (after letters-6)
    MODULE_NUMBERS,         // letters-7 (after numbers)
    MODULE_LETTERS_7,       // letters-8
    MODULE_LETTERS_8,       // punctuation
    MODULE_PUNCTUATION,     // words
    MODULE_WORDS_COMMON     // callsigns
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

// Get NEW characters introduced in THIS lesson only (for INTRO phase)
// Returns only characters that haven't been introduced in previous lessons of this module
String getVailCourseNewCharsForLesson(VailCourseModule module, int lesson) {
    if (module >= MODULE_COUNT || lesson <= 0) return "";

    int lessonIdx = lesson - 1;  // Convert to 0-based indexing
    if (lessonIdx >= vailCourseLessonCounts[module]) return "";

    const char* lessonChars = vailCourseLessonChars[module][lessonIdx];

    // For first lesson, all chars in this lesson are new
    if (lesson == 1) {
        return String(lessonChars);
    }

    // For subsequent lessons, return chars from this lesson
    // (The lesson array is already designed to only contain new chars per lesson)
    return String(lessonChars);
}

// Get ALL characters available for current lesson (for MIXED/GROUPS phases)
// Returns cumulative: all chars from previous modules + all chars from this module up to current lesson
String getVailCourseCharsForLesson(VailCourseModule module, int lesson) {
    String chars = "";

    // Add all chars from previous modules
    for (int m = 0; m < module; m++) {
        chars += String(vailCourseModuleChars[m]);
    }

    // Add all chars from this module up to and including current lesson
    int lessonIdx = lesson - 1;  // Convert to 0-based
    for (int l = 0; l <= lessonIdx && l < vailCourseLessonCounts[module]; l++) {
        const char* lessonChars = vailCourseLessonChars[module][l];

        // Add each character if not already present (avoid duplicates)
        for (size_t i = 0; i < strlen(lessonChars); i++) {
            char c = lessonChars[i];
            if (chars.indexOf(c) == -1) {
                chars += c;
            }
        }
    }

    return chars;
}

// ============================================
// Shared practice helpers (Daily / Copy / Send)
// ============================================

// All characters the user has learned so far: the NEW characters from every
// lesson they have completed, across all modules. Empty for a brand-new user.
String getVailCourseLearnedChars() {
    String chars = "";
    for (int m = 0; m < MODULE_COUNT; m++) {
        int lessonsDone = (int)vailCourseProgress.lessonsCompleted[m];
        if (lessonsDone <= 0) continue;
        for (int l = 0; l < lessonsDone && l < vailCourseLessonCounts[m]; l++) {
            const char* lc = vailCourseLessonChars[m][l];
            for (size_t i = 0; i < strlen(lc); i++) {
                if (chars.indexOf(lc[i]) == -1) chars += lc[i];
            }
        }
    }
    return chars;
}

// Mastery weight for a character: weaker characters weigh more so they are
// drilled more often (mirrors the web app's weak-character boosting). Floor
// keeps mastered characters in the rotation.
static int vailCourseCharWeight(char c) {
    int idx = getVailCourseCharIndex(c);
    int mastery = (idx >= 0 && idx < VAIL_CHAR_COUNT) ? vailCourseProgress.charMastery[idx].mastery : 0;
    int w = 1000 - mastery;
    return (w < 60) ? 60 : w;
}

// Pick a character from `pool`, weighted toward weak mastery.
char getVailCourseWeightedRandomChar(const String& pool) {
    int n = pool.length();
    if (n == 0) return '?';
    long total = 0;
    for (int i = 0; i < n; i++) total += vailCourseCharWeight(pool[i]);
    if (total <= 0) return pool[random(n)];
    long r = random(total);
    for (int i = 0; i < n; i++) {
        long w = vailCourseCharWeight(pool[i]);
        if (r < w) return pool[i];
        r -= w;
    }
    return pool[n - 1];
}

// Record one copy answer against a character's mastery (shared by every mode so
// Learn / Daily / Copy all contribute to the same 0-1000 mastery, like the web).
void vailCourseRecordAnswer(char c, bool correct) {
    int idx = getVailCourseCharIndex(c);
    if (idx < 0 || idx >= VAIL_CHAR_COUNT) return;
    VailCourseCharMastery& m = vailCourseProgress.charMastery[idx];
    m.attempts++;
    if (correct) {
        m.correct++;
        m.mastery += 50;
        if (m.mastery > 1000) m.mastery = 1000;
    } else {
        m.mastery -= 25;
        if (m.mastery < 0) m.mastery = 0;
    }
}

// ============================================
// Common Words & Callsign content (modules words-common / callsigns)
// ============================================

// Word lists for the words-common module, one array per lesson (0-based),
// ported verbatim from the web app's definitions.ts. NULL-terminated.
//   lesson 1 (idx 0) = CW Basics
//   lesson 2 (idx 1) = Q Codes
//   lesson 3 (idx 2) = Abbreviations
static const char* vailCourseWordsBasics[] = {
    "CQ", "DE", "K", "73", "88", "TNX", "FB", "OM", "HI",
    "ES", "R", "TU", "SK", "AR", NULL
};
static const char* vailCourseWordsQCodes[] = {
    "QTH", "QSO", "QSL", "QRZ", "QRM", "QRN", "QRP", "QRO",
    "QSB", "QSY", "QRT", "QRV", NULL
};
static const char* vailCourseWordsAbbrev[] = {
    "UR", "RST", "ANT", "RIG", "WX", "PWR", "SIG", "RPT", "AGN", "BK",
    "PSE", "CFM", "GD", "HPE", "SRI", "TU", "VY", "WL", "ABT", "NW", NULL
};
static const char* const* vailCourseWordLists[3] = {
    vailCourseWordsBasics, vailCourseWordsQCodes, vailCourseWordsAbbrev
};

// Pick a random word for a given words-common lesson (0-based). Out-of-range
// lessons fall back to the basics list.
const char* getVailCourseRandomWord(int lessonIdx) {
    if (lessonIdx < 0 || lessonIdx > 2) lessonIdx = 0;
    const char* const* list = vailCourseWordLists[lessonIdx];
    int n = 0;
    while (list[n] != NULL) n++;
    if (n == 0) return "CQ";
    return list[random(n)];
}

// Representative DX (non-US) prefixes for callsign generation.
static const char* vailCourseDxPrefixes[] = {
    "G", "M", "DL", "F", "EA", "I", "ON", "PA", "SM", "OZ", "LA", "OH",
    "JA", "VK", "ZL", "PY", "LU", "VE", "UA", "SP", "OK", "HB", "9A",
    "S5", "YO", "LZ", NULL
};

// Generate a plausible callsign into buf. style 0 = US, style 1 = DX.
//   US:  W/K/N, or A + (A-L), then a digit, then 2-3 suffix letters.
//   DX:  an international prefix, then a digit, then 2-3 suffix letters.
void generateVailCourseCallsign(char* buf, int bufSize, int style) {
    char tmp[16];
    int p = 0;
    if (style == 0) {
        int r = random(4);
        if (r == 0)      tmp[p++] = 'W';
        else if (r == 1) tmp[p++] = 'K';
        else if (r == 2) tmp[p++] = 'N';
        else { tmp[p++] = 'A'; tmp[p++] = 'A' + random(12); }  // AA-AL
    } else {
        int n = 0;
        while (vailCourseDxPrefixes[n] != NULL) n++;
        const char* pre = vailCourseDxPrefixes[random(n)];
        for (int i = 0; pre[i] && p < 12; i++) tmp[p++] = pre[i];
    }
    tmp[p++] = '0' + random(10);
    int suffixLen = 2 + random(2);  // 2-3 letters
    for (int i = 0; i < suffixLen && p < 15; i++) tmp[p++] = 'A' + random(26);
    tmp[p] = '\0';
    strncpy(buf, tmp, bufSize - 1);
    buf[bufSize - 1] = '\0';
}

// ============================================
// Persistence Functions
// ============================================

void loadVailCourseProgress() {
    // Curriculum version guard: if the stored progress predates the Koch
    // re-derivation, the module identities/order have changed and old unlock
    // bitfields would be wrong. Reset position/unlock state once, then continue.
    // (Character mastery in "vcmastery" is keyed by character, not module, so it
    // stays valid and is intentionally not reset here.)
    vailCoursePrefs.begin("vailcourse", true);
    int storedVer = vailCoursePrefs.getInt("cver", 0);
    vailCoursePrefs.end();
    if (storedVer != VAIL_CURRICULUM_VERSION) {
        vailCoursePrefs.begin("vailcourse", false);
        vailCoursePrefs.putInt("cver", VAIL_CURRICULUM_VERSION);
        vailCoursePrefs.putInt("module", MODULE_LETTERS_1);
        vailCoursePrefs.putInt("lesson", 1);
        vailCoursePrefs.putUInt("unlocked", 0x01);
        vailCoursePrefs.putUInt("completed", 0);
        for (int i = 0; i < MODULE_COUNT; i++) {
            char key[12];
            snprintf(key, sizeof(key), "lc%d", i);
            vailCoursePrefs.putInt(key, 0);
        }
        vailCoursePrefs.end();
        Serial.println("[VailCourse] Curriculum version changed - progress reset to defaults");
    }

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
