/*
 * Morse Mailbox API Client
 * Handles authentication, message retrieval, sending, and background polling
 */

#ifndef MORSE_MAILBOX_H
#define MORSE_MAILBOX_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "../settings/settings_mailbox.h"
#include "internet_check.h"

// ============================================
// API Configuration
// ============================================
// Note: Firebase Cloud Functions are deployed at their export name, not REST paths
// e.g., api_device_requestCode -> /api_device_requestCode
#define MAILBOX_FUNCTIONS_BASE "https://us-central1-morse-mailbox.cloudfunctions.net"
#define FIREBASE_API_KEY "AIzaSyBb_wl_selKtgg5laFmrkaiITC45M_YoK4"
#define MAILBOX_DEVICE_TYPE "vail_summit"
#define MAILBOX_HTTP_TIMEOUT 10000  // 10 seconds

// Polling configuration
#define MAILBOX_POLL_INTERVAL_MS (3 * 60 * 1000)  // 3 minutes

// ============================================
// Mailbox State
// ============================================

// Device linking state machine
enum MailboxLinkState {
    MAILBOX_LINK_IDLE,
    MAILBOX_LINK_REQUESTING_CODE,
    MAILBOX_LINK_WAITING_FOR_USER,
    MAILBOX_LINK_CHECKING,
    MAILBOX_LINK_EXCHANGING_TOKEN,
    MAILBOX_LINK_SUCCESS,
    MAILBOX_LINK_ERROR,
    MAILBOX_LINK_EXPIRED
};

// Message playback state
enum MailboxPlaybackState {
    MB_PLAYBACK_IDLE,
    MB_PLAYBACK_LOADING,
    MB_PLAYBACK_READY,
    MB_PLAYBACK_PLAYING,
    MB_PLAYBACK_PAUSED,
    MB_PLAYBACK_COMPLETE,
    MB_PLAYBACK_ERROR
};

// Message recording state
enum MailboxRecordState {
    MB_RECORD_IDLE,
    MB_RECORD_READY,
    MB_RECORD_RECORDING,
    MB_RECORD_STOPPED,
    MB_RECORD_SENDING,
    MB_RECORD_SENT,
    MB_RECORD_ERROR
};

// Inbox message (lightweight, no timing data)
struct MailboxMessage {
    String id;
    String senderCallsign;
    String senderMmid;
    String status;  // "unread", "read", "archived"
    String sentAt;
    int durationMs;
    int eventCount;
};

// Global state
static MailboxLinkState mailboxLinkState = MAILBOX_LINK_IDLE;
static MailboxPlaybackState mailboxPlaybackState = MB_PLAYBACK_IDLE;
static MailboxRecordState mailboxRecordState = MB_RECORD_IDLE;

// Device linking state
static String linkCode = "";
static String linkUrl = "";
static int linkExpiresIn = 0;
static unsigned long linkRequestTime = 0;
static unsigned long lastLinkCheckTime = 0;
static String linkErrorMessage = "";
static String pendingCustomToken = "";

// Background polling state
static unsigned long lastPollTime = 0;
static int mailboxUnreadCount = 0;
static bool mailboxPollEnabled = true;

// Inbox cache
#define MAILBOX_INBOX_CACHE_SIZE 20
static MailboxMessage inboxCache[MAILBOX_INBOX_CACHE_SIZE];
static int inboxCacheCount = 0;
static bool inboxCacheValid = false;

// Current message for playback
static JsonDocument currentMessageDoc;
static bool currentMessageLoaded = false;

// Forward declarations
bool refreshMailboxIdToken();
String getValidMailboxToken();
int mailboxHttpRequest(const String& method, const String& endpoint, const String& body, String& response);

// ============================================
// Token Management
// ============================================

// Get a valid ID token, refreshing if needed
String getValidMailboxToken() {
    if (!isMailboxLinked()) {
        return "";
    }

    // Check if token is expired or expiring soon
    if (isMailboxTokenExpired()) {
        Serial.println("[Mailbox] Token expired, refreshing...");
        if (!refreshMailboxIdToken()) {
            Serial.println("[Mailbox] Failed to refresh token");
            return "";
        }
    }

    return getMailboxIdToken();
}

