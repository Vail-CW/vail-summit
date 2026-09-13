/*
 * Effective ("actual") WPM tracking - PARIS convention (5 chars = 1 word).
 *
 * A keyer-speed or decoder dit-length WPM figure only measures element
 * speed - it ignores whatever inter-character/word spacing the operator
 * actually adds, so for a keyer user it just echoes the keyer setting back.
 * This tracker instead measures real throughput: wall-clock time from the
 * first tone of a sending burst to the most recent tone end, divided into
 * the number of characters decoded in that span. Spacing the operator adds
 * is therefore counted against them, not filtered out.
 *
 * Idle rule: if a tone starts more than EFFECTIVE_WPM_IDLE_MS after the
 * previous tone ended, the operator has stopped sending. The current burst
 * is closed out (folded into the running session totals) and a new burst
 * begins. The pause itself is excluded from both burst and session time.
 *
 * Two readouts share the same underlying events:
 *   - burstWpm()   - current burst only; responsive, freezes at its last
 *                    valid value during a pause, -1 until there's data.
 *   - sessionWpm() - everything since the last reset(), pauses excluded.
 *
 * This header is numeric-only. Per project policy, effective WPM is always
 * shown as a number - never as a live dot/dash or symbol-timing display.
 */

#ifndef EFFECTIVE_WPM_H
#define EFFECTIVE_WPM_H

#define EFFECTIVE_WPM_IDLE_MS 5000

struct EffectiveWpm {
  unsigned long burstStartMs    = 0;     // first tone of the current burst (0 = no burst yet)
  unsigned long lastToneEndMs   = 0;     // most recent tone end
  int           burstChars      = 0;     // decoded chars in the current burst (excl. ' ')
  unsigned long sessionActiveMs = 0;     // summed elapsed of CLOSED bursts
  int           sessionChars    = 0;     // chars in CLOSED bursts
  float         lastBurstWpm    = -1.0f; // frozen reading shown during a pause; -1 = none yet

  void reset() {
    burstStartMs = 0;
    lastToneEndMs = 0;
    burstChars = 0;
    sessionActiveMs = 0;
    sessionChars = 0;
    lastBurstWpm = -1.0f;
  }

  // Shared PARIS-convention WPM calc with the validity/clamp rules applied.
  // Returns -1.0f when the sample isn't large enough to trust.
  static float computeWpm(int chars, unsigned long activeMs) {
    if (chars < 3 || activeMs < 1000) return -1.0f;
    float wpmVal = (chars / 5.0f) / (activeMs / 60000.0f);
    if (wpmVal < 0.0f) wpmVal = 0.0f;
    if (wpmVal > 99.0f) wpmVal = 99.0f;
    return wpmVal;
  }

  // Call when a tone starts (key/paddle goes active).
  void onToneStart(unsigned long now) {
    if (burstStartMs != 0 && lastToneEndMs != 0 &&
        (now - lastToneEndMs) > EFFECTIVE_WPM_IDLE_MS) {
      // Operator stopped sending for a while - close out the current burst.
      unsigned long elapsed = (lastToneEndMs >= burstStartMs) ? (lastToneEndMs - burstStartMs) : 0;
      float closedWpm = computeWpm(burstChars, elapsed);
      if (closedWpm >= 0.0f) lastBurstWpm = closedWpm;
      sessionActiveMs += elapsed;
      sessionChars += burstChars;
      burstStartMs = now;
      burstChars = 0;
    } else if (burstStartMs == 0) {
      // First tone ever.
      burstStartMs = now;
    }
    // Otherwise: normal gap within an ongoing burst - nothing to do.
  }

  // Call when a tone ends (key/paddle released).
  void onToneEnd(unsigned long now) {
    lastToneEndMs = now;
  }

  // Call once per decoded character (including '?'); word-space markers
  // (' ') are excluded from the count, per the PARIS "5 chars = 1 word" rule.
  void onChar(char c) {
    if (c != ' ') burstChars++;
  }

  // Current burst's effective WPM. Stores into lastBurstWpm when the burst
  // itself is valid; otherwise returns the last frozen reading (or -1 if
  // there has never been one).
  float burstWpm() {
    if (burstStartMs == 0) return lastBurstWpm;
    unsigned long elapsed = (lastToneEndMs >= burstStartMs) ? (lastToneEndMs - burstStartMs) : 0;
    float wpmVal = computeWpm(burstChars, elapsed);
    if (wpmVal >= 0.0f) {
      lastBurstWpm = wpmVal;
      return wpmVal;
    }
    return lastBurstWpm;
  }

  // Effective WPM across everything since the last reset() - closed bursts
  // plus whatever the current burst has accumulated so far. Pauses between
  // bursts are excluded. Returns -1.0f if there isn't a valid sample yet.
  float sessionWpm() {
    unsigned long burstElapsed = (burstStartMs != 0 && lastToneEndMs >= burstStartMs)
        ? (lastToneEndMs - burstStartMs) : 0;
    int totalChars = sessionChars + burstChars;
    unsigned long totalMs = sessionActiveMs + burstElapsed;
    return computeWpm(totalChars, totalMs);
  }
};

#endif // EFFECTIVE_WPM_H
