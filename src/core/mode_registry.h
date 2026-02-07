/*
 * VAIL SUMMIT - Mode Registry
 * Centralizes all mode metadata: parent hierarchy, flags, cleanup/poll callbacks.
 * Replaces scattered switch statements and if/else chains with data tables.
 */

#ifndef MODE_REGISTRY_H
#define MODE_REGISTRY_H

#include "modes.h"

// ============================================
// Mode Flags
// ============================================

#define MODE_FLAG_MENU           0x01  // Menu screen (not an active feature)
#define MODE_FLAG_PURE_NAV       0x02  // Pure navigation menu (safe for global hotkeys)
#define MODE_FLAG_SETTINGS       0x04  // Settings screen
#define MODE_FLAG_AUDIO_CRITICAL 0x08  // Needs fast polling (1ms loop delay)
#define MODE_FLAG_NO_STATUS      0x10  // Skip periodic status bar updates

// ============================================
// Parent Mode Lookup Table
// ============================================

struct ModeParent {
    int16_t mode;
    int16_t parent;
};

static const ModeParent parentTable[] = {
    // Main menu (root)
    { MODE_MAIN_MENU,           MODE_MAIN_MENU },

    // Top-level menus → main
    { MODE_CW_MENU,             MODE_MAIN_MENU },
    { MODE_GAMES_MENU,          MODE_MAIN_MENU },
    { MODE_HAM_TOOLS_MENU,      MODE_MAIN_MENU },
    { MODE_SETTINGS_MENU,       MODE_MAIN_MENU },

    // CW menu items → CW menu
    { MODE_TRAINING_MENU,       MODE_CW_MENU },
    { MODE_PRACTICE,            MODE_CW_MENU },
    { MODE_VAIL_REPEATER,       MODE_CW_MENU },
    { MODE_BLUETOOTH_MENU,      MODE_CW_MENU },
    { MODE_RADIO_OUTPUT,        MODE_CW_MENU },
    { MODE_CW_MEMORIES,         MODE_CW_MENU },
    { MODE_MORSE_MAILBOX,       MODE_CW_MENU },
    { MODE_MORSE_MAILBOX_LINK,  MODE_CW_MENU },
    { MODE_MORSE_NOTES_LIBRARY, MODE_CW_MENU },

    // Training menu items → Training menu
    { MODE_HEAR_IT_MENU,                MODE_TRAINING_MENU },
    { MODE_HEAR_IT_TYPE_IT,             MODE_TRAINING_MENU },
    { MODE_HEAR_IT_START,               MODE_TRAINING_MENU },
    { MODE_CW_ACADEMY_TRACK_SELECT,     MODE_TRAINING_MENU },
    { MODE_VAIL_MASTER,                 MODE_TRAINING_MENU },
    { MODE_LICW_CAROUSEL_SELECT,        MODE_TRAINING_MENU },
    { MODE_CWSCHOOL,                    MODE_TRAINING_MENU },

    // Vail Master sub-screens → Vail Master
    { MODE_VAIL_MASTER_PRACTICE,  MODE_VAIL_MASTER },
    { MODE_VAIL_MASTER_SETTINGS,  MODE_VAIL_MASTER },
    { MODE_VAIL_MASTER_HISTORY,   MODE_VAIL_MASTER },
    { MODE_VAIL_MASTER_CHARSET,   MODE_VAIL_MASTER },

    // Hear It submenu → Hear It menu
    { MODE_HEAR_IT_CONFIGURE,   MODE_HEAR_IT_MENU },

    // Games menu items → Games menu
    { MODE_MORSE_SHOOTER,       MODE_GAMES_MENU },
    { MODE_MORSE_MEMORY,        MODE_GAMES_MENU },
    { MODE_SPARK_WATCH,         MODE_GAMES_MENU },
    { MODE_STORY_TIME,          MODE_GAMES_MENU },
    { MODE_CW_SPEEDER_SELECT,   MODE_GAMES_MENU },

    // Spark Watch hierarchy
    { MODE_SPARK_WATCH_DIFFICULTY, MODE_SPARK_WATCH },
    { MODE_SPARK_WATCH_CAMPAIGN,   MODE_SPARK_WATCH },
    { MODE_SPARK_WATCH_SETTINGS,   MODE_SPARK_WATCH },
    { MODE_SPARK_WATCH_STATS,      MODE_SPARK_WATCH },
    { MODE_SPARK_WATCH_CHALLENGE,  MODE_SPARK_WATCH_DIFFICULTY },
    { MODE_SPARK_WATCH_MISSION,    MODE_SPARK_WATCH_CAMPAIGN },
    { MODE_SPARK_WATCH_BRIEFING,   MODE_SPARK_WATCH_CHALLENGE },
    { MODE_SPARK_WATCH_GAMEPLAY,   MODE_SPARK_WATCH_BRIEFING },
    { MODE_SPARK_WATCH_RESULTS,    MODE_SPARK_WATCH_GAMEPLAY },
    { MODE_SPARK_WATCH_DEBRIEFING, MODE_SPARK_WATCH_GAMEPLAY },

    // Story Time hierarchy
    { MODE_STORY_TIME_DIFFICULTY,  MODE_STORY_TIME },
    { MODE_STORY_TIME_PROGRESS,    MODE_STORY_TIME },
    { MODE_STORY_TIME_SETTINGS,    MODE_STORY_TIME },
    { MODE_STORY_TIME_LIST,        MODE_STORY_TIME_DIFFICULTY },
    { MODE_STORY_TIME_LISTEN,      MODE_STORY_TIME_LIST },
    { MODE_STORY_TIME_QUIZ,        MODE_STORY_TIME_LISTEN },
    { MODE_STORY_TIME_RESULTS,     MODE_STORY_TIME_QUIZ },

    // Settings hierarchy
    { MODE_DEVICE_SETTINGS_MENU,   MODE_SETTINGS_MENU },
    { MODE_CW_SETTINGS,            MODE_SETTINGS_MENU },

    // Device settings items → Device settings menu
    { MODE_WIFI_SUBMENU,          MODE_DEVICE_SETTINGS_MENU },
    { MODE_GENERAL_SUBMENU,       MODE_DEVICE_SETTINGS_MENU },
    { MODE_DEVICE_BT_SUBMENU,     MODE_DEVICE_SETTINGS_MENU },
    { MODE_SYSTEM_INFO,           MODE_DEVICE_SETTINGS_MENU },

    // WiFi submenu items
    { MODE_WIFI_SETTINGS,          MODE_WIFI_SUBMENU },
    { MODE_WEB_PASSWORD_SETTINGS,  MODE_WIFI_SUBMENU },

    // General submenu items
    { MODE_CALLSIGN_SETTINGS,      MODE_GENERAL_SUBMENU },
    { MODE_VOLUME_SETTINGS,        MODE_GENERAL_SUBMENU },
    { MODE_BRIGHTNESS_SETTINGS,    MODE_GENERAL_SUBMENU },
    { MODE_THEME_SETTINGS,         MODE_GENERAL_SUBMENU },

    // Device BT submenu items
    { MODE_BT_KEYBOARD_SETTINGS,   MODE_DEVICE_BT_SUBMENU },

    // Bluetooth menu items → Bluetooth menu
    { MODE_BT_HID,                 MODE_BLUETOOTH_MENU },
    { MODE_BT_MIDI,                MODE_BLUETOOTH_MENU },

    // Ham Tools items → Ham Tools menu
    { MODE_QSO_LOGGER_MENU,        MODE_HAM_TOOLS_MENU },
    { MODE_BAND_PLANS,             MODE_HAM_TOOLS_MENU },
    { MODE_PROPAGATION,            MODE_HAM_TOOLS_MENU },
    { MODE_ANTENNAS,               MODE_HAM_TOOLS_MENU },
    { MODE_LICENSE_SELECT,         MODE_HAM_TOOLS_MENU },
    { MODE_SUMMIT_CHAT,            MODE_HAM_TOOLS_MENU },
    { MODE_POTA_MENU,              MODE_HAM_TOOLS_MENU },

    // POTA hierarchy
    { MODE_POTA_ACTIVE_SPOTS,      MODE_POTA_MENU },
    { MODE_POTA_ACTIVATE,          MODE_POTA_MENU },
    { MODE_POTA_RECORDER_SETUP,    MODE_POTA_MENU },
    { MODE_POTA_SPOT_DETAIL,       MODE_POTA_ACTIVE_SPOTS },
    { MODE_POTA_FILTERS,           MODE_POTA_ACTIVE_SPOTS },
    { MODE_POTA_RECORDER,          MODE_POTA_RECORDER_SETUP },

    // QSO Logger items → QSO Logger menu
    { MODE_QSO_LOG_ENTRY,          MODE_QSO_LOGGER_MENU },
    { MODE_QSO_VIEW_LOGS,          MODE_QSO_LOGGER_MENU },
    { MODE_QSO_STATISTICS,         MODE_QSO_LOGGER_MENU },
    { MODE_QSO_LOGGER_SETTINGS,    MODE_QSO_LOGGER_MENU },

    // CW Academy hierarchy
    { MODE_CW_ACADEMY_SESSION_SELECT,       MODE_CW_ACADEMY_TRACK_SELECT },
    { MODE_CW_ACADEMY_PRACTICE_TYPE_SELECT, MODE_CW_ACADEMY_SESSION_SELECT },
    { MODE_CW_ACADEMY_MESSAGE_TYPE_SELECT,  MODE_CW_ACADEMY_PRACTICE_TYPE_SELECT },
    { MODE_CW_ACADEMY_COPY_PRACTICE,        MODE_CW_ACADEMY_MESSAGE_TYPE_SELECT },
    { MODE_CW_ACADEMY_SENDING_PRACTICE,     MODE_CW_ACADEMY_MESSAGE_TYPE_SELECT },
    { MODE_CW_ACADEMY_QSO_PRACTICE,         MODE_CW_ACADEMY_PRACTICE_TYPE_SELECT },

    // License hierarchy
    { MODE_LICENSE_QUIZ,            MODE_LICENSE_SELECT },
    { MODE_LICENSE_STATS,           MODE_LICENSE_SELECT },
    { MODE_LICENSE_DOWNLOAD,        MODE_LICENSE_SELECT },
    { MODE_LICENSE_WIFI_ERROR,      MODE_LICENSE_SELECT },
    { MODE_LICENSE_SD_ERROR,        MODE_LICENSE_SELECT },
    { MODE_LICENSE_ALL_STATS,       MODE_LICENSE_SELECT },

    // LICW hierarchy
    { MODE_LICW_LESSON_SELECT,      MODE_LICW_CAROUSEL_SELECT },
    { MODE_LICW_PRACTICE_TYPE,      MODE_LICW_LESSON_SELECT },
    { MODE_LICW_COPY_PRACTICE,      MODE_LICW_PRACTICE_TYPE },
    { MODE_LICW_SEND_PRACTICE,      MODE_LICW_PRACTICE_TYPE },
    { MODE_LICW_TTR_PRACTICE,       MODE_LICW_PRACTICE_TYPE },
    { MODE_LICW_IFR_PRACTICE,       MODE_LICW_PRACTICE_TYPE },
    { MODE_LICW_CSF_INTRO,          MODE_LICW_PRACTICE_TYPE },
    { MODE_LICW_WORD_DISCOVERY,     MODE_LICW_PRACTICE_TYPE },
    { MODE_LICW_QSO_PRACTICE,       MODE_LICW_PRACTICE_TYPE },
    { MODE_LICW_CFP_PRACTICE,       MODE_LICW_PRACTICE_TYPE },
    { MODE_LICW_ADVERSE_COPY,       MODE_LICW_PRACTICE_TYPE },
    { MODE_LICW_SETTINGS,           MODE_LICW_CAROUSEL_SELECT },
    { MODE_LICW_PROGRESS,           MODE_LICW_CAROUSEL_SELECT },

    // CW Speeder
    { MODE_CW_SPEEDER,              MODE_CW_SPEEDER_SELECT },

    // Morse Mailbox hierarchy
    { MODE_MORSE_MAILBOX_INBOX,     MODE_MORSE_MAILBOX },
    { MODE_MORSE_MAILBOX_ACCOUNT,   MODE_MORSE_MAILBOX },
    { MODE_MORSE_MAILBOX_PLAYBACK,  MODE_MORSE_MAILBOX_INBOX },
    { MODE_MORSE_MAILBOX_COMPOSE,   MODE_MORSE_MAILBOX_INBOX },

    // Morse Notes hierarchy
    { MODE_MORSE_NOTES_RECORD,      MODE_MORSE_NOTES_LIBRARY },
    { MODE_MORSE_NOTES_PLAYBACK,    MODE_MORSE_NOTES_LIBRARY },
    { MODE_MORSE_NOTES_SETTINGS,    MODE_MORSE_NOTES_LIBRARY },

    // CW School hierarchy
    { MODE_CWSCHOOL_LINK,           MODE_CWSCHOOL },
    { MODE_CWSCHOOL_ACCOUNT,        MODE_CWSCHOOL },
    { MODE_CWSCHOOL_TRAINING,       MODE_CWSCHOOL },
    { MODE_CWSCHOOL_PROGRESS,       MODE_CWSCHOOL },

    // Vail Course hierarchy
    { MODE_VAIL_COURSE_MODULE_SELECT,  MODE_CWSCHOOL_TRAINING },
    { MODE_VAIL_COURSE_LESSON_SELECT,  MODE_VAIL_COURSE_MODULE_SELECT },
    { MODE_VAIL_COURSE_LESSON,         MODE_VAIL_COURSE_LESSON_SELECT },
    { MODE_VAIL_COURSE_PROGRESS,       MODE_VAIL_COURSE_MODULE_SELECT },

    // Web-triggered modes (entered via web interface, ESC returns to CW menu)
    { MODE_WEB_PRACTICE,               MODE_CW_MENU },
    { MODE_WEB_MEMORY_CHAIN,           MODE_CW_MENU },
    { MODE_WEB_HEAR_IT,                MODE_CW_MENU },

    // Web files update (in WiFi submenu, triggers reboot)
    { MODE_WEB_FILES_UPDATE,           MODE_WIFI_SUBMENU },
};