// Exchange custom token for Firebase ID token
bool exchangeCustomToken(const String& customToken) {
    HTTPClient http;
    String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithCustomToken?key=";
    url += FIREBASE_API_KEY;

    http.begin(url);
    http.setTimeout(MAILBOX_HTTP_TIMEOUT);
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

            saveMailboxTokens(idToken, refreshToken, expiresIn);
            Serial.println("[Mailbox] Token exchange successful");
            return true;
        }
    }

    Serial.printf("[Mailbox] Token exchange failed: %d\n", httpCode);
    return false;
}

// Refresh expired ID token using refresh token
bool refreshMailboxIdToken() {
    String refreshToken = getMailboxRefreshToken();
    if (refreshToken.length() == 0) {
        Serial.println("[Mailbox] No refresh token available");
        return false;
    }

    HTTPClient http;
    String url = "https://securetoken.googleapis.com/v1/token?key=";
    url += FIREBASE_API_KEY;

    http.begin(url);
    http.setTimeout(MAILBOX_HTTP_TIMEOUT);
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

            saveMailboxTokens(newIdToken, newRefreshToken, expiresIn);
            Serial.println("[Mailbox] Token refresh successful");
            return true;
        }
    }

    Serial.printf("[Mailbox] Token refresh failed: %d\n", httpCode);
    return false;
}

// ============================================
// HTTP Helper
// ============================================

// Make authenticated HTTP request to Mailbox API
// Returns HTTP status code, response body in 'response' parameter
// Note: endpoint should be the full function name, e.g., "api_device_requestCode"
int mailboxHttpRequest(const String& method, const String& functionName, const String& body, String& response) {
    HTTPClient http;
    String url = String(MAILBOX_FUNCTIONS_BASE) + "/" + functionName;

    http.begin(url);
    http.setTimeout(MAILBOX_HTTP_TIMEOUT);
    http.addHeader("Content-Type", "application/json");

    // Add auth header if we have a token
    String token = getValidMailboxToken();
    if (token.length() > 0) {
        http.addHeader("Authorization", "Bearer " + token);
    }

    // Add device ID header if linked
    if (isMailboxLinked()) {
        http.addHeader("X-Device-ID", getMailboxDeviceId());
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
        Serial.println("[Mailbox] Got 401, attempting token refresh...");
        if (refreshMailboxIdToken()) {
            // Retry the request
            return mailboxHttpRequest(method, functionName, body, response);
        }
    }

    return httpCode;
}

// Call a Firebase Callable function (onCall)
// These require POST with body wrapped in {"data": {...}} and return {"result": {...}}
int mailboxCallableRequest(const String& functionName, const JsonDocument& data, JsonDocument& result) {
    HTTPClient http;
    String url = String(MAILBOX_FUNCTIONS_BASE) + "/" + functionName;

    http.begin(url);
    http.setTimeout(MAILBOX_HTTP_TIMEOUT);
    http.addHeader("Content-Type", "application/json");

    // Add auth header (required for callable functions)
    String token = getValidMailboxToken();
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
                    Serial.printf("[Mailbox] Callable error: %s\n",
                        respDoc["error"]["message"].as<const char*>());
                }
            }
        } else {
            Serial.printf("[Mailbox] Callable %s failed: %d\n", functionName.c_str(), httpCode);
            Serial.printf("[Mailbox] Response: %s\n", response.c_str());
        }
    }
    http.end();

    // Handle 401 - token expired, try refresh once
    if (httpCode == 401 && token.length() > 0) {
        Serial.println("[Mailbox] Got 401 on callable, attempting token refresh...");
        if (refreshMailboxIdToken()) {
            return mailboxCallableRequest(functionName, data, result);
        }
    }

    return httpCode;
}

// ============================================
// Device Linking Flow
// ============================================

