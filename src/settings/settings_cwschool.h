/*
 * Vail CW School Settings Module
 * Handles authentication tokens and account settings storage for CW School integration
 */

#ifndef SETTINGS_CWSCHOOL_H
#define SETTINGS_CWSCHOOL_H

#include <Preferences.h>
#include <Arduino.h>

// CW School settings state
struct CWSchoolSettings {
    bool linked = false;
    String deviceId;
    String idToken;
    String refreshToken;
    unsigned long tokenExpiry = 0;  // millis() when token expires
    String userUid;                 // Firebase Auth UID
    String userCallsign;
    String displayName;
};

static CWSchoolSettings cwschoolSettings;
static Preferences cwschoolPrefs;

// Forward declarations
void loadCWSchoolSettings();
void saveCWSchoolSettings();
void clearCWSchoolCredentials();
bool isCWSchoolLinked();
String getCWSchoolDeviceId();
String getCWSchoolUserCallsign();
String getCWSchoolDisplayName();
String getCWSchoolUserUid();

// Load CW School settings from flash
void loadCWSchoolSettings() {
    cwschoolPrefs.begin("cwschool", true);  // Read-only

    cwschoolSettings.linked = cwschoolPrefs.getBool("linked", false);
    cwschoolSettings.deviceId = cwschoolPrefs.getString("device_id", "");
    cwschoolSettings.idToken = cwschoolPrefs.getString("id_token", "");
    cwschoolSettings.refreshToken = cwschoolPrefs.getString("refresh_tkn", "");
    cwschoolSettings.tokenExpiry = cwschoolPrefs.getULong("token_exp", 0);
    cwschoolSettings.userUid = cwschoolPrefs.getString("uid", "");
    cwschoolSettings.userCallsign = cwschoolPrefs.getString("callsign", "");
    cwschoolSettings.displayName = cwschoolPrefs.getString("display", "");

    cwschoolPrefs.end();

    Serial.printf("[CWSchool] Settings loaded - linked: %s, callsign: %s\n",
                  cwschoolSettings.linked ? "yes" : "no",
                  cwschoolSettings.userCallsign.c_str());
}

// Save CW School settings to flash
void saveCWSchoolSettings() {
    cwschoolPrefs.begin("cwschool", false);  // Read-write

    cwschoolPrefs.putBool("linked", cwschoolSettings.linked);
    cwschoolPrefs.putString("device_id", cwschoolSettings.deviceId);
    cwschoolPrefs.putString("id_token", cwschoolSettings.idToken);
    cwschoolPrefs.putString("refresh_tkn", cwschoolSettings.refreshToken);
    cwschoolPrefs.putULong("token_exp", cwschoolSettings.tokenExpiry);
    cwschoolPrefs.putString("uid", cwschoolSettings.userUid);
    cwschoolPrefs.putString("callsign", cwschoolSettings.userCallsign);
    cwschoolPrefs.putString("display", cwschoolSettings.displayName);

    cwschoolPrefs.end();

    Serial.println("[CWSchool] Settings saved");
}

// Save authentication tokens specifically (called after token refresh)
void saveCWSchoolTokens(const String& idToken, const String& refreshToken, unsigned long expiresInSeconds) {
    cwschoolSettings.idToken = idToken;
    cwschoolSettings.refreshToken = refreshToken;
    // Calculate expiry time - subtract 5 minutes for safety margin
    cwschoolSettings.tokenExpiry = millis() + ((expiresInSeconds - 300) * 1000);

    cwschoolPrefs.begin("cwschool", false);
    cwschoolPrefs.putString("id_token", idToken);
    cwschoolPrefs.putString("refresh_tkn", refreshToken);
    cwschoolPrefs.putULong("token_exp", cwschoolSettings.tokenExpiry);
    cwschoolPrefs.end();

    Serial.printf("[CWSchool] Tokens saved, expires in %lu seconds\n", expiresInSeconds - 300);
}

// Save device link info (called after successful device linking)
void saveCWSchoolDeviceLink(const String& deviceId, const String& uid, const String& callsign, const String& displayName) {
    cwschoolSettings.linked = true;
    cwschoolSettings.deviceId = deviceId;
    cwschoolSettings.userUid = uid;
    cwschoolSettings.userCallsign = callsign;
    cwschoolSettings.displayName = displayName;

    cwschoolPrefs.begin("cwschool", false);
    cwschoolPrefs.putBool("linked", true);
    cwschoolPrefs.putString("device_id", deviceId);
    cwschoolPrefs.putString("uid", uid);
    cwschoolPrefs.putString("callsign", callsign);
    cwschoolPrefs.putString("display", displayName);
    cwschoolPrefs.end();

    Serial.printf("[CWSchool] Device linked as %s (%s)\n", callsign.c_str(), displayName.c_str());
}

// Clear all CW School credentials (for unlinking)
void clearCWSchoolCredentials() {
    cwschoolSettings.linked = false;
    cwschoolSettings.deviceId = "";
    cwschoolSettings.idToken = "";
    cwschoolSettings.refreshToken = "";
    cwschoolSettings.tokenExpiry = 0;
    cwschoolSettings.userUid = "";
    cwschoolSettings.userCallsign = "";
    cwschoolSettings.displayName = "";

    cwschoolPrefs.begin("cwschool", false);
    cwschoolPrefs.clear();
    cwschoolPrefs.end();

    Serial.println("[CWSchool] Credentials cleared - device unlinked");
}

// Check if device is linked to a CW School account
bool isCWSchoolLinked() {
    return cwschoolSettings.linked && cwschoolSettings.deviceId.length() > 0;
}

// Check if token needs refresh (expired or expiring soon)
bool isCWSchoolTokenExpired() {
    if (cwschoolSettings.idToken.length() == 0) return true;
    return millis() > cwschoolSettings.tokenExpiry;
}

// Check if token is expiring within specified seconds
bool isCWSchoolTokenExpiring(unsigned long withinSeconds) {
    if (cwschoolSettings.idToken.length() == 0) return true;
    return millis() > (cwschoolSettings.tokenExpiry - (withinSeconds * 1000));
}

// Get stored ID token (may be expired - caller should check/refresh)
String getCWSchoolIdToken() {
    return cwschoolSettings.idToken;
}

// Get stored refresh token
String getCWSchoolRefreshToken() {
    return cwschoolSettings.refreshToken;
}

// Get device ID
String getCWSchoolDeviceId() {
    return cwschoolSettings.deviceId;
}

// Get user's Firebase UID
String getCWSchoolUserUid() {
    return cwschoolSettings.userUid;
}

// Get user's callsign
String getCWSchoolUserCallsign() {
    return cwschoolSettings.userCallsign;
}

// Get user's display name
String getCWSchoolDisplayName() {
    return cwschoolSettings.displayName;
}

#endif // SETTINGS_CWSCHOOL_H
