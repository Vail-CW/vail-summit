/*
 * Morse Code Decoder
 * Based on morse-pro by Stephen C Phillips (https://github.com/scp93ch/morse-pro)
 * Ported to C++ for ESP32/Arduino by VAIL SUMMIT project
 *
 * Original license: EUPL v1.2
 * Copyright (c) 2024 Stephen C Phillips
 * Modifications Copyright (c) 2025 VAIL SUMMIT Contributors
 *
 * Licensed under the European Union Public Licence (EUPL) v1.2
 * https://opensource.org/licenses/EUPL-1.2
 */

#ifndef MORSE_DECODER_H
#define MORSE_DECODER_H

#include <Arduino.h>
#include <vector>
#include "morse_wpm.h"  // Same folder
#include "../core/morse_code.h"

// Maximum history size for decoder vectors to prevent unbounded memory growth
#define MORSE_DECODER_MAX_HISTORY 500
#define MORSE_DECODER_TRIM_AMOUNT 100

/**
 * Reverse lookup: morse pattern -> character or prosign as a static string.
 * Returns "" for an unknown pattern. No heap use: every result points into
 * flash/static tables, so the decoders can call this per character freely.
 */
static const char* const kMorseProsigns[][2] = {
  { ".-.-.",     "<AR>"  },  // End of message (+ over .)
  { ".-...",     "<AS>"  },  // Wait
  { "-...-.-",   "<BK>"  },  // Break (B + K)
  { "-...-",     "<BT>"  },  // Break (= over -)
  { "-.-.-",     "<CT>"  },  // Starting signal
  { "........",  "<HH>"  },  // Error/correction (8 dits)
  { "...-.-",    "<SK>"  },  // End of contact (. over -)
  { "...-.",     "<SN>"  },  // Understood
  { "...---...", "<SOS>" },  // Distress
};

// Single-character results, in morseTable order (A-Z, 0-9, punctuation)
static const char kMorseSingle[54][2] = {
  {'A', 0},
  {'B', 0},
  {'C', 0},
  {'D', 0},
  {'E', 0},
  {'F', 0},
  {'G', 0},
  {'H', 0},
  {'I', 0},
  {'J', 0},
  {'K', 0},
  {'L', 0},
  {'M', 0},
  {'N', 0},
  {'O', 0},
  {'P', 0},
  {'Q', 0},
  {'R', 0},
  {'S', 0},
  {'T', 0},
  {'U', 0},
  {'V', 0},
  {'W', 0},
  {'X', 0},
  {'Y', 0},
  {'Z', 0},
  {'0', 0},
  {'1', 0},
  {'2', 0},
  {'3', 0},
  {'4', 0},
  {'5', 0},
  {'6', 0},
  {'7', 0},
  {'8', 0},
  {'9', 0},
  {'.', 0},
  {',', 0},
  {'?', 0},
  {'\'', 0},
  {'!', 0},
  {'/', 0},
  {'(', 0},
  {')', 0},
  {'&', 0},
  {':', 0},
  {';', 0},
  {'=', 0},
  {'+', 0},
  {'-', 0},
  {'_', 0},
  {'\"', 0},
  {'$', 0},
  {'@', 0}
};

const char* morseToText(const char* pattern) {
  // Prosigns first (sent without inter-character spacing)
  for (size_t i = 0; i < sizeof(kMorseProsigns) / sizeof(kMorseProsigns[0]); i++) {
    if (strcmp(pattern, kMorseProsigns[i][0]) == 0) return kMorseProsigns[i][1];
  }
  for (int i = 0; i < 54; i++) {
    if (strcmp(pattern, morseTable[i]) == 0) return kMorseSingle[i];
  }
  return "";  // Unknown pattern
}

// Single-character convenience wrapper
char morseToChar(const char* pattern) {
  const char* text = morseToText(pattern);
  return (text[0] != '\0' && text[1] == '\0') ? text[0] : '\0';
}

/**
 * MorseDecoder - Base class for morse code decoding
 * Accepts timing values (positive for tone, negative for silence)
 * and converts them to text
 */
