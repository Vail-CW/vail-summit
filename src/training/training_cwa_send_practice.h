/*
 * CW Academy Training - Sending Practice Mode
 * Send morse code with paddle/key and receive feedback
 */

#ifndef TRAINING_CWA_SEND_PRACTICE_H
#define TRAINING_CWA_SEND_PRACTICE_H

#include "training_cwa_core.h"  // Same folder
#include "training_cwa_copy_practice.h"  // Same folder - For generateCWAContent()
#include "../keyer/keyer.h"

// ============================================
// Sending Practice State
// ============================================

String cwaSendTarget = "";               // What student should send
String cwaSendDecoded = "";              // What student actually sent (decoded)
int cwaSendRound = 0;                    // Current round number
int cwaSendCorrect = 0;                  // Number correct this session
int cwaSendTotal = 0;                    // Total attempts this session
bool cwaSendWaitingForSend = false;      // Waiting for student to key
bool cwaSendShowingFeedback = false;     // Showing feedback
bool cwaSendShowReference = true;        // Show what to send
unsigned long cwaSendStartTime = 0;      // Session start time (for 15 min timer)
unsigned long cwaSendKeyStartTime = 0;   // When student started keying
bool cwaSendNeedsUIUpdate = false;       // Flag to trigger UI refresh when decoded text changes

// Decoder for sending practice
MorseDecoderAdaptive cwaSendDecoder(15, 15, 30);  // 15 WPM for sending practice

// Keyer state - using unified keyer module
static bool cwaSendDitPressed = false;
static bool cwaSendDahPressed = false;
static StraightKeyer* cwaSendKeyer = nullptr;
static int cwaSendDitDuration = 0;

// Decoder timing capture
unsigned long cwaSendLastStateChangeTime = 0;
bool cwaSendLastToneState = false;
unsigned long cwaSendLastElementTime = 0;

// Forward declaration for callback
void cwaSendKeyerCallback(bool txOn, int element);

// ============================================
// Round Management
// ============================================

/*
 * Start a new sending practice round
 */
void startCWASendRound() {
  cwaSendRound++;
  cwaSendTarget = generateCWAContent();
  cwaSendDecoded = "";
  cwaSendWaitingForSend = true;
  cwaSendShowingFeedback = false;
  cwaSendKeyStartTime = 0;

  // Reset decoder
  cwaSendDecoder.reset();
  cwaSendDecoder.flush();
  cwaSendLastStateChangeTime = 0;
  cwaSendLastToneState = false;
  cwaSendLastElementTime = 0;
}

// ============================================
// UI Functions
// ============================================

/*
 * Draw sending practice UI updates
 */
void drawCWASendingPracticeUI(LGFX& tft) {
  if (cwaUseLVGL) return;  // LVGL handles display
  // Clear main area
  tft.fillRect(0, 35, SCREEN_WIDTH, 130, COLOR_BACKGROUND);

  // Draw round and score
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 40);
  tft.print("Round: ");
  tft.print(cwaSendRound);
  tft.print("/10  Score: ");
  tft.print(cwaSendCorrect);
  tft.print("/");
  tft.print(cwaSendTotal);

  // Show session info
  tft.setCursor(10, 55);
  tft.print("Session ");
  tft.print(cwaSelectedSession);
  tft.print(" - ");
  tft.print(cwaMessageTypeNames[cwaSelectedMessageType]);

  if (cwaSendShowingFeedback) {
    // Show feedback
    tft.fillRect(8, 75, SCREEN_WIDTH - 16, 80, COLOR_BACKGROUND);

    bool correct = cwaSendDecoded.equalsIgnoreCase(cwaSendTarget);

    tft.setTextSize(2);
    tft.setTextColor(correct ? ST77XX_GREEN : ST77XX_RED);
    tft.setCursor(15, 85);
    tft.print(correct ? "Correct!" : "Incorrect");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(15, 105);
    tft.print("Target:  ");
    tft.setTextColor(ST77XX_WHITE);
    tft.print(cwaSendTarget);

    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(15, 120);
    tft.print("You sent: ");
    tft.setTextColor(correct ? ST77XX_GREEN : ST77XX_YELLOW);
    tft.print(cwaSendDecoded.length() > 0 ? cwaSendDecoded : "(nothing)");

    tft.setTextSize(1);
    tft.setTextColor(0x7BEF);
    tft.setCursor(60, 145);
    tft.print("Press any key to continue");

  } else {
    // Draw target box
    tft.drawRect(8, 75, SCREEN_WIDTH - 16, 50, ST77XX_CYAN);
    if (cwaSendShowReference) {
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_GREEN);
      tft.setCursor(15, 88);
      tft.print("Send: ");
      tft.setTextColor(ST77XX_WHITE);
      tft.print(cwaSendTarget);
    } else {
      tft.setTextSize(1);
      tft.setTextColor(0x7BEF);
      tft.setCursor(80, 95);
      tft.print("(Reference hidden)");
    }

    // Show what they've sent so far
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(15, 135);
    tft.print("Decoded: ");
    tft.setTextColor(ST77XX_YELLOW);
    tft.print(cwaSendDecoded.length() > 0 ? cwaSendDecoded : "(waiting...)");
  }
}