static const int parentTableSize = sizeof(parentTable) / sizeof(parentTable[0]);

// Look up parent mode for back navigation
// Returns MODE_MAIN_MENU if mode not found in table
static int lookupParentMode(int mode) {
    for (int i = 0; i < parentTableSize; i++) {
        if (parentTable[i].mode == mode) return parentTable[i].parent;
    }
    return MODE_MAIN_MENU;
}

// ============================================
// Mode Flag Lookup
// ============================================

struct ModeFlags {
    int16_t mode;
    uint8_t flags;
};

static const ModeFlags flagTable[] = {
    // Menu screens (FLAG_MENU | FLAG_PURE_NAV for nav-safe menus)
    { MODE_MAIN_MENU,           MODE_FLAG_MENU | MODE_FLAG_PURE_NAV },
    { MODE_CW_MENU,             MODE_FLAG_MENU | MODE_FLAG_PURE_NAV },
    { MODE_TRAINING_MENU,       MODE_FLAG_MENU | MODE_FLAG_PURE_NAV },
    { MODE_GAMES_MENU,          MODE_FLAG_MENU | MODE_FLAG_PURE_NAV },
    { MODE_SETTINGS_MENU,       MODE_FLAG_MENU | MODE_FLAG_PURE_NAV },
    { MODE_DEVICE_SETTINGS_MENU, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV },
    { MODE_HAM_TOOLS_MENU,      MODE_FLAG_MENU | MODE_FLAG_PURE_NAV },
    { MODE_BLUETOOTH_MENU,      MODE_FLAG_MENU | MODE_FLAG_PURE_NAV },
    { MODE_QSO_LOGGER_MENU,     MODE_FLAG_MENU | MODE_FLAG_PURE_NAV },
    // Menu-only (not pure nav — may have text input overlays)
    { MODE_WIFI_SUBMENU,        MODE_FLAG_MENU },
    { MODE_GENERAL_SUBMENU,     MODE_FLAG_MENU },
    { MODE_HEAR_IT_MENU,        MODE_FLAG_MENU },
    { MODE_DEVICE_BT_SUBMENU,   MODE_FLAG_MENU },
    { MODE_LICENSE_SELECT,       MODE_FLAG_MENU },

    // Settings screens
    { MODE_VOLUME_SETTINGS,      MODE_FLAG_SETTINGS },
    { MODE_BRIGHTNESS_SETTINGS,  MODE_FLAG_SETTINGS },
    { MODE_CW_SETTINGS,          MODE_FLAG_SETTINGS },
    { MODE_CALLSIGN_SETTINGS,    MODE_FLAG_SETTINGS },
    { MODE_WEB_PASSWORD_SETTINGS, MODE_FLAG_SETTINGS },
    { MODE_WIFI_SETTINGS,        MODE_FLAG_SETTINGS },
    { MODE_BT_KEYBOARD_SETTINGS, MODE_FLAG_SETTINGS },
    { MODE_THEME_SETTINGS,       MODE_FLAG_SETTINGS },
    { MODE_SYSTEM_INFO,          MODE_FLAG_SETTINGS },
    { MODE_MORSE_NOTES_SETTINGS, MODE_FLAG_SETTINGS },

    // Audio-critical modes (need 1ms polling)
    { MODE_PRACTICE,                    MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_CW_ACADEMY_SENDING_PRACTICE, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_MORSE_SHOOTER,               MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_MORSE_MEMORY,                MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_RADIO_OUTPUT,                MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_WEB_PRACTICE,                MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_VAIL_REPEATER,               MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_BT_HID,                      MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_BT_MIDI,                     MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_VAIL_MASTER_PRACTICE,        MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_CW_SPEEDER,                  MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_MORSE_NOTES_RECORD,          MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },

    // LICW audio-critical practice modes
    { MODE_LICW_COPY_PRACTICE,          MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_LICW_SEND_PRACTICE,          MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_LICW_IFR_PRACTICE,           MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_LICW_CFP_PRACTICE,           MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_LICW_WORD_DISCOVERY,         MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_LICW_QSO_PRACTICE,           MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_LICW_ADVERSE_COPY,           MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },

    // Vail Course lesson needs audio-critical
    { MODE_VAIL_COURSE_LESSON,          MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },

    // CWA copy and QSO also need audio-critical
    { MODE_CW_ACADEMY_COPY_PRACTICE,    MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },
    { MODE_CW_ACADEMY_QSO_PRACTICE,     MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },

    // Web Memory Chain needs audio-critical for decoder fast polling
    { MODE_WEB_MEMORY_CHAIN,            MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS },

    // Non-audio modes that still skip status updates
    { MODE_HEAR_IT_TYPE_IT,             MODE_FLAG_NO_STATUS },
    { MODE_WEB_HEAR_IT,                 MODE_FLAG_NO_STATUS },
};

