/*
 * CW Academy Training - Core Structures and Utilities
 * Shared definitions, enums, and helper functions for all CWA training modules
 */

#ifndef TRAINING_CWA_CORE_H
#define TRAINING_CWA_CORE_H

#include <Preferences.h>
#include "../core/config.h"
#include "../audio/i2s_audio.h"
#include "../core/morse_code.h"
#include "../audio/morse_decoder_adaptive.h"
#include "../settings/settings_cw.h"  // For KeyType enum and cwSpeed/cwTone/cwKeyType
#include "../core/task_manager.h"     // For requestStopTone() in dual-core audio

// ============================================
// Track and Session Data Structures
// ============================================

// CW Academy Training Tracks
enum CWATrack {
  TRACK_BEGINNER = 0,
  TRACK_FUNDAMENTAL = 1,
  TRACK_INTERMEDIATE = 2,
  TRACK_ADVANCED = 3
};

const char* cwaTrackNames[] = {
  "Beginner",
  "Fundamental",
  "Intermediate",
  "Advanced"
};

const char* cwaTrackDescriptions[] = {
  "Learn CW from zero",
  "Build solid foundation",
  "Increase speed & skill",
  "Master advanced CW"
};

const int CWA_TOTAL_TRACKS = 4;

// Session structure
struct CWASession {
  int sessionNum;           // Session number (1-16)
  int charCount;            // Total characters learned by this session
  const char* newChars;     // New characters introduced in this session
  const char* description;  // Session description
};

// CW Academy Session Progression (Beginner track)
const CWASession cwaSessionData[] = {
  {1,  4,  "AENT",           "Foundation"},
  {2,  9,  "SIO14",          "Numbers Begin"},
  {3,  15, "RHDL25",         "Building Words"},
  {4,  17, "CU",             "Conversations"},
  {5,  22, "MW36?",          "Questions"},
  {6,  25, "FY,",            "Punctuation"},
  {7,  31, "GPQ79/",         "Complete Numbers"},
  {8,  34, "BV<AR>",         "Pro-signs Start"},
  {9,  39, "JK08<BT>",       "Advanced Signs"},
  {10, 44, "XZ.<BK><SK>",    "Complete!"},
  {11, 44, "",               "QSO Practice 1"},
  {12, 44, "",               "QSO Practice 2"},
  {13, 44, "",               "QSO Practice 3"},
  {14, 44, "",               "On-Air Prep 1"},
  {15, 44, "",               "On-Air Prep 2"},
  {16, 44, "",               "On-Air Prep 3"}
};

const int CWA_TOTAL_SESSIONS = 16;

// ============================================
// Intermediate Track Session Data
// ============================================

// Intermediate session structure - includes WPM targets
struct CWAIntermediateSession {
  int sessionNum;           // Session number (1-16)
  int targetWPM;            // Target WPM for this session
  const char* description;  // Session description
  const char* objective;    // Learning focus
};

// WPM progression per session (from CWA Intermediate curriculum)
// Sessions 1-3: 10-13 WPM, Sessions 4-6: 13-15 WPM, Sessions 7-10: 15 WPM
// Sessions 11-13: 18 WPM, Sessions 14-15: 20 WPM, Session 16: 25 WPM
const int cwaIntermediateWPM[] = {
  10, 10, 13,      // Sessions 1-3
  13, 13, 13,      // Sessions 4-6
  15, 15, 15, 15,  // Sessions 7-10
  18, 18, 18,      // Sessions 11-13
  20, 20,          // Sessions 14-15
  25               // Session 16
};

const CWAIntermediateSession cwaIntermediateSessionData[] = {
  {1,  10, "Words & Prefixes",     "Build 10-13 WPM foundation"},
  {2,  10, "Suffixes & QSO",       "Recognize suffix sounds"},
  {3,  13, "Speed Increase",       "Push to 13 WPM"},
  {4,  13, "Words 202",            "Comfortable at 13 WPM"},
  {5,  13, "QSO Practice",         "Exchange practice"},
  {6,  13, "POTA Intro",           "Park exchange format"},
  {7,  15, "15 WPM Target",        "Sustained 15 WPM"},
  {8,  15, "Prefix Mastery",       "Hear prefixes as sounds"},
  {9,  15, "Full QSO",             "Complete exchanges"},
  {10, 15, "Consolidation",        "Solid 15 WPM"},
  {11, 18, "Head Copy Intro",      "18 WPM, less writing"},
  {12, 18, "CWT Introduction",     "Contest exchange format"},
  {13, 18, "Speed Push",           "Push boundaries"},
  {14, 20, "20 WPM Target",        "Operating speed"},
  {15, 20, "Advanced QSO",         "Complex exchanges"},
  {16, 25, "25 WPM Challenge",     "Taste of high speed"}
};

