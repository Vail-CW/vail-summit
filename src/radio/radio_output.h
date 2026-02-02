/*
 * Radio Output Mode
 * Allows keying external radios via 3.5mm jack outputs
 * Supports Summit Keyer mode (internal keying) and Radio Keyer mode (passthrough)
 */

#ifndef RADIO_OUTPUT_H
#define RADIO_OUTPUT_H

#include "../core/config.h"
#include "../settings/settings_cw.h"
#include "../keyer/keyer.h"
#include <Preferences.h>

// Radio keyer modes
enum RadioMode {
  RADIO_MODE_SUMMIT_KEYER,   // Summit does the keying logic, outputs straight key format
  RADIO_MODE_RADIO_KEYER     // Passthrough dit/dah contacts to radio's internal keyer
};

// Radio output state
bool radioOutputActive = false;
RadioMode radioMode = RADIO_MODE_SUMMIT_KEYER;
int radioSettingSelection = 0;  // 0=Speed, 1=Key Type, 2=Radio Mode
#define RADIO_SETTINGS_COUNT 3

// Memory selector state
bool memorySelectorActive = false;
int memorySelectorSelection = 0;  // Selected memory slot (0-9)

// Message queue for web-based transmission
#define RADIO_MESSAGE_QUEUE_SIZE 5
#define RADIO_MESSAGE_MAX_LENGTH 200
struct RadioMessageQueue {
  char messages[RADIO_MESSAGE_QUEUE_SIZE][RADIO_MESSAGE_MAX_LENGTH];
  int readIndex;
  int writeIndex;
  int count;
} radioMessageQueue = {
  .readIndex = 0,
  .writeIndex = 0,
  .count = 0
};

// Current message transmission state
bool isTransmittingMessage = false;
int messageCharIndex = 0;
unsigned long messageTransmissionTimer = 0;
char currentTransmittingMessage[RADIO_MESSAGE_MAX_LENGTH] = "";

// Keyer state for Summit Keyer mode - using unified keyer module
static StraightKeyer* radioKeyer = nullptr;
static bool radioDitPressed = false;
static bool radioDahPressed = false;
static int radioDitDuration = 0;

// Preferences for radio mode
Preferences radioPrefs;

// LVGL mode flag - when true, skip legacy draw functions (LVGL handles display)
bool radioOutputUseLVGL = true;  // Default to LVGL mode

// Keying callback for POTA Recorder (and other features that need timing capture)
// Parameters: keyDown (true=key pressed, false=key released), timestamp (millis())
void (*radioKeyingCallback)(bool keyDown, unsigned long timestamp) = nullptr;

// Forward declarations
void startRadioOutput(LGFX &display);
void drawRadioOutputUI(LGFX &display);
int handleRadioOutputInput(char key, LGFX &display);
void updateRadioOutput();
void saveRadioSettings();
void loadRadioSettings();
void radioKeyerCallback(bool txOn, int element);
bool queueRadioMessage(const char* message);
void processRadioMessageQueue();
void playMorseCharViaRadio(char c);

// Load radio settings from flash
void loadRadioSettings() {
  radioPrefs.begin("radio", true);
  radioMode = (RadioMode)radioPrefs.getInt("mode", RADIO_MODE_SUMMIT_KEYER);
  radioPrefs.end();

  Serial.print("Radio settings loaded: Mode = ");
  Serial.println(radioMode == RADIO_MODE_SUMMIT_KEYER ? "Summit Keyer" : "Radio Keyer");
}

// Save radio settings to flash
void saveRadioSettings() {
  radioPrefs.begin("radio", false);
  radioPrefs.putInt("mode", (int)radioMode);
  radioPrefs.end();

  Serial.println("Radio settings saved");
}

