/*
 * Morse Shooter Game
 * Classic arcade-style game where players shoot falling letters using morse code
 */

#ifndef GAME_MORSE_SHOOTER_H
#define GAME_MORSE_SHOOTER_H

#include "../core/config.h"
#include "../core/morse_code.h"
#include "../audio/i2s_audio.h"
#include "../audio/morse_decoder_direct.h"
#include "../settings/settings_decoder.h"
#include "../keyer/keyer.h"
#include <Preferences.h>

// ============================================
// Game Constants
// ============================================

#define MAX_FALLING_LETTERS 8      // Object pool size (matches LVGL label pool)
#define GAME_TICK_MS 50            // Physics tick interval - smooth 20fps motion
// Game ground level for LVGL layout (canvas is 240px tall, starts at y=40)
// Letters hit ground when y >= 200 in game coords (y=240 on screen, above bottom HUD)
#define GAME_GROUND_Y 200
#define MAX_LIVES 3               // Default lives (letters that can hit ground)
#define WORD_CHAR_PX 20           // Approx glyph width of font_large, for word layout

// ============================================
// Game Modes
// ============================================

enum ShooterGameMode {
    SHOOTER_MODE_CLASSIC = 0,
    SHOOTER_MODE_PROGRESSIVE = 1,
    SHOOTER_MODE_WORD = 2,
    SHOOTER_MODE_CALLSIGN = 3
};

static const char* GAME_MODE_NAMES[] = {"Classic", "Progressive", "Word", "Callsign"};

// ============================================
// Difficulty System
// ============================================

// Preset difficulty levels
enum ShooterPreset {
    PRESET_CUSTOM = 0,
    PRESET_BEGINNER = 1,
    PRESET_EASY = 2,
    PRESET_MEDIUM = 3,
    PRESET_HARD = 4,
    PRESET_EXPERT = 5,
    PRESET_INSANE = 6
};

static const char* PRESET_NAMES[] = {"Custom", "Beginner", "Easy", "Medium", "Hard", "Expert", "Insane"};

// Character set flags (bitmask)
#define CHARSET_FLAG_LETTERS     0x01
#define CHARSET_FLAG_NUMBERS     0x02
#define CHARSET_FLAG_PUNCTUATION 0x04

// Character sets
static const char CHARSET_BEGINNER[] = "ETIANMS";
static const char CHARSET_LETTERS_ALL[] = "ETIANMSURWDKGOHVFLPJBXCYZQ";
static const char CHARSET_LETTERS_NUMBERS[] = "ETIANMSURWDKGOHVFLPJBXCYZQ0123456789";
static const char CHARSET_FULL[] = "ETIANMSURWDKGOHVFLPJBXCYZQ0123456789.,?/";

// Full settings structure for granular control
struct ShooterSettings {
    uint8_t gameMode;        // ShooterGameMode
    uint8_t preset;          // ShooterPreset
    uint8_t fallSpeed;       // 1-10 scale
    uint8_t spawnRate;       // 1-10 scale
    uint8_t maxLetters;      // 3-8 concurrent falling objects
    uint8_t startLives;      // 1-5 lives
    uint8_t charsetFlags;    // Bitmask of character groups
};

// Default settings
static ShooterSettings shooterSettings = {
    SHOOTER_MODE_CLASSIC,    // gameMode
    PRESET_MEDIUM,           // preset
    5,                       // fallSpeed (1-10)
    5,                       // spawnRate (1-10)
    5,                       // maxLetters
    3,                       // startLives
    CHARSET_FLAG_LETTERS     // charsetFlags
};

// Preset configurations: {speed, spawn, lives, maxLetters, charsetFlags}
struct PresetConfig {
    uint8_t fallSpeed;
    uint8_t spawnRate;
    uint8_t startLives;
    uint8_t maxLetters;
    uint8_t charsetFlags;
    const char* charset;     // Direct charset pointer for presets
    int charsetSize;
};

static const PresetConfig PRESET_CONFIGS[] = {
    {5, 5, 3, 5, CHARSET_FLAG_LETTERS, CHARSET_LETTERS_ALL, 26},                          // Custom (defaults)
    {1, 1, 5, 3, CHARSET_FLAG_LETTERS, CHARSET_BEGINNER, 7},                              // Beginner
    {3, 3, 3, 4, CHARSET_FLAG_LETTERS, CHARSET_BEGINNER, 7},                              // Easy
    {5, 5, 3, 5, CHARSET_FLAG_LETTERS, CHARSET_LETTERS_ALL, 26},                          // Medium
    {7, 7, 3, 5, CHARSET_FLAG_LETTERS | CHARSET_FLAG_NUMBERS, CHARSET_LETTERS_NUMBERS, 36}, // Hard
    {8, 8, 2, 6, CHARSET_FLAG_LETTERS | CHARSET_FLAG_NUMBERS | CHARSET_FLAG_PUNCTUATION, CHARSET_FULL, 40}, // Expert
    {10, 10, 1, 8, CHARSET_FLAG_LETTERS | CHARSET_FLAG_NUMBERS | CHARSET_FLAG_PUNCTUATION, CHARSET_FULL, 40} // Insane
};

// Mapping functions: convert 1-10 scale to actual values
inline float speedToPixels(uint8_t level) {
    // Pixels per SECOND. 1 -> 6 px/s (~32s to cross), 10 -> 50 px/s (~4s to cross)
    return 6.0f + (level - 1) * 4.9f;
}

inline uint32_t spawnToInterval(uint8_t level) {
    // 1 -> 5000ms, 10 -> ~1000ms
    return 5000 - (level - 1) * 444;
}

