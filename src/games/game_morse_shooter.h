/*
 * Morse Shooter Game
 * Classic arcade-style game where players shoot falling letters using morse code
 */

#ifndef GAME_MORSE_SHOOTER_H
#define GAME_MORSE_SHOOTER_H

#include "../core/config.h"
#include "../core/morse_code.h"
#include "../audio/i2s_audio.h"
#include "../audio/morse_decoder_adaptive.h"
#include "../keyer/keyer.h"
#include <Preferences.h>

// ============================================
// Game Constants
// ============================================

#define MAX_FALLING_LETTERS 5
#define GAME_UPDATE_INTERVAL 1000  // ms between game updates (1 second)
// Game ground level for LVGL layout (canvas is 240px tall, starts at y=40)
// Letters hit ground when y >= 200 in game coords (y=240 on screen, above bottom HUD)
#define GAME_GROUND_Y 200
#define MAX_LIVES 3               // Lives (letters that can hit ground)

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
// Difficulty System (Expanded)
// ============================================

enum ShooterDifficulty {
    SHOOTER_EASY = 0,
    SHOOTER_MEDIUM = 1,
    SHOOTER_HARD = 2
};

// Preset difficulty levels (expanded)
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
#define CHARSET_FLAG_PROSIGNS    0x08

// Character sets
static const char CHARSET_BEGINNER[] = "ETIANMS";
static const char CHARSET_LETTERS[] = "ETIANMSURWDKGOHVFLPJBXCYZQ";
static const char CHARSET_NUMBERS[] = "0123456789";
static const char CHARSET_PUNCTUATION[] = ".,?/=-";
static const char CHARSET_PROSIGNS[] = "";  // Prosigns handled specially

// Legacy charsets for compatibility
static const char CHARSET_EASY[] = "ETIANMS";
static const char CHARSET_MEDIUM[] = "ETIANMSURWDKGOHVFLPJBXCYZQ";
static const char CHARSET_HARD[] = "ETIANMSURWDKGOHVFLPJBXCYZQ0123456789";

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
    {5, 5, 3, 5, CHARSET_FLAG_LETTERS, CHARSET_MEDIUM, 26},                              // Custom (defaults)
    {1, 1, 5, 3, CHARSET_FLAG_LETTERS, CHARSET_BEGINNER, 7},                             // Beginner
    {3, 3, 3, 4, CHARSET_FLAG_LETTERS, CHARSET_BEGINNER, 7},                             // Easy
    {5, 5, 3, 5, CHARSET_FLAG_LETTERS, CHARSET_MEDIUM, 26},                              // Medium
    {7, 7, 3, 5, CHARSET_FLAG_LETTERS | CHARSET_FLAG_NUMBERS, CHARSET_HARD, 36},         // Hard
    {8, 8, 2, 6, CHARSET_FLAG_LETTERS | CHARSET_FLAG_NUMBERS | CHARSET_FLAG_PUNCTUATION, CHARSET_HARD, 36}, // Expert
    {10, 10, 1, 8, CHARSET_FLAG_LETTERS | CHARSET_FLAG_NUMBERS | CHARSET_FLAG_PUNCTUATION, CHARSET_HARD, 36} // Insane
};

// Legacy DifficultyParams for backward compatibility
struct DifficultyParams {
    int spawnInterval;      // ms between spawns
    float fallSpeed;        // pixels per update
    const char* charset;    // available characters
    int charsetSize;
    int startLives;
    int scoreMultiplier;
    const char* name;       // display name
};

// Difficulty parameters table (legacy - still used for now)
static const DifficultyParams DIFF_PARAMS[] = {
    { 4000, 0.5f, CHARSET_EASY,   7,  3, 1, "Easy" },
    { 3000, 1.0f, CHARSET_MEDIUM, 26, 3, 2, "Medium" },
    { 2000, 1.5f, CHARSET_HARD,   36, 3, 3, "Hard" }
};

// Mapping functions: convert 1-10 scale to actual values
inline float speedToPixels(uint8_t level) {
    // 1 → 0.3, 10 → 2.5 (linear interpolation)
    return 0.3f + (level - 1) * 0.244f;
}

inline uint32_t spawnToInterval(uint8_t level) {
    // 1 → 5000ms, 10 → 1000ms
    return 5000 - (level - 1) * 444;
}

// Current difficulty setting (legacy)
static ShooterDifficulty shooterDifficulty = SHOOTER_MEDIUM;
static int shooterHighScores[3] = {0, 0, 0};  // Per difficulty (legacy)

// Per-mode high scores
static int shooterHighScoreClassic = 0;
static int shooterHighScoreProgressive = 0;
static int shooterHighScoreWord = 0;
static int shooterHighScoreCallsign = 0;

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
    unsigned long now = millis();

    // Quick hit bonus (hit within 1 second of spawn)
    if (letterY < 50) {  // Still near top of screen
        bonus += 5;
    }

    // Top-third bonus
    if (letterY < 70) {  // Upper portion of play area
        bonus += 3;
    }

    return bonus;
}

// Reset combo on miss
inline void resetCombo() {
    if (comboCount >= 3) {
        // Show "STREAK LOST" feedback (handled by UI)
        Serial.printf("[Shooter] Combo lost! Was at %d\n", comboCount);
    }
    comboCount = 0;
}

