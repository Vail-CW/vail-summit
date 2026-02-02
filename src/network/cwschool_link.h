/*
 * Vail CW School Link API Client
 * Handles device linking and authentication for CW School integration
 */

#ifndef CWSCHOOL_LINK_H
#define CWSCHOOL_LINK_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "../settings/settings_cwschool.h"
#include "../core/config.h"
#include "../core/secrets.h"
#include "internet_check.h"

// ============================================
// API Configuration
// ============================================
#define CWSCHOOL_FUNCTIONS_BASE CWSCHOOL_FUNCTIONS_BASE_URL
#define CWSCHOOL_FIREBASE_API_KEY FIREBASE_CWSCHOOL_API_KEY
#define CWSCHOOL_DEVICE_TYPE "vail_summit"
#define CWSCHOOL_HTTP_TIMEOUT 15000  // 15 seconds

// ============================================
// Link State Machine
// ============================================

enum CWSchoolLinkState {
    CWSCHOOL_LINK_IDLE,
    CWSCHOOL_LINK_REQUESTING_CODE,
    CWSCHOOL_LINK_WAITING_FOR_USER,
    CWSCHOOL_LINK_CHECKING,
    CWSCHOOL_LINK_EXCHANGING_TOKEN,
    CWSCHOOL_LINK_SUCCESS,
    CWSCHOOL_LINK_ERROR,
    CWSCHOOL_LINK_EXPIRED
};

// Global state
static CWSchoolLinkState cwschoolLinkState = CWSCHOOL_LINK_IDLE;

// Device linking state
static String cwschoolLinkCode = "";
static String cwschoolLinkUrl = "";
static int cwschoolLinkExpiresIn = 0;
static unsigned long cwschoolLinkRequestTime = 0;
static unsigned long cwschoolLastLinkCheckTime = 0;
static String cwschoolLinkErrorMessage = "";
static String cwschoolPendingCustomToken = "";

// Forward declarations
bool refreshCWSchoolIdToken();
String getValidCWSchoolToken();
int cwschoolHttpRequest(const String& method, const String& endpoint, const String& body, String& response);

// ============================================
// Token Management
// ============================================

// Get a valid ID token, refreshing if needed
String getValidCWSchoolToken() {
    if (!isCWSchoolLinked()) {
        return "";
    }

    // Check if token is expired or expiring soon (within 5 minutes)
    if (isCWSchoolTokenExpiring(300)) {
        Serial.println("[CWSchool] Token expiring, refreshing...");
        if (!refreshCWSchoolIdToken()) {
            Serial.println("[CWSchool] Failed to refresh token");
            return "";
        }
    }

    return getCWSchoolIdToken();
}

// Exchange custom token for Firebase ID token
bool exchangeCWSchoolCustomToken(const String& customToken) {
    HTTPClient http;
    String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithCustomToken?key=";
    url += CWSCHOOL_FIREBASE_API_KEY;

    http.begin(url);
    http.setTimeout(CWSCHOOL_HTTP_TIMEOUT);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["token"] = customToken;
    doc["returnSecureToken"] = true;

    String body;
    serializeJson(doc, body);

    int httpCode = http.POST(body);
    String response = http.getString();
    http.end();

    if (httpCode == 200) {
        JsonDocument respDoc;
        if (deserializeJson(respDoc, response) == DeserializationError::Ok) {
            String idToken = respDoc["idToken"].as<String>();
            String refreshToken = respDoc["refreshToken"].as<String>();
            int expiresIn = respDoc["expiresIn"].as<int>();

            if (expiresIn == 0) expiresIn = 3600;  // Default 1 hour

            saveCWSchoolTokens(idToken, refreshToken, expiresIn);
            Serial.println("[CWSchool] Token exchange successful");
            return true;
        }
    }

    Serial.printf("[CWSchool] Token exchange failed: %d\n", httpCode);
    Serial.printf("[CWSchool] Response: %s\n", response.c_str());
    return false;
}

