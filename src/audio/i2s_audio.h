/*
 * I2S Audio Library for MAX98357A Class-D Amplifier
 * Replaces PWM buzzer with high-quality audio output
 * Includes software volume control (0-100%)
 */

#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include <driver/i2s.h>
#include <driver/gpio.h>
#include <math.h>
#include <Preferences.h>
#include "../core/config.h"
#include "../core/deferred_save.h"

// I2S port number
#define I2S_NUM I2S_NUM_0

// Base amplitude for audio output (max 32767 for 16-bit). This is the digital
// ceiling at 100% volume; the perceptual curve (getVolumeScale) scales down
// from here. NOTE: the earlier "distortion at any level" was a hardware fault
// (the MAX98357A LRC/word-select pin was unsoldered, so I2S framing was broken)
// - not amplitude. With proper I2S, this moderate level is clean.
#define I2S_BASE_AMPLITUDE 8000

// Forward declarations
void continueTone(int frequency);
static void initSineLUT();

// Boot preset options
enum BootPreset {
    BOOT_NORMAL = 0,      // Boot at last saved volume
    BOOT_LOW_VOLUME = 1,  // Boot at 10% (legacy quiet boot)
    BOOT_HEADPHONES = 2,  // Boot at headphones preset
    BOOT_SPEAKER = 3      // Boot at speaker preset
};

// Global audio state
static bool i2s_initialized = false;
static bool tone_playing = false;
static unsigned long tone_start_time = 0;
static unsigned long tone_duration = 0;
static float phase = 0.0;  // Phase accumulator for continuous tone
static int current_frequency = 0;
static int audio_volume = DEFAULT_VOLUME;  // Volume 0-100%
static bool quiet_boot_enabled = false;    // Boot at low volume (10%) - legacy, migrated to bootPreset
static int headphones_preset = 25;         // Headphones preset volume
static int speaker_preset = 75;            // Speaker preset volume
static int boot_preset = BOOT_NORMAL;      // Boot volume option
static Preferences volumePrefs;

/*
 * Load volume from preferences
 * Also loads presets and boot option, with migration from legacy quietboot
 */
void loadVolume() {
  volumePrefs.begin("audio", false);

  // Load main volume
  audio_volume = volumePrefs.getInt("volume", DEFAULT_VOLUME);
  if (audio_volume < VOLUME_MIN || audio_volume > VOLUME_MAX) {
    audio_volume = DEFAULT_VOLUME;
  }

  // Load presets
  headphones_preset = volumePrefs.getInt("presetHP", 25);
  speaker_preset = volumePrefs.getInt("presetSpk", 75);

  // Migration: check for legacy quietboot setting
  bool hasBootPreset = volumePrefs.isKey("bootPreset");
  quiet_boot_enabled = volumePrefs.getBool("quietboot", false);

  if (!hasBootPreset) {
    // First run after update - migrate from quietboot
    if (quiet_boot_enabled) {
      boot_preset = BOOT_LOW_VOLUME;
      Serial.println("Migrated quietboot=true to BOOT_LOW_VOLUME");
    } else {
      boot_preset = BOOT_NORMAL;
    }
    volumePrefs.putInt("bootPreset", boot_preset);
  } else {
    boot_preset = volumePrefs.getInt("bootPreset", BOOT_NORMAL);
  }

  volumePrefs.end();

  Serial.printf("Loaded volume: %d%%\n", audio_volume);
  Serial.printf("Headphones preset: %d%%, Speaker preset: %d%%\n", headphones_preset, speaker_preset);
  Serial.printf("Boot preset: %d\n", boot_preset);

  // Apply boot preset override
  switch (boot_preset) {
    case BOOT_LOW_VOLUME:
      audio_volume = 10;
      Serial.println("Boot preset: Low volume (10%)");
      break;
    case BOOT_HEADPHONES:
      audio_volume = headphones_preset;
      Serial.printf("Boot preset: Headphones (%d%%)\n", audio_volume);
      break;
    case BOOT_SPEAKER:
      audio_volume = speaker_preset;
      Serial.printf("Boot preset: Speaker (%d%%)\n", audio_volume);
      break;
    case BOOT_NORMAL:
    default:
      Serial.println("Boot preset: Normal (saved volume)");
      break;
  }
}

/*
 * Save volume to preferences
 */
void saveVolume() {
  volumePrefs.begin("audio", false);
  volumePrefs.putInt("volume", audio_volume);
  volumePrefs.end();
  Serial.printf("Saved volume: %d%%\n", audio_volume);
}

/*
 * Set volume (0-100%)
 */