// Record a hit and return total points earned
inline int recordHit(float letterY) {
    comboCount++;
    lastHitTime = millis();
    comboDisplayUntil = lastHitTime + 1500;  // Show combo for 1.5 seconds

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
static const char* WORDS_EASY[] = {"CQ", "DE", "HI", "OK", "IT", "IS", "TO", "OF", "73", "88"};
static const int WORDS_EASY_COUNT = 10;

static const char* WORDS_MEDIUM[] = {"CALL", "COPY", "NAME", "QTH", "RST", "BAND", "FREQ", "WIRE", "TEST", "GOOD"};
static const int WORDS_MEDIUM_COUNT = 10;

static const char* WORDS_HARD[] = {"ANTENNA", "WEATHER", "STATION", "AMATEUR", "CONTEST", "REPEATER", "SIGNAL"};
static const int WORDS_HARD_COUNT = 7;

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
inline void generateCallsign(char* buffer, bool includeInternational = false) {
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

    sprintf(buffer, "%s%d%s", prefix, digit, suffix);
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
unsigned long lastSpawnTime = 0;
unsigned long lastGameUpdate = 0;
unsigned long gameStartTime = 0;
bool gameOver = false;
bool gamePaused = false;
// Note: highScore moved to shooterHighScores[] array per difficulty

// Decoder state
MorseDecoderAdaptive shooterDecoder(20, 20, 30);  // Initial 20 WPM, buffer size 30
String shooterDecodedText = "";
unsigned long shooterLastStateChangeTime = 0;
bool shooterLastToneState = false;
unsigned long shooterLastElementTime = 0;  // Track last element for timeout flush

// Unified keyer for all key types
static StraightKeyer* shooterKeyer = nullptr;
static bool shooterDitPressed = false;
static bool shooterDahPressed = false;

// Settings mode state (legacy - will be removed)
bool inShooterSettings = false;
int shooterSettingsSelection = 0;  // 0=Speed, 1=Tone, 2=Key Type, 3=Save & Return

// LVGL mode flag - when true, skip legacy draw functions (LVGL handles display)
bool shooterUseLVGL = true;  // Default to LVGL mode

// Forward declarations for settings
void drawShooterSettings(LGFX& tft);
int handleShooterSettingsInput(char key, LGFX& tft);

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
          shooterDecoder.addTiming(-silenceDuration);
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
        shooterDecoder.addTiming(toneDuration);
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

    // Load legacy difficulty
    shooterDifficulty = (ShooterDifficulty)prefs.getInt("difficulty", SHOOTER_MEDIUM);
    if (shooterDifficulty > SHOOTER_HARD) shooterDifficulty = SHOOTER_MEDIUM;

    // Load legacy high scores
    shooterHighScores[0] = prefs.getInt("hs_easy", 0);
    shooterHighScores[1] = prefs.getInt("hs_medium", 0);
    shooterHighScores[2] = prefs.getInt("hs_hard", 0);

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

    // Load per-mode high scores
    shooterHighScoreClassic = prefs.getInt("hs_classic", 0);
    shooterHighScoreProgressive = prefs.getInt("hs_prog", 0);
    shooterHighScoreWord = prefs.getInt("hs_word", 0);
    shooterHighScoreCallsign = prefs.getInt("hs_call", 0);

    prefs.end();

    Serial.printf("[Shooter] Loaded prefs: mode=%d, preset=%d, speed=%d, spawn=%d, lives=%d\n",
                  shooterSettings.gameMode, shooterSettings.preset,
                  shooterSettings.fallSpeed, shooterSettings.spawnRate, shooterSettings.startLives);
    Serial.printf("[Shooter] High scores: classic=%d, prog=%d, word=%d, call=%d\n",
                  shooterHighScoreClassic, shooterHighScoreProgressive,
                  shooterHighScoreWord, shooterHighScoreCallsign);
}

void saveShooterPrefs() {
    Preferences prefs;
    prefs.begin("shooter", false);  // read-write

    // Save legacy difficulty
    prefs.putInt("difficulty", (int)shooterDifficulty);

    // Save expanded settings
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

    // Save legacy high scores
    const char* keys[] = {"hs_easy", "hs_medium", "hs_hard"};
    prefs.putInt(keys[shooterDifficulty], shooterHighScores[shooterDifficulty]);

    // Save per-mode high scores
    prefs.putInt("hs_classic", shooterHighScoreClassic);
    prefs.putInt("hs_prog", shooterHighScoreProgressive);
    prefs.putInt("hs_word", shooterHighScoreWord);
    prefs.putInt("hs_call", shooterHighScoreCallsign);

    prefs.end();
    Serial.printf("[Shooter] Saved high scores\n");
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

// Get current effective parameters (from settings or preset)
void getEffectiveParams(float& fallSpeed, uint32_t& spawnInterval, int& maxLetters, int& lives, const char*& charset, int& charsetSize) {
    if (shooterSettings.preset != PRESET_CUSTOM) {
        const PresetConfig& config = PRESET_CONFIGS[shooterSettings.preset];
        fallSpeed = speedToPixels(config.fallSpeed);
        spawnInterval = spawnToInterval(config.spawnRate);
        maxLetters = config.maxLetters;
        lives = config.startLives;
        charset = config.charset;
        charsetSize = config.charsetSize;
    } else {
        fallSpeed = speedToPixels(shooterSettings.fallSpeed);
        spawnInterval = spawnToInterval(shooterSettings.spawnRate);
        maxLetters = shooterSettings.maxLetters;
        lives = shooterSettings.startLives;
        // Build charset from flags
        charset = CHARSET_MEDIUM;  // Default
        charsetSize = 26;
        if (shooterSettings.charsetFlags & CHARSET_FLAG_NUMBERS) {
            charset = CHARSET_HARD;
            charsetSize = 36;
        }
    }
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

// Update high score for current mode
void updateCurrentModeHighScore(int score) {
    bool updated = false;
    switch (shooterSettings.gameMode) {
        case SHOOTER_MODE_CLASSIC:
            if (score > shooterHighScoreClassic) {
                shooterHighScoreClassic = score;
                updated = true;
            }
            break;
        case SHOOTER_MODE_PROGRESSIVE:
            if (score > shooterHighScoreProgressive) {
                shooterHighScoreProgressive = score;
                updated = true;
            }
            break;
        case SHOOTER_MODE_WORD:
            if (score > shooterHighScoreWord) {
                shooterHighScoreWord = score;
                updated = true;
            }
            break;
        case SHOOTER_MODE_CALLSIGN:
            if (score > shooterHighScoreCallsign) {
                shooterHighScoreCallsign = score;
                updated = true;
            }
            break;
    }
    if (updated) {
        saveShooterHighScore();
    }
}

// ============================================
// LVGL Display Update Functions (defined in lv_game_screens.h)
// ============================================

extern void updateShooterScore(int score);
extern void updateShooterLives(int lives);
extern void updateShooterDecoded(const char* text);
extern void updateShooterLetter(int index, char letter, int x, int y, bool visible);
extern void updateShooterCombo(int combo, int multiplier);
extern void showShooterHitEffect(int x, int y);
extern void showShooterGameOver();

/*
 * Initialize a falling letter (with collision avoidance)
 */
void initFallingLetter(int index) {
  const DifficultyParams& params = DIFF_PARAMS[shooterDifficulty];
  fallingLetters[index].letter = params.charset[random(params.charsetSize)];

  // Try to find a spawn position that doesn't overlap with existing letters
  int attempts = 0;
  bool positionOk = false;
  int newX;

  // Letters spawn at top of play area (just below HUD at y=40 in LVGL coords)
  // Using game coords: y=5 + offset 40 = 45 on screen (just below HUD)
  const int SPAWN_Y = 5;

  while (!positionOk && attempts < 20) {
    newX = random(20, SCREEN_WIDTH - 40);
    positionOk = true;

    // Check if this position is too close to any active letter
    for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
      if (i != index && fallingLetters[i].active) {
        // Letters are about 20 pixels wide, so check for 30 pixel spacing
        if (abs(newX - (int)fallingLetters[i].x) < 30 &&
            abs(SPAWN_Y - (int)fallingLetters[i].y) < 40) {
          positionOk = false;
          break;
        }
      }
    }
    attempts++;
  }

  fallingLetters[index].x = newX;
  fallingLetters[index].y = SPAWN_Y;  // Start at top of play area
  fallingLetters[index].active = true;

  // Update LVGL display (y+40 for header offset)
  updateShooterLetter(index, fallingLetters[index].letter,
                     (int)fallingLetters[index].x,
                     (int)fallingLetters[index].y + 40, true);
}

/*
 * Reset game state
 */
void resetGame() {
  const DifficultyParams& params = DIFF_PARAMS[shooterDifficulty];

  // Clear all falling letters
  for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
    fallingLetters[i].active = false;
  }

  // Clear falling words (for Word/Callsign modes)
  for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
    fallingWords[i].active = false;
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

  // Reset decoder state
  shooterDecoder.reset();
  shooterDecoder.flush();
  shooterDecoder.setWPM(cwSpeed);
  shooterDecodedText = "";
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

  // Determine lives from settings or legacy difficulty
  int startLives = params.startLives;
  if (shooterSettings.preset != PRESET_CUSTOM) {
    startLives = PRESET_CONFIGS[shooterSettings.preset].startLives;
  } else if (shooterSettings.startLives >= 1 && shooterSettings.startLives <= 5) {
    startLives = shooterSettings.startLives;
  }

  // Reset game variables
  gameScore = 0;
  gameLives = startLives;
  lastSpawnTime = millis();
  lastGameUpdate = millis();
  gameStartTime = millis();
  gameOver = false;
  gamePaused = false;

  // Update LVGL display
  updateShooterScore(0);
  updateShooterLives(gameLives);
  updateShooterDecoded("");
  updateShooterCombo(0, 1);  // Reset combo display
  for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
    updateShooterLetter(i, ' ', 0, 0, false);  // Hide all letters
  }

  Serial.printf("[Shooter] Game reset: mode=%s, preset=%s, lives=%d\n",
                GAME_MODE_NAMES[shooterSettings.gameMode],
                PRESET_NAMES[shooterSettings.preset], gameLives);
}

