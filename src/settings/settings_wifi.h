/*
 * WiFi Settings Module
 * Handles WiFi network scanning, connection, and credential storage
 */

#ifndef SETTINGS_WIFI_H
#define SETTINGS_WIFI_H

#include <WiFi.h>
#include <Preferences.h>
#include "../core/lazy_alloc.h"
#include "../core/config.h"

// Buffer sizes
#define WIFI_SSID_MAX_LEN 33     // 32 chars + null terminator
#define WIFI_PASSWORD_MAX_LEN 65 // 64 chars + null terminator
#define WIFI_STATUS_MAX_LEN 64   // Status message buffer
#define WIFI_AP_PASSWORD_LEN 16  // AP password buffer

// WiFi settings state machine
enum WiFiSettingsState {
  WIFI_STATE_CURRENT_CONNECTION,  // Show current connection status
  WIFI_STATE_SCANNING,
  WIFI_STATE_NETWORK_LIST,
  WIFI_STATE_PASSWORD_INPUT,
  WIFI_STATE_CONNECTING,
  WIFI_STATE_CONNECTED,
  WIFI_STATE_ERROR,
  WIFI_STATE_RESET_CONFIRM,
  WIFI_STATE_AP_MODE
};

// WiFi network info
struct WiFiNetwork {
  char ssid[33];  // 802.11 SSID max 32 bytes + NUL
  int rssi;
  bool encrypted;
};

// WiFi settings globals
WiFiSettingsState wifiState = WIFI_STATE_SCANNING;
#define WIFI_SCAN_MAX_NETWORKS 20
// Scan results live only while the WiFi settings screen is open: allocated
// by scanNetworks(), released by freeWiFiNetworkList() from cleanupWiFiScreen()
WiFiNetwork* networks = nullptr;
int networkCount = 0;

static bool ensureWiFiNetworkList() {
  if (!networks) networks = (WiFiNetwork*)lazyAlloc(sizeof(WiFiNetwork) * WIFI_SCAN_MAX_NETWORKS);
  return networks != nullptr;
}

void freeWiFiNetworkList() {
  lazyFree(networks);
  networkCount = 0;
}
Preferences wifiPrefs;
bool isAPMode = false;  // Track if device is in AP mode
char apPassword[WIFI_AP_PASSWORD_LEN] = "vailsummit";  // Default AP password

// LVGL mode flag - when true, skip legacy draw functions (LVGL handles display)

// ============================================
// Non-Blocking WiFi Connection State Machine
// ============================================

enum WiFiConnectionState {
    WIFI_CONN_IDLE,       // No connection in progress
    WIFI_CONN_REQUESTED,  // Connection requested, not started yet
    WIFI_CONN_STARTING,   // WiFi.begin() called, waiting for result
    WIFI_CONN_SUCCESS,    // Connection succeeded (transient state)
    WIFI_CONN_FAILED      // Connection failed (transient state)
};

struct WiFiConnectionRequest {
    WiFiConnectionState state = WIFI_CONN_IDLE;
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASSWORD_MAX_LEN];
    unsigned long startTime = 0;
    unsigned long timeout = 10000;  // 10 second timeout
    bool wasInAPMode = false;
};

static WiFiConnectionRequest wifiConnRequest;

// Forward declarations for non-blocking connection
void requestWiFiConnection(const char* ssid, const char* password);
bool updateWiFiConnection();
void clearWiFiConnectionState();
WiFiConnectionState getWiFiConnectionState();
void scanNetworks();
void connectToWiFi(const char* ssid, const char* password);
void saveWiFiCredentials(const char* ssid, const char* password);
int loadAllWiFiCredentials(String ssids[3], String passwords[3]);
bool loadWiFiCredentials(String &ssid, String &password);
void autoConnectWiFi();
void resetWiFiSettings();
void startAPMode();
void stopAPMode();

