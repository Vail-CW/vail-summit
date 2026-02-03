#ifndef MORSE_NOTES_STORAGE_H
#define MORSE_NOTES_STORAGE_H

#include "morse_notes_types.h"
#include <SD.h>
#include <ArduinoJson.h>
#include <time.h>

// ===================================
// MORSE NOTES - STORAGE LAYER
// ===================================

// Global library state
static MorseNoteMetadata mnLibrary[MN_MAX_RECORDINGS];
static int mnLibraryCount = 0;
static bool mnLibraryLoaded = false;

// ===================================
// FILENAME GENERATION
// ===================================

/**
 * Generate filename from timestamp
 * Format: /morse-notes/YYYYMMDD_HHMMSS.mr
 */
void mnGenerateFilename(unsigned long timestamp, char* buffer, size_t bufferSize) {
    struct tm timeinfo;
    time_t ts = (time_t)timestamp;
    localtime_r(&ts, &timeinfo);

    snprintf(buffer, bufferSize, "%s/%04d%02d%02d_%02d%02d%02d.mr",
             MN_DIR,
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
}

/**
 * Generate default title from timestamp
 * Format: "Recording YYYYMMDD_HHMMSS"
 */
void mnGenerateDefaultTitle(unsigned long timestamp, char* buffer, size_t bufferSize) {
    struct tm timeinfo;
    time_t ts = (time_t)timestamp;
    localtime_r(&ts, &timeinfo);

    snprintf(buffer, bufferSize, "Recording %04d%02d%02d_%02d%02d%02d",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
}

// ===================================
// LIBRARY JSON MANAGEMENT
// ===================================

/**
 * Initialize Morse Notes directory
 */
bool mnInitStorage() {
    if (!SD.exists(MN_DIR)) {
        Serial.println("[MorseNotes] Creating directory: " MN_DIR);
        if (!SD.mkdir(MN_DIR)) {
            Serial.println("[MorseNotes] ERROR: Failed to create directory");
            return false;
        }
    }
    return true;
}

/**
 * Load library.json into memory
 */
bool mnLoadLibrary() {
    if (mnLibraryLoaded) {
        return true;  // Already loaded
    }

    // Initialize directory
    if (!mnInitStorage()) {
        return false;
    }

    // Check if library file exists
    if (!SD.exists(MN_LIBRARY_FILE)) {
        Serial.println("[MorseNotes] No library file, starting fresh");
        mnLibraryCount = 0;
        mnLibraryLoaded = true;
        return true;
    }

    // Open library file
    File file = SD.open(MN_LIBRARY_FILE, FILE_READ);
    if (!file) {
        Serial.println("[MorseNotes] ERROR: Failed to open library file");
        return false;
    }

    // Parse JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        Serial.printf("[MorseNotes] ERROR: JSON parse error: %s\n", err.c_str());
        return false;
    }

    // Load recordings
    JsonArray recordings = doc["recordings"];
    mnLibraryCount = 0;

    for (JsonObject rec : recordings) {
        if (mnLibraryCount >= MN_MAX_RECORDINGS) {
            Serial.println("[MorseNotes] WARNING: Max recordings reached");
            break;
        }

        MorseNoteMetadata& meta = mnLibrary[mnLibraryCount];
        meta.id = rec["id"] | 0;
        strlcpy(meta.title, rec["title"] | "Untitled", sizeof(meta.title));
        meta.timestamp = rec["timestamp"] | 0;
        meta.durationMs = rec["durationMs"] | 0;
        meta.eventCount = rec["eventCount"] | 0;
        meta.avgWPM = rec["avgWPM"] | 0.0f;
        meta.toneFrequency = rec["toneFrequency"] | 700;
        strlcpy(meta.tags, rec["tags"] | "", sizeof(meta.tags));

        mnLibraryCount++;
    }

    Serial.printf("[MorseNotes] Loaded %d recordings\n", mnLibraryCount);
    mnLibraryLoaded = true;
    return true;
}

/**
 * Save library to library.json (atomic write)
 */
bool mnSaveLibrary() {
    // Open temp file
    File file = SD.open(MN_LIBRARY_TMP_FILE, FILE_WRITE);
    if (!file) {
        Serial.println("[MorseNotes] ERROR: Failed to open temp file");
        return false;
    }

    // Create JSON document
    JsonDocument doc;
    doc["version"] = "1.0";
    doc["lastModified"] = (unsigned long)(millis() / 1000);

    JsonArray recordings = doc["recordings"].to<JsonArray>();

    for (int i = 0; i < mnLibraryCount; i++) {
        MorseNoteMetadata& meta = mnLibrary[i];
        JsonObject rec = recordings.add<JsonObject>();

        rec["id"] = meta.id;
        rec["title"] = meta.title;
        rec["timestamp"] = meta.timestamp;
        rec["durationMs"] = meta.durationMs;
        rec["eventCount"] = meta.eventCount;
        rec["avgWPM"] = meta.avgWPM;
        rec["toneFrequency"] = meta.toneFrequency;
        rec["tags"] = meta.tags;

        // Also include filename for web API convenience
        char filename[64];
        mnGenerateFilename(meta.timestamp, filename, sizeof(filename));
        rec["filename"] = filename + strlen(MN_DIR) + 1;  // Strip "/morse-notes/"
    }

    // Write JSON
    serializeJson(doc, file);
    file.close();

    // Atomic rename
    if (SD.exists(MN_LIBRARY_FILE)) {
        SD.remove(MN_LIBRARY_FILE);
    }
    if (!SD.rename(MN_LIBRARY_TMP_FILE, MN_LIBRARY_FILE)) {
        Serial.println("[MorseNotes] ERROR: Failed to rename temp file");
        return false;
    }

    Serial.println("[MorseNotes] Library saved successfully");
    return true;
}

// ===================================
// BINARY FILE I/O
// ===================================

/**
 * Save recording to binary .mr file
 * @param title User-provided title
 * @param timings Array of timing events
 * @param eventCount Number of events
 * @param toneFreq Tone frequency used
 * @param avgWPM Average WPM
 * @return true if successful
 */
bool mnSaveRecording(const char* title, const float* timings, int eventCount,
                     int toneFreq, float avgWPM) {
    // Ensure library is loaded
    if (!mnLoadLibrary()) {
        return false;
    }

    // Check if we have space
    if (mnLibraryCount >= MN_MAX_RECORDINGS) {
        Serial.println("[MorseNotes] ERROR: Library full");
        return false;
    }

    // Generate timestamp and filename
    time_t now = time(nullptr);
    char filename[64];
    mnGenerateFilename((unsigned long)now, filename, sizeof(filename));

    // Calculate duration
    unsigned long durationMs = mnCalculateDuration(timings, eventCount);

    // Open file
    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
        Serial.printf("[MorseNotes] ERROR: Failed to create file: %s\n", filename);
        return false;
    }

    // Write header
    MorseNoteFileHeader header;
    header.magic = MN_FILE_MAGIC;
    header.version = MN_FILE_VERSION;
    header.flags = 0;
    header.eventCount = (uint32_t)eventCount;
    header.toneFrequency = (uint32_t)toneFreq;
    header.timestamp = (uint64_t)now;
    header.avgWPM = avgWPM;

    file.write((uint8_t*)&header, sizeof(header));

    // Write timing array
    file.write((uint8_t*)timings, eventCount * sizeof(float));

    file.close();

    // Add to library
    MorseNoteMetadata& meta = mnLibrary[mnLibraryCount];
    meta.id = (unsigned long)now;
    strlcpy(meta.title, title, sizeof(meta.title));
    meta.timestamp = (unsigned long)now;
    meta.durationMs = durationMs;
    meta.eventCount = eventCount;
    meta.avgWPM = avgWPM;
    meta.toneFrequency = toneFreq;
    meta.tags[0] = '\0';

    mnLibraryCount++;

    // Save library
    if (!mnSaveLibrary()) {
        Serial.println("[MorseNotes] WARNING: Failed to update library");
    }

    Serial.printf("[MorseNotes] Saved recording: %s\n", filename);
    return true;
}

