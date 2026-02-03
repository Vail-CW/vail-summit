#ifndef MORSE_NOTES_RECORDER_H
#define MORSE_NOTES_RECORDER_H

#include "morse_notes_types.h"
#include "morse_notes_storage.h"
#include "../audio/morse_decoder_adaptive.h"

// ===================================
// MORSE NOTES - RECORDING ENGINE
// ===================================

// Global recording session
static MorseNotesRecordingSession mnRecordingSession;
static float mnRecordingTimingBuffer[MN_MAX_RECORDING_EVENTS];
static MorseDecoderAdaptive mnRecordingDecoder(20, 20, 30);  // For WPM calculation

// External tone control (from task_manager.h)
extern void requestStartTone(int frequency);
extern void requestStopTone();

// External settings (from config)
extern int cwTone;
extern int cwSpeed;

// ===================================
// RECORDING CONTROL
// ===================================

/**
 * Start recording
 */
bool mnStartRecording() {
    // Check if already recording
    if (mnRecordingSession.state == MN_REC_RECORDING) {
        Serial.println("[MorseNotes] WARNING: Already recording");
        return false;
    }

    // Check SD card space (minimum 500KB)
    if (!mnCheckSpace(500000)) {
        Serial.println("[MorseNotes] ERROR: Insufficient SD card space");
        return false;
    }

    // Initialize session
    mnRecordingSession.state = MN_REC_RECORDING;
    mnRecordingSession.timingBuffer = mnRecordingTimingBuffer;
    mnRecordingSession.eventCount = 0;
    mnRecordingSession.startTime = millis();
    mnRecordingSession.lastEventTime = 0;
    mnRecordingSession.keyState = false;
    memset(mnRecordingSession.title, 0, sizeof(mnRecordingSession.title));

    // Reset decoder for WPM calculation
    mnRecordingDecoder.flush();

    Serial.println("[MorseNotes] Recording started");
    return true;
}

/**
 * Stop recording
 */
bool mnStopRecording() {
    if (mnRecordingSession.state != MN_REC_RECORDING) {
        Serial.println("[MorseNotes] WARNING: Not currently recording");
        return false;
    }

    // Ensure tone is stopped
    requestStopTone();

    // Update state
    mnRecordingSession.state = MN_REC_COMPLETE;

    Serial.printf("[MorseNotes] Recording stopped. Events: %d, Duration: %lu ms\n",
                  mnRecordingSession.eventCount,
                  millis() - mnRecordingSession.startTime);

    return true;
}

/**
 * Save recording with title
 */
bool mnSaveRecording(const char* title) {
    if (mnRecordingSession.state != MN_REC_COMPLETE) {
        Serial.println("[MorseNotes] ERROR: No recording to save");
        return false;
    }

    if (mnRecordingSession.eventCount == 0) {
        Serial.println("[MorseNotes] ERROR: Empty recording");
        return false;
    }

    // Use provided title or generate default
    const char* finalTitle = title;
    char defaultTitle[64];
    if (title == nullptr || strlen(title) == 0) {
        time_t now = time(nullptr);
        mnGenerateDefaultTitle((unsigned long)now, defaultTitle, sizeof(defaultTitle));
        finalTitle = defaultTitle;
    }

    // Calculate average WPM from decoder
    float avgWPM = mnRecordingDecoder.getWPM();
    if (avgWPM < 5.0f) {
        avgWPM = (float)cwSpeed;  // Fall back to configured speed
    }

    // Save to SD card
    bool success = mnSaveRecording(
        finalTitle,
        mnRecordingSession.timingBuffer,
        mnRecordingSession.eventCount,
        cwTone,
        avgWPM
    );

    if (success) {
        mnRecordingSession.state = MN_REC_IDLE;
        Serial.println("[MorseNotes] Recording saved successfully");
    } else {
        Serial.println("[MorseNotes] ERROR: Failed to save recording");
    }

    return success;
}

/**
 * Discard recording
 */
void mnDiscardRecording() {
    mnRecordingSession.state = MN_REC_IDLE;
    mnRecordingSession.eventCount = 0;
    Serial.println("[MorseNotes] Recording discarded");
}

/**
 * Check if currently recording
 */
bool mnIsRecording() {
    return mnRecordingSession.state == MN_REC_RECORDING;
}

/**
 * Check if recording is complete (awaiting save)
 */
bool mnIsRecordingComplete() {
    return mnRecordingSession.state == MN_REC_COMPLETE;
}

/**
 * Get current recording state
 */
MorseNotesRecordState mnGetRecordingState() {
    return mnRecordingSession.state;
}

