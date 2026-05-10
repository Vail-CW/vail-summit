/*
 * MorseDecoderDirect
 *
 * Extends MorseDecoderAdaptive with timer-driven character flushing.
 *
 * The timer-driven approach — decoupling character boundary detection from key
 * events by using a periodic tick — is inspired by the CW decoder in Fldigi
 * (https://sourceforge.net/projects/fldigi/), an open source digital modes
 * program by Dave Freese W1HKJ et al. No Fldigi code was used; this is an
 * independent implementation of the concept adapted for ESP32/Arduino.
 *
 * The adaptive decoder only flushes when addTiming() is called with a silence
 * value large enough to cross the threshold — meaning it waits for the next
 * keypress to feed that silence. MorseDecoderDirect decouples flushing from
 * key events: it tracks silence internally and exposes tick(), which should be
 * called every ~5ms by a hardware timer task. When silence since the last tone
 * element exceeds 2.5x ditLen, tick() calls flush() directly so the character
 * is emitted immediately without waiting for the operator to start the next one.
 *
 * Word spacing is still handled by the base class: when the next tone starts,
 * addTiming(-longSilence) is called and the base class detects the word gap
 * and emits a space normally.
 */

#ifndef MORSE_DECODER_DIRECT_H
#define MORSE_DECODER_DIRECT_H

#include "morse_decoder_adaptive.h"

class MorseDecoderDirect : public MorseDecoderAdaptive {
private:
  unsigned long lastToneEndMs = 0;  // millis() when last tone element ended
  bool inSilence = false;           // true after a tone ends, until next tone starts
  bool charFlushed = false;         // prevent repeated flush for the same gap

public:
  MorseDecoderDirect(float wpm = 20.0f, float fwpm = 20.0f, int bufSize = 30)
    : MorseDecoderAdaptive(wpm, fwpm, bufSize) {}

  void addTiming(float duration) override {
    if (duration > 0) {
      // Tone element just ended — silence period begins now
      lastToneEndMs = millis();
      inSilence = true;
      charFlushed = false;
    } else {
      // Silence ended — next tone is starting
      inSilence = false;
    }
    MorseDecoderAdaptive::addTiming(duration);
  }

  // Call every ~5ms from a timer task.
  // Flushes the buffered character as soon as silence >= 2.0x ditLen,
  // without waiting for the next keypress.
  void tick() override {
    if (!inSilence || charFlushed || isEmpty()) return;

    unsigned long silenceMs = millis() - lastToneEndMs;
    if (silenceMs >= (unsigned long)(getDitLen() * 2.5f)) {
      flush();
      charFlushed = true;
    }
  }

  void reset() override {
    MorseDecoderAdaptive::reset();
    lastToneEndMs = 0;
    inSilence = false;
    charFlushed = false;
  }
};

#endif // MORSE_DECODER_DIRECT_H
