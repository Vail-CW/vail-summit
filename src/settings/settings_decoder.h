/*
 * Decoder Settings
 * Persists user choice between Adaptive and Direct decoder.
 */

#ifndef SETTINGS_DECODER_H
#define SETTINGS_DECODER_H

#include <Preferences.h>

enum DecoderType { DECODER_ADAPTIVE = 0, DECODER_DIRECT = 1 };

DecoderType decoderType = DECODER_DIRECT;

// Clamp to the enum range (decoderType indexes the decoder name table)
inline DecoderType clampDecoderType(int type) {
  if (type < DECODER_ADAPTIVE || type > DECODER_DIRECT) return DECODER_DIRECT;
  return (DecoderType)type;
}

void loadDecoderSettings() {
  Preferences prefs;
  prefs.begin("decoder", true);
  decoderType = clampDecoderType(prefs.getInt("type", DECODER_DIRECT));
  prefs.end();
}

void saveDecoderSettings() {
  decoderType = clampDecoderType((int)decoderType);
  Preferences prefs;
  prefs.begin("decoder", false);
  prefs.putInt("type", (int)decoderType);
  prefs.end();
}

#endif // SETTINGS_DECODER_H
