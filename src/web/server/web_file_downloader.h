/*
 * Web File Downloader
 * Downloads web interface files from GitHub to SD card
 */

#ifndef WEB_FILE_DOWNLOADER_H
#define WEB_FILE_DOWNLOADER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <Preferences.h>
#include "../../core/config.h"
#include "../../storage/sd_card.h"

// ============================================
// Early Boot Download Mode
// ============================================
// Due to memory constraints, SSL downloads must happen
// before LVGL is initialized. We use a reboot approach:
// 1. Set flag and reboot
// 2. On boot, check flag before LVGL init
// 3. Download files with plenty of RAM
// 4. Clear flag and reboot to normal mode

#define WEB_DOWNLOAD_PREF_NAMESPACE "webdl"
#define WEB_DOWNLOAD_PREF_PENDING "pending"

/**
 * Check if a web download is pending (call early in setup, before LVGL)
 */
bool isWebDownloadPending() {
  Preferences prefs;
  prefs.begin(WEB_DOWNLOAD_PREF_NAMESPACE, true);  // read-only
  bool pending = prefs.getBool(WEB_DOWNLOAD_PREF_PENDING, false);
  prefs.end();
  return pending;
}

/**
 * Request a web files download on next boot
 * Sets flag and reboots the device
 */
void requestWebDownloadAndReboot() {
  Serial.println("[WebDownload] Setting download pending flag and rebooting...");
  Preferences prefs;
  prefs.begin(WEB_DOWNLOAD_PREF_NAMESPACE, false);  // read-write
  prefs.putBool(WEB_DOWNLOAD_PREF_PENDING, true);
  prefs.end();
  delay(100);
  ESP.restart();
}

/**
 * Clear the web download pending flag
 */
void clearWebDownloadPending() {
  Preferences prefs;
  prefs.begin(WEB_DOWNLOAD_PREF_NAMESPACE, false);
  prefs.putBool(WEB_DOWNLOAD_PREF_PENDING, false);
  prefs.end();
}

/**
 * Perform web files download early in boot (before LVGL)
 * This runs when plenty of RAM is available
 * @param ssid WiFi SSID to connect to
 * @param password WiFi password
 * @return true if download successful
 */