/*
 * Draw old-school ground scenery
 */
void drawGroundScenery(LGFX& tft) {
  // Skip legacy drawing when using LVGL
  if (shooterUseLVGL) return;

  // Ground line
  tft.drawFastHLine(0, GROUND_Y, SCREEN_WIDTH, ST77XX_GREEN);
  tft.drawFastHLine(0, GROUND_Y + 1, SCREEN_WIDTH, 0x05E0);

  // Houses (simple rectangles with roofs)
  // House 1 (left edge)
  tft.fillRect(5, GROUND_Y - 25, 30, 25, 0x4208);  // Dark gray house
  tft.fillTriangle(5, GROUND_Y - 25, 35, GROUND_Y - 25, 20, GROUND_Y - 35, ST77XX_RED);  // Red roof
  tft.fillRect(13, GROUND_Y - 12, 8, 12, 0x0861);  // Dark window

  // House 2
  tft.fillRect(90, GROUND_Y - 30, 35, 30, 0x52AA);  // Blue-gray house
  tft.fillTriangle(90, GROUND_Y - 30, 125, GROUND_Y - 30, 107, GROUND_Y - 42, 0xC618);  // Orange roof
  tft.fillRect(100, GROUND_Y - 15, 10, 15, 0x2104);  // Dark window

  // House 3
  tft.fillRect(195, GROUND_Y - 28, 32, 28, 0x6B4D);  // Tan house
  tft.fillTriangle(195, GROUND_Y - 28, 227, GROUND_Y - 28, 211, GROUND_Y - 38, 0x7800);  // Brown roof
  tft.fillRect(203, GROUND_Y - 14, 8, 14, 0x18C3);  // Door

  // House 4 (right side)
  tft.fillRect(270, GROUND_Y - 27, 30, 27, 0x39C7);  // Purple-gray house
  tft.fillTriangle(270, GROUND_Y - 27, 300, GROUND_Y - 27, 285, GROUND_Y - 37, 0xF800);  // Red roof
  tft.fillRect(278, GROUND_Y - 13, 8, 13, 0x18C3);  // Window

  // Trees (simple triangles)
  // Tree 1
  tft.fillRect(55, GROUND_Y - 15, 6, 15, 0x4A00);  // Brown trunk
  tft.fillTriangle(52, GROUND_Y - 15, 64, GROUND_Y - 15, 58, GROUND_Y - 28, 0x0400);  // Dark green
  tft.fillTriangle(53, GROUND_Y - 20, 63, GROUND_Y - 20, 58, GROUND_Y - 32, 0x05E0);  // Green

  // Tree 2
  tft.fillRect(165, GROUND_Y - 18, 6, 18, 0x4A00);  // Brown trunk
  tft.fillTriangle(162, GROUND_Y - 18, 174, GROUND_Y - 18, 168, GROUND_Y - 32, 0x0400);  // Dark green
  tft.fillTriangle(163, GROUND_Y - 24, 173, GROUND_Y - 24, 168, GROUND_Y - 36, 0x05E0);  // Green

  // Tree 3 (right side)
  tft.fillRect(245, GROUND_Y - 16, 6, 16, 0x4A00);  // Brown trunk
  tft.fillTriangle(242, GROUND_Y - 16, 254, GROUND_Y - 16, 248, GROUND_Y - 30, 0x0400);  // Dark green
  tft.fillTriangle(243, GROUND_Y - 22, 253, GROUND_Y - 22, 248, GROUND_Y - 34, 0x05E0);  // Green

  // Tree 4 (far right)
  tft.fillRect(310, GROUND_Y - 14, 5, 14, 0x4A00);  // Brown trunk
  tft.fillTriangle(308, GROUND_Y - 14, 318, GROUND_Y - 14, 313, GROUND_Y - 26, 0x0400);  // Dark green
  tft.fillTriangle(309, GROUND_Y - 19, 317, GROUND_Y - 19, 313, GROUND_Y - 30, 0x05E0);  // Green

  // Turret at bottom center (simple tank-like shape)
  tft.fillRect(150, GROUND_Y - 20, 20, 12, 0x7BEF);  // Gray base
  tft.fillRect(157, GROUND_Y - 26, 6, 10, 0x4208);   // Dark gray barrel
  tft.drawCircle(160, GROUND_Y - 14, 3, ST77XX_CYAN); // Turret circle accent
}