// Start radio output mode
void startRadioOutput(LGFX &display) {
  radioOutputActive = true;
  radioSettingSelection = 0;

  // Load radio settings
  loadRadioSettings();

  // Calculate dit duration from current CW speed setting
  radioDitDuration = DIT_DURATION(cwSpeed);

  // Initialize unified keyer
  radioKeyer = getKeyer(cwKeyType);
  radioKeyer->reset();
  radioKeyer->setDitDuration(radioDitDuration);
  radioKeyer->setTxCallback(radioKeyerCallback);
  radioDitPressed = false;
  radioDahPressed = false;

  // Set radio output pins as outputs
  pinMode(RADIO_KEY_DIT_PIN, OUTPUT);
  pinMode(RADIO_KEY_DAH_PIN, OUTPUT);
  digitalWrite(RADIO_KEY_DIT_PIN, LOW);  // Not keyed
  digitalWrite(RADIO_KEY_DAH_PIN, LOW);  // Not keyed

  // UI is now handled by LVGL - see lv_mode_screens.h
}

// Draw radio output UI
void drawRadioOutputUI(LGFX &display) {
  if (radioOutputUseLVGL) return;  // LVGL handles display
  // Clear screen (preserve header)
  display.fillRect(0, 42, SCREEN_WIDTH, SCREEN_HEIGHT - 42, COLOR_BACKGROUND);

  // Modern card container
  int cardX = 20;
  int cardY = 55;
  int cardW = SCREEN_WIDTH - 40;
  int cardH = 150;

  display.fillRoundRect(cardX, cardY, cardW, cardH, 12, 0x1082); // Dark blue fill
  display.drawRoundRect(cardX, cardY, cardW, cardH, 12, 0x34BF); // Light blue outline

  // Setting 0: Speed (WPM)
  int yPos = cardY + 15;
  bool isSelected = (radioSettingSelection == 0);

  if (isSelected) {
    display.fillRoundRect(cardX + 8, yPos, cardW - 16, 38, 8, 0x249F); // Blue highlight
  }

  display.setTextSize(1);
  display.setTextColor(isSelected ? ST77XX_WHITE : 0x7BEF); // Light gray
  display.setCursor(cardX + 15, yPos + 8);
  display.print("Speed");

  display.setTextSize(2);
  display.setTextColor(isSelected ? ST77XX_WHITE : ST77XX_CYAN);
  display.setCursor(cardX + 15, yPos + 20);
  display.print(cwSpeed);
  display.print(" WPM");

  // Setting 1: Key Type
  yPos += 45;
  isSelected = (radioSettingSelection == 1);

  if (isSelected) {
    display.fillRoundRect(cardX + 8, yPos, cardW - 16, 38, 8, 0x249F); // Blue highlight
  }

  display.setTextSize(1);
  display.setTextColor(isSelected ? ST77XX_WHITE : 0x7BEF); // Light gray
  display.setCursor(cardX + 15, yPos + 8);
  display.print("Key Type");

  display.setTextSize(2);
  display.setTextColor(isSelected ? ST77XX_WHITE : ST77XX_CYAN);
  display.setCursor(cardX + 15, yPos + 20);

  if (cwKeyType == KEY_STRAIGHT) {
    display.print("Straight");
  } else if (cwKeyType == KEY_IAMBIC_A) {
    display.print("Iambic A");
  } else {
    display.print("Iambic B");
  }

  // Setting 2: Radio Mode
  yPos += 45;
  isSelected = (radioSettingSelection == 2);

  if (isSelected) {
    display.fillRoundRect(cardX + 8, yPos, cardW - 16, 38, 8, 0x249F); // Blue highlight
  }

  display.setTextSize(1);
  display.setTextColor(isSelected ? ST77XX_WHITE : 0x7BEF); // Light gray
  display.setCursor(cardX + 15, yPos + 8);
  display.print("Radio Mode");

  display.setTextSize(2);
  display.setTextColor(isSelected ? ST77XX_WHITE : ST77XX_CYAN);
  display.setCursor(cardX + 15, yPos + 20);

  if (radioMode == RADIO_MODE_SUMMIT_KEYER) {
    display.print("Summit Keyer");
  } else {
    display.print("Radio Keyer");
  }

  // Footer with instructions
  display.setTextSize(1);
  display.setTextColor(COLOR_WARNING);
  String helpText = "\x18\x19 Select  \x1B\x1A Adjust  M Memories  ESC Back";
  int16_t x1, y1;
  uint16_t w, h;
  getTextBounds_compat(display, helpText.c_str(), 0, 0, &x1, &y1, &w, &h);
  int centerX = (SCREEN_WIDTH - w) / 2;
  display.setCursor(centerX, SCREEN_HEIGHT - 12);
  display.print(helpText);
}

