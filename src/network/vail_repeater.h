/*
 * Vail Repeater Module
 * WebSocket client for vailmorse.com morse code repeater
 *
 * REQUIRED LIBRARIES (install via Arduino Library Manager):
 * 1. WebSockets by Markus Sattler
 * 2. ArduinoJson by Benoit Blanchon
 */

#ifndef VAIL_REPEATER_H
#define VAIL_REPEATER_H

#include <WiFi.h>
#include <WiFiClientSecure.h>


  #include <WebSocketsClient.h>
  #include <ArduinoJson.h>
  #include <deque>

#include "../core/config.h"
#include "../core/task_manager.h"  // For dual-core audio API
#include "../settings/settings_cw.h"
#include "../settings/settings_decoder.h"
#include "../audio/morse_decoder_adaptive.h"
#include "../audio/morse_decoder_direct.h"
#include "../keyer/keyer.h"
#include "internet_check.h"
#include <esp_timer.h>

// Current room (persisted). Protocol room names are short; 31 chars + NUL.
#define VAIL_CHANNEL_LEN 32
#define VAIL_CALLSIGN_LEN 16
char vailChannel[VAIL_CHANNEL_LEN] = "General";

// Settings persisted to NVS (declared before load/save functions that reference them)
bool vailListenOnly = false;

// Preferences for Vail settings persistence
Preferences vailPrefs;

// Load Vail settings from flash
void loadVailSettings() {
  vailPrefs.begin("vail", true);  // Read-only mode
  vailPrefs.getString("room", vailChannel, sizeof(vailChannel));
  if (vailChannel[0] == '\0') strlcpy(vailChannel, "General", sizeof(vailChannel));
  vailListenOnly = vailPrefs.getBool("listenOnly", false);
  vailPrefs.end();
  Serial.printf("[Vail] Loaded room: %s\n", vailChannel);
}

// Save Vail settings to flash
void saveVailSettings() {
  vailPrefs.begin("vail", false);  // Read-write mode
  vailPrefs.putString("room", vailChannel);
  vailPrefs.putBool("listenOnly", vailListenOnly);
  vailPrefs.end();
  Serial.printf("[Vail] Saved room: %s\n", vailChannel);
}

// User identification
char vailCallsign[VAIL_CALLSIGN_LEN] = "GUEST";  // Default callsign (user can configure)
uint8_t vailTxTone = 72;        // MIDI note 72 = C5 (523 Hz) - default CW tone

// Audio settings
// When true, all received morse plays at user's local cwTone frequency for consistency
// When false, received morse plays at sender's TX tone frequency (original behavior)
#define VAIL_USE_LOCAL_TONE_FOR_RECEIVE true


// Vail repeater state
enum VailState {
  VAIL_DISCONNECTED,
  VAIL_CONNECTING,
  VAIL_CONNECTED,
  VAIL_ERROR
};

// Vail globals
WebSocketsClient webSocket;
VailState vailState = VAIL_DISCONNECTED;
static const char vailServer[] = "vailmorse.com";
int vailPort = 443;  // WSS (secure WebSocket)
int connectedClients = 0;
char statusText[40] = "";
// Note: needsUIRedraw removed - LVGL handles all UI updates via updateVailScreenLVGL()
unsigned long lastKeepaliveTime = 0;  // Track last keepalive message

// Verbose per-message serial logging. OFF by default: dumping JSON payloads
// and per-element send lines stalls the audio-critical Core-1 loop enough to
// make keying timing audibly rough. Enable only when debugging the protocol.
// #define VAIL_VERBOSE_LOGS
#ifdef VAIL_VERBOSE_LOGS
  #define VAIL_LOG(...) Serial.printf(__VA_ARGS__)
#else
  #define VAIL_LOG(...) do {} while (0)
#endif

// Transmit state
bool vailIsTransmitting = false;
unsigned long vailTxStartTime = 0;
std::vector<uint16_t> vailTxDurations;
// Echo filtering: Track recent transmission timestamps to filter echoes
std::deque<int64_t> recentTxTimestamps;
std::deque<int64_t> recentChatTimestamps;  // own chat echo detection (exact ts match)
const size_t MAX_TX_TIMESTAMPS = 20;

// Deferred element sends: the keyer callback runs inside vailKeyer->tick() on
// the audio-critical loop, DURING the tone - building JSON and pushing a TLS
// websocket frame there jitters element timing (audible crunch). The callback
// only queues (duration, timestamp); flushVailPendingSends() transmits from
// updateVailRepeater() after keyer servicing. Timestamps are captured at
// key-down, so deferring the send a few ms changes nothing on receivers.
struct VailPendingSend {
  uint16_t durMs;
  int64_t ts;
};
static VailPendingSend vailPendingSends[8];
static int vailPendingSendCount = 0;

