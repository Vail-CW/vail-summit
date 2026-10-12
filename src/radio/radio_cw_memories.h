/*
 * CW Memories Module
 * Store and manage CW message presets (up to 10 slots)
 * Presets can be previewed on device speaker or transmitted via Radio Output mode
 */

#ifndef RADIO_CW_MEMORIES_H
#define RADIO_CW_MEMORIES_H

#include "../core/config.h"
#include "../core/morse_code.h"
#include <Preferences.h>
#include "../core/lazy_alloc.h"

// External declarations
extern int cwSpeed;
extern int cwTone;

// ============================================
// Data Structures
// ============================================

#define CW_MEMORY_MAX_SLOTS 10
#define CW_MEMORY_LABEL_MAX_LENGTH 15
#define CW_MEMORY_MESSAGE_MAX_LENGTH 100

struct CWMemoryPreset {
  char label[CW_MEMORY_LABEL_MAX_LENGTH + 1];     // Label (15 chars + null)
  char message[CW_MEMORY_MESSAGE_MAX_LENGTH + 1]; // Message (100 chars + null)
  bool isEmpty;
};

// ============================================
// Global State
// ============================================

// Allocated (PSRAM first) and loaded from Preferences on first use by the
// CW Memories / Radio Output screens or the web API; freed when those
// screens are left and the web server is not running. See ensureCWMemories().
CWMemoryPreset* cwMemories = nullptr;
Preferences cwMemoryPrefs;

// UI State

// Context menu state
enum CWMemoryContextMenu {
  CONTEXT_NONE,
  CONTEXT_EMPTY_SLOT,     // Create or Cancel
  CONTEXT_OCCUPIED_SLOT   // Preview, Edit, Delete, Cancel
};


// Edit mode state
enum CWMemoryEditMode {
  EDIT_NONE,
  EDIT_CREATE_LABEL,
  EDIT_CREATE_MESSAGE,
  EDIT_EDIT_LABEL,
  EDIT_EDIT_MESSAGE
};


// Preview state

// LVGL mode flag - when true, skip legacy draw functions (LVGL handles display)

// ============================================
// Storage Functions
// ============================================

// Load all CW memories from Preferences
void loadCWMemoriesInto(CWMemoryPreset* tbl) {
  cwMemoryPrefs.begin("cw_memories", true);  // Read-only

  for (int i = 0; i < CW_MEMORY_MAX_SLOTS; i++) {
    String labelKey = "label" + String(i + 1);
    String messageKey = "message" + String(i + 1);

    String label = cwMemoryPrefs.getString(labelKey.c_str(), "");
    String message = cwMemoryPrefs.getString(messageKey.c_str(), "");

    if (label.length() == 0 && message.length() == 0) {
      tbl[i].isEmpty = true;
      tbl[i].label[0] = '\0';
      tbl[i].message[0] = '\0';
    } else {
      tbl[i].isEmpty = false;
      strlcpy(tbl[i].label, label.c_str(), CW_MEMORY_LABEL_MAX_LENGTH + 1);
      strlcpy(tbl[i].message, message.c_str(), CW_MEMORY_MESSAGE_MAX_LENGTH + 1);

      // Debug logging
      Serial.print("Loaded slot ");
      Serial.print(i + 1);
      Serial.print(": Label='");
      Serial.print(tbl[i].label);
      Serial.print("' Message='");
      Serial.print(tbl[i].message);
      Serial.println("'");
    }
  }

  cwMemoryPrefs.end();

  Serial.println("CW Memories loaded from Preferences");
}

// Make sure the table exists and holds the saved presets.
static portMUX_TYPE cwMemAllocMux = portMUX_INITIALIZER_UNLOCKED;
bool ensureCWMemories() {
  if (cwMemories) return true;
  // Main loop and the async web task can both get here: allocate and load
  // outside the lock, publish under it, drop the loser's copy.
  CWMemoryPreset* t = (CWMemoryPreset*)lazyAlloc(sizeof(CWMemoryPreset) * CW_MEMORY_MAX_SLOTS);
  if (!t) {
    Serial.println("[CWMem] alloc failed");
    return false;
  }
  loadCWMemoriesInto(t);
  bool used = false;
  portENTER_CRITICAL(&cwMemAllocMux);
  if (cwMemories == nullptr) { cwMemories = t; used = true; }
  portEXIT_CRITICAL(&cwMemAllocMux);
  if (!used) free(t);
  return true;
}

