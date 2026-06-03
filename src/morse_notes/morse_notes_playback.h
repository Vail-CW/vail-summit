#ifndef MORSE_NOTES_PLAYBACK_H
#define MORSE_NOTES_PLAYBACK_H

#include "morse_notes_types.h"
#include "morse_notes_storage.h"

// ===================================
// MORSE NOTES - PLAYBACK ENGINE
// ===================================

// Global playback session
static MorseNotesPlaybackSession mnPlaybackSession;

// Timing buffer allocated in PSRAM on first use (saves 40KB heap)
static float* mnPlaybackTimingBuffer = nullptr;

// Allocate playback buffer in PSRAM
bool mnEnsurePlaybackBuffer() {
    if (mnPlaybackTimingBuffer != nullptr) return true;

    if (psramFound()) {
        mnPlaybackTimingBuffer = (float*)ps_malloc(MN_MAX_RECORDING_EVENTS * sizeof(float));
        Serial.printf("[MorseNotes] Playback buffer allocated in PSRAM (%d bytes)\n",
                      MN_MAX_RECORDING_EVENTS * sizeof(float));
    } else {
        mnPlaybackTimingBuffer = (float*)malloc(MN_MAX_RECORDING_EVENTS * sizeof(float));
        Serial.println("[MorseNotes] WARNING: PSRAM not found, using heap for playback buffer");
    }

    if (mnPlaybackTimingBuffer == nullptr) {
        Serial.println("[MorseNotes] ERROR: Failed to allocate playback buffer!");
        return false;
    }
    return true;
}

// External tone control (from task_manager.h)
extern void requestStartTone(int frequency);
extern void requestStopTone();

// Forward declarations
void mnStopPlayback();

// ===================================
// PLAYBACK CONTROL
// ===================================

/**
 * Load recording for playback
 */
bool mnLoadForPlayback(unsigned long id) {
    // Stop any current playback
    if (mnPlaybackSession.state == MN_PLAY_PLAYING) {
        mnStopPlayback();
    }

    // Ensure buffer is allocated (in PSRAM)
    if (!mnEnsurePlaybackBuffer()) {
        mnPlaybackSession.state = MN_PLAY_ERROR;
        return false;
    }

    // Load recording from SD card
    int eventCount;
    int toneFreq;
    MorseNoteMetadata* metadata;

    mnPlaybackSession.state = MN_PLAY_LOADING;

    bool success = mnLoadRecording(
        id,
        mnPlaybackTimingBuffer,
        MN_MAX_RECORDING_EVENTS,
        eventCount,
        toneFreq,
        &metadata
    );

    if (!success) {
        mnPlaybackSession.state = MN_PLAY_ERROR;
        Serial.println("[MorseNotes] ERROR: Failed to load recording");
        return false;
    }

    // Initialize playback session
    mnPlaybackSession.state = MN_PLAY_READY;
    mnPlaybackSession.timingBuffer = mnPlaybackTimingBuffer;
    mnPlaybackSession.eventCount = eventCount;
    mnPlaybackSession.currentIndex = 0;
    mnPlaybackSession.startTime = 0;
    mnPlaybackSession.speed = 1.0f;
    mnPlaybackSession.toneFrequency = toneFreq;
    mnPlaybackSession.metadata = metadata;

    Serial.printf("[MorseNotes] Loaded recording: %s (%d events)\n",
                  metadata->title, eventCount);
    return true;
}

/**
 * Initialize playback from recording buffer (for preview before save)
 * Uses the recording session's timing buffer directly
 */
