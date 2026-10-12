/*
 * Web Password Settings Module
 * Allows user to set/change password for web interface access
 * Password is stored and can be changed without knowing current password
 */

#ifndef SETTINGS_WEB_PASSWORD_H
#define SETTINGS_WEB_PASSWORD_H

#include <Preferences.h>
#include "../core/config.h"

// Buffer sizes
#define WEB_PASSWORD_MAX_LEN 17  // 16 chars + null terminator

// Password settings state
enum PasswordState {
  PASSWORD_STATE_NORMAL,
  PASSWORD_STATE_CONFIRM_DISABLE
};

// Password settings globals
Preferences webPasswordPrefs;

// Global password state (used by web_server.h)
char webPassword[WEB_PASSWORD_MAX_LEN] = "";
bool webAuthEnabled = false;

// Web server start policy (used by web_server.h). false = on demand: the
// server (and its AsyncTCP task) only starts when a screen that shows the
// web URL is opened or a web mode runs, and stops again after idle time.
// true = classic behaviour, start as soon as WiFi connects.
bool webServerAlwaysOn = false;

void saveWebServerAlwaysOn(bool alwaysOn) {
  webServerAlwaysOn = alwaysOn;
  webPasswordPrefs.begin("webpw", false);
  webPasswordPrefs.putBool("srvAlways", alwaysOn);
  webPasswordPrefs.end();
  Serial.printf("[WebPW] Web server policy: %s\n", alwaysOn ? "always on" : "on demand");
}

// LVGL mode flag - when true, skip legacy draw functions (LVGL handles display)

void saveWebPassword(const char* password);
void clearWebPassword();

// Generate a random per-device password (unambiguous charset: no 0/O/1/I/L).
void generateWebPassword(char* buf, size_t len) {
  static const char charset[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
  int n = (int)len - 1;
  for (int i = 0; i < n; i++) {
    buf[i] = charset[esp_random() % (sizeof(charset) - 1)];
  }
  buf[n] = '\0';
}

// Save web password to flash memory
void saveWebPassword(const char* password) {
  Serial.printf("[WebPW] Saving password (len=%d)\n", (int)strlen(password));  // value not logged
  webPasswordPrefs.begin("webpw", false);
  webPasswordPrefs.putString("pw", password);
  webPasswordPrefs.putBool("enabled", true);
  webPasswordPrefs.putBool("provisioned", true);
  webPasswordPrefs.end();

  Serial.println("[WebPW] Password saved and enabled");
}

// Clear/disable web password
void clearWebPassword() {
  webPasswordPrefs.begin("webpw", false);
  webPasswordPrefs.remove("pw");
  webPasswordPrefs.putBool("enabled", false);
  webPasswordPrefs.end();

  Serial.println("Web password disabled");
}

// Load web password on startup (call from setup()).
//
// Secure by default: on the very first boot (or after a factory reset), no
// password exists yet, so we PROVISION a random per-device password and enable
// auth. The device is therefore never open out of the box; the owner reads the
// password off the System Info screen and can change or disable it afterward. A
// "provisioned" flag ensures we never override a user's later explicit disable.
void loadSavedWebPassword() {
  webPasswordPrefs.begin("webpw", true);  // read-only probe
  bool provisioned = webPasswordPrefs.getBool("provisioned", false);
  bool enabled = webPasswordPrefs.getBool("enabled", false);
  webServerAlwaysOn = webPasswordPrefs.getBool("srvAlways", false);
  char stored[WEB_PASSWORD_MAX_LEN] = "";
  webPasswordPrefs.getString("pw", stored, sizeof(stored));
  webPasswordPrefs.end();

  if (!provisioned) {
    char gen[WEB_PASSWORD_MAX_LEN];
    generateWebPassword(gen, 9);  // 8-char password
    webPasswordPrefs.begin("webpw", false);
    webPasswordPrefs.putString("pw", gen);
    webPasswordPrefs.putBool("enabled", true);
    webPasswordPrefs.putBool("provisioned", true);
    webPasswordPrefs.end();

    strncpy(webPassword, gen, sizeof(webPassword) - 1);
    webPassword[sizeof(webPassword) - 1] = '\0';
    webAuthEnabled = true;
    Serial.println("[WebPW] First boot - provisioned a random web password (shown in System Info)");
    return;
  }

  if (enabled && strlen(stored) > 0) {
    strncpy(webPassword, stored, sizeof(webPassword) - 1);
    webPassword[sizeof(webPassword) - 1] = '\0';
    webAuthEnabled = true;
    Serial.println("[WebPW] Web password protection enabled");
  } else {
    webPassword[0] = '\0';
    webAuthEnabled = false;
    Serial.println("[WebPW] Web password protection disabled by user");
  }
}

#endif // SETTINGS_WEB_PASSWORD_H