// Per-mode high scores
static int shooterHighScoreClassic = 0;
static int shooterHighScoreProgressive = 0;
static int shooterHighScoreWord = 0;
static int shooterHighScoreCallsign = 0;
static bool shooterHighScoreDirty = false;  // Deferred NVS persist (flash writes crunch audio)

// ============================================
// Combo Scoring System
// ============================================

static int comboCount = 0;
static unsigned long lastHitTime = 0;
static unsigned long comboDisplayUntil = 0;  // When to hide combo display

// Get current combo multiplier based on streak
inline int getComboMultiplier() {
    if (comboCount >= 20) return 10;
    if (comboCount >= 10) return 5;
    if (comboCount >= 5)  return 3;
    if (comboCount >= 3)  return 2;
    return 1;
}

// Get speed bonus for quick hits (call immediately after hit)
inline int getSpeedBonus(float letterY) {
    int bonus = 0;

    // Quick hit bonus (still near top of screen)
    if (letterY < 50) {
        bonus += 5;
    }

    // Top-third bonus
    if (letterY < 70) {
        bonus += 3;
    }

    return bonus;
}

// Reset combo on miss
inline void resetCombo() {
    if (comboCount >= 3) {
        Serial.printf("[Shooter] Combo lost! Was at %d\n", comboCount);
    }
    comboCount = 0;
}

// Record a hit and return total points earned
inline int recordHit(float letterY) {
    comboCount++;
    lastHitTime = millis();
    comboDisplayUntil = lastHitTime + 2000;  // Show combo for 2 seconds

    int multiplier = getComboMultiplier();
    int basePoints = 10;
    int speedBonus = getSpeedBonus(letterY);
    int totalPoints = (basePoints * multiplier) + speedBonus;

    Serial.printf("[Shooter] Hit! Combo=%d, Mult=%dx, Speed bonus=%d, Total=%d\n",
                  comboCount, multiplier, speedBonus, totalPoints);

    return totalPoints;
}

// ============================================
// Progressive Mode State
// ============================================

static int progressiveLevel = 1;
static int progressiveHits = 0;
static unsigned long progressiveLevelStartTime = 0;
static unsigned long progressiveTimeSurvived = 0;

// Character groups for progressive unlocking
static const char* PROGRESSIVE_CHARSETS[] = {
    "ET",                                         // Level 1
    "ETIANM",                                     // Level 2
    "ETIANMSURWDKGO",                            // Level 3
    "ETIANMSURWDKGOHVFLPJBXCYZQ",               // Level 4
    "ETIANMSURWDKGOHVFLPJBXCYZQ0123456789",     // Level 5
    "ETIANMSURWDKGOHVFLPJBXCYZQ0123456789.,?/"  // Level 6+
};
static const int PROGRESSIVE_CHARSET_SIZES[] = {2, 6, 14, 26, 36, 40};

// ============================================
// Word Mode Data
// ============================================

// Word lists by difficulty
static const char* WORDS_EASY[] = {"CQ", "DE", "HI", "OK", "IT", "IS", "TO", "OF", "73", "88",
                                   "GM", "GN", "TU", "UR", "ES", "HW", "RIG", "ANT"};
static const int WORDS_EASY_COUNT = 18;

static const char* WORDS_MEDIUM[] = {"CALL", "COPY", "NAME", "QTH", "RST", "BAND", "FREQ", "WIRE",
                                     "TEST", "GOOD", "RADIO", "MORSE", "POWER", "WATTS"};
static const int WORDS_MEDIUM_COUNT = 14;

static const char* WORDS_HARD[] = {"ANTENNA", "WEATHER", "STATION", "AMATEUR", "CONTEST",
                                   "REPEATER", "SIGNAL", "OPERATOR", "FREQUENCY"};
static const int WORDS_HARD_COUNT = 9;

// Structure for falling words
struct FallingWord {
    char word[12];           // Max 11 chars + null
    uint8_t length;
    uint8_t lettersTyped;    // Progress tracker
    float x, y;
    bool active;
    unsigned long spawnTime;
};

static FallingWord fallingWords[MAX_FALLING_LETTERS];

// ============================================
// Callsign Mode Data
// ============================================

// US callsign prefixes
static const char* US_PREFIXES[] = {"W", "K", "N", "WA", "WB", "WD", "KA", "KB", "KC", "KD", "KE", "KF", "KG"};
static const int US_PREFIX_COUNT = 13;

// International prefixes
static const char* INTL_PREFIXES[] = {"VE", "G", "DL", "F", "I", "JA", "VK", "ZL", "EA", "OH", "SM", "PA"};
static const int INTL_PREFIX_COUNT = 12;

// Generate a random callsign
inline void generateCallsign(char* buffer, size_t bufSize, bool includeInternational = false) {
    const char** prefixes;
    int prefixCount;

    if (includeInternational && random(100) < 30) {  // 30% chance of international
        prefixes = INTL_PREFIXES;
        prefixCount = INTL_PREFIX_COUNT;
    } else {
        prefixes = US_PREFIXES;
        prefixCount = US_PREFIX_COUNT;
    }

    const char* prefix = prefixes[random(prefixCount)];
    int digit = random(10);

    // Suffix: 1-3 letters
    int suffixLen = random(1, 4);
    char suffix[4] = {0};
    for (int i = 0; i < suffixLen; i++) {
        suffix[i] = 'A' + random(26);
    }

    snprintf(buffer, bufSize, "%s%d%s", prefix, digit, suffix);
}

// ============================================
// Game State Structures
// ============================================

struct FallingLetter {
  char letter;
  float x;
  float y;
  bool active;
};

struct MorseInputBuffer {
  bool ditPressed;
  bool dahPressed;
};

// ============================================
// Game State Variables
// ============================================

