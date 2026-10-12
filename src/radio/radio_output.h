/*
 * Radio Output Mode
 * Allows keying external radios via 3.5mm jack outputs
 * Supports Summit Keyer mode (internal keying) and Radio Keyer mode (passthrough)
 */

#ifndef RADIO_OUTPUT_H
#define RADIO_OUTPUT_H

#include "../core/config.h"
#include "../core/lazy_alloc.h"
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
#define RADIO_SETTINGS_COUNT 3

// Memory selector state

// Message queue for web-based transmission
#define RADIO_MESSAGE_QUEUE_SIZE 5
#define RADIO_MESSAGE_MAX_LENGTH 200
struct RadioMessageQueue {
  char messages[RADIO_MESSAGE_QUEUE_SIZE][RADIO_MESSAGE_MAX_LENGTH];
  int readIndex;
  int writeIndex;
  int count;
};
// Allocated on first queueRadioMessage(); freed when the Radio Output
// screen is left with nothing pending (freeRadioMessageQueue)
RadioMessageQueue* radioMessageQueue = nullptr;

static portMUX_TYPE radioQueueAllocMux = portMUX_INITIALIZER_UNLOCKED;
static bool ensureRadioMessageQueue() {
  if (radioMessageQueue) return true;
  RadioMessageQueue* q = (RadioMessageQueue*)lazyAlloc(sizeof(RadioMessageQueue));
  if (!q) return false;
  bool used = false;
  portENTER_CRITICAL(&radioQueueAllocMux);
  if (!radioMessageQueue) { radioMessageQueue = q; used = true; }
  portEXIT_CRITICAL(&radioQueueAllocMux);
  if (!used) free(q);  // another task won the race
  return true;
}

void freeRadioMessageQueue() {
  extern bool webServerRunning;
  if (webServerRunning) return;  // web API may queue from the async task
  if (radioMessageQueue && radioMessageQueue->count == 0) {
    lazyFree(radioMessageQueue);
  }
}

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

// Keying callback for POTA Recorder (and other features that need timing capture)
// Parameters: keyDown (true=key pressed, false=key released), timestamp (millis())
void (*radioKeyingCallback)(bool keyDown, unsigned long timestamp) = nullptr;

// Morse Notes keying callback
// Parameters: keyDown (true=key pressed, false=key released), timestamp (millis())
void (*morseNotesKeyCallback)(bool keyDown, unsigned long timestamp) = nullptr;

// Forward declarations
void startRadioOutput();
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
void startRadioOutput() {
  radioOutputActive = true;

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

  // Call Morse Notes keying callback for recording
  if (morseNotesKeyCallback) {
    morseNotesKeyCallback(txOn, currentTime);
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
  if (!ensureRadioMessageQueue()) return false;
  if (radioMessageQueue->count >= RADIO_MESSAGE_QUEUE_SIZE) {
    return false; // Queue is full
  }

  // Copy message to queue
  strlcpy(radioMessageQueue->messages[radioMessageQueue->writeIndex], message, RADIO_MESSAGE_MAX_LENGTH);

  // Advance write index (circular buffer)
  radioMessageQueue->writeIndex = (radioMessageQueue->writeIndex + 1) % RADIO_MESSAGE_QUEUE_SIZE;
  radioMessageQueue->count++;

  Serial.print("Message queued (");
  Serial.print(radioMessageQueue->count);
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
    if (radioMessageQueue && radioMessageQueue->count > 0) {
      // Dequeue message
      strlcpy(currentTransmittingMessage, radioMessageQueue->messages[radioMessageQueue->readIndex], RADIO_MESSAGE_MAX_LENGTH);
      radioMessageQueue->readIndex = (radioMessageQueue->readIndex + 1) % RADIO_MESSAGE_QUEUE_SIZE;
      radioMessageQueue->count--;

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
