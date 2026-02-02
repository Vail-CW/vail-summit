/*
 * Practice Oscillator Mode
 * Allows free-form morse code practice with paddle/key
 * Includes real-time morse decoding with adaptive speed tracking
 */

#ifndef TRAINING_PRACTICE_H
#define TRAINING_PRACTICE_H

#include "../core/config.h"
#include "../settings/settings_cw.h"
#include "../audio/morse_decoder_adaptive.h"
#include "../keyer/keyer.h"


// Practice mode state
bool practiceActive = false;
bool ditPressed = false;
bool dahPressed = false;
bool lastDitPressed = false;
bool lastDahPressed = false;
unsigned long practiceStartupTime = 0;  // Track startup time for input delay

// Deferred save state - debounces rapid setting changes when holding keys
unsigned long lastSettingSaveTime = 0;
bool settingSavePending = false;
#define SETTING_SAVE_DEBOUNCE_MS 500  // Save 500ms after last change

// Unified keyer instance
static StraightKeyer* practiceKeyer = nullptr;
int ditDuration = 0;

// Statistics
unsigned long practiceStartTime = 0;

// Decoder state
MorseDecoderAdaptive decoder(20, 20, 30);  // Initial 20 WPM, buffer size 30
String decodedText = "";
String decodedMorse = "";
bool showDecoding = true;
bool needsUIUpdate = false;

// Timing capture for decoder
unsigned long lastStateChangeTime = 0;
bool lastToneState = false;
unsigned long lastElementTime = 0;  // Track last element for timeout flush

// Decoder display line management
#define DECODER_CHARS_PER_LINE 27
#define DECODER_MAX_LINES 4

// Helper to manage line wrapping and clearing when full
void manageDecoderLines() {
    // Insert line break if current line is too long
    int lastNewline = decodedText.lastIndexOf('\n');
    int currentLineLen = (lastNewline < 0) ? decodedText.length() : decodedText.length() - lastNewline - 1;

    if (currentLineLen >= DECODER_CHARS_PER_LINE) {
        decodedText += '\n';
    }

    // Count lines and clear all if exceeding max (wipe and start fresh)
    int lineCount = 1;
    for (int i = 0; i < decodedText.length(); i++) {
        if (decodedText[i] == '\n') lineCount++;
    }

    if (lineCount > DECODER_MAX_LINES) {
        decodedText = "";  // Clear all and start at top
    }
}

// Forward declarations
void startPracticeMode(LGFX &display);
void updatePracticeOscillator();
void practiceKeyerCallback(bool txOn, int element);

// LVGL-callable action functions
void practiceHandleEsc();
void practiceHandleClear();
void practiceAdjustSpeed(int delta);
void practiceCycleKeyType(int direction);
void practiceToggleDecoding();
void practiceCheckDeferredSave();

// Start practice mode
void startPracticeMode(LGFX &display) {
  practiceActive = true;
  ditPressed = false;
  dahPressed = false;
  practiceStartupTime = millis();  // Record startup time for input delay

  // Clear any lingering touch sensor state
  touchRead(TOUCH_DIT_PIN);
  touchRead(TOUCH_DAH_PIN);
  delay(50);

  // Reinitialize I2S to ensure clean state
  Serial.println("Reinitializing I2S for practice mode...");
  i2s_zero_dma_buffer(I2S_NUM_0);
  delay(50);

  // Calculate dit duration from current speed setting
  ditDuration = DIT_DURATION(cwSpeed);

  // Initialize unified keyer
  practiceKeyer = getKeyer(cwKeyType);
  practiceKeyer->reset();
  practiceKeyer->setDitDuration(ditDuration);
  practiceKeyer->setTxCallback(practiceKeyerCallback);

  // Reset statistics
  practiceStartTime = millis();

  // Reset decoder and clear any buffered state
  decoder.reset();
  decoder.flush();  // Clear any pending timings
  decoder.setWPM(cwSpeed);
  decodedText = "";
  decodedMorse = "";
  lastStateChangeTime = 0;  // Don't initialize until first key press
  lastToneState = false;
  lastElementTime = 0;  // Reset element timeout tracker
  showDecoding = true;
  needsUIUpdate = false;

  // Setup decoder callbacks
  decoder.messageCallback = [](String morse, String text) {
    // Process each character in the decoded text individually
    for (int i = 0; i < text.length(); i++) {
      decodedText += text[i];
      manageDecoderLines();  // Handle line wrapping and scrolling
    }

    // Also track morse pattern
    if (decodedMorse.length() + morse.length() > 100) {
      decodedMorse = "";  // Clear morse if it gets too long
    }
    decodedMorse += morse;

    needsUIUpdate = true;

    Serial.print("Decoded: ");
    Serial.print(text);
    Serial.print(" (");
    Serial.print(morse);
    Serial.println(")");
  };

  decoder.speedCallback = [](float wpm, float fwpm) {
    Serial.print("Speed detected: ");
    Serial.print(wpm);
    Serial.println(" WPM");
  };

  Serial.println("Practice mode started with decoding enabled");
  Serial.print("Speed: ");
  Serial.print(cwSpeed);
  Serial.print(" WPM, Tone: ");
  Serial.print(cwTone);
  Serial.print(" Hz, Key type: ");
  if (cwKeyType == KEY_STRAIGHT) {
    Serial.println("Straight");
  } else if (cwKeyType == KEY_IAMBIC_A) {
    Serial.println("Iambic A");
  } else if (cwKeyType == KEY_IAMBIC_B) {
    Serial.println("Iambic B");
  } else {
    Serial.println("Ultimatic");
  }
}