// Scan for WiFi networks
void scanNetworks() {
  Serial.println("Scanning for WiFi networks...");

  // Ensure clean WiFi state before scanning
  WiFi.disconnect(true);  // Disconnect and clear saved credentials from WiFi hardware
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);

  int n = WiFi.scanNetworks();

  Serial.print("Scan result: ");
  Serial.println(n);

  if (n < 0) {
    // Scan failed
    Serial.println("WiFi scan failed!");
    networkCount = 0;
    return;
  }

  if (!ensureWiFiNetworkList()) {
    networkCount = 0;
    WiFi.scanDelete();
    return;
  }
  networkCount = min(n, WIFI_SCAN_MAX_NETWORKS);

  Serial.print("Found ");
  Serial.print(n);
  Serial.println(" networks");

  for (int i = 0; i < networkCount; i++) {
    strlcpy(networks[i].ssid, WiFi.SSID(i).c_str(), sizeof(networks[i].ssid));
    networks[i].rssi = WiFi.RSSI(i);
    networks[i].encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

    Serial.print(i);
    Serial.print(": ");
    Serial.print(networks[i].ssid);
    Serial.print(" (");
    Serial.print(networks[i].rssi);
    Serial.print(" dBm) ");
    Serial.println(networks[i].encrypted ? "[Encrypted]" : "[Open]");
  }

  // Results are copied into networks[] - free the driver's scan list
  WiFi.scanDelete();
}

// Connect to WiFi network
void connectToWiFi(const char* ssid, const char* password) {
  Serial.print("Connecting to: ");
  Serial.println(ssid);

  // If we were in AP mode, stop it first
  bool wasInAPMode = isAPMode;
  if (isAPMode) {
    Serial.println("Stopping AP mode before connecting to WiFi...");
    // Stop web server if running
    extern bool webServerRunning;
    extern void stopWebServer();
    if (webServerRunning) {
      stopWebServer();
    }

    WiFi.softAPdisconnect(true);
    isAPMode = false;
    delay(100);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait up to 10 seconds for connection
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(250);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    wifiState = WIFI_STATE_CONNECTED;

    // Save credentials
    saveWiFiCredentials(ssid, password);

    // If we were in AP mode, set flag to auto-exit after showing success
    if (wasInAPMode) {
      Serial.println("Connection successful from AP mode - will return to main menu");
      // The WiFi event handler will start the web server automatically
    }
  } else {
    Serial.println("Connection failed!");
    wifiState = WIFI_STATE_ERROR;

    // If we were in AP mode and connection failed, restart AP mode
    if (wasInAPMode) {
      Serial.println("Connection failed - restarting AP mode...");
      startAPMode();
    }
  }
}

// Save WiFi credentials to flash memory (up to 3 networks)
void saveWiFiCredentials(const char* ssid, const char* password) {
  wifiPrefs.begin("wifi", false);

  // Load existing saved networks
  String ssid1 = wifiPrefs.getString("ssid1", "");
  String pass1 = wifiPrefs.getString("pass1", "");
  String ssid2 = wifiPrefs.getString("ssid2", "");
  String pass2 = wifiPrefs.getString("pass2", "");
  String ssid3 = wifiPrefs.getString("ssid3", "");
  String pass3 = wifiPrefs.getString("pass3", "");

  // Check if this SSID already exists in the list
  if (strcmp(ssid, ssid1.c_str()) == 0) {
    // Update slot 1
    wifiPrefs.putString("pass1", password);
    Serial.println("Updated existing network in slot 1");
  }
  else if (strcmp(ssid, ssid2.c_str()) == 0) {
    // Update slot 2
    wifiPrefs.putString("pass2", password);
    Serial.println("Updated existing network in slot 2");
  }
  else if (strcmp(ssid, ssid3.c_str()) == 0) {
    // Update slot 3
    wifiPrefs.putString("pass3", password);
    Serial.println("Updated existing network in slot 3");
  }
  else {
    // Add new network - shift existing ones down
    if (ssid1.length() == 0) {
      // Slot 1 is empty
      wifiPrefs.putString("ssid1", ssid);
      wifiPrefs.putString("pass1", password);
      Serial.println("Saved to slot 1");
    }
    else if (ssid2.length() == 0) {
      // Slot 2 is empty
      wifiPrefs.putString("ssid2", ssid);
      wifiPrefs.putString("pass2", password);
      Serial.println("Saved to slot 2");
    }
    else if (ssid3.length() == 0) {
      // Slot 3 is empty
      wifiPrefs.putString("ssid3", ssid);
      wifiPrefs.putString("pass3", password);
      Serial.println("Saved to slot 3");
    }
    else {
      // All slots full, shift down and add to slot 1 (most recent)
      wifiPrefs.putString("ssid3", ssid2);
      wifiPrefs.putString("pass3", pass2);
      wifiPrefs.putString("ssid2", ssid1);
      wifiPrefs.putString("pass2", pass1);
      wifiPrefs.putString("ssid1", ssid);
      wifiPrefs.putString("pass1", password);
      Serial.println("Saved to slot 1 (shifted others down, slot 3 dropped)");
    }
  }

  wifiPrefs.end();
  Serial.println("WiFi credentials saved");
}