// Request a device linking code
bool requestDeviceCode() {
    if (getInternetStatus() != INET_CONNECTED) {
        linkErrorMessage = "No internet connection";
        mailboxLinkState = MAILBOX_LINK_ERROR;
        return false;
    }

    mailboxLinkState = MAILBOX_LINK_REQUESTING_CODE;

    JsonDocument doc;
    doc["device_name"] = "VAIL Summit";
    doc["device_type"] = MAILBOX_DEVICE_TYPE;
    doc["firmware_version"] = FIRMWARE_VERSION;

    String body;
    serializeJson(doc, body);

    String response;
    int httpCode = mailboxHttpRequest("POST", "api_device_requestCode", body, response);

    if (httpCode == 200) {
        JsonDocument respDoc;
        if (deserializeJson(respDoc, response) == DeserializationError::Ok) {
            linkCode = respDoc["code"].as<String>();
            linkUrl = respDoc["link_url"].as<String>();
            linkExpiresIn = respDoc["expires_in"].as<int>();

            linkRequestTime = millis();
            lastLinkCheckTime = 0;
            mailboxLinkState = MAILBOX_LINK_WAITING_FOR_USER;

            Serial.printf("[Mailbox] Got device code: %s (expires in %d sec)\n",
                          linkCode.c_str(), linkExpiresIn);
            return true;
        }
    }

    // Show specific error based on HTTP code
    if (httpCode == 404) {
        linkErrorMessage = "API endpoint not found (404)";
    } else if (httpCode == 0 || httpCode < 0) {
        linkErrorMessage = "Connection failed - check WiFi";
    } else if (httpCode == 500) {
        linkErrorMessage = "Server error (500)";
    } else {
        char errBuf[48];
        snprintf(errBuf, sizeof(errBuf), "Failed (HTTP %d)", httpCode);
        linkErrorMessage = errBuf;
    }
    mailboxLinkState = MAILBOX_LINK_ERROR;
    Serial.printf("[Mailbox] requestDeviceCode failed: %d\n", httpCode);
    Serial.printf("[Mailbox] Response: %s\n", response.c_str());
    return false;
}

// Check if user has completed device linking
// Returns: 0=pending, 1=linked, -1=expired/error
int checkDeviceCode() {
    if (linkCode.length() == 0) {
        return -1;
    }

    // Check if code expired locally
    unsigned long elapsed = millis() - linkRequestTime;
    if (elapsed > (unsigned long)linkExpiresIn * 1000) {
        mailboxLinkState = MAILBOX_LINK_EXPIRED;
        linkErrorMessage = "Code expired";
        return -1;
    }

    // Rate limiting is handled by the LVGL timer (5 second interval)
    // No need for additional rate limiting here

    mailboxLinkState = MAILBOX_LINK_CHECKING;

    String response;
    String functionName = "api_device_checkCode?code=" + linkCode;
    int httpCode = mailboxHttpRequest("GET", functionName, "", response);

    Serial.printf("[Mailbox] checkDeviceCode HTTP code: %d\n", httpCode);
    Serial.printf("[Mailbox] checkDeviceCode response: %s\n", response.c_str());

    if (httpCode == 200) {
        JsonDocument respDoc;
        DeserializationError jsonErr = deserializeJson(respDoc, response);
        if (jsonErr == DeserializationError::Ok) {
            String status = respDoc["status"].as<String>();
            Serial.printf("[Mailbox] checkDeviceCode response status: '%s'\n", status.c_str());

            if (status == "pending") {
                mailboxLinkState = MAILBOX_LINK_WAITING_FOR_USER;
                return 0;
            }
            else if (status == "linked") {
                Serial.println("[Mailbox] Got 'linked' status - exchanging token");
                // Got custom token - exchange for ID token
                pendingCustomToken = respDoc["custom_token"].as<String>();
                String deviceId = respDoc["device_id"].as<String>();
                String callsign = respDoc["user"]["callsign"].as<String>();
                String mmid = respDoc["user"]["morse_mailbox_id"].as<String>();
                Serial.printf("[Mailbox] deviceId: %s, callsign: %s, mmid: %s\n",
                              deviceId.c_str(), callsign.c_str(), mmid.c_str());

                mailboxLinkState = MAILBOX_LINK_EXCHANGING_TOKEN;

                if (exchangeCustomToken(pendingCustomToken)) {
                    saveMailboxDeviceLink(deviceId, callsign, mmid);
                    mailboxLinkState = MAILBOX_LINK_SUCCESS;
                    linkCode = "";  // Clear code
                    Serial.println("[Mailbox] Link SUCCESS!");
                    return 1;
                } else {
                    linkErrorMessage = "Failed to exchange token";
                    mailboxLinkState = MAILBOX_LINK_ERROR;
                    Serial.println("[Mailbox] Token exchange FAILED");
                    return -1;
                }
            } else {
                Serial.printf("[Mailbox] Unknown status: '%s'\n", status.c_str());
            }
        } else {
            Serial.printf("[Mailbox] JSON parse error: %s\n", jsonErr.c_str());
        }
    }
    else if (httpCode == 410) {
        mailboxLinkState = MAILBOX_LINK_EXPIRED;
        linkErrorMessage = "Code expired";
        return -1;
    }
    else if (httpCode == 404) {
        mailboxLinkState = MAILBOX_LINK_ERROR;
        linkErrorMessage = "Code not found";
        return -1;
    }

    // Still pending or transient error
    mailboxLinkState = MAILBOX_LINK_WAITING_FOR_USER;
    return 0;
}

