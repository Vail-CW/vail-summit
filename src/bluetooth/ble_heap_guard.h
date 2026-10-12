/*
 * BLE pre-init heap guard
 *
 * The BT controller and NimBLE host allocate tens of KB in several blocks
 * during NimBLEDevice::init(). Starting them on a starved heap fails deep
 * inside the controller instead of cleanly, so every BLE entry point logs the
 * heap and refuses to start below the thresholds in config.h.
 */

#ifndef BLE_HEAP_GUARD_H
#define BLE_HEAP_GUARD_H

#include <NimBLEDevice.h>
#include "../core/config.h"

// Last BLE bring-up failure, shown on the HID/MIDI/keyboard screens.
// Empty when the last init succeeded.
char bleLastErrorMsg[48] = "";

/*
 * Log heap state and decide whether the BLE controller + host can be brought
 * up. With WiFi connected the free heap has been measured at ~90 KB with a
 * 47 KB largest block. Returns false (and fills bleLastErrorMsg) when the
 * heap is below BLE_MIN_FREE_HEAP / BLE_MIN_LARGEST_BLOCK so the caller can
 * skip NimBLEDevice::init().
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

#endif // BLE_HEAP_GUARD_H