static void queueVailElementSend(uint16_t durMs, int64_t ts) {
  if (vailPendingSendCount < 8) {
    vailPendingSends[vailPendingSendCount].durMs = durMs;
    vailPendingSends[vailPendingSendCount].ts = ts;
    vailPendingSendCount++;
  }
}

// Keyer - runs in main loop on Core 1 (like practice mode)
static StraightKeyer* vailKeyer = nullptr;
static bool vailDitPressed = false;
static bool vailDahPressed = false;
static unsigned long vailLastStateChangeTime = 0;
static bool vailLastToneState = false;
static int64_t vailToneStartTimestamp = 0;  // server-clock time the current tone began
static bool vailSentAtKeyDown = false;      // keyer modes send at key-down (length known)

// Receive state
struct VailMessage {
  int64_t timestamp;
  uint16_t clients;
  uint8_t txTone;  // Sender's TX tone (MIDI note number)
  std::vector<uint16_t> durations;
};

std::vector<VailMessage> rxQueue;
unsigned long playbackDelay = 500;  // 500ms delay for network jitter
int64_t clockSkew = 0;  // Offset to convert millis() to server time
int clockSkewSamples = 0;  // Number of clock skew samples received

// RX decoding (incoming morse -> text) is only allowed on the dedicated
// "Decoder" room, matching vailmorse.com behavior. On all other rooms the
// decoder must stay dormant — both decoded text and the dot/dash row.
// TX-side decoding/visualization is unaffected (operator's own keying).
static inline bool vailIsOnDecoderChannel() {
    return strcmp(vailChannel, "Decoder") == 0;
}

volatile bool vailTxDecodedReady = false;  // set when TX decoder emits a char
char vailTxLastDecodedChar = 0;       // the decoded character

// Decoded character slots shared with LVGL update
#define VAIL_DECODE_SLOTS 22
struct VailDecodedEntry {
    char ch;
    char morse[8];
};
VailDecodedEntry vailDecodedEntries[VAIL_DECODE_SLOTS];
int vailDecodedCount = 0;
volatile bool vailDecodedNeedsUpdate = false;

// Decoders for RX and TX
static MorseDecoder* vailRxDecoder = nullptr;
static MorseDecoder* vailTxDecoder = nullptr;
static esp_timer_handle_t vailRxTickTimer = nullptr;
static esp_timer_handle_t vailTxTickTimer = nullptr;
static volatile bool vailRxTickPending = false;
static volatile bool vailTxTickPending = false;
static void vailRxTickCallback(void*) { vailRxTickPending = true; }
static void vailTxTickCallback(void*) { vailTxTickPending = true; }

static void vailOnDecoded(String morse, String text);

static void vailTxOnDecoded(String morse, String text) {
    for (int i = 0; i < (int)text.length(); i++) {
        vailTxLastDecodedChar = text[i];
        vailTxDecodedReady = true;
    }
    // On the Decoder room the operator's own keying shows in the decoded
    // strip too, interleaved with received decode — same single stream.
    if (vailIsOnDecoderChannel()) vailOnDecoded(morse, text);
}

static void vailOnDecoded(String morse, String text) {
    for (int i = 0; i < (int)text.length(); i++) {
        VailDecodedEntry entry;
        entry.ch = text[i];
        strncpy(entry.morse, morse.c_str(), sizeof(entry.morse) - 1);
        entry.morse[sizeof(entry.morse) - 1] = '\0';
        if (vailDecodedCount < VAIL_DECODE_SLOTS) {
            vailDecodedEntries[vailDecodedCount++] = entry;
        } else {
            for (int j = 0; j < VAIL_DECODE_SLOTS - 1; j++)
                vailDecodedEntries[j] = vailDecodedEntries[j + 1];
            vailDecodedEntries[VAIL_DECODE_SLOTS - 1] = entry;
        }
    }
    vailDecodedNeedsUpdate = true;
}

static void startVailDecoders() {
    // RX decoder
    if (vailRxTickTimer != nullptr) {
        esp_timer_stop(vailRxTickTimer);
        esp_timer_delete(vailRxTickTimer);
        vailRxTickTimer = nullptr;
    }
    delete vailRxDecoder;
    vailRxDecoder = (decoderType == DECODER_DIRECT)
        ? (MorseDecoder*) new MorseDecoderDirect(cwSpeed, cwSpeed, 30)
        : (MorseDecoder*) new MorseDecoderAdaptive(cwSpeed, cwSpeed, 30);
    vailRxDecoder->messageCallback = vailOnDecoded;
    esp_timer_create_args_t rxArgs = {};
    rxArgs.callback = vailRxTickCallback;
    rxArgs.name = "vail_rx_tick";
    esp_timer_create(&rxArgs, &vailRxTickTimer);
    esp_timer_start_periodic(vailRxTickTimer, 5000);

    // TX decoder
    if (vailTxTickTimer != nullptr) {
        esp_timer_stop(vailTxTickTimer);
        esp_timer_delete(vailTxTickTimer);
        vailTxTickTimer = nullptr;
    }
    delete vailTxDecoder;
    vailTxDecoder = (decoderType == DECODER_DIRECT)
        ? (MorseDecoder*) new MorseDecoderDirect(cwSpeed, cwSpeed, 30)
        : (MorseDecoder*) new MorseDecoderAdaptive(cwSpeed, cwSpeed, 30);
    vailTxDecoder->messageCallback = vailTxOnDecoded;
    esp_timer_create_args_t txArgs = {};
    txArgs.callback = vailTxTickCallback;
    txArgs.name = "vail_tx_tick";
    esp_timer_create(&txArgs, &vailTxTickTimer);
    esp_timer_start_periodic(vailTxTickTimer, 5000);

    vailDecodedCount = 0;
    vailDecodedNeedsUpdate = true;
}

