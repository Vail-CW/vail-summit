/*
 * Web Practice Mode Module
 * Handles web-based practice mode where browser sends keying events
 * and device runs decoder, returning decoded text
 */

#ifndef WEB_PRACTICE_MODE_H
#define WEB_PRACTICE_MODE_H

#include "../../audio/morse_decoder_adaptive.h"
#include "../../audio/morse_wpm.h"
#include "../../core/config.h"

// Web practice decoder instance (separate from device practice mode)
MorseDecoderAdaptive webPracticeDecoder(20.0f);  // Default 20 WPM

// Forward declarations from web_practice_socket.h
extern void sendPracticeDecoded(const char* morse, const char* text);
extern void sendPracticeWPM(float wpm);
extern bool webPracticeModeActive;

/*
 * Decoder callback: character decoded
 */
void onWebPracticeDecoded(String morse, String text) {
    Serial.print("Web Practice Decoded: ");
    Serial.print(morse);
    Serial.print(" = ");
    Serial.println(text);

    // Send to browser via WebSocket
    sendPracticeDecoded(morse.c_str(), text.c_str());
}

/*
 * Decoder callback: speed detected
 */
void onWebPracticeSpeed(float wpm, float fwpm) {
    Serial.print("Web Practice Speed: ");
    Serial.print(wpm);
    Serial.println(" WPM");

    // Send to browser via WebSocket
    sendPracticeWPM(wpm);
}

/*
 * Initialize web practice mode (called from initializeModeInt)
 */
void initWebPracticeMode() {
    Serial.println("Initializing web practice mode");
    webPracticeDecoder.messageCallback = onWebPracticeDecoded;
    webPracticeDecoder.speedCallback = onWebPracticeSpeed;
    webPracticeDecoder.reset();
}

/*
 * Update function (called every loop iteration)
 * Web practice mode is mostly passive - decoder is fed via WebSocket
 */
void updateWebPracticeMode() {
    // No continuous updates needed - decoder is fed by WebSocket handler
}

#endif // WEB_PRACTICE_MODE_H