/**
 * Load recording from binary .mr file
 * @param id Recording ID (timestamp)
 * @param timings Output buffer for timing events (must be preallocated)
 * @param maxEvents Maximum events buffer can hold
 * @param eventCount Output: actual number of events loaded
 * @param toneFreq Output: tone frequency
 * @param metadata Output: pointer to metadata in library
 * @return true if successful
 */
bool mnLoadRecording(unsigned long id, float* timings, int maxEvents,
                     int& eventCount, int& toneFreq, MorseNoteMetadata** metadata) {
    // Find metadata
    *metadata = nullptr;
    for (int i = 0; i < mnLibraryCount; i++) {
        if (mnLibrary[i].id == id) {
            *metadata = &mnLibrary[i];
            break;
        }
    }

    if (*metadata == nullptr) {
        Serial.printf("[MorseNotes] ERROR: Recording not found: %lu\n", id);
        return false;
    }

    // Generate filename
    char filename[64];
    mnGenerateFilename((*metadata)->timestamp, filename, sizeof(filename));

    // Open file
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.printf("[MorseNotes] ERROR: Failed to open file: %s\n", filename);
        return false;
    }

    // Read and validate header
    MorseNoteFileHeader header;
    if (file.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
        Serial.println("[MorseNotes] ERROR: Failed to read header");
        file.close();
        return false;
    }

    if (header.magic != MN_FILE_MAGIC) {
        Serial.printf("[MorseNotes] ERROR: Invalid magic: 0x%08X\n", header.magic);
        file.close();
        return false;
    }

    if (header.version != MN_FILE_VERSION) {
        Serial.printf("[MorseNotes] WARNING: Version mismatch: 0x%04X\n", header.version);
    }

    // Check event count
    if (header.eventCount > (uint32_t)maxEvents) {
        Serial.printf("[MorseNotes] ERROR: Event count too large: %u > %d\n",
                      header.eventCount, maxEvents);
        file.close();
        return false;
    }

    // Read timing array
    eventCount = (int)header.eventCount;
    toneFreq = (int)header.toneFrequency;

    size_t bytesToRead = eventCount * sizeof(float);
    if (file.read((uint8_t*)timings, bytesToRead) != bytesToRead) {
        Serial.println("[MorseNotes] ERROR: Failed to read timing array");
        file.close();
        return false;
    }

    file.close();

    Serial.printf("[MorseNotes] Loaded %d events from: %s\n", eventCount, filename);
    return true;
}