static void stopVailDecoders() {
    if (vailRxTickTimer != nullptr) {
        esp_timer_stop(vailRxTickTimer);
        esp_timer_delete(vailRxTickTimer);
        vailRxTickTimer = nullptr;
    }
    if (vailTxTickTimer != nullptr) {
        esp_timer_stop(vailTxTickTimer);
        esp_timer_delete(vailTxTickTimer);
        vailTxTickTimer = nullptr;
    }
    delete vailRxDecoder; vailRxDecoder = nullptr;
    delete vailTxDecoder; vailTxDecoder = nullptr;
    vailRxTickPending = false;
    vailTxTickPending = false;
}

// Playback state machine variables
static bool isPlaying = false;
static size_t playbackIndex = 0;
static unsigned long playbackElementStart = 0;
static int playbackToneFrequency = 0;  // Current playback tone frequency

// Chat mode state
bool vailChatMode = false;  // false = vail info, true = chat view
const int MAX_CHAT_MESSAGES = 20;  // Keep last 20 messages
const int MAX_CHAT_INPUT = 40;     // Max 40 chars per message
const int MAX_ROOM_NAME = 30;      // Max room name length

struct ChatMessage {
  char callsign[VAIL_CALLSIGN_LEN];
  char message[128];  // received chat can be longer than our 40-char input
  unsigned long timestamp;
};
std::vector<ChatMessage> chatHistory;

// Room selection state (custom room being typed on the LVGL screen)
char roomInput[MAX_ROOM_NAME + 1] = "";
struct RoomInfo {
  char name[VAIL_CHANNEL_LEN];
  int users;
  bool isPrivate;
};
std::vector<RoomInfo> activeRooms;

// User list state
struct UserInfo {
  char callsign[VAIL_CALLSIGN_LEN];
  uint8_t txTone;
};
std::vector<UserInfo> connectedUsers;

// Forward declarations
void startVailRepeater();
void vailKeyerCallback(bool txOn, int element);
void updateVailRepeater();
void connectToVail(const char* channel);
void disconnectFromVail();
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void sendVailMessage(std::vector<uint16_t> durations, int64_t timestamp = 0);
void sendChatMessage(const char* message);
void addChatMessage(const char* callsign, const char* message);
void sendInitialMessage();
void sendKeepalive();
void processReceivedMessage(String jsonPayload);
void playbackMessages();
int64_t getCurrentTimestamp();
void updateVailPaddles();

// Convert MIDI note number to frequency (Hz)
// Formula: frequency = 440 * 2^((note - 69) / 12)
float midiNoteToFrequency(uint8_t note) {
  if (note == 0) return 440.0;  // Default to A4 if not specified
  return 440.0 * pow(2.0, (note - 69) / 12.0);
}

// Get current timestamp in milliseconds (Unix epoch)
int64_t getCurrentTimestamp() {
  // Prefer the SERVER-derived clock (millis + skew, EMA-updated from every
  // empty-Duration server message). Receivers schedule playback against the
  // server clock, so any NTP-vs-server difference in our stamps shows up on
  // their end as "increase receive delay" errors. NTP is only the fallback
  // until the first server clock-sync message arrives.
  if (clockSkewSamples > 0) {
    return (int64_t)millis() + clockSkew;
  }

  struct timeval tv;
  gettimeofday(&tv, NULL);
  int64_t timestamp = (int64_t)(tv.tv_sec) * 1000LL + (int64_t)(tv.tv_usec / 1000);

  // If timestamp is unreasonably small, we don't have NTP time yet either
  if (timestamp < 1000000000000LL) {
    timestamp = (int64_t)millis() + clockSkew;
  }

  return timestamp;
}


