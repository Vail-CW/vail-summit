/*
 * POTA Recorder
 * Records QSOs during POTA activations by decoding your own keying
 *
 * Flow: Paddle Input -> Timing Capture -> Morse Decoder -> QSO Parser -> Logger
 */

#ifndef POTA_RECORDER_H
#define POTA_RECORDER_H

#include <Arduino.h>
#include <Preferences.h>
#include "../audio/morse_decoder.h"
#include "pota_qso_parser.h"
#include "../qso/qso_logger.h"
#include "../qso/qso_logger_storage.h"
#include "../radio/radio_output.h"
#include "../settings/settings_cw.h"

// ============================================
// State
// ============================================

// Recorder active state
static bool potaRecorderActive = false;

// Morse decoder for timing-to-text conversion
static MorseDecoder* potaDecoder = nullptr;

// QSO parser for exchange recognition
static POTAQSOParser* potaParser = nullptr;

// Timing state for callback
static unsigned long potaLastKeyTime = 0;
static bool potaLastKeyState = false;

// Decoded text buffer (circular, for display)
#define POTA_TEXT_BUFFER_SIZE 256
static char potaDecodedText[POTA_TEXT_BUFFER_SIZE];
static int potaTextWritePos = 0;

// Session stats
static int potaSessionQSOCount = 0;
static unsigned long potaSessionStartTime = 0;

// Settings (persisted)
static char potaMyCallsign[12] = "";
static char potaMyPark[12] = "";

// Preferences
static Preferences potaPrefs;

// ============================================
// Forward Declarations
// ============================================

void initPOTARecorder();
void startPOTARecorder();
void stopPOTARecorder();
void updatePOTARecorder();
void loadPOTASettings();
void savePOTASettings();
void onPOTACharDecoded(String morse, String text);
void potaKeyingCallback(bool keyDown, unsigned long timestamp);
void clearPOTATextBuffer();

// ============================================
// Implementation
// ============================================

/*
 * Initialize POTA Recorder (called once at startup)
 */
void initPOTARecorder() {
    // Allocate decoder and parser
    if (!potaDecoder) {
        potaDecoder = new MorseDecoder(cwSpeed);
        potaDecoder->messageCallback = onPOTACharDecoded;
    }
    if (!potaParser) {
        potaParser = new POTAQSOParser();
    }

    // Load settings
    loadPOTASettings();

    Serial.println("[POTA Recorder] Initialized");
}

/*
 * Load POTA settings from flash
 */
void loadPOTASettings() {
    potaPrefs.begin("pota", true);
    String call = potaPrefs.getString("callsign", "");
    String park = potaPrefs.getString("park", "");
    potaPrefs.end();

    strncpy(potaMyCallsign, call.c_str(), sizeof(potaMyCallsign) - 1);
    strncpy(potaMyPark, park.c_str(), sizeof(potaMyPark) - 1);

    Serial.printf("[POTA Recorder] Loaded settings: callsign=%s park=%s\n",
                  potaMyCallsign, potaMyPark);
}

/*
 * Save POTA settings to flash
 */
void savePOTASettings() {
    potaPrefs.begin("pota", false);
    potaPrefs.putString("callsign", potaMyCallsign);
    potaPrefs.putString("park", potaMyPark);
    potaPrefs.end();

    Serial.printf("[POTA Recorder] Saved settings: callsign=%s park=%s\n",
                  potaMyCallsign, potaMyPark);
}

/*
 * Set my callsign (for parser)
 */
void setPOTACallsign(const char* callsign) {
    strncpy(potaMyCallsign, callsign, sizeof(potaMyCallsign) - 1);
    potaMyCallsign[sizeof(potaMyCallsign) - 1] = '\0';
    if (potaParser) {
        potaParser->setMyCallsign(potaMyCallsign);
    }
}

/*
 * Set my park reference (for parser)
 */
void setPOTAPark(const char* park) {
    strncpy(potaMyPark, park, sizeof(potaMyPark) - 1);
    potaMyPark[sizeof(potaMyPark) - 1] = '\0';
    if (potaParser) {
        potaParser->setMyPark(potaMyPark);
    }
}

/*
 * Get current callsign
 */
const char* getPOTACallsign() {
    return potaMyCallsign;
}

/*
 * Get current park reference
 */
const char* getPOTAPark() {
    return potaMyPark;
}

/*
 * Clear decoded text buffer
 */
void clearPOTATextBuffer() {
    memset(potaDecodedText, 0, POTA_TEXT_BUFFER_SIZE);
    potaTextWritePos = 0;
}

/*
 * Callback when morse decoder produces text
 */
void onPOTACharDecoded(String morse, String text) {
    if (!potaRecorderActive) return;

    Serial.printf("[POTA Recorder] Decoded: %s -> %s\n", morse.c_str(), text.c_str());

    // Add to text buffer
    for (int i = 0; i < text.length(); i++) {
        potaDecodedText[potaTextWritePos] = text[i];
        potaTextWritePos = (potaTextWritePos + 1) % (POTA_TEXT_BUFFER_SIZE - 1);
        potaDecodedText[potaTextWritePos] = '\0';  // Null terminate
    }

    // Feed to QSO parser
    if (potaParser) {
        potaParser->feedText(text.c_str());
    }
}

