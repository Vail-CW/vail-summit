/*
 * Web Server Module (Modular Version)
 * Provides a comprehensive web interface for device management
 * Features: QSO logging, settings management, device status
 *
 * Access via: http://vail-summit.local or device IP address
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

// Include password protection module (must be before other includes)
#include "../../settings/settings_web_password.h"

// Include modular web components
#include "../pages/web_pages_setup.h"      // Setup splash page (always in flash)
#include "../api/web_api_wifi.h"
#include "../api/web_api_qso.h"
#include "../api/web_api_settings.h"
#include "../api/web_api_memories.h"
#include "../api/web_api_storage.h"
#include "../api/web_api_morse_notes.h"
#include "../api/web_api_screenshot.h"
#include "web_server_api.h"  // Same folder
#include "web_file_downloader.h"  // GitHub download functions
#include "../pages/web_logger_enhanced.h"
#include "../modes/web_practice_socket.h"
#include "../modes/web_memory_chain_socket.h"
#include "../modes/web_hear_it_socket.h"
#include "../../storage/sd_card.h"
#include "../../core/mode_registry.h"

// Global web server instance
AsyncWebServer webServer(80);

// WebSocket pointers - allocated on-demand when web modes are used
// This saves 20-60KB of heap when web practice modes are not in use
AsyncWebSocket* practiceWebSocket = nullptr;
AsyncWebSocket* hearItWebSocket = nullptr;

// mDNS hostname
static const char mdnsHostname[] = "vail-summit";

// Server state
bool webServerRunning = false;
bool webPracticeModeActive = false;
bool webFilesOnSD = false;  // True if web files exist on SD card
bool webServerRestartPending = false;  // Flag to restart server (checked in main loop)

// On-demand lifecycle. The server is started when a screen that shows the
// web URL is open or a MODE_WEB_* mode is running (or always, when
// webServerAlwaysOn is set) and stopped after WEB_SERVER_IDLE_TIMEOUT_MS
// with no HTTP request, no WebSocket client and no such screen open. The
// AsyncTCP service task (CONFIG_ASYNC_TCP_STACK_SIZE, 16 KB) is created on
// the first begin() and is never deleted by the library, so the win of
// on-demand mode is largest when the server has never been started at all.
#define WEB_SERVER_IDLE_TIMEOUT_MS (10UL * 60UL * 1000UL)
unsigned long webLastActivity = 0;
static bool webActivityMiddlewareAdded = false;

// Deferred web mode start flags (set from API handlers, consumed in main loop)
volatile bool webPracticeStartPending = false;
volatile bool webHearItStartPending = false;
volatile bool webMemoryChainStartPending = false;

// Deferred web mode disconnect flag
volatile bool webModeDisconnectPending = false;

// Forward declarations
void setupWebServer();
void stopWebServer();
void restartWebServer();
void requestWebServerRestart();
bool checkWebAuth(AsyncWebServerRequest *request);
void onPracticeWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void sendPracticeDecoded(const char* morse, const char* text);
void sendPracticeWPM(float wpm);

// WebSocket lifecycle management - allocate on-demand, cleanup when mode exits
void initPracticeWebSocket();
void cleanupPracticeWebSocket();
void initHearItWebSocket();
void cleanupHearItWebSocket();
void initMemoryChainWebSocket();
void cleanupMemoryChainWebSocket();

/*
 * Check web authentication
 * Returns true if request is authenticated or auth is disabled
 */
bool checkWebAuth(AsyncWebServerRequest *request) {
  // If auth is disabled, allow all requests
  if (!webAuthEnabled || strlen(webPassword) == 0) {
    Serial.println("[WebAuth] Auth disabled or no password set");
    return true;
  }

  // (password value intentionally not logged)

  // Check for HTTP Basic Auth header
  if (!request->authenticate("admin", webPassword)) {
    Serial.println("[WebAuth] Authentication failed!");
    request->requestAuthentication();
    return false;
  }

  Serial.println("[WebAuth] Authentication successful");
  return true;
}

/*
 * WebSocket Lifecycle Management
 * WebSockets are allocated on-demand to save memory when web modes are not used
 */

void initPracticeWebSocket() {
    if (practiceWebSocket != nullptr) return;  // Already initialized

    Serial.println("[WebSocket] Allocating practice WebSocket...");
    practiceWebSocket = new AsyncWebSocket("/ws/practice");
    practiceWebSocket->onEvent(onPracticeWebSocketEvent);
    webServer.addHandler(practiceWebSocket);
    Serial.printf("[WebSocket] Practice WebSocket ready (heap: %d)\n", ESP.getFreeHeap());
}