// Draw memory selector overlay
void drawMemorySelector(LGFX &display) {
  if (radioOutputUseLVGL) return;  // LVGL handles display
  // Semi-transparent overlay effect (just draw over existing content)
  display.fillRoundRect(30, 50, SCREEN_WIDTH - 60, 160, 12, 0x0841);  // Very dark blue
  display.drawRoundRect(30, 50, SCREEN_WIDTH - 60, 160, 12, 0x34BF);  // Light blue outline

  // Title
  display.setTextSize(1);
  display.setTextColor(ST77XX_CYAN);
  int16_t x1, y1;
  uint16_t w, h;
  String title = "CW MEMORIES";
  getTextBounds_compat(display, title.c_str(), 0, 0, &x1, &y1, &w, &h);
  int centerX = (SCREEN_WIDTH - w) / 2;
  display.setCursor(centerX, 65);
  display.print(title);

  // Draw memory list (show 5 at a time, scrollable)
  int startY = 85;
  int itemHeight = 22;
  int visibleItems = 5;
  int scrollOffset = (memorySelectorSelection > 4) ? (memorySelectorSelection - 4) : 0;

  for (int i = 0; i < visibleItems && (scrollOffset + i) < CW_MEMORY_MAX_SLOTS; i++) {
    int slot = scrollOffset + i;
    int yPos = startY + (i * itemHeight);

    bool isSelected = (slot == memorySelectorSelection);

    // Highlight selected item
    if (isSelected) {
      display.fillRoundRect(40, yPos - 2, SCREEN_WIDTH - 80, itemHeight - 3, 6, 0x249F);
    }

    // Draw slot number and label
    display.setTextSize(1);
    display.setTextColor(isSelected ? ST77XX_WHITE : ST77XX_CYAN);
    display.setCursor(50, yPos + 6);
    display.print(slot + 1);
    display.print(". ");

    if (cwMemories[slot].isEmpty) {
      display.setTextColor(isSelected ? 0xC618 : 0x7BEF);  // Gray
      display.print("(empty)");
    } else {
      display.setTextColor(isSelected ? ST77XX_WHITE : ST77XX_CYAN);
      // Truncate label if too long
      if (strlen(cwMemories[slot].label) > 20) {
        char truncated[21];
        strlcpy(truncated, cwMemories[slot].label, 18);
        strcat(truncated, "...");
        display.print(truncated);
      } else {
        display.print(cwMemories[slot].label);
      }
    }
  }

  // Footer instructions
  display.setTextSize(1);
  display.setTextColor(COLOR_WARNING);
  String helpText = "\x18\x19 Select  ENTER Send  ESC Cancel";
  getTextBounds_compat(display, helpText.c_str(), 0, 0, &x1, &y1, &w, &h);
  centerX = (SCREEN_WIDTH - w) / 2;
  display.setCursor(centerX, 195);
  display.print(helpText);
}