/*
 * Draw only the decoded text area (for real-time updates without full redraw)
 */
void drawCWASendDecodedOnly(LGFX& tft) {
  if (cwaUseLVGL) return;  // LVGL handles display
  // Only update if we're in the sending state (not showing feedback)
  if (cwaSendShowingFeedback) return;

  // Clear the decoded text area (y=135, text size 1 = 8px height)
  tft.fillRect(15, 135, SCREEN_WIDTH - 30, 12, COLOR_BACKGROUND);

  // Redraw decoded text
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(15, 135);
  tft.print("Decoded: ");
  tft.setTextColor(ST77XX_YELLOW);
  tft.print(cwaSendDecoded.length() > 0 ? cwaSendDecoded : "(waiting...)");
}

/*
 * Start CWA Sending Practice mode
 */
void startCWASendingPractice(LGFX& tft) {
  cwaSendRound = 0;
  cwaSendCorrect = 0;
  cwaSendTotal = 0;
  cwaSendShowReference = true;
  cwaSendStartTime = millis();

  // Set sending practice speed (15 WPM character speed)
  cwaSendDecoder.setWPM(15);
  cwaSendDitDuration = DIT_DURATION(15);

  // Initialize unified keyer
  cwaSendKeyer = getKeyer(cwKeyType);
  cwaSendKeyer->reset();
  cwaSendKeyer->setDitDuration(cwaSendDitDuration);
  cwaSendKeyer->setTxCallback(cwaSendKeyerCallback);
  cwaSendDitPressed = false;
  cwaSendDahPressed = false;

  // Setup decoder callback
  cwaSendDecoder.messageCallback = [](String morse, String text) {
    for (int i = 0; i < text.length(); i++) {
      cwaSendDecoded += text[i];
    }
    cwaSendNeedsUIUpdate = true;  // Trigger UI refresh
  };

  // Reinitialize I2S
  i2s_zero_dma_buffer(I2S_NUM_0);
  delay(50);

  // Start first round
  startCWASendRound();

  // Draw UI
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(10, 10);
  tft.print("CWA Sending Practice");

  // Draw round and score
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 40);
  tft.print("Round: ");
  tft.print(cwaSendRound);
  tft.print("/10  Score: ");
  tft.print(cwaSendCorrect);
  tft.print("/");
  tft.print(cwaSendTotal);

  // Show session info
  tft.setCursor(10, 55);
  tft.print("Session ");
  tft.print(cwaSelectedSession);
  tft.print(" - ");
  tft.print(cwaMessageTypeNames[cwaSelectedMessageType]);

  // Draw target box
  tft.drawRect(8, 75, SCREEN_WIDTH - 16, 50, ST77XX_CYAN);
  if (cwaSendShowReference) {
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(15, 88);
    tft.print("Send: ");
    tft.setTextColor(ST77XX_WHITE);
    tft.print(cwaSendTarget);
  } else {
    tft.setTextSize(1);
    tft.setTextColor(0x7BEF);
    tft.setCursor(80, 95);
    tft.print("(Reference hidden)");
  }

  // Instructions
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 140);
  tft.print("Use your key to send the message");

  tft.setTextColor(0x7BEF);
  tft.setCursor(10, 160);
  tft.print("P: Play target  R: Toggle reference");
  tft.setCursor(10, 175);
  tft.print("ENTER: Done  ESC: Exit");

  Serial.println("CWA Sending Practice started");
  Serial.print("Target: ");
  Serial.println(cwaSendTarget);
}

// ============================================
// Keyer Callback and Update
// ============================================

/*
 * Keyer callback - called by unified keyer when tone state changes
 */
void cwaSendKeyerCallback(bool txOn, int element) {
  unsigned long currentTime = millis();

  // Track when student first starts keying
  if (txOn && cwaSendKeyStartTime == 0) {
    cwaSendKeyStartTime = currentTime;
  }

  if (txOn) {
    // Tone starting
    if (cwaSendLastToneState == false) {
      if (cwaSendLastStateChangeTime > 0) {
        float silenceDuration = currentTime - cwaSendLastStateChangeTime;
        if (silenceDuration > 0) {
          cwaSendDecoder.addTiming(-silenceDuration);
        }
      }
      cwaSendLastStateChangeTime = currentTime;
      cwaSendLastToneState = true;
    }
    startTone(cwTone);
  } else {
    // Tone stopping
    if (cwaSendLastToneState == true) {
      float toneDuration = currentTime - cwaSendLastStateChangeTime;
      if (toneDuration > 0) {
        cwaSendDecoder.addTiming(toneDuration);
        cwaSendLastElementTime = currentTime;
      }
      cwaSendLastStateChangeTime = currentTime;
      cwaSendLastToneState = false;
    }
    stopTone();
  }
}

