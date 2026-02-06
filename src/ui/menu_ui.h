/*
 * Menu UI Module
 * Provides drawHeader for legacy TFT screens,
 * and forward declarations used across the codebase.
 *
 * MenuMode enum is now in src/core/modes.h (single source of truth).
 */

#ifndef MENU_UI_H
#define MENU_UI_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "../core/config.h"
#include "../core/modes.h"

// Forward declarations from main file
extern LGFX tft;
extern int currentSelection;

// Forward declaration for CW Memories helper function (from radio_cw_memories.h)
bool shouldDrawCWMemoriesList();

// Forward declarations for status bar functions
void drawStatusIcons();

// Forward declarations for mode-specific UI functions
// These are needed by files included before the function definitions
void drawHearItTypeItUI(LGFX& tft);
void drawHearItConfigureUI(LGFX& tft);
void drawPracticeUI(LGFX& tft);
void drawCWATrackSelectUI(LGFX& tft);
void drawCWASessionSelectUI(LGFX& tft);
void drawCWAPracticeTypeSelectUI(LGFX& tft);
void drawCWAMessageTypeSelectUI(LGFX& tft);
void drawCWACopyPracticeUI(LGFX& tft);
void drawCWASendingPracticeUI(LGFX& tft);
void drawCWAQSOPracticeUI(LGFX& tft);
void drawMorseShooterUI(LGFX& tft);
void drawMemoryUI(LGFX& tft);
void drawWiFiUI(LGFX& tft);
void drawCWSettingsUI(LGFX& tft);
void drawVolumeDisplay(LGFX& tft);
void drawBrightnessDisplay(LGFX& tft);
void drawCallsignUI(LGFX& tft);
void drawWebPasswordUI(LGFX& tft);
void drawVailUI(LGFX& tft);
void drawToolsMenu(LGFX& tft);
void drawQSOLoggerMenu(LGFX& tft);
void drawQSOLogEntryUI(LGFX& tft);
void drawQSOViewLogsUI(LGFX& tft);
void drawQSOStatisticsUI(LGFX& tft);
void drawRadioOutputUI(LGFX& tft);
void drawCWMemoriesUI(LGFX& tft);
void drawWebPracticeUI(LGFX& tft);
void drawBTHIDUI(LGFX& tft);
void drawBTMIDIUI(LGFX& tft);
void drawBTKeyboardSettingsUI(LGFX& tft);
void drawLicenseQuizUI(LGFX& tft);
void drawLicenseStatsUI(LGFX& tft);

/*
 * Draw header bar with title and status icons
 * Still used by legacy TFT screens (morse shooter, vail repeater, CWA, QSO logger, etc.)
 */