// Get device link state
MailboxLinkState getMailboxLinkState() {
    return mailboxLinkState;
}

// Get link code for display
String getMailboxLinkCode() {
    return linkCode;
}

// Get link URL for display
String getMailboxLinkUrl() {
    return linkUrl;
}

// Get remaining time for link code
int getMailboxLinkRemainingSeconds() {
    if (linkExpiresIn == 0 || linkRequestTime == 0) return 0;
    unsigned long elapsed = (millis() - linkRequestTime) / 1000;
    if (elapsed >= (unsigned long)linkExpiresIn) return 0;
    return linkExpiresIn - elapsed;
}

// Get link error message
String getMailboxLinkError() {
    return linkErrorMessage;
}

// Reset link state (for retry)
void resetMailboxLinkState() {
    mailboxLinkState = MAILBOX_LINK_IDLE;
    linkCode = "";
    linkUrl = "";
    linkExpiresIn = 0;
    linkRequestTime = 0;
    linkErrorMessage = "";
    pendingCustomToken = "";
}

// ============================================
// Inbox & Messages
// ============================================

// Get count of unread messages
// Returns -1 on error
int getMailboxUnreadCount() {
    return mailboxUnreadCount;
}

// Check if there are unread messages (for status bar icon)
bool hasUnreadMailboxMessages() {
    return isMailboxLinked() && mailboxUnreadCount > 0;
}

// Fetch inbox messages
// Returns true on success, populates inboxCache
bool fetchMailboxInbox(int limit = 20, const String& status = "all") {
    if (!isMailboxLinked()) return false;

    JsonDocument requestData;
    requestData["limit"] = limit;
    if (status != "all") {
        requestData["status"] = status;
    }
    requestData["device_id"] = getMailboxDeviceId();

    JsonDocument result;
    int httpCode = mailboxCallableRequest("api_messages_inbox", requestData, result);

    if (httpCode == 200) {
        JsonArray messages = result["messages"].as<JsonArray>();

        inboxCacheCount = 0;
        int unreadCount = 0;

        for (JsonObject msg : messages) {
            if (inboxCacheCount >= MAILBOX_INBOX_CACHE_SIZE) break;

            MailboxMessage& m = inboxCache[inboxCacheCount];
            m.id = msg["id"].as<String>();
            m.senderCallsign = msg["sender"]["callsign"].as<String>();
            m.senderMmid = msg["sender"]["morse_mailbox_id"].as<String>();
            m.status = msg["status"].as<String>();
            m.sentAt = msg["sent_at"].as<String>();
            m.durationMs = msg["duration_ms"].as<int>();
            m.eventCount = msg["event_count"].as<int>();

            if (m.status == "unread") unreadCount++;

            inboxCacheCount++;
        }

        mailboxUnreadCount = unreadCount;
        inboxCacheValid = true;

        Serial.printf("[Mailbox] Fetched %d messages, %d unread\n", inboxCacheCount, unreadCount);
        return true;
    }

    Serial.printf("[Mailbox] fetchInbox failed: %d\n", httpCode);
    return false;
}