static const int flagTableSize = sizeof(flagTable) / sizeof(flagTable[0]);

// Look up flags for a mode (returns 0 if not in table)
static uint8_t lookupModeFlags(int mode) {
    for (int i = 0; i < flagTableSize; i++) {
        if (flagTable[i].mode == mode) return flagTable[i].flags;
    }
    return 0;
}

// Convenience flag check functions
static bool isModeMenu(int mode) { return lookupModeFlags(mode) & MODE_FLAG_MENU; }
static bool isModePureNav(int mode) { return lookupModeFlags(mode) & MODE_FLAG_PURE_NAV; }
static bool isModeSettings(int mode) { return lookupModeFlags(mode) & MODE_FLAG_SETTINGS; }
static bool isModeAudioCritical(int mode) { return lookupModeFlags(mode) & MODE_FLAG_AUDIO_CRITICAL; }
static bool isModeNoStatus(int mode) { return lookupModeFlags(mode) & MODE_FLAG_NO_STATUS; }

// ============================================
// Training Mode Name Lookup
// ============================================

struct TrainingEntry {
    int16_t mode;
    const char* name;
};

static const TrainingEntry trainingTable[] = {
    { MODE_PRACTICE,                     "Practice" },
    { MODE_CW_ACADEMY_COPY_PRACTICE,     "CWA" },
    { MODE_CW_ACADEMY_SENDING_PRACTICE,  "CWA" },
    { MODE_CW_ACADEMY_QSO_PRACTICE,      "CWA" },
    { MODE_HEAR_IT_TYPE_IT,              "HearIt" },
    { MODE_HEAR_IT_START,                "HearIt" },
    { MODE_VAIL_MASTER,                  "VailMaster" },
    { MODE_VAIL_MASTER_PRACTICE,         "VailMaster" },
    { MODE_LICW_COPY_PRACTICE,           "LICW" },
    { MODE_LICW_SEND_PRACTICE,           "LICW" },
    { MODE_LICW_TTR_PRACTICE,            "LICW" },
    { MODE_LICW_IFR_PRACTICE,            "LICW" },
    { MODE_LICW_CSF_INTRO,              "LICW" },
    { MODE_LICW_WORD_DISCOVERY,          "LICW" },
    { MODE_LICW_QSO_PRACTICE,            "LICW" },
    { MODE_LICW_CFP_PRACTICE,            "LICW" },
    { MODE_LICW_ADVERSE_COPY,            "LICW" },
    { MODE_CWSCHOOL_TRAINING,            "CWSchool" },
    { MODE_VAIL_COURSE_LESSON,           "VailCourse" },
    { MODE_MORSE_MEMORY,                 "MemoryChain" },
    { MODE_CW_SPEEDER,                   "CWSpeeder" },
    { MODE_STORY_TIME_LISTEN,            "StoryTime" },
    { MODE_STORY_TIME_QUIZ,              "StoryTime" },
};

static const int trainingTableSize = sizeof(trainingTable) / sizeof(trainingTable[0]);

// Look up training name for a mode (returns NULL if not a training mode)
static const char* lookupTrainingName(int mode) {
    for (int i = 0; i < trainingTableSize; i++) {
        if (trainingTable[i].mode == mode) return trainingTable[i].name;
    }
    return NULL;
}

// Check if mode is a training/practice mode
static bool isModeTraining(int mode) {
    return lookupTrainingName(mode) != NULL;
}

// ============================================
// Callback Types for Cleanup and Poll
// ============================================
// Actual tables defined in lv_mode_integration.h (cleanup) and vail-summit.ino (poll)
// since they reference functions from those files.

typedef void (*ModeCallback)();

struct ModeCallbackEntry {
    int16_t mode;
    ModeCallback callback;
};

// Generic dispatch function for callback tables
static void dispatchModeCallback(const ModeCallbackEntry* table, int tableSize, int mode) {
    for (int i = 0; i < tableSize; i++) {
        if (table[i].mode == mode) {
            table[i].callback();
            return;
        }
    }
}

#endif // MODE_REGISTRY_H
