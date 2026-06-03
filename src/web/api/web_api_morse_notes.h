/*
 * Web API - Morse Notes Endpoints
 * REST API for Morse Notes recording management and WAV export
 */

#ifndef WEB_API_MORSE_NOTES_H
#define WEB_API_MORSE_NOTES_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "../../morse_notes/morse_notes_types.h"
#include "../../morse_notes/morse_notes_storage.h"
#include "../../morse_notes/morse_notes_wav_export.h"
#include "../../storage/sd_card.h"

// ===================================
// API HANDLERS
// ===================================

/**
 * GET /api/morse-notes/list
 * Returns JSON array of all Morse Notes recordings
 */
void handleGetMorseNotesList(AsyncWebServerRequest *request) {
    if (!sdCardAvailable) {
        request->send(503, "application/json", "{\"error\":\"SD card not available\"}");
        return;
    }

    // Load library
    if (!mnLoadLibrary()) {
        request->send(500, "application/json", "{\"error\":\"Failed to load library\"}");
        return;
    }

    // Build JSON response
    JsonDocument doc;
    JsonArray recordings = doc["recordings"].to<JsonArray>();

    for (int i = 0; i < mnLibraryCount; i++) {
        JsonObject rec = recordings.add<JsonObject>();
        rec["id"] = (unsigned long)mnLibrary[i].id;
        rec["title"] = String(mnLibrary[i].title);
        rec["timestamp"] = (unsigned long)mnLibrary[i].timestamp;
        rec["durationMs"] = (unsigned long)mnLibrary[i].durationMs;
        rec["eventCount"] = mnLibrary[i].eventCount;
        rec["avgWPM"] = mnLibrary[i].avgWPM;
        rec["toneFrequency"] = mnLibrary[i].toneFrequency;
        rec["tags"] = String(mnLibrary[i].tags);
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

/**
 * GET /api/morse-notes/metadata?id=X
 * Returns JSON metadata for a single recording
 */
void handleGetMorseNoteMetadata(AsyncWebServerRequest *request) {
    if (!sdCardAvailable) {
        request->send(503, "application/json", "{\"error\":\"SD card not available\"}");
        return;
    }

    if (!request->hasParam("id")) {
        request->send(400, "application/json", "{\"error\":\"Missing id parameter\"}");
        return;
    }

    unsigned long id = request->getParam("id")->value().toInt();

    // Load library
    if (!mnLoadLibrary()) {
        request->send(500, "application/json", "{\"error\":\"Failed to load library\"}");
        return;
    }

    // Find recording
    MorseNoteMetadata* metadata = nullptr;
    for (int i = 0; i < mnLibraryCount; i++) {
        if (mnLibrary[i].id == id) {
            metadata = &mnLibrary[i];
            break;
        }
    }

    if (!metadata) {
        request->send(404, "application/json", "{\"error\":\"Recording not found\"}");
        return;
    }

    // Build JSON response
    JsonDocument doc;
    doc["id"] = (unsigned long)metadata->id;
    doc["title"] = String(metadata->title);
    doc["timestamp"] = (unsigned long)metadata->timestamp;
    doc["durationMs"] = (unsigned long)metadata->durationMs;
    doc["eventCount"] = metadata->eventCount;
    doc["avgWPM"] = metadata->avgWPM;
    doc["toneFrequency"] = metadata->toneFrequency;
    doc["tags"] = String(metadata->tags);

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

/**
 * GET /api/morse-notes/download?id=X
 * Downloads raw .mr file
 */
void handleDownloadMorseNote(AsyncWebServerRequest *request) {
    if (!sdCardAvailable) {
        request->send(503, "text/plain", "SD card not available");
        return;
    }

    if (!request->hasParam("id")) {
        request->send(400, "text/plain", "Missing id parameter");
        return;
    }

    unsigned long id = request->getParam("id")->value().toInt();

    // Build filename
    char filename[64];
    snprintf(filename, sizeof(filename), MN_DIR "/%lu.mr", id);

    if (!fileExists(filename)) {
        request->send(404, "text/plain", "Recording not found");
        return;
    }

    // Send file
    request->send(SD, filename, "application/octet-stream", true);
}

/**
 * GET /api/morse-notes/export/wav?id=X
 * Exports recording as WAV file
 */
void handleExportMorseNoteWAV(AsyncWebServerRequest *request) {
    if (!sdCardAvailable) {
        request->send(503, "text/plain", "SD card not available");
        return;
    }

    if (!request->hasParam("id")) {
        request->send(400, "text/plain", "Missing id parameter");
        return;
    }

    unsigned long id = request->getParam("id")->value().toInt();

    // Generate WAV file
    String wavPath = mnGenerateWAV(id);

    if (wavPath.isEmpty()) {
        request->send(500, "text/plain", "Failed to generate WAV file");
        return;
    }

    // Send WAV file
    File wavFile = SD.open(wavPath.c_str(), FILE_READ);
    if (!wavFile) {
        request->send(500, "text/plain", "Failed to open WAV file");
        return;
    }

    // Create response with callback to delete temp file after sending
    AsyncWebServerResponse *response = request->beginResponse(
        SD,
        wavPath,
        "audio/wav",
        true  // download = true
    );

    // Set filename for download
    char filename[128];
    snprintf(filename, sizeof(filename), "attachment; filename=\"morse_note_%lu.wav\"", id);
    response->addHeader("Content-Disposition", filename);

    // Send response
    request->send(response);

    // Note: Temp file cleanup happens in mnGenerateWAV or can be done periodically
}

/**
 * DELETE /api/morse-notes/delete?id=X
 * Deletes a recording
 */
void handleDeleteMorseNote(AsyncWebServerRequest *request) {
    if (!sdCardAvailable) {
        request->send(503, "application/json", "{\"success\":false,\"error\":\"SD card not available\"}");
        return;
    }

    if (!request->hasParam("id")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing id parameter\"}");
        return;
    }

    unsigned long id = request->getParam("id")->value().toInt();

    if (mnDeleteRecording(id)) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to delete recording\"}");
    }
}