void setVolume(int vol) {
  audio_volume = constrain(vol, VOLUME_MIN, VOLUME_MAX);
  // Deferred: an NVS commit per slider tick crunches the live beep
  markDeferredSave(saveVolume);
}

/*
 * Get current volume
 */
int getVolume() {
  return audio_volume;
}

/*
 * Load quiet boot setting from preferences
 */
void loadQuietBoot() {
  volumePrefs.begin("audio", false);
  quiet_boot_enabled = volumePrefs.getBool("quietboot", false);
  volumePrefs.end();
  Serial.printf("Loaded quiet boot: %s\n", quiet_boot_enabled ? "enabled" : "disabled");
}

/*
 * Save quiet boot setting to preferences
 */
void saveQuietBoot(bool enabled) {
  quiet_boot_enabled = enabled;
  volumePrefs.begin("audio", false);
  volumePrefs.putBool("quietboot", enabled);
  volumePrefs.end();
  Serial.printf("Saved quiet boot: %s\n", enabled ? "enabled" : "disabled");
}

/*
 * Get quiet boot setting
 */
bool getQuietBootEnabled() {
  return quiet_boot_enabled;
}

/*
 * Set quiet boot setting
 */
void setQuietBootEnabled(bool enabled) {
  saveQuietBoot(enabled);
}

/*
 * Get headphones preset volume
 */
int getHeadphonesPreset() {
  return headphones_preset;
}

/*
 * Set headphones preset volume
 */
void setHeadphonesPreset(int vol) {
  headphones_preset = constrain(vol, VOLUME_MIN, VOLUME_MAX);
  volumePrefs.begin("audio", false);
  volumePrefs.putInt("presetHP", headphones_preset);
  volumePrefs.end();
  Serial.printf("Saved headphones preset: %d%%\n", headphones_preset);
}

/*
 * Get speaker preset volume
 */
int getSpeakerPreset() {
  return speaker_preset;
}

/*
 * Set speaker preset volume
 */
void setSpeakerPreset(int vol) {
  speaker_preset = constrain(vol, VOLUME_MIN, VOLUME_MAX);
  volumePrefs.begin("audio", false);
  volumePrefs.putInt("presetSpk", speaker_preset);
  volumePrefs.end();
  Serial.printf("Saved speaker preset: %d%%\n", speaker_preset);
}

/*
 * Get boot preset option
 */
int getBootPreset() {
  return boot_preset;
}

/*
 * Set boot preset option
 */
void setBootPreset(int preset) {
  boot_preset = preset;
  volumePrefs.begin("audio", false);
  volumePrefs.putInt("bootPreset", boot_preset);
  volumePrefs.end();
  Serial.printf("Saved boot preset: %d\n", boot_preset);
}

/*
 * Apply headphones preset - sets current volume to headphones preset value
 */
void applyHeadphonesPreset() {
  setVolume(headphones_preset);
  Serial.printf("Applied headphones preset: %d%%\n", headphones_preset);
}

/*
 * Apply speaker preset - sets current volume to speaker preset value
 */
void applySpeakerPreset() {
  setVolume(speaker_preset);
  Serial.printf("Applied speaker preset: %d%%\n", speaker_preset);
}

/*
 * Initialize I2S interface for MAX98357A amplifier
 */