FallingLetter fallingLetters[MAX_FALLING_LETTERS];
MorseInputBuffer morseInput;
int gameScore = 0;
int gameLives = MAX_LIVES;
int gameMaxLives = MAX_LIVES;
unsigned long lastSpawnTime = 0;
unsigned long lastGameUpdate = 0;
unsigned long gameStartTime = 0;
bool gameOver = false;
bool gamePaused = false;

// Decoder state
MorseDecoder* shooterDecoder = nullptr;
static char shooterDecodedText[13] = "";  // Rolling display of recently decoded chars
unsigned long shooterLastStateChangeTime = 0;
bool shooterLastToneState = false;
unsigned long shooterLastElementTime = 0;  // Track last element for timeout flush

// Unified keyer for all key types
static StraightKeyer* shooterKeyer = nullptr;
static bool shooterDitPressed = false;
static bool shooterDahPressed = false;

// Keyer callback - called when tone state changes
void shooterKeyerCallback(bool txOn, int element) {
  unsigned long now = millis();

  if (txOn) {
    // Tone starting
    if (!shooterLastToneState) {
      // Send silence duration to decoder (negative)
      if (shooterLastStateChangeTime > 0) {
        float silenceDuration = now - shooterLastStateChangeTime;
        if (silenceDuration > 0) {
          shooterDecoder->addTiming(-silenceDuration);
        }
      }
      shooterLastStateChangeTime = now;
      shooterLastToneState = true;
    }
    startTone(cwTone);
  } else {
    // Tone stopping
    if (shooterLastToneState) {
      float toneDuration = now - shooterLastStateChangeTime;
      if (toneDuration > 0) {
        shooterDecoder->addTiming(toneDuration);
        shooterLastElementTime = now;
      }
      shooterLastStateChangeTime = now;
      shooterLastToneState = false;
    }
    stopTone();
  }
}

// ============================================
// Preferences Functions
// ============================================

void loadShooterPrefs() {
    Preferences prefs;
    prefs.begin("shooter", true);  // read-only

    // Load expanded settings
    shooterSettings.gameMode = prefs.getUChar("mode", SHOOTER_MODE_CLASSIC);
    shooterSettings.preset = prefs.getUChar("preset", PRESET_MEDIUM);
    shooterSettings.fallSpeed = prefs.getUChar("speed", 5);
    shooterSettings.spawnRate = prefs.getUChar("spawn", 5);
    shooterSettings.maxLetters = prefs.getUChar("maxlet", 5);
    shooterSettings.startLives = prefs.getUChar("lives", 3);
    shooterSettings.charsetFlags = prefs.getUChar("charset", CHARSET_FLAG_LETTERS);

    // Clamp values to valid ranges
    if (shooterSettings.gameMode > SHOOTER_MODE_CALLSIGN) shooterSettings.gameMode = SHOOTER_MODE_CLASSIC;
    if (shooterSettings.preset > PRESET_INSANE) shooterSettings.preset = PRESET_MEDIUM;
    if (shooterSettings.fallSpeed < 1 || shooterSettings.fallSpeed > 10) shooterSettings.fallSpeed = 5;
    if (shooterSettings.spawnRate < 1 || shooterSettings.spawnRate > 10) shooterSettings.spawnRate = 5;
    if (shooterSettings.maxLetters < 3 || shooterSettings.maxLetters > 8) shooterSettings.maxLetters = 5;
    if (shooterSettings.startLives < 1 || shooterSettings.startLives > 5) shooterSettings.startLives = 3;
    shooterSettings.charsetFlags |= CHARSET_FLAG_LETTERS;  // Letters always included

    // Load per-mode high scores
    shooterHighScoreClassic = prefs.getInt("hs_classic", 0);
    shooterHighScoreProgressive = prefs.getInt("hs_prog", 0);
    shooterHighScoreWord = prefs.getInt("hs_word", 0);
    shooterHighScoreCallsign = prefs.getInt("hs_call", 0);

    prefs.end();

    Serial.printf("[Shooter] Loaded prefs: mode=%d, preset=%d, speed=%d, spawn=%d, lives=%d\n",
                  shooterSettings.gameMode, shooterSettings.preset,
                  shooterSettings.fallSpeed, shooterSettings.spawnRate, shooterSettings.startLives);
}

void saveShooterPrefs() {
    Preferences prefs;
    prefs.begin("shooter", false);  // read-write

    prefs.putUChar("mode", shooterSettings.gameMode);
    prefs.putUChar("preset", shooterSettings.preset);
    prefs.putUChar("speed", shooterSettings.fallSpeed);
    prefs.putUChar("spawn", shooterSettings.spawnRate);
    prefs.putUChar("maxlet", shooterSettings.maxLetters);
    prefs.putUChar("lives", shooterSettings.startLives);
    prefs.putUChar("charset", shooterSettings.charsetFlags);

    prefs.end();
    Serial.printf("[Shooter] Saved settings: mode=%d, preset=%d\n",
                  shooterSettings.gameMode, shooterSettings.preset);
}

void saveShooterHighScore() {
    Preferences prefs;
    prefs.begin("shooter", false);  // read-write

    prefs.putInt("hs_classic", shooterHighScoreClassic);
    prefs.putInt("hs_prog", shooterHighScoreProgressive);
    prefs.putInt("hs_word", shooterHighScoreWord);
    prefs.putInt("hs_call", shooterHighScoreCallsign);

    prefs.end();
    Serial.printf("[Shooter] Saved high scores\n");
}

// Persist high scores only if changed since last save. NVS commits stall the
// flash cache and crunch active audio, so this must only run at game over /
// exit - never per hit.
void persistShooterHighScore() {
    if (!shooterHighScoreDirty) return;
    shooterHighScoreDirty = false;
    saveShooterHighScore();
}