/**
 * Get recording duration in milliseconds
 */
unsigned long mnGetRecordingDuration() {
    if (mnRecordingSession.state == MN_REC_RECORDING) {
        return millis() - mnRecordingSession.startTime;
    } else if (mnRecordingSession.state == MN_REC_COMPLETE) {
        return mnCalculateDuration(mnRecordingSession.timingBuffer,
                                   mnRecordingSession.eventCount);
    }
    return 0;
}

/**
 * Get recording event count
 */
int mnGetRecordingEventCount() {
    return mnRecordingSession.eventCount;
}

/**
 * Get recording average WPM
 */
float mnGetRecordingWPM() {
    float wpm = mnRecordingDecoder.getWPM();
    return (wpm >= 5.0f) ? wpm : (float)cwSpeed;
}

/**
 * Check if key is currently down
 */
bool mnIsKeyDown() {
    return mnRecordingSession.keyState;
}

// ===================================
// KEYER CALLBACK
// ===================================

/**
 * Keyer callback for timing capture
 * Called from radio_output.h when key state changes
 *
 * @param keyDown true if key pressed, false if released
 * @param timestamp Current time in milliseconds
 */
void mnKeyerCallback(bool keyDown, unsigned long timestamp) {
    // Only record if in recording state
    if (mnRecordingSession.state != MN_REC_RECORDING) {
        return;
    }

    // Check buffer limit
    if (mnRecordingSession.eventCount >= MN_MAX_RECORDING_EVENTS) {
        Serial.println("[MorseNotes] WARNING: Event buffer full, stopping recording");
        mnStopRecording();
        return;
    }

    // Check duration limit
    unsigned long elapsed = timestamp - mnRecordingSession.startTime;
    if (elapsed >= MN_MAX_RECORDING_DURATION_MS) {
        Serial.println("[MorseNotes] WARNING: Duration limit reached, stopping recording");
        mnStopRecording();
        return;
    }

    // Record timing event
    if (keyDown && !mnRecordingSession.keyState) {
        // Key down - add silence duration
        if (mnRecordingSession.lastEventTime > 0) {
            float silence = -(float)(timestamp - mnRecordingSession.lastEventTime);
            mnRecordingSession.timingBuffer[mnRecordingSession.eventCount++] = silence;

            // Feed to decoder for WPM calculation
            mnRecordingDecoder.addTiming(silence);
        }
        mnRecordingSession.lastEventTime = timestamp;
        mnRecordingSession.keyState = true;

        // Play sidetone
        requestStartTone(cwTone);
    }
    else if (!keyDown && mnRecordingSession.keyState) {
        // Key up - add tone duration
        float tone = (float)(timestamp - mnRecordingSession.lastEventTime);
        mnRecordingSession.timingBuffer[mnRecordingSession.eventCount++] = tone;

        // Feed to decoder for WPM calculation
        mnRecordingDecoder.addTiming(tone);

        mnRecordingSession.lastEventTime = timestamp;
        mnRecordingSession.keyState = false;

        // Stop sidetone
        requestStopTone();
    }
}

// ===================================
// RECORDING INFO
// ===================================

/**
 * Get formatted recording stats string
 * Format: "N events • X.X WPM avg"
 */
void mnGetRecordingStats(char* buffer, size_t bufferSize) {
    float wpm = mnGetRecordingWPM();
    snprintf(buffer, bufferSize, "%d events  •  %.1f WPM avg",
             mnRecordingSession.eventCount, wpm);
}

/**
 * Get formatted recording duration string
 * Format: "MM:SS / 05:00"
 */
void mnGetRecordingDurationString(char* buffer, size_t bufferSize) {
    unsigned long elapsed = mnGetRecordingDuration();
    unsigned long maxDuration = MN_MAX_RECORDING_DURATION_MS;

    int elapsedMins = elapsed / 60000;
    int elapsedSecs = (elapsed / 1000) % 60;
    int maxMins = maxDuration / 60000;
    int maxSecs = (maxDuration / 1000) % 60;

    snprintf(buffer, bufferSize, "%02d:%02d / %02d:%02d",
             elapsedMins, elapsedSecs, maxMins, maxSecs);
}

/**
 * Check if recording should show warning (near limit)
 */
bool mnShouldShowRecordingWarning() {
    if (mnRecordingSession.state != MN_REC_RECORDING) {
        return false;
    }

    unsigned long elapsed = millis() - mnRecordingSession.startTime;
    return elapsed >= MN_WARNING_TIME_MS;
}

#endif // MORSE_NOTES_RECORDER_H