// Start Vail repeater mode
void startVailRepeater() {
  vailState = VAIL_DISCONNECTED;
  strlcpy(statusText, "Enter channel name", sizeof(statusText));
  vailIsTransmitting = false;
  rxQueue.clear();
  vailTxDurations.clear();

  // Initialize keyer (runs in main loop on Core 1, like practice mode)
  vailDitPressed = false;
  vailDahPressed = false;
  vailLastStateChangeTime = 0;
  vailLastToneState = false;
  vailToneStartTimestamp = 0;
  int ditDuration = DIT_DURATION(cwSpeed);
  vailKeyer = getKeyer(cwKeyType);
  vailKeyer->reset();
  vailKeyer->setDitDuration(ditDuration);
  vailKeyer->setTxCallback(vailKeyerCallback);

  // Initialize chat mode
  vailChatMode = false;
  chatHistory.clear();

  // Initialize room selection
  roomInput[0] = '\0';
  activeRooms.clear();

  // Initialize user list
  connectedUsers.clear();

  // UI is now handled by LVGL - see lv_mode_screens.h
}

// Connect to Vail repeater
void connectToVail(const char* channel) {
  // Check internet connectivity first
  InternetStatus inetStatus = getInternetStatus();
  if (inetStatus != INET_CONNECTED) {
    Serial.println("[Vail] Cannot connect - no internet connectivity");
    vailState = VAIL_ERROR;
    if (inetStatus == INET_WIFI_ONLY) {
      strlcpy(statusText, "WiFi connected but no internet", sizeof(statusText));
    } else {
      strlcpy(statusText, "No WiFi connection", sizeof(statusText));
    }
    return;
  }

  strlcpy(vailChannel, channel, sizeof(vailChannel));
  saveVailSettings();  // Persist room selection for next boot

  // Clear any stale decoded state from a previous room — decoded text only
  // appears on the dedicated "Decoder" room.
  vailDecodedCount = 0;
  vailDecodedNeedsUpdate = true;

  vailState = VAIL_CONNECTING;
  strlcpy(statusText, "Connecting...", sizeof(statusText));

  Serial.print("Connecting to Vail repeater: ");
  Serial.println(channel);

  // WebSocket connection with subprotocol
  char path[64];
  snprintf(path, sizeof(path), "/chat?repeater=%s", channel);

  Serial.println("WebSocket connecting...");
  Serial.print("URL: wss://");
  Serial.print(vailServer);
  Serial.print(":");
  Serial.print(vailPort);
  Serial.println(path);

  // Set event handler first
  webSocket.onEvent(webSocketEvent);

  // Enable aggressive heartbeat to prevent disconnections
  // Parameters: ping interval (10s), pong timeout (5s), max retries (3)
  // Aggressive settings prevent Cloud Run and load balancer timeouts
  webSocket.enableHeartbeat(10000, 5000, 3);

  // Set subprotocol using extra headers (WebSocketsClient method)
  webSocket.setExtraHeaders("Sec-WebSocket-Protocol: json.vailmorse.com");

  // Simple beginSSL - library should handle SSL automatically
  webSocket.beginSSL(vailServer, vailPort, path);

  // Set reconnect interval
  webSocket.setReconnectInterval(5000);

  Serial.println("WebSocket setup complete");
}

// Disconnect from Vail
void disconnectFromVail() {
  if (vailState == VAIL_DISCONNECTED) {
    return;  // Already disconnected
  }

  Serial.println("[Vail] Disconnecting...");

  // Stop any ongoing tone playback (use non-blocking request API)
  requestStopTone();

  // Send disconnect to server
  webSocket.disconnect();

  // Wait for disconnect to complete (with timeout)
  // This ensures the server receives the close frame
  unsigned long startTime = millis();
  const unsigned long timeout = 500;  // 500ms timeout

  while (vailState != VAIL_DISCONNECTED && (millis() - startTime) < timeout) {
    webSocket.loop();  // Process pending WebSocket events
    delay(10);
  }

  // Force state to disconnected if timeout occurred
  vailState = VAIL_DISCONNECTED;
  strlcpy(statusText, "Disconnected", sizeof(statusText));

  // Stop any repeater playback
  isPlaying = false;
  playbackToneFrequency = 0;

  // Clear all queues and state to prevent stale data on reconnect
  rxQueue.clear();
  vailTxDurations.clear();
  recentTxTimestamps.clear();
  recentChatTimestamps.clear();
  chatHistory.clear();
  vailPendingSendCount = 0;
  connectedUsers.clear();
  activeRooms.clear();
  clockSkewSamples = 0;  // Reset clock sync on disconnect
  vailIsTransmitting = false;

  // Reset keyer state
  vailDitPressed = false;
  vailDahPressed = false;
  vailLastStateChangeTime = 0;
  vailLastToneState = false;
  vailToneStartTimestamp = 0;
  if (vailKeyer) {
    vailKeyer->reset();
  }

  // Stop and free RX/TX decoder esp_timers and decoder objects so back-nav
  // doesn't leave zombie timers firing into freed callback context.
  stopVailDecoders();

  Serial.println("[Vail] Disconnected and state cleared");
}