void cleanupPracticeWebSocket() {
    if (practiceWebSocket == nullptr) return;

    Serial.println("[WebSocket] Cleaning up practice WebSocket...");
    practiceWebSocket->closeAll();
    // removeHandler() owns and deletes the handler (unique_ptr list in
    // ESPAsyncWebServer 3.6.0) - only delete here if it was not registered
    if (!webServer.removeHandler(practiceWebSocket)) {
      delete practiceWebSocket;
    }
    practiceWebSocket = nullptr;
    Serial.printf("[WebSocket] Practice WebSocket freed (heap: %d)\n", ESP.getFreeHeap());
}

void initHearItWebSocket() {
    if (hearItWebSocket != nullptr) return;

    Serial.println("[WebSocket] Allocating hear-it WebSocket...");
    hearItWebSocket = new AsyncWebSocket("/ws/hear-it");
    hearItWebSocket->onEvent(onHearItWebSocketEvent);
    webServer.addHandler(hearItWebSocket);
    Serial.printf("[WebSocket] Hear-it WebSocket ready (heap: %d)\n", ESP.getFreeHeap());
}

void cleanupHearItWebSocket() {
    if (hearItWebSocket == nullptr) return;

    Serial.println("[WebSocket] Cleaning up hear-it WebSocket...");
    hearItWebSocket->closeAll();
    // removeHandler() owns and deletes the handler (unique_ptr list in
    // ESPAsyncWebServer 3.6.0) - only delete here if it was not registered
    if (!webServer.removeHandler(hearItWebSocket)) {
      delete hearItWebSocket;
    }
    hearItWebSocket = nullptr;
    Serial.printf("[WebSocket] Hear-it WebSocket freed (heap: %d)\n", ESP.getFreeHeap());
}

// Memory chain WebSocket is in web_memory_chain_socket.h
extern AsyncWebSocket* memoryChainWebSocket;
extern void onMemoryChainWebSocketEvent(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType, void*, uint8_t*, size_t);

void initMemoryChainWebSocket() {
    if (memoryChainWebSocket != nullptr) return;

    Serial.println("[WebSocket] Allocating memory-chain WebSocket...");
    memoryChainWebSocket = new AsyncWebSocket("/ws/memory-chain");
    memoryChainWebSocket->onEvent(onMemoryChainWebSocketEvent);
    webServer.addHandler(memoryChainWebSocket);
    Serial.printf("[WebSocket] Memory-chain WebSocket ready (heap: %d)\n", ESP.getFreeHeap());
}

void cleanupMemoryChainWebSocket() {
    if (memoryChainWebSocket == nullptr) return;

    Serial.println("[WebSocket] Cleaning up memory-chain WebSocket...");
    memoryChainWebSocket->closeAll();
    // removeHandler() owns and deletes the handler (unique_ptr list in
    // ESPAsyncWebServer 3.6.0) - only delete here if it was not registered
    if (!webServer.removeHandler(memoryChainWebSocket)) {
      delete memoryChainWebSocket;
    }
    memoryChainWebSocket = nullptr;
    Serial.printf("[WebSocket] Memory-chain WebSocket freed (heap: %d)\n", ESP.getFreeHeap());
}

/*
 * Initialize and start the web server
 * Called automatically when WiFi connects
 */
