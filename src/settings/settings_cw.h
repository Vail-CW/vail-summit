/*
 * CW Settings Module
 * Handles morse code speed, tone, and key type settings
 */

#ifndef SETTINGS_CW_H
#define SETTINGS_CW_H

#include <Preferences.h>
#include "../core/config.h"

// Key types
enum KeyType {
  KEY_STRAIGHT = 0,
  KEY_IAMBIC_A = 1,
  KEY_IAMBIC_B = 2,
  KEY_ULTIMATIC = 3
};

// CW settings state
enum CWSettingsState {
  CW_SETTING_SPEED,
  CW_SETTING_TONE,
  CW_SETTING_KEY_TYPE
};

// CW settings globals
int cwSpeed = DEFAULT_WPM;         // WPM
int cwTone = TONE_SIDETONE;        // Hz
KeyType cwKeyType = KEY_IAMBIC_B;  // Default to Iambic B
Preferences cwPrefs;

// Setting selection
#define CW_SETTINGS_COUNT 3

// LVGL mode flag - when true, skip legacy draw functions (LVGL handles display)

void saveCWSettings();
void loadCWSettings();

// Load CW settings from flash
void loadCWSettings() {
  if (!cwPrefs.begin("cw", true)) {
    // Namespace missing (fresh device) or NVS unhealthy - keep defaults
    Serial.println("[CW] WARNING: could not open 'cw' namespace read-only, using defaults");
  }
  cwSpeed = cwPrefs.getInt("speed", DEFAULT_WPM);
  cwTone = cwPrefs.getInt("tone", TONE_SIDETONE);
  cwKeyType = (KeyType)cwPrefs.getInt("keytype", KEY_IAMBIC_B);
  cwPrefs.end();

  // Validate settings
  if (cwSpeed < WPM_MIN) cwSpeed = WPM_MIN;
  if (cwSpeed > WPM_MAX) cwSpeed = WPM_MAX;
  if (cwTone < 400) cwTone = 400;
  if (cwTone > 1200) cwTone = 1200;

  Serial.print("CW Settings loaded: ");
  Serial.print(cwSpeed);
  Serial.print(" WPM, ");
  Serial.print(cwTone);
  Serial.print(" Hz, Key type: ");
  Serial.println(cwKeyType);
}

// Save CW settings to flash
void saveCWSettings() {
  if (!cwPrefs.begin("cw", false)) {
    Serial.println("[CW] ERROR: cannot open 'cw' namespace for write - settings NOT saved");
    return;
  }
  // putInt returns bytes written (0 = failure, e.g. NVS partition full)
  bool ok = true;
  ok &= cwPrefs.putInt("speed", cwSpeed) > 0;
  ok &= cwPrefs.putInt("tone", cwTone) > 0;
  ok &= cwPrefs.putInt("keytype", (int)cwKeyType) > 0;
  cwPrefs.end();

  if (ok) {
    Serial.println("CW Settings saved");
  } else {
    Serial.println("[CW] ERROR: NVS write failed (partition full or corrupt?) - settings NOT saved");
  }
}

#endif // SETTINGS_CW_H