// ============================================
// Practice Types and Message Types
// ============================================

// Practice types
enum CWAPracticeType {
  PRACTICE_COPY = 0,         // Copy practice (receive, keyboard input)
  PRACTICE_SENDING = 1,      // Sending practice (transmit, physical key input)
  PRACTICE_DAILY_DRILL = 2   // Daily drill (warm-up exercise)
};

const char* cwaPracticeTypeNames[] = {
  "Copy Practice",
  "Sending Practice",
  "Daily Drill"
};

const char* cwaPracticeTypeDescriptions[] = {
  "Listen & type",
  "Send with key",
  "Warm-up drills"
};

const int CWA_TOTAL_PRACTICE_TYPES = 3;

// Message types (content types for practice)
enum CWAMessageType {
  MESSAGE_CHARACTERS = 0,
  MESSAGE_WORDS = 1,
  MESSAGE_ABBREVIATIONS = 2,
  MESSAGE_NUMBERS = 3,
  MESSAGE_CALLSIGNS = 4,
  MESSAGE_PHRASES = 5,
  // Intermediate track message types
  MESSAGE_PREFIXES = 6,       // DIS, IM, IN, IR, RE, UN prefix words
  MESSAGE_SUFFIXES = 7,       // ED, ES, ING, LY suffix words
  MESSAGE_QSO_EXCHANGE = 8,   // Callsign + Name + QTH format
  MESSAGE_POTA_EXCHANGE = 9   // Callsign + Park ID format
};

const char* cwaMessageTypeNames[] = {
  "Characters",
  "Words",
  "CW Abbreviations",
  "Numbers",
  "Callsigns",
  "Phrases",
  // Intermediate types
  "Prefix Words",
  "Suffix Words",
  "QSO Exchange",
  "POTA Exchange"
};

const char* cwaMessageTypeDescriptions[] = {
  "Individual letters",
  "Common words",
  "Ham radio terms",
  "Number practice",
  "Call signs",
  "Sentences",
  // Intermediate descriptions
  "DIS/IM/IN/RE/UN words",
  "ED/ES/ING/LY words",
  "Call, Name, QTH",
  "Call, Park ID"
};

const int CWA_BEGINNER_MESSAGE_TYPES = 6;
const int CWA_TOTAL_MESSAGE_TYPES = 10;

// ============================================
// Session Definitions (Beginner Track)
// ============================================

// Character sets introduced in each session (cumulative)
const char* cwaSessionCharSets[] = {
  "AENT",                    // Session 1
  "AENTSIO14",               // Session 2
  "AENTSIO14RHDL25",         // Session 3
  "AENTSIO14RHDL25CU",       // Session 4
  "AENTSIO14RHDL25CUMW36",   // Session 5
  "AENTSIO14RHDL25CUMW36FY", // Session 6
  "AENTSIO14RHDL25CUMW36FYGPQ79", // Session 7
  "AENTSIO14RHDL25CUMW36FYGPQ79BV", // Session 8
  "AENTSIO14RHDL25CUMW36FYGPQ79BVJK08", // Session 9
  "AENTSIO14RHDL25CUMW36FYGPQ79BVJK08XZ" // Session 10 (all alphanumeric)
};

const char* cwaSessionDescriptions[] = {
  "A E N T",
  "+ S I O 1 4",
  "+ R H D L 2 5",
  "+ C U",
  "+ M W 3 6 ?",
  "+ F Y ,",
  "+ G P Q 7 9 /",
  "+ B V <AR>",
  "+ J K 0 8 <BT>",
  "+ X Z . <BK> <SK>",
  "QSO Practice 1",
  "QSO Practice 2",
  "QSO Practice 3",
  "On-Air Prep 1",
  "On-Air Prep 2",
  "On-Air Prep 3"
};

// ============================================
// CW Academy State
// ============================================

CWATrack cwaSelectedTrack = TRACK_BEGINNER;  // Currently selected track
int cwaSelectedSession = 1;  // Currently selected session (1-16)
CWAPracticeType cwaSelectedPracticeType = PRACTICE_COPY;  // Currently selected practice type
CWAMessageType cwaSelectedMessageType = MESSAGE_CHARACTERS;  // Currently selected message type