// WebSocket event handler
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected");
      vailState = VAIL_DISCONNECTED;
      strlcpy(statusText, "Disconnected", sizeof(statusText));
      break;

    case WStype_CONNECTED:
      {
        Serial.println("[WS] Connected");
        vailState = VAIL_CONNECTED;
        strlcpy(statusText, "Connected", sizeof(statusText));

        // Get the URL we connected to
        String url = String((char*)payload);
        Serial.print("[WS] Connected to: ");
        Serial.println(url);

        // Start decoders — TX always needed for chat compose, RX for display
        startVailDecoders();

        // Send initial connection message (required by API)
        sendInitialMessage();
        lastKeepaliveTime = millis();  // Reset keepalive timer
      }
      break;

    case WStype_TEXT:
      VAIL_LOG("[WS] Received: %s\n", payload);
      processReceivedMessage(String((char*)payload));
      break;

    case WStype_ERROR:
      Serial.println("[WS] Error");
      vailState = VAIL_ERROR;
      strlcpy(statusText, "Connection error", sizeof(statusText));
      break;

    case WStype_PING:
      Serial.println("[WS] Ping");
      break;

    case WStype_PONG:
      Serial.println("[WS] Pong");
      break;

    default:
      break;
  }
}

// Process received JSON message
void processReceivedMessage(String jsonPayload) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonPayload);

  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  VailMessage msg;
  msg.timestamp = doc["Timestamp"].as<int64_t>();
  msg.clients = doc["Clients"].as<uint16_t>();
  msg.txTone = doc["TxTone"] | 69;  // Default to MIDI note 69 (A4 = 440Hz) if not specified

  // Update client count (LVGL will update on next frame)
  if (connectedClients != msg.clients) {
    connectedClients = msg.clients;
  }

  // Parse Users array (optional - for future UI enhancements)
  if (doc.containsKey("Users")) {
    JsonArray users = doc["Users"];
    (void)users;
  }

  // Parse UsersInfo array (optional - detailed user info with TX tones)
  if (doc.containsKey("UsersInfo")) {
    connectedUsers.clear();
    JsonArray usersInfo = doc["UsersInfo"];
    for (JsonVariant userInfo : usersInfo) {
      UserInfo user;
      strlcpy(user.callsign, userInfo["callsign"] | "Unknown", sizeof(user.callsign));
      user.txTone = userInfo["txTone"] | 69;
      connectedUsers.push_back(user);

    }
  }

  // Parse Rooms array (active public rooms)
  if (doc.containsKey("Rooms")) {
    activeRooms.clear();
    JsonArray rooms = doc["Rooms"];
    for (JsonVariant roomVar : rooms) {
      RoomInfo room;
      strlcpy(room.name, roomVar["name"] | "Unknown", sizeof(room.name));
      room.users = roomVar["users"] | 0;
      room.isPrivate = roomVar["private"] | false;
      activeRooms.push_back(room);
    }
    VAIL_LOG("Active rooms: %d\n", (int)activeRooms.size());
  }

  // Check for text chat message
  bool isChatMessage = false;
  if (doc.containsKey("Text") && !doc["Text"].isNull()) {
    const char* text = doc["Text"] | "";
    if (text[0] != '\0') {
      isChatMessage = true;
      const char* callsign = doc["Callsign"] | "Unknown";

      // Don't add our own echo again (we already added when sending). Match
      // by exact timestamp, NOT callsign - a callsign comparison silently
      // muted any other station using the same callsign as ours.
      bool isOwnChat = false;
      for (int64_t ts : recentChatTimestamps) {
        if (msg.timestamp == ts) {
          isOwnChat = true;
          break;
        }
      }
      if (!isOwnChat) {
        addChatMessage(callsign, text);
      }
    }
  }

  JsonArray durations = doc["Duration"];
  if (durations.size() > 0) {
    // Check if this is our own message echoed back. Per the protocol spec this
    // is an EXACT timestamp match - the server echoes our Timestamp untouched.
    // (The old +/-2000ms window also swallowed OTHER users' keying whenever it
    // landed within 2s of ours, i.e. any time both sides keyed at once.)
    bool isEcho = false;
    for (int64_t ts : recentTxTimestamps) {
      if (msg.timestamp == ts) {
        isEcho = true;
        break;
      }
    }
    if (isEcho) {
      VAIL_LOG("Ignoring echo of our own transmission\n");
      return;
    }

    for (uint16_t duration : durations) {
      msg.durations.push_back(duration);
    }

    // Add to receive queue with playback delay
    rxQueue.push_back(msg);

    VAIL_LOG("Queued message: %d elements at tone %d\n", (int)msg.durations.size(), (int)msg.txTone);
  } else if (!isChatMessage) {
    // Empty duration + no Text = clock sync message (keepalive/room update,
    // stamped with server-now). Chat messages are EXCLUDED: the server replays
    // up to 50 history messages with their ORIGINAL timestamps on join, and
    // treating those as sync poisoned the skew by hours - every subsequent
    // keyed element was stamped in the past and the server kicked us
    // ("Your clock is off by too much") the moment we sent.
    //
    // Direct assignment, last message wins - same as the reference web client.
    // (An EMA here converges too slowly to recover from one bad sample before
    // the server's 10s kick threshold, and float math on the epoch-scale skew
    // quantizes to ~131s steps. Keep this integer and simple.)
    clockSkew = msg.timestamp - (int64_t)millis();
    clockSkewSamples++;

    VAIL_LOG("Clock sync: skew=%lld (samples=%d)\n", (long long)clockSkew, clockSkewSamples);
  }
}

