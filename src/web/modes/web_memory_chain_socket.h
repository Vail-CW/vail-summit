/*
 * Web Memory Chain - WebSocket Handler
 * Handles morse timing input and relays decoded characters to browser.
 * Game logic runs entirely in browser.
 */

#ifndef WEB_MEMORY_CHAIN_SOCKET_H
#define WEB_MEMORY_CHAIN_SOCKET_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "../../audio/morse_decoder_adaptive.h"

// WebSocket pointer (allocated on-demand)
AsyncWebSocket* memoryChainWebSocket = nullptr;

// State
bool webMemoryChainModeActive = false;
MorseDecoderAdaptive webMemoryChainDecoder(15.0f);

/*
 * Send decoded character to browser for game logic processing
 */
void sendMemoryChainDecoded(const char* morse, const char* text) {
    if (webMemoryChainModeActive && memoryChainWebSocket && memoryChainWebSocket->count() > 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"type\":\"decoded\",\"morse\":\"%s\",\"char\":\"%s\"}", morse, text);
        memoryChainWebSocket->textAll(buf);
    }
}

/*
 * WebSocket Event Handler
 */
void onMemoryChainWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch(type) {
        case WS_EVT_CONNECT:
            Serial.printf("Memory Chain WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            webMemoryChainModeActive = true;
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("Memory Chain WebSocket client #%u disconnected\n", client->id());
            webMemoryChainModeActive = false;
            {
                extern volatile bool webModeDisconnectPending;
                extern MenuMode currentMode;
                if (currentMode == MODE_WEB_MEMORY_CHAIN) {
                    webModeDisconnectPending = true;
                }
            }
            break;

        case WS_EVT_DATA: {
            AwsFrameInfo *info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                data[len] = 0;

                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, (char*)data);

                if (!error) {
                    const char* msgType = doc["type"] | "";

                    if (strcmp(msgType, "timing") == 0) {
                        float duration = doc["duration"].as<float>();
                        bool positive = doc["positive"].as<bool>();

                        if (positive) {
                            webMemoryChainDecoder.addTiming(duration);
                        } else {
                            webMemoryChainDecoder.addTiming(-duration);
                        }
                    }
                }
            }
            break;
        }

        case WS_EVT_ERROR:
            Serial.printf("Memory Chain WebSocket error: %u\n", *((uint16_t*)arg));
            break;

        case WS_EVT_PONG:
            break;
    }
}

#endif // WEB_MEMORY_CHAIN_SOCKET_H
