/*
 * Morse Mailbox Settings Module
 * Handles authentication tokens and account settings storage
 */

#ifndef SETTINGS_MAILBOX_H
#define SETTINGS_MAILBOX_H

#include <Preferences.h>
#include <Arduino.h>

// Mailbox settings state
struct MailboxSettings {
    bool linked = false;
    String deviceId;
    String idToken;
    String refreshToken;
    unsigned long tokenExpiry = 0;  // millis() when token expires
    String userCallsign;
    String userMmid;  // Morse Mailbox ID (MM-XXXXX)
};

static MailboxSettings mailboxSettings;
static Preferences mailboxPrefs;

// Forward declarations
void loadMailboxSettings();
void saveMailboxSettings();
void clearMailboxCredentials();
bool isMailboxLinked();
String getMailboxDeviceId();
String getMailboxUserCallsign();
String getMailboxUserMmid();

// Load mailbox settings from flash
void loadMailboxSettings() {
    mailboxPrefs.begin("mailbox", true);  // Read-only

    mailboxSettings.linked = mailboxPrefs.getBool("linked", false);
    mailboxSettings.deviceId = mailboxPrefs.getString("device_id", "");
    mailboxSettings.idToken = mailboxPrefs.getString("id_token", "");
    mailboxSettings.refreshToken = mailboxPrefs.getString("refresh_tkn", "");
    mailboxSettings.tokenExpiry = mailboxPrefs.getULong("token_exp", 0);
    mailboxSettings.userCallsign = mailboxPrefs.getString("callsign", "");
    mailboxSettings.userMmid = mailboxPrefs.getString("mmid", "");

    mailboxPrefs.end();

    Serial.printf("[Mailbox] Settings loaded - linked: %s, callsign: %s\n",
                  mailboxSettings.linked ? "yes" : "no",
                  mailboxSettings.userCallsign.c_str());
}

// Save mailbox settings to flash
void saveMailboxSettings() {
    mailboxPrefs.begin("mailbox", false);  // Read-write

    mailboxPrefs.putBool("linked", mailboxSettings.linked);
    mailboxPrefs.putString("device_id", mailboxSettings.deviceId);
    mailboxPrefs.putString("id_token", mailboxSettings.idToken);
    mailboxPrefs.putString("refresh_tkn", mailboxSettings.refreshToken);
    mailboxPrefs.putULong("token_exp", mailboxSettings.tokenExpiry);
    mailboxPrefs.putString("callsign", mailboxSettings.userCallsign);
    mailboxPrefs.putString("mmid", mailboxSettings.userMmid);

    mailboxPrefs.end();

    Serial.println("[Mailbox] Settings saved");
}

// Save authentication tokens specifically (called after token refresh)
void saveMailboxTokens(const String& idToken, const String& refreshToken, unsigned long expiresInSeconds) {
    mailboxSettings.idToken = idToken;
    mailboxSettings.refreshToken = refreshToken;
    // Calculate expiry time - subtract 5 minutes for safety margin
    mailboxSettings.tokenExpiry = millis() + ((expiresInSeconds - 300) * 1000);

    mailboxPrefs.begin("mailbox", false);
    mailboxPrefs.putString("id_token", idToken);
    mailboxPrefs.putString("refresh_tkn", refreshToken);
    mailboxPrefs.putULong("token_exp", mailboxSettings.tokenExpiry);
    mailboxPrefs.end();

    Serial.printf("[Mailbox] Tokens saved, expires in %lu seconds\n", expiresInSeconds - 300);
}

// Save device link info (called after successful device linking)
void saveMailboxDeviceLink(const String& deviceId, const String& callsign, const String& mmid) {
    mailboxSettings.linked = true;
    mailboxSettings.deviceId = deviceId;
    mailboxSettings.userCallsign = callsign;
    mailboxSettings.userMmid = mmid;

    mailboxPrefs.begin("mailbox", false);
    mailboxPrefs.putBool("linked", true);
    mailboxPrefs.putString("device_id", deviceId);
    mailboxPrefs.putString("callsign", callsign);
    mailboxPrefs.putString("mmid", mmid);
    mailboxPrefs.end();

    Serial.printf("[Mailbox] Device linked as %s (%s)\n", callsign.c_str(), mmid.c_str());
}

// Clear all mailbox credentials (for unlinking)
void clearMailboxCredentials() {
    mailboxSettings.linked = false;
    mailboxSettings.deviceId = "";
    mailboxSettings.idToken = "";
    mailboxSettings.refreshToken = "";
    mailboxSettings.tokenExpiry = 0;
    mailboxSettings.userCallsign = "";
    mailboxSettings.userMmid = "";

    mailboxPrefs.begin("mailbox", false);
    mailboxPrefs.clear();
    mailboxPrefs.end();

    Serial.println("[Mailbox] Credentials cleared - device unlinked");
}

// Check if device is linked to a Morse Mailbox account
bool isMailboxLinked() {
    return mailboxSettings.linked && mailboxSettings.deviceId.length() > 0;
}

// Check if token needs refresh (expired or expiring soon)
bool isMailboxTokenExpired() {
    if (mailboxSettings.idToken.length() == 0) return true;
    return millis() > mailboxSettings.tokenExpiry;
}

// Get stored ID token (may be expired - caller should check/refresh)
String getMailboxIdToken() {
    return mailboxSettings.idToken;
}

// Get stored refresh token
String getMailboxRefreshToken() {
    return mailboxSettings.refreshToken;
}

// Get device ID
String getMailboxDeviceId() {
    return mailboxSettings.deviceId;
}

// Get user's callsign
String getMailboxUserCallsign() {
    return mailboxSettings.userCallsign;
}

// Get user's Morse Mailbox ID
String getMailboxUserMmid() {
    return mailboxSettings.userMmid;
}

#endif // SETTINGS_MAILBOX_H