// Send initial connection message (required by new API)
void sendInitialMessage() {
  StaticJsonDocument<256> doc;
  doc["Timestamp"] = getCurrentTimestamp();
  JsonArray duration = doc.createNestedArray("Duration");  // Empty array
  doc["Callsign"] = vailCallsign;
  doc["TxTone"] = vailTxTone;
  doc["Private"] = false;  // Public room
  doc["Decoder"] = vailIsOnDecoderChannel();  // True only on the dedicated Decoder room

  String output;
  serializeJson(doc, output);

  Serial.print("Sending initial message: ");
  Serial.println(output);

  webSocket.sendTXT(output);
}

// Send keepalive message (required every 30 seconds)
void sendKeepalive() {
  if (vailState != VAIL_CONNECTED) {
    return;
  }

  StaticJsonDocument<256> doc;
  doc["Timestamp"] = getCurrentTimestamp();
  JsonArray duration = doc.createNestedArray("Duration");  // Empty array
  doc["Callsign"] = vailCallsign;
  doc["TxTone"] = vailTxTone;
  doc["Private"] = false;  // Public room
  doc["Decoder"] = vailIsOnDecoderChannel();  // True only on the dedicated Decoder room

  String output;
  serializeJson(doc, output);

  VAIL_LOG("Sending keepalive: %s\n", output.c_str());

  webSocket.sendTXT(output);
}

// Send message to Vail repeater. Stack buffers only - this runs on the
// audio-critical loop during keying, so no String/heap churn.
void sendVailMessage(std::vector<uint16_t> durations, int64_t timestamp) {
  if (vailState != VAIL_CONNECTED) {
    VAIL_LOG("Not connected to Vail\n");
    return;
  }

  StaticJsonDocument<512> doc;

  // Use provided timestamp (when tone started), or get current time if not provided
  if (timestamp == 0) {
    timestamp = getCurrentTimestamp();
  }
  doc["Timestamp"] = timestamp;
  // Note: Do NOT send Clients field - server populates this
  doc["Callsign"] = vailCallsign;  // Add callsign to all messages
  doc["TxTone"] = vailTxTone;      // Add TX tone to all messages

  JsonArray durArray = doc.createNestedArray("Duration");
  for (uint16_t dur : durations) {
    durArray.add(dur);
  }

  char output[384];
  size_t outLen = serializeJson(doc, output, sizeof(output));
  if (outLen == 0 || outLen >= sizeof(output)) return;

  VAIL_LOG("Sending (ts=%lld): %s\n", (long long)timestamp, output);

  // Remember this timestamp to filter out the echo (use queue for rapid keying)
  recentTxTimestamps.push_back(timestamp);
  while (recentTxTimestamps.size() > MAX_TX_TIMESTAMPS) {
    recentTxTimestamps.pop_front();
  }

  webSocket.sendTXT(output, outLen);
}

// Transmit any element sends queued by the keyer callback. Called from
// updateVailRepeater() after keyer servicing, so JSON building and the TLS
// websocket write never run inside an element's tone window.
static void flushVailPendingSends() {
  for (int i = 0; i < vailPendingSendCount; i++) {
    sendVailMessage({vailPendingSends[i].durMs}, vailPendingSends[i].ts);
  }
  vailPendingSendCount = 0;
}

