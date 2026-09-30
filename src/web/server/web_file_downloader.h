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

// Progress callback for early boot download display updates
// Parameters: status message, current file number, total file count
typedef void (*EarlyBootProgressCallback)(const char* status, int currentFile, int totalFiles);

#define WEB_DOWNLOAD_PREF_NAMESPACE "webdl"
#define WEB_DOWNLOAD_PREF_PENDING "pending"
// Snapshot of the live connection taken when the download is requested, so
// early boot can rejoin exactly what was working (independent of which saved
// slot it lives in, and pinned to the same access point / channel).
#define WEB_DOWNLOAD_PREF_SSID "ssid"
#define WEB_DOWNLOAD_PREF_PASS "pass"
#define WEB_DOWNLOAD_PREF_BSSID "bssid"
#define WEB_DOWNLOAD_PREF_CHAN "chan"

#define EARLY_WIFI_ATTEMPT_MS 20000   // per attempt
#define EARLY_WIFI_MAX_ATTEMPTS 6     // live pinned + live unpinned + 3 slots, with headroom

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
  if (WiFi.status() == WL_CONNECTED) {
    prefs.putString(WEB_DOWNLOAD_PREF_SSID, WiFi.SSID().c_str());
    prefs.putString(WEB_DOWNLOAD_PREF_PASS, WiFi.psk().c_str());
    uint8_t* bssid = WiFi.BSSID();
    if (bssid) prefs.putBytes(WEB_DOWNLOAD_PREF_BSSID, bssid, 6);
    prefs.putUChar(WEB_DOWNLOAD_PREF_CHAN, (uint8_t)WiFi.channel());
  } else {
    prefs.remove(WEB_DOWNLOAD_PREF_SSID);
    prefs.remove(WEB_DOWNLOAD_PREF_PASS);
    prefs.remove(WEB_DOWNLOAD_PREF_BSSID);
    prefs.remove(WEB_DOWNLOAD_PREF_CHAN);
  }
  prefs.end();
  delay(100);
  ESP.restart();
}

/**
 * Clear the web download pending flag and the connection snapshot
 */
void clearWebDownloadPending() {
  Preferences prefs;
  prefs.begin(WEB_DOWNLOAD_PREF_NAMESPACE, false);
  prefs.putBool(WEB_DOWNLOAD_PREF_PENDING, false);
  prefs.remove(WEB_DOWNLOAD_PREF_SSID);
  prefs.remove(WEB_DOWNLOAD_PREF_PASS);
  prefs.remove(WEB_DOWNLOAD_PREF_BSSID);
  prefs.remove(WEB_DOWNLOAD_PREF_CHAN);
  prefs.end();
}

/**
 * Check whether early boot has anything to connect with: the live snapshot
 * or any saved slot (ssid1..ssid3 in "wifi" prefs)
 */
bool hasSavedWiFiForEarlyBoot() {
  Preferences prefs;
  prefs.begin(WEB_DOWNLOAD_PREF_NAMESPACE, true);
  bool any = prefs.getString(WEB_DOWNLOAD_PREF_SSID, "").length() > 0;
  prefs.end();
  prefs.begin("wifi", true);
  any = any ||
        prefs.getString("ssid1", "").length() > 0 ||
        prefs.getString("ssid2", "").length() > 0 ||
        prefs.getString("ssid3", "").length() > 0;
  prefs.end();
  return any;
}

struct EarlyWiFiAttempt {
  char ssid[33];
  char pass[65];
  uint8_t bssid[6];
  uint8_t channel;   // 0 = not pinned
};

static const char* earlyWiFiStatusText(wl_status_t st) {
  switch (st) {
    case WL_NO_SSID_AVAIL:   return "network not found";
    case WL_CONNECT_FAILED:  return "rejected (password?)";
    case WL_CONNECTION_LOST: return "connection lost";
    case WL_DISCONNECTED:    return "timed out";
    default:                 return "timed out";
  }
}

// Fully reset the radio so each attempt starts clean (no stale auth state)
static void resetEarlyBootRadio() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(200);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                   // stay awake: faster, more reliable join
  WiFi.setTxPower(WIFI_POWER_19_5dBm);    // full power for the join
  delay(100);
}

/**
 * Connect for the early boot download. Nothing else is running yet (no LVGL,
 * no audio, no BLE, no web server), so the radio and heap are all ours.
 * Order: live snapshot pinned to its AP/channel, live snapshot unpinned,
 * then every saved slot. Each attempt gets a clean radio reset.
 */
