/*
 * Brightness Settings Module
 * Handles LCD backlight brightness control via PWM
 */

#ifndef SETTINGS_BRIGHTNESS_H
#define SETTINGS_BRIGHTNESS_H

#include <Preferences.h>
#include "../core/config.h"

// Brightness settings state
int brightnessValue = DEFAULT_BRIGHTNESS;

// Preferences for persistent storage
Preferences brightnessPrefs;

// LVGL mode flag - when true, skip legacy draw functions (LVGL handles display)

/*
 * Setup PWM for backlight control
 * Call this once during setup() before using brightness functions
 */
void setupBrightnessPWM() {
  // ledcAttach() allocates an LEDC channel internally; ledcWrite() is
  // addressed by pin.
  ledcAttach(TFT_BL, BRIGHTNESS_PWM_FREQ, BRIGHTNESS_PWM_RESOLUTION);
}

/*
 * Apply brightness value (0-100%) to backlight
 */
void applyBrightness(int percent) {
  int pwmValue = map(percent, 0, 100, 0, 255);
  ledcWrite(TFT_BL, pwmValue);
}

/*
 * Load brightness from preferences
 */
void loadBrightnessSettings() {
  brightnessPrefs.begin("display", true);  // Read-only
  brightnessValue = brightnessPrefs.getInt("brightness", DEFAULT_BRIGHTNESS);
  brightnessPrefs.end();

  // Ensure value is within bounds
  brightnessValue = constrain(brightnessValue, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
}

/*
 * Save brightness to preferences
 */
void saveBrightnessSettings() {
  brightnessPrefs.begin("display", false);  // Read-write
  brightnessPrefs.putInt("brightness", brightnessValue);
  brightnessPrefs.end();
}

/*
 * Get current brightness value
 */
int getBrightness() {
  return brightnessValue;
}

/*
 * Set brightness value and apply it
 */
void setBrightness(int percent) {
  brightnessValue = constrain(percent, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
  applyBrightness(brightnessValue);
  // Deferred: an NVS commit per slider tick crunches the live beep
  markDeferredSave(saveBrightnessSettings);
}

#endif // SETTINGS_BRIGHTNESS_H