void initI2SAudio() {
  if (i2s_initialized) {
    return;
  }

  // Load saved volume
  loadVolume();

  // Precompute the DRAM sine table so tone generation never reaches into flash.
  initSineLUT();

  // I2S configuration for ESP32-S3 with MAX98357A
  // CRITICAL: Match the working test sketch exactly
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL3,  // Highest priority - must beat SPI DMA
    .dma_buf_count = 8,
    .dma_buf_len = 64,  // Smaller buffers for lower latency morse timing
    .use_apll = true,  // Use Audio PLL for cleaner clock (reduces noise)
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  Serial.println("Configuring I2S for ESP32-S3 with MAX98357A...");

  // Pin configuration - MAX98357A needs BCK, LRC, and DIN
  // GAIN pin controls hardware gain (leave floating for 9dB default)
  // SD (shutdown) pin: leave floating for always-on
  i2s_pin_config_t pin_config = {
    .mck_io_num = I2S_PIN_NO_CHANGE,  // No MCLK needed for MAX98357A
    .bck_io_num = I2S_BCK_PIN,
    .ws_io_num = I2S_LCK_PIN,
    .data_out_num = I2S_DATA_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  Serial.printf("Pin config: BCLK=%d, LRC=%d, DIN=%d\n",
                pin_config.bck_io_num,
                pin_config.ws_io_num,
                pin_config.data_out_num);

  // Install and start I2S driver
  esp_err_t err = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed to install I2S driver: %d\n", err);
    return;
  }

  err = i2s_set_pin(I2S_NUM, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed to set I2S pins: %d\n", err);
    i2s_driver_uninstall(I2S_NUM);
    return;
  }

  // Set GPIO drive strength to maximum for reliable I2S signals
  gpio_set_drive_capability((gpio_num_t)I2S_BCK_PIN, GPIO_DRIVE_CAP_3);
  gpio_set_drive_capability((gpio_num_t)I2S_LCK_PIN, GPIO_DRIVE_CAP_3);
  gpio_set_drive_capability((gpio_num_t)I2S_DATA_PIN, GPIO_DRIVE_CAP_3);
  Serial.println("Set I2S GPIO drive strength to maximum");

  // Clear the DMA buffers
  i2s_zero_dma_buffer(I2S_NUM);

  i2s_initialized = true;
  Serial.println("I2S Audio initialized successfully");
  Serial.printf("  BCK: GPIO %d\n", I2S_BCK_PIN);
  Serial.printf("  LCK: GPIO %d\n", I2S_LCK_PIN);
  Serial.printf("  DATA: GPIO %d\n", I2S_DATA_PIN);
  Serial.printf("  Sample Rate: %d Hz\n", I2S_SAMPLE_RATE);
}

/*
 * Perceptual volume scale (0.0-1.0). Loudness is logarithmic, so a linear
 * volume % feels far too loud at low settings (almost all of the perceived
 * change is in the bottom few percent). Squaring the normalized volume gives a
 * usable range where low UI values are genuinely quiet.
 */
static inline float getVolumeScale() {
  float v = (float)audio_volume * 0.01f;   // 0..1
  return v * v;                            // perceptual curve
}

// ============================================
// DRAM sine table (flash-independent tone generation)
// ============================================
// The generator indexes this table instead of calling sinf() per sample. sinf()
// is libm code in flash; calling it ~22 kHz tied the audio hot path to the
// instruction cache, so a flash write on the OTHER core (an NVS commit during UI
// navigation disables both cores' cache for the duration) would stall sample
// generation and underrun the I2S DMA -> the intermittent distortion. The table
// lives in DRAM and the fill routine in IRAM, so the buffer keeps being fed even
// while the flash cache is briefly disabled. This is what lets audio stay ahead
// of everything else in the stack.
#define SINE_LUT_BITS 10
#define SINE_LUT_SIZE (1 << SINE_LUT_BITS)        // 1024 entries
#define SINE_LUT_MASK (SINE_LUT_SIZE - 1)
static int16_t sineLUT[SINE_LUT_SIZE];            // .bss (DRAM)
static bool sineLUTReady = false;

static void initSineLUT() {
  if (sineLUTReady) return;
  for (int i = 0; i < SINE_LUT_SIZE; i++) {
    sineLUT[i] = (int16_t)lroundf(sinf(2.0f * (float)PI * i / SINE_LUT_SIZE) * 32767.0f);
  }
  sineLUTReady = true;
}

// Fill a stereo buffer with a tone. Lives in IRAM and reads only DRAM (the
// table), so it runs even while the flash cache is disabled. `phaseRef` carries
// the phase accumulator across calls for click-free continuity; `amp` already
// folds in the volume scale and the 16-bit amplitude ceiling, so each sample is
// sineLUT[idx] * amp == sin(phase) * I2S_BASE_AMPLITUDE * volume (unchanged).
static void IRAM_ATTR fillToneBuffer(int16_t* buf, int frames, float* phaseRef,
                                     float phase_increment, float amp) {
  const float twoPi = 2.0f * (float)PI;
  const float idxScale = (float)SINE_LUT_SIZE / twoPi;
  float ph = *phaseRef;
  for (int i = 0; i < frames; i++) {
    int idx = (int)(ph * idxScale) & SINE_LUT_MASK;
    int16_t s = (int16_t)(sineLUT[idx] * amp);
    buf[i * 2]     = s;   // Left
    buf[i * 2 + 1] = s;   // Right
    ph += phase_increment;
    if (ph >= twoPi) ph -= twoPi;
  }
  *phaseRef = ph;
}

// Per-sample amplitude factor: sample = sineLUT[idx] * this == sin * AMP * vol.
static inline float toneAmp() {
  return getVolumeScale() * (I2S_BASE_AMPLITUDE / 32767.0f);
}