// Preferences for saving progress
Preferences cwaPrefs;

// LVGL mode flag - when true, skip legacy draw functions (LVGL handles display)
bool cwaUseLVGL = true;  // Default to LVGL mode

// Settings (declared extern from settings_cw.h)
extern int cwSpeed;
extern int cwTone;
// Note: cwKeyType is KeyType enum, not int - see settings_cw.h

// ============================================
// Helper Functions
// ============================================

/*
 * Count items in a null-terminated string array
 */
int countArrayItems(const char** arr) {
  if (arr == nullptr) return 0;
  int count = 0;
  while (arr[count] != nullptr) {
    count++;
  }
  return count;
}

/*
 * Select random items from array and concatenate with spaces
 */
String selectRandomItems(const char** arr, int numItems) {
  if (arr == nullptr || numItems <= 0) return "";

  int arraySize = countArrayItems(arr);
  if (arraySize == 0) return "";

  String result = "";
  for (int i = 0; i < numItems; i++) {
    if (i > 0) result += " ";
    int index = random(arraySize);
    result += arr[index];
  }
  return result;
}

/*
 * Get the WPM for the current session based on track
 * Intermediate track uses session-specific WPM; others use global cwSpeed
 */
int getSessionWPM() {
  if (cwaSelectedTrack == TRACK_INTERMEDIATE) {
    int sessionIdx = cwaSelectedSession - 1;
    if (sessionIdx >= 0 && sessionIdx < 16) {
      return cwaIntermediateWPM[sessionIdx];
    }
  }
  return cwSpeed;  // Use global setting for Beginner and other tracks
}

/*
 * Load saved CW Academy progress
 */
void loadCWAProgress() {
  cwaPrefs.begin("cwa", false); // Read-only
  cwaSelectedTrack = (CWATrack)cwaPrefs.getInt("track", TRACK_BEGINNER);
  cwaSelectedSession = cwaPrefs.getInt("session", 1);
  cwaSelectedPracticeType = (CWAPracticeType)cwaPrefs.getInt("practype", PRACTICE_COPY);
  cwaSelectedMessageType = (CWAMessageType)cwaPrefs.getInt("msgtype", MESSAGE_CHARACTERS);
  cwaPrefs.end();
}

/*
 * Save CW Academy progress
 */
void saveCWAProgress() {
  cwaPrefs.begin("cwa", false); // Read-write
  cwaPrefs.putInt("track", (int)cwaSelectedTrack);
  cwaPrefs.putInt("session", cwaSelectedSession);
  cwaPrefs.putInt("practype", (int)cwaSelectedPracticeType);
  cwaPrefs.putInt("msgtype", (int)cwaSelectedMessageType);
  cwaPrefs.end();
}

// ============================================
// State Reset Functions
// Used when exiting modes to ensure clean state on re-entry
// ============================================

// Copy practice state (defined in training_cwa_copy_practice.h)
extern String cwaCopyTarget;
extern String cwaCopyInput;
extern int cwaCopyRound;
extern int cwaCopyCorrect;
extern int cwaCopyTotal;
extern int cwaCopyCharCount;
extern bool cwaCopyWaitingForInput;
extern bool cwaCopyShowingFeedback;

// Sending practice state (defined in training_cwa_send_practice.h)
extern String cwaSendTarget;
extern String cwaSendDecoded;
extern int cwaSendRound;
extern int cwaSendCorrect;
extern int cwaSendTotal;
extern bool cwaSendWaitingForSend;
extern bool cwaSendShowingFeedback;
extern bool cwaSendShowReference;

/*
 * Reset copy practice state
 */
void resetCWACopyPracticeState() {
  cwaCopyTarget = "";
  cwaCopyInput = "";
  cwaCopyRound = 0;
  cwaCopyCorrect = 0;
  cwaCopyTotal = 0;
  cwaCopyWaitingForInput = false;
  cwaCopyShowingFeedback = false;
}

/*
 * Reset sending practice state
 */
void resetCWASendingPracticeState() {
  cwaSendTarget = "";
  cwaSendDecoded = "";
  cwaSendRound = 0;
  cwaSendCorrect = 0;
  cwaSendTotal = 0;
  cwaSendWaitingForSend = false;
  cwaSendShowingFeedback = false;
  cwaSendShowReference = true;
  requestStopTone();  // Ensure any tone is stopped
}

#endif // TRAINING_CWA_CORE_H
