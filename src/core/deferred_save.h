/*
 * Deferred NVS Saves
 *
 * An NVS commit disables the flash cache on BOTH cores for its duration.
 * Saving on every slider tick / value change lands those commits in the middle
 * of UI beeps and keyed elements, which audibly crunches the audio (the I2S
 * DMA cushion is only ~23ms). This module coalesces hot-path saves: callers
 * mark a save function dirty, and the main loop commits it once the value has
 * been stable for DEFERRED_SAVE_MS *and* no audio is active.
 *
 * Settings still save automatically with no user action - just ~1s later and
 * never mid-tone. Discrete one-shot saves (e.g. "Save & Exit" buttons) can
 * keep calling their saveXxx() directly.
 */

#ifndef DEFERRED_SAVE_H
#define DEFERRED_SAVE_H

#include <Arduino.h>

#define DEFERRED_SAVE_MS    800
#define DEFERRED_SAVE_SLOTS 8

// Defined in vail-summit.ino: true when no tone is active AND the current
// mode is not audio-critical (a commit in an inter-element gap would stall
// the start of the next element).
bool deferredSavesAllowed();

typedef void (*DeferredSaveFn)();
static DeferredSaveFn deferredSaveFns[DEFERRED_SAVE_SLOTS] = {nullptr};
static unsigned long deferredSaveAt[DEFERRED_SAVE_SLOTS] = {0};

// Mark a save function dirty (re-stamps the stability timer if already dirty).
inline void markDeferredSave(DeferredSaveFn fn) {
  for (int i = 0; i < DEFERRED_SAVE_SLOTS; i++) {
    if (deferredSaveFns[i] == fn) {
      deferredSaveAt[i] = millis();
      return;
    }
  }
  for (int i = 0; i < DEFERRED_SAVE_SLOTS; i++) {
    if (deferredSaveFns[i] == nullptr) {
      deferredSaveFns[i] = fn;
      deferredSaveAt[i] = millis();
      return;
    }
  }
  fn();  // table full (shouldn't happen) - save immediately rather than drop
}

// Commit pending saves whose values have settled. Call from loop().
inline void updateDeferredSaves() {
  if (!deferredSavesAllowed()) return;
  unsigned long now = millis();
  for (int i = 0; i < DEFERRED_SAVE_SLOTS; i++) {
    if (deferredSaveFns[i] != nullptr && (now - deferredSaveAt[i]) >= DEFERRED_SAVE_MS) {
      DeferredSaveFn fn = deferredSaveFns[i];
      deferredSaveFns[i] = nullptr;
      fn();
    }
  }
}

// Commit everything now, regardless of audio state. Call before deliberate
// reboots so a just-changed setting can't be lost.
inline void flushDeferredSaves() {
  for (int i = 0; i < DEFERRED_SAVE_SLOTS; i++) {
    if (deferredSaveFns[i] != nullptr) {
      DeferredSaveFn fn = deferredSaveFns[i];
      deferredSaveFns[i] = nullptr;
      fn();
    }
  }
}

#endif // DEFERRED_SAVE_H