/*
 * Generate and play a tone at specified frequency for specified duration
 * Non-blocking - call updateTone() in loop to handle timing
 */
void playTone(int frequency, int duration_ms) {
  if (!i2s_initialized) {
    Serial.println("ERROR: I2S not initialized in playTone!");
    return;
  }

  tone_playing = true;
  tone_start_time = millis();
  tone_duration = duration_ms;

  // Reset phase for clean start
  float local_phase = 0.0;
  float phase_increment = 2.0 * PI * frequency / I2S_SAMPLE_RATE;

  int16_t sample_buffer[I2S_BUFFER_SIZE];

  // Write samples to I2S - keep writing while tone should be playing
  size_t bytes_written;
  unsigned long samples_to_write = (unsigned long)I2S_SAMPLE_RATE * duration_ms / 1000;
  unsigned long samples_written = 0;

  while (samples_written < samples_to_write && tone_playing) {
    // Generate from the DRAM table via the IRAM fill routine so the buffer keeps
    // feeding even if the flash cache is briefly disabled on the other core.
    fillToneBuffer(sample_buffer, I2S_BUFFER_SIZE / 2, &local_phase, phase_increment, toneAmp());

    esp_err_t result = i2s_write(I2S_NUM, sample_buffer, I2S_BUFFER_SIZE * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    if (result != ESP_OK) {
      Serial.printf("I2S write error: %d\n", result);
    }
    samples_written += I2S_BUFFER_SIZE / 2;

    // Allow other tasks to run
    yield();
  }

  // Silence at end
  memset(sample_buffer, 0, sizeof(sample_buffer));
  i2s_write(I2S_NUM, sample_buffer, I2S_BUFFER_SIZE * sizeof(int16_t), &bytes_written, portMAX_DELAY);

  tone_playing = false;
}

/*
 * Start playing a continuous tone at specified frequency
 * Use for morse code where you control start/stop timing
 */
void startTone(int frequency) {
  if (!i2s_initialized) {
    Serial.println("ERROR: I2S not initialized in startTone!");
    return;
  }

  // Only reset phase when frequency actually changes (not when restarting same tone)
  // This prevents clicks when rapidly starting/stopping at the same frequency
  if (current_frequency != frequency) {
    phase = 0.0;  // Reset phase only on frequency change
    current_frequency = frequency;
  }
  // (no per-tone serial prints - this runs per keyed element)

  tone_playing = true;

  // Immediately fill the I2S buffer to start playback
  // This prevents the clicking issue by ensuring continuous data flow
  continueTone(frequency);
}

/*
 * Continue playing the current tone
 * Call this repeatedly in loop while tone should continue
 */
void continueTone(int frequency) {
  if (!i2s_initialized || !tone_playing) {
    return;
  }

  // Update frequency if changed
  if (current_frequency != frequency) {
    current_frequency = frequency;
  }

  int16_t sample_buffer[I2S_BUFFER_SIZE];
  float phase_increment = 2.0 * PI * current_frequency / I2S_SAMPLE_RATE;

  // Generate from the DRAM table via the IRAM fill routine (flash-independent),
  // carrying the shared phase accumulator for click-free continuity.
  fillToneBuffer(sample_buffer, I2S_BUFFER_SIZE / 2, &phase, phase_increment, toneAmp());

  // Write samples - MUST block to ensure continuous playback
  size_t bytes_written;
  i2s_write(I2S_NUM, sample_buffer, I2S_BUFFER_SIZE * sizeof(int16_t), &bytes_written, portMAX_DELAY);
}

/*
 * Stop the currently playing tone with optional fade-out to prevent clicks
 */
void stopTone() {
  if (!i2s_initialized) {
    return;
  }

  // Generate a short fade-out ramp to prevent click (about 2ms at 44100 Hz)
  if (tone_playing && current_frequency > 0) {
    int16_t ramp_buffer[I2S_BUFFER_SIZE];
    float phase_increment = 2.0 * PI * current_frequency / I2S_SAMPLE_RATE;
    float amp = toneAmp();
    int ramp_samples = I2S_BUFFER_SIZE / 4;  // Fade over ~2ms
    const float idxScale = (float)SINE_LUT_SIZE / (2.0f * (float)PI);

    for (int i = 0; i < I2S_BUFFER_SIZE / 2; i++) {
      // Calculate fade factor (1.0 -> 0.0 over ramp_samples)
      float fade = (i < ramp_samples) ? (1.0f - (float)i / ramp_samples) : 0.0f;
      int idx = (int)(phase * idxScale) & SINE_LUT_MASK;
      int16_t sample = (int16_t)(sineLUT[idx] * amp * fade);

      ramp_buffer[i * 2] = sample;       // Left
      ramp_buffer[i * 2 + 1] = sample;   // Right

      phase += phase_increment;
      if (phase >= 2.0 * PI) {
        phase -= 2.0 * PI;
      }
    }

    size_t bytes_written;
    i2s_write(I2S_NUM, ramp_buffer, I2S_BUFFER_SIZE * sizeof(int16_t), &bytes_written, 10);
  }

  tone_playing = false;
  // Don't reset phase or frequency here - preserve for potential restart at same freq
  // phase = 0.0;
  // current_frequency = 0;

  // Write silence to clear the buffer
  int16_t silence[I2S_BUFFER_SIZE] = {0};
  size_t bytes_written;
  i2s_write(I2S_NUM, silence, I2S_BUFFER_SIZE * sizeof(int16_t), &bytes_written, 10);
  i2s_zero_dma_buffer(I2S_NUM);
}

/*
 * Completely deinitialize I2S driver to free DMA memory
 * Use this before OTA updates or deep sleep to reclaim memory
 */
void deinitI2SAudio() {
  if (!i2s_initialized) {
    return;
  }

  Serial.println("Deinitializing I2S audio...");

  // Stop any playing tone first
  stopTone();

  // Uninstall I2S driver (frees DMA buffers)
  esp_err_t err = i2s_driver_uninstall(I2S_NUM);
  if (err != ESP_OK) {
    Serial.printf("Failed to uninstall I2S driver: %d\n", err);
  } else {
    Serial.println("I2S driver uninstalled successfully");
  }

  i2s_initialized = false;
}

/*
 * Check if a tone is currently playing
 */
bool isTonePlaying() {
  return tone_playing;
}

/*
 * Blocking beep function for UI feedback
 * Compatible with existing code
 */
// Forward decls: defined in task_manager.h (included after this file in the TU).
extern void requestBeep(int frequency, int duration_ms);
extern bool isAudioTaskRunning();
extern bool isMorsePlaybackActive();

void beep(int frequency, int duration) {
  // Generate UI/nav beeps directly on the CALLING core. The caller is the LVGL
  // event handler on Core 1, which has no WiFi stack (WiFi is pinned to Core 0),
  // so these short tones are immune to the WiFi-driven preemption that distorts
  // them when generated on the Core-0 audio task. This restores the clean
  // pre-rewrite behavior (the earlier "route through the task" change was made
  // to chase screeching that was actually a hardware fault - the unsoldered
  // MAX98357A LRC pin - and is no longer needed now that the pin is fixed).
  //
  // Only hand off to the audio task when it is ALREADY driving a tone (async
  // morse playback or a live keyer sidetone). That is the one case where playing
  // here too would have two cores writing the I2S peripheral at once; routing
  // through the task keeps it a single writer.
  if (isAudioTaskRunning() && (isTonePlaying() || isMorsePlaybackActive())) {
    requestBeep(frequency, duration);
  } else {
    // No trailing delay: playTone() returns once the samples (plus a silence
    // tail) are queued in I2S DMA, and the peripheral drains them on its own.
    // Blocking past that point froze the UI thread for duration+10ms on every
    // nav/select beep, which made the whole menu system feel laggy.
    // Back-to-back beeps still play sequentially - i2s_write self-paces when
    // the DMA queue is full.
    playTone(frequency, duration);
  }
}

// ============================================
// Internal Functions for Task Manager
// ============================================
// These functions are called from the audio task on Core 0
// They are the actual I2S operations, separate from the request API

/*
 * Internal: Play a tone for a specific duration
 * Called from audio task - blocks until complete
 */
void playToneInternal(int frequency, int duration_ms) {
  playTone(frequency, duration_ms);
}

/*
 * Internal: Start a continuous tone
 * Called from audio task
 */
void startToneInternal(int frequency) {
  if (!i2s_initialized) {
    return;
  }

  if (!tone_playing || current_frequency != frequency) {
    phase = 0.0;
    current_frequency = frequency;
  }

  tone_playing = true;
  continueTone(frequency);
}

/*
 * Internal: Continue filling the audio buffer
 * Called from audio task when tone is playing
 */
void continueToneInternal(int frequency) {
  continueTone(frequency);
}

/*
 * Internal: Stop the current tone
 * Called from audio task
 */
void stopToneInternal() {
  stopTone();
}

/*
 * Internal: Check if tone is playing
 * Can be called from any core
 */
bool isTonePlayingInternal() {
  return tone_playing;
}

#endif // I2S_AUDIO_H