// Get cached inbox messages
MailboxMessage* getMailboxInboxCache() {
    return inboxCache;
}

int getMailboxInboxCount() {
    return inboxCacheCount;
}

bool isMailboxInboxCacheValid() {
    return inboxCacheValid;
}

void invalidateMailboxInboxCache() {
    inboxCacheValid = false;
}

// Fetch full message with timing data
bool fetchMailboxMessage(const String& messageId) {
    if (!isMailboxLinked()) return false;

    mailboxPlaybackState = MB_PLAYBACK_LOADING;

    JsonDocument requestData;
    requestData["message_id"] = messageId;
    requestData["device_id"] = getMailboxDeviceId();

    int httpCode = mailboxCallableRequest("api_messages_get", requestData, currentMessageDoc);

    if (httpCode == 200) {
        currentMessageLoaded = true;
        mailboxPlaybackState = MB_PLAYBACK_READY;
        Serial.printf("[Mailbox] Loaded message %s\n", messageId.c_str());
        return true;
    }

    mailboxPlaybackState = MB_PLAYBACK_ERROR;
    currentMessageLoaded = false;
    Serial.printf("[Mailbox] fetchMessage failed: %d\n", httpCode);
    return false;
}

// Get current loaded message document
JsonDocument& getCurrentMailboxMessage() {
    return currentMessageDoc;
}

bool isMailboxMessageLoaded() {
    return currentMessageLoaded;
}

// Mark message as read
bool markMailboxMessageRead(const String& messageId) {
    if (!isMailboxLinked()) return false;

    JsonDocument requestData;
    requestData["message_id"] = messageId;
    requestData["status"] = "read";
    requestData["device_id"] = getMailboxDeviceId();

    JsonDocument result;
    int httpCode = mailboxCallableRequest("api_messages_update", requestData, result);

    if (httpCode == 200) {
        // Update cache
        for (int i = 0; i < inboxCacheCount; i++) {
            if (inboxCache[i].id == messageId && inboxCache[i].status == "unread") {
                inboxCache[i].status = "read";
                if (mailboxUnreadCount > 0) mailboxUnreadCount--;
                break;
            }
        }
        Serial.printf("[Mailbox] Marked %s as read\n", messageId.c_str());
        return true;
    }

    Serial.printf("[Mailbox] markRead failed: %d\n", httpCode);
    return false;
}

// ============================================
// Sending Messages
// ============================================

// Send a message with timing data
// timingJson should be a JSON array string like: [{"timestamp":0,"type":"keydown"},...]
bool sendMailboxMessage(const String& recipient, const String& timingJson) {
    if (!isMailboxLinked()) return false;

    JsonDocument requestData;
    requestData["recipient"] = recipient;
    requestData["device_id"] = getMailboxDeviceId();

    // Parse timing array
    JsonDocument timingDoc;
    if (deserializeJson(timingDoc, timingJson) != DeserializationError::Ok) {
        Serial.println("[Mailbox] Invalid timing JSON");
        return false;
    }
    requestData["morse_timing"] = timingDoc.as<JsonArray>();

    JsonDocument result;
    int httpCode = mailboxCallableRequest("api_messages_send", requestData, result);

    if (httpCode == 200) {
        Serial.printf("[Mailbox] Message sent to %s\n", recipient.c_str());
        return true;
    }

    Serial.printf("[Mailbox] sendMessage failed: %d\n", httpCode);
    return false;
}