// Refresh expired ID token using refresh token
bool refreshCWSchoolIdToken() {
    String refreshToken = getCWSchoolRefreshToken();
    if (refreshToken.length() == 0) {
        Serial.println("[CWSchool] No refresh token available");
        return false;
    }

    HTTPClient http;
    String url = "https://securetoken.googleapis.com/v1/token?key=";
    url += CWSCHOOL_FIREBASE_API_KEY;

    http.begin(url);
    http.setTimeout(CWSCHOOL_HTTP_TIMEOUT);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = "grant_type=refresh_token&refresh_token=" + refreshToken;

    int httpCode = http.POST(body);
    String response = http.getString();
    http.end();

    if (httpCode == 200) {
        JsonDocument respDoc;
        if (deserializeJson(respDoc, response) == DeserializationError::Ok) {
            String newIdToken = respDoc["id_token"].as<String>();
            String newRefreshToken = respDoc["refresh_token"].as<String>();
            int expiresIn = respDoc["expires_in"].as<int>();

            if (expiresIn == 0) expiresIn = 3600;

            saveCWSchoolTokens(newIdToken, newRefreshToken, expiresIn);
            Serial.println("[CWSchool] Token refresh successful");
            return true;
        }
    }

    Serial.printf("[CWSchool] Token refresh failed: %d\n", httpCode);
    return false;
}

// ============================================
// HTTP Helper
// ============================================

// Make HTTP request to CW School API
// Returns HTTP status code, response body in 'response' parameter
int cwschoolHttpRequest(const String& method, const String& functionName, const String& body, String& response) {
    HTTPClient http;
    String url = String(CWSCHOOL_FUNCTIONS_BASE) + "/" + functionName;

    http.begin(url);
    http.setTimeout(CWSCHOOL_HTTP_TIMEOUT);
    http.addHeader("Content-Type", "application/json");

    // Add auth header if we have a token
    String token = getValidCWSchoolToken();
    if (token.length() > 0) {
        http.addHeader("Authorization", "Bearer " + token);
    }

    // Add device ID header if linked
    if (isCWSchoolLinked()) {
        http.addHeader("X-Device-ID", getCWSchoolDeviceId());
    }

    int httpCode;
    if (method == "GET") {
        httpCode = http.GET();
    } else if (method == "POST") {
        httpCode = http.POST(body);
    } else if (method == "PATCH") {
        httpCode = http.PATCH(body);
    } else if (method == "DELETE") {
        httpCode = http.sendRequest("DELETE");
    } else {
        http.end();
        return -1;
    }

    if (httpCode > 0) {
        response = http.getString();
    }
    http.end();

    // Handle 401 - token expired, try refresh once
    if (httpCode == 401 && token.length() > 0) {
        Serial.println("[CWSchool] Got 401, attempting token refresh...");
        if (refreshCWSchoolIdToken()) {
            // Retry the request
            return cwschoolHttpRequest(method, functionName, body, response);
        }
    }

    return httpCode;
}

// Call a Firebase Callable function (onCall)
// These require POST with body wrapped in {"data": {...}} and return {"result": {...}}
int cwschoolCallableRequest(const String& functionName, const JsonDocument& data, JsonDocument& result) {
    HTTPClient http;
    String url = String(CWSCHOOL_FUNCTIONS_BASE) + "/" + functionName;

    http.begin(url);
    http.setTimeout(CWSCHOOL_HTTP_TIMEOUT);
    http.addHeader("Content-Type", "application/json");

    // Add auth header (required for callable functions)
    String token = getValidCWSchoolToken();
    if (token.length() > 0) {
        http.addHeader("Authorization", "Bearer " + token);
    }

    // Wrap data in {"data": ...}
    JsonDocument requestDoc;
    requestDoc["data"] = data;

    String body;
    serializeJson(requestDoc, body);

    int httpCode = http.POST(body);

    if (httpCode > 0) {
        String response = http.getString();

        if (httpCode == 200) {
            JsonDocument respDoc;
            if (deserializeJson(respDoc, response) == DeserializationError::Ok) {
                // Unwrap result from {"result": ...}
                if (respDoc.containsKey("result")) {
                    result.set(respDoc["result"]);
                } else if (respDoc.containsKey("error")) {
                    Serial.printf("[CWSchool] Callable error: %s\n",
                        respDoc["error"]["message"].as<const char*>());
                }
            }
        } else {
            Serial.printf("[CWSchool] Callable %s failed: %d\n", functionName.c_str(), httpCode);
            Serial.printf("[CWSchool] Response: %s\n", response.c_str());
        }
    }
    http.end();

    // Handle 401 - token expired, try refresh once
    if (httpCode == 401 && token.length() > 0) {
        Serial.println("[CWSchool] Got 401 on callable, attempting token refresh...");
        if (refreshCWSchoolIdToken()) {
            return cwschoolCallableRequest(functionName, data, result);
        }
    }

    return httpCode;
}

