/*
 * Web Hear It Type It Mode - Device Side
 * Minimal device-side module. All game logic runs in browser.
 */

#ifndef WEB_HEAR_IT_MODE_H
#define WEB_HEAR_IT_MODE_H

#include "../../core/config.h"

// Web hear it mode state
bool webHearItModeActive = false;

/*
 * Initialize web hear it mode (called from initializeModeInt)
 */
void initWebHearItMode() {
    Serial.println("Initializing web Hear It Type It mode");
    webHearItModeActive = true;
}

/*
 * Update function (called every loop iteration)
 * All game logic runs in browser - nothing to do here
 */
void updateWebHearItMode() {
    // Browser handles all game logic (callsign generation, validation, audio)
    // Device just displays LVGL status screen
}

#endif // WEB_HEAR_IT_MODE_H