// ============================================
// User Search
// ============================================

// Search for users by callsign
// Results stored in provided array, returns count
int searchMailboxUsers(const String& query, String callsigns[], String mmids[], int maxResults) {
    if (!isMailboxLinked() || query.length() < 2) return 0;

    JsonDocument requestData;
    requestData["q"] = query;
    requestData["limit"] = maxResults;
    requestData["device_id"] = getMailboxDeviceId();

    JsonDocument result;
    int httpCode = mailboxCallableRequest("api_users_search", requestData, result);

    if (httpCode == 200) {
        JsonArray users = result["users"].as<JsonArray>();
        int count = 0;
        for (JsonObject user : users) {
            if (count >= maxResults) break;
            callsigns[count] = user["callsign"].as<String>();
            mmids[count] = user["morse_mailbox_id"].as<String>();
            count++;
        }
        return count;
    }

    return 0;
}

// ============================================
// Background Polling
// ============================================

// Initialize mailbox polling (call in setup)
void initMailboxPolling() {
    loadMailboxSettings();
    lastPollTime = 0;
    mailboxPollEnabled = true;
}

// Update mailbox polling (call in main loop)
// Non-blocking - checks if it's time to poll and does so
void updateMailboxPolling() {
    // Only poll if linked and internet connected
    if (!isMailboxLinked()) return;
    if (!mailboxPollEnabled) return;
    if (getInternetStatus() != INET_CONNECTED) return;

    unsigned long now = millis();
    if (now - lastPollTime < MAILBOX_POLL_INTERVAL_MS) return;
    lastPollTime = now;

    // Quick poll - just get unread count
    JsonDocument requestData;
    requestData["limit"] = 1;
    requestData["status"] = "unread";
    requestData["device_id"] = getMailboxDeviceId();

    JsonDocument result;
    int httpCode = mailboxCallableRequest("api_messages_inbox", requestData, result);

    if (httpCode == 200) {
        // Count messages in response
        JsonArray messages = result["messages"].as<JsonArray>();
        bool hasMore = result["has_more"].as<bool>();

        // If has_more is true, there are more unread messages
        // For now, just track if there are any unread
        int oldCount = mailboxUnreadCount;
        mailboxUnreadCount = messages.size();
        if (hasMore) mailboxUnreadCount++;  // At least 2+

        if (mailboxUnreadCount > 0 && mailboxUnreadCount > oldCount) {
            Serial.printf("[Mailbox] New messages detected! Count: %d\n", mailboxUnreadCount);
            // UI will detect this via hasUnreadMailboxMessages()
        }
    }
}

// Force an immediate poll (e.g., when entering mailbox screen)
void forceMailboxPoll() {
    lastPollTime = 0;  // Will trigger poll on next updateMailboxPolling()
}

// Enable/disable polling (e.g., disable while in mailbox screen to avoid conflicts)
void setMailboxPollingEnabled(bool enabled) {
    mailboxPollEnabled = enabled;
}

// ============================================
// Playback State Management
// ============================================

void setMailboxPlaybackState(MailboxPlaybackState state) {
    mailboxPlaybackState = state;
}

MailboxPlaybackState getMailboxPlaybackState() {
    return mailboxPlaybackState;
}

// ============================================
// Recording State Management
// ============================================

void setMailboxRecordState(MailboxRecordState state) {
    mailboxRecordState = state;
}

MailboxRecordState getMailboxRecordState() {
    return mailboxRecordState;
}

// ============================================
// Message Recording (Timing Capture)
// ============================================

// Recording timing storage
#define MAILBOX_MAX_TIMING_EVENTS 500  // Max events per recording (prevents memory issues)

struct MailboxTimingEvent {
    int timestamp;  // Milliseconds from recording start
    bool keydown;   // true = keydown, false = keyup
};