// Load WiFi credentials from flash memory (loads all saved networks)
// Note: Returns String arrays for compatibility with LVGL wifi screen
int loadAllWiFiCredentials(String ssids[3], String passwords[3]) {
  wifiPrefs.begin("wifi", true);

  int count = 0;

  ssids[0] = wifiPrefs.getString("ssid1", "");
  passwords[0] = wifiPrefs.getString("pass1", "");
  if (ssids[0].length() > 0) count++;

  ssids[1] = wifiPrefs.getString("ssid2", "");
  passwords[1] = wifiPrefs.getString("pass2", "");
  if (ssids[1].length() > 0) count++;

  ssids[2] = wifiPrefs.getString("ssid3", "");
  passwords[2] = wifiPrefs.getString("pass3", "");
  if (ssids[2].length() > 0) count++;

  wifiPrefs.end();

  return count;
}

// Load WiFi credentials from flash memory (legacy function for compatibility)
bool loadWiFiCredentials(String &ssid, String &password) {
  String ssids[3];
  String passwords[3];
  int count = loadAllWiFiCredentials(ssids, passwords);

  if (count > 0) {
    ssid = ssids[0];
    password = passwords[0];
    return true;
  }

  return false;
}

// Auto-connect to saved WiFi on startup (tries all 3 saved networks)
void autoConnectWiFi() {
  String ssids[3];
  String passwords[3];

  int count = loadAllWiFiCredentials(ssids, passwords);

  if (count == 0) {
    Serial.println("No saved WiFi credentials");
    return;
  }

  Serial.print("Found ");
  Serial.print(count);
  Serial.println(" saved network(s)");

  WiFi.mode(WIFI_STA);

  // Try each saved network in order
  for (int i = 0; i < count; i++) {
    Serial.print("Attempting to connect to: ");
    Serial.println(ssids[i]);

    WiFi.begin(ssids[i].c_str(), passwords[i].c_str());

    // Wait up to 10 seconds for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
      delay(250);
      Serial.print(".");
      attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Auto-connect successful!");
      Serial.print("Connected to: ");
      Serial.println(ssids[i]);
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return;  // Successfully connected, exit
    } else {
      Serial.print("Failed to connect to: ");
      Serial.println(ssids[i]);
      WiFi.disconnect();
    }
  }

  Serial.println("Could not connect to any saved network");
}

// Reset WiFi settings - erase all saved credentials
void resetWiFiSettings() {
  Serial.println("Resetting WiFi settings...");

  wifiPrefs.begin("wifi", false);

  // Clear all saved network credentials
  wifiPrefs.putString("ssid1", "");
  wifiPrefs.putString("pass1", "");
  wifiPrefs.putString("ssid2", "");
  wifiPrefs.putString("pass2", "");
  wifiPrefs.putString("ssid3", "");
  wifiPrefs.putString("pass3", "");

  wifiPrefs.end();

  // Disconnect from current network
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  Serial.println("All WiFi credentials erased");
}

// Start AP mode - create access point for direct connection
void startAPMode() {
  Serial.println("Starting AP mode...");

  // Disconnect from any existing network
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  // Generate unique SSID based on chip ID
  uint32_t chipId = 0;
  for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  char apSSID[32];
  snprintf(apSSID, sizeof(apSSID), "VAIL-SUMMIT-%X", chipId);
  // Convert to uppercase
  for (char* p = apSSID; *p; p++) *p = toupper(*p);

  // Start AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP Mode started. SSID: ");
  Serial.println(apSSID);
  Serial.print("Password: ");
  Serial.println(apPassword);
  Serial.print("AP IP address: ");
  Serial.println(IP);

  isAPMode = true;

  // Start web server for AP mode
  extern bool webServerRunning;
  extern void setupWebServer();
  if (!webServerRunning) {
    Serial.println("Starting web server for AP mode...");
    setupWebServer();
  }
}

// Stop AP mode and switch back to station mode
void stopAPMode() {
  Serial.println("Stopping AP mode...");

  // Stop web server if running in AP mode
  extern bool webServerRunning;
  extern void stopWebServer();
  if (webServerRunning) {
    Serial.println("Stopping web server for AP mode...");
    stopWebServer();
  }

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);

  isAPMode = false;

  Serial.println("AP mode stopped");
}

