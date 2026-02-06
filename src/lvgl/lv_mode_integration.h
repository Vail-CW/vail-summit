/*
 * VAIL SUMMIT - LVGL Mode Integration
 * Connects LVGL screens to the existing mode state machine
 *
 * This module provides the bridge between:
 * - The MenuMode enum (defined in src/core/modes.h)
 * - The LVGL-based screen rendering
 * - Input handling delegation
 */

#ifndef LV_MODE_INTEGRATION_H
#define LV_MODE_INTEGRATION_H

#include <lvgl.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "../core/modes.h"
#include "../core/mode_registry.h"
#include "lv_screen_manager.h"
#include "lv_menu_screens.h"
#include "lv_settings_screens.h"
#include "lv_training_screens.h"
#include "lv_game_screens.h"
#include "lv_mode_screens.h"
#include "lv_band_conditions.h"
#include "lv_band_plans.h"
#include "lv_pota_screens.h"
#include "lv_story_time_screens.h"
#include "lv_web_download_screen.h"
#include "lv_mailbox_screens.h"
#include "lv_morse_notes_screens.h"
#include "lv_cwschool_screens.h"
#include "lv_vail_course_screens.h"
#include "../core/config.h"
#include "../settings/settings_practice_time.h"
#include "../network/progress_sync.h"
#include "../core/hardware_init.h"
#include "../storage/sd_card.h"

// ============================================
// Forward declarations from main file
// ============================================

extern int currentSelection;
extern LGFX tft;

// Note: We access currentMode via extern int
// The actual type is MenuMode but we cast it

// Forward declarations for mode start functions (kept for modes with initialization logic)
extern void startPracticeMode(LGFX& tft);
extern void startVailRepeater(LGFX& tft);
extern void startCWAcademy(LGFX& tft);
extern void startMorseShooter(LGFX& tft);
extern void loadShooterPrefs();  // Load shooter settings before showing settings screen
extern void memoryChainStart();  // New Memory Chain implementation
extern void startSparkWatch();
extern void storyTimeStart();
extern void startRadioOutput(LGFX& tft);
extern void startCWMemoriesMode(LGFX& tft);
extern void startBTHID(LGFX& tft);
extern void startBTMIDI(LGFX& tft);
extern void startWiFiSettings(LGFX& tft);
extern void startCWSettings(LGFX& tft);
extern void initVolumeSettings(LGFX& tft);
extern void startCallsignSettings(LGFX& tft);
extern void startWebPasswordSettings(LGFX& tft);
extern void initBrightnessSettings(LGFX& tft);
extern void startBTKeyboardSettings(LGFX& tft);
extern void startViewLogs(LGFX& tft);
extern void startStatistics(LGFX& tft);
extern void startLoggerSettings(LGFX& tft);
extern void startCWACopyPractice(LGFX& tft);
extern void startCWASendingPractice(LGFX& tft);
extern void startCWAQSOPractice(LGFX& tft);
// CW Academy LVGL init functions (defined in lv_training_screens.h)
extern void initCWACopyPractice();
extern void initCWASendingPractice();
extern void initCWAQSOPractice();
extern void startWebPracticeMode(LGFX& tft);
extern void startWebMemoryChainMode(LGFX& tft, int difficulty, int mode, int wpm, bool sound, bool hints);
extern void startWebHearItMode(LGFX& tft);
extern void startHearItTypeItMode(LGFX& tft);
extern void startLicenseQuiz(LGFX& tft, int licenseType);
extern void startLicenseStats(LGFX& tft);
extern void updateLicenseQuizDisplay();
extern void startVailMaster(LGFX& tft);
// CW Speeder game functions (defined in game_cw_speeder.h)
extern void cwSpeedSelectStart();
extern void cwSpeedGameStart();
// CW Academy state reset functions (defined in training_cwa_core.h)
extern void resetCWACopyPracticeState();
extern void resetCWASendingPracticeState();

// Forward declaration for license session
extern struct LicenseStudySession licenseSession;

// ============================================
// LVGL Configuration
// ============================================

// LVGL is the only UI system - no legacy rendering available

// ============================================
// Global Hotkey State
// ============================================