class MorseDecoder {
protected:
  float ditLen;    // Current dit length estimate (ms)
  float fditLen;   // Current Farnsworth dit length estimate (ms)
  float ditDahThreshold;   // Threshold between dit and dah
  float dahSpaceThreshold; // Threshold between dah and character space
  float noiseThreshold;    // Filter out very short durations (ms)

  std::vector<float> unusedTimes;   // Buffer of timings not yet decoded
  std::vector<float> timings;       // All timings (for debugging/analysis)
  std::vector<char> characters;     // All decoded characters

  /**
   * Update classification thresholds based on current dit/fdit estimates
   */
  void updateThresholds() {
    // Dit/Dah boundary: midpoint between 1-dit and 3-dit (= 2 dits)
    ditDahThreshold = ditLen + (3.0f * ditLen - ditLen) / 2.0f;

    // Dah/Space boundary: midpoint between 3-fdit and 7-fdit gaps (= 5 fdits)
    dahSpaceThreshold = 3.0f * fditLen + (7.0f * fditLen - 3.0f * fditLen) / 2.0f;
  }

  /**
   * Convert timings to morse code characters
   * @param times Vector of timing values
   * @return String of morse characters: . - ' ' (char gap) / (word gap)
   */
  String timings2morse(const std::vector<float>& times) {
    String result = "";

    for (float duration : times) {
      char character;

      if (duration > 0) {
        // Positive = tone (dit or dah)
        if (duration < ditDahThreshold) {
          character = '.';
        } else {
          character = '-';
        }
      } else {
        // Negative = silence (element gap, character gap, or word gap)
        float absDuration = abs(duration);

        if (absDuration < ditDahThreshold) {
          // Element gap - ignore (part of same character)
          continue;
        } else if (absDuration < dahSpaceThreshold) {
          // Character gap
          character = ' ';
        } else {
          // Word gap
          character = '/';
        }
      }

      result += character;
    }

    return result;
  }

  /**
   * Called after each element is decoded
   * Override in subclasses for adaptive speed tracking
   * @param duration Timing duration
   * @param character Decoded character (. - ' ' /)
   */
  virtual void addDecode(float duration, char character) {
    // Base class does nothing - override for adaptive behavior
  }

public:
  // Callback for decoded messages
  // Parameters: (morsePattern, decodedText)
  void (*messageCallback)(String morse, String text) = nullptr;

  // Callback for speed updates
  // Parameters: (wpm, fwpm)
  void (*speedCallback)(float wpm, float fwpm) = nullptr;

  /**
   * Constructor
   * @param wpm Initial words per minute estimate
   * @param fwpm Initial Farnsworth WPM estimate
   */
  MorseDecoder(float wpm = 20.0f, float fwpm = 20.0f) {
    ditLen = MorseWPM::ditLength(wpm);
    fditLen = MorseWPM::farnsworthDitLength(wpm, fwpm);
    noiseThreshold = 10.0f; // Filter durations < 10ms
    updateThresholds();
  }

  virtual ~MorseDecoder() {}

  bool isEmpty() const { return unusedTimes.empty(); }
  float getDitLen() const { return ditLen; }

  // Called periodically by external timer; override in subclasses for proactive flushing
  virtual void tick() {}

  /**
   * Add a timing value to the decoder
   * @param duration Positive for tone-on, negative for silence
   */
  virtual void addTiming(float duration) {
    // Get last timing if buffer not empty
    float last = unusedTimes.empty() ? 0 : unusedTimes.back();

    // Combine consecutive same-sign timings (extend duration)
    if (duration * last > 0) {
      unusedTimes.pop_back();
      duration = last + duration;
    }
    // Filter noise: very short durations get merged with previous
    else if (abs(duration) <= noiseThreshold && !unusedTimes.empty()) {
      unusedTimes.pop_back();
      duration = last - duration;  // Subtract noise from opposite sign
    }

    unusedTimes.push_back(duration);

    // Auto-flush on character gap (3 dits) to handle real-time decoding
    // Note: dahSpaceThreshold is the midpoint between dah and char gap (5 fdits)
    // We use 2.5 dits as the threshold to reliably detect character boundaries
    // while avoiding false triggers on inter-element gaps (1 dit) with jitter
    if (duration < 0 && abs(duration) >= (ditLen * 2.5f)) {
      flush();
    }
  }

