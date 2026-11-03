/*
 * Web API - WiFi Endpoints
 * Handles WiFi status, scanning, and connection
 */

#ifndef WEB_API_WIFI_H
#define WEB_API_WIFI_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// External function declarations
extern bool checkWebAuth(AsyncWebServerRequest *request);

// WiFi status endpoint - check if in AP mode and if remote connection
void setupWiFiStatusEndpoint(AsyncWebServer &server) {
  server.on("/api/wifi/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;

    extern bool isAPMode;  // From settings_wifi.h

    JsonDocument doc(psramJsonAllocator());
    doc["isAPMode"] = isAPMode;

    // Check if this is a remote connection (client IP != AP IP)
    // If we're in STA mode (connected to a network), any web access is "remote"
    // If we're in AP mode, web access is "local" (direct connection)
    bool isRemoteConnection = !isAPMode && WiFi.status() == WL_CONNECTED;
    doc["isRemoteConnection"] = isRemoteConnection;

    doc["wifiMode"] = isAPMode ? "AP" : "STA";
    doc["connected"] = WiFi.status() == WL_CONNECTED;

    if (isAPMode) {
      doc["ssid"] = WiFi.softAPSSID();
      doc["ip"] = WiFi.softAPIP().toString();
    } else if (WiFi.status() == WL_CONNECTED) {
      doc["ssid"] = WiFi.SSID();
      doc["ip"] = WiFi.localIP().toString();
    }

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });
}

// WiFi scan endpoint
void setupWiFiScanEndpoint(AsyncWebServer &server) {
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;

    extern bool isAPMode;

    // Safety check: only allow scanning if in AP mode or not connected
    if (!isAPMode && WiFi.status() == WL_CONNECTED) {
      request->send(403, "application/json", "{\"success\":false,\"error\":\"WiFi changes disabled during remote connection\"}");
      return;
    }

    Serial.println("Web API: Scanning for WiFi networks...");

    // Scan networks
    int n = WiFi.scanNetworks();

    JsonDocument doc(psramJsonAllocator());
    if (n < 0) {
      doc["success"] = false;
      doc["error"] = "Scan failed";
    } else {
      doc["success"] = true;
      JsonArray networks = doc["networks"].to<JsonArray>();

      for (int i = 0; i < n; i++) {
        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["encrypted"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
      }
    }
    // Results are copied into the JSON doc - free the driver's scan list
    WiFi.scanDelete();

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);

    Serial.print("Web API: Scan complete, found ");
    Serial.print(n);
    Serial.println(" networks");
  });
}

// WiFi connect endpoint
// Runs in the async web server task: validates and queues the request, then
// answers immediately. The main loop starts the connection through the
// non-blocking state machine in settings_wifi.h (processWebWiFiConnectRequest),
// which may stop this server (leaving AP mode) - so nothing here blocks or
// touches WiFi / the server / NVS.
void setupWiFiConnectEndpoint(AsyncWebServer &server) {
  server.on("/api/wifi/connect", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      // Body handler sends the reply; this only covers the auth challenge
      checkWebAuth(request);
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      // Single-chunk JSON body only; answer a split body instead of hanging
      if (index != 0) return;
      if (len != total) {
        request->send(413, "application/json", "{\"success\":false,\"error\":\"Request too large\"}");
        return;
      }
      if (!checkWebAuth(request)) return;

      extern bool isAPMode;

      // Safety check: only allow connection if in AP mode or not connected
      if (!isAPMode && WiFi.status() == WL_CONNECTED) {
        request->send(403, "application/json", "{\"success\":false,\"error\":\"WiFi changes disabled during remote connection\"}");
        return;
      }

      if (isWebWiFiConnectBusy()) {
        request->send(409, "application/json", "{\"success\":false,\"error\":\"A connection attempt is already in progress\"}");
        return;
      }

      JsonDocument doc(psramJsonAllocator());
      DeserializationError error = deserializeJson(doc, data, len);

      if (error) {
        Serial.print("Web API: JSON parse error: ");
        Serial.println(error.c_str());
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
      }

      const char* ssid = doc["ssid"] | "";
      const char* password = doc["password"] | "";
      size_t ssidLen = strlen(ssid);
      size_t passLen = strlen(password);

      if (ssidLen == 0) {
        Serial.println("Web API: ERROR - SSID is empty");
        request->send(400, "application/json", "{\"success\":false,\"error\":\"SSID required\"}");
        return;
      }
      if (ssidLen > WIFI_SSID_MAX_LEN - 1 || passLen > WIFI_PASSWORD_MAX_LEN - 1) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"SSID or password too long\"}");
        return;
      }

      Serial.printf("Web API: Queuing connection to '%s' (password length %u)\n", ssid, (unsigned)passLen);
      requestWiFiConnectionFromWeb(ssid, password);

      request->send(200, "application/json",
        "{\"success\":true,\"pending\":true,\"message\":\"Connection started - check the device screen\"}");
    });
}

// Setup all WiFi API endpoints
void setupWiFiAPI(AsyncWebServer &server) {
  setupWiFiStatusEndpoint(server);
  setupWiFiScanEndpoint(server);
  setupWiFiConnectEndpoint(server);
}

#endif // WEB_API_WIFI_H