// Track if volume was accessed via global "V" shortcut
// When true, ESC returns to returnToModeAfterVolume instead of normal parent
static bool volumeViaShortcut = false;
static int returnToModeAfterVolume = MODE_MAIN_MENU;

// ============================================
// Mode Category Detection
// ============================================

/*
 * Check if a mode is a menu mode (not an active feature)
 */
bool isMenuModeInt(int mode) {
    return isModeMenu(mode);
}

/*
 * Check if a mode is a pure navigation menu (no text input)
 * Used for global hotkeys like V for volume - only allow from these screens
 * to avoid intercepting key input in training/input modes
 */
bool isPureNavigationMenuInt(int mode) {
    return isModePureNav(mode);
}

/*
 * Check if a mode is a settings mode
 */
bool isSettingsModeInt(int mode) {
    return isModeSettings(mode);
}

/*
 * Get the training mode name for practice time tracking
 * Returns the name as a const char* (NULL if not a training mode)
 */
const char* getTrainingModeNameStr(int mode) {
    return lookupTrainingName(mode);
}

/*
 * Check if a mode is a training/practice mode
 */
bool isTrainingModeInt(int mode) {
    return isModeTraining(mode);
}

// ============================================
// Mode-to-Screen Mapping
// ============================================

/*
 * Create the appropriate LVGL screen for a given mode
 * LVGL handles ALL modes - no legacy rendering
 */
lv_obj_t* createScreenForModeInt(int mode) {
    // Menu screens
    if (isMenuModeInt(mode)) {
        switch (mode) {
            case MODE_MAIN_MENU:
                return createMainMenuScreen();
            case MODE_CW_MENU:
                return createCWMenuScreen();
            case MODE_TRAINING_MENU:
                return createTrainingMenuScreen();
            case MODE_GAMES_MENU:
                return createGamesMenuScreen();
            case MODE_SETTINGS_MENU:
                return createSettingsMenuScreen();
            case MODE_DEVICE_SETTINGS_MENU:
                return createDeviceSettingsMenuScreen();
            case MODE_WIFI_SUBMENU:
                return createWiFiSubmenuScreen();
            case MODE_GENERAL_SUBMENU:
                return createGeneralSubmenuScreen();
            case MODE_HAM_TOOLS_MENU:
                return createHamToolsMenuScreen();
            case MODE_BLUETOOTH_MENU:
                return createBluetoothMenuScreen();
            case MODE_QSO_LOGGER_MENU:
                return createQSOLoggerMenuScreen();
            default:
                break;
        }
    }

    // Settings screens
    if (isSettingsModeInt(mode)) {
        return createSettingsScreenForMode(mode);
    }

    // Training screens - delegate to training screen selector
    lv_obj_t* trainingScreen = createTrainingScreenForMode(mode);
    if (trainingScreen != NULL) return trainingScreen;

    // Game screens - delegate to game screen selector
    lv_obj_t* gameScreen = createGameScreenForMode(mode);
    if (gameScreen != NULL) return gameScreen;

    // Mode screens (network, radio, etc) - delegate to mode screen selector
    lv_obj_t* modeScreen = createModeScreenForMode(mode);
    if (modeScreen != NULL) return modeScreen;

    // POTA screens
    lv_obj_t* potaScreen = createPOTAScreenForMode(mode);
    if (potaScreen != NULL) return potaScreen;

    // Story Time screens
    lv_obj_t* storyTimeScreen = createStoryTimeScreenForMode(mode);
    if (storyTimeScreen != NULL) return storyTimeScreen;

    // Morse Mailbox screens
    if (mode >= MODE_MORSE_MAILBOX && mode <= MODE_MORSE_MAILBOX_ACCOUNT) {
        lv_obj_t* mailboxScreen = NULL;
        if (handleMailboxMode(mode)) {
            // handleMailboxMode calls loadScreen internally, return NULL to avoid double loading
            return NULL;
        }
    }

    // CW School screens
    if (mode >= MODE_CWSCHOOL && mode <= MODE_CWSCHOOL_PROGRESS) {
        if (handleCWSchoolMode(mode)) {
            // handleCWSchoolMode calls loadScreen internally, return NULL to avoid double loading
            return NULL;
        }
    }

    // Morse Notes screens
    if (mode >= MODE_MORSE_NOTES_LIBRARY && mode <= MODE_MORSE_NOTES_SETTINGS) {
        switch (mode) {
            case MODE_MORSE_NOTES_LIBRARY:
                return createMorseNotesLibraryScreen();
            case MODE_MORSE_NOTES_RECORD:
                return createMorseNotesRecordScreen();
            case MODE_MORSE_NOTES_PLAYBACK:
                return createMorseNotesPlaybackScreen();
            case MODE_MORSE_NOTES_SETTINGS:
                return createMorseNotesSettingsScreen();
        }
    }

    // Vail Course screens
    if (mode >= MODE_VAIL_COURSE_MODULE_SELECT && mode <= MODE_VAIL_COURSE_PROGRESS) {
        if (handleVailCourseMode(mode)) {
            // handleVailCourseMode calls loadScreen internally, return NULL to avoid double loading
            return NULL;
        }
    }

    // Placeholder screens for unimplemented features
    switch (mode) {
        case MODE_BAND_PLANS:
            return createBandPlansScreen();
        case MODE_PROPAGATION:
            return createBandConditionsScreen();
        case MODE_ANTENNAS:
            return createComingSoonScreen("ANTENNAS");
        case MODE_SUMMIT_CHAT:
            return createComingSoonScreen("SUMMIT CHAT");
        default:
            break;
    }

    // If we get here, create a placeholder screen with mode number
    Serial.printf("[ModeIntegration] No LVGL screen for mode %d, creating placeholder\n", mode);
    char placeholder_title[32];
    snprintf(placeholder_title, sizeof(placeholder_title), "MODE %d", mode);
    return createComingSoonScreen(placeholder_title);
}