bool performEarlyBootWebDownload(const char* ssid, const char* password) {
  Serial.println("\n========================================");
  Serial.println("EARLY BOOT WEB DOWNLOAD MODE");
  Serial.println("========================================\n");

  Serial.printf("Free heap: %d bytes, max block: %d bytes\n",
    ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  // Connect to WiFi
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed!");
    return false;
  }
  Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());

  // Initialize SD card
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card init failed!");
    return false;
  }

  Serial.printf("After WiFi+SD - heap: %d, max block: %d\n",
    ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  // Now do the SSL download
  String manifestUrl = String(WEB_FILES_BASE_URL) + WEB_FILES_MANIFEST;
  Serial.printf("Fetching: %s\n", manifestUrl.c_str());

  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  secureClient.setHandshakeTimeout(30);

  Serial.println("Connecting to GitHub (SSL)...");
  if (!secureClient.connect("raw.githubusercontent.com", 443)) {
    Serial.println("SSL connection failed!");
    return false;
  }
  Serial.println("SSL connected!");

  // Send request
  String path = manifestUrl.substring(manifestUrl.indexOf("/", 8));
  secureClient.printf("GET %s HTTP/1.1\r\n", path.c_str());
  secureClient.println("Host: raw.githubusercontent.com");
  secureClient.println("User-Agent: ESP32");
  secureClient.println("Connection: close");
  secureClient.println();

  // Read response
  unsigned long timeout = millis() + 10000;
  while (secureClient.connected() && !secureClient.available() && millis() < timeout) {
    delay(10);
  }

  String statusLine = secureClient.readStringUntil('\n');
  Serial.printf("HTTP Status: %s\n", statusLine.c_str());

  // Skip headers
  while (secureClient.connected()) {
    String line = secureClient.readStringUntil('\n');
    if (line == "\r" || line == "") break;
  }

  String manifestJson = secureClient.readString();
  secureClient.stop();

  Serial.printf("Received %d bytes\n", manifestJson.length());

  // Parse manifest
  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, manifestJson);
  if (error) {
    Serial.printf("JSON parse error: %s\n", error.c_str());
    return false;
  }

  const char* version = doc["version"] | "unknown";
  Serial.printf("Remote version: %s\n", version);

  // Download each file
  JsonArray files = doc["files"].as<JsonArray>();
  int fileCount = files.size();
  int downloaded = 0;

  // Create www directory if needed
  if (!SD.exists("/www")) {
    SD.mkdir("/www");
  }

  for (JsonObject fileObj : files) {
    const char* fileName = fileObj["name"] | "";
    if (strlen(fileName) == 0) continue;

    String fileUrl = String(WEB_FILES_BASE_URL) + fileName;
    String sdPath = String("/www/") + fileName;

    Serial.printf("Downloading %d/%d: %s\n", downloaded + 1, fileCount, fileName);

    WiFiClientSecure fileClient;
    fileClient.setInsecure();

    if (!fileClient.connect("raw.githubusercontent.com", 443)) {
      Serial.printf("  Failed to connect for %s\n", fileName);
      continue;
    }

    String filePath = fileUrl.substring(fileUrl.indexOf("/", 8));
    fileClient.printf("GET %s HTTP/1.1\r\n", filePath.c_str());
    fileClient.println("Host: raw.githubusercontent.com");
    fileClient.println("User-Agent: ESP32");
    fileClient.println("Connection: close");
    fileClient.println();

    // Wait for response
    timeout = millis() + 30000;
    while (fileClient.connected() && !fileClient.available() && millis() < timeout) {
      delay(10);
    }

    // Skip status and headers
    fileClient.readStringUntil('\n');  // status
    while (fileClient.connected()) {
      String line = fileClient.readStringUntil('\n');
      if (line == "\r" || line == "") break;
    }

    // Save to SD
    File outFile = SD.open(sdPath.c_str(), FILE_WRITE);
    if (outFile) {
      while (fileClient.connected() || fileClient.available()) {
        if (fileClient.available()) {
          uint8_t buf[512];
          int len = fileClient.read(buf, sizeof(buf));
          if (len > 0) {
            outFile.write(buf, len);
          }
        }
      }
      outFile.close();
      Serial.printf("  Saved: %s\n", sdPath.c_str());
      downloaded++;
    } else {
      Serial.printf("  Failed to create: %s\n", sdPath.c_str());
    }

    fileClient.stop();
  }

  // Save version
  File versionFile = SD.open("/www/version.txt", FILE_WRITE);
  if (versionFile) {
    versionFile.print(version);
    versionFile.close();
  }

  Serial.printf("\nDownload complete! %d/%d files\n", downloaded, fileCount);
  return downloaded > 0;
}

// ============================================
// Download State
// ============================================

enum WebDownloadState {
  DOWNLOAD_IDLE,
  DOWNLOAD_FETCHING_MANIFEST,
  DOWNLOAD_IN_PROGRESS,
  DOWNLOAD_COMPLETE,
  DOWNLOAD_ERROR
};

// Download progress tracking
struct WebDownloadProgress {
  WebDownloadState state;
  int totalFiles;
  int currentFile;
  int currentFileBytes;
  int currentFileTotal;
  String currentFileName;
  String errorMessage;
  bool cancelled;
};

WebDownloadProgress webDownloadProgress = {
  DOWNLOAD_IDLE, 0, 0, 0, 0, "", "", false
};

// ============================================
// Helper Functions
// ============================================

/**
 * Create directories recursively for a path
 * @param path Full path (e.g., "/www/css/styles.css")
 */