/*
 * Update sending practice (called from main loop)
 */
void updateCWASendingPractice() {
  if (!cwaSendWaitingForSend) return;
  if (!cwaSendKeyer) return;

  // Check for decoder timeout (flush after word gap)
  if (cwaSendLastElementTime > 0 && !cwaSendDitPressed && !cwaSendDahPressed) {
    unsigned long timeSinceLastElement = millis() - cwaSendLastElementTime;
    float wordGapDuration = MorseWPM::wordGap(15);  // 15 WPM sending speed

    if (timeSinceLastElement > wordGapDuration) {
      cwaSendDecoder.flush();
      cwaSendLastElementTime = 0;
    }
  }

  // Get paddle state from centralized handler (includes debounce)
  bool newDitPressed, newDahPressed;
  getPaddleState(&newDitPressed, &newDahPressed);

  // Feed paddle state to unified keyer
  if (newDitPressed != cwaSendDitPressed) {
    cwaSendKeyer->key(PADDLE_DIT, newDitPressed);
    cwaSendDitPressed = newDitPressed;
  }
  if (newDahPressed != cwaSendDahPressed) {
    cwaSendKeyer->key(PADDLE_DAH, newDahPressed);
    cwaSendDahPressed = newDahPressed;
  }

  // Tick the keyer state machine
  cwaSendKeyer->tick(millis());

  // Keep tone playing if keyer is active (for audio buffer continuity)
  if (cwaSendKeyer->isTxActive()) {
    continueTone(cwTone);
  }
}

// ============================================
// Input Handler
// ============================================

/*
 * Handle input for sending practice mode
 */
int handleCWASendingPracticeInput(char key, LGFX& tft) {
  if (key == 0x1B) {  // ESC
    stopTone();  // Ensure tone is stopped
    if (cwaSendKeyer) {
      cwaSendKeyer->reset();
    }
    return -1;  // Exit
  }

  // If showing feedback, any key continues
  if (cwaSendShowingFeedback) {
    if (cwaSendRound >= 10) {
      // Show final score
      tft.fillRect(0, 42, SCREEN_WIDTH, SCREEN_HEIGHT - 42, COLOR_BACKGROUND);
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_CYAN);
      tft.setCursor(40, 80);
      tft.print("Practice Complete!");

      tft.setTextSize(3);
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(60, 120);
      tft.print("Score: ");
      tft.print(cwaSendCorrect);
      tft.print("/");
      tft.print(cwaSendTotal);

      int percentage = (cwaSendTotal > 0) ? (cwaSendCorrect * 100 / cwaSendTotal) : 0;
      tft.setTextSize(2);
      tft.setTextColor(percentage >= 70 ? ST77XX_GREEN : ST77XX_YELLOW);
      tft.setCursor(90, 160);
      tft.print(percentage);
      tft.print("%");

      // Show elapsed time
      unsigned long elapsedSeconds = (millis() - cwaSendStartTime) / 1000;
      tft.setTextSize(1);
      tft.setTextColor(0x7BEF);
      tft.setCursor(70, 185);
      tft.print("Time: ");
      tft.print(elapsedSeconds / 60);
      tft.print(":");
      if (elapsedSeconds % 60 < 10) tft.print("0");
      tft.print(elapsedSeconds % 60);

      tft.setCursor(60, 205);
      tft.print("Press any key to exit...");

      delay(3000);
      return -1;
    } else {
      startCWASendRound();
      drawCWASendingPracticeUI(tft);
      return 0;
    }
  }

  // Waiting for student to send
  if (cwaSendWaitingForSend) {
    if (key == 'P' || key == 'p') {
      // Play target message (async)
      requestPlayMorseString(cwaSendTarget.c_str(), 15, cwTone);  // 15 WPM for sending practice
      return 0;

    } else if (key == 'R' || key == 'r') {
      // Toggle reference display
      cwaSendShowReference = !cwaSendShowReference;
      beep(TONE_MENU_NAV, BEEP_SHORT);
      return 2;  // Redraw

    } else if (key == KEY_ENTER || key == KEY_ENTER_ALT) {
      // Submit what they sent
      cwaSendDecoder.flush();  // Flush any remaining buffered data

      cwaSendTotal++;
      if (cwaSendDecoded.equalsIgnoreCase(cwaSendTarget)) {
        cwaSendCorrect++;
        beep(1000, 200);  // Success beep
      } else {
        beep(400, 300);  // Error beep
      }

      cwaSendShowingFeedback = true;
      cwaSendWaitingForSend = false;
      stopTone();  // Ensure tone is stopped
      return 2;  // Redraw
    }
  }

  return 0;
}

#endif // TRAINING_CWA_SEND_PRACTICE_H
