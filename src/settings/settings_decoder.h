/*
 * Decoder Settings
 * Persists user choice between Adaptive and Direct decoder.
 */

#ifndef SETTINGS_DECODER_H
#define SETTINGS_DECODER_H

#include <Preferences.h>

enum DecoderType { DECODER_ADAPTIVE = 0, DECODER_DIRECT = 1 };

DecoderType decoderType = DECODER_DIRECT;

void loadDecoderSettings() {
  Preferences prefs;
  prefs.begin("decoder", true);
  decoderType = (DecoderType)prefs.getInt("type", DECODER_DIRECT);
  prefs.end();
}

void saveDecoderSettings() {
  Preferences prefs;
  prefs.begin("decoder", false);
  prefs.putInt("type", (int)decoderType);
  prefs.end();
}

#endif // SETTINGS_DECODER_H