// Handle radio output input
int handleRadioOutputInput(char key, LGFX &display) {
  // Handle memory selector if active
  if (memorySelectorActive) {
    if (key == KEY_UP) {
      if (memorySelectorSelection > 0) {
        memorySelectorSelection--;
        drawMemorySelector(display);
        beep(TONE_MENU_NAV, BEEP_SHORT);
        return 0;  // Don't request full redraw - just redrew overlay
      }
    }
    else if (key == KEY_DOWN) {
      if (memorySelectorSelection < CW_MEMORY_MAX_SLOTS - 1) {
        memorySelectorSelection++;
        drawMemorySelector(display);
        beep(TONE_MENU_NAV, BEEP_SHORT);
        return 0;  // Don't request full redraw - just redrew overlay
      }
    }
    else if (key == KEY_ENTER || key == KEY_ENTER_ALT) {
      // Send selected memory
      if (!cwMemories[memorySelectorSelection].isEmpty) {
        bool success = queueRadioMessage(cwMemories[memorySelectorSelection].message);
        memorySelectorActive = false;
        drawRadioOutputUI(display);

        if (success) {
          beep(TONE_SUCCESS, BEEP_MEDIUM);
        } else {
          beep(TONE_ERROR, BEEP_LONG);
        }
        return 2;
      } else {
        // Empty slot - show error
        beep(TONE_ERROR, BEEP_SHORT);
        return 0;
      }
    }
    else if (key == KEY_ESC) {
      // Cancel memory selector
      memorySelectorActive = false;
      drawRadioOutputUI(display);
      beep(TONE_MENU_NAV, BEEP_SHORT);
      return 2;
    }
    return 0;
  }

  // Check for 'M' key to open memory selector
  if (key == 'm' || key == 'M') {
    memorySelectorActive = true;
    memorySelectorSelection = 0;
    drawMemorySelector(display);
    beep(TONE_SELECT, BEEP_SHORT);
    return 0;  // Don't request redraw - overlay is already drawn
  }

  // Navigation
  if (key == KEY_UP) {
    if (radioSettingSelection > 0) {
      radioSettingSelection--;
      beep(TONE_MENU_NAV, BEEP_SHORT);
      return 2; // Redraw requested
    }
  }
  else if (key == KEY_DOWN) {
    if (radioSettingSelection < RADIO_SETTINGS_COUNT - 1) {
      radioSettingSelection++;
      beep(TONE_MENU_NAV, BEEP_SHORT);
      return 2; // Redraw requested
    }
  }
  // Adjust values
  else if (key == KEY_LEFT || key == KEY_RIGHT) {
    bool increase = (key == KEY_RIGHT);
    bool changed = false;

    if (radioSettingSelection == 0) {
      // Adjust speed
      if (increase && cwSpeed < WPM_MAX) {
        cwSpeed++;
        changed = true;
      } else if (!increase && cwSpeed > WPM_MIN) {
        cwSpeed--;
        changed = true;
      }
      if (changed) {
        radioDitDuration = DIT_DURATION(cwSpeed);
        saveCWSettings();
      }
    }
    else if (radioSettingSelection == 1) {
      // Cycle key type
      if (increase) {
        if (cwKeyType == KEY_STRAIGHT) cwKeyType = KEY_IAMBIC_A;
        else if (cwKeyType == KEY_IAMBIC_A) cwKeyType = KEY_IAMBIC_B;
        else cwKeyType = KEY_STRAIGHT;
      } else {
        if (cwKeyType == KEY_STRAIGHT) cwKeyType = KEY_IAMBIC_B;
        else if (cwKeyType == KEY_IAMBIC_A) cwKeyType = KEY_STRAIGHT;
        else cwKeyType = KEY_IAMBIC_A;
      }
      saveCWSettings();
      changed = true;
    }
    else if (radioSettingSelection == 2) {
      // Toggle radio mode
      if (radioMode == RADIO_MODE_SUMMIT_KEYER) {
        radioMode = RADIO_MODE_RADIO_KEYER;
      } else {
        radioMode = RADIO_MODE_SUMMIT_KEYER;
      }
      saveRadioSettings();
      changed = true;
    }

    if (changed) {
      beep(TONE_MENU_NAV, BEEP_SHORT);
      return 2; // Redraw requested
    }
  }
  else if (key == KEY_ESC) {
    // Exit radio output mode
    radioOutputActive = false;

    // Release radio keying outputs
    digitalWrite(RADIO_KEY_DIT_PIN, LOW);
    digitalWrite(RADIO_KEY_DAH_PIN, LOW);

    return -1; // Exit to radio menu
  }

  return 0; // Normal input processed
}