// Apply preset to settings
void applyShooterPreset(ShooterPreset preset) {
    if (preset == PRESET_CUSTOM) return;  // Don't overwrite custom settings

    const PresetConfig& config = PRESET_CONFIGS[preset];
    shooterSettings.preset = preset;
    shooterSettings.fallSpeed = config.fallSpeed;
    shooterSettings.spawnRate = config.spawnRate;
    shooterSettings.startLives = config.startLives;
    shooterSettings.maxLetters = config.maxLetters;
    shooterSettings.charsetFlags = config.charsetFlags;

    Serial.printf("[Shooter] Applied preset %s: speed=%d, spawn=%d, lives=%d\n",
                  PRESET_NAMES[preset], config.fallSpeed, config.spawnRate, config.startLives);
}

// Get high score for current mode
int getCurrentModeHighScore() {
    switch (shooterSettings.gameMode) {
        case SHOOTER_MODE_CLASSIC:
            return shooterHighScoreClassic;
        case SHOOTER_MODE_PROGRESSIVE:
            return shooterHighScoreProgressive;
        case SHOOTER_MODE_WORD:
            return shooterHighScoreWord;
        case SHOOTER_MODE_CALLSIGN:
            return shooterHighScoreCallsign;
        default:
            return 0;
    }
}

// Update high score for current mode (in-memory only; persisted at game over)
void updateCurrentModeHighScore(int score) {
    switch (shooterSettings.gameMode) {
        case SHOOTER_MODE_CLASSIC:
            if (score > shooterHighScoreClassic) {
                shooterHighScoreClassic = score;
                shooterHighScoreDirty = true;
            }
            break;
        case SHOOTER_MODE_PROGRESSIVE:
            if (score > shooterHighScoreProgressive) {
                shooterHighScoreProgressive = score;
                shooterHighScoreDirty = true;
            }
            break;
        case SHOOTER_MODE_WORD:
            if (score > shooterHighScoreWord) {
                shooterHighScoreWord = score;
                shooterHighScoreDirty = true;
            }
            break;
        case SHOOTER_MODE_CALLSIGN:
            if (score > shooterHighScoreCallsign) {
                shooterHighScoreCallsign = score;
                shooterHighScoreDirty = true;
            }
            break;
    }
}

// ============================================
// LVGL Display Update Functions (defined in lv_game_screens.h)
// ============================================

extern void updateShooterScore(int score);
extern void updateShooterLives(int lives, int maxLives);
extern void updateShooterDecoded(const char* text);
extern void updateShooterLetter(int index, char letter, int x, int y, bool visible);
extern void updateShooterWord(int index, const char* word, int lettersTyped, int x, int y, bool visible);
extern void updateShooterCombo(int combo, int multiplier);
extern void showShooterHitEffect(int x, int y);
extern void showShooterGameOver();

// Append a decoded character to the rolling display buffer
static void shooterAppendDecoded(char c) {
  size_t len = strlen(shooterDecodedText);
  if (len >= sizeof(shooterDecodedText) - 1) {
    memmove(shooterDecodedText, shooterDecodedText + 1, len);  // Drop oldest char
    len--;
  }
  shooterDecodedText[len] = c;
  shooterDecodedText[len + 1] = '\0';
}

/*
 * Reset game state
 */
void resetGame() {
  // Clear all falling letters
  for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
    fallingLetters[i].active = false;
  }

  // Clear falling words (for Word/Callsign modes)
  for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
    memset(&fallingWords[i], 0, sizeof(FallingWord));
  }

  // Reset morse input
  morseInput.ditPressed = false;
  morseInput.dahPressed = false;
  shooterDitPressed = false;
  shooterDahPressed = false;

  // Initialize unified keyer
  shooterKeyer = getKeyer(cwKeyType);
  shooterKeyer->reset();
  shooterKeyer->setDitDuration(DIT_DURATION(cwSpeed));
  shooterKeyer->setTxCallback(shooterKeyerCallback);

  // Initialize decoder
  delete shooterDecoder;
  shooterDecoder = (decoderType == DECODER_DIRECT)
      ? (MorseDecoder*) new MorseDecoderDirect(cwSpeed, cwSpeed, 30)
      : (MorseDecoder*) new MorseDecoderAdaptive(cwSpeed, cwSpeed, 30);
  shooterDecoder->reset();
  shooterDecoder->flush();
  shooterDecoder->setWPM(cwSpeed);
  shooterDecodedText[0] = '\0';
  shooterLastStateChangeTime = 0;
  shooterLastToneState = false;
  shooterLastElementTime = 0;

  // Reset combo system
  comboCount = 0;
  lastHitTime = 0;
  comboDisplayUntil = 0;

  // Reset progressive mode state
  progressiveLevel = 1;
  progressiveHits = 0;
  progressiveLevelStartTime = millis();
  progressiveTimeSurvived = 0;

  // Determine lives from settings/preset
  int startLives = 3;  // safe default
  if (shooterSettings.preset != PRESET_CUSTOM) {
    startLives = PRESET_CONFIGS[shooterSettings.preset].startLives;
  } else {
    startLives = shooterSettings.startLives;
  }
  if (startLives < 1) startLives = 1;
  if (startLives > 5) startLives = 5;

  // Reset game variables
  gameScore = 0;
  gameLives = startLives;
  gameMaxLives = startLives;
  lastSpawnTime = millis();
  lastGameUpdate = millis();
  gameStartTime = millis();
  gameOver = false;
  gamePaused = false;

  // Update LVGL display
  updateShooterScore(0);
  updateShooterLives(gameLives, gameMaxLives);
  updateShooterDecoded("");
  updateShooterCombo(0, 1);  // Reset combo display
  for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
    updateShooterLetter(i, ' ', 0, 0, false);  // Hide all letters
  }

  Serial.printf("[Shooter] Game reset: mode=%s, preset=%s, lives=%d\n",
                GAME_MODE_NAMES[shooterSettings.gameMode],
                PRESET_NAMES[shooterSettings.preset], gameLives);
}