bool mnInitPreviewPlayback(float* timingBuffer, int eventCount, int toneFreq) {
    // Stop any current playback
    if (mnPlaybackSession.state == MN_PLAY_PLAYING) {
        mnStopPlayback();
    }

    if (eventCount == 0 || timingBuffer == nullptr) {
        Serial.println("[MorseNotes] ERROR: No recording data for preview");
        return false;
    }

    // Initialize playback session with recording buffer
    mnPlaybackSession.state = MN_PLAY_READY;
    mnPlaybackSession.timingBuffer = timingBuffer;
    mnPlaybackSession.eventCount = eventCount;
    mnPlaybackSession.currentIndex = 0;
    mnPlaybackSession.startTime = 0;
    mnPlaybackSession.speed = 1.0f;
    mnPlaybackSession.toneFrequency = toneFreq;
    mnPlaybackSession.metadata = nullptr;  // No metadata for preview

    Serial.printf("[MorseNotes] Preview initialized (%d events)\n", eventCount);
    return true;
}

/**
 * Start playback
 */
bool mnStartPlayback() {
    if (mnPlaybackSession.state != MN_PLAY_READY &&
        mnPlaybackSession.state != MN_PLAY_COMPLETE) {
        Serial.println("[MorseNotes] ERROR: Not ready for playback");
        return false;
    }

    mnPlaybackSession.state = MN_PLAY_PLAYING;
    mnPlaybackSession.currentIndex = 0;
    mnPlaybackSession.startTime = millis();

    Serial.println("[MorseNotes] Playback started");
    return true;
}

/**
 * Stop playback
 */
void mnStopPlayback() {
    if (mnPlaybackSession.state == MN_PLAY_PLAYING) {
        requestStopTone();
    }

    mnPlaybackSession.state = MN_PLAY_READY;
    mnPlaybackSession.currentIndex = 0;

    Serial.println("[MorseNotes] Playback stopped");
}

/**
 * Pause playback (optional for v1)
 */
void mnPausePlayback() {
    if (mnPlaybackSession.state == MN_PLAY_PLAYING) {
        requestStopTone();
        mnPlaybackSession.state = MN_PLAY_PAUSED;
        Serial.println("[MorseNotes] Playback paused");
    }
}

/**
 * Resume playback (optional for v1)
 */
void mnResumePlayback() {
    if (mnPlaybackSession.state == MN_PLAY_PAUSED) {
        mnPlaybackSession.state = MN_PLAY_PLAYING;
        // Adjust start time to account for pause
        Serial.println("[MorseNotes] Playback resumed");
    }
}

/**
 * Check if currently playing
 */
bool mnIsPlaying() {
    return mnPlaybackSession.state == MN_PLAY_PLAYING;
}

/**
 * Check if playback is complete
 */
bool mnIsPlaybackComplete() {
    return mnPlaybackSession.state == MN_PLAY_COMPLETE;
}

/**
 * Get playback state
 */
MorseNotesPlaybackState mnGetPlaybackState() {
    return mnPlaybackSession.state;
}

// ===================================
// SPEED CONTROL
// ===================================

/**
 * Set playback speed
 */
void mnSetPlaybackSpeed(float speed) {
    if (speed < 0.1f || speed > 3.0f) {
        Serial.printf("[MorseNotes] WARNING: Invalid speed: %.2f\n", speed);
        return;
    }

    mnPlaybackSession.speed = speed;
    Serial.printf("[MorseNotes] Playback speed: %.2fx\n", speed);
}

/**
 * Get current playback speed
 */
float mnGetPlaybackSpeed() {
    return mnPlaybackSession.speed;
}

/**
 * Cycle to next speed (0.5x → 0.75x → 1.0x → ... → 2.0x → 0.5x)
 */
void mnCyclePlaybackSpeed(bool increment) {
    // Find current speed index
    int currentIndex = MN_SPEED_DEFAULT_INDEX;
    for (int i = 0; i < MN_SPEED_COUNT; i++) {
        if (abs(MN_SPEED_OPTIONS[i] - mnPlaybackSession.speed) < 0.01f) {
            currentIndex = i;
            break;
        }
    }

    // Get next index
    int nextIndex = mnGetNextSpeedIndex(currentIndex, increment);
    mnPlaybackSession.speed = MN_SPEED_OPTIONS[nextIndex];

    Serial.printf("[MorseNotes] Speed: %.2fx\n", mnPlaybackSession.speed);
}

