/*
 * Web Hear It Type It WebSocket Handler
 * Minimal handler - game logic runs entirely in browser.
 * WebSocket is only used for connection status tracking.
 */

#ifndef WEB_HEAR_IT_SOCKET_H
#define WEB_HEAR_IT_SOCKET_H

#include <ESPAsyncWebServer.h>

// Forward declarations
extern bool webHearItModeActive;
extern AsyncWebSocket* hearItWebSocket;

/*
 * WebSocket Event Handler for Hear It Type It Mode
 */
void onHearItWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch(type) {
        case WS_EVT_CONNECT:
            Serial.printf("Hear It WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            webHearItModeActive = true;
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("Hear It WebSocket client #%u disconnected\n", client->id());
            webHearItModeActive = false;

            {
                extern volatile bool webModeDisconnectPending;
                extern MenuMode currentMode;
                if (currentMode == MODE_WEB_HEAR_IT) {
                    webModeDisconnectPending = true;
                }
            }
            break;

        case WS_EVT_DATA:
            // Browser handles all game logic - no device-side data processing needed
            break;

        case WS_EVT_ERROR:
            Serial.printf("Hear It WebSocket error: %u\n", *((uint16_t*)arg));
            break;

        case WS_EVT_PONG:
            break;
    }
}

#endif // WEB_HEAR_IT_SOCKET_H