// ============================================
// Non-Blocking WiFi Connection Functions
// ============================================

// Request a WiFi connection (non-blocking, called from LVGL event handlers)
void requestWiFiConnection(const char* ssid, const char* password) {
    if (wifiConnRequest.state != WIFI_CONN_IDLE) {
        Serial.println("[WiFi] Connection already in progress, ignoring request");
        return;
    }

    strncpy(wifiConnRequest.ssid, ssid, sizeof(wifiConnRequest.ssid) - 1);
    wifiConnRequest.ssid[sizeof(wifiConnRequest.ssid) - 1] = '\0';
    strncpy(wifiConnRequest.password, password, sizeof(wifiConnRequest.password) - 1);
    wifiConnRequest.password[sizeof(wifiConnRequest.password) - 1] = '\0';
    wifiConnRequest.state = WIFI_CONN_REQUESTED;
    wifiConnRequest.timeout = 10000;  // 10 second timeout
    wifiConnRequest.wasInAPMode = isAPMode;

    Serial.printf("[WiFi] Non-blocking connection requested to: %s\n", ssid);
}

// Internal: Start the actual WiFi connection
static void startWiFiConnectionInternal() {
    // Stop AP mode if active
    if (wifiConnRequest.wasInAPMode) {
        Serial.println("[WiFi] Stopping AP mode before connecting...");
        extern bool webServerRunning;
        extern void stopWebServer();
        if (webServerRunning) {
            stopWebServer();
        }
        WiFi.softAPdisconnect(true);
        isAPMode = false;
        delay(50);  // Brief delay for mode switch (non-blocking alternative would be complex)
    }

    // Start connection
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiConnRequest.ssid, wifiConnRequest.password);

    wifiConnRequest.startTime = millis();
    wifiConnRequest.state = WIFI_CONN_STARTING;

    Serial.printf("[WiFi] WiFi.begin() called for: %s\n", wifiConnRequest.ssid);
}

// Poll the connection state (called from main loop)
// Returns: true if state changed to SUCCESS or FAILED
bool updateWiFiConnection() {
    switch (wifiConnRequest.state) {
        case WIFI_CONN_IDLE:
            return false;

        case WIFI_CONN_REQUESTED:
            // Start the connection on next loop iteration
            startWiFiConnectionInternal();
            return false;  // Not done yet, just starting

        case WIFI_CONN_STARTING:
            // Check if connected
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[WiFi] Connected successfully!");
                Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());

                // Save credentials
                saveWiFiCredentials(wifiConnRequest.ssid, wifiConnRequest.password);

                // Track if connected from AP mode
                if (wifiConnRequest.wasInAPMode) {
                }

                wifiConnRequest.state = WIFI_CONN_SUCCESS;
                return true;  // State changed - caller should update UI
            }

            // Check for timeout
            if (millis() - wifiConnRequest.startTime > wifiConnRequest.timeout) {
                Serial.println("[WiFi] Connection timeout!");

                // Restore AP mode if we were in it
                if (wifiConnRequest.wasInAPMode) {
                    Serial.println("[WiFi] Restoring AP mode...");
                    startAPMode();
                }

                wifiConnRequest.state = WIFI_CONN_FAILED;
                return true;  // State changed - caller should update UI
            }

            // Still waiting - check for intermediate failures
            if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_NO_SSID_AVAIL) {
                Serial.printf("[WiFi] Connection failed early (status: %d)\n", WiFi.status());

                if (wifiConnRequest.wasInAPMode) {
                    Serial.println("[WiFi] Restoring AP mode...");
                    startAPMode();
                }

                wifiConnRequest.state = WIFI_CONN_FAILED;
                return true;
            }

            return false;  // Still connecting

        case WIFI_CONN_SUCCESS:
        case WIFI_CONN_FAILED:
            // Transient states - UI should read and then clear
            return false;
    }
    return false;
}

// Clear connection state (call after handling success/failure in UI)
void clearWiFiConnectionState() {
    wifiConnRequest.state = WIFI_CONN_IDLE;
    wifiConnRequest.ssid[0] = '\0';
    wifiConnRequest.password[0] = '\0';
    wifiConnRequest.startTime = 0;
    wifiConnRequest.wasInAPMode = false;
}

// Get current connection state
WiFiConnectionState getWiFiConnectionState() {
    return wifiConnRequest.state;
}

#endif // SETTINGS_WIFI_H
