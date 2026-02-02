/*
 * Vail CW School Progress Sync
 * Handles bidirectional progress synchronization with CW School cloud
 */

#ifndef PROGRESS_SYNC_H
#define PROGRESS_SYNC_H

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "../settings/settings_cwschool.h"
#include "../settings/settings_practice_time.h"
#include "cwschool_link.h"
#include "internet_check.h"

// ============================================
// Sync Configuration
// ============================================

#define PROGRESS_SYNC_VERSION 1
#define OFFLINE_QUEUE_SIZE 10
#define SYNC_COOLDOWN_MS 60000  // Minimum 1 minute between syncs

// ============================================
// Sync State
// ============================================

enum SyncStatus {
    SYNC_IDLE,
    SYNC_IN_PROGRESS,
    SYNC_SUCCESS,
    SYNC_FAILED,
    SYNC_OFFLINE_QUEUED
};

struct SyncState {
    SyncStatus status;
    unsigned long lastSyncTime;      // millis() of last successful sync
    unsigned long lastSyncAttempt;   // millis() of last sync attempt
    int pendingQueueCount;           // Number of items in offline queue
    String lastError;                // Last error message
};

static SyncState syncState = {
    .status = SYNC_IDLE,
    .lastSyncTime = 0,
    .lastSyncAttempt = 0,
    .pendingQueueCount = 0,
    .lastError = ""
};

// Offline queue entry
struct OfflineQueueEntry {
    unsigned long timestamp;         // When this entry was created
    String payload;                  // JSON payload to sync
    bool valid;                      // Is this entry valid?
};

static Preferences syncPrefs;

// ============================================
// Sync Payload Generation
// ============================================

/*
 * Build a progress sync payload
 * Includes practice time, session info, and any training progress
 */
String buildSyncPayload(unsigned long sessionDurationSec, const String& sessionMode) {
    JsonDocument doc;

    // Version and device info
    doc["v"] = PROGRESS_SYNC_VERSION;
    doc["device_id"] = getCWSchoolDeviceId();
    doc["last_sync"] = syncState.lastSyncTime;

    // Current session info (if any)
    if (sessionDurationSec > 0) {
        JsonObject session = doc["session"].to<JsonObject>();
        session["duration_sec"] = sessionDurationSec;
        session["mode"] = sessionMode;
    }

    // Practice time data
    JsonObject practiceTimeObj = doc["practice_time"].to<JsonObject>();
    practiceTimeObj["today_sec"] = getTodayPracticeSeconds();
    practiceTimeObj["total_sec"] = getTotalPracticeSeconds();
    practiceTimeObj["streak"] = getPracticeStreak();
    practiceTimeObj["best_streak"] = getLongestPracticeStreak();

    // Practice history (parse from the JSON string helper)
    String historyJson = getPracticeHistoryJson();
    JsonDocument historyDoc;
    if (deserializeJson(historyDoc, historyJson) == DeserializationError::Ok) {
        doc["practice_time"]["history"] = historyDoc;
    }

    // Serialize to string
    String payload;
    serializeJson(doc, payload);
    return payload;
}

/*
 * Build a minimal sync payload (just practice time, for session end)
 */
String buildMinimalSyncPayload() {
    return buildSyncPayload(0, "");
}

// ============================================
// Offline Queue Management
// ============================================

/*
 * Load offline queue from NVS
 */
int loadOfflineQueueCount() {
    syncPrefs.begin("progsync", true);
    int count = syncPrefs.getInt("queue_count", 0);
    syncPrefs.end();
    return count;
}

/*
 * Add entry to offline queue
 */