// ============================================
// Device Linking Flow
// ============================================

// Request a device linking code
bool requestCWSchoolDeviceCode() {
    if (getInternetStatus() != INET_CONNECTED) {
        cwschoolLinkErrorMessage = "No internet connection";
        cwschoolLinkState = CWSCHOOL_LINK_ERROR;
        return false;
    }

    cwschoolLinkState = CWSCHOOL_LINK_REQUESTING_CODE;

    JsonDocument doc;
    doc["device_name"] = "VAIL Summit";
    doc["device_type"] = CWSCHOOL_DEVICE_TYPE;
    doc["firmware_version"] = FIRMWARE_VERSION;

    // Include existing device ID if we have one (for re-linking)
    if (getCWSchoolDeviceId().length() > 0) {
        doc["device_id"] = getCWSchoolDeviceId();
    }

    String body;
    serializeJson(doc, body);

    String response;
    int httpCode = cwschoolHttpRequest("POST", "api_summit_requestCode", body, response);

    if (httpCode == 200) {
        JsonDocument respDoc;
        if (deserializeJson(respDoc, response) == DeserializationError::Ok) {
            cwschoolLinkCode = respDoc["code"].as<String>();
            cwschoolLinkUrl = respDoc["link_url"].as<String>();
            cwschoolLinkExpiresIn = respDoc["expires_in"].as<int>();

            // Store device ID if provided (new device)
            if (respDoc.containsKey("device_id")) {
                String newDeviceId = respDoc["device_id"].as<String>();
                if (newDeviceId.length() > 0) {
                    // Save just the device ID for now
                    cwschoolPrefs.begin("cwschool", false);
                    cwschoolPrefs.putString("device_id", newDeviceId);
                    cwschoolPrefs.end();
                    cwschoolSettings.deviceId = newDeviceId;
                }
            }

            cwschoolLinkRequestTime = millis();
            cwschoolLastLinkCheckTime = 0;
            cwschoolLinkState = CWSCHOOL_LINK_WAITING_FOR_USER;

            Serial.printf("[CWSchool] Got device code: %s (expires in %d sec)\n",
                          cwschoolLinkCode.c_str(), cwschoolLinkExpiresIn);
            return true;
        }
    }

    // Show specific error based on HTTP code
    if (httpCode == 404) {
        cwschoolLinkErrorMessage = "API not found - check server";
    } else if (httpCode == 0 || httpCode < 0) {
        cwschoolLinkErrorMessage = "Connection failed - check WiFi";
    } else if (httpCode == 429) {
        cwschoolLinkErrorMessage = "Too many requests - wait and retry";
    } else if (httpCode == 500) {
        cwschoolLinkErrorMessage = "Server error (500)";
    } else {
        char errBuf[48];
        snprintf(errBuf, sizeof(errBuf), "Failed (HTTP %d)", httpCode);
        cwschoolLinkErrorMessage = errBuf;
    }
    cwschoolLinkState = CWSCHOOL_LINK_ERROR;
    Serial.printf("[CWSchool] requestDeviceCode failed: %d\n", httpCode);
    Serial.printf("[CWSchool] Response: %s\n", response.c_str());
    return false;
}

