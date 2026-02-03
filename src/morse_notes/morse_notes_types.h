#ifndef MORSE_NOTES_TYPES_H
#define MORSE_NOTES_TYPES_H

#include <Arduino.h>

// ===================================
// MORSE NOTES - DATA STRUCTURES & CONSTANTS
// ===================================

// Maximum limits
#define MN_MAX_RECORDING_EVENTS    10000                    // Max timing events per recording
#define MN_MAX_RECORDING_DURATION_MS (5 * 60 * 1000)       // 5 minutes
#define MN_MAX_RECORDINGS          200                      // Max recordings in library
#define MN_WARNING_TIME_MS         (4 * 60 * 1000 + 30 * 1000)  // Warning at 4:30

// File paths
#define MN_DIR                     "/morse-notes"
#define MN_LIBRARY_FILE            "/morse-notes/library.json"
#define MN_LIBRARY_TMP_FILE        "/morse-notes/library.tmp"

// Binary file format constants
#define MN_FILE_MAGIC              0x4D524E54  // "MRNT" (Morse Record Note Timing)
#define MN_FILE_VERSION            0x0001
#define MN_FILE_HEADER_SIZE        28

// Recording state machine
enum MorseNotesRecordState {
    MN_REC_IDLE,          // Not recording, initial state
    MN_REC_READY,         // Setup complete, ready to start
    MN_REC_RECORDING,     // Active recording in progress
    MN_REC_PAUSED,        // Recording paused (optional for v1)
    MN_REC_COMPLETE,      // Recording finished, awaiting save
    MN_REC_SAVING,        // Writing to SD card
    MN_REC_ERROR          // Error occurred
};

// Playback state machine
enum MorseNotesPlaybackState {
    MN_PLAY_IDLE,         // Not playing
    MN_PLAY_LOADING,      // Loading .mr file from SD
    MN_PLAY_READY,        // Loaded and ready
    MN_PLAY_PLAYING,      // Active playback
    MN_PLAY_PAUSED,       // Paused (optional for v1)
    MN_PLAY_COMPLETE,     // Finished playing
    MN_PLAY_ERROR         // Load/playback error
};

// Metadata structure for library.json
struct MorseNoteMetadata {
    unsigned long id;              // Unique ID (Unix timestamp)
    char title[64];                // User-provided title
    unsigned long timestamp;       // Unix timestamp (seconds since epoch)
    unsigned long durationMs;      // Total recording duration in milliseconds
    int eventCount;                // Number of timing events
    float avgWPM;                  // Average WPM calculated from timing
    int toneFrequency;             // Tone frequency used (Hz)
    char tags[128];                // Comma-separated tags (future use)

    // Constructor to zero-initialize
    MorseNoteMetadata() {
        memset(this, 0, sizeof(MorseNoteMetadata));
    }
};

// Binary file header structure (28 bytes)
// Packed to ensure exact byte layout
struct __attribute__((packed)) MorseNoteFileHeader {
    uint32_t magic;                // 0x4D524E54 ("MRNT")
    uint16_t version;              // Format version (0x0001)
    uint16_t flags;                // Reserved flags (set to 0)
    uint32_t eventCount;           // Number of timing events
    uint32_t toneFrequency;        // Tone frequency in Hz
    uint64_t timestamp;            // Unix timestamp
    float avgWPM;                  // Average WPM
};

// Recording session state
struct MorseNotesRecordingSession {
    MorseNotesRecordState state;
    float* timingBuffer;           // Pointer to timing array
    int eventCount;                // Current number of events
    unsigned long startTime;       // Recording start time (millis())
    unsigned long lastEventTime;   // Last key event time (millis())
    bool keyState;                 // Current key state (down=true)
    char title[64];                // Recording title

    MorseNotesRecordingSession() {
        state = MN_REC_IDLE;
        timingBuffer = nullptr;
        eventCount = 0;
        startTime = 0;
        lastEventTime = 0;
        keyState = false;
        memset(title, 0, sizeof(title));
    }
};

// Playback session state
struct MorseNotesPlaybackSession {
    MorseNotesPlaybackState state;
    float* timingBuffer;           // Pointer to timing array
    int eventCount;                // Total number of events
    int currentIndex;              // Current playback index
    unsigned long startTime;       // Playback start time (millis())
    float speed;                   // Playback speed (0.5x - 2.0x)
    int toneFrequency;             // Tone frequency for playback
    MorseNoteMetadata* metadata;   // Pointer to current recording metadata

    MorseNotesPlaybackSession() {
        state = MN_PLAY_IDLE;
        timingBuffer = nullptr;
        eventCount = 0;
        currentIndex = 0;
        startTime = 0;
        speed = 1.0f;
        toneFrequency = 700;
        metadata = nullptr;
    }
};

// Speed control options (7 steps)
const float MN_SPEED_OPTIONS[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
const int MN_SPEED_COUNT = 7;
const int MN_SPEED_DEFAULT_INDEX = 2;  // 1.0x

// Helper function to get next speed index
inline int mnGetNextSpeedIndex(int currentIndex, bool increment) {
    if (increment) {
        return (currentIndex + 1) % MN_SPEED_COUNT;
    } else {
        return (currentIndex - 1 + MN_SPEED_COUNT) % MN_SPEED_COUNT;
    }
}

// Helper function to format speed as string
inline void mnFormatSpeed(float speed, char* buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize, "%.2fx", speed);
}

// Helper function to format duration as MM:SS
inline void mnFormatDuration(unsigned long durationMs, char* buffer, size_t bufferSize) {
    int mins = durationMs / 60000;
    int secs = (durationMs / 1000) % 60;
    snprintf(buffer, bufferSize, "%02d:%02d", mins, secs);
}

// Helper function to calculate total duration from timing array
inline unsigned long mnCalculateDuration(const float* timings, int count) {
    float total = 0.0f;
    for (int i = 0; i < count; i++) {
        total += abs(timings[i]);
    }
    return (unsigned long)total;
}

#endif // MORSE_NOTES_TYPES_H