// Keyer callback - called by unified keyer when tx state changes
void radioKeyerCallback(bool txOn, int element) {
  unsigned long currentTime = millis();

  // Output straight key format on DIT pin (Summit Keyer mode)
  digitalWrite(RADIO_KEY_DIT_PIN, txOn ? HIGH : LOW);
  digitalWrite(RADIO_KEY_DAH_PIN, LOW);

  // Call keying callback for POTA Recorder timing capture
  if (radioKeyingCallback) {
    radioKeyingCallback(txOn, currentTime);
  }
}

// Update radio output (called from main loop)
void updateRadioOutput() {
  if (!radioOutputActive) return;

  // Process message queue first (for web-based transmission)
  processRadioMessageQueue();

  // Get paddle state from centralized handler (includes debounce)
  bool newDitPressed, newDahPressed;
  getPaddleState(&newDitPressed, &newDahPressed);

  if (radioMode == RADIO_MODE_SUMMIT_KEYER) {
    // Summit Keyer mode: Do keying logic on Summit, output straight key format
    if (radioKeyer) {
      // Feed paddle state to unified keyer
      if (newDitPressed != radioDitPressed) {
        radioKeyer->key(PADDLE_DIT, newDitPressed);
        radioDitPressed = newDitPressed;
      }
      if (newDahPressed != radioDahPressed) {
        radioKeyer->key(PADDLE_DAH, newDahPressed);
        radioDahPressed = newDahPressed;
      }

      // Tick the keyer state machine
      radioKeyer->tick(millis());
    }

  } else {
    // Radio Keyer mode: Passthrough contacts to radio
    // Update state for passthrough mode too
    radioDitPressed = newDitPressed;
    radioDahPressed = newDahPressed;

    if (cwKeyType == KEY_STRAIGHT) {
      // Straight key: output on DIT pin
      digitalWrite(RADIO_KEY_DIT_PIN, newDitPressed ? HIGH : LOW);
      digitalWrite(RADIO_KEY_DAH_PIN, LOW);
    } else {
      // Iambic: pass both dit and dah to radio
      digitalWrite(RADIO_KEY_DIT_PIN, newDitPressed ? HIGH : LOW);
      digitalWrite(RADIO_KEY_DAH_PIN, newDahPressed ? HIGH : LOW);
    }
  }
}

// Queue a message for radio transmission
bool queueRadioMessage(const char* message) {
  if (radioMessageQueue.count >= RADIO_MESSAGE_QUEUE_SIZE) {
    return false; // Queue is full
  }

  // Copy message to queue
  strlcpy(radioMessageQueue.messages[radioMessageQueue.writeIndex], message, RADIO_MESSAGE_MAX_LENGTH);

  // Advance write index (circular buffer)
  radioMessageQueue.writeIndex = (radioMessageQueue.writeIndex + 1) % RADIO_MESSAGE_QUEUE_SIZE;
  radioMessageQueue.count++;

  Serial.print("Message queued (");
  Serial.print(radioMessageQueue.count);
  Serial.print(" in queue): '");
  Serial.print(message);
  Serial.print("' Length=");
  Serial.println(strlen(message));

  return true;
}

