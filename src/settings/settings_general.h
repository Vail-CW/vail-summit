/*
 * General Settings Module
 * Allows user to configure general/miscellaneous settings
 * Currently includes: Callsign/Name for Vail repeater
 */

#ifndef SETTINGS_GENERAL_H
#define SETTINGS_GENERAL_H

#include <Preferences.h>
#include "../core/config.h"
#include "../network/vail_repeater.h"

// Buffer sizes
#define CALLSIGN_MAX_LEN 13  // 12 chars + null terminator

// Callsign settings globals
Preferences callsignPrefs;

void saveCallsign(const char* callsign);
bool loadCallsign(char* callsign, size_t len);

// Save callsign to flash memory
void saveCallsign(const char* callsign) {
  callsignPrefs.begin("callsign", false);
  callsignPrefs.putString("call", callsign);
  callsignPrefs.end();

  Serial.print("Callsign saved: ");
  Serial.println(callsign);
}

// Load callsign from flash memory
bool loadCallsign(char* callsign, size_t len) {
  callsignPrefs.begin("callsign", true);
  String val = callsignPrefs.getString("call", "");
  callsignPrefs.end();

  strncpy(callsign, val.c_str(), len - 1);
  callsign[len - 1] = '\0';

  return (strlen(callsign) > 0);
}

// Load callsign on startup (call from setup())
void loadSavedCallsign() {
  char savedCallsign[CALLSIGN_MAX_LEN];
  if (loadCallsign(savedCallsign, sizeof(savedCallsign))) {
    strlcpy(vailCallsign, savedCallsign, VAIL_CALLSIGN_LEN);
    Serial.print("Loaded callsign: ");
    Serial.println(savedCallsign);
  } else {
    Serial.println("No saved callsign, using default: GUEST");
  }
}

#endif // SETTINGS_GENERAL_H