void createDirectoriesForPath(const char* path) {
  String pathStr = path;
  int lastSlash = pathStr.lastIndexOf('/');
  if (lastSlash <= 0) return;  // No subdirectories needed

  String dirPath = pathStr.substring(0, lastSlash);

  // Create each directory level
  int start = 1;  // Skip leading slash
  while (true) {
    int nextSlash = dirPath.indexOf('/', start);
    if (nextSlash == -1) {
      // Create final directory
      if (!SD.exists(dirPath.c_str())) {
        SD.mkdir(dirPath.c_str());
        Serial.printf("Created directory: %s\n", dirPath.c_str());
      }
      break;
    }

    String subDir = dirPath.substring(0, nextSlash);
    if (!SD.exists(subDir.c_str())) {
      SD.mkdir(subDir.c_str());
      Serial.printf("Created directory: %s\n", subDir.c_str());
    }
    start = nextSlash + 1;
  }
}

/**
 * Download a single file from URL to SD card
 * @param url Full URL to download
 * @param sdPath Path on SD card to save file
 * @return true if successful
 */
bool downloadFileToSD(const char* url, const char* sdPath) {
  Serial.printf("Downloading: %s -> %s\n", url, sdPath);

  // Create directories if needed
  createDirectoriesForPath(sdPath);

  WiFiClientSecure secureClient;
  secureClient.setInsecure();  // Skip certificate validation for GitHub CDN

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(secureClient, url);
  http.setTimeout(30000);

  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("HTTP error %d for %s\n", httpCode, url);
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  webDownloadProgress.currentFileTotal = contentLength;
  webDownloadProgress.currentFileBytes = 0;

  // Open file for writing
  File file = SD.open(sdPath, FILE_WRITE);
  if (!file) {
    Serial.printf("Failed to create file: %s\n", sdPath);
    http.end();
    return false;
  }

  // Get stream and download in chunks
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[512];
  int bytesRead = 0;

  while (http.connected() && (contentLength > 0 || contentLength == -1)) {
    size_t available = stream->available();
    if (available) {
      int toRead = min((int)available, (int)sizeof(buffer));
      int c = stream->readBytes(buffer, toRead);
      file.write(buffer, c);

      bytesRead += c;
      webDownloadProgress.currentFileBytes = bytesRead;

      if (contentLength > 0) {
        contentLength -= c;
      }
    }

    // Check for cancellation
    if (webDownloadProgress.cancelled) {
      Serial.println("Download cancelled by user");
      file.close();
      SD.remove(sdPath);  // Clean up partial file
      http.end();
      return false;
    }

    yield();  // Allow other tasks to run
  }

  file.close();
  http.end();

  Serial.printf("Downloaded %d bytes to %s\n", bytesRead, sdPath);
  return true;
}

// ============================================
// Main Download Functions
// ============================================

/**
 * Download web files from GitHub using manifest
 * @return true if all files downloaded successfully
 */