// Keyer callback - called from Core 1 main loop (like practice mode)
void vailKeyerCallback(bool txOn, int element) {
  unsigned long now = millis();

  if (txOn) {
    // Use Core 0 audio task for sidetone so it auto-refills the I2S DMA
    // independent of Core 1's loop (webSocket.loop() can stall long enough
    // to underrun the buffer if we drive continueTone from here).
    requestStartTone(cwTone);

    // Stop any received-message playback in progress
    if (isPlaying) {
      isPlaying = false;
      playbackToneFrequency = 0;
    }

    // Feed inter-element silence to decoder
    if (vailTxDecoder && vailLastStateChangeTime > 0 && vailLastToneState == false) {
      float silenceDuration = now - vailLastStateChangeTime;
      if (silenceDuration > 0) vailTxDecoder->addTiming(-silenceDuration);
    }
    // Vail protocol: Timestamp = when the tone STARTED. Capture it here at
    // key-down. Stamping at key-up shifts each element late by its own length,
    // which makes a dah's remote playback window swallow the following dit
    // (receivers heard only dahs during iambic keying).
    vailToneStartTimestamp = getCurrentTimestamp();

    // Keyer modes (iambic/ultimatic) generate fixed-length elements that
    // always complete, so the duration is already known - send NOW. The
    // packet reaches receivers ~network-lag after its stamp instead of
    // element-length later (the web client sends at key-up, which is why it
    // needs receive delay > dah length; we can do better). Straight key
    // duration is unknown until key-up, so it still sends there.
    vailSentAtKeyDown = false;
    if (!vailListenOnly && !vailChatMode && cwKeyType != KEY_STRAIGHT) {
      uint16_t elemMs = (uint16_t)((element == PADDLE_DAH) ? DIT_DURATION(cwSpeed) * 3
                                                           : DIT_DURATION(cwSpeed));
      queueVailElementSend(elemMs, vailToneStartTimestamp);
      vailSentAtKeyDown = true;
      vailTxStartTime = now;
      if (!vailIsTransmitting) {
        vailIsTransmitting = true;
        vailTxDurations.clear();
      }
    }
    vailLastStateChangeTime = now;
    vailLastToneState = true;
  } else {
    requestStopTone();

    if (vailLastToneState && vailLastStateChangeTime > 0) {
      float toneDuration = now - vailLastStateChangeTime;
      if (toneDuration > 0) {
        // Feed to decoder
        if (vailTxDecoder) vailTxDecoder->addTiming(toneDuration);

        // Send to network, stamped with the tone's START time (key-down).
        // Keyer-mode elements were already sent at key-down; this path only
        // fires for straight key, where the duration is measured here.
        if (!vailListenOnly && !vailChatMode && !vailSentAtKeyDown) {
          int64_t serverTs = (vailToneStartTimestamp > 0) ? vailToneStartTimestamp
                                                          : getCurrentTimestamp();
          queueVailElementSend((uint16_t)toneDuration, serverTs);
          vailTxStartTime = now;
          if (!vailIsTransmitting) {
            vailIsTransmitting = true;
            vailTxDurations.clear();
          }
        }
        vailSentAtKeyDown = false;
      }
    }
    vailLastStateChangeTime = now;
    vailLastToneState = false;
  }
}

// Update Vail repeater (call in main loop)
// Note: Legacy UI rendering removed - LVGL handles all UI updates via updateVailScreenLVGL()
void updateVailRepeater() {
  webSocket.loop();

  // Send keepalive every 15 seconds (aggressive keepalive prevents Cloud Run and load balancer timeouts)
  // This matches the web repeater's 15s interval for optimal connection stability
  if (vailState == VAIL_CONNECTED && (millis() - lastKeepaliveTime > 15000)) {
    sendKeepalive();
    lastKeepaliveTime = millis();
  }

  // Service decoder ticks (pending flags set by esp_timer ISR)
  if (vailRxTickPending && vailRxDecoder) {
    vailRxTickPending = false;
    vailRxDecoder->tick();
  }
  if (vailTxTickPending && vailTxDecoder) {
    vailTxTickPending = false;
    vailTxDecoder->tick();
  }

  // Paddle + keyer (like practice mode — Core 1, no flag relay)
  if (vailKeyer) {
    bool newDitPressed, newDahPressed;
    getPaddleState(&newDitPressed, &newDahPressed);
    if (newDitPressed != vailDitPressed) {
      vailKeyer->key(PADDLE_DIT, newDitPressed);
      vailDitPressed = newDitPressed;
    }
    if (newDahPressed != vailDahPressed) {
      vailKeyer->key(PADDLE_DAH, newDahPressed);
      vailDahPressed = newDahPressed;
    }
    vailKeyer->tick(millis());
    // Core 0 audio task auto-refills the I2S buffer while a tone is active,
    // so no continueTone() call is needed here.

    // Transmit elements the keyer callback queued (kept out of the callback
    // so the TLS write never lands inside a tone window)
    flushVailPendingSends();

    // Reset transmission state after 2 seconds of inactivity
    if (vailIsTransmitting && !vailKeyer->isTxActive() && (millis() - vailTxStartTime > 2000)) {
      vailIsTransmitting = false;
    }
  }

  // Playback received messages
  playbackMessages();

  // Note: UI updates are now handled by LVGL via updateVailScreenLVGL()
}