// Play a single morse character via radio output
void playMorseCharViaRadio(char c) {
  const char* pattern = getMorseCode(c);
  if (pattern == nullptr) {
    return; // Skip unknown characters
  }

  MorseTiming timing(cwSpeed);

  // Play each element in the pattern
  for (int i = 0; pattern[i] != '\0'; i++) {
    int duration;
    if (pattern[i] == '.') {
      duration = timing.ditDuration;
    } else if (pattern[i] == '-') {
      duration = timing.dahDuration;
    } else {
      continue;
    }

    // Key the radio output (DIT pin for straight key format)
    digitalWrite(RADIO_KEY_DIT_PIN, HIGH);
    if (radioKeyingCallback) radioKeyingCallback(true, millis());
    delay(duration);
    digitalWrite(RADIO_KEY_DIT_PIN, LOW);
    if (radioKeyingCallback) radioKeyingCallback(false, millis());

    // Gap between elements (unless last element)
    if (pattern[i + 1] != '\0') {
      delay(timing.elementGap);
    }
  }
}

// Process radio message queue (called from updateRadioOutput)
void processRadioMessageQueue() {
  // Only process queue if we're in radio output mode
  if (!radioOutputActive) {
    return;
  }

  // Don't start new message if user is keying OR if keyer state machine is active
  bool ditPressed, dahPressed;
  getPaddleState(&ditPressed, &dahPressed);

  // In Summit Keyer mode, also check if keyer state machine is busy
  if (radioMode == RADIO_MODE_SUMMIT_KEYER) {
    if (ditPressed || dahPressed || (radioKeyer && radioKeyer->isTxActive())) {
      return; // Wait for keyer to be completely idle
    }
  } else {
    // In Radio Keyer mode, just check paddle states
    if (ditPressed || dahPressed) {
      return; // Wait for user to finish keying
    }
  }

  // If not currently transmitting, check if there's a message in the queue
  if (!isTransmittingMessage) {
    if (radioMessageQueue.count > 0) {
      // Dequeue message
      strlcpy(currentTransmittingMessage, radioMessageQueue.messages[radioMessageQueue.readIndex], RADIO_MESSAGE_MAX_LENGTH);
      radioMessageQueue.readIndex = (radioMessageQueue.readIndex + 1) % RADIO_MESSAGE_QUEUE_SIZE;
      radioMessageQueue.count--;

      // Start transmission
      isTransmittingMessage = true;
      messageCharIndex = 0;
      messageTransmissionTimer = millis(); // Start immediately

      Serial.print("Starting transmission: '");
      Serial.print(currentTransmittingMessage);
      Serial.print("' Length=");
      Serial.println(strlen(currentTransmittingMessage));
    }
  } else {
    // Continue transmitting current message
    unsigned long currentTime = millis();

    // Check if it's time to send the next character
    if (currentTime >= messageTransmissionTimer) {
      if (messageCharIndex < strlen(currentTransmittingMessage)) {
        char c = currentTransmittingMessage[messageCharIndex];

        if (c == ' ') {
          // Word gap (7 dits total, but we already have letter gap from previous character, so add 4 more dits)
          MorseTiming timing(cwSpeed);
          messageTransmissionTimer = currentTime + (timing.ditDuration * 4);
        } else {
          // Play morse character (this blocks during transmission)
          unsigned long charStartTime = millis();
          playMorseCharViaRadio(c);
          unsigned long charEndTime = millis();

          // Calculate how long the character took to send
          unsigned long charDuration = charEndTime - charStartTime;

          // Add letter gap after the character
          MorseTiming timing(cwSpeed);
          messageTransmissionTimer = charEndTime + timing.letterGap;

          Serial.print("Sent: ");
          Serial.print(c);
          Serial.print(" (took ");
          Serial.print(charDuration);
          Serial.print("ms, next at ");
          Serial.print(messageTransmissionTimer);
          Serial.println("ms)");
        }

        messageCharIndex++;
      } else {
        // Message complete
        isTransmittingMessage = false;
        Serial.println("Transmission complete");

        // Add extra delay before starting next message
        messageTransmissionTimer = currentTime + (DIT_DURATION(cwSpeed) * 7);
      }
    }
  }
}

#endif // RADIO_OUTPUT_H