void setupWebServer() {
  if (webServerRunning) {
    Serial.println("Web server already running");
    return;
  }

  Serial.println("Starting web server...");

  // Check if we're in AP mode (from settings_wifi.h)
  extern bool isAPMode;

  // Every request (pages, API, WebSocket handshakes) refreshes the idle
  // timer. reset() does not clear middlewares, so add it exactly once.
  webLastActivity = millis();
  if (!webActivityMiddlewareAdded) {
    webActivityMiddlewareAdded = true;
    webServer.addMiddleware([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
      webLastActivity = millis();
      next();
    });
  }

  // Set up mDNS responder (only works in Station mode, not AP mode)
  if (!isAPMode) {
    if (MDNS.begin(mdnsHostname)) {
      Serial.print("mDNS responder started: http://");
      Serial.print(mdnsHostname);
      Serial.println(".local");
      MDNS.addService("http", "tcp", 80);
    } else {
      Serial.println("Error setting up mDNS responder!");
    }
  } else {
    Serial.println("Skipping mDNS setup (not supported in AP mode)");
  }

  // ============================================
  // Check if web files exist on SD card
  // ============================================
  webFilesOnSD = webFilesExist();
  Serial.printf("Web files on SD card: %s\n", webFilesOnSD ? "YES" : "NO");

  // ============================================
  // Web Files Status API (read-only)
  // Note: Download/upload removed - web files can only be downloaded via device WiFi Settings menu
  // ============================================
  webServer.on("/api/webfiles/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc(psramJsonAllocator());
    doc["installed"] = webFilesOnSD;
    doc["version"] = getWebFilesVersion();

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  // ============================================
  // Setup Mode vs Normal Mode Routing
  // ============================================
  if (!webFilesOnSD) {
    // SETUP MODE: Serve setup splash page for all HTML routes
    Serial.println("Web server running in SETUP MODE (no web files on SD)");

    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (!checkWebAuth(request)) return;
      request->send_P(200, "text/html", SETUP_HTML);
    });

    // All other page routes redirect to setup
    webServer.on("/logger", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });
    webServer.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });
    webServer.on("/radio", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });
    webServer.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });
    webServer.on("/system", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });
    webServer.on("/storage", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });
    webServer.on("/practice", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });
    webServer.on("/memory-chain", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });
    webServer.on("/hear-it", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });

  } else {
    // NORMAL MODE: Serve pages from SD card
    Serial.println("Web server running in NORMAL MODE (serving from SD card)");

    // Serve static files from SD card /www/ directory
    // Add authentication if password is enabled
    if (webAuthEnabled && strlen(webPassword) > 0) {
      webServer.serveStatic("/", SD, "/www/")
        .setDefaultFile("index.html")
        .setAuthentication("admin", webPassword);
    } else {
      webServer.serveStatic("/", SD, "/www/").setDefaultFile("index.html");
    }
  }

  // ============================================
  // API Endpoints (always available regardless of mode)
  // ============================================

  // Device status endpoint
  webServer.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;
    request->send(200, "application/json", getDeviceStatusJSON());
  });

  // QSO logs list endpoint
  webServer.on("/api/qsos", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;
    request->send(200, "application/json", getQSOLogsJSON());
  });

  // ADIF export endpoint
  webServer.on("/api/export/adif", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;
    String adif = generateADIF();
    AsyncWebServerResponse *response = request->beginResponse(200, "application/x-adif", adif);
    response->addHeader("Content-Disposition", "attachment; filename=vail-summit-logs.adi");
    request->send(response);
  });

  // CSV export endpoint
  webServer.on("/api/export/csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;
    String csv = generateCSV();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/csv", csv);
    response->addHeader("Content-Disposition", "attachment; filename=vail-summit-logs.csv");
    request->send(response);
  });

  // Practice mode API endpoint
  webServer.on("/api/practice/start", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkWebAuth(request)) return;

    initPracticeWebSocket();
    webPracticeStartPending = true;

    JsonDocument doc(psramJsonAllocator());
    doc["status"] = "active";
    doc["endpoint"] = "ws://vail-summit.local/ws/practice";

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  // Memory Chain mode API endpoint
  webServer.on("/api/memory-chain/start", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (!checkWebAuth(request)) {
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
      }
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (!checkWebAuth(request)) return;

      initMemoryChainWebSocket();
      webMemoryChainStartPending = true;

      JsonDocument response(psramJsonAllocator());
      response["status"] = "active";
      response["endpoint"] = "ws://vail-summit.local/ws/memory-chain";

      String output;
      serializeJson(response, output);
      request->send(200, "application/json", output);
    });

  // Hear It Type It mode API endpoint
  webServer.on("/api/hear-it/start", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (!checkWebAuth(request)) {
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
      }
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (!checkWebAuth(request)) return;

      initHearItWebSocket();
      webHearItStartPending = true;

      JsonDocument response(psramJsonAllocator());
      response["status"] = "active";
      response["endpoint"] = "ws://vail-summit.local/ws/hear-it";

      String output;
      serializeJson(response, output);
      request->send(200, "application/json", output);
    });

  // Setup modular API endpoints
  setupQSOAPI(webServer);
  setupWiFiAPI(webServer);
  setupSettingsAPI(webServer);
  setupMemoriesAPI(webServer);
  registerStorageAPI(&webServer);
  registerMorseNotesAPI(&webServer);
  registerScreenshotAPI(&webServer);

  // NOTE: WebSockets are now allocated on-demand when web modes are started
  // This saves 20-60KB of heap memory when web practice modes are not used
  // See: initPracticeWebSocket(), initHearItWebSocket(), initMemoryChainWebSocket()

  // Start server
  webServer.begin();
  webServerRunning = true;

  Serial.println("Web server started successfully");

  // Show appropriate access method based on mode
  if (isAPMode) {
    Serial.print("Access at: http://");
    Serial.print(WiFi.softAPIP());
    Serial.println("/");
    Serial.println("(mDNS not available in AP mode - use IP address only)");
  } else {
    Serial.print("Access at: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");
    Serial.print("Or via mDNS: http://");
    Serial.print(mdnsHostname);
    Serial.println(".local/");
  }
}