bool addToOfflineQueue(const String& payload) {
    syncPrefs.begin("progsync", false);

    int count = syncPrefs.getInt("queue_count", 0);
    if (count >= OFFLINE_QUEUE_SIZE) {
        // Queue full - remove oldest entry (shift down)
        for (int i = 0; i < OFFLINE_QUEUE_SIZE - 1; i++) {
            char keyFrom[16], keyTo[16];
            snprintf(keyFrom, sizeof(keyFrom), "q_payload_%d", i + 1);
            snprintf(keyTo, sizeof(keyTo), "q_payload_%d", i);

            String val = syncPrefs.getString(keyFrom, "");
            syncPrefs.putString(keyTo, val);

            snprintf(keyFrom, sizeof(keyFrom), "q_time_%d", i + 1);
            snprintf(keyTo, sizeof(keyTo), "q_time_%d", i);
            unsigned long ts = syncPrefs.getULong(keyFrom, 0);
            syncPrefs.putULong(keyTo, ts);
        }
        count = OFFLINE_QUEUE_SIZE - 1;
    }

    // Add new entry at end
    char keyPayload[16], keyTime[16];
    snprintf(keyPayload, sizeof(keyPayload), "q_payload_%d", count);
    snprintf(keyTime, sizeof(keyTime), "q_time_%d", count);

    syncPrefs.putString(keyPayload, payload);
    syncPrefs.putULong(keyTime, millis());
    syncPrefs.putInt("queue_count", count + 1);

    syncPrefs.end();

    syncState.pendingQueueCount = count + 1;
    Serial.printf("[Sync] Added to offline queue (count: %d)\n", count + 1);
    return true;
}

/*
 * Get oldest entry from offline queue (peek, doesn't remove)
 */
String peekOfflineQueue() {
    syncPrefs.begin("progsync", true);
    String payload = syncPrefs.getString("q_payload_0", "");
    syncPrefs.end();
    return payload;
}

/*
 * Remove oldest entry from offline queue (after successful sync)
 */
void removeFromOfflineQueue() {
    syncPrefs.begin("progsync", false);

    int count = syncPrefs.getInt("queue_count", 0);
    if (count <= 0) {
        syncPrefs.end();
        return;
    }

    // Shift all entries down
    for (int i = 0; i < count - 1; i++) {
        char keyFrom[16], keyTo[16];
        snprintf(keyFrom, sizeof(keyFrom), "q_payload_%d", i + 1);
        snprintf(keyTo, sizeof(keyTo), "q_payload_%d", i);

        String val = syncPrefs.getString(keyFrom, "");
        syncPrefs.putString(keyTo, val);

        snprintf(keyFrom, sizeof(keyFrom), "q_time_%d", i + 1);
        snprintf(keyTo, sizeof(keyTo), "q_time_%d", i);
        unsigned long ts = syncPrefs.getULong(keyFrom, 0);
        syncPrefs.putULong(keyTo, ts);
    }

    // Clear last slot and update count
    char keyPayload[16], keyTime[16];
    snprintf(keyPayload, sizeof(keyPayload), "q_payload_%d", count - 1);
    snprintf(keyTime, sizeof(keyTime), "q_time_%d", count - 1);
    syncPrefs.remove(keyPayload);
    syncPrefs.remove(keyTime);
    syncPrefs.putInt("queue_count", count - 1);

    syncPrefs.end();

    syncState.pendingQueueCount = count - 1;
    Serial.printf("[Sync] Removed from offline queue (count: %d)\n", count - 1);
}

/*
 * Clear entire offline queue
 */
void clearOfflineQueue() {
    syncPrefs.begin("progsync", false);
    syncPrefs.clear();
    syncPrefs.end();
    syncState.pendingQueueCount = 0;
    Serial.println("[Sync] Offline queue cleared");
}

// ============================================
// Sync API Functions
// ============================================

/*
 * Sync progress with CW School cloud
 * Returns true if sync was successful or queued
 */
bool syncProgressToCloud(const String& payload) {
    // Check if we're linked
    if (!isCWSchoolLinked()) {
        Serial.println("[Sync] Not linked to CW School - skipping sync");
        return false;
    }

    // Check cooldown
    if (millis() - syncState.lastSyncAttempt < SYNC_COOLDOWN_MS) {
        Serial.println("[Sync] Cooldown active - skipping sync");
        return false;
    }

    syncState.lastSyncAttempt = millis();
    syncState.status = SYNC_IN_PROGRESS;

    // Check internet connectivity
    if (getInternetStatus() != INET_CONNECTED) {
        Serial.println("[Sync] No internet - adding to offline queue");
        addToOfflineQueue(payload);
        syncState.status = SYNC_OFFLINE_QUEUED;
        return true;
    }

    // Make sync request
    String response;
    int httpCode = cwschoolHttpRequest("POST", "api_summit_syncProgress", payload, response);

    if (httpCode == 200) {
        // Parse response to update local state if needed
        JsonDocument respDoc;
        if (deserializeJson(respDoc, response) == DeserializationError::Ok) {
            // Update last sync time
            syncState.lastSyncTime = millis();
            syncState.status = SYNC_SUCCESS;
            syncState.lastError = "";

            // Save last sync time
            syncPrefs.begin("progsync", false);
            syncPrefs.putULong("last_sync", syncState.lastSyncTime);
            syncPrefs.end();

            Serial.println("[Sync] Progress synced successfully");

            // TODO: Process merged_progress from response to update local state
            // This would include any progress made on the web that we don't have locally

            return true;
        }
    }

    // Sync failed
    syncState.status = SYNC_FAILED;
    syncState.lastError = "HTTP " + String(httpCode);
    Serial.printf("[Sync] Sync failed: HTTP %d\n", httpCode);

    // Add to offline queue for retry
    addToOfflineQueue(payload);
    return false;
}