// ============================================
// Menu Selection Handler
// ============================================

// External currentMode (declared in menu_navigation.h as MenuMode)
// We access it through a getter/setter approach

// Getter/setter functions to access currentMode
// These will be defined after menu_ui.h is included
extern int getCurrentModeAsInt();
extern void setCurrentModeFromInt(int mode);

// Track previous mode for practice session management
static int previousModeForPractice = MODE_MAIN_MENU;

/*
 * Initialize mode-specific state after screen is loaded
 * This calls the appropriate start function for modes that need initialization
 * (decoders, audio callbacks, game state, etc.)
 */
void initializeModeInt(int mode) {
    // Practice time tracking: end previous session if leaving training mode
    const char* prevName = getTrainingModeNameStr(previousModeForPractice);
    const char* newName = getTrainingModeNameStr(mode);
    bool sameTraining = (prevName && newName && strcmp(prevName, newName) == 0);

    if (prevName && !sameTraining) {
        // Leaving a training mode (or switching to different training mode)
        unsigned long sessionDuration = endPracticeSession();
        Serial.printf("[Practice] Ended %s session: %lu sec\n", prevName, sessionDuration);

        // Sync session to cloud (if linked and significant duration)
        if (sessionDuration >= 30) {
            syncSession(sessionDuration, prevName);
        }
    }

    if (newName && !sameTraining) {
        // Entering a training mode
        startPracticeSession(newName);
        Serial.printf("[Practice] Started %s session\n", newName);
    }

    previousModeForPractice = mode;

    switch (mode) {
        // Training modes
        case MODE_PRACTICE:
            Serial.println("[ModeInit] Starting Practice mode");
            startPracticeMode(tft);
            break;
        case MODE_CW_ACADEMY_TRACK_SELECT:
            Serial.println("[ModeInit] Starting CW Academy Track Select");
            loadCWAProgress();  // Load saved progress
            break;
        case MODE_CW_ACADEMY_SESSION_SELECT:
            Serial.println("[ModeInit] CW Academy Session Select");
            // Session select screen handles its own init
            break;
        case MODE_CW_ACADEMY_PRACTICE_TYPE_SELECT:
            Serial.println("[ModeInit] CW Academy Practice Type Select");
            // Practice type select screen handles its own init
            break;
        case MODE_CW_ACADEMY_MESSAGE_TYPE_SELECT:
            Serial.println("[ModeInit] CW Academy Message Type Select");
            // Message type select screen handles its own init
            break;
        case MODE_CW_ACADEMY_COPY_PRACTICE:
            Serial.println("[ModeInit] Starting CW Academy Copy Practice (LVGL)");
            initCWACopyPractice();  // LVGL version initializes in screen creation
            break;
        case MODE_CW_ACADEMY_SENDING_PRACTICE:
            Serial.println("[ModeInit] Starting CW Academy Sending Practice (LVGL)");
            initCWASendingPractice();  // LVGL version with dual-core audio
            break;
        case MODE_CW_ACADEMY_QSO_PRACTICE:
            Serial.println("[ModeInit] Starting CW Academy QSO Practice (LVGL)");
            initCWAQSOPractice();  // LVGL version
            break;
        case MODE_HEAR_IT_TYPE_IT:
        case MODE_HEAR_IT_MENU:
            Serial.println("[ModeInit] Starting Hear It Type It");
            startHearItTypeItMode(tft);
            break;
        case MODE_VAIL_MASTER:
            Serial.println("[ModeInit] Starting Vail Master");
            startVailMaster(tft);
            break;

        // LICW Training modes
        case MODE_LICW_CAROUSEL_SELECT:
            Serial.println("[ModeInit] Starting LICW Carousel Select");
            initLICWTraining();  // Load saved progress
            break;
        case MODE_LICW_COPY_PRACTICE:
            Serial.println("[ModeInit] Starting LICW Copy Practice");
            // Session reset handled in screen creation
            break;

        // Game modes
        case MODE_MORSE_SHOOTER:
            // Just load preferences, game starts when user presses START on settings screen
            Serial.println("[ModeInit] Loading Morse Shooter settings");
            loadShooterPrefs();
            break;
        case MODE_MORSE_MEMORY:
            Serial.println("[ModeInit] Starting Memory Chain");
            memoryChainStart();
            break;
        case MODE_SPARK_WATCH:
            Serial.println("[ModeInit] Starting Spark Watch");
            startSparkWatch();
            break;
        case MODE_STORY_TIME:
            Serial.println("[ModeInit] Starting Story Time");
            storyTimeStart();
            break;

        // CW Speeder game
        case MODE_CW_SPEEDER_SELECT:
            Serial.println("[ModeInit] Starting CW Speeder - Word Select");
            cwSpeedSelectStart();
            break;
        case MODE_CW_SPEEDER:
            Serial.println("[ModeInit] Starting CW Speeder - Game");
            cwSpeedGameStart();
            break;

        // Morse Mailbox modes
        case MODE_MORSE_MAILBOX:
        case MODE_MORSE_MAILBOX_LINK:
        case MODE_MORSE_MAILBOX_INBOX:
        case MODE_MORSE_MAILBOX_PLAYBACK:
        case MODE_MORSE_MAILBOX_COMPOSE:
        case MODE_MORSE_MAILBOX_ACCOUNT:
            Serial.printf("[ModeInit] Starting Morse Mailbox mode %d\n", mode);
            // Screen creation handled by handleMailboxMode()
            break;

        // Morse Notes modes
        case MODE_MORSE_NOTES_LIBRARY:
        case MODE_MORSE_NOTES_RECORD:
        case MODE_MORSE_NOTES_PLAYBACK:
        case MODE_MORSE_NOTES_SETTINGS:
            Serial.printf("[ModeInit] Starting Morse Notes mode %d\n", mode);
            // Screen creation handled in onLVGLMenuSelect()
            break;

        // Network/radio modes
        case MODE_VAIL_REPEATER:
            Serial.println("[ModeInit] Starting Vail Repeater");
            startVailRepeater(tft);
            // Auto-connect to General room only if callsign is set
            // (checkVailCallsignRequired is checked in createVailRepeaterScreen)
            if (vailCallsign.length() > 0 && vailCallsign != "GUEST") {
                connectToVail("General");
            }
            break;
        case MODE_RADIO_OUTPUT:
            Serial.println("[ModeInit] Starting Radio Output");
            startRadioOutput(tft);
            break;
        case MODE_CW_MEMORIES:
            Serial.println("[ModeInit] Starting CW Memories");
            startCWMemoriesMode(tft);
            break;
        case MODE_PROPAGATION:
            Serial.println("[ModeInit] Starting Band Conditions");
            startBandConditions(tft);
            break;

        // POTA modes
        case MODE_POTA_ACTIVE_SPOTS:
            Serial.println("[ModeInit] Starting POTA Active Spots");
            startPOTAActiveSpots(tft);
            break;

        // Bluetooth modes
        case MODE_BT_HID:
            Serial.println("[ModeInit] Starting BT HID");
            startBTHID(tft);
            break;
        case MODE_BT_MIDI:
            Serial.println("[ModeInit] Starting BT MIDI");
            startBTMIDI(tft);
            break;
        case MODE_BT_KEYBOARD_SETTINGS:
            Serial.println("[ModeInit] Starting BT Keyboard Settings");
            startBTKeyboardSettings(tft);
            break;

        // Settings modes
        case MODE_WIFI_SETTINGS:
            Serial.println("[ModeInit] Starting WiFi Settings (LVGL)");
            startWiFiSetupLVGL();  // Initialize WiFi setup state
            break;
        case MODE_CW_SETTINGS:
            Serial.println("[ModeInit] Starting CW Settings");
            startCWSettings(tft);
            break;
        case MODE_VOLUME_SETTINGS:
            Serial.println("[ModeInit] Starting Volume Settings");
            initVolumeSettings(tft);
            break;
        case MODE_BRIGHTNESS_SETTINGS:
            Serial.println("[ModeInit] Starting Brightness Settings");
            initBrightnessSettings(tft);
            break;
        case MODE_CALLSIGN_SETTINGS:
            Serial.println("[ModeInit] Starting Callsign Settings");
            startCallsignSettings(tft);
            break;
        case MODE_WEB_PASSWORD_SETTINGS:
            Serial.println("[ModeInit] Starting Web Password Settings");
            startWebPasswordSettings(tft);
            break;

        // QSO Logger modes - now handled by LVGL screens in lv_mode_screens.h
        // Screen creation handles data loading, no legacy init needed
        case MODE_QSO_VIEW_LOGS:
            Serial.println("[ModeInit] View Logs - LVGL screen handles init");
            // loadQSOsForView() is called in createQSOViewLogsScreen()
            break;
        case MODE_QSO_STATISTICS:
            Serial.println("[ModeInit] Statistics - LVGL screen handles init");
            // calculateStatistics() is called in createQSOStatisticsScreen()
            break;
        case MODE_QSO_LOGGER_SETTINGS:
            Serial.println("[ModeInit] Logger Settings - LVGL screen handles init");
            // loadLoggerLocation() is called in createQSOLoggerSettingsScreen()
            break;

        // Web modes
        case MODE_WEB_PRACTICE:
            Serial.println("[ModeInit] Starting Web Practice Mode");
            startWebPracticeMode(tft);
            break;
        case MODE_WEB_HEAR_IT:
            Serial.println("[ModeInit] Starting Web Hear It Mode");
            startWebHearItMode(tft);
            break;

        // License study modes
        case MODE_LICENSE_SELECT:
            Serial.println("[ModeInit] Starting License Select");
            // Focus first license card for keyboard navigation
            if (license_select_cards[0]) {
                lv_group_focus_obj(license_select_cards[0]);
            }
            break;
        case MODE_LICENSE_QUIZ:
            Serial.println("[ModeInit] Starting License Quiz");
            // NOTE: File existence is checked in license_type_select_handler before navigating here
            // If we reach this point, files should already exist on SD card

            // Load questions and start session
            startLicenseQuizLVGL(licenseSession.selectedLicense);
            // Update the LVGL display after loading questions
            updateLicenseQuizDisplay();
            // Focus first answer button for keyboard navigation
            if (license_answer_btns[0]) {
                lv_group_focus_obj(license_answer_btns[0]);
            }
            break;
        case MODE_LICENSE_STATS:
            Serial.println("[ModeInit] Starting License Stats");
            // Use LVGL version if available, otherwise call legacy
            startLicenseQuizLVGL(licenseSession.selectedLicense);  // Ensure pool is loaded
            break;
        case MODE_LICENSE_DOWNLOAD:
            Serial.println("[ModeInit] Starting License Download");
            // Perform downloads and show progress
            if (performLicenseDownloadsLVGL()) {
                // Downloads succeeded - transition to quiz
                Serial.println("[ModeInit] Downloads complete, transitioning to quiz");
                clearNavigationGroup();
                lv_obj_t* quiz_screen = createLicenseQuizScreen();
                loadScreen(quiz_screen, SCREEN_ANIM_FADE);
                setCurrentModeFromInt(MODE_LICENSE_QUIZ);
                startLicenseQuizLVGL(licenseSession.selectedLicense);
                updateLicenseQuizDisplay();
            } else {
                // Downloads failed - add focus widget for ESC navigation
                Serial.println("[ModeInit] Downloads failed, user can press ESC to go back");
            }
            break;
        case MODE_LICENSE_WIFI_ERROR:
        case MODE_LICENSE_SD_ERROR:
            // Error screens just show message - ESC handled by focus container
            break;

        // Menu modes and others - no initialization needed
        default:
            if (!isMenuModeInt(mode)) {
                Serial.printf("[ModeInit] No init function for mode %d\n", mode);
            }
            break;
    }
}

