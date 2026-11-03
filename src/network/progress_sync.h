/*
 * Vail CW School Progress Sync
 * Handles bidirectional progress synchronization with CW School cloud
 */

#ifndef PROGRESS_SYNC_H
#define PROGRESS_SYNC_H

#include <ArduinoJson.h>
#include "../core/psram_json.h"
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
#define SYNC_NEXT_ENTRY_MS 5000       // Gap between queue entries after a success
#define SYNC_RETRY_MIN_MS 60000       // First retry delay after a failure
#define SYNC_RETRY_MAX_MS 600000      // Backoff cap (10 minutes)
#define SYNC_USER_IDLE_MS 5000        // Only make a request after this long without input

// Defined in vail-summit.ino: true while a tone plays (plus hold) or the
// current mode is audio-critical. A TLS handshake must not start then.
bool audioPriorityActive();

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
    int queueHead;                   // Ring-buffer slot of the oldest entry
    unsigned long retryDelayMs;      // Wait before the next request attempt
    char lastError[32];              // Last error message
};

static SyncState syncState = {
    .status = SYNC_IDLE,
    .lastSyncTime = 0,
    .lastSyncAttempt = 0,
    .pendingQueueCount = 0,
    .queueHead = 0,
    .retryDelayMs = 0,
    .lastError = {0}
};

static Preferences syncPrefs;

// ============================================
// Sync Payload Generation
// ============================================

/*
 * Build a progress sync payload
 * Includes practice time, session info, and any training progress
 */