bool downloadWebFilesFromGitHub() {
  // Reset progress
  webDownloadProgress.state = DOWNLOAD_FETCHING_MANIFEST;
  webDownloadProgress.totalFiles = 0;
  webDownloadProgress.currentFile = 0;
  webDownloadProgress.currentFileName = "manifest.json";
  webDownloadProgress.errorMessage = "";
  webDownloadProgress.cancelled = false;

  // Check WiFi
  if (WiFi.status() != WL_CONNECTED) {
    webDownloadProgress.state = DOWNLOAD_ERROR;
    webDownloadProgress.errorMessage = "No WiFi connection";
    return false;
  }

  // Check SD card
  if (!sdCardAvailable) {
    if (!initSDCard()) {
      webDownloadProgress.state = DOWNLOAD_ERROR;
      webDownloadProgress.errorMessage = "SD card not available";
      return false;
    }
  }

  // Create /www directory if it doesn't exist
  if (!SD.exists(WEB_FILES_PATH)) {
    SD.mkdir(WEB_FILES_PATH);
    Serial.printf("Created directory: %s\n", WEB_FILES_PATH);
  }

  // Build manifest URL
  String manifestUrl = String(WEB_FILES_BASE_URL) + WEB_FILES_MANIFEST;
  Serial.printf("Fetching manifest: %s\n", manifestUrl.c_str());

  WiFiClientSecure secureClient;
  secureClient.setInsecure();  // Skip certificate validation for GitHub CDN

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(secureClient, manifestUrl)) {
    webDownloadProgress.state = DOWNLOAD_ERROR;
    webDownloadProgress.errorMessage = "Failed to connect to GitHub";
    return false;
  }
  http.setTimeout(15000);

  int httpCode = http.GET();

  if (httpCode != 200) {
    webDownloadProgress.state = DOWNLOAD_ERROR;
    webDownloadProgress.errorMessage = "Failed to fetch manifest (HTTP " + String(httpCode) + ")";
    http.end();
    return false;
  }

  String manifestJson = http.getString();
  http.end();

  // Parse manifest
  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, manifestJson);

  if (error) {
    webDownloadProgress.state = DOWNLOAD_ERROR;
    webDownloadProgress.errorMessage = "Failed to parse manifest: " + String(error.c_str());
    return false;
  }

  // Get file list from manifest
  JsonArray files = doc["files"].as<JsonArray>();
  webDownloadProgress.totalFiles = files.size();
  webDownloadProgress.state = DOWNLOAD_IN_PROGRESS;

  Serial.printf("Manifest contains %d files\n", webDownloadProgress.totalFiles);

  // Download each file
  int fileIndex = 0;
  for (JsonObject fileObj : files) {
    if (webDownloadProgress.cancelled) {
      webDownloadProgress.state = DOWNLOAD_ERROR;
      webDownloadProgress.errorMessage = "Download cancelled";
      return false;
    }

    const char* fileName = fileObj["name"];
    webDownloadProgress.currentFile = fileIndex + 1;
    webDownloadProgress.currentFileName = fileName;

    // Build URLs/paths
    String fileUrl = String(WEB_FILES_BASE_URL) + fileName;
    String sdPath = String(WEB_FILES_PATH) + fileName;

    if (!downloadFileToSD(fileUrl.c_str(), sdPath.c_str())) {
      webDownloadProgress.state = DOWNLOAD_ERROR;
      webDownloadProgress.errorMessage = "Failed to download: " + String(fileName);
      return false;
    }

    fileIndex++;
  }

  // Save manifest version info
  String versionPath = String(WEB_FILES_PATH) + "version.txt";
  const char* version = doc["version"] | "unknown";
  writeSDFile(versionPath.c_str(), version);

  webDownloadProgress.state = DOWNLOAD_COMPLETE;
  Serial.println("Web files download complete!");

  // Update SD card stats
  updateSDCardStats();

  return true;
}

/**
 * Cancel ongoing download
 */
void cancelWebFileDownload() {
  webDownloadProgress.cancelled = true;
}

/**
 * Get current download progress as JSON
 */
String getWebDownloadProgressJson() {
  StaticJsonDocument<512> doc;

  doc["state"] = (int)webDownloadProgress.state;
  doc["totalFiles"] = webDownloadProgress.totalFiles;
  doc["currentFile"] = webDownloadProgress.currentFile;
  doc["currentFileName"] = webDownloadProgress.currentFileName;
  doc["currentFileBytes"] = webDownloadProgress.currentFileBytes;
  doc["currentFileTotal"] = webDownloadProgress.currentFileTotal;
  doc["error"] = webDownloadProgress.errorMessage;

  // Calculate overall progress percentage
  int overallProgress = 0;
  if (webDownloadProgress.totalFiles > 0) {
    overallProgress = (webDownloadProgress.currentFile * 100) / webDownloadProgress.totalFiles;
  }
  doc["overallProgress"] = overallProgress;

  String output;
  serializeJson(doc, output);
  return output;
}