// ============================================
// Global Hotkey Handler
// ============================================

/*
 * Handle global hotkeys before LVGL processing
 * Returns true if key was handled (should not pass to LVGL)
 *
 * Currently supported hotkeys:
 *   V/v - Volume Settings (only from pure navigation menus, not input screens)
 */
bool handleGlobalHotkey(char key) {
    int currentModeInt = getCurrentModeAsInt();

    // V key = Volume shortcut (only from pure navigation menus)
    // Do NOT intercept V in modes that accept text input (training, games, etc.)
    if ((key == 'V' || key == 'v') && isPureNavigationMenuInt(currentModeInt)) {
        Serial.printf("[Hotkey] V pressed in navigation menu mode %d, opening Volume Settings\n", currentModeInt);

        // Store current mode to return to after ESC
        volumeViaShortcut = true;
        returnToModeAfterVolume = currentModeInt;

        // Navigate to volume settings (plays beep, creates screen)
        onLVGLMenuSelect(MODE_VOLUME_SETTINGS);
        return true;
    }

    return false;
}

// ============================================
// Menu Selection Handler
// ============================================

/*
 * Handler for menu item selection from LVGL menus
 * Called when user selects a menu item
 * All modes are handled by LVGL - no legacy fallback
 */
void onLVGLMenuSelect(int target_mode) {
    Serial.printf("[ModeIntegration] Menu selected mode: %d\n", target_mode);

    // Web Files update mode - uses reboot-based download (SSL requires early boot RAM)
    if (target_mode == MODE_WEB_FILES_UPDATE) {
        // Play selection beep
        beep(TONE_SELECT, BEEP_MEDIUM);

        // Check requirements: WiFi and SD card
        if (!WiFi.isConnected()) {
            beep(400, 200);  // Error beep
            Serial.println("[ModeIntegration] Web Files update requires WiFi");
            createAlertDialog("WiFi Required",
                "Please connect to WiFi\nfirst to download web files.");
            return;
        }
        if (!sdCardAvailable) {
            initSDCard();
        }
        if (!sdCardAvailable) {
            beep(400, 200);  // Error beep
            Serial.println("[ModeIntegration] Web Files update requires SD card");
            createAlertDialog("SD Card Required",
                "Please insert an SD card\nto store web files.");
            return;
        }

        // Note: Version checking requires SSL which fails due to RAM constraints
        // when LVGL is running. We skip version check and just offer to download.
        // The download itself happens at early boot via reboot.

        // Check if web files exist to determine prompt text
        if (webFilesExist()) {
            String localVersion = getWebFilesVersion();
            Serial.printf("[ModeIntegration] Web files exist (v%s), offering update\n", localVersion.c_str());
            webFilesUpdateAvailable = true;
        } else {
            Serial.println("[ModeIntegration] No web files found, offering download");
            webFilesUpdateAvailable = false;
        }

        // Show the download screen (for update or fresh install)
        // User will press Y to trigger reboot-based download
        showWebFilesDownloadScreen();
        return;
    }

    // Check SD card requirement for QSO log entry
    if (target_mode == MODE_QSO_LOG_ENTRY) {
        // Try to initialize SD card if not already done
        if (!sdCardAvailable) {
            initSDCard();
        }
        // If still not available, show error and don't navigate
        if (!sdCardAvailable) {
            beep(400, 200);  // Error beep
            Serial.println("[ModeIntegration] QSO Logger requires SD card");

            static const char* btns[] = {"OK", ""};
            lv_obj_t* msgbox = lv_msgbox_create(NULL, "SD Card Required",
                "Please insert an SD card\nto use the QSO Logger.", btns, false);
            lv_obj_center(msgbox);
            lv_obj_add_style(msgbox, getStyleMsgbox(), 0);

            // Add button to navigation group for keyboard control
            lv_obj_t* btns_obj = lv_msgbox_get_btns(msgbox);
            addNavigableWidget(btns_obj);

            // Close on button click
            lv_obj_add_event_cb(msgbox, [](lv_event_t* e) {
                lv_obj_t* obj = lv_event_get_current_target(e);
                lv_msgbox_close(obj);
            }, LV_EVENT_VALUE_CHANGED, NULL);

            return;
        }
    }

    // Play selection beep
    beep(TONE_SELECT, BEEP_MEDIUM);

    // Update selection
    currentSelection = 0;

    // Clear navigation group before creating new screen's widgets
    clearNavigationGroup();

    // Create and load LVGL screen for the target mode
    lv_obj_t* screen = createScreenForModeInt(target_mode);

    // Update mode and load screen
    setCurrentModeFromInt(target_mode);
    if (screen != NULL) {
        loadScreen(screen, SCREEN_ANIM_SLIDE_LEFT);

        // Initialize mode-specific state (decoders, audio callbacks, game state)
        initializeModeInt(target_mode);

        // Debug: verify navigation group has widgets
        lv_group_t* group = getLVGLInputGroup();
        Serial.printf("[ModeIntegration] Screen loaded, nav group has %d objects\n",
                      group ? lv_group_get_obj_count(group) : -1);
    } else {
        Serial.printf("[ModeIntegration] WARNING: No screen for mode %d\n", target_mode);
    }
}