// Keyer callback - called by unified keyer when tone state changes
void practiceKeyerCallback(bool txOn, int element) {
  unsigned long currentTime = millis();

  if (txOn) {
    // Tone starting
    if (showDecoding && lastToneState == false) {
      // Send silence duration to decoder (negative)
      if (lastStateChangeTime > 0) {
        float silenceDuration = currentTime - lastStateChangeTime;
        if (silenceDuration > 0) {
          decoder.addTiming(-silenceDuration);
        }
      }
      lastStateChangeTime = currentTime;
      lastToneState = true;
    }
    startTone(cwTone);
  } else {
    // Tone stopping
    if (showDecoding && lastToneState == true) {
      // Send tone duration to decoder (positive)
      float toneDuration = currentTime - lastStateChangeTime;
      if (toneDuration > 0) {
        decoder.addTiming(toneDuration);
        lastElementTime = currentTime;  // Update timeout tracker
      }
      lastStateChangeTime = currentTime;
      lastToneState = false;
    }
    stopTone();
  }
}

// Update practice oscillator (called in main loop)
void updatePracticeOscillator() {
  if (!practiceActive) return;
  if (!practiceKeyer) return;

  // Check for deferred settings save
  practiceCheckDeferredSave();

  // Ignore all input for first 1000ms to prevent startup glitches
  if (millis() - practiceStartupTime < 1000) {
    return;
  }

  // Check for decoder timeout (flush if no activity for word gap duration)
  if (showDecoding && lastElementTime > 0 && !ditPressed && !dahPressed) {
    unsigned long timeSinceLastElement = millis() - lastElementTime;
    float wordGapDuration = MorseWPM::wordGap(decoder.getWPM());

    // Flush buffered data after word gap silence (backup timeout)
    if (timeSinceLastElement > wordGapDuration) {
      decoder.flush();
      lastElementTime = 0;  // Reset timeout
    }
  }

  // Read paddle/key inputs (physical + capacitive touch)
  bool newDitPressed = (digitalRead(DIT_PIN) == PADDLE_ACTIVE) || (touchRead(TOUCH_DIT_PIN) > TOUCH_THRESHOLD);
  bool newDahPressed = (digitalRead(DAH_PIN) == PADDLE_ACTIVE) || (touchRead(TOUCH_DAH_PIN) > TOUCH_THRESHOLD);

  // Feed paddle state to unified keyer
  if (newDitPressed != ditPressed) {
    practiceKeyer->key(PADDLE_DIT, newDitPressed);
    ditPressed = newDitPressed;
  }
  if (newDahPressed != dahPressed) {
    practiceKeyer->key(PADDLE_DAH, newDahPressed);
    dahPressed = newDahPressed;
  }

  // Tick the keyer state machine
  practiceKeyer->tick(millis());

  // Keep tone playing if keyer is active (for audio buffer continuity)
  if (practiceKeyer->isTxActive()) {
    continueTone(cwTone);
  }

  // Update visual feedback if state changed
  if (ditPressed != lastDitPressed || dahPressed != lastDahPressed) {
    lastDitPressed = ditPressed;
    lastDahPressed = dahPressed;
  }
}

// ============================================
// LVGL-Callable Action Functions
// ============================================