/**
 * Check if web files exist on SD card
 * @return true if /www/index.html exists
 */
bool webFilesExist() {
  if (!sdCardAvailable) {
    // Try to init SD card first
    if (!initSDCard()) {
      return false;
    }
  }

  String indexPath = String(WEB_FILES_PATH) + "index.html";
  return SD.exists(indexPath.c_str());
}

/**
 * Get web files version from SD card
 * @return Version string or empty if not found
 */
String getWebFilesVersion() {
  if (!sdCardAvailable) return "";

  String versionPath = String(WEB_FILES_PATH) + "version.txt";
  if (!SD.exists(versionPath.c_str())) return "";

  return readSDFile(versionPath.c_str());
}

// Cached remote version to avoid multiple HTTP requests per session
static String cachedRemoteVersion = "";
static bool remoteVersionFetched = false;

/**
 * Fetch the latest web files version from GitHub manifest
 * Caches the result to avoid multiple HTTP requests per session
 * @param forceRefresh If true, fetches fresh even if cached
 * @return Version string or empty if fetch failed
 */
String fetchRemoteWebFilesVersion(bool forceRefresh = false) {
  // Return cached version if available
  if (remoteVersionFetched && !forceRefresh) {
    return cachedRemoteVersion;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - cannot fetch remote version");
    return "";
  }

  String manifestUrl = String(WEB_FILES_BASE_URL) + WEB_FILES_MANIFEST;
  Serial.printf("Checking remote version: %s\n", manifestUrl.c_str());

  // Log memory stats before stopping web server
  Serial.printf("[WebDownload] Free heap: %d bytes, max block: %d bytes\n",
    ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  // Stop web server to free up RAM for SSL
  extern AsyncWebServer webServer;
  Serial.println("[WebDownload] Stopping web server to free RAM...");
  webServer.end();
  delay(100);  // Let memory consolidate

  // Log memory after stopping web server
  Serial.printf("[WebDownload] After stopping server - heap: %d, max block: %d\n",
    ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  // Allocate client on heap
  WiFiClientSecure* secureClient = new WiFiClientSecure();
  if (!secureClient) {
    Serial.println("[WebDownload] Failed to allocate WiFiClientSecure!");
    Serial.println("[WebDownload] Restarting web server...");
    webServer.begin();
    remoteVersionFetched = true;
    cachedRemoteVersion = "";
    return "";
  }

  secureClient->setInsecure();
  secureClient->setHandshakeTimeout(30);

  Serial.println("[WebDownload] Connecting to raw.githubusercontent.com:443...");
  if (!secureClient->connect("raw.githubusercontent.com", 443)) {
    char errBuf[256];
    int err = secureClient->lastError(errBuf, sizeof(errBuf));
    Serial.printf("[WebDownload] SSL FAILED! Error %d: %s\n", err, errBuf);
    Serial.printf("[WebDownload] Free heap after fail: %d\n", ESP.getFreeHeap());
    delete secureClient;
    // Restart web server
    Serial.println("[WebDownload] Restarting web server...");
    webServer.begin();
    remoteVersionFetched = true;
    cachedRemoteVersion = "";
    return "";
  }
  Serial.println("[WebDownload] SSL connection OK!");

  // Build HTTP GET request manually
  String path = manifestUrl.substring(manifestUrl.indexOf("/", 8));
  secureClient->printf("GET %s HTTP/1.1\r\n", path.c_str());
  secureClient->println("Host: raw.githubusercontent.com");
  secureClient->println("User-Agent: ESP32");
  secureClient->println("Connection: close");
  secureClient->println();

  // Wait for response
  unsigned long timeout = millis() + 10000;
  while (secureClient->connected() && !secureClient->available() && millis() < timeout) {
    delay(10);
  }

  // Read HTTP status line
  String statusLine = secureClient->readStringUntil('\n');
  Serial.printf("[WebDownload] Status: %s\n", statusLine.c_str());

  int httpCode = 0;
  if (statusLine.startsWith("HTTP/1.")) {
    httpCode = statusLine.substring(9, 12).toInt();
  }

  // Skip headers
  while (secureClient->connected()) {
    String line = secureClient->readStringUntil('\n');
    if (line == "\r" || line == "") break;
  }

  // Read body
  String manifestJson = secureClient->readString();
  secureClient->stop();
  delete secureClient;

  // Restart web server now that SSL is done
  Serial.println("[WebDownload] Restarting web server...");
  webServer.begin();

  Serial.printf("[WebDownload] Got %d bytes, HTTP %d\n", manifestJson.length(), httpCode);

  if (httpCode != 200) {
    Serial.printf("Failed to fetch manifest (HTTP %d)\n", httpCode);
    remoteVersionFetched = true;
    cachedRemoteVersion = "";
    return "";
  }

  // Parse just the version field
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, manifestJson);

  if (error) {
    Serial.printf("Failed to parse manifest: %s\n", error.c_str());
    remoteVersionFetched = true;
    cachedRemoteVersion = "";
    return "";
  }

  const char* version = doc["version"] | "";
  Serial.printf("Remote web files version: %s\n", version);

  // Cache the result
  cachedRemoteVersion = String(version);
  cachedRemoteVersion.trim();
  remoteVersionFetched = true;

  return cachedRemoteVersion;
}

/**
 * Check if web files need updating by comparing local and remote versions
 * @param outRemoteVersion If provided, stores the remote version string
 * @return true if update is available (remote version differs from local)
 */
bool isWebFilesUpdateAvailable(String* outRemoteVersion = nullptr) {
  String localVersion = getWebFilesVersion();
  if (localVersion.isEmpty()) {
    // No local version means files don't exist or version.txt missing
    return false;  // Let webFilesExist() handle missing files case
  }

  String remoteVersion = fetchRemoteWebFilesVersion();
  if (remoteVersion.isEmpty()) {
    // Couldn't fetch remote version, assume no update
    return false;
  }

  // Store remote version if caller wants it
  if (outRemoteVersion != nullptr) {
    *outRemoteVersion = remoteVersion;
  }

  // Trim whitespace for comparison
  localVersion.trim();

  bool needsUpdate = (localVersion != remoteVersion);
  if (needsUpdate) {
    Serial.printf("Web files update available: %s -> %s\n",
                  localVersion.c_str(), remoteVersion.c_str());
  }

  return needsUpdate;
}

/**
 * Get the cached remote version (call after isWebFilesUpdateAvailable)
 * @return Cached remote version or empty string
 */
String getCachedRemoteVersion() {
  return cachedRemoteVersion;
}

/**
 * Delete all web files from SD card
 * @return true if successful
 */
bool deleteWebFiles() {
  if (!sdCardAvailable) return false;

  // List and delete all files in /www/
  File root = SD.open(WEB_FILES_PATH);
  if (!root || !root.isDirectory()) {
    return false;
  }

  File file = root.openNextFile();
  while (file) {
    String path = String(WEB_FILES_PATH) + file.name();
    if (file.isDirectory()) {
      // Recursive delete for subdirectories
      File subdir = SD.open(path.c_str());
      if (subdir && subdir.isDirectory()) {
        File subfile = subdir.openNextFile();
        while (subfile) {
          String subpath = path + "/" + subfile.name();
          SD.remove(subpath.c_str());
          subfile = subdir.openNextFile();
        }
        subdir.close();
        SD.rmdir(path.c_str());
      }
    } else {
      SD.remove(path.c_str());
    }
    file = root.openNextFile();
  }
  root.close();

  // Remove the /www/ directory itself
  SD.rmdir(WEB_FILES_PATH);

  return true;
}

#endif // WEB_FILE_DOWNLOADER_H