// ============================================
// Back Navigation Handler
// ============================================

/*
 * Get the parent mode for a given mode (for back navigation)
 * Uses centralized parent table from mode_registry.h
 */
int getParentModeInt(int mode) {
    // Special case: Volume accessed via global "V" shortcut
    // Returns to the menu the user was on, not the normal parent
    if (mode == MODE_VOLUME_SETTINGS && volumeViaShortcut) {
        int returnMode = returnToModeAfterVolume;
        volumeViaShortcut = false;  // Reset flag for next time
        Serial.printf("[ModeIntegration] Returning from Volume shortcut to mode %d\n", returnMode);
        return returnMode;
    }

    return lookupParentMode(mode);
}

// ============================================
// Cleanup Dispatch Table
// ============================================
// Maps modes to their cleanup callbacks, called on back navigation.
// Centralizes cleanup that was previously scattered as if-chains.

static const ModeCallbackEntry cleanupTable[] = {
    { MODE_PROPAGATION,                  cleanupBandConditions },
    { MODE_WIFI_SETTINGS,                cleanupWiFiScreen },
    { MODE_BT_HID,                       cleanupBTHIDScreen },
    { MODE_HEAR_IT_TYPE_IT,              cleanupHearItTypeItScreen },
    { MODE_HEAR_IT_MENU,                 cleanupHearItTypeItScreen },
    { MODE_POTA_ACTIVE_SPOTS,            cleanupPOTAScreen },
    { MODE_POTA_SPOT_DETAIL,             cleanupPOTAScreen },
    { MODE_POTA_FILTERS,                 cleanupPOTAScreen },
    { MODE_VAIL_REPEATER,                disconnectFromVail },
    { MODE_CW_ACADEMY_COPY_PRACTICE,     resetCWACopyPracticeState },
    { MODE_CW_ACADEMY_SENDING_PRACTICE,  resetCWASendingPracticeState },
    { MODE_MORSE_NOTES_RECORD,           cleanupMorseNotesRecordScreen },
    { MODE_MORSE_NOTES_PLAYBACK,         cleanupMorseNotesPlaybackScreen },
    { MODE_VAIL_MASTER_CHARSET,          cleanupVailMasterCharset },
};
static const int cleanupTableSize = sizeof(cleanupTable) / sizeof(cleanupTable[0]);