String buildSyncPayload(unsigned long sessionDurationSec, const char* sessionMode) {
    JsonDocument doc(psramJsonAllocator());

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
    JsonDocument historyDoc(psramJsonAllocator());
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
// Every sync goes through this queue (NVS namespace "progsync") and is sent
// later by updateProgressSync(), so navigation never waits on HTTPS.
// The queue is a ring buffer: slot = (head + i) % OFFLINE_QUEUE_SIZE, so an
// add or remove is a constant number of NVS writes. A full queue overwrites
// the oldest entry. Entries written by the old shifting queue start at slot
// 0 with no "queue_head" key, which reads back as head 0 - compatible.

static void queueSlotKeys(int slot, char* keyPayload, char* keyTime, size_t n) {
    snprintf(keyPayload, n, "q_payload_%d", slot);
    snprintf(keyTime, n, "q_time_%d", slot);
}

/*
 * Load offline queue count from NVS
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
    int count = syncState.pendingQueueCount;
    int head = syncState.queueHead;
    if (count < 0) count = 0;
    if (count > OFFLINE_QUEUE_SIZE) count = OFFLINE_QUEUE_SIZE;

    int slot;
    if (count >= OFFLINE_QUEUE_SIZE) {
        // Full - overwrite the oldest entry and advance the head
        slot = head;
        head = (head + 1) % OFFLINE_QUEUE_SIZE;
    } else {
        slot = (head + count) % OFFLINE_QUEUE_SIZE;
        count++;
    }

    char keyPayload[16], keyTime[16];
    queueSlotKeys(slot, keyPayload, keyTime, sizeof(keyPayload));

    syncPrefs.begin("progsync", false);
    syncPrefs.putString(keyPayload, payload);
    syncPrefs.putULong(keyTime, millis());
    syncPrefs.putInt("queue_count", count);
    syncPrefs.putInt("queue_head", head);
    syncPrefs.end();

    syncState.pendingQueueCount = count;
    syncState.queueHead = head;
    Serial.printf("[Sync] Added to offline queue (count: %d)\n", count);
    return true;
}

/*
 * Get oldest entry from offline queue (peek, doesn't remove)
 */
String peekOfflineQueue() {
    if (syncState.pendingQueueCount <= 0) return String();
    char keyPayload[16], keyTime[16];
    queueSlotKeys(syncState.queueHead, keyPayload, keyTime, sizeof(keyPayload));
    syncPrefs.begin("progsync", true);
    String payload = syncPrefs.getString(keyPayload, "");
    syncPrefs.end();
    return payload;
}

/*
 * Remove oldest entry from offline queue (after successful sync).
 * If saveSyncTime, also persists lastSyncTime in the same NVS session.
 */
void removeFromOfflineQueue(bool saveSyncTime = false) {
    int count = syncState.pendingQueueCount;
    if (count <= 0) return;

    char keyPayload[16], keyTime[16];
    queueSlotKeys(syncState.queueHead, keyPayload, keyTime, sizeof(keyPayload));
    int head = (syncState.queueHead + 1) % OFFLINE_QUEUE_SIZE;
    count--;
    if (count == 0) head = 0;

    syncPrefs.begin("progsync", false);
    syncPrefs.remove(keyPayload);
    syncPrefs.remove(keyTime);
    syncPrefs.putInt("queue_count", count);
    syncPrefs.putInt("queue_head", head);
    if (saveSyncTime) syncPrefs.putULong("last_sync", syncState.lastSyncTime);
    syncPrefs.end();

    syncState.pendingQueueCount = count;
    syncState.queueHead = head;
    Serial.printf("[Sync] Removed from offline queue (count: %d)\n", count);
}

/*
 * Clear entire offline queue
 */
void clearOfflineQueue() {
    syncPrefs.begin("progsync", false);
    syncPrefs.clear();
    syncPrefs.end();
    syncState.pendingQueueCount = 0;
    syncState.queueHead = 0;
    Serial.println("[Sync] Offline queue cleared");
}

// ============================================
// Sync API Functions
// ============================================

/*
 * Queue a progress payload for the CW School cloud. Never blocks on the
 * network: updateProgressSync() sends it when conditions allow.
 * Returns true if queued.
 */
bool syncProgressToCloud(const String& payload) {
    if (!isCWSchoolLinked()) {
        Serial.println("[Sync] Not linked to CW School - skipping sync");
        return false;
    }
    addToOfflineQueue(payload);
    syncState.status = SYNC_OFFLINE_QUEUED;
    return true;
}

/*
 * Sync a completed session
 * Called when user exits a training mode. Only queues - see updateProgressSync().
 */
bool syncSession(unsigned long durationSec, const char* mode) {
    if (durationSec < 30) {
        // Don't sync very short sessions (< 30 seconds)
        Serial.println("[Sync] Session too short to sync");
        return false;
    }
    if (!isCWSchoolLinked()) return false;

    String payload = buildSyncPayload(durationSec, mode ? mode : "");
    return syncProgressToCloud(payload);
}

/*
 * Ask updateProgressSync() to try the queue right away (e.g. WiFi reconnected),
 * skipping any failure backoff. Does no network work itself.
 */
void flushOfflineQueue() {
    syncState.retryDelayMs = 0;
}

/*
 * Background sender - call every loop(). Makes at most ONE HTTPS request per
 * call, and only when: something is queued, linked, WiFi + internet up, no
 * audio has priority, and the user hasn't touched a key for a few seconds
 * (the request blocks loop() for the handshake). After a success the next
 * queued entry follows SYNC_NEXT_ENTRY_MS later, which drains the backlog
 * a bit at a time; failures back off exponentially.
 */
void updateProgressSync() {
    if (syncState.pendingQueueCount <= 0) return;

    // WiFi just came up: don't wait out a backoff from the offline period
    static bool wifiWasUp = false;
    bool wifiUp = (WiFi.status() == WL_CONNECTED);
    if (wifiUp && !wifiWasUp) flushOfflineQueue();
    wifiWasUp = wifiUp;
    if (!wifiUp) return;

    unsigned long now = millis();
    if (syncState.lastSyncAttempt != 0 &&
        now - syncState.lastSyncAttempt < syncState.retryDelayMs) return;

    if (!isCWSchoolLinked()) return;
    if (getInternetStatus() != INET_CONNECTED) return;
    if (audioPriorityActive()) return;
    if (lv_disp_get_inactive_time(NULL) < SYNC_USER_IDLE_MS) return;

    String payload = peekOfflineQueue();
    if (payload.length() == 0) {
        // Missing/empty slot - drop it so the queue can't stall
        removeFromOfflineQueue();
        return;
    }

    syncState.lastSyncAttempt = now;
    syncState.status = SYNC_IN_PROGRESS;

    String response;
    // Short timeout: this runs inline in loop(), so a slow server must not
    // hold the UI for the full 15 s (a 401 refresh + retry would triple it)
    cwschoolRequestTimeoutMs = 5000;
    int httpCode = cwschoolHttpRequest("POST", "api_summit_syncProgress", payload, response);
    cwschoolRequestTimeoutMs = CWSCHOOL_HTTP_TIMEOUT;

    if (httpCode == 200) {
        syncState.lastSyncTime = millis();
        syncState.status = SYNC_SUCCESS;
        syncState.lastError[0] = '\0';
        syncState.retryDelayMs = SYNC_NEXT_ENTRY_MS;
        removeFromOfflineQueue(true);
        Serial.println("[Sync] Progress synced successfully");

        // TODO: Process merged_progress from response to update local state
        // This would include any progress made on the web that we don't have locally
        return;
    }

    syncState.status = SYNC_FAILED;
    snprintf(syncState.lastError, sizeof(syncState.lastError), "HTTP %d", httpCode);
    Serial.printf("[Sync] Sync failed: HTTP %d\n", httpCode);

    // A request the server rejects outright will never succeed - drop it
    // rather than block everything queued behind it
    if (httpCode >= 400 && httpCode < 500 &&
        httpCode != 401 && httpCode != 408 && httpCode != 429) {
        removeFromOfflineQueue();
        syncState.retryDelayMs = SYNC_NEXT_ENTRY_MS;
        return;
    }

    if (syncState.retryDelayMs < SYNC_RETRY_MIN_MS) {
        syncState.retryDelayMs = SYNC_RETRY_MIN_MS;
    } else {
        syncState.retryDelayMs *= 2;
        if (syncState.retryDelayMs > SYNC_RETRY_MAX_MS) syncState.retryDelayMs = SYNC_RETRY_MAX_MS;
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

    JsonDocument reqDoc(psramJsonAllocator());
    reqDoc["device_id"] = getCWSchoolDeviceId();

    String body;
    serializeJson(reqDoc, body);

    String response;
    int httpCode = cwschoolHttpRequest("POST", "api_summit_getProgress", body, response);

    if (httpCode == 200) {
        JsonDocument respDoc(psramJsonAllocator());
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

const char* getLastSyncError() {
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
    syncState.queueHead = syncPrefs.getInt("queue_head", 0);
    syncPrefs.end();
    if (syncState.pendingQueueCount < 0 || syncState.pendingQueueCount > OFFLINE_QUEUE_SIZE) {
        syncState.pendingQueueCount = 0;
    }
    if (syncState.queueHead < 0 || syncState.queueHead >= OFFLINE_QUEUE_SIZE) {
        syncState.queueHead = 0;
    }

    Serial.printf("[Sync] Initialized - last sync: %lu, queue: %d\n",
                  syncState.lastSyncTime, syncState.pendingQueueCount);
}

#endif // PROGRESS_SYNC_H