// ===================================
// PLAYBACK UPDATE (Call from timer)
// ===================================

/**
 * Update playback state
 * Call this from a timer callback (e.g., every 50ms)
 * Based on Mailbox playback pattern
 */
void mnUpdatePlayback() {
    if (mnPlaybackSession.state != MN_PLAY_PLAYING) {
        return;
    }

    // Check if playback complete
    if (mnPlaybackSession.currentIndex >= mnPlaybackSession.eventCount) {
        requestStopTone();
        mnPlaybackSession.state = MN_PLAY_COMPLETE;
        Serial.println("[MorseNotes] Playback complete");
        return;
    }

    // Calculate elapsed time with speed adjustment
    unsigned long elapsed = (unsigned long)((millis() - mnPlaybackSession.startTime) *
                                            mnPlaybackSession.speed);

    // Calculate cumulative time to current index
    float cumulativeTime = 0.0f;
    for (int i = 0; i < mnPlaybackSession.currentIndex && i < mnPlaybackSession.eventCount; i++) {
        cumulativeTime += abs(mnPlaybackSession.timingBuffer[i]);
    }

    // Process events until we catch up to elapsed time
    while (mnPlaybackSession.currentIndex < mnPlaybackSession.eventCount) {
        float eventValue = mnPlaybackSession.timingBuffer[mnPlaybackSession.currentIndex];
        float eventDuration = abs(eventValue);

        // Check if this event should have happened by now
        if (cumulativeTime > (float)elapsed) {
            break;  // Wait for time to catch up
        }

        // Process event
        if (eventValue > 0.0f) {
            // Positive = tone on
            requestStartTone(mnPlaybackSession.toneFrequency);
        } else {
            // Negative = tone off
            requestStopTone();
        }

        // Move to next event
        mnPlaybackSession.currentIndex++;
        cumulativeTime += eventDuration;
    }
}

// ===================================
// PLAYBACK INFO
// ===================================

/**
 * Get playback progress (0.0 - 1.0)
 */
float mnGetPlaybackProgress() {
    if (mnPlaybackSession.eventCount == 0) {
        return 0.0f;
    }

    // Progress based on event index
    return (float)mnPlaybackSession.currentIndex / (float)mnPlaybackSession.eventCount;
}

/**
 * Get playback elapsed time in milliseconds
 */
unsigned long mnGetPlaybackElapsed() {
    if (mnPlaybackSession.state != MN_PLAY_PLAYING) {
        return 0;
    }

    // Calculate based on events processed
    float cumulativeTime = 0.0f;
    for (int i = 0; i < mnPlaybackSession.currentIndex && i < mnPlaybackSession.eventCount; i++) {
        cumulativeTime += abs(mnPlaybackSession.timingBuffer[i]);
    }

    return (unsigned long)cumulativeTime;
}

/**
 * Get total playback duration in milliseconds
 */
unsigned long mnGetPlaybackTotalDuration() {
    if (mnPlaybackSession.metadata) {
        return mnPlaybackSession.metadata->durationMs;
    }

    return mnCalculateDuration(mnPlaybackSession.timingBuffer,
                               mnPlaybackSession.eventCount);
}

/**
 * Get formatted playback time string
 * Format: "MM:SS / MM:SS"
 */
void mnGetPlaybackTimeString(char* buffer, size_t bufferSize) {
    unsigned long elapsed = mnGetPlaybackElapsed();
    unsigned long total = mnGetPlaybackTotalDuration();

    int elapsedMins = elapsed / 60000;
    int elapsedSecs = (elapsed / 1000) % 60;
    int totalMins = total / 60000;
    int totalSecs = (total / 1000) % 60;

    snprintf(buffer, bufferSize, "%02d:%02d / %02d:%02d",
             elapsedMins, elapsedSecs, totalMins, totalSecs);
}

/**
 * Get current metadata
 */
MorseNoteMetadata* mnGetCurrentMetadata() {
    return mnPlaybackSession.metadata;
}

#endif // MORSE_NOTES_PLAYBACK_H