/*
 * Draw falling letters (with background clearing for current position)
 */
void drawFallingLetters(LGFX& tft, bool clearOld = false) {
  // Skip legacy drawing when using LVGL
  if (shooterUseLVGL) return;

  static int lastY[MAX_FALLING_LETTERS] = {0};

  tft.setTextSize(3);
  for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
    if (fallingLetters[i].active) {
      // Clear old position if requested (but only if it's below the header!)
      if (clearOld && lastY[i] != (int)fallingLetters[i].y && lastY[i] > 42) {
        tft.fillRect((int)fallingLetters[i].x - 2, lastY[i] - 2, 24, 28, COLOR_BACKGROUND);
      }

      // Draw at new position (only if below header)
      if (fallingLetters[i].y > 42) {
        tft.setTextColor(ST77XX_YELLOW, COLOR_BACKGROUND);
        tft.setCursor((int)fallingLetters[i].x, (int)fallingLetters[i].y);
        tft.print(fallingLetters[i].letter);
        lastY[i] = (int)fallingLetters[i].y;
      }
    } else if (clearOld && lastY[i] > 42) {
      // Clear if letter was just deactivated (but only if below header)
      tft.fillRect((int)fallingLetters[i].x - 2, lastY[i] - 2, 24, 28, COLOR_BACKGROUND);
      lastY[i] = 0;
    }
  }
}

