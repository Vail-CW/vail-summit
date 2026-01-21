/*
 * Web First Boot Handler
 * Prompts user to download web files on first WiFi connection
 */

#ifndef WEB_FIRST_BOOT_H
#define WEB_FIRST_BOOT_H

#include <Arduino.h>
#include <Preferences.h>
#include "../../core/config.h"
#include "../../storage/sd_card.h"
#include "web_file_downloader.h"

// First boot state tracking
bool webFilesDownloadPromptShown = false;
bool webFilesDownloading = false;
bool webFilesPromptPending = false;  // Flag set after version check completes
bool webFilesUpdateAvailable = false;  // True if checking found an update
bool webFilesCheckPending = false;  // Flag set by WiFi event to trigger check in main loop
unsigned long webFilesCheckRequestTime = 0;  // When the check was requested

// Preferences for tracking first boot
Preferences webFilesPrefs;

// Forward declaration of display functions (implemented in main sketch)
extern void beep(int frequency, int duration);
extern LGFX tft;

// ============================================
// First Boot Detection
// ============================================

/**
 * Check if this is first WiFi connection with SD card but no web files,
 * OR if an update is available for existing web files
 * @return true if should prompt for web files download/update
 */
bool shouldPromptForWebFilesDownload() {
  // Don't prompt if already shown this session
  if (webFilesDownloadPromptShown) {
    return false;
  }

  // Must have WiFi connected
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  // Check if SD card is available
  if (!sdCardAvailable) {
    // Try to initialize SD card
    if (!initSDCard()) {
      Serial.println("No SD card available - skipping web files prompt");
      return false;
    }
  }

  // Check if web files already exist
  String indexPath = String(WEB_FILES_PATH) + "index.html";
  bool filesExist = SD.exists(indexPath.c_str());

  if (filesExist) {
    // Files exist - check if an update is available
    Serial.println("Web files exist - checking for updates...");

    // Check remote version (result is cached)
    String remoteVersion;
    if (isWebFilesUpdateAvailable(&remoteVersion)) {
      // Check if user declined updates for this specific version
      webFilesPrefs.begin("webfiles", true);
      String declinedVersion = webFilesPrefs.getString("declined_ver", "");
      webFilesPrefs.end();

      // If user declined this specific version, don't prompt again
      if (declinedVersion == remoteVersion) {
        Serial.printf("User declined update to version %s\n", remoteVersion.c_str());
        return false;
      }

      // Update available and not declined
      webFilesUpdateAvailable = true;
      return true;
    }

    Serial.println("Web files are up to date");
    return false;
  }

  // Files don't exist - check if user has declined initial install
  webFilesPrefs.begin("webfiles", true);
  bool declined = webFilesPrefs.getBool("declined", false);
  webFilesPrefs.end();

  if (declined) {
    Serial.println("User previously declined web files download");
    return false;
  }

  webFilesUpdateAvailable = false;  // This is a fresh install, not an update
  return true;
}

/**
 * Check if the pending prompt is for an update (vs new install)
 * @return true if this is an update prompt
 */
bool isWebFilesUpdatePrompt() {
  return webFilesUpdateAvailable;
}

/**
 * Mark that user declined the download prompt
 * For updates, stores the declined version so we don't prompt again for the same version
 */
void declineWebFilesDownload() {
  webFilesPrefs.begin("webfiles", false);

  if (webFilesUpdateAvailable) {
    // For updates, store the declined version (use cached version)
    String remoteVersion = getCachedRemoteVersion();
    if (!remoteVersion.isEmpty()) {
      webFilesPrefs.putString("declined_ver", remoteVersion);
      Serial.printf("User declined update to version %s\n", remoteVersion.c_str());
    }
  } else {
    // For fresh installs, mark as declined entirely
    webFilesPrefs.putBool("declined", true);
  }

  webFilesPrefs.end();
  webFilesDownloadPromptShown = true;
}

/**
 * Reset the declined flag (e.g., when user manually deletes web files)
 */
void resetWebFilesDeclined() {
  webFilesPrefs.begin("webfiles", false);
  webFilesPrefs.putBool("declined", false);
  webFilesPrefs.end();
}

// ============================================
// Non-Blocking Prompt Detection (used by LVGL version)
// ============================================

/**
 * Trigger web files check after internet connectivity is verified
 * Called from internet_check.h when INET_CONNECTED state is first reached
 * This ensures we only check for web files when we know internet is actually available
 */
void triggerWebFilesCheckIfReady() {
  if (!webFilesDownloadPromptShown && !webFilesCheckPending) {
    Serial.println("Internet verified - scheduling web files version check");
    webFilesCheckPending = true;
    webFilesCheckRequestTime = millis();
  }
}

/**
 * Request a web files check (safe to call from WiFi event handler)
 * DEPRECATED: Now handled by triggerWebFilesCheckIfReady() called from internet_check.h
 * Kept for backwards compatibility - redirects to new function
 */
void checkAndShowWebFilesPrompt() {
  // Redirect to new function for backwards compatibility
  triggerWebFilesCheckIfReady();
}

/**
 * Perform the actual web files version check (call from main loop)
 * This makes HTTP requests so must NOT be called from event handlers
 * @return true if check was performed
 */
bool performWebFilesCheck() {
  if (!webFilesCheckPending) {
    return false;
  }

  // Wait at least 2 seconds after WiFi connect before checking
  // This gives the network stack time to stabilize
  if (millis() - webFilesCheckRequestTime < 2000) {
    return false;
  }

  Serial.println("Performing web files version check...");
  webFilesCheckPending = false;

  if (shouldPromptForWebFilesDownload()) {
    Serial.println("Web files update/install available - setting prompt flag");
    webFilesPromptPending = true;
    return true;
  }

  Serial.println("Web files check complete - no action needed");
  return true;
}

/**
 * Check if web files prompt is pending (call from main loop)
 * @return true if prompt should be shown
 */
bool isWebFilesPromptPending() {
  return webFilesPromptPending && !webFilesDownloadPromptShown;
}

/**
 * Clear the pending prompt flag
 */
void clearWebFilesPromptPending() {
  webFilesPromptPending = false;
}

#endif // WEB_FIRST_BOOT_H