/*
 * Handle back navigation from LVGL screens
 * All navigation is handled by LVGL - no legacy fallback
 */
void onLVGLBackNavigation() {
    int currentModeInt = getCurrentModeAsInt();
    Serial.printf("[ModeIntegration] Back navigation from mode: %d\n", currentModeInt);

    // Play navigation beep
    beep(TONE_MENU_NAV, BEEP_SHORT);

    // Mode-specific cleanup before leaving (dispatch from table)
    dispatchModeCallback(cleanupTable, cleanupTableSize, currentModeInt);

    // Get parent mode
    int parentMode = getParentModeInt(currentModeInt);

    if (parentMode == currentModeInt) {
        // Already at top level, ignore or handle deep sleep triple-ESC
        return;
    }

    // Update mode and selection
    setCurrentModeFromInt(parentMode);
    currentSelection = 0;

    // Clear navigation group before creating new screen's widgets
    clearNavigationGroup();

    // Create and load parent screen
    lv_obj_t* screen = createScreenForModeInt(parentMode);
    if (screen != NULL) {
        loadScreen(screen, SCREEN_ANIM_SLIDE_RIGHT);

        // Debug: verify navigation group has widgets
        lv_group_t* group = getLVGLInputGroup();
        Serial.printf("[ModeIntegration] Parent screen loaded, nav group has %d objects\n",
                      group ? lv_group_get_obj_count(group) : -1);
    } else {
        Serial.printf("[ModeIntegration] WARNING: No parent screen for mode %d\n", parentMode);
    }
}