/*
 * Draw turret laser when shooting
 */
void drawLaserShot(LGFX& tft, int targetX, int targetY) {
  // Skip legacy drawing when using LVGL
  if (shooterUseLVGL) return;

  // Draw laser from turret to target
  tft.drawLine(160, GROUND_Y - 26, targetX + 10, targetY + 10, ST77XX_CYAN);
  tft.drawLine(159, GROUND_Y - 26, targetX + 10, targetY + 10, ST77XX_WHITE);
  tft.drawLine(161, GROUND_Y - 26, targetX + 10, targetY + 10, ST77XX_WHITE);
}

/*
 * Draw explosion effect
 */
void drawExplosion(LGFX& tft, int x, int y) {
  // Skip legacy drawing when using LVGL
  if (shooterUseLVGL) return;

  // Simple star burst explosion
  tft.drawCircle(x + 10, y + 10, 8, ST77XX_YELLOW);
  tft.drawCircle(x + 10, y + 10, 6, ST77XX_RED);
  tft.drawCircle(x + 10, y + 10, 4, ST77XX_WHITE);
  // Rays
  for (int i = 0; i < 8; i++) {
    float angle = i * 3.14159 / 4;
    int x2 = x + 10 + (int)(12 * cos(angle));
    int y2 = y + 10 + (int)(12 * sin(angle));
    tft.drawLine(x + 10, y + 10, x2, y2, ST77XX_YELLOW);
  }
}

/*
 * Draw HUD (score, lives, morse input)
 */
void drawHUD(LGFX& tft) {
  // Skip legacy drawing when using LVGL
  if (shooterUseLVGL) return;

  // Score (top left corner)
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, COLOR_BACKGROUND);
  tft.setCursor(10, 50);
  tft.print("Score:");
  tft.setCursor(50, 50);
  tft.print(gameScore);

  // Lives (top left, second line)
  tft.setCursor(10, 62);
  tft.setTextColor(gameLives <= 2 ? ST77XX_RED : ST77XX_GREEN, COLOR_BACKGROUND);
  tft.print("Lives:");
  tft.setCursor(50, 62);
  tft.print(gameLives);

  // Decoded text display (bottom, above ground)
  if (shooterDecodedText.length() > 0) {
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, COLOR_BACKGROUND);
    tft.setCursor(10, GROUND_Y + 10);
    tft.print(shooterDecodedText);
    tft.print("   ");  // Clear extra space
  } else {
    // Clear decoded text area when empty
    tft.fillRect(10, GROUND_Y + 10, 100, 20, COLOR_BACKGROUND);
  }
}

// Get current fall speed based on settings/mode
float getCurrentFallSpeed() {
  if (shooterSettings.gameMode == SHOOTER_MODE_PROGRESSIVE) {
    // Progressive mode: speed increases with level
    float baseSpeed = 0.3f;
    float speedIncrease = 0.2f * (progressiveLevel - 1);
    return min(baseSpeed + speedIncrease, 3.0f);  // Cap at 3.0
  } else if (shooterSettings.preset != PRESET_CUSTOM) {
    return speedToPixels(PRESET_CONFIGS[shooterSettings.preset].fallSpeed);
  } else {
    return speedToPixels(shooterSettings.fallSpeed);
  }
}