/*
 * Keying callback - receives timing data from radio_output.h
 */
void potaKeyingCallback(bool keyDown, unsigned long timestamp) {
    if (!potaRecorderActive || !potaDecoder) return;

    if (keyDown && !potaLastKeyState) {
        // Key down - send previous silence duration
        if (potaLastKeyTime > 0) {
            float silence = timestamp - potaLastKeyTime;
            potaDecoder->addTiming(-silence);
        }
        potaLastKeyTime = timestamp;
        potaLastKeyState = true;
    }
    else if (!keyDown && potaLastKeyState) {
        // Key up - send tone duration
        float tone = timestamp - potaLastKeyTime;
        potaDecoder->addTiming(tone);
        potaLastKeyTime = timestamp;
        potaLastKeyState = false;
    }
}

/*
 * Start POTA recording session
 */
void startPOTARecorder() {
    if (potaRecorderActive) return;

    // Initialize if needed
    if (!potaDecoder) {
        initPOTARecorder();
    }

    // Reset decoder with current WPM
    potaDecoder->setWPM(cwSpeed);
    potaDecoder->reset();

    // Reset parser with current settings
    if (potaParser) {
        potaParser->reset();
        potaParser->setMyCallsign(potaMyCallsign);
        potaParser->setMyPark(potaMyPark);
    }

    // Clear text buffer
    clearPOTATextBuffer();

    // Reset timing state
    potaLastKeyTime = 0;
    potaLastKeyState = false;

    // Reset session stats
    potaSessionQSOCount = 0;
    potaSessionStartTime = millis();

    // Install keying callback
    radioKeyingCallback = potaKeyingCallback;

    potaRecorderActive = true;

    Serial.printf("[POTA Recorder] Started: callsign=%s park=%s wpm=%d\n",
                  potaMyCallsign, potaMyPark, cwSpeed);
}

/*
 * Stop POTA recording session
 */
void stopPOTARecorder() {
    if (!potaRecorderActive) return;

    // Flush any remaining timing data
    if (potaDecoder) {
        potaDecoder->flush();
    }

    // Remove keying callback
    radioKeyingCallback = nullptr;

    potaRecorderActive = false;

    Serial.printf("[POTA Recorder] Stopped: %d QSOs logged\n", potaSessionQSOCount);
}

/*
 * Check if recorder is active
 */
bool isPOTARecorderActive() {
    return potaRecorderActive;
}

/*
 * Get decoded text buffer for display
 */
const char* getPOTADecodedText() {
    return potaDecodedText;
}

/*
 * Get QSO parser (for UI to read state)
 */
POTAQSOParser* getPOTAParser() {
    return potaParser;
}

/*
 * Get session QSO count
 */
int getPOTASessionQSOCount() {
    return potaSessionQSOCount;
}

/*
 * Get session duration in seconds
 */
unsigned long getPOTASessionDuration() {
    if (!potaRecorderActive || potaSessionStartTime == 0) return 0;
    return (millis() - potaSessionStartTime) / 1000;
}

/*
 * Update POTA recorder (called from main loop)
 * Checks for new QSOs and saves them
 */
void updatePOTARecorder() {
    if (!potaRecorderActive || !potaParser) return;

    // Check for new QSO
    if (potaParser->hasNewQSO()) {
        POTAQSORecord record = potaParser->getLastQSO();
        potaSessionQSOCount++;

        Serial.printf("[POTA Recorder] New QSO: %s RST:%s QTH:%s\n",
                      record.theirCallsign, record.rstSent, record.stateReceived);

        // Convert to Summit QSO struct and save
        QSO qso;
        memset(&qso, 0, sizeof(QSO));

        strncpy(qso.callsign, record.theirCallsign, sizeof(qso.callsign) - 1);
        strncpy(qso.rst_sent, record.rstSent, sizeof(qso.rst_sent) - 1);
        strncpy(qso.rst_rcvd, record.rstReceived, sizeof(qso.rst_rcvd) - 1);
        strncpy(qso.qth, record.stateReceived, sizeof(qso.qth) - 1);
        strncpy(qso.my_pota_ref, record.myPark, sizeof(qso.my_pota_ref) - 1);
        strncpy(qso.their_pota_ref, record.theirPark, sizeof(qso.their_pota_ref) - 1);
        strncpy(qso.operator_call, record.myCallsign, sizeof(qso.operator_call) - 1);

        strncpy(qso.mode, "CW", sizeof(qso.mode) - 1);
        qso.id = record.timestamp;

        // Copy date/time strings directly (format already matches)
        strncpy(qso.date, record.qsoDate, sizeof(qso.date) - 1);
        strncpy(qso.time_on, record.timeOn, sizeof(qso.time_on) - 1);

        // Save to SD card
        if (saveQSO(qso)) {
            Serial.println("[POTA Recorder] QSO saved to SD card");
        } else {
            Serial.println("[POTA Recorder] ERROR: Failed to save QSO");
        }
    }
}

#endif // POTA_RECORDER_H
