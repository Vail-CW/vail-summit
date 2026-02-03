#ifndef MORSE_NOTES_WAV_EXPORT_H
#define MORSE_NOTES_WAV_EXPORT_H

#include "morse_notes_types.h"
#include "morse_notes_storage.h"
#include <SD.h>
#include <math.h>

// ===================================
// MORSE NOTES - WAV FILE EXPORT
// ===================================

// WAV format constants
#define WAV_SAMPLE_RATE    22050
#define WAV_BIT_DEPTH      16
#define WAV_CHANNELS       1
#define WAV_HEADER_SIZE    44

// Math constants
#ifndef PI
#define PI 3.14159265358979323846
#endif

// ===================================
// WAV HEADER STRUCTURE
// ===================================

struct __attribute__((packed)) WAVHeader {
    // RIFF chunk
    char riffID[4];              // "RIFF"
    uint32_t riffSize;           // File size - 8
    char waveID[4];              // "WAVE"

    // fmt subchunk
    char fmtID[4];               // "fmt "
    uint32_t fmtSize;            // 16 for PCM
    uint16_t audioFormat;        // 1 for PCM
    uint16_t numChannels;        // 1 for mono
    uint32_t sampleRate;         // 22050 Hz
    uint32_t byteRate;           // sampleRate * channels * (bitDepth/8)
    uint16_t blockAlign;         // channels * (bitDepth/8)
    uint16_t bitsPerSample;      // 16

    // data subchunk
    char dataID[4];              // "data"
    uint32_t dataSize;           // Number of bytes of PCM data
};

// ===================================
// WAV GENERATION
// ===================================

/**
 * Generate WAV file from timing array
 * Returns temp filename on success, empty string on failure
 */
String mnGenerateWAV(unsigned long recordingId) {
    // Load recording
    float timings[MN_MAX_RECORDING_EVENTS];
    int eventCount;
    int toneFreq;
    MorseNoteMetadata* metadata;

    bool success = mnLoadRecording(
        recordingId,
        timings,
        MN_MAX_RECORDING_EVENTS,
        eventCount,
        toneFreq,
        &metadata
    );

    if (!success || metadata == nullptr) {
        Serial.println("[MorseNotes] ERROR: Failed to load recording for WAV export");
        return "";
    }

    // Calculate total duration
    float totalDurationMs = 0.0f;
    for (int i = 0; i < eventCount; i++) {
        totalDurationMs += abs(timings[i]);
    }

    // Calculate sample count
    int totalSamples = (int)((totalDurationMs / 1000.0f) * WAV_SAMPLE_RATE);
    uint32_t dataSize = totalSamples * sizeof(int16_t);

    // Create temp filename
    char tempFilename[64];
    snprintf(tempFilename, sizeof(tempFilename), "%s/temp_%lu.wav", MN_DIR, millis());

    // Open temp file
    File file = SD.open(tempFilename, FILE_WRITE);
    if (!file) {
        Serial.println("[MorseNotes] ERROR: Failed to create temp WAV file");
        return "";
    }

    Serial.printf("[MorseNotes] Generating WAV: %d samples, %d events\n",
                  totalSamples, eventCount);

    // Create and write WAV header
    WAVHeader header;
    memcpy(header.riffID, "RIFF", 4);
    header.riffSize = 36 + dataSize;
    memcpy(header.waveID, "WAVE", 4);

    memcpy(header.fmtID, "fmt ", 4);
    header.fmtSize = 16;
    header.audioFormat = 1;  // PCM
    header.numChannels = WAV_CHANNELS;
    header.sampleRate = WAV_SAMPLE_RATE;
    header.byteRate = WAV_SAMPLE_RATE * WAV_CHANNELS * (WAV_BIT_DEPTH / 8);
    header.blockAlign = WAV_CHANNELS * (WAV_BIT_DEPTH / 8);
    header.bitsPerSample = WAV_BIT_DEPTH;

    memcpy(header.dataID, "data", 4);
    header.dataSize = dataSize;

    file.write((uint8_t*)&header, sizeof(header));

    // Generate audio samples
    float phase = 0.0f;
    float phaseIncrement = (2.0f * PI * (float)toneFreq) / (float)WAV_SAMPLE_RATE;
    const int16_t amplitude = 16384;  // Half of max int16_t for headroom

    int sampleIndex = 0;
    bool toneOn = false;

    // Buffer for writing samples in chunks
    const int BUFFER_SIZE = 256;
    int16_t buffer[BUFFER_SIZE];
    int bufferIndex = 0;

    for (int i = 0; i < eventCount; i++) {
        float duration = abs(timings[i]);
        toneOn = (timings[i] > 0.0f);  // Positive = tone on

        int samples = (int)((duration / 1000.0f) * WAV_SAMPLE_RATE);

        for (int j = 0; j < samples && sampleIndex < totalSamples; j++) {
            int16_t sample;

            if (toneOn) {
                // Generate sine wave
                sample = (int16_t)(amplitude * sin(phase));
                phase += phaseIncrement;
                if (phase >= 2.0f * PI) {
                    phase -= 2.0f * PI;
                }
            } else {
                // Silence
                sample = 0;
            }

            // Add to buffer
            buffer[bufferIndex++] = sample;

            // Flush buffer when full
            if (bufferIndex >= BUFFER_SIZE) {
                file.write((uint8_t*)buffer, bufferIndex * sizeof(int16_t));
                bufferIndex = 0;
            }

            sampleIndex++;
        }

        // Yield periodically to keep system responsive
        if (i % 100 == 0) {
            yield();
        }
    }

    // Flush remaining buffer
    if (bufferIndex > 0) {
        file.write((uint8_t*)buffer, bufferIndex * sizeof(int16_t));
    }

    file.close();

    Serial.printf("[MorseNotes] WAV generated: %s\n", tempFilename);
    return String(tempFilename);
}

/**
 * Delete temp WAV file
 */
bool mnDeleteTempWAV(const String& filename) {
    if (filename.length() == 0) {
        return false;
    }

    if (SD.exists(filename.c_str())) {
        bool success = SD.remove(filename.c_str());
        if (success) {
            Serial.printf("[MorseNotes] Deleted temp WAV: %s\n", filename.c_str());
        }
        return success;
    }

    return false;
}

/**
 * Get WAV file size estimate (without generating)
 */
uint32_t mnEstimateWAVSize(unsigned long recordingId) {
    MorseNoteMetadata* metadata = mnGetMetadata(recordingId);
    if (metadata == nullptr) {
        return 0;
    }

    // Calculate samples
    int totalSamples = (int)((metadata->durationMs / 1000.0f) * WAV_SAMPLE_RATE);
    uint32_t dataSize = totalSamples * sizeof(int16_t);

    return WAV_HEADER_SIZE + dataSize;
}

#endif // MORSE_NOTES_WAV_EXPORT_H