// Handle ESC key - stop practice and prepare for exit
void practiceHandleEsc() {
  practiceActive = false;
  stopTone();
  if (practiceKeyer) {
    practiceKeyer->reset();
  }
  decoder.flush();  // Decode any remaining buffered timings

  // Save any pending settings before exit
  if (settingSavePending) {
    saveCWSettings();
    settingSavePending = false;
    Serial.println("[Practice] Saved pending settings on exit");
  }

  Serial.println("[Practice] ESC - exiting practice mode");
}

// Clear decoder text
void practiceHandleClear() {
  decodedText = "";
  decodedMorse = "";
  decoder.reset();
  decoder.flush();
  needsUIUpdate = true;  // Signal LVGL to update display
  beep(TONE_MENU_NAV, BEEP_SHORT);
  Serial.println("[Practice] Cleared decoder text");
}

// Adjust WPM speed (delta can be any value, e.g., 1, 2, 4 based on acceleration)
void practiceAdjustSpeed(int delta) {
  int newSpeed = cwSpeed + delta;
  if (newSpeed >= WPM_MIN && newSpeed <= WPM_MAX) {
    cwSpeed = newSpeed;
    ditDuration = DIT_DURATION(cwSpeed);
    decoder.setWPM(cwSpeed);
    if (practiceKeyer) {
      practiceKeyer->setDitDuration(ditDuration);
    }

    // Mark save as pending instead of immediate save (debounces rapid changes)
    settingSavePending = true;
    lastSettingSaveTime = millis();

    beep(TONE_MENU_NAV, BEEP_SHORT);
    Serial.printf("[Practice] Speed changed to %d WPM (save pending)\n", cwSpeed);
  }
}

// Check and perform deferred save of CW settings
// Call this from updatePracticeOscillator() to save after debounce period
void practiceCheckDeferredSave() {
  if (settingSavePending && (millis() - lastSettingSaveTime > SETTING_SAVE_DEBOUNCE_MS)) {
    saveCWSettings();
    settingSavePending = false;
    Serial.println("[Practice] Deferred CW settings save completed");
  }
}

// Cycle key type (+1 forward, -1 backward)
void practiceCycleKeyType(int direction) {
  if (direction > 0) {
    // Cycle forward: Straight -> Iambic A -> Iambic B -> Ultimatic -> Straight
    if (cwKeyType == KEY_STRAIGHT) {
      cwKeyType = KEY_IAMBIC_A;
    } else if (cwKeyType == KEY_IAMBIC_A) {
      cwKeyType = KEY_IAMBIC_B;
    } else if (cwKeyType == KEY_IAMBIC_B) {
      cwKeyType = KEY_ULTIMATIC;
    } else {
      cwKeyType = KEY_STRAIGHT;
    }
  } else {
    // Cycle backward: Ultimatic -> Iambic B -> Iambic A -> Straight -> Ultimatic
    if (cwKeyType == KEY_ULTIMATIC) {
      cwKeyType = KEY_IAMBIC_B;
    } else if (cwKeyType == KEY_IAMBIC_B) {
      cwKeyType = KEY_IAMBIC_A;
    } else if (cwKeyType == KEY_IAMBIC_A) {
      cwKeyType = KEY_STRAIGHT;
    } else {
      cwKeyType = KEY_ULTIMATIC;
    }
  }

  // Reinitialize keyer with new type
  practiceKeyer = getKeyer(cwKeyType);
  practiceKeyer->reset();
  practiceKeyer->setDitDuration(ditDuration);
  practiceKeyer->setTxCallback(practiceKeyerCallback);

  // Mark save as pending (use same debounce as speed changes)
  settingSavePending = true;
  lastSettingSaveTime = millis();

  beep(TONE_MENU_NAV, BEEP_SHORT);

  const char* keyTypeStr = (cwKeyType == KEY_STRAIGHT) ? "Straight" :
                           (cwKeyType == KEY_IAMBIC_A) ? "Iambic A" :
                           (cwKeyType == KEY_IAMBIC_B) ? "Iambic B" : "Ultimatic";
  Serial.printf("[Practice] Key type changed to %s (save pending)\n", keyTypeStr);
}

// Toggle decoding display
void practiceToggleDecoding() {
  showDecoding = !showDecoding;
  beep(TONE_MENU_NAV, BEEP_SHORT);
  Serial.printf("[Practice] Decoding %s\n", showDecoding ? "enabled" : "disabled");
}

#endif // TRAINING_PRACTICE_H