static bool connectEarlyBootWiFi(EarlyBootProgressCallback progressCb) {
  EarlyWiFiAttempt attempts[EARLY_WIFI_MAX_ATTEMPTS];  // stack, early boot only - no permanent RAM
  memset(attempts, 0, sizeof(attempts));
  int count = 0;

  Preferences prefs;
  prefs.begin(WEB_DOWNLOAD_PREF_NAMESPACE, true);
  EarlyWiFiAttempt live;
  memset(&live, 0, sizeof(live));
  prefs.getString(WEB_DOWNLOAD_PREF_SSID, live.ssid, sizeof(live.ssid));
  prefs.getString(WEB_DOWNLOAD_PREF_PASS, live.pass, sizeof(live.pass));
  bool haveBssid = prefs.getBytes(WEB_DOWNLOAD_PREF_BSSID, live.bssid, 6) == 6;
  live.channel = prefs.getUChar(WEB_DOWNLOAD_PREF_CHAN, 0);
  prefs.end();

  if (live.ssid[0] != '\0') {
    if (haveBssid && live.channel > 0) attempts[count++] = live;   // pinned
    live.channel = 0;
    attempts[count++] = live;                                       // unpinned
  }

  prefs.begin("wifi", true);
  for (int i = 0; i < 3 && count < EARLY_WIFI_MAX_ATTEMPTS; i++) {
    EarlyWiFiAttempt a;
    memset(&a, 0, sizeof(a));
    char key[8];
    snprintf(key, sizeof(key), "ssid%d", i + 1);
    prefs.getString(key, a.ssid, sizeof(a.ssid));
    snprintf(key, sizeof(key), "pass%d", i + 1);
    prefs.getString(key, a.pass, sizeof(a.pass));
    if (a.ssid[0] == '\0') continue;
    // Skip a slot identical to the live snapshot we already tried
    if (strcmp(a.ssid, live.ssid) == 0 && strcmp(a.pass, live.pass) == 0) continue;
    attempts[count++] = a;
  }
  prefs.end();

  char status[64];
  for (int n = 0; n < count; n++) {
    EarlyWiFiAttempt& a = attempts[n];
    resetEarlyBootRadio();

    snprintf(status, sizeof(status), "Connecting to %.22s...", a.ssid);  // fits 480px at size 2
    Serial.printf("WiFi attempt %d/%d: %s%s (heap %u, max block %u)\n", n + 1, count, a.ssid,
                  a.channel ? " [pinned AP]" : "",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    if (progressCb) progressCb(status, 0, 0);

    if (a.channel) {
      WiFi.begin(a.ssid, a.pass, a.channel, a.bssid);
    } else {
      WiFi.begin(a.ssid, a.pass);
    }

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < EARLY_WIFI_ATTEMPT_MS) {
      // A hard reject is final for this attempt; don't sit out the timeout
      if (WiFi.status() == WL_CONNECT_FAILED) break;
      delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Connected on attempt %d\n", n + 1);
      return true;
    }

    wl_status_t st = WiFi.status();
    Serial.printf("Attempt %d failed: status %d\n", n + 1, (int)st);
    snprintf(status, sizeof(status), "%.20s: %s", a.ssid, earlyWiFiStatusText(st));
    if (progressCb) progressCb(status, 0, 0);
    delay(1500);  // leave the reason on screen long enough to read / photograph
  }
  return false;
}

/**
 * Perform web files download early in boot (before LVGL)
 * This runs when plenty of RAM is available
 * @return true if download successful
 */
bool performEarlyBootWebDownload(EarlyBootProgressCallback progressCb = nullptr) {
  Serial.println("\n========================================");
  Serial.println("EARLY BOOT WEB DOWNLOAD MODE");
  Serial.println("========================================\n");

  Serial.printf("Free heap: %d bytes, max block: %d bytes\n",
    ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  // Connect to WiFi
  if (!connectEarlyBootWiFi(progressCb)) {
    Serial.println("WiFi connection failed!");
    if (progressCb) progressCb("WiFi connection failed!", 0, 0);
    delay(2000);
    return false;
  }
  Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  if (progressCb) progressCb("WiFi connected!", 0, 0);

  // Initialize SD card
  Serial.println("Initializing SD card...");
  if (progressCb) progressCb("Initializing SD card...", 0, 0);
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card init failed!");
    if (progressCb) progressCb("SD card init failed!", 0, 0);
    delay(2000);
    return false;
  }

  Serial.printf("After WiFi+SD - heap: %d, max block: %d\n",
    ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  // Now do the SSL download
  if (progressCb) progressCb("Fetching file manifest...", 0, 0);
  String manifestUrl = String(WEB_FILES_BASE_URL) + WEB_FILES_MANIFEST;
  Serial.printf("Fetching: %s\n", manifestUrl.c_str());

  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  secureClient.setHandshakeTimeout(30);

  Serial.println("Connecting to GitHub (SSL)...");
  if (!secureClient.connect("raw.githubusercontent.com", 443)) {
    Serial.println("SSL connection failed!");
    if (progressCb) progressCb("SSL connection failed!", 0, 0);
    delay(2000);
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
    if (progressCb) progressCb("Failed to parse manifest!", 0, 0);
    delay(2000);
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
    if (progressCb) progressCb(fileName, downloaded + 1, fileCount);

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
  if (progressCb) {
    char doneBuf[48];
    snprintf(doneBuf, sizeof(doneBuf), "Done! %d/%d files downloaded", downloaded, fileCount);
    progressCb(doneBuf, fileCount, fileCount);
  }
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