// Playback received messages (non-blocking)
// Uses dual-core audio API: all audio requests are non-blocking, handled by Core 0
void playbackMessages() {
  // Don't play if transmitting - transmission has audio priority
  bool keyerActive = (vailKeyer && vailKeyer->isTxActive());
  if (vailIsTransmitting || keyerActive) {
    if (isPlaying) {
      // Stop playback if we started transmitting
      requestStopTone();  // Non-blocking - audio task handles stop
      isPlaying = false;
      playbackToneFrequency = 0;
    }
    return;
  }

  if (rxQueue.empty() && !isPlaying) return;

  // Audio task handles continuous buffer filling automatically
  // No need for continueTone() calls from UI loop - this was causing the distortion!

  int64_t now = getCurrentTimestamp();

  // Start playing if not already playing
  if (!isPlaying && !rxQueue.empty()) {
    VailMessage &msg = rxQueue[0];
    int64_t playTime = msg.timestamp + playbackDelay;

    if (now >= playTime) {
      VAIL_LOG("Starting playback of %d elements\n", (int)msg.durations.size());
      isPlaying = true;
      playbackIndex = 0;
      playbackElementStart = millis();

      // Start first element
      if (msg.durations.size() > 0) {
        // Feed the decoder (automatic on the Decoder room). Without this the
        // first tone of every message was skipped and decode came out garbled.
        if (vailIsOnDecoderChannel() && vailRxDecoder)
          vailRxDecoder->addTiming((float)msg.durations[0]);
        // Play at local cwTone (consistent experience) or sender's TX tone
        #if VAIL_USE_LOCAL_TONE_FOR_RECEIVE
          playbackToneFrequency = cwTone;
        #else
          playbackToneFrequency = midiNoteToFrequency(msg.txTone);
        #endif
        requestStartTone(playbackToneFrequency);  // Non-blocking - audio task starts tone
      }
    }
  }

  // Continue playing current message
  if (isPlaying && !rxQueue.empty()) {
    VailMessage &msg = rxQueue[0];

    // Check if current element is done
    unsigned long elapsed = millis() - playbackElementStart;
    if (elapsed >= msg.durations[playbackIndex]) {
      // Move to next element
      playbackIndex++;

      if (playbackIndex >= msg.durations.size()) {
        // Message complete
        requestStopTone();  // Non-blocking - audio task handles stop
        isPlaying = false;
        playbackIndex = 0;
        playbackToneFrequency = 0;
        rxQueue.erase(rxQueue.begin());
        VAIL_LOG("Playback complete\n");
      } else {
        // Start next element
        playbackElementStart = millis();

        uint16_t elemDur = msg.durations[playbackIndex];
        if (playbackIndex % 2 == 0) {
          // Even index = tone
          if (vailIsOnDecoderChannel() && vailRxDecoder)
            vailRxDecoder->addTiming((float)elemDur);
          #if VAIL_USE_LOCAL_TONE_FOR_RECEIVE
            playbackToneFrequency = cwTone;
          #else
            playbackToneFrequency = midiNoteToFrequency(msg.txTone);
          #endif
          requestStartTone(playbackToneFrequency);  // Non-blocking - audio task starts tone
        } else {
          // Odd index = silence
          if (vailIsOnDecoderChannel() && vailRxDecoder)
            vailRxDecoder->addTiming(-(float)elemDur);
          playbackToneFrequency = 0;
          requestStopTone();  // Non-blocking - audio task handles stop
        }
      }
    }
  }
}

// Add message to chat history
void addChatMessage(const char* callsign, const char* message) {
  ChatMessage msg;
  memset(&msg, 0, sizeof(msg));
  strlcpy(msg.callsign, callsign, sizeof(msg.callsign));
  strlcpy(msg.message, message, sizeof(msg.message));
  msg.timestamp = millis();

  chatHistory.push_back(msg);

  // Keep only last MAX_CHAT_MESSAGES
  while (chatHistory.size() > MAX_CHAT_MESSAGES) {
    chatHistory.erase(chatHistory.begin());
  }

  Serial.print("Chat: ");
  Serial.print(callsign);
  Serial.print(": ");
  Serial.println(message);

}

// Send text chat message over WebSocket. Text-only — no synthesized morse on
// the air. Per project policy, all over-the-air CW must be hand-keyed by the
// operator; chat is delivered as a Text payload with an empty Duration array.
void sendChatMessage(const char* message) {
  if (vailState != VAIL_CONNECTED) {
    Serial.println("Not connected - cannot send chat message");
    return;
  }

  StaticJsonDocument<512> doc;
  int64_t chatTs = getCurrentTimestamp();
  doc["Timestamp"] = chatTs;
  doc["Callsign"] = vailCallsign;
  doc["TxTone"] = vailTxTone;
  doc["Text"] = message;
  doc.createNestedArray("Duration");  // empty — never synthesize morse

  // Remember the timestamp so the server's echo of this chat is recognized as
  // our own (exact-match, like the morse echo filter). Filtering by callsign
  // is wrong: it mutes any OTHER station that happens to share our callsign.
  recentChatTimestamps.push_back(chatTs);
  while (recentChatTimestamps.size() > 8) {
    recentChatTimestamps.pop_front();
  }

  String output;
  serializeJson(doc, output);

  Serial.print("Sending chat message: ");
  Serial.println(output);

  webSocket.sendTXT(output);
}



#endif // VAIL_REPEATER_H
