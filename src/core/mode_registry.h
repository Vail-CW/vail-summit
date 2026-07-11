/*
 * VAIL SUMMIT - Mode Registry
 * Centralizes all mode metadata: parent hierarchy, flags, training names,
 * and (via separate callback tables) cleanup/poll dispatch.
 *
 * All per-mode metadata lives in ONE descriptor table (modeInfoTable). Adding
 * or removing a mode's parent/flags/training-name is a single-row edit here.
 *
 * Function-pointer tables (cleanup, poll) remain separate by necessity: this is
 * a single-translation-unit, header-only build, and those tables reference
 * functions defined late in the include chain. cleanupTable lives in
 * lv_mode_integration.h; pollTable lives in vail-summit.ino. Both reuse the
 * ModeCallbackEntry / dispatchModeCallback mechanism at the bottom of this file.
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
// Unified Mode Descriptor Table
// ============================================
// One row per mode. Columns:
//   mode          - the MenuMode this row describes
//   parent        - parent mode for ESC/back navigation (root modes point to self)
//   flags         - bitwise OR of MODE_FLAG_* (0 = none)
//   trainingName  - practice-time tracking name, or nullptr if not a training mode
//
// A mode absent from this table behaves as: parent = MODE_MAIN_MENU, flags = 0,
// not a training mode (matching the historical per-table defaults).

struct ModeInfo {
    int16_t     mode;
    int16_t     parent;
    uint8_t     flags;
    const char* trainingName;
};

static const ModeInfo modeInfoTable[] = {
    // mode, parent, flags, trainingName

    // Home dashboard (true root / boot screen)
    { MODE_HOME, MODE_HOME, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },

    // Vail CW School: unified hub + practice modes. Hub's ESC parent is HOME
    // (the home hero arms the home-return override); practice modes return to
    // the hub. Daily/Copy/Send carry training names so they count toward streak.
    { MODE_SCHOOL_HUB,   MODE_HOME,       MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },
    { MODE_SCHOOL_DAILY, MODE_SCHOOL_HUB, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "Daily" },
    { MODE_SCHOOL_COPY,  MODE_SCHOOL_HUB, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "Copy" },
    { MODE_SCHOOL_SEND,  MODE_SCHOOL_HUB, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "Send" },

    // Main menu — the legacy grid, now reachable via the "More" launcher.
    // Its ESC parent is MODE_HOME so back-nav from the old menu returns home.
    { MODE_MAIN_MENU, MODE_HOME, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },

    // Top-level menus → main
    { MODE_CW_MENU, MODE_MAIN_MENU, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },
    { MODE_GAMES_MENU, MODE_MAIN_MENU, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },
    { MODE_HAM_TOOLS_MENU, MODE_MAIN_MENU, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },
    { MODE_SETTINGS_MENU, MODE_MAIN_MENU, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },

    // CW menu items → CW menu
    { MODE_TRAINING_MENU, MODE_CW_MENU, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },
    { MODE_PRACTICE, MODE_CW_MENU, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "Practice" },
    { MODE_VAIL_REPEATER, MODE_CW_MENU, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, nullptr },
    { MODE_BLUETOOTH_MENU, MODE_CW_MENU, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },
    { MODE_RADIO_OUTPUT, MODE_CW_MENU, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, nullptr },
    { MODE_CW_MEMORIES, MODE_CW_MENU, 0, nullptr },
    { MODE_MORSE_MAILBOX, MODE_CW_MENU, 0, nullptr },
    { MODE_MORSE_MAILBOX_LINK, MODE_CW_MENU, 0, nullptr },
    { MODE_MORSE_NOTES_LIBRARY, MODE_CW_MENU, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },

    // Training menu items → Training menu
    { MODE_HEAR_IT_MENU, MODE_TRAINING_MENU, MODE_FLAG_MENU, nullptr },
    { MODE_HEAR_IT_TYPE_IT, MODE_TRAINING_MENU, MODE_FLAG_NO_STATUS, "HearIt" },
    { MODE_HEAR_IT_START, MODE_TRAINING_MENU, 0, "HearIt" },
    { MODE_CW_ACADEMY_TRACK_SELECT, MODE_TRAINING_MENU, 0, nullptr },
    { MODE_VAIL_MASTER, MODE_TRAINING_MENU, 0, "VailMaster" },
    { MODE_LICW_CAROUSEL_SELECT, MODE_TRAINING_MENU, 0, nullptr },
    { MODE_CWSCHOOL, MODE_TRAINING_MENU, 0, nullptr },

    // Vail Master sub-screens → Vail Master
    { MODE_VAIL_MASTER_PRACTICE, MODE_VAIL_MASTER, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "VailMaster" },
    { MODE_VAIL_MASTER_SETTINGS, MODE_VAIL_MASTER, 0, nullptr },
    { MODE_VAIL_MASTER_HISTORY, MODE_VAIL_MASTER, 0, nullptr },
    { MODE_VAIL_MASTER_CHARSET, MODE_VAIL_MASTER, 0, nullptr },

    // Hear It submenu → Hear It menu
    { MODE_HEAR_IT_CONFIGURE, MODE_HEAR_IT_MENU, 0, nullptr },

    // Games menu items → Games menu
    { MODE_MORSE_SHOOTER, MODE_GAMES_MENU, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, nullptr },
    { MODE_MORSE_MEMORY, MODE_GAMES_MENU, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "MemoryChain" },
    { MODE_SPARK_WATCH, MODE_GAMES_MENU, 0, nullptr },
    { MODE_STORY_TIME, MODE_GAMES_MENU, 0, nullptr },
    { MODE_CW_SPEEDER_SELECT, MODE_GAMES_MENU, 0, nullptr },

    // Spark Watch hierarchy
    { MODE_SPARK_WATCH_DIFFICULTY, MODE_SPARK_WATCH, 0, nullptr },
    { MODE_SPARK_WATCH_CAMPAIGN, MODE_SPARK_WATCH, 0, nullptr },
    { MODE_SPARK_WATCH_SETTINGS, MODE_SPARK_WATCH, 0, nullptr },
    { MODE_SPARK_WATCH_STATS, MODE_SPARK_WATCH, 0, nullptr },
    { MODE_SPARK_WATCH_CHALLENGE, MODE_SPARK_WATCH_DIFFICULTY, 0, nullptr },
    { MODE_SPARK_WATCH_MISSION, MODE_SPARK_WATCH_CAMPAIGN, 0, nullptr },
    { MODE_SPARK_WATCH_BRIEFING, MODE_SPARK_WATCH_CHALLENGE, 0, nullptr },
    { MODE_SPARK_WATCH_GAMEPLAY, MODE_SPARK_WATCH_BRIEFING, 0, nullptr },
    { MODE_SPARK_WATCH_RESULTS, MODE_SPARK_WATCH_GAMEPLAY, 0, nullptr },
    { MODE_SPARK_WATCH_DEBRIEFING, MODE_SPARK_WATCH_GAMEPLAY, 0, nullptr },

    // Story Time hierarchy
    { MODE_STORY_TIME_DIFFICULTY, MODE_STORY_TIME, 0, nullptr },
    { MODE_STORY_TIME_PROGRESS, MODE_STORY_TIME, 0, nullptr },
    { MODE_STORY_TIME_SETTINGS, MODE_STORY_TIME, 0, nullptr },
    { MODE_STORY_TIME_LIST, MODE_STORY_TIME_DIFFICULTY, 0, nullptr },
    { MODE_STORY_TIME_LISTEN, MODE_STORY_TIME_LIST, 0, "StoryTime" },
    { MODE_STORY_TIME_QUIZ, MODE_STORY_TIME_LISTEN, 0, "StoryTime" },
    { MODE_STORY_TIME_RESULTS, MODE_STORY_TIME_QUIZ, 0, nullptr },

    // Settings hierarchy
    { MODE_DEVICE_SETTINGS_MENU, MODE_SETTINGS_MENU, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },
    { MODE_CW_SETTINGS, MODE_SETTINGS_MENU, MODE_FLAG_SETTINGS, nullptr },

    // Device settings items → Device settings menu
    { MODE_WIFI_SUBMENU, MODE_DEVICE_SETTINGS_MENU, MODE_FLAG_MENU, nullptr },
    { MODE_GENERAL_SUBMENU, MODE_DEVICE_SETTINGS_MENU, MODE_FLAG_MENU, nullptr },
    { MODE_DEVICE_BT_SUBMENU, MODE_DEVICE_SETTINGS_MENU, MODE_FLAG_MENU, nullptr },
    { MODE_SYSTEM_INFO, MODE_DEVICE_SETTINGS_MENU, MODE_FLAG_SETTINGS, nullptr },
    { MODE_FACTORY_RESET, MODE_DEVICE_SETTINGS_MENU, 0, nullptr },
    { MODE_ONBOARDING, MODE_HOME, 0, nullptr },

    // WiFi submenu items
    { MODE_WIFI_SETTINGS, MODE_WIFI_SUBMENU, MODE_FLAG_SETTINGS, nullptr },
    { MODE_WEB_PASSWORD_SETTINGS, MODE_WIFI_SUBMENU, MODE_FLAG_SETTINGS, nullptr },

    // General submenu items
    { MODE_CALLSIGN_SETTINGS, MODE_GENERAL_SUBMENU, MODE_FLAG_SETTINGS, nullptr },
    { MODE_VOLUME_SETTINGS, MODE_GENERAL_SUBMENU, MODE_FLAG_SETTINGS, nullptr },
    { MODE_BRIGHTNESS_SETTINGS, MODE_GENERAL_SUBMENU, MODE_FLAG_SETTINGS, nullptr },
    { MODE_THEME_SETTINGS, MODE_GENERAL_SUBMENU, MODE_FLAG_SETTINGS, nullptr },

    // Device BT submenu items
    { MODE_BT_KEYBOARD_SETTINGS, MODE_DEVICE_BT_SUBMENU, MODE_FLAG_SETTINGS, nullptr },

    // Bluetooth menu items → Bluetooth menu
    { MODE_BT_HID, MODE_BLUETOOTH_MENU, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, nullptr },
    { MODE_BT_MIDI, MODE_BLUETOOTH_MENU, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, nullptr },

    // Ham Tools items → Ham Tools menu
    { MODE_QSO_LOGGER_MENU, MODE_HAM_TOOLS_MENU, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },
    { MODE_BAND_PLANS, MODE_HAM_TOOLS_MENU, 0, nullptr },
    { MODE_PROPAGATION, MODE_HAM_TOOLS_MENU, 0, nullptr },
    { MODE_ANTENNAS, MODE_HAM_TOOLS_MENU, 0, nullptr },
    { MODE_LICENSE_SELECT, MODE_HAM_TOOLS_MENU, MODE_FLAG_MENU, nullptr },
    { MODE_SUMMIT_CHAT, MODE_HAM_TOOLS_MENU, 0, nullptr },
    { MODE_POTA_MENU, MODE_HAM_TOOLS_MENU, 0, nullptr },

    // Satellite tracker hierarchy: hub menu with list/window/settings leaves.
    // Pass detail's parent is the passes list even when reached from a sky
    // window - backing into "all passes of that bird" is useful context.
    { MODE_SAT_MENU, MODE_HAM_TOOLS_MENU, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },
    { MODE_SAT_MY, MODE_SAT_MENU, 0, nullptr },
    { MODE_SAT_POPULAR, MODE_SAT_MENU, 0, nullptr },
    { MODE_SAT_LIST, MODE_SAT_MENU, 0, nullptr },
    { MODE_SAT_BYPASS, MODE_SAT_MENU, 0, nullptr },
    { MODE_SAT_WINDOW, MODE_SAT_MENU, 0, nullptr },
    { MODE_SAT_WINDOW_NOW, MODE_SAT_MENU, 0, nullptr },
    { MODE_SAT_SETTINGS, MODE_SAT_MENU, 0, nullptr },
    { MODE_SAT_TLE_UPDATE, MODE_SAT_MENU, 0, nullptr },
    { MODE_SAT_PASSES, MODE_SAT_LIST, 0, nullptr },
    { MODE_SAT_PASS_DETAIL, MODE_SAT_PASSES, 0, nullptr },
    { MODE_SAT_LIVE, MODE_SAT_PASS_DETAIL, 0, nullptr },

    // POTA hierarchy
    { MODE_POTA_ACTIVE_SPOTS, MODE_POTA_MENU, 0, nullptr },
    { MODE_POTA_ACTIVATE, MODE_POTA_MENU, 0, nullptr },
    { MODE_POTA_RECORDER_SETUP, MODE_POTA_MENU, 0, nullptr },
    { MODE_POTA_SPOT_DETAIL, MODE_POTA_ACTIVE_SPOTS, 0, nullptr },
    { MODE_POTA_FILTERS, MODE_POTA_ACTIVE_SPOTS, 0, nullptr },
    { MODE_POTA_RECORDER, MODE_POTA_RECORDER_SETUP, 0, nullptr },

    // QSO Logger items → QSO Logger menu
    { MODE_QSO_LOG_ENTRY, MODE_QSO_LOGGER_MENU, 0, nullptr },
    { MODE_QSO_VIEW_LOGS, MODE_QSO_LOGGER_MENU, 0, nullptr },
    { MODE_QSO_STATISTICS, MODE_QSO_LOGGER_MENU, 0, nullptr },
    { MODE_QSO_LOGGER_SETTINGS, MODE_QSO_LOGGER_MENU, 0, nullptr },

    // CW Academy hierarchy
    { MODE_CW_ACADEMY_SESSION_SELECT, MODE_CW_ACADEMY_TRACK_SELECT, 0, nullptr },
    { MODE_CW_ACADEMY_PRACTICE_TYPE_SELECT, MODE_CW_ACADEMY_SESSION_SELECT, 0, nullptr },
    { MODE_CW_ACADEMY_MESSAGE_TYPE_SELECT, MODE_CW_ACADEMY_PRACTICE_TYPE_SELECT, 0, nullptr },
    { MODE_CW_ACADEMY_COPY_PRACTICE, MODE_CW_ACADEMY_MESSAGE_TYPE_SELECT, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "CWA" },
    { MODE_CW_ACADEMY_SENDING_PRACTICE, MODE_CW_ACADEMY_MESSAGE_TYPE_SELECT, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "CWA" },
    { MODE_CW_ACADEMY_QSO_PRACTICE, MODE_CW_ACADEMY_PRACTICE_TYPE_SELECT, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "CWA" },

    // License hierarchy
    { MODE_LICENSE_QUIZ, MODE_LICENSE_SELECT, 0, nullptr },
    { MODE_LICENSE_STATS, MODE_LICENSE_SELECT, 0, nullptr },
    { MODE_LICENSE_DOWNLOAD, MODE_LICENSE_SELECT, 0, nullptr },
    { MODE_LICENSE_WIFI_ERROR, MODE_LICENSE_SELECT, 0, nullptr },
    { MODE_LICENSE_SD_ERROR, MODE_LICENSE_SELECT, 0, nullptr },
    { MODE_LICENSE_ALL_STATS, MODE_LICENSE_SELECT, 0, nullptr },

    // LICW hierarchy
    { MODE_LICW_LESSON_SELECT, MODE_LICW_CAROUSEL_SELECT, 0, nullptr },
    { MODE_LICW_PRACTICE_TYPE, MODE_LICW_LESSON_SELECT, 0, nullptr },
    { MODE_LICW_COPY_PRACTICE, MODE_LICW_PRACTICE_TYPE, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "LICW" },
    { MODE_LICW_SEND_PRACTICE, MODE_LICW_PRACTICE_TYPE, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "LICW" },
    { MODE_LICW_TTR_PRACTICE, MODE_LICW_PRACTICE_TYPE, 0, "LICW" },
    { MODE_LICW_IFR_PRACTICE, MODE_LICW_PRACTICE_TYPE, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "LICW" },
    { MODE_LICW_CSF_INTRO, MODE_LICW_PRACTICE_TYPE, 0, "LICW" },
    { MODE_LICW_WORD_DISCOVERY, MODE_LICW_PRACTICE_TYPE, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "LICW" },
    { MODE_LICW_QSO_PRACTICE, MODE_LICW_PRACTICE_TYPE, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "LICW" },
    { MODE_LICW_CFP_PRACTICE, MODE_LICW_PRACTICE_TYPE, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "LICW" },
    { MODE_LICW_ADVERSE_COPY, MODE_LICW_PRACTICE_TYPE, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "LICW" },
    { MODE_LICW_SETTINGS, MODE_LICW_CAROUSEL_SELECT, 0, nullptr },
    { MODE_LICW_PROGRESS, MODE_LICW_CAROUSEL_SELECT, 0, nullptr },

    // CW Speeder
    { MODE_CW_SPEEDER, MODE_CW_SPEEDER_SELECT, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "CWSpeeder" },

    // Morse Mailbox hierarchy
    { MODE_MORSE_MAILBOX_INBOX, MODE_MORSE_MAILBOX, 0, nullptr },
    { MODE_MORSE_MAILBOX_ACCOUNT, MODE_MORSE_MAILBOX, 0, nullptr },
    { MODE_MORSE_MAILBOX_PLAYBACK, MODE_MORSE_MAILBOX_INBOX, 0, nullptr },
    { MODE_MORSE_MAILBOX_COMPOSE, MODE_MORSE_MAILBOX_INBOX, 0, nullptr },

    // Morse Notes hierarchy
    { MODE_MORSE_NOTES_LIST, MODE_MORSE_NOTES_LIBRARY, MODE_FLAG_MENU | MODE_FLAG_PURE_NAV, nullptr },
    { MODE_MORSE_NOTES_RECORD, MODE_MORSE_NOTES_LIBRARY, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, nullptr },
    { MODE_MORSE_NOTES_PLAYBACK, MODE_MORSE_NOTES_LIST, 0, nullptr },
    { MODE_MORSE_NOTES_SETTINGS, MODE_MORSE_NOTES_LIBRARY, MODE_FLAG_SETTINGS, nullptr },

    // CW School hierarchy
    { MODE_CWSCHOOL_LINK, MODE_CWSCHOOL, 0, nullptr },
    { MODE_CWSCHOOL_ACCOUNT, MODE_CWSCHOOL, 0, nullptr },
    { MODE_CWSCHOOL_TRAINING, MODE_CWSCHOOL, 0, "CWSchool" },
    { MODE_CWSCHOOL_PROGRESS, MODE_CWSCHOOL, 0, nullptr },

    // Vail Course hierarchy
    // Back from course picker goes to CW School landing (MODE_CWSCHOOL_TRAINING has no LVGL screen)
    { MODE_VAIL_COURSE_MODULE_SELECT, MODE_SCHOOL_HUB, 0, nullptr },
    { MODE_VAIL_COURSE_LESSON_SELECT, MODE_VAIL_COURSE_MODULE_SELECT, 0, nullptr },
    { MODE_VAIL_COURSE_LESSON, MODE_VAIL_COURSE_LESSON_SELECT, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, "VailCourse" },
    { MODE_VAIL_COURSE_PROGRESS, MODE_VAIL_COURSE_MODULE_SELECT, 0, nullptr },

    // Web-triggered modes (entered via web interface, ESC returns to CW menu)
    { MODE_WEB_PRACTICE, MODE_CW_MENU, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, nullptr },
    { MODE_WEB_MEMORY_CHAIN, MODE_CW_MENU, MODE_FLAG_AUDIO_CRITICAL | MODE_FLAG_NO_STATUS, nullptr },
    { MODE_WEB_HEAR_IT, MODE_CW_MENU, MODE_FLAG_NO_STATUS, nullptr },

    // Web files update (in WiFi submenu, triggers reboot)
    { MODE_WEB_FILES_UPDATE, MODE_WIFI_SUBMENU, 0, nullptr },
};

static const int modeInfoTableSize = sizeof(modeInfoTable) / sizeof(modeInfoTable[0]);

// Find the descriptor for a mode, or nullptr if it has no row.
static const ModeInfo* lookupModeInfo(int mode) {
    for (int i = 0; i < modeInfoTableSize; i++) {
        if (modeInfoTable[i].mode == mode) return &modeInfoTable[i];
    }
    return nullptr;
}

// ============================================
// Parent Mode Lookup
// ============================================

// Look up parent mode for back navigation.
// Returns MODE_MAIN_MENU if mode not found in table.
static int lookupParentMode(int mode) {
    const ModeInfo* mi = lookupModeInfo(mode);
    return mi ? mi->parent : MODE_MAIN_MENU;
}

// ============================================
// Mode Flag Lookup
// ============================================

// Look up flags for a mode (returns 0 if not in table)
static uint8_t lookupModeFlags(int mode) {
    const ModeInfo* mi = lookupModeInfo(mode);
    return mi ? mi->flags : 0;
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

// Look up training name for a mode (returns NULL if not a training mode)
static const char* lookupTrainingName(int mode) {
    const ModeInfo* mi = lookupModeInfo(mode);
    return mi ? mi->trainingName : nullptr;
}

// Check if mode is a training/practice mode
static bool isModeTraining(int mode) {
    return lookupTrainingName(mode) != nullptr;
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