// Check if user has completed device linking
// Returns: 0=pending, 1=linked, -1=expired/error
int checkCWSchoolDeviceCode() {
    if (cwschoolLinkCode.length() == 0) {
        return -1;
    }

    // Check if code expired locally
    unsigned long elapsed = millis() - cwschoolLinkRequestTime;
    if (elapsed > (unsigned long)cwschoolLinkExpiresIn * 1000) {
        cwschoolLinkState = CWSCHOOL_LINK_EXPIRED;
        cwschoolLinkErrorMessage = "Code expired";
        return -1;
    }

    cwschoolLinkState = CWSCHOOL_LINK_CHECKING;

    // Build query string
    String functionName = "api_summit_checkCode?code=" + cwschoolLinkCode;
    if (getCWSchoolDeviceId().length() > 0) {
        functionName += "&device_id=" + getCWSchoolDeviceId();
    }

    String response;
    int httpCode = cwschoolHttpRequest("GET", functionName, "", response);

    Serial.printf("[CWSchool] checkDeviceCode HTTP code: %d\n", httpCode);

    if (httpCode == 200) {
        JsonDocument respDoc;
        DeserializationError jsonErr = deserializeJson(respDoc, response);
        if (jsonErr == DeserializationError::Ok) {
            String status = respDoc["status"].as<String>();
            Serial.printf("[CWSchool] checkDeviceCode status: '%s'\n", status.c_str());

            if (status == "pending") {
                cwschoolLinkState = CWSCHOOL_LINK_WAITING_FOR_USER;
                return 0;
            }
            else if (status == "linked") {
                Serial.println("[CWSchool] Got 'linked' status - exchanging token");

                // Got custom token - exchange for ID token
                cwschoolPendingCustomToken = respDoc["custom_token"].as<String>();
                String deviceId = respDoc["device_id"].as<String>();
                String uid = respDoc["user"]["uid"].as<String>();
                String callsign = respDoc["user"]["callsign"].as<String>();
                String displayName = respDoc["user"]["display_name"].as<String>();

                Serial.printf("[CWSchool] deviceId: %s, uid: %s, callsign: %s\n",
                              deviceId.c_str(), uid.c_str(), callsign.c_str());

                cwschoolLinkState = CWSCHOOL_LINK_EXCHANGING_TOKEN;

                if (exchangeCWSchoolCustomToken(cwschoolPendingCustomToken)) {
                    saveCWSchoolDeviceLink(deviceId, uid, callsign, displayName);
                    cwschoolLinkState = CWSCHOOL_LINK_SUCCESS;
                    cwschoolLinkCode = "";  // Clear code
                    Serial.println("[CWSchool] Link SUCCESS!");
                    return 1;
                } else {
                    cwschoolLinkErrorMessage = "Failed to exchange token";
                    cwschoolLinkState = CWSCHOOL_LINK_ERROR;
                    Serial.println("[CWSchool] Token exchange FAILED");
                    return -1;
                }
            } else {
                Serial.printf("[CWSchool] Unknown status: '%s'\n", status.c_str());
            }
        } else {
            Serial.printf("[CWSchool] JSON parse error: %s\n", jsonErr.c_str());
        }
    }
    else if (httpCode == 410) {
        cwschoolLinkState = CWSCHOOL_LINK_EXPIRED;
        cwschoolLinkErrorMessage = "Code expired";
        return -1;
    }
    else if (httpCode == 404) {
        cwschoolLinkState = CWSCHOOL_LINK_ERROR;
        cwschoolLinkErrorMessage = "Code not found";
        return -1;
    }

    // Still pending or transient error
    cwschoolLinkState = CWSCHOOL_LINK_WAITING_FOR_USER;
    return 0;
}

// Get device link state
CWSchoolLinkState getCWSchoolLinkState() {
    return cwschoolLinkState;
}

// Get link code for display
String getCWSchoolLinkCode() {
    return cwschoolLinkCode;
}

// Get link URL for display
String getCWSchoolLinkUrl() {
    return cwschoolLinkUrl;
}

// Get remaining time for link code
int getCWSchoolLinkRemainingSeconds() {
    if (cwschoolLinkExpiresIn == 0 || cwschoolLinkRequestTime == 0) return 0;
    unsigned long elapsed = (millis() - cwschoolLinkRequestTime) / 1000;
    if (elapsed >= (unsigned long)cwschoolLinkExpiresIn) return 0;
    return cwschoolLinkExpiresIn - elapsed;
}

// Get link error message
String getCWSchoolLinkError() {
    return cwschoolLinkErrorMessage;
}

// Reset link state (for retry)
void resetCWSchoolLinkState() {
    cwschoolLinkState = CWSCHOOL_LINK_IDLE;
    cwschoolLinkCode = "";
    cwschoolLinkUrl = "";
    cwschoolLinkExpiresIn = 0;
    cwschoolLinkRequestTime = 0;
    cwschoolLinkErrorMessage = "";
    cwschoolPendingCustomToken = "";
}

// ============================================
// Initialization
// ============================================

// Initialize CW School integration (call in setup)
void initCWSchool() {
    loadCWSchoolSettings();
    Serial.printf("[CWSchool] Initialized - %s\n",
                  isCWSchoolLinked() ? "linked" : "not linked");
}

// ============================================
// Account Info
// ============================================

// Get formatted account display string
String getCWSchoolAccountDisplay() {
    if (!isCWSchoolLinked()) {
        return "Not linked";
    }

    String display = getCWSchoolUserCallsign();
    if (display.length() == 0) {
        display = getCWSchoolDisplayName();
    }
    if (display.length() == 0) {
        display = "Linked";
    }
    return display;
}

#endif // CWSCHOOL_LINK_H
