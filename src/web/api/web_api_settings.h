// web_api_settings.h
// Device Settings and System Info API Endpoints for VAIL SUMMIT
// Handles radio control, CW settings, volume, callsign, and system diagnostics

#ifndef WEB_API_SETTINGS_H
#define WEB_API_SETTINGS_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <SPIFFS.h>

// External declarations for global variables
extern MenuMode currentMode;
extern bool radioOutputActive;
extern LGFX tft;
extern int cwSpeed;
extern int cwTone;
extern KeyType cwKeyType;
extern char vailCallsign[];
extern StorageStats storageStats;
extern bool hasMAX17048;
extern bool hasLC709203;
extern Adafruit_LC709203F lc;
extern Adafruit_MAX17048 maxlipo;

// External function declarations
extern void startRadioOutput();
extern bool queueRadioMessage(const char* message);
extern void saveCWSettings();
extern void setVolume(int volume);
extern int getVolume();
extern void saveCallsign(const char* callsign);
extern bool checkWebAuth(AsyncWebServerRequest *request);
extern void onLVGLMenuSelect(int target_mode);
String getWebFilesVersion();  // Forward declaration (defined in web_file_downloader.h)

// ============================================
// Deferred work from the async web server task
// ============================================
// Handlers below run in the AsyncTCP task. They never touch LVGL,
// currentMode or NVS: they stage values here and the main loop applies
// them in processWebSettingsRequests(). -1 / false = nothing pending.
volatile bool webRadioEnterPending = false;
volatile int webPendingWpm = -1;
volatile int webPendingTone = -1;
volatile int webPendingKeyType = -1;
volatile int webPendingVolume = -1;
volatile bool webCallsignPending = false;
static char webPendingCallsign[VAIL_CALLSIGN_LEN];

/*
 * Main loop: apply settings staged by the web API. CW and volume saves go
 * through markDeferredSave() (which is only safe on this task); the callsign
 * is a one-shot text save.
 */
void processWebSettingsRequests() {
  bool cwChanged = false;
  int v;

  // Take-and-clear in one step so a value the web task writes between a
  // read and a clear can't be lost
  v = __atomic_exchange_n(&webPendingWpm, -1, __ATOMIC_SEQ_CST);
  if (v >= 0) {
    if (cwSpeed != v) {
      cwSpeed = v;
      // Radio Output caches the dit length when it starts; follow the change
      if (radioOutputActive && radioKeyer) {
        radioDitDuration = DIT_DURATION(cwSpeed);
        radioKeyer->setDitDuration(radioDitDuration);
      }
    }
    cwChanged = true;
  }

  v = __atomic_exchange_n(&webPendingTone, -1, __ATOMIC_SEQ_CST);
  if (v >= 0) {
    cwTone = v;
    cwChanged = true;
  }

  v = __atomic_exchange_n(&webPendingKeyType, -1, __ATOMIC_SEQ_CST);
  if (v >= 0) {
    // Radio Output picks its keyer at start; a new key type applies the
    // next time it is entered
    if (v <= KEY_ULTIMATIC) cwKeyType = (KeyType)v;  // range-checked by the handler
    cwChanged = true;
  }

  if (cwChanged) {
    markDeferredSave(saveCWSettings);
  }

  v = __atomic_exchange_n(&webPendingVolume, -1, __ATOMIC_SEQ_CST);
  if (v >= 0) {
    setVolume(v);  // deferred save inside
  }

  if (webCallsignPending) {
    // Copy before clearing the flag: the handler refuses a new callsign
    // while the flag is set, so the buffer can't change under the copy
    char call[VAIL_CALLSIGN_LEN];
    strlcpy(call, webPendingCallsign, sizeof(call));
    __sync_synchronize();
    webCallsignPending = false;
    strlcpy(vailCallsign, call, VAIL_CALLSIGN_LEN);
    saveCallsign(call);
  }

  if (webRadioEnterPending) {
    webRadioEnterPending = false;
    if (currentMode != MODE_RADIO_OUTPUT) {
      Serial.println("Switching to Radio Output mode via web interface");
      onLVGLMenuSelect(MODE_RADIO_OUTPUT);
    }
  }
}

// ============================================
// Setup Function - Register All Settings API Endpoints
// ============================================