/**
 * PUT /api/morse-notes/update?id=X&title=Y
 * Updates recording title
 */
void handleUpdateMorseNote(AsyncWebServerRequest *request) {
    if (!sdCardAvailable) {
        request->send(503, "application/json", "{\"success\":false,\"error\":\"SD card not available\"}");
        return;
    }

    if (!request->hasParam("id") || !request->hasParam("title")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing id or title parameter\"}");
        return;
    }

    unsigned long id = request->getParam("id")->value().toInt();
    String title = request->getParam("title")->value();

    if (mnRenameRecording(id, title.c_str())) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to update recording\"}");
    }
}

// ===================================
// REGISTRATION
// ===================================

/**
 * Register all Morse Notes API endpoints
 */
void registerMorseNotesAPI(AsyncWebServer* server) {
    // List all recordings
    server->on("/api/morse-notes/list", HTTP_GET, handleGetMorseNotesList);

    // Get single recording metadata
    server->on("/api/morse-notes/metadata", HTTP_GET, handleGetMorseNoteMetadata);

    // Download raw .mr file
    server->on("/api/morse-notes/download", HTTP_GET, handleDownloadMorseNote);

    // Export as WAV
    server->on("/api/morse-notes/export/wav", HTTP_GET, handleExportMorseNoteWAV);

    // Delete recording
    server->on("/api/morse-notes/delete", HTTP_DELETE, handleDeleteMorseNote);

    // Update recording title
    server->on("/api/morse-notes/update", HTTP_PUT, handleUpdateMorseNote);

    Serial.println("[WebAPI] Morse Notes API registered");
}

#endif // WEB_API_MORSE_NOTES_H