// Get current spawn interval based on settings/mode
uint32_t getCurrentSpawnInterval() {
  if (shooterSettings.gameMode == SHOOTER_MODE_PROGRESSIVE) {
    // Progressive mode: spawn rate increases with level
    uint32_t baseInterval = 5000;
    uint32_t decrease = 300 * (progressiveLevel - 1);
    return max(baseInterval - decrease, (uint32_t)1000);  // Min 1000ms
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
    if (shooterSettings.charsetFlags & CHARSET_FLAG_NUMBERS) {
      charset = CHARSET_HARD;
      size = 36;
    } else {
      charset = CHARSET_MEDIUM;
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
 * Update falling letters (physics) - supports all modes
 */
void updateFallingLetters() {
  float fallSpeed = getCurrentFallSpeed();

  for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
    if (fallingLetters[i].active) {
      fallingLetters[i].y += fallSpeed;

      // Update LVGL display (y+40 for header offset)
      updateShooterLetter(i, fallingLetters[i].letter,
                         (int)fallingLetters[i].x,
                         (int)fallingLetters[i].y + 40, true);

      // Check if letter hit the ground (GAME_GROUND_Y = 200 for LVGL layout)
      if (fallingLetters[i].y >= GAME_GROUND_Y) {
        fallingLetters[i].active = false;
        updateShooterLetter(i, ' ', 0, 0, false);  // Hide letter
        gameLives--;
        resetCombo();  // Reset combo on ground hit
        updateShooterLives(gameLives);  // Update LVGL
        updateShooterCombo(0, 1);
        beep(TONE_ERROR, 200);  // Hit ground sound

        if (gameLives <= 0) {
          gameOver = true;
          // Track survival time for progressive mode
          if (shooterSettings.gameMode == SHOOTER_MODE_PROGRESSIVE) {
            progressiveTimeSurvived = millis() - gameStartTime;
            Serial.printf("[Shooter] Progressive game over: Level %d, Time %lu ms\n",
                          progressiveLevel, progressiveTimeSurvived);
          }
          showShooterGameOver();  // Show game over overlay
        }
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
 * Spawn new falling letter - supports all modes
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
    newX = random(20, SCREEN_WIDTH - 40);
    positionOk = true;

    for (int i = 0; i < MAX_FALLING_LETTERS; i++) {
      if (i != emptySlot && fallingLetters[i].active) {
        if (abs(newX - (int)fallingLetters[i].x) < 30 &&
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
 * Check decoded text and try to shoot matching letter
 */
bool checkMorseShoot(LGFX& tft) {
  if (shooterDecodedText.length() == 0) {
    return false;
  }

  // Get the last decoded character
  char decodedChar = shooterDecodedText[shooterDecodedText.length() - 1];

  // Convert to uppercase if needed
  if (decodedChar >= 'a' && decodedChar <= 'z') {
    decodedChar = decodedChar - 'a' + 'A';
  }

  // Find matching falling letter
  for (int j = 0; j < MAX_FALLING_LETTERS; j++) {
    if (fallingLetters[j].active && fallingLetters[j].letter == decodedChar) {
      // HIT!
      int targetX = (int)fallingLetters[j].x;
      int targetY = (int)fallingLetters[j].y;
      float letterY = fallingLetters[j].y;  // Save for speed bonus calc

      // Remove letter FIRST (before any redraw)
      fallingLetters[j].active = false;
      updateShooterLetter(j, ' ', 0, 0, false);  // Hide letter in LVGL

      // Play hit sounds (no delays for smoother LVGL updates)
      beep(1200, 50);  // Laser sound

      // Show hit effect in LVGL (y+40 for header offset)
      showShooterHitEffect(targetX, targetY + 40);

      // Legacy drawing (will no-op when using LVGL)
      drawLaserShot(tft, targetX, targetY);
      drawExplosion(tft, targetX, targetY);
      drawGroundScenery(tft);
      drawFallingLetters(tft);

      // Calculate score using combo system
      int pointsEarned = recordHit(letterY);

      // Also apply legacy difficulty multiplier for backward compatibility
      const DifficultyParams& params = DIFF_PARAMS[shooterDifficulty];
      pointsEarned = pointsEarned * params.scoreMultiplier;

      gameScore += pointsEarned;
      updateShooterScore(gameScore);  // Update LVGL

      // Update combo display
      int currentMultiplier = getComboMultiplier();
      updateShooterCombo(comboCount, currentMultiplier);

      // Update high score for current difficulty (legacy)
      if (gameScore > shooterHighScores[shooterDifficulty]) {
        shooterHighScores[shooterDifficulty] = gameScore;
        saveShooterHighScore();
      }

      // Update high score for current mode
      updateCurrentModeHighScore(gameScore);

      // Progressive mode: track hits for level advancement
      if (shooterSettings.gameMode == SHOOTER_MODE_PROGRESSIVE) {
        progressiveHits++;
        if (progressiveHits >= 10) {
          progressiveHits = 0;
          progressiveLevel++;
          Serial.printf("[Shooter] Progressive level up! Now level %d\n", progressiveLevel);
          // TODO: Update UI to show level change
        }
      }

      // Keep decoded text visible until next input starts
      // (cleared in updateMorseInputFast when paddle is pressed)
      return true;
    }
  }

  // Decoded character but no matching letter falling - MISS
  beep(600, 100);  // Miss sound
  resetCombo();    // Reset combo streak
  updateShooterCombo(0, 1);  // Clear combo display
  shooterDecodedText = "";
  return false;
}

/*
 * Read paddle input and decode morse using adaptive decoder
 * Uses unified keyer module for all key types
 */
void updateMorseInputFast(LGFX& tft) {
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

  // Clear previous hit text when starting new input
  bool keyerWasIdle = !shooterKeyer->isTxActive();
  if ((newDitPressed || newDahPressed) && shooterDecodedText.length() > 0 && keyerWasIdle) {
    shooterDecodedText = "";
    updateShooterDecoded("");
  }

  // Check for decoder timeout (flush if no activity for word gap duration)
  if (shooterLastElementTime > 0 && !newDitPressed && !newDahPressed && !shooterKeyer->isTxActive()) {
    unsigned long timeSinceLastElement = now - shooterLastElementTime;
    float wordGapDuration = MorseWPM::wordGap(shooterDecoder.getWPM());

    if (timeSinceLastElement > wordGapDuration) {
      shooterDecoder.flush();
      shooterLastElementTime = 0;

      if (shooterDecodedText.length() > 0) {
        checkMorseShoot(tft);
      }
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

  // Keep tone playing if keyer is active (for audio buffer continuity)
  if (shooterKeyer->isTxActive()) {
    continueTone(cwTone);
  }
}

/*
 * Draw shooter settings screen
 */
void drawShooterSettings(LGFX& tft) {
  if (shooterUseLVGL) return;  // LVGL handles display
  tft.fillRect(0, 42, SCREEN_WIDTH, SCREEN_HEIGHT - 42, COLOR_BACKGROUND);

  // Title
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(80, 50);
  tft.print("SETTINGS");

  // Settings menu items
  int yPos = 75;
  int spacing = 32;

  // Option 0: Speed
  tft.setTextSize(2);
  if (shooterSettingsSelection == 0) {
    tft.setTextColor(ST77XX_BLACK, ST77XX_CYAN);  // Highlighted
  } else {
    tft.setTextColor(ST77XX_WHITE, COLOR_BACKGROUND);
  }
  tft.setCursor(20, yPos);
  tft.print("Speed: ");
  tft.print(cwSpeed);
  tft.print(" WPM   ");

  // Option 1: Tone
  yPos += spacing;
  if (shooterSettingsSelection == 1) {
    tft.setTextColor(ST77XX_BLACK, ST77XX_CYAN);
  } else {
    tft.setTextColor(ST77XX_WHITE, COLOR_BACKGROUND);
  }
  tft.setCursor(20, yPos);
  tft.print("Tone: ");
  tft.print(cwTone);
  tft.print(" Hz   ");

  // Option 2: Key Type
  yPos += spacing;
  if (shooterSettingsSelection == 2) {
    tft.setTextColor(ST77XX_BLACK, ST77XX_CYAN);
  } else {
    tft.setTextColor(ST77XX_WHITE, COLOR_BACKGROUND);
  }
  tft.setCursor(20, yPos);
  tft.print("Key: ");
  if (cwKeyType == KEY_STRAIGHT) {
    tft.print("Straight  ");
  } else if (cwKeyType == KEY_IAMBIC_A) {
    tft.print("Iambic A  ");
  } else {
    tft.print("Iambic B  ");
  }

  // Option 3: Save & Return
  yPos += spacing + 5;
  if (shooterSettingsSelection == 3) {
    tft.setTextColor(ST77XX_BLACK, ST77XX_GREEN);
  } else {
    tft.setTextColor(ST77XX_GREEN, COLOR_BACKGROUND);
  }
  tft.setCursor(50, yPos);
  tft.print("SAVE & PLAY");

  // Instructions (moved higher to avoid overlap)
  tft.setTextSize(1);
  tft.setTextColor(COLOR_WARNING, COLOR_BACKGROUND);
  tft.setCursor(20, 195);
  tft.print("\x18\x19:Select  \x1B\x1A:Change  ENTER:OK");
}

/*
 * Handle shooter settings input
 */
int handleShooterSettingsInput(char key, LGFX& tft) {
  if (key == KEY_UP) {
    shooterSettingsSelection--;
    if (shooterSettingsSelection < 0) shooterSettingsSelection = 3;
    drawShooterSettings(tft);
    beep(TONE_MENU_NAV, BEEP_SHORT);
    return 1;
  }
  else if (key == KEY_DOWN) {
    shooterSettingsSelection++;
    if (shooterSettingsSelection > 3) shooterSettingsSelection = 0;
    drawShooterSettings(tft);
    beep(TONE_MENU_NAV, BEEP_SHORT);
    return 1;
  }
  else if (key == KEY_LEFT) {
    if (shooterSettingsSelection == 0) {
      // Decrease speed
      if (cwSpeed > WPM_MIN) {
        cwSpeed--;
        drawShooterSettings(tft);
        beep(TONE_MENU_NAV, BEEP_SHORT);
      }
    }
    else if (shooterSettingsSelection == 1) {
      // Decrease tone
      if (cwTone > 400) {
        cwTone -= 50;
        drawShooterSettings(tft);
        beep(TONE_MENU_NAV, BEEP_SHORT);
      }
    }
    else if (shooterSettingsSelection == 2) {
      // Cycle key type backward
      if (cwKeyType == KEY_IAMBIC_B) {
        cwKeyType = KEY_IAMBIC_A;
      } else if (cwKeyType == KEY_IAMBIC_A) {
        cwKeyType = KEY_STRAIGHT;
      } else {
        cwKeyType = KEY_IAMBIC_B;
      }
      drawShooterSettings(tft);
      beep(TONE_MENU_NAV, BEEP_SHORT);
    }
    return 1;
  }
  else if (key == KEY_RIGHT) {
    if (shooterSettingsSelection == 0) {
      // Increase speed
      if (cwSpeed < WPM_MAX) {
        cwSpeed++;
        drawShooterSettings(tft);
        beep(TONE_MENU_NAV, BEEP_SHORT);
      }
    }
    else if (shooterSettingsSelection == 1) {
      // Increase tone
      if (cwTone < 1200) {
        cwTone += 50;
        drawShooterSettings(tft);
        beep(TONE_MENU_NAV, BEEP_SHORT);
      }
    }
    else if (shooterSettingsSelection == 2) {
      // Cycle key type forward
      if (cwKeyType == KEY_STRAIGHT) {
        cwKeyType = KEY_IAMBIC_A;
      } else if (cwKeyType == KEY_IAMBIC_A) {
        cwKeyType = KEY_IAMBIC_B;
      } else {
        cwKeyType = KEY_STRAIGHT;
      }
      drawShooterSettings(tft);
      beep(TONE_MENU_NAV, BEEP_SHORT);
    }
    return 1;
  }
  else if (key == KEY_ENTER || key == KEY_ENTER_ALT) {
    if (shooterSettingsSelection == 3) {
      // Save & Return
      saveCWSettings();  // Save to preferences
      inShooterSettings = false;
      gamePaused = false;  // Unpause the game
      resetGame();  // Start new game
      drawMorseShooterUI(tft);
      beep(TONE_SELECT, BEEP_MEDIUM);
      return 2;  // Full redraw
    }
  }
  else if (key == KEY_ESC) {
    // Cancel without saving
    loadCWSettings();  // Reload original settings
    inShooterSettings = false;
    gamePaused = false;  // Unpause the game
    drawMorseShooterUI(tft);
    beep(TONE_MENU_NAV, BEEP_SHORT);
    return 2;
  }

  return 0;
}

/*
 * Draw game over screen
 */
void drawGameOver(LGFX& tft) {
  if (shooterUseLVGL) return;  // LVGL handles display
  tft.fillRect(0, 42, SCREEN_WIDTH, SCREEN_HEIGHT - 42, COLOR_BACKGROUND);

  // Game Over text
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_RED);
  tft.setCursor(50, 80);
  tft.print("GAME OVER");

  // Final score
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(80, 120);
  tft.print("Score: ");
  tft.print(gameScore);

  // High score
  tft.setCursor(70, 145);
  tft.setTextColor(ST77XX_YELLOW);
  tft.print("Best: ");
  tft.print(shooterHighScores[shooterDifficulty]);

  // Instructions
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(50, 180);
  tft.print("ENTER Play Again");
  tft.setCursor(80, 195);
  tft.print("ESC Exit");
}

/*
 * Initialize game (called when entering from Games menu)
 */
void startMorseShooter(LGFX& tft) {
  resetGame();

  // Setup decoder callback to capture decoded text
  shooterDecoder.messageCallback = [](String morse, String text) {
    // Append decoded characters
    for (int i = 0; i < text.length(); i++) {
      shooterDecodedText += text[i];
    }

    // Update LVGL display
    updateShooterDecoded(shooterDecodedText.c_str());

    Serial.print("Morse Shooter decoded: ");
    Serial.print(text);
    Serial.print(" (");
    Serial.print(morse);
    Serial.println(")");
  };

  // UI is now handled by LVGL - see lv_game_screens.h
}

/*
 * Draw main game UI
 */
void drawMorseShooterUI(LGFX& tft) {
  // Skip legacy drawing when using LVGL
  if (shooterUseLVGL) return;

  // Clear screen
  tft.fillScreen(COLOR_BACKGROUND);

  // Draw header
  drawHeader();

  // If in settings mode, show settings
  if (inShooterSettings) {
    drawShooterSettings(tft);
    return;
  }

  if (gameOver) {
    drawGameOver(tft);
    return;
  }

  // Draw game elements
  drawGroundScenery(tft);
  drawFallingLetters(tft);
  drawHUD(tft);
}

/*
 * Update morse input (called every loop for responsive keying)
 * This is separate from visual updates
 */
void updateMorseShooterInput(LGFX& tft) {
  if (gameOver || gamePaused) {
    return;
  }
  updateMorseInputFast(tft);
}

/*
 * Update game visuals (called once per second to avoid screen tearing)
 * This is separate from input polling
 * Screen is FROZEN while any paddle is held or pattern is being entered
 */
void updateMorseShooterVisuals(LGFX& tft) {
  if (gameOver || gamePaused) {
    return;
  }

  // FREEZE screen only during active keying (not when decoded text exists)
  // This allows physics/ground collision to continue while hit text is displayed
  bool isKeying = (shooterKeyer && shooterKeyer->isTxActive()) ||
                  morseInput.ditPressed || morseInput.dahPressed;

  if (isKeying) {
    return;
  }

  unsigned long now = millis();

  // Only update game physics and visuals once per second
  if (now - lastGameUpdate >= GAME_UPDATE_INTERVAL) {
    lastGameUpdate = now;

    // Update game logic
    updateFallingLetters();
    spawnFallingLetter();

    // Redraw only changed elements (no full screen clear)
    drawFallingLetters(tft, true);  // Clear old positions, draw new
    drawHUD(tft);
  }
}

/*
 * Handle keyboard input for game
 * Returns: -1 to exit game, 0 for normal input, 2 for full redraw
 */
int handleMorseShooterInput(char key, LGFX& tft) {
  // If in settings mode, route to settings handler
  if (inShooterSettings) {
    return handleShooterSettingsInput(key, tft);
  }

  if (key == KEY_ESC) {
    return -1;  // Exit to games menu
  }

  if (gameOver) {
    if (key == KEY_ENTER || key == KEY_ENTER_ALT) {
      // Restart game
      resetGame();
      return 2;  // Full redraw
    }
    return 0;
  }

  // Settings with 'S' key
  if (key == 's' || key == 'S') {
    inShooterSettings = true;
    gamePaused = true;  // Pause the game
    shooterSettingsSelection = 0;
    drawShooterSettings(tft);
    beep(TONE_SELECT, BEEP_MEDIUM);
    return 2;  // Full redraw
  }

  // Pause/unpause with SPACE
  if (key == ' ') {
    gamePaused = !gamePaused;
    if (gamePaused) {
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_YELLOW, COLOR_BACKGROUND);
      tft.setCursor(110, 100);
      tft.print("PAUSED");
    }
    return 2;  // Redraw
  }

  return 0;
}

#endif // GAME_MORSE_SHOOTER_H