// Get current fall speed (pixels per second) based on settings/mode
float getCurrentFallSpeed() {
  if (shooterSettings.gameMode == SHOOTER_MODE_PROGRESSIVE) {
    // Progressive mode: speed increases with level
    float baseSpeed = 6.0f;
    float speedIncrease = 4.0f * (progressiveLevel - 1);
    return min(baseSpeed + speedIncrease, 45.0f);
  } else if (shooterSettings.preset != PRESET_CUSTOM) {
    return speedToPixels(PRESET_CONFIGS[shooterSettings.preset].fallSpeed);
  } else {
    return speedToPixels(shooterSettings.fallSpeed);
  }
}

// Get current spawn interval based on settings/mode
uint32_t getCurrentSpawnInterval() {
  if (shooterSettings.gameMode == SHOOTER_MODE_PROGRESSIVE) {
    // Progressive mode: spawn rate increases with level (signed math - at high
    // levels the subtraction goes negative and must clamp, not wrap)
    int interval = 5000 - 300 * (progressiveLevel - 1);
    if (interval < 1200) interval = 1200;
    return (uint32_t)interval;
  } else if (shooterSettings.preset != PRESET_CUSTOM) {
    return spawnToInterval(PRESET_CONFIGS[shooterSettings.preset].spawnRate);
  } else {
    return spawnToInterval(shooterSettings.spawnRate);
  }
}

// Get current character set based on settings/mode
void getCurrentCharset(const char*& charset, int& size) {
  if (shooterSettings.gameMode == SHOOTER_MODE_PROGRESSIVE) {
    // Progressive mode: unlock characters with level
    int levelIndex = min(progressiveLevel - 1, 5);  // Max level 6
    charset = PROGRESSIVE_CHARSETS[levelIndex];
    size = PROGRESSIVE_CHARSET_SIZES[levelIndex];
  } else if (shooterSettings.preset != PRESET_CUSTOM) {
    charset = PRESET_CONFIGS[shooterSettings.preset].charset;
    size = PRESET_CONFIGS[shooterSettings.preset].charsetSize;
  } else {
    // Custom: use charset flags to determine
    if (shooterSettings.charsetFlags & CHARSET_FLAG_PUNCTUATION) {
      charset = CHARSET_FULL;
      size = 40;
    } else if (shooterSettings.charsetFlags & CHARSET_FLAG_NUMBERS) {
      charset = CHARSET_LETTERS_NUMBERS;
      size = 36;
    } else {
      charset = CHARSET_LETTERS_ALL;
      size = 26;
    }
  }
}

// Get current max letters based on settings/mode
int getCurrentMaxLetters() {
  if (shooterSettings.gameMode == SHOOTER_MODE_PROGRESSIVE) {
    // Progressive mode: more letters with level
    return min(3 + progressiveLevel / 2, 8);
  } else if (shooterSettings.preset != PRESET_CUSTOM) {
    return PRESET_CONFIGS[shooterSettings.preset].maxLetters;
  } else {
    return shooterSettings.maxLetters;
  }
}

/*
 * Handle a life lost (letter/word reached the ground). Returns true if game over.
 */
static bool shooterLoseLife() {
  gameLives--;
  resetCombo();
  updateShooterLives(gameLives, gameMaxLives);
  updateShooterCombo(0, 1);
  beep(TONE_ERROR, 200);  // Hit ground sound

  if (gameLives <= 0) {
    gameOver = true;
    if (shooterSettings.gameMode == SHOOTER_MODE_PROGRESSIVE) {
      progressiveTimeSurvived = millis() - gameStartTime;
      Serial.printf("[Shooter] Progressive game over: Level %d, Time %lu ms\n",
                    progressiveLevel, progressiveTimeSurvived);
    }
    showShooterGameOver();  // Show game over overlay (persists high score)
    return true;
  }
  return false;
}

/*
 * Update falling letters (physics) - dt is elapsed seconds since last tick
 */
void updateFallingLetters(float dt) {
  float fallSpeed = getCurrentFallSpeed();

  for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
    if (fallingLetters[i].active) {
      fallingLetters[i].y += fallSpeed * dt;

      // Update LVGL display (y+40 for header offset)
      updateShooterLetter(i, fallingLetters[i].letter,
                         (int)fallingLetters[i].x,
                         (int)fallingLetters[i].y + 40, true);

      // Check if letter hit the ground
      if (fallingLetters[i].y >= GAME_GROUND_Y) {
        fallingLetters[i].active = false;
        updateShooterLetter(i, ' ', 0, 0, false);  // Hide letter
        if (shooterLoseLife()) return;
      }
    }
  }

  // Progressive mode: check for level advancement by time
  if (shooterSettings.gameMode == SHOOTER_MODE_PROGRESSIVE && !gameOver) {
    unsigned long timeSinceLevel = millis() - progressiveLevelStartTime;
    if (timeSinceLevel >= 30000) {  // 30 seconds per level
      progressiveLevel++;
      progressiveLevelStartTime = millis();
      Serial.printf("[Shooter] Progressive level up (time)! Now level %d\n", progressiveLevel);
      beep(1000, 100);  // Level up beep
    }
  }
}

/*
 * Spawn new falling letter - supports Classic/Progressive modes
 */