  /**
   * Force decode of buffered timings
   */
  void flush() {
    if (unusedTimes.empty()) return;

    // Convert timings to morse pattern
    String morse = timings2morse(unusedTimes);

    // Store timings for analysis
    for (float t : unusedTimes) {
      timings.push_back(t);
    }

    // Cap timings history to prevent unbounded growth
    if (timings.size() > MORSE_DECODER_MAX_HISTORY) {
      timings.erase(timings.begin(), timings.begin() + MORSE_DECODER_TRIM_AMOUNT);
    }

    // Call addDecode for each element (for adaptive tracking).
    // Re-classify each timing individually so element gaps (skipped in timings2morse)
    // don't cause index misalignment with the morse string.
    for (float t : unusedTimes) {
      float abst = abs(t);
      char cls;
      if (t > 0) {
        cls = (abst < ditDahThreshold) ? '.' : '-';
      } else if (abst < ditDahThreshold) {
        cls = '\0';  // element gap = 1 dit
      } else if (abst < dahSpaceThreshold) {
        cls = ' ';
      } else {
        cls = '/';
      }
      addDecode(t, cls);
    }

    // Store morse characters
    for (char c : morse) {
      characters.push_back(c);
    }

    // Cap characters history to prevent unbounded growth
    if (characters.size() > MORSE_DECODER_MAX_HISTORY) {
      characters.erase(characters.begin(), characters.begin() + MORSE_DECODER_TRIM_AMOUNT);
    }

    // Decode morse pattern to text
    String decodedText = "";
    String currentPattern = "";

    for (char c : morse) {
      if (c == '.' || c == '-') {
        currentPattern += c;
      } else if (c == ' ') {
        // Character boundary
        if (currentPattern.length() > 0) {
          const char* decoded = morseToText(currentPattern.c_str());
          if (decoded[0] != '\0') {
            decodedText += decoded;
          } else {
            decodedText += '?'; // Unknown pattern
          }
          currentPattern = "";
        }
      } else if (c == '/') {
        // Word boundary
        if (currentPattern.length() > 0) {
          const char* decoded = morseToText(currentPattern.c_str());
          if (decoded[0] != '\0') {
            decodedText += decoded;
          } else {
            decodedText += '?';
          }
          currentPattern = "";
        }
        decodedText += ' ';  // Add space between words
      }
    }

    // Handle remaining pattern
    if (currentPattern.length() > 0) {
      const char* decoded = morseToText(currentPattern.c_str());
      if (decoded[0] != '\0') {
        decodedText += decoded;
      } else {
        decodedText += '?';
      }
    }

    // Clear buffer
    unusedTimes.clear();

    // Trigger callback
    if (messageCallback != nullptr && decodedText.length() > 0) {
      messageCallback(morse, decodedText);
    }
  }

  /**
   * Set expected WPM speed
   * @param wpm Words per minute
   */
  void setWPM(float wpm) {
    ditLen = MorseWPM::ditLength(wpm);
    updateThresholds();

    if (speedCallback != nullptr) {
      speedCallback(wpm, MorseWPM::wpm(fditLen));
    }
  }

  /**
   * Set Farnsworth WPM
   * @param wpm Character speed
   * @param fwpm Effective speed
   */
  void setFarnsworthWPM(float wpm, float fwpm) {
    ditLen = MorseWPM::ditLength(wpm);
    fditLen = MorseWPM::farnsworthDitLength(wpm, fwpm);
    updateThresholds();

    if (speedCallback != nullptr) {
      speedCallback(wpm, fwpm);
    }
  }

  /**
   * Get current WPM estimate
   * @return Words per minute
   */
  float getWPM() const {
    return MorseWPM::wpm(ditLen);
  }

  /**
   * Get current Farnsworth WPM estimate
   * @return Effective words per minute
   */
  float getFarnsworthWPM() const {
    return MorseWPM::wpm(fditLen);
  }

  /**
   * Reset decoder state
   */
  virtual void reset() {
    unusedTimes.clear();
    timings.clear();
    characters.clear();
  }
};

#endif // MORSE_DECODER_H