static MailboxTimingEvent recordedTiming[MAILBOX_MAX_TIMING_EVENTS];
static int recordedTimingCount = 0;
static unsigned long recordingStartTime = 0;
static bool isRecordingActive = false;
static volatile bool recordKeyState = false;  // Current key state during recording

// Get recorded timing count
int getRecordedTimingCount() {
    return recordedTimingCount;
}

// Get recording duration (ms)
int getRecordedDurationMs() {
    if (recordedTimingCount == 0) return 0;
    // Find last keyup event
    for (int i = recordedTimingCount - 1; i >= 0; i--) {
        if (!recordedTiming[i].keydown) {
            return recordedTiming[i].timestamp;
        }
    }
    // If no keyup, use last event
    return recordedTiming[recordedTimingCount - 1].timestamp;
}

// Check if recording is active
bool isMailboxRecordingActive() {
    return isRecordingActive;
}

// Start recording
void startMailboxRecording() {
    recordedTimingCount = 0;
    recordingStartTime = 0;  // Will be set on first keydown
    isRecordingActive = true;
    recordKeyState = false;
    mailboxRecordState = MB_RECORD_RECORDING;
    Serial.println("[Mailbox] Recording started");
}

// Stop recording and trim to last keyup
void stopMailboxRecording() {
    isRecordingActive = false;

    // If key is still down, add a keyup event
    if (recordKeyState && recordedTimingCount > 0 && recordedTimingCount < MAILBOX_MAX_TIMING_EVENTS) {
        recordedTiming[recordedTimingCount].timestamp = millis() - recordingStartTime;
        recordedTiming[recordedTimingCount].keydown = false;
        recordedTimingCount++;
        recordKeyState = false;
    }

    // Trim to last keyup event (remove trailing entries after last keyup)
    while (recordedTimingCount > 0 && recordedTiming[recordedTimingCount - 1].keydown) {
        recordedTimingCount--;
    }

    if (recordedTimingCount > 0) {
        mailboxRecordState = MB_RECORD_STOPPED;
        Serial.printf("[Mailbox] Recording stopped: %d events, %d ms\n",
                      recordedTimingCount, getRecordedDurationMs());
    } else {
        mailboxRecordState = MB_RECORD_READY;
        Serial.println("[Mailbox] Recording empty, reset to ready");
    }
}

// Clear recorded timing
void clearMailboxRecording() {
    recordedTimingCount = 0;
    recordingStartTime = 0;
    isRecordingActive = false;
    recordKeyState = false;
    mailboxRecordState = MB_RECORD_READY;
}

// Record a key event (called from keyer callback on Core 0)
void recordMailboxKeyEvent(bool keydown) {
    if (!isRecordingActive) return;
    if (recordedTimingCount >= MAILBOX_MAX_TIMING_EVENTS) {
        Serial.println("[Mailbox] Recording buffer full!");
        return;
    }

    unsigned long now = millis();

    // Set start time on first keydown
    if (recordingStartTime == 0 && keydown) {
        recordingStartTime = now;
    }

    // Ignore events before first keydown
    if (recordingStartTime == 0) return;

    // Don't record duplicate events (same key state)
    if (keydown == recordKeyState) return;

    recordedTiming[recordedTimingCount].timestamp = now - recordingStartTime;
    recordedTiming[recordedTimingCount].keydown = keydown;
    recordedTimingCount++;
    recordKeyState = keydown;

    Serial.printf("[Mailbox] Recorded %s at %d ms\n",
                  keydown ? "keydown" : "keyup",
                  recordedTiming[recordedTimingCount - 1].timestamp);
}

// Convert recorded timing to JSON string for API
String getRecordedTimingJson() {
    if (recordedTimingCount == 0) return "[]";

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < recordedTimingCount; i++) {
        JsonObject event = arr.add<JsonObject>();
        event["timestamp"] = recordedTiming[i].timestamp;
        event["type"] = recordedTiming[i].keydown ? "keydown" : "keyup";
    }

    String result;
    serializeJson(doc, result);
    return result;
}

#endif // MORSE_MAILBOX_H
