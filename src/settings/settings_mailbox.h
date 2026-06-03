/*
 * Morse Mailbox Settings Module
 * Handles authentication tokens and account settings storage
 */

#ifndef SETTINGS_MAILBOX_H
#define SETTINGS_MAILBOX_H

#include <Preferences.h>
#include <Arduino.h>

// Buffer sizes for fixed-length fields
#define MAILBOX_DEVICE_ID_LEN 64   // Device ID
#define MAILBOX_CALLSIGN_LEN 16    // User callsign
#define MAILBOX_MMID_LEN 16        // Morse Mailbox ID (MM-XXXXX)

// Mailbox settings state
// Note: idToken and refreshToken are kept as String because JWT tokens
// can be 500-2000+ bytes and have variable length. Fixed buffers would
// risk truncation. The short bounded fields use char buffers.
struct MailboxSettings {
    bool linked = false;
    char deviceId[MAILBOX_DEVICE_ID_LEN];
    String idToken;         // JWT token - variable length, kept as String
    String refreshToken;    // Refresh token - variable length, kept as String
    unsigned long tokenExpiry = 0;  // millis() when token expires
    char userCallsign[MAILBOX_CALLSIGN_LEN];
    char userMmid[MAILBOX_MMID_LEN];  // Morse Mailbox ID (MM-XXXXX)
};

static MailboxSettings mailboxSettings;
static Preferences mailboxPrefs;

// Forward declarations
void loadMailboxSettings();
void saveMailboxSettings();
void clearMailboxCredentials();
bool isMailboxLinked();
const char* getMailboxDeviceId();
const char* getMailboxUserCallsign();
const char* getMailboxUserMmid();

// Load mailbox settings from flash
void loadMailboxSettings() {
    mailboxPrefs.begin("mailbox", true);  // Read-only

    mailboxSettings.linked = mailboxPrefs.getBool("linked", false);

    String val;
    val = mailboxPrefs.getString("device_id", "");
    strncpy(mailboxSettings.deviceId, val.c_str(), sizeof(mailboxSettings.deviceId) - 1);
    mailboxSettings.deviceId[sizeof(mailboxSettings.deviceId) - 1] = '\0';

    mailboxSettings.idToken = mailboxPrefs.getString("id_token", "");
    mailboxSettings.refreshToken = mailboxPrefs.getString("refresh_tkn", "");
    mailboxSettings.tokenExpiry = mailboxPrefs.getULong("token_exp", 0);

    val = mailboxPrefs.getString("callsign", "");
    strncpy(mailboxSettings.userCallsign, val.c_str(), sizeof(mailboxSettings.userCallsign) - 1);
    mailboxSettings.userCallsign[sizeof(mailboxSettings.userCallsign) - 1] = '\0';

    val = mailboxPrefs.getString("mmid", "");
    strncpy(mailboxSettings.userMmid, val.c_str(), sizeof(mailboxSettings.userMmid) - 1);
    mailboxSettings.userMmid[sizeof(mailboxSettings.userMmid) - 1] = '\0';

    mailboxPrefs.end();

    Serial.printf("[Mailbox] Settings loaded - linked: %s, callsign: %s\n",
                  mailboxSettings.linked ? "yes" : "no",
                  mailboxSettings.userCallsign);
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
void saveMailboxDeviceLink(const char* deviceId, const char* callsign, const char* mmid) {
    mailboxSettings.linked = true;
    strncpy(mailboxSettings.deviceId, deviceId, sizeof(mailboxSettings.deviceId) - 1);
    mailboxSettings.deviceId[sizeof(mailboxSettings.deviceId) - 1] = '\0';
    strncpy(mailboxSettings.userCallsign, callsign, sizeof(mailboxSettings.userCallsign) - 1);
    mailboxSettings.userCallsign[sizeof(mailboxSettings.userCallsign) - 1] = '\0';
    strncpy(mailboxSettings.userMmid, mmid, sizeof(mailboxSettings.userMmid) - 1);
    mailboxSettings.userMmid[sizeof(mailboxSettings.userMmid) - 1] = '\0';

    mailboxPrefs.begin("mailbox", false);
    mailboxPrefs.putBool("linked", true);
    mailboxPrefs.putString("device_id", deviceId);
    mailboxPrefs.putString("callsign", callsign);
    mailboxPrefs.putString("mmid", mmid);
    mailboxPrefs.end();

    Serial.printf("[Mailbox] Device linked as %s (%s)\n", callsign, mmid);
}

// Clear all mailbox credentials (for unlinking)
void clearMailboxCredentials() {
    mailboxSettings.linked = false;
    mailboxSettings.deviceId[0] = '\0';
    mailboxSettings.idToken = "";
    mailboxSettings.refreshToken = "";
    mailboxSettings.tokenExpiry = 0;
    mailboxSettings.userCallsign[0] = '\0';
    mailboxSettings.userMmid[0] = '\0';

    mailboxPrefs.begin("mailbox", false);
    mailboxPrefs.clear();
    mailboxPrefs.end();

    Serial.println("[Mailbox] Credentials cleared - device unlinked");
}

// Check if device is linked to a Morse Mailbox account
bool isMailboxLinked() {
    return mailboxSettings.linked && strlen(mailboxSettings.deviceId) > 0;
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
const char* getMailboxDeviceId() {
    return mailboxSettings.deviceId;
}

// Get user's callsign
const char* getMailboxUserCallsign() {
    return mailboxSettings.userCallsign;
}

// Get user's Morse Mailbox ID
const char* getMailboxUserMmid() {
    return mailboxSettings.userMmid;
}

#endif // SETTINGS_MAILBOX_H