void drawHeader() {
  // Clean minimal header - solid dark background with subtle border
  tft.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_BG_LAYER2);

  // Subtle bottom border
  tft.drawLine(0, HEADER_HEIGHT, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_BORDER_SUBTLE);

  // Draw title based on current mode using clean font
  tft.setFont(&FreeSansBold12pt7b);  // Smaller, cleaner font
  tft.setTextColor(COLOR_TEXT_PRIMARY);
  tft.setTextSize(1);
  String title = "VAIL SUMMIT";

  if (currentMode == MODE_TRAINING_MENU) {
    title = "TRAINING";
  } else if (currentMode == MODE_HEAR_IT_TYPE_IT) {
    title = "TRAINING";
  } else if (currentMode == MODE_PRACTICE) {
    title = "PRACTICE";
  } else if (currentMode == MODE_CW_ACADEMY_TRACK_SELECT) {
    title = "CW ACADEMY";
  } else if (currentMode == MODE_CW_ACADEMY_SESSION_SELECT) {
    title = "CW ACADEMY";
  } else if (currentMode == MODE_CW_ACADEMY_PRACTICE_TYPE_SELECT) {
    title = "CW ACADEMY";
  } else if (currentMode == MODE_CW_ACADEMY_MESSAGE_TYPE_SELECT) {
    title = "CW ACADEMY";
  } else if (currentMode == MODE_CW_ACADEMY_COPY_PRACTICE) {
    title = "CW ACADEMY";
  } else if (currentMode == MODE_CW_ACADEMY_SENDING_PRACTICE) {
    title = "CW ACADEMY";
  } else if (currentMode == MODE_CW_ACADEMY_QSO_PRACTICE) {
    title = "CW ACADEMY";
  } else if (currentMode == MODE_GAMES_MENU) {
    title = "GAMES";
  } else if (currentMode == MODE_MORSE_SHOOTER) {
    title = "MORSE SHOOTER";
  } else if (currentMode == MODE_RADIO_OUTPUT) {
    title = "RADIO OUTPUT";
  } else if (currentMode == MODE_CW_MEMORIES) {
    title = "CW MEMORIES";
  } else if (currentMode == MODE_SETTINGS_MENU) {
    title = "SETTINGS";
  } else if (currentMode == MODE_DEVICE_SETTINGS_MENU) {
    title = "DEVICE SETTINGS";
  } else if (currentMode == MODE_WIFI_SUBMENU) {
    title = "WIFI";
  } else if (currentMode == MODE_GENERAL_SUBMENU) {
    title = "GENERAL";
  } else if (currentMode == MODE_WIFI_SETTINGS) {
    title = "WIFI SETUP";
  } else if (currentMode == MODE_CW_SETTINGS) {
    title = "CW SETTINGS";
  } else if (currentMode == MODE_VOLUME_SETTINGS) {
    title = "VOLUME";
  } else if (currentMode == MODE_BRIGHTNESS_SETTINGS) {
    title = "BRIGHTNESS";
  } else if (currentMode == MODE_CALLSIGN_SETTINGS) {
    title = "CALLSIGN";
  } else if (currentMode == MODE_WEB_PASSWORD_SETTINGS) {
    title = "WEB PASSWORD";
  } else if (currentMode == MODE_VAIL_REPEATER) {
    title = "VAIL CHAT";
  } else if (currentMode == MODE_BLUETOOTH_MENU) {
    title = "BLUETOOTH";
  } else if (currentMode == MODE_BT_HID) {
    title = "BT HID";
  } else if (currentMode == MODE_BT_MIDI) {
    title = "BT MIDI";
  } else if (currentMode == MODE_DEVICE_BT_SUBMENU) {
    title = "BLUETOOTH";
  } else if (currentMode == MODE_BT_KEYBOARD_SETTINGS) {
    title = "BT KEYBOARD";
  } else if (currentMode == MODE_CW_MENU) {
    title = "CW";
  } else if (currentMode == MODE_HAM_TOOLS_MENU) {
    title = "HAM TOOLS";
  } else if (currentMode == MODE_BAND_PLANS) {
    title = "BAND PLANS";
  } else if (currentMode == MODE_PROPAGATION) {
    title = "PROPAGATION";
  } else if (currentMode == MODE_ANTENNAS) {
    title = "ANTENNAS";
  } else if (currentMode == MODE_LICENSE_SELECT) {
    title = "LICENSE STUDY";
  } else if (currentMode == MODE_LICENSE_QUIZ) {
    title = "LICENSE STUDY";
  } else if (currentMode == MODE_LICENSE_STATS) {
    title = "LICENSE STUDY";
  } else if (currentMode == MODE_SUMMIT_CHAT) {
    title = "SUMMIT CHAT";
  } else if (currentMode == MODE_QSO_LOGGER_MENU) {
    title = "QSO LOGGER";
  } else if (currentMode == MODE_QSO_LOG_ENTRY) {
    title = "NEW LOG";
  } else if (currentMode == MODE_QSO_VIEW_LOGS) {
    title = "VIEW LOGS";
  } else if (currentMode == MODE_QSO_STATISTICS) {
    title = "STATISTICS";
  } else if (currentMode == MODE_QSO_LOGGER_SETTINGS) {
    title = "LOGGER SETTINGS";
  }

  tft.setCursor(15, 15); // Left-justified, vertically centered in 45px header
  tft.print(title);
  tft.setFont(nullptr); // Reset to default font for status icons

  // Draw status icons
  drawStatusIcons();
}

/*
 * No-op stub for legacy drawMenu() calls
 * LVGL handles all screen rendering now.
 */
void drawMenu() {
  // LVGL handles all modes - legacy TFT rendering disabled
}

#endif // MENU_UI_H