// ===================================
// FILE OPERATIONS
// ===================================

/**
 * Delete recording
 */
bool mnDeleteRecording(unsigned long id) {
    // Find in library
    int index = -1;
    for (int i = 0; i < mnLibraryCount; i++) {
        if (mnLibrary[i].id == id) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        Serial.printf("[MorseNotes] ERROR: Recording not found: %lu\n", id);
        return false;
    }

    // Generate filename
    char filename[64];
    mnGenerateFilename(mnLibrary[index].timestamp, filename, sizeof(filename));

    // Delete file
    if (SD.exists(filename)) {
        if (!SD.remove(filename)) {
            Serial.printf("[MorseNotes] ERROR: Failed to delete file: %s\n", filename);
            return false;
        }
    }

    // Remove from library array
    for (int i = index; i < mnLibraryCount - 1; i++) {
        mnLibrary[i] = mnLibrary[i + 1];
    }
    mnLibraryCount--;

    // Save updated library
    if (!mnSaveLibrary()) {
        Serial.println("[MorseNotes] WARNING: Failed to update library");
    }

    Serial.printf("[MorseNotes] Deleted recording: %lu\n", id);
    return true;
}

/**
 * Rename recording (update title only)
 */
bool mnRenameRecording(unsigned long id, const char* newTitle) {
    for (int i = 0; i < mnLibraryCount; i++) {
        if (mnLibrary[i].id == id) {
            strlcpy(mnLibrary[i].title, newTitle, sizeof(mnLibrary[i].title));
            return mnSaveLibrary();
        }
    }

    Serial.printf("[MorseNotes] ERROR: Recording not found: %lu\n", id);
    return false;
}

/**
 * Get metadata by ID
 */
MorseNoteMetadata* mnGetMetadata(unsigned long id) {
    for (int i = 0; i < mnLibraryCount; i++) {
        if (mnLibrary[i].id == id) {
            return &mnLibrary[i];
        }
    }
    return nullptr;
}

/**
 * Get metadata by index
 */
MorseNoteMetadata* mnGetMetadataByIndex(int index) {
    if (index >= 0 && index < mnLibraryCount) {
        return &mnLibrary[index];
    }
    return nullptr;
}

/**
 * Get library count
 */
int mnGetLibraryCount() {
    return mnLibraryCount;
}

/**
 * Check SD card space
 * @param minFreeBytes Minimum free bytes required
 * @return true if enough space available
 */
bool mnCheckSpace(uint64_t minFreeBytes) {
    uint64_t freeSpace = SD.totalBytes() - SD.usedBytes();
    return freeSpace >= minFreeBytes;
}

#endif // MORSE_NOTES_STORAGE_H
