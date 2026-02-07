/*
 * Web Practice WebSocket Handler
 * Handles WebSocket connections for web-based practice mode
 * Receives timing data from browser and sends decoded results
 */

#ifndef WEB_PRACTICE_SOCKET_H
#define WEB_PRACTICE_SOCKET_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Forward declaration
extern bool webPracticeModeActive;
extern AsyncWebSocket* practiceWebSocket;

/*
 * WebSocket Event Handler for Practice Mode
 */
void onPracticeWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  extern MorseDecoderAdaptive webPracticeDecoder;  // Decoder instance for web practice mode

  switch(type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      webPracticeModeActive = true;
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      webPracticeModeActive = false;

      {
        extern volatile bool webModeDisconnectPending;
        extern MenuMode currentMode;
        if (currentMode == MODE_WEB_PRACTICE) {
          webModeDisconnectPending = true;
        }
      }
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;  // Null terminate

        // Parse JSON message
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (char*)data);

        if (!error) {
          const char* msgType = doc["type"] | "";

          if (strcmp(msgType, "timing") == 0) {
            // Timing data from browser
            float duration = doc["duration"].as<float>();
            bool positive = doc["positive"].as<bool>();
            const char* key = doc["key"] | "";

            Serial.printf("Received timing: %s, duration: %.1f ms, positive: %d\n",
                         key, duration, positive);

            // Feed timing to decoder
            if (positive) {
              webPracticeDecoder.addTiming(duration);
            } else {
              webPracticeDecoder.addTiming(-duration);  // Negative for silence
            }
          }
          else if (strcmp(msgType, "start") == 0) {
            Serial.println("Practice mode start requested via WebSocket");
            webPracticeModeActive = true;
          }
        }
      }
      break;
    }

    case WS_EVT_ERROR:
      Serial.printf("WebSocket error: %u\n", *((uint16_t*)arg));
      break;

    case WS_EVT_PONG:
      // Ignore pong responses
      break;
  }
}

/*
 * Send decoded morse character to browser
 */
void sendPracticeDecoded(const char* morse, const char* text) {
  if (webPracticeModeActive && practiceWebSocket && practiceWebSocket->count() > 0) {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"type\":\"decoded\",\"morse\":\"%s\",\"text\":\"%s\"}", morse, text);
    practiceWebSocket->textAll(buf);
  }
}

/*
 * Send WPM update to browser
 */
void sendPracticeWPM(float wpm) {
  if (webPracticeModeActive && practiceWebSocket && practiceWebSocket->count() > 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"type\":\"wpm\",\"value\":%.1f}", wpm);
    practiceWebSocket->textAll(buf);
  }
}

#endif // WEB_PRACTICE_SOCKET_H
