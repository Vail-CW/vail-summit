/*
 * Bluetooth Keyboard Settings Module
 * Handles BLE keyboard scanning, pairing, and connection management
 */

#ifndef SETTINGS_BT_KEYBOARD_H
#define SETTINGS_BT_KEYBOARD_H

#include "../core/config.h"
#include "../bluetooth/ble_keyboard_host.h"

// Bluetooth keyboard settings state machine
enum BTKeyboardSettingsState {
  BTKB_UI_STATUS,         // Show current status and paired device
  BTKB_UI_SCANNING,       // Actively scanning for keyboards
  BTKB_UI_DEVICE_LIST,    // Show list of found devices
  BTKB_UI_CONNECTING,     // Connecting to selected device
  BTKB_UI_FORGET_CONFIRM  // Confirm forget pairing
};

// UI state

// LVGL mode flag - when true, skip legacy draw functions (LVGL handles display)

// Forward declarations
void startBTKeyboardSettings();
// Start Bluetooth keyboard settings mode
void startBTKeyboardSettings() {
  Serial.println("Starting BT Keyboard Settings");


  // Initialize BLE keyboard host if not already
  if (bleKBHost.state == BLEKB_STATE_IDLE) {
    loadBLEKeyboardSettings();
    initBLEKeyboardHost();
  }

}

#endif // SETTINGS_BT_KEYBOARD_H