/*
 * Sync a completed session
 * Called when user exits a training mode
 */
bool syncSession(unsigned long durationSec, const String& mode) {
    if (durationSec < 30) {
        // Don't sync very short sessions (< 30 seconds)
        Serial.println("[Sync] Session too short to sync");
        return false;
    }

    String payload = buildSyncPayload(durationSec, mode);
    return syncProgressToCloud(payload);
}

/*
 * Flush offline queue (call when WiFi reconnects)
 */
void flushOfflineQueue() {
    if (!isCWSchoolLinked()) return;
    if (getInternetStatus() != INET_CONNECTED) return;

    int count = loadOfflineQueueCount();
    if (count == 0) return;

    Serial.printf("[Sync] Flushing offline queue (%d entries)\n", count);

    while (syncState.pendingQueueCount > 0) {
        String payload = peekOfflineQueue();
        if (payload.length() == 0) break;

        // Try to sync this entry
        String response;
        int httpCode = cwschoolHttpRequest("POST", "api_summit_syncProgress", payload, response);

        if (httpCode == 200) {
            removeFromOfflineQueue();
            Serial.println("[Sync] Flushed queue entry successfully");
        } else {
            // Stop trying if we hit an error
            Serial.printf("[Sync] Queue flush failed: HTTP %d\n", httpCode);
            break;
        }

        // Small delay between requests
        delay(500);
    }
}

/*
 * Pull progress from cloud (initial sync)
 * Returns true if successful
 */
bool pullProgressFromCloud() {
    if (!isCWSchoolLinked()) {
        return false;
    }

    if (getInternetStatus() != INET_CONNECTED) {
        return false;
    }

    JsonDocument reqDoc;
    reqDoc["device_id"] = getCWSchoolDeviceId();

    String body;
    serializeJson(reqDoc, body);

    String response;
    int httpCode = cwschoolHttpRequest("POST", "api_summit_getProgress", body, response);

    if (httpCode == 200) {
        JsonDocument respDoc;
        if (deserializeJson(respDoc, response) == DeserializationError::Ok) {
            Serial.println("[Sync] Progress pulled from cloud");

            // TODO: Merge cloud progress with local state
            // This includes:
            // - Practice time (take max of local/cloud)
            // - Streaks (take max)
            // - Training progress (union of completed items)

            return true;
        }
    }

    Serial.printf("[Sync] Pull progress failed: HTTP %d\n", httpCode);
    return false;
}

// ============================================
// Sync State Getters
// ============================================

SyncStatus getSyncStatus() {
    return syncState.status;
}

unsigned long getLastSyncTime() {
    return syncState.lastSyncTime;
}

int getPendingQueueCount() {
    return syncState.pendingQueueCount;
}

String getLastSyncError() {
    return syncState.lastError;
}

// ============================================
// Initialization
// ============================================

/*
 * Initialize progress sync (call in setup)
 */
void initProgressSync() {
    syncPrefs.begin("progsync", true);
    syncState.lastSyncTime = syncPrefs.getULong("last_sync", 0);
    syncState.pendingQueueCount = syncPrefs.getInt("queue_count", 0);
    syncPrefs.end();

    Serial.printf("[Sync] Initialized - last sync: %lu, queue: %d\n",
                  syncState.lastSyncTime, syncState.pendingQueueCount);
}

#endif // PROGRESS_SYNC_H
