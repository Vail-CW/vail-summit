/*
 * Web Memory Chain Mode - Device Side
 * Device handles morse decoding only. All game logic runs in browser.
 */

#ifndef WEB_MEMORY_CHAIN_MODE_H
#define WEB_MEMORY_CHAIN_MODE_H

#include "../../core/config.h"
#include "../../core/morse_code.h"
#include "web_memory_chain_socket.h"

// Forward declaration for decoded character relay
extern void sendMemoryChainDecoded(const char* morse, const char* text);

/*
 * Callback when decoder finishes a character - relay to browser
 */
void onWebMemoryDecoded(String morse, String text) {
    Serial.printf("Web Memory Chain decoded: %s = %s\n", morse.c_str(), text.c_str());
    sendMemoryChainDecoded(morse.c_str(), text.c_str());
}

/*
 * Initialize web memory chain mode (called from initializeModeInt)
 */
void initWebMemoryChainMode() {
    Serial.println("Initializing web Memory Chain mode");
    webMemoryChainDecoder.messageCallback = onWebMemoryDecoded;
    webMemoryChainDecoder.reset();
}

/*
 * Update function (called every loop iteration)
 * Browser handles all game logic - device just runs decoder
 */
void updateWebMemoryChain() {
    // Decoder processes timings via callbacks automatically
    // Game logic runs entirely in browser
}

#endif // WEB_MEMORY_CHAIN_MODE_H