/*
 * Stop the web server
 */
void stopWebServer() {
  if (!webServerRunning) {
    return;
  }

  // Clean up any allocated WebSockets
  cleanupPracticeWebSocket();
  cleanupHearItWebSocket();
  cleanupMemoryChainWebSocket();

  webServer.end();
  // end() only closes the listener - reset() frees every route handler so
  // the next setupWebServer() does not stack a second copy of all routes
  webServer.reset();
  MDNS.end();
  webServerRunning = false;
  Serial.println("Web server stopped");
}

/*
 * True for modes whose screen shows the web URL or that are driven from
 * the browser: the server must be up while one of them is current.
 */
bool webServerWantedByMode(int mode) {
  switch (mode) {
    case MODE_WEB_PRACTICE:
    case MODE_WEB_HEAR_IT:
    case MODE_WEB_MEMORY_CHAIN:
    case MODE_SYSTEM_INFO:
    case MODE_WIFI_SETTINGS:
    case MODE_WEB_PASSWORD_SETTINGS:
    case MODE_WEB_FILES_UPDATE:
      return true;
    default:
      return false;
  }
}

/*
 * Start the server now if the network is up and it is not already running.
 * Called from the main loop / LVGL handlers.
 */
void requestWebServer() {
  extern bool isAPMode;
  if (webServerRunning) {
    webLastActivity = millis();
    return;
  }
  if (!WiFi.isConnected() && !isAPMode) return;
  setupWebServer();
}

/*
 * WebSocket client count across the on-demand sockets (0 when none exist)
 */
static uint32_t webSocketClientCount() {
  uint32_t n = 0;
  if (practiceWebSocket) n += practiceWebSocket->count();
  if (hearItWebSocket) n += hearItWebSocket->count();
  return n;
}

/*
 * Main-loop lifecycle: start on demand for URL screens / web modes / the
 * always-on setting, stop after idle. AP mode (captive portal / SETUP MODE)
 * keeps the server up for as long as the AP exists.
 */
void updateWebServerLifecycle() {
  static unsigned long lastCheck = 0;
  unsigned long now = millis();
  if (now - lastCheck < 1000) return;
  lastCheck = now;

  extern bool isAPMode;
  bool wanted = isAPMode || webServerAlwaysOn || webServerWantedByMode((int)currentMode);

  if (!webServerRunning) {
    if (wanted && (WiFi.isConnected() || isAPMode)) {
      Serial.println("[WebServer] Starting on demand");
      setupWebServer();
    }
    return;
  }

  if (wanted) {
    webLastActivity = now;  // a wanted server never goes idle
    return;
  }
  if (webSocketClientCount() > 0) {
    webLastActivity = now;
    return;
  }
  if (now - webLastActivity >= WEB_SERVER_IDLE_TIMEOUT_MS && !isModeAudioCritical((int)currentMode)) {
    Serial.println("[WebServer] Idle - stopping to free RAM");
    stopWebServer();
    // CW memories / radio queue are kept while the server runs (its handlers
    // use them); free them now unless their own screens are open
    extern void freeCWMemories();
    extern void freeRadioMessageQueue();
    if (currentMode != MODE_CW_MEMORIES && currentMode != MODE_RADIO_OUTPUT) {
      freeCWMemories();
      freeRadioMessageQueue();
    }
  }
}

/*
 * Restart the web server (to pick up new routes after file upload)
 * This should be called from the main loop when webServerRestartPending is true
 */
void restartWebServer() {
  Serial.println("Restarting web server...");
  stopWebServer();
  delay(100);  // Brief delay to ensure clean shutdown
  setupWebServer();
  webServerRestartPending = false;
}

/*
 * Check if web server restart is pending (call from main loop)
 */
bool isWebServerRestartPending() {
  return webServerRestartPending;
}

/*
 * Request web server restart (safe to call from request handlers)
 */
void requestWebServerRestart() {
  webServerRestartPending = true;
  Serial.println("Web server restart requested");
}

#endif // WEB_SERVER_H