// Release the table. Skipped while the web server runs because its API
// handlers (async task) read the table; it is freed on the next screen exit.
void freeCWMemories() {
  extern bool webServerRunning;
  if (webServerRunning) return;
  lazyFree(cwMemories);
}

// Save a single CW memory to Preferences
void saveCWMemory(int slot) {
  if (!ensureCWMemories()) return;
  if (slot < 0 || slot >= CW_MEMORY_MAX_SLOTS) {
    Serial.println("Error: Invalid slot number");
    return;
  }

  cwMemoryPrefs.begin("cw_memories", false);  // Read-write

  String labelKey = "label" + String(slot + 1);
  String messageKey = "message" + String(slot + 1);

  if (cwMemories[slot].isEmpty) {
    cwMemoryPrefs.putString(labelKey.c_str(), "");
    cwMemoryPrefs.putString(messageKey.c_str(), "");
    Serial.print("Cleared slot ");
    Serial.println(slot + 1);
  } else {
    cwMemoryPrefs.putString(labelKey.c_str(), cwMemories[slot].label);
    cwMemoryPrefs.putString(messageKey.c_str(), cwMemories[slot].message);

    // Debug logging
    Serial.print("Saved slot ");
    Serial.print(slot + 1);
    Serial.print(": Label='");
    Serial.print(cwMemories[slot].label);
    Serial.print("' Message='");
    Serial.print(cwMemories[slot].message);
    Serial.println("'");
  }

  cwMemoryPrefs.end();
}

// Delete a CW memory (clear the slot)
void deleteCWMemory(int slot) {
  if (!ensureCWMemories()) return;
  if (slot < 0 || slot >= CW_MEMORY_MAX_SLOTS) {
    Serial.println("Error: Invalid slot number");
    return;
  }

  cwMemories[slot].isEmpty = true;
  cwMemories[slot].label[0] = '\0';
  cwMemories[slot].message[0] = '\0';

  saveCWMemory(slot);

  Serial.print("CW Memory deleted: Slot ");
  Serial.println(slot + 1);
}

// ============================================
// Preview Function
// ============================================

// Preview a memory on device speaker (non-blocking setup)
void previewCWMemory(int slot) {
  if (slot < 0 || slot >= CW_MEMORY_MAX_SLOTS) {
    Serial.println("Error: Invalid slot for preview");
    return;
  }

  if (cwMemories[slot].isEmpty || strlen(cwMemories[slot].message) == 0) {
    Serial.println("Error: Cannot preview empty memory");
    beep(TONE_ERROR, BEEP_SHORT);
    return;
  }


  Serial.print("Previewing memory slot ");
  Serial.print(slot + 1);
  Serial.print(": Label='");
  Serial.print(cwMemories[slot].label);
  Serial.print("' Message='");
  Serial.print(cwMemories[slot].message);
  Serial.print("' Length=");
  Serial.println(strlen(cwMemories[slot].message));

  // Play the message using existing morse code functions
  playMorseString(cwMemories[slot].message, cwSpeed, cwTone);

  // Preview complete
}

// ============================================
// Validation Functions
// ============================================

// Check if a character is valid for morse code
bool isValidMorseChar(char c) {
  c = toupper(c);
  if (c >= 'A' && c <= 'Z') return true;
  if (c >= '0' && c <= '9') return true;
  if (c == ' ') return true;
  if (c == '.' || c == ',' || c == '?' || c == '/' || c == '-') return true;
  // Prosigns entered as <AR>, <SK>, etc. are handled as regular characters
  if (c == '<' || c == '>') return true;
  return false;
}

// Validate message contains only valid morse characters
bool isValidMorseMessage(const char* message) {
  for (int i = 0; message[i] != '\0'; i++) {
    if (!isValidMorseChar(message[i])) {
      return false;
    }
  }
  return true;
}

// ============================================
// UI Drawing Functions
// ============================================

// ============================================
// Helper Functions
// ============================================

// ============================================
// Mode Entry Function
// ============================================

// ============================================
// Input Handling Functions
// ============================================

#endif // RADIO_CW_MEMORIES_H