void spawnFallingLetter() {
  uint32_t spawnInterval = getCurrentSpawnInterval();
  int maxLetters = getCurrentMaxLetters();

  if (millis() - lastSpawnTime < spawnInterval) {
    return;  // Not time to spawn yet
  }

  // Count active letters
  int activeCount = 0;
  int emptySlot = -1;
  for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
    if (fallingLetters[i].active) {
      activeCount++;
    } else if (emptySlot < 0) {
      emptySlot = i;
    }
  }

  // Don't exceed max letters for current difficulty
  if (activeCount >= maxLetters || emptySlot < 0) {
    return;
  }

  // Initialize the letter based on current mode/settings
  const char* charset;
  int charsetSize;
  getCurrentCharset(charset, charsetSize);

  fallingLetters[emptySlot].letter = charset[random(charsetSize)];

  // Find spawn position with collision avoidance
  int attempts = 0;
  bool positionOk = false;
  int newX;
  const int SPAWN_Y = 5;

  while (!positionOk && attempts < 20) {
    newX = random(20, SCREEN_WIDTH - 50);
    positionOk = true;

    for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
      if (i != emptySlot && fallingLetters[i].active) {
        if (abs(newX - (int)fallingLetters[i].x) < 34 &&
            abs(SPAWN_Y - (int)fallingLetters[i].y) < 40) {
          positionOk = false;
          break;
        }
      }
    }
    attempts++;
  }

  fallingLetters[emptySlot].x = newX;
  fallingLetters[emptySlot].y = SPAWN_Y;
  fallingLetters[emptySlot].active = true;

  // Update LVGL display
  updateShooterLetter(emptySlot, fallingLetters[emptySlot].letter,
                     (int)fallingLetters[emptySlot].x,
                     (int)fallingLetters[emptySlot].y + 40, true);

  lastSpawnTime = millis();
}

/*
 * Get word list based on current preset difficulty
 */
void getWordList(const char**& words, int& count) {
  uint8_t preset = shooterSettings.preset;
  if (preset <= PRESET_EASY) {
    words = WORDS_EASY;
    count = WORDS_EASY_COUNT;
  } else if (preset <= PRESET_HARD) {
    words = WORDS_MEDIUM;
    count = WORDS_MEDIUM_COUNT;
  } else {
    words = WORDS_HARD;
    count = WORDS_HARD_COUNT;
  }
}

/*
 * Spawn a falling word/callsign into a free slot with width-aware placement.
 * Shared by Word and Callsign modes.
 */
static void spawnWordCommon(const char* text) {
  uint32_t spawnInterval = getCurrentSpawnInterval();
  int maxLetters = getCurrentMaxLetters();

  // Words take longer to shoot, so allow fewer concurrent
  int maxWords = max(1, maxLetters / 2);

  if (millis() - lastSpawnTime < spawnInterval) return;

  int activeCount = 0;
  int emptySlot = -1;
  for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
    if (fallingWords[i].active) {
      activeCount++;
      // Don't spawn while another word is still near the top - prevents
      // words stacking on the same line and overlapping
      if (fallingWords[i].y < 55) return;
    } else if (emptySlot < 0) {
      emptySlot = i;
    }
  }

  if (activeCount >= maxWords || emptySlot < 0) return;

  FallingWord& fw = fallingWords[emptySlot];
  strncpy(fw.word, text, sizeof(fw.word) - 1);
  fw.word[sizeof(fw.word) - 1] = '\0';
  fw.length = strlen(fw.word);
  fw.lettersTyped = 0;
  fw.y = 5;
  fw.spawnTime = millis();

  // Width-aware spawn position: keep the whole word on screen and avoid
  // horizontal overlap with other words still in the upper half
  int wordPx = fw.length * WORD_CHAR_PX;
  int maxX = SCREEN_WIDTH - wordPx - 10;
  if (maxX <= 12) maxX = 13;  // Very long word: pin near left edge

  int newX = 10;
  int attempts = 0;
  bool positionOk = false;
  while (!positionOk && attempts < 20) {
    newX = random(10, maxX);
    positionOk = true;
    for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
      if (i != emptySlot && fallingWords[i].active && fallingWords[i].y < 120) {
        int otherPx = fallingWords[i].length * WORD_CHAR_PX;
        bool overlapX = (newX < (int)fallingWords[i].x + otherPx + 16) &&
                        ((int)fallingWords[i].x < newX + wordPx + 16);
        if (overlapX) {
          positionOk = false;
          break;
        }
      }
    }
    attempts++;
  }
  fw.x = newX;
  fw.active = true;

  updateShooterWord(emptySlot, fw.word, 0, newX, 5 + 40, true);
  lastSpawnTime = millis();
}

/*
 * Spawn a falling word (Word mode)
 */
void spawnFallingWord() {
  const char** words;
  int wordCount;
  getWordList(words, wordCount);
  spawnWordCommon(words[random(wordCount)]);
}

/*
 * Spawn a falling callsign (Callsign mode)
 */
void spawnFallingCallsign() {
  char callbuf[12];
  memset(callbuf, 0, sizeof(callbuf));
  generateCallsign(callbuf, sizeof(callbuf), true);
  spawnWordCommon(callbuf);
}

/*
 * Update falling words physics (Word/Callsign modes) - dt in seconds
 */
void updateFallingWords(float dt) {
  float fallSpeed = getCurrentFallSpeed() * 0.6f;  // Words fall slower

  for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
    if (fallingWords[i].active) {
      fallingWords[i].y += fallSpeed * dt;

      updateShooterWord(i, fallingWords[i].word, fallingWords[i].lettersTyped,
                        (int)fallingWords[i].x, (int)fallingWords[i].y + 40, true);

      if (fallingWords[i].y >= GAME_GROUND_Y) {
        fallingWords[i].active = false;
        updateShooterWord(i, NULL, 0, 0, 0, false);
        if (shooterLoseLife()) return;
      }
    }
  }
}

