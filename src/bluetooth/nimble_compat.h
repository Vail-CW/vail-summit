/*
 * NimBLE-Arduino version shim
 *
 * The 2.0.14 build pins NimBLE-Arduino 1.4.2; the core 3.x build needs
 * NimBLE-Arduino 2.x (1.4.2 vendors a NimBLE host whose CONFIG_BT_* macros
 * disagree with the prebuilt IDF 5 controller and corrupts the heap during
 * esp_bt_controller_init). The two library majors differ in callback
 * signatures, HID helper method names, scan API and timeout units, so every
 * BLE file includes this header and switches on SUMMIT_NIMBLE_V2.
 *
 * Also hosts the pre-init heap check shared by the server (HID/MIDI) and
 * client (keyboard host) paths.
 */

#ifndef NIMBLE_COMPAT_H
#define NIMBLE_COMPAT_H

#include <NimBLEDevice.h>
#include "../core/config.h"

// NimBLE-Arduino 2.x defines NIMBLE_CPP_VERSION (NimBLECppVersion.h); 1.4.2
// has no version macro at all. Nested #if because a function-like macro that
// is undefined is a preprocessor syntax error even behind a false defined().
#if defined(NIMBLE_CPP_VERSION_MAJOR)
#if NIMBLE_CPP_VERSION_MAJOR >= 2
#define SUMMIT_NIMBLE_V2 1
#else
#define SUMMIT_NIMBLE_V2 0
#endif
#else
#define SUMMIT_NIMBLE_V2 0
#endif

// Last BLE bring-up failure, shown on the HID/MIDI/keyboard screens.
// Empty when the last init succeeded.
char bleLastErrorMsg[48] = "";

/*
 * Log heap state and decide whether the BLE controller + host can be brought
 * up. On core 3.x with WiFi connected the free heap has been measured at
 * ~90 KB with a 47 KB largest block; the controller allocates several
 * multi-KB blocks during esp_bt_controller_init and NimBLE adds its own
 * pools. Returns false (and fills bleLastErrorMsg) when the heap is below
 * BLE_MIN_FREE_HEAP / BLE_MIN_LARGEST_BLOCK (config.h), so the caller can
 * skip NimBLEDevice::init() instead of risking an allocation failure deep
 * inside the controller.
 */
static bool bleHeapOkForInit(const char* who) {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t maxBlock = ESP.getMaxAllocHeap();
  Serial.printf("[%s] Heap before NimBLE init: free=%lu largest=%lu (need %lu/%lu)\n",
                who, (unsigned long)freeHeap, (unsigned long)maxBlock,
                (unsigned long)BLE_MIN_FREE_HEAP, (unsigned long)BLE_MIN_LARGEST_BLOCK);
  if (freeHeap < BLE_MIN_FREE_HEAP || maxBlock < BLE_MIN_LARGEST_BLOCK) {
    snprintf(bleLastErrorMsg, sizeof(bleLastErrorMsg),
             "Low memory (%luK free) - turn WiFi off", (unsigned long)(freeHeap / 1024));
    Serial.printf("[%s] %s; skipping NimBLE init\n", who, bleLastErrorMsg);
    return false;
  }
  bleLastErrorMsg[0] = '\0';
  return true;
}

#endif // NIMBLE_COMPAT_H