void setupSettingsAPI(AsyncWebServer &webServer) {

  // ============================================
  // Radio Control API Endpoints
  // ============================================

  // Radio status endpoint
  webServer.on("/api/radio/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;

    JsonDocument doc(psramJsonAllocator());
    doc["active"] = (currentMode == MODE_RADIO_OUTPUT && radioOutputActive);
    doc["mode"] = (currentMode == MODE_RADIO_OUTPUT) ? "radio_output" : "other";

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  // Enter radio mode endpoint
  webServer.on("/api/radio/enter", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;

    // Mode switch happens on the main loop (processWebSettingsRequests)
    webRadioEnterPending = true;

    request->send(200, "application/json", "{\"success\":true}");
  });

  // Send morse message endpoint
  webServer.on("/api/radio/send", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (!checkWebAuth(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
      }
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (!checkWebAuth(request)) return;

      // Parse JSON body
      JsonDocument doc(psramJsonAllocator());
      DeserializationError error = deserializeJson(doc, data, len);

      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
      }

      const char* message = doc["message"] | "";
      if (strlen(message) == 0) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Message is empty\"}");
        return;
      }

      // Queue message for transmission
      if (queueRadioMessage(message)) {
        Serial.print("Queued radio message: ");
        Serial.println(message);
        request->send(200, "application/json", "{\"success\":true}");
      } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Message queue is full\"}");
      }
    });

  // Get WPM speed endpoint
  webServer.on("/api/radio/wpm", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;

    JsonDocument doc(psramJsonAllocator());
    doc["wpm"] = cwSpeed;

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  // Set WPM speed endpoint
  webServer.on("/api/radio/wpm", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (!checkWebAuth(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
      }
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (!checkWebAuth(request)) return;

      // Parse JSON body
      JsonDocument doc(psramJsonAllocator());
      DeserializationError error = deserializeJson(doc, data, len);

      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
      }

      int wpm = doc["wpm"] | 0;
      if (wpm < 5 || wpm > 40) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"WPM must be between 5 and 40\"}");
        return;
      }

      // Applied and saved on the main loop
      webPendingWpm = wpm;

      Serial.print("CW speed updated to ");
      Serial.print(wpm);
      Serial.println(" WPM via web interface");

      request->send(200, "application/json", "{\"success\":true}");
    });

  // ============================================
  // Device Settings API Endpoints
  // ============================================

  // Get CW settings
  webServer.on("/api/settings/cw", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;

    JsonDocument doc(psramJsonAllocator());
    doc["wpm"] = cwSpeed;
    doc["tone"] = cwTone;
    doc["keyType"] = (int)cwKeyType;

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  // Set CW settings
  webServer.on("/api/settings/cw", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (!checkWebAuth(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
      }
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (!checkWebAuth(request)) return;

      JsonDocument doc(psramJsonAllocator());
      DeserializationError error = deserializeJson(doc, data, len);

      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
      }

      // Validate everything first, then stage. A missing or null field
      // leaves that setting unchanged.
      int wpm = -1, tone = -1, keyType = -1;

      if (doc["wpm"].is<int>()) {
        wpm = doc["wpm"].as<int>();
        if (wpm < 5 || wpm > 40) {
          request->send(400, "application/json", "{\"success\":false,\"error\":\"WPM must be between 5 and 40\"}");
          return;
        }
      } else if (!doc["wpm"].isNull()) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"WPM must be a number\"}");
        return;
      }

      if (doc["tone"].is<int>()) {
        tone = doc["tone"].as<int>();
        if (tone < 400 || tone > 1200) {
          request->send(400, "application/json", "{\"success\":false,\"error\":\"Tone must be between 400 and 1200 Hz\"}");
          return;
        }
      } else if (!doc["tone"].isNull()) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Tone must be a number\"}");
        return;
      }

      if (doc["keyType"].is<int>()) {
        keyType = doc["keyType"].as<int>();
        if (keyType < KEY_STRAIGHT || keyType > KEY_ULTIMATIC) {
          request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid key type\"}");
          return;
        }
      } else if (!doc["keyType"].isNull()) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid key type\"}");
        return;
      }

      // Applied and saved on the main loop
      if (wpm >= 0) webPendingWpm = wpm;
      if (tone >= 0) webPendingTone = tone;
      if (keyType >= 0) webPendingKeyType = keyType;

      Serial.println("CW settings updated via web interface");

      request->send(200, "application/json", "{\"success\":true}");
    });

  // Get volume
  webServer.on("/api/settings/volume", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;

    JsonDocument doc(psramJsonAllocator());
    doc["volume"] = getVolume();

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  // Set volume
  webServer.on("/api/settings/volume", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (!checkWebAuth(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
      }
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (!checkWebAuth(request)) return;

      JsonDocument doc(psramJsonAllocator());
      DeserializationError error = deserializeJson(doc, data, len);

      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
      }

      int volume = doc["volume"] | -1;
      if (volume < 0 || volume > 100) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Volume must be between 0 and 100\"}");
        return;
      }

      // Applied and saved on the main loop
      webPendingVolume = volume;

      Serial.print("Volume updated to ");
      Serial.print(volume);
      Serial.println("% via web interface");

      request->send(200, "application/json", "{\"success\":true}");
    });

  // Get callsign
  webServer.on("/api/settings/callsign", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;

    JsonDocument doc(psramJsonAllocator());
    doc["callsign"] = vailCallsign;

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  // Set callsign
  webServer.on("/api/settings/callsign", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (!checkWebAuth(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
      }
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (!checkWebAuth(request)) return;

      JsonDocument doc(psramJsonAllocator());
      DeserializationError error = deserializeJson(doc, data, len);

      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
      }

      // Trim and upper-case into a fixed buffer
      const char* raw = doc["callsign"] | "";
      while (*raw == ' ' || *raw == '\t') raw++;
      size_t rawLen = strlen(raw);
      while (rawLen > 0 && (raw[rawLen - 1] == ' ' || raw[rawLen - 1] == '\t')) rawLen--;

      if (rawLen == 0) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Callsign cannot be empty\"}");
        return;
      }

      if (rawLen > 10) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Callsign too long (max 10 characters)\"}");
        return;
      }

      if (webCallsignPending) {
        request->send(409, "application/json", "{\"success\":false,\"error\":\"Busy, try again\"}");
        return;
      }

      // Applied and saved on the main loop
      for (size_t i = 0; i < rawLen; i++) webPendingCallsign[i] = toupper((unsigned char)raw[i]);
      webPendingCallsign[rawLen] = '\0';
      __sync_synchronize();  // buffer visible before the flag
      webCallsignPending = true;

      Serial.print("Callsign update queued via web interface: ");
      Serial.println(webPendingCallsign);

      request->send(200, "application/json", "{\"success\":true}");
    });

  // ============================================
  // System Info API Endpoint
  // ============================================

  // Get system info
  webServer.on("/api/system/info", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;

    JsonDocument doc(psramJsonAllocator());

    // Firmware
    doc["firmware"] = FIRMWARE_VERSION;
    doc["firmwareDate"] = FIRMWARE_DATE;
    doc["firmwareName"] = FIRMWARE_NAME;

    // Chip info
    doc["chipModel"] = ESP.getChipModel();
    doc["chipRevision"] = ESP.getChipRevision();

    // System
    doc["uptime"] = millis();
    doc["cpuFreq"] = ESP.getCpuFreqMHz();
    doc["flashSize"] = ESP.getFlashChipSize();

    // Memory
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["minFreeHeap"] = ESP.getMinFreeHeap();
    doc["psramFound"] = psramFound();
    if (psramFound()) {
      doc["freePsram"] = ESP.getFreePsram();
      doc["minFreePsram"] = ESP.getMinFreePsram();
      doc["psramSize"] = ESP.getPsramSize();
    }

    // Storage
    doc["spiffsUsed"] = SPIFFS.usedBytes();
    doc["spiffsTotal"] = SPIFFS.totalBytes();
    doc["qsoCount"] = storageStats.totalLogs;

    // Web files
    doc["webFilesExpected"] = WEB_FILES_VERSION;
    String installedWebVer = getWebFilesVersion();
    installedWebVer.trim();
    doc["webFilesInstalled"] = installedWebVer.isEmpty() ? "Not installed" : installedWebVer.c_str();
    doc["webFilesMatch"] = (!installedWebVer.isEmpty() && installedWebVer == WEB_FILES_VERSION);

    // WiFi
    doc["wifiConnected"] = WiFi.isConnected();
    if (WiFi.isConnected()) {
      doc["wifiSSID"] = WiFi.SSID();
      doc["wifiIP"] = WiFi.localIP().toString();
      doc["wifiRSSI"] = WiFi.RSSI();
    }

    // Battery
    float batteryVoltage = 0;
    float batteryPercent = 0;
    String batteryMonitor = "None";

    if (hasMAX17048) {
      batteryVoltage = maxlipo.cellVoltage();
      batteryPercent = maxlipo.cellPercent();
      batteryMonitor = "MAX17048";
    } else if (hasLC709203) {
      batteryVoltage = lc.cellVoltage();
      batteryPercent = lc.cellPercent();
      batteryMonitor = "LC709203F";
    }

    doc["batteryVoltage"] = batteryVoltage;
    doc["batteryPercent"] = batteryPercent;
    doc["batteryMonitor"] = batteryMonitor;

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  Serial.println("Settings API endpoints registered");
}

#endif // WEB_API_SETTINGS_H