/*
 * Advance a word/callsign by one already-matched character.
 * Handles completion (scoring, effects, high score).
 */
static void shooterAdvanceWord(int j) {
  fallingWords[j].lettersTyped++;
  beep(1200, 30);  // Partial hit beep

  updateShooterWord(j, fallingWords[j].word, fallingWords[j].lettersTyped,
                    (int)fallingWords[j].x, (int)fallingWords[j].y + 40, true);

  if (fallingWords[j].lettersTyped >= fallingWords[j].length) {
    // WORD COMPLETE!
    int targetX = (int)fallingWords[j].x;
    int targetY = (int)fallingWords[j].y;
    float wordY = fallingWords[j].y;
    int wordLen = fallingWords[j].length;

    fallingWords[j].active = false;
    updateShooterWord(j, NULL, 0, 0, 0, false);

    beep(1200, 80);  // Completion sound
    showShooterHitEffect(targetX, targetY + 40);

    // Score: base points * word length * combo multiplier
    int pointsEarned = recordHit(wordY) * wordLen;
    gameScore += pointsEarned;
    updateShooterScore(gameScore);

    updateShooterCombo(comboCount, getComboMultiplier());
    updateCurrentModeHighScore(gameScore);
  }
}

/*
 * Process one decoded character against the game targets.
 * Called for EVERY character the decoder emits, as soon as it is decoded -
 * so multi-letter sequences (word/callsign modes) register every keystroke
 * and classic mode shoots at character-gap latency instead of word-gap.
 */
void shooterProcessChar(char decodedChar) {
  if (gameOver || gamePaused) return;

  if (decodedChar >= 'a' && decodedChar <= 'z') {
    decodedChar = decodedChar - 'a' + 'A';
  }

  // Word/Callsign modes: match character against next expected letter in words.
  // Once a word is started it is "committed" - subsequent characters apply to it
  // until it completes or you mistype. This prevents callsigns that share a prefix
  // from stealing each other's keystrokes. When no word is in progress, the closest
  // to the ground (most urgent) matching word is started.
  if (shooterSettings.gameMode == SHOOTER_MODE_WORD ||
      shooterSettings.gameMode == SHOOTER_MODE_CALLSIGN) {

    // 1. A word already in progress has priority (you committed to it).
    int inProgress = -1;
    for (int j = 0; j < MAX_FALLING_LETTERS; j++) {
      if (fallingWords[j].active && fallingWords[j].lettersTyped > 0) {
        inProgress = j;
        break;  // Only one word is ever in progress at a time
      }
    }

    if (inProgress >= 0) {
      char expected = fallingWords[inProgress].word[fallingWords[inProgress].lettersTyped];
      if (expected >= 'a' && expected <= 'z') expected = expected - 'a' + 'A';

      if (decodedChar == expected) {
        shooterAdvanceWord(inProgress);
        return;
      }

      // Mistyped the committed word - abandon its progress and fall through so
      // this character can instead start a different word (more forgiving).
      fallingWords[inProgress].lettersTyped = 0;
      updateShooterWord(inProgress, fallingWords[inProgress].word, 0,
                        (int)fallingWords[inProgress].x,
                        (int)fallingWords[inProgress].y + 40, true);
    }

    // 2. No committed word matched - start the closest matching word to the ground.
    int bestIdx = -1;
    float bestY = -1.0f;
    for (int j = 0; j < MAX_FALLING_LETTERS; j++) {
      if (!fallingWords[j].active) continue;

      char first = fallingWords[j].word[0];
      if (first >= 'a' && first <= 'z') first = first - 'a' + 'A';

      if (decodedChar == first && fallingWords[j].y > bestY) {
        bestY = fallingWords[j].y;
        bestIdx = j;
      }
    }

    if (bestIdx >= 0) {
      shooterAdvanceWord(bestIdx);
      return;
    }

    // 3. No match at all - miss.
    beep(600, 100);  // Miss sound
    resetCombo();
    updateShooterCombo(0, 1);
    return;
  }

  // Classic/Progressive modes: shoot the matching letter closest to the ground
  int bestIdx = -1;
  float bestY = -1.0f;
  for (int j = 0; j < MAX_FALLING_LETTERS; j++) {
    if (fallingLetters[j].active && fallingLetters[j].letter == decodedChar &&
        fallingLetters[j].y > bestY) {
      bestY = fallingLetters[j].y;
      bestIdx = j;
    }
  }

  if (bestIdx >= 0) {
    // HIT!
    int targetX = (int)fallingLetters[bestIdx].x;
    int targetY = (int)fallingLetters[bestIdx].y;
    float letterY = fallingLetters[bestIdx].y;

    fallingLetters[bestIdx].active = false;
    updateShooterLetter(bestIdx, ' ', 0, 0, false);

    beep(1200, 50);  // Laser sound
    showShooterHitEffect(targetX, targetY + 40);

    // Calculate score
    int pointsEarned = recordHit(letterY);
    gameScore += pointsEarned;
    updateShooterScore(gameScore);
    updateShooterCombo(comboCount, getComboMultiplier());
    updateCurrentModeHighScore(gameScore);

    // Progressive mode: track hits for level advancement
    if (shooterSettings.gameMode == SHOOTER_MODE_PROGRESSIVE) {
      progressiveHits++;
      if (progressiveHits >= 10) {
        progressiveHits = 0;
        progressiveLevel++;
        Serial.printf("[Shooter] Progressive level up! Now level %d\n", progressiveLevel);
        beep(1000, 100);  // Level up beep
      }
    }
    return;
  }

  // Decoded character but no matching letter falling - MISS
  beep(600, 100);  // Miss sound
  resetCombo();
  updateShooterCombo(0, 1);
}

/*
 * Read paddle input and decode morse using adaptive decoder
 * Uses unified keyer module for all key types
 */