// ============================================
// Initialization
// ============================================

/*
 * Initialize LVGL mode integration
 * Call this after LVGL and theme are initialized
 */
void initLVGLModeIntegration() {
    Serial.println("[ModeIntegration] Initializing LVGL mode integration");

    // Set up menu selection callback
    setMenuSelectCallback(onLVGLMenuSelect);

    // Set up back navigation callback
    setBackCallback(onLVGLBackNavigation);

    Serial.println("[ModeIntegration] Mode integration initialized");
}

/*
 * Show the initial LVGL screen (main menu)
 */
void showInitialLVGLScreen() {
    Serial.println("[ModeIntegration] Loading initial LVGL screen (main menu)");

    // Clear any widgets from splash screen before creating menu
    clearNavigationGroup();

    lv_obj_t* main_menu = createMainMenuScreen();
    if (main_menu != NULL) {
        loadScreen(main_menu, SCREEN_ANIM_NONE);
        setCurrentModeFromInt(MODE_MAIN_MENU);
        currentSelection = 0;

        // Debug: verify navigation group has widgets
        lv_group_t* group = getLVGLInputGroup();
        Serial.printf("[ModeIntegration] Main menu loaded, nav group has %d objects\n",
                      group ? lv_group_get_obj_count(group) : -1);
    } else {
        Serial.println("[ModeIntegration] CRITICAL: Failed to create main menu screen!");
    }
}

/*
 * Check if LVGL mode is enabled
 * Note: LVGL is now the only UI system - always returns true
 */
bool isLVGLModeEnabled() {
    return true;  // LVGL is always enabled - no legacy UI
}

// ============================================
// Dynamic Screen Updates
// ============================================

/*
 * Refresh the current LVGL screen based on mode
 * Call this when mode state changes need to be reflected in UI
 */
void refreshCurrentLVGLScreen() {
    int currentModeInt = getCurrentModeAsInt();
    lv_obj_t* screen = createScreenForModeInt(currentModeInt);
    if (screen != NULL) {
        loadScreen(screen, SCREEN_ANIM_NONE);
    }
}

/*
 * Update specific UI elements without full screen reload
 * Used for real-time updates in training/game modes
 */
void updateLVGLModeUI() {
    // The individual screen modules provide update functions
    // This function can be extended to call those based on mode
    // For now, specific updates are called directly from the mode handlers
}

#endif // LV_MODE_INTEGRATION_H
