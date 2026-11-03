/*
 * Web API - Storage Management Endpoints
 * REST API for SD card file operations
 */

#ifndef WEB_API_STORAGE_H
#define WEB_API_STORAGE_H

#include <ESPAsyncWebServer.h>
#include <SD.h>
#include "../../storage/sd_card.h"

extern bool checkWebAuth(AsyncWebServerRequest *request);

#define WEB_STORAGE_PATH_MAX 128

// SD mount requested by the storage page (initSDCard() is not safe to run
// from the async web server task; the main loop consumes this flag)
volatile bool webSDInitPending = false;

void processWebSDInitRequest() {
  if (!webSDInitPending) return;
  webSDInitPending = false;
  if (!sdCardAvailable) initSDCard();
}

/*
 * Path validation for request parameters.
 * Accepts absolute paths only, no "..", no backslashes, no empty
 * components and no control characters.
 */
static bool isSafeSDPath(const char* path) {
  if (path == nullptr || path[0] != '/') return false;
  size_t len = strlen(path);
  if (len >= WEB_STORAGE_PATH_MAX) return false;
  if (strstr(path, "..") != nullptr) return false;
  if (strstr(path, "//") != nullptr) return false;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)path[i];
    if (c < 0x20 || c == 0x7F || c == '\\' || c == ':' || c == '"') return false;
  }
  return true;
}

// The storage page manages files in the card root only. Writes and deletes
// are limited to "/<name>" so the web files (/www), Morse Notes, QSO logs
// and other data folders can't be overwritten or removed from here.
static bool isRootFilePath(const char* path) {
  return isSafeSDPath(path) && path[1] != '\0' && strchr(path + 1, '/') == nullptr;
}

// Get SD card status
void handleGetStorageStatus(AsyncWebServerRequest *request) {
  if (!checkWebAuth(request)) return;

  // Mount on first access: the main loop does it, the page refreshes later
  if (!sdCardAvailable) {
    webSDInitPending = true;
  }

  updateSDCardStats();

  char json[160];
  snprintf(json, sizeof(json),
           "{\"available\":%s,\"totalMB\":%lu,\"usedMB\":%lu,\"freeMB\":%lu}",
           sdCardAvailable ? "true" : "false",
           (unsigned long)sdCardSize,
           (unsigned long)sdCardUsed,
           (unsigned long)(sdCardSize - sdCardUsed));

  request->send(200, "application/json", json);
}

// List files in SD card root or specific directory
void handleListFiles(AsyncWebServerRequest *request) {
  if (!checkWebAuth(request)) return;

  if (!sdCardAvailable) {
    request->send(503, "application/json", "{\"error\":\"SD card not available\"}");
    return;
  }

  const char* path = "/";
  if (request->hasParam("path")) {
    path = request->getParam("path")->value().c_str();
  }

  if (!isSafeSDPath(path)) {
    request->send(400, "application/json", "{\"error\":\"Invalid path\"}");
    return;
  }

  String fileList = listSDFiles(path);
  request->send(200, "application/json", fileList);
}

// Download file from SD card
void handleDownloadFile(AsyncWebServerRequest *request) {
  if (!checkWebAuth(request)) return;

  if (!sdCardAvailable) {
    request->send(503, "text/plain", "SD card not available");
    return;
  }

  if (!request->hasParam("file")) {
    request->send(400, "text/plain", "Missing file parameter");
    return;
  }

  const String& filepath = request->getParam("file")->value();

  if (!isSafeSDPath(filepath.c_str())) {
    request->send(400, "text/plain", "Invalid file path");
    return;
  }

  if (!fileExists(filepath.c_str())) {
    request->send(404, "text/plain", "File not found");
    return;
  }

  // Send file for download
  request->send(SD, filepath, "application/octet-stream", true);
}

// Delete file from SD card
void handleDeleteFile(AsyncWebServerRequest *request) {
  if (!checkWebAuth(request)) return;

  if (!sdCardAvailable) {
    request->send(503, "application/json", "{\"success\":false,\"error\":\"SD card not available\"}");
    return;
  }

  if (!request->hasParam("file")) {
    request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing file parameter\"}");
    return;
  }

  const char* filepath = request->getParam("file")->value().c_str();

  if (!isRootFilePath(filepath)) {
    request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid file path\"}");
    return;
  }

  if (deleteSDFile(filepath)) {
    updateSDCardStats();
    request->send(200, "application/json", "{\"success\":true}");
  } else {
    request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to delete file\"}");
  }
}

// Upload state (one upload at a time)
static File uploadFile;
static bool uploadAccepted = false;    // auth + path checks passed for the current upload
static char uploadPath[WEB_STORAGE_PATH_MAX];

// Upload file to SD card
void handleUploadFile(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  // Start of upload: authenticate and validate once
  if (index == 0) {
    if (uploadFile) uploadFile.close();
    uploadAccepted = false;
    uploadPath[0] = '\0';

    if (!checkWebAuth(request)) return;

    if (!sdCardAvailable) {
      request->send(503, "application/json", "{\"success\":false,\"error\":\"SD card not available\"}");
      return;
    }

    // Root-level files only: the name may not carry its own directories
    const char* name = filename.c_str();
    if (name[0] == '/') name++;
    snprintf(uploadPath, sizeof(uploadPath), "/%s", name);
    if (strlen(name) == 0 || strlen(name) + 1 >= sizeof(uploadPath) || !isRootFilePath(uploadPath)) {
      uploadPath[0] = '\0';
      request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid file name\"}");
      return;
    }

    Serial.printf("Upload Start: %s\n", uploadPath);

    uploadFile = SD.open(uploadPath, FILE_WRITE);
    if (!uploadFile) {
      Serial.println("Failed to open file for upload");
      request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to create file\"}");
      return;
    }
    uploadAccepted = true;
  }

  if (!uploadAccepted) return;

  // Write chunk to file
  if (uploadFile && len > 0) {
    uploadFile.write(data, len);
  }

  // End of upload
  if (final) {
    if (uploadFile) {
      uploadFile.close();
    }
    uploadAccepted = false;
    Serial.printf("Upload Complete: %s (%u bytes)\n", uploadPath, (unsigned)(index + len));
    updateSDCardStats();

    char resp[WEB_STORAGE_PATH_MAX + 48];
    snprintf(resp, sizeof(resp), "{\"success\":true,\"file\":\"%s\"}", uploadPath);
    request->send(200, "application/json", resp);
  }
}

// Register all storage API endpoints
void registerStorageAPI(AsyncWebServer* server) {
  // Get storage status
  server->on("/api/storage/status", HTTP_GET, handleGetStorageStatus);

  // List files
  server->on("/api/storage/files", HTTP_GET, handleListFiles);

  // Download file
  server->on("/api/storage/download", HTTP_GET, handleDownloadFile);

  // Delete file
  server->on("/api/storage/delete", HTTP_DELETE, handleDeleteFile);

  // Upload file
  server->on("/api/storage/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      // Called after the upload completes; the upload handler has already
      // queued the reply. Covers a POST without a file part.
      if (request->getResponse() != nullptr) return;  // upload already replied
      if (!checkWebAuth(request)) return;
      request->send(400, "application/json", "{\"success\":false,\"error\":\"No file in upload\"}");
    },
    handleUploadFile
  );
}

#endif // WEB_API_STORAGE_H