void updateMorseInputFast() {
  if (!shooterKeyer) return;

  unsigned long now = millis();

  // Get paddle state from centralized handler (includes debounce)
  bool newDitPressed, newDahPressed;
  getPaddleState(&newDitPressed, &newDahPressed);
  // In straight key mode, ignore DAH pin entirely - the TRS ring may be grounded
  if (cwKeyType == KEY_STRAIGHT) {
    newDahPressed = false;
  }

  morseInput.ditPressed = newDitPressed;
  morseInput.dahPressed = newDahPressed;

  // Clear previous decoded text display when starting new input after idle
  bool keyerWasIdle = !shooterKeyer->isTxActive();
  if ((newDitPressed || newDahPressed) && shooterDecodedText[0] != '\0' && keyerWasIdle &&
      shooterLastElementTime == 0) {
    shooterDecodedText[0] = '\0';
    updateShooterDecoded("");
  }

  // Check for decoder timeout (flush trailing character after word gap of silence).
  // Characters are processed as they decode via the messageCallback, so this
  // flush is just a safety net for the adaptive decoder's last character.
  if (shooterLastElementTime > 0 && !newDitPressed && !newDahPressed && !shooterKeyer->isTxActive()) {
    unsigned long timeSinceLastElement = now - shooterLastElementTime;
    float wordGapDuration = MorseWPM::wordGap(shooterDecoder->getWPM());

    if (timeSinceLastElement > wordGapDuration) {
      shooterDecoder->flush();
      shooterLastElementTime = 0;
    }
  }

  // Feed paddle state to unified keyer
  if (newDitPressed != shooterDitPressed) {
    shooterKeyer->key(PADDLE_DIT, newDitPressed);
    shooterDitPressed = newDitPressed;
  }
  if (newDahPressed != shooterDahPressed) {
    shooterKeyer->key(PADDLE_DAH, newDahPressed);
    shooterDahPressed = newDahPressed;
  }

  // Tick the keyer state machine
  shooterKeyer->tick(now);

  // Tick the decoder (Direct mode: proactive character-gap flush)
  shooterDecoder->tick();

  // Keep tone playing if keyer is active (for audio buffer continuity)
  if (shooterKeyer->isTxActive()) {
    continueTone(cwTone);
  }
}

/*
 * Initialize game (called when entering from Games menu)
 */
void startMorseShooter(LGFX& tft) {
  resetGame();

  // Setup decoder callback: display + shoot each character as it decodes
  shooterDecoder->messageCallback = [](String morse, String text) {
    for (unsigned int i = 0; i < text.length(); i++) {
      char c = text[i];
      if (c == ' ' || c == '\n' || c == '\r') continue;
      if (c == '<') {
        // Prosign like <AR>: skip the whole token, it is never a game target
        while (i < text.length() && text[i] != '>') i++;
        continue;
      }
      shooterAppendDecoded(c);
      updateShooterDecoded(shooterDecodedText);
      shooterProcessChar(c);
    }

    Serial.print("Morse Shooter decoded: ");
    Serial.print(text);
    Serial.print(" (");
    Serial.print(morse);
    Serial.println(")");
  };

  // UI is handled by LVGL - see lv_game_screens.h
}

/*
 * Update morse input (called every loop for responsive keying)
 * This is separate from visual updates
 */
void updateMorseShooterInput(LGFX& tft) {
  if (gameOver || gamePaused) {
    return;
  }
  updateMorseInputFast();
}

/*
 * Update game physics/visuals. Runs a smooth dt-based tick, but freezes
 * while the player is actively keying: moving LVGL objects forces display
 * flushes that can crunch live audio, and it gives the player a fair chance
 * to finish the character they started.
 */
void updateMorseShooterVisuals(LGFX& tft) {
  if (gameOver || gamePaused) {
    return;
  }

  unsigned long now = millis();

  bool isKeying = (shooterKeyer && shooterKeyer->isTxActive()) ||
                  morseInput.ditPressed || morseInput.dahPressed;

  if (isKeying) {
    // Keep the clock current so time spent keying doesn't turn into a
    // teleport-sized dt step when the screen unfreezes
    lastGameUpdate = now;
    return;
  }

  unsigned long dtMs = now - lastGameUpdate;
  if (dtMs < GAME_TICK_MS) return;
  if (dtMs > 250) dtMs = 250;  // Clamp hiccups (screen loads, SD access, etc.)
  lastGameUpdate = now;
  float dt = dtMs / 1000.0f;

  // Hide the combo badge once its display window expires
  if (comboDisplayUntil > 0 && now > comboDisplayUntil) {
    comboDisplayUntil = 0;
    updateShooterCombo(0, 1);
  }

  // Update and spawn based on game mode
  if (shooterSettings.gameMode == SHOOTER_MODE_WORD) {
    updateFallingWords(dt);
    if (!gameOver) spawnFallingWord();
  } else if (shooterSettings.gameMode == SHOOTER_MODE_CALLSIGN) {
    updateFallingWords(dt);
    if (!gameOver) spawnFallingCallsign();
  } else {
    // Classic and Progressive modes
    updateFallingLetters(dt);
    if (!gameOver) spawnFallingLetter();
  }
}

/*
 * Cleanup on exit - save settings, stop audio, reset state
 */
void cleanupMorseShooter() {
  // Save current settings and any unpersisted high score
  saveShooterPrefs();
  saveCWSettings();
  persistShooterHighScore();

  stopTone();
  if (shooterKeyer) {
    shooterKeyer->reset();
  }
  if (shooterDecoder) {
    shooterDecoder->reset();
  }
  gameOver = true;
  gamePaused = true;
}

#endif // GAME_MORSE_SHOOTER_H
