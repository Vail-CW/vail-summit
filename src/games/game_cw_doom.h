/*
 * CW DOOM - Morse Code DOOM
 * A simplified DOOM-style raycaster controlled via CW paddles
 */

#ifndef GAME_CW_DOOM_H
#define GAME_CW_DOOM_H

#include "../core/config.h"
#include "../keyer/keyer.h"
#include "../audio/i2s_audio.h"
#include "../audio/morse_decoder_adaptive.h"
#include <Preferences.h>

// ============================================
// Rendering Constants
// ============================================

#define DOOM_RENDER_WIDTH 120
#define DOOM_RENDER_HEIGHT 80
#define DOOM_SCALE 4
#define DOOM_SCREEN_WIDTH (DOOM_RENDER_WIDTH * DOOM_SCALE)
#define DOOM_SCREEN_HEIGHT (DOOM_RENDER_HEIGHT * DOOM_SCALE)

#define DOOM_FOV 60
#define DOOM_HALF_FOV 30

// ============================================
// Map Constants
// ============================================

#define DOOM_MAP_WIDTH 16
#define DOOM_MAP_HEIGHT 16

#define CELL_EMPTY 0
#define CELL_WALL 1
#define CELL_WALL_RED 2
#define CELL_WALL_BLUE 3
#define CELL_DOOR 4
#define CELL_EXIT 5
#define CELL_ENEMY_SPAWN 6
#define CELL_EXIT_LOCKED 7    // Rendered as red - must kill all enemies
#define CELL_EXIT_UNLOCKED 8  // Rendered as green - can pass through

// ============================================
// Fixed-Point Math (16.16 format)
// ============================================

#define FP_SHIFT 16
#define FP_ONE (1 << FP_SHIFT)
#define FP_HALF (FP_ONE >> 1)
#define INT_TO_FP(x) ((int32_t)(x) << FP_SHIFT)
#define FP_TO_INT(x) ((int32_t)(x) >> FP_SHIFT)
#define FLOAT_TO_FP(x) ((int32_t)((x) * FP_ONE))
#define FP_TO_FLOAT(x) ((float)(x) / FP_ONE)
#define FP_MUL(a, b) ((int32_t)(((int64_t)(a) * (b)) >> FP_SHIFT))
#define FP_DIV(a, b) ((int32_t)(((int64_t)(a) << FP_SHIFT) / (b)))

// ============================================
// Game Constants
// ============================================

#define DOOM_MAX_ENEMIES 8
#define DOOM_MAX_PROJECTILES 4
#define DOOM_PLAYER_SPEED FLOAT_TO_FP(0.08f)
#define DOOM_PLAYER_ROT_SPEED 4
#define DOOM_PROJECTILE_SPEED FLOAT_TO_FP(0.2f)

#define DOOM_TAP_THRESHOLD_MS 150

// ============================================
// Lookup Tables
// ============================================

static int32_t doom_sin_table[360];
static int32_t doom_cos_table[360];
static bool doom_trig_initialized = false;

static void initDoomTrigTables() {
    if (doom_trig_initialized) return;
    for (int i = 0; i < 360; i++) {
        doom_sin_table[i] = FLOAT_TO_FP(sin(i * PI / 180.0f));
        doom_cos_table[i] = FLOAT_TO_FP(cos(i * PI / 180.0f));
    }
    doom_trig_initialized = true;
}

static inline int32_t doomSin(int angle) {
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;
    return doom_sin_table[angle];
}

static inline int32_t doomCos(int angle) {
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;
    return doom_cos_table[angle];
}

// ============================================
// Enums
// ============================================

enum DoomGameState {
    DOOM_STATE_MENU,
    DOOM_STATE_PLAYING,
    DOOM_STATE_PAUSED,
    DOOM_STATE_GAME_OVER,
    DOOM_STATE_VICTORY
};

// Control mode removed - now uses live keying with "type to shoot" mechanic
// Keep enum for backwards compatibility with settings
enum DoomControlMode {
    DOOM_CTRL_LIVE = 0,
    DOOM_CTRL_LETTER = 1  // Deprecated, not used
};

enum DoomDifficulty {
    DOOM_EASY = 0,
    DOOM_MEDIUM = 1,
    DOOM_HARD = 2
};

// ============================================
// Structures
// ============================================

struct DoomPlayer {
    int32_t x;
    int32_t y;
    int angle;
    int health;
    int ammo;
    int score;
    int kills;
};

struct DoomEnemy {
    int32_t x;
    int32_t y;
    int health;
    int type;
    bool active;
    int hitTimer;
};

struct DoomProjectile {
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    bool active;
    bool isPlayer;
};

struct DoomRayHit {
    int32_t distance;
    int wallType;
    bool isVertical;
    int32_t hitX;
    int32_t hitY;
};

// ============================================
// Game State
// ============================================

struct DoomGame {
    DoomGameState state;
    DoomControlMode controlMode;  // Kept for settings compatibility but not used
    DoomDifficulty difficulty;

    DoomPlayer player;
    DoomEnemy enemies[DOOM_MAX_ENEMIES];
    int enemyCount;
    DoomProjectile projectiles[DOOM_MAX_PROJECTILES];

    uint8_t map[DOOM_MAP_HEIGHT][DOOM_MAP_WIDTH];
    int currentLevel;
    int totalEnemies;

    unsigned long lastFrameTime;
    unsigned long frameCount;
    bool needsRender;

    bool ditPressed;
    bool dahPressed;
    bool lastDitPressed;
    bool lastDahPressed;
    unsigned long ditPressTime;
    unsigned long dahPressTime;
    unsigned long ditReleaseTime;
    unsigned long dahReleaseTime;

    char lastDecodedChar;
    bool hasDecodedChar;

    // Type-to-shoot mechanic
    char targetChar;           // Character to type to shoot (0 if no enemy in view)
    int targetEnemyIndex;      // Which enemy is being targeted (-1 if none)
    bool enemyInView;          // Is there an enemy in the crosshairs?

    int highScores[3];
};

static DoomGame doomGame;
static bool doomActive = false;
static bool doomUseLVGL = true;

// Keyer integration
static StraightKeyer* doomKeyer = nullptr;
static MorseDecoderAdaptive doomDecoder(20, 20, 30);
static unsigned long doomLastToneTime = 0;
static bool doomLastToneState = false;

// ============================================
// Level Data (PROGMEM)
// ============================================

static const uint8_t DOOM_LEVEL_1[DOOM_MAP_HEIGHT][DOOM_MAP_WIDTH] PROGMEM = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,2,2,0,0,0,0,3,3,3,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,0,3,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,0,3,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,6,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,3,0,0,0,0,0,0,0,0,2,0,0,1},
    {1,0,0,3,0,0,0,0,0,0,0,0,2,0,0,1},
    {1,0,0,3,3,3,0,0,0,0,2,2,2,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,5,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

static const uint8_t DOOM_LEVEL_2[DOOM_MAP_HEIGHT][DOOM_MAP_WIDTH] PROGMEM = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1},
    {1,0,6,0,1,0,0,0,0,0,0,1,0,6,0,1},
    {1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1},
    {1,1,4,1,1,0,0,2,2,0,0,1,1,4,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,0,0,0,6,0,0,0,2,0,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,2,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,4,1,1,0,0,3,3,0,0,1,1,4,1,1},
    {1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1},
    {1,0,0,0,1,0,0,5,0,0,0,1,0,0,0,1},
    {1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

static const uint8_t DOOM_LEVEL_3[DOOM_MAP_HEIGHT][DOOM_MAP_WIDTH] PROGMEM = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1},
    {1,0,6,0,6,0,1,0,0,1,0,6,0,6,0,1},
    {1,0,0,0,0,0,4,0,0,4,0,0,0,0,0,1},
    {1,0,0,2,2,2,1,0,0,1,2,2,2,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,0,2,0,0,1},
    {1,1,4,1,0,0,0,6,0,0,0,0,1,4,1,1},
    {1,0,0,0,0,0,3,0,0,3,0,0,0,0,0,1},
    {1,0,0,0,0,0,3,0,0,3,0,0,0,0,0,1},
    {1,1,4,1,0,0,0,0,0,0,0,0,1,4,1,1},
    {1,0,0,2,0,0,0,0,0,0,0,0,2,0,0,1},
    {1,0,0,2,2,2,1,0,0,1,2,2,2,0,0,1},
    {1,0,0,0,0,0,4,0,0,4,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,0,5,1,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// ============================================
// Initialization
// ============================================

static void loadDoomLevel(int level) {
    const uint8_t* levelData;
    switch (level) {
        case 1: levelData = &DOOM_LEVEL_1[0][0]; break;
        case 2: levelData = &DOOM_LEVEL_2[0][0]; break;
        case 3: levelData = &DOOM_LEVEL_3[0][0]; break;
        default: levelData = &DOOM_LEVEL_1[0][0]; break;
    }

    doomGame.enemyCount = 0;
    doomGame.totalEnemies = 0;

    for (int y = 0; y < DOOM_MAP_HEIGHT; y++) {
        for (int x = 0; x < DOOM_MAP_WIDTH; x++) {
            uint8_t cell = pgm_read_byte(levelData + y * DOOM_MAP_WIDTH + x);
            doomGame.map[y][x] = cell;

            if (cell == CELL_ENEMY_SPAWN && doomGame.enemyCount < DOOM_MAX_ENEMIES) {
                DoomEnemy& enemy = doomGame.enemies[doomGame.enemyCount];
                enemy.x = INT_TO_FP(x) + FP_HALF;
                enemy.y = INT_TO_FP(y) + FP_HALF;
                enemy.health = (doomGame.difficulty == DOOM_HARD) ? 2 : 1;
                enemy.type = 0;
                enemy.active = true;
                enemy.hitTimer = 0;
                doomGame.map[y][x] = CELL_EMPTY;
                doomGame.enemyCount++;
                doomGame.totalEnemies++;
            }
        }
    }
}

static void initDoomGame(int level, DoomDifficulty difficulty) {
    initDoomTrigTables();

    doomGame.state = DOOM_STATE_PLAYING;
    doomGame.difficulty = difficulty;
    doomGame.currentLevel = level;

    doomGame.player.x = INT_TO_FP(2) + FP_HALF;
    doomGame.player.y = INT_TO_FP(2) + FP_HALF;
    doomGame.player.angle = 0;
    doomGame.player.health = 100;
    doomGame.player.ammo = (difficulty == DOOM_EASY) ? 50 : 30;
    doomGame.player.score = 0;
    doomGame.player.kills = 0;

    for (int i = 0; i < DOOM_MAX_PROJECTILES; i++) {
        doomGame.projectiles[i].active = false;
    }

    loadDoomLevel(level);

    doomGame.lastFrameTime = millis();
    doomGame.frameCount = 0;
    doomGame.needsRender = true;

    doomGame.ditPressed = false;
    doomGame.dahPressed = false;
    doomGame.lastDitPressed = false;
    doomGame.lastDahPressed = false;
    doomGame.ditPressTime = 0;
    doomGame.dahPressTime = 0;
    doomGame.ditReleaseTime = 0;
    doomGame.dahReleaseTime = 0;
    doomGame.lastDecodedChar = 0;
    doomGame.hasDecodedChar = false;

    // Type-to-shoot initialization
    doomGame.targetChar = 0;
    doomGame.targetEnemyIndex = -1;
    doomGame.enemyInView = false;

    doomDecoder.reset();
    doomActive = true;
}

// ============================================
// Keyer Callback
// ============================================

static void doomKeyerCallback(bool txOn, int element) {
    unsigned long now = millis();

    if (txOn) {
        if (!doomLastToneState) {
            if (doomLastToneTime > 0) {
                float silenceDuration = now - doomLastToneTime;
                if (silenceDuration > 0) {
                    doomDecoder.addTiming(-silenceDuration);
                }
            }
            doomLastToneTime = now;
            doomLastToneState = true;
        }
        startTone(cwTone);
    } else {
        if (doomLastToneState) {
            float toneDuration = now - doomLastToneTime;
            if (toneDuration > 0) {
                doomDecoder.addTiming(toneDuration);
            }
            doomLastToneTime = now;
            doomLastToneState = false;
        }
        stopTone();
    }
}

// Decoder callback - receives decoded morse characters
static void doomDecoderCallback(String morse, String text) {
    if (text.length() > 0) {
        char c = toupper(text[0]);
        doomGame.lastDecodedChar = c;
        doomGame.hasDecodedChar = true;
    }
}

static void initDoomKeyer() {
    doomKeyer = getKeyer(cwKeyType);
    doomKeyer->reset();
    doomKeyer->setDitDuration(DIT_DURATION(cwSpeed));
    doomKeyer->setTxCallback(doomKeyerCallback);
    doomDecoder.messageCallback = doomDecoderCallback;
    doomDecoder.setWPM(cwSpeed);
    doomLastToneTime = 0;
    doomLastToneState = false;
}

// ============================================
// Collision Detection
// ============================================

static bool doomIsWall(int32_t x, int32_t y) {
    int mapX = FP_TO_INT(x);
    int mapY = FP_TO_INT(y);

    if (mapX < 0 || mapX >= DOOM_MAP_WIDTH || mapY < 0 || mapY >= DOOM_MAP_HEIGHT) {
        return true;
    }

    uint8_t cell = doomGame.map[mapY][mapX];
    return (cell == CELL_WALL || cell == CELL_WALL_RED || cell == CELL_WALL_BLUE || cell == CELL_DOOR);
}

// Count remaining active enemies
static int doomEnemiesRemaining() {
    int count = 0;
    for (int i = 0; i < doomGame.enemyCount; i++) {
        if (doomGame.enemies[i].active) count++;
    }
    return count;
}

// Check if exit is unlocked (all enemies killed)
static bool doomIsExitUnlocked() {
    return doomEnemiesRemaining() == 0;
}

static bool doomCheckExit(int32_t x, int32_t y) {
    int mapX = FP_TO_INT(x);
    int mapY = FP_TO_INT(y);

    if (mapX < 0 || mapX >= DOOM_MAP_WIDTH || mapY < 0 || mapY >= DOOM_MAP_HEIGHT) {
        return false;
    }

    // Only allow exit if all enemies are killed
    return doomGame.map[mapY][mapX] == CELL_EXIT && doomIsExitUnlocked();
}

// ============================================
// Raycasting
// ============================================

static DoomRayHit castDoomRay(int32_t startX, int32_t startY, int angle) {
    DoomRayHit hit;
    hit.distance = INT_TO_FP(100);
    hit.wallType = CELL_WALL;
    hit.isVertical = false;

    int32_t rayDirX = doomCos(angle);
    int32_t rayDirY = doomSin(angle);

    int mapX = FP_TO_INT(startX);
    int mapY = FP_TO_INT(startY);

    int32_t deltaDistX = (rayDirX == 0) ? INT_TO_FP(100) : abs(FP_DIV(FP_ONE, rayDirX));
    int32_t deltaDistY = (rayDirY == 0) ? INT_TO_FP(100) : abs(FP_DIV(FP_ONE, rayDirY));

    int stepX, stepY;
    int32_t sideDistX, sideDistY;

    if (rayDirX < 0) {
        stepX = -1;
        sideDistX = FP_MUL(startX - INT_TO_FP(mapX), deltaDistX);
    } else {
        stepX = 1;
        sideDistX = FP_MUL(INT_TO_FP(mapX + 1) - startX, deltaDistX);
    }

    if (rayDirY < 0) {
        stepY = -1;
        sideDistY = FP_MUL(startY - INT_TO_FP(mapY), deltaDistY);
    } else {
        stepY = 1;
        sideDistY = FP_MUL(INT_TO_FP(mapY + 1) - startY, deltaDistY);
    }

    bool hitWall = false;
    int side = 0;
    int maxSteps = 32;

    while (!hitWall && maxSteps-- > 0) {
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
            side = 0;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
            side = 1;
        }

        if (mapX < 0 || mapX >= DOOM_MAP_WIDTH || mapY < 0 || mapY >= DOOM_MAP_HEIGHT) {
            hitWall = true;
            hit.wallType = CELL_WALL;
        } else {
            uint8_t cell = doomGame.map[mapY][mapX];
            if (cell == CELL_EXIT) {
                // Exit is always visible - color indicates if locked or unlocked
                hitWall = true;
                hit.wallType = doomIsExitUnlocked() ? CELL_EXIT_UNLOCKED : CELL_EXIT_LOCKED;
            } else if (cell != CELL_EMPTY && cell != CELL_ENEMY_SPAWN) {
                hitWall = true;
                hit.wallType = cell;
            }
        }
    }

    if (side == 0) {
        hit.distance = sideDistX - deltaDistX;
        hit.isVertical = true;
    } else {
        hit.distance = sideDistY - deltaDistY;
        hit.isVertical = false;
    }

    if (hit.distance < FLOAT_TO_FP(0.1f)) {
        hit.distance = FLOAT_TO_FP(0.1f);
    }

    hit.hitX = startX + FP_MUL(rayDirX, hit.distance);
    hit.hitY = startY + FP_MUL(rayDirY, hit.distance);

    return hit;
}

// ============================================
// Movement and Input
// ============================================

static void doomMovePlayer(int32_t dx, int32_t dy) {
    int32_t newX = doomGame.player.x + dx;
    int32_t newY = doomGame.player.y + dy;

    int32_t margin = FLOAT_TO_FP(0.2f);

    if (!doomIsWall(newX + margin, doomGame.player.y) &&
        !doomIsWall(newX - margin, doomGame.player.y)) {
        doomGame.player.x = newX;
    }

    if (!doomIsWall(doomGame.player.x, newY + margin) &&
        !doomIsWall(doomGame.player.x, newY - margin)) {
        doomGame.player.y = newY;
    }

    if (doomCheckExit(doomGame.player.x, doomGame.player.y)) {
        doomGame.state = DOOM_STATE_VICTORY;
        doomGame.player.score += 100 * doomGame.currentLevel;
    }

    doomGame.needsRender = true;
}

static void doomShoot() {
    if (doomGame.player.ammo <= 0) return;

    for (int i = 0; i < DOOM_MAX_PROJECTILES; i++) {
        if (!doomGame.projectiles[i].active) {
            DoomProjectile& proj = doomGame.projectiles[i];
            proj.x = doomGame.player.x;
            proj.y = doomGame.player.y;
            proj.dx = FP_MUL(doomCos(doomGame.player.angle), DOOM_PROJECTILE_SPEED);
            proj.dy = FP_MUL(doomSin(doomGame.player.angle), DOOM_PROJECTILE_SPEED);
            proj.active = true;
            proj.isPlayer = true;
            doomGame.player.ammo--;
            break;
        }
    }

    doomGame.needsRender = true;
}

static void doomOpenDoor() {
    int32_t checkDist = FP_ONE;
    int32_t checkX = doomGame.player.x + FP_MUL(doomCos(doomGame.player.angle), checkDist);
    int32_t checkY = doomGame.player.y + FP_MUL(doomSin(doomGame.player.angle), checkDist);

    int mapX = FP_TO_INT(checkX);
    int mapY = FP_TO_INT(checkY);

    if (mapX >= 0 && mapX < DOOM_MAP_WIDTH && mapY >= 0 && mapY < DOOM_MAP_HEIGHT) {
        if (doomGame.map[mapY][mapX] == CELL_DOOR) {
            doomGame.map[mapY][mapX] = CELL_EMPTY;
            doomGame.needsRender = true;
        }
    }
}

// Characters for type-to-shoot by difficulty
// Easy: single letters (E, T, A, I, N)
// Medium: common 2-letter combos
// Hard: 3-letter words
static const char* DOOM_EASY_CHARS = "ETAIN";
static const char* DOOM_MEDIUM_CHARS[] = {"AN", "AT", "EN", "IN", "IT", "NO", "ON", "SO", "TO", "TE"};
static const char* DOOM_HARD_CHARS[] = {"CAT", "DOG", "RUN", "HIT", "WIN", "MAP", "KEY", "GUN"};
static const int DOOM_MEDIUM_COUNT = 10;
static const int DOOM_HARD_COUNT = 8;

// Get a random target character/phrase based on difficulty
static const char* getRandomTarget() {
    switch (doomGame.difficulty) {
        case DOOM_EASY: {
            int idx = random(5);
            static char single[2] = {0, 0};
            single[0] = DOOM_EASY_CHARS[idx];
            return single;
        }
        case DOOM_MEDIUM:
            return DOOM_MEDIUM_CHARS[random(DOOM_MEDIUM_COUNT)];
        case DOOM_HARD:
            return DOOM_HARD_CHARS[random(DOOM_HARD_COUNT)];
        default:
            return "E";
    }
}

// Check if an enemy is in the crosshairs and update target
static void updateEnemyInView() {
    doomGame.enemyInView = false;
    doomGame.targetEnemyIndex = -1;

    float closestDist = 999.0f;
    int closestEnemy = -1;

    for (int e = 0; e < doomGame.enemyCount; e++) {
        DoomEnemy& enemy = doomGame.enemies[e];
        if (!enemy.active) continue;

        // Calculate angle and distance to enemy
        float dx = FP_TO_FLOAT(enemy.x - doomGame.player.x);
        float dy = FP_TO_FLOAT(enemy.y - doomGame.player.y);
        float dist = sqrt(dx * dx + dy * dy);
        if (dist < 0.5f || dist > 10.0f) continue;  // Too close or too far

        float angleToEnemy = atan2(dy, dx) * 180.0f / PI;
        float relAngle = angleToEnemy - doomGame.player.angle;
        while (relAngle > 180) relAngle -= 360;
        while (relAngle < -180) relAngle += 360;

        // Check if enemy is near center of view (within ~15 degrees)
        if (abs(relAngle) < 15.0f && dist < closestDist) {
            closestDist = dist;
            closestEnemy = e;
        }
    }

    if (closestEnemy >= 0) {
        doomGame.enemyInView = true;

        // If we're targeting a new enemy, pick a new character
        if (doomGame.targetEnemyIndex != closestEnemy) {
            doomGame.targetEnemyIndex = closestEnemy;
            const char* target = getRandomTarget();
            doomGame.targetChar = target[0];  // For now, just use first char
        }
    } else {
        // No enemy in view - clear target
        doomGame.targetChar = 0;
        doomGame.targetEnemyIndex = -1;
    }
}

// Process type-to-shoot: check if decoded char matches target
static void processTypeToShoot() {
    if (!doomGame.hasDecodedChar) return;

    char decoded = doomGame.lastDecodedChar;
    doomGame.hasDecodedChar = false;

    // Check if we typed the correct character while enemy is in view
    if (doomGame.enemyInView && doomGame.targetChar != 0) {
        if (decoded == doomGame.targetChar) {
            // Correct! Shoot the targeted enemy
            if (doomGame.targetEnemyIndex >= 0 &&
                doomGame.targetEnemyIndex < doomGame.enemyCount) {
                DoomEnemy& enemy = doomGame.enemies[doomGame.targetEnemyIndex];
                if (enemy.active) {
                    enemy.health--;
                    enemy.hitTimer = 10;

                    if (enemy.health <= 0) {
                        enemy.active = false;
                        doomGame.player.kills++;
                        doomGame.player.score += 10 * (doomGame.difficulty + 1);
                    }

                    // Pick new target character for next shot
                    const char* target = getRandomTarget();
                    doomGame.targetChar = target[0];
                    doomGame.needsRender = true;
                }
            }
        }
        // Wrong character - maybe penalize? For now just ignore
    }
}

static void processLiveKeyingInput() {
    unsigned long now = millis();

    // Movement controls via paddle holds
    if (doomGame.ditPressed && doomGame.dahPressed) {
        // Squeeze = move forward
        int32_t dx = FP_MUL(doomCos(doomGame.player.angle), DOOM_PLAYER_SPEED);
        int32_t dy = FP_MUL(doomSin(doomGame.player.angle), DOOM_PLAYER_SPEED);
        doomMovePlayer(dx, dy);
    } else if (doomGame.ditPressed) {
        // Dit held = turn left
        doomGame.player.angle -= DOOM_PLAYER_ROT_SPEED;
        if (doomGame.player.angle < 0) doomGame.player.angle += 360;
        doomGame.needsRender = true;
    } else if (doomGame.dahPressed) {
        // Dah held = turn right
        doomGame.player.angle += DOOM_PLAYER_ROT_SPEED;
        if (doomGame.player.angle >= 360) doomGame.player.angle -= 360;
        doomGame.needsRender = true;
    }

    // Quick dah tap = open door (kept this for interacting with doors)
    if (!doomGame.dahPressed && doomGame.lastDahPressed) {
        unsigned long pressDuration = now - doomGame.dahPressTime;
        if (pressDuration < DOOM_TAP_THRESHOLD_MS) {
            doomOpenDoor();
        }
    }

    // Note: Shooting is now handled by type-to-shoot mechanic
    // Type the displayed character to hit enemies
}

static void processLetterCommandInput() {
    if (!doomGame.hasDecodedChar) return;

    char cmd = doomGame.lastDecodedChar;
    doomGame.hasDecodedChar = false;

    int32_t dx, dy;

    switch (cmd) {
        case 'E':
            dx = FP_MUL(doomCos(doomGame.player.angle), DOOM_PLAYER_SPEED * 2);
            dy = FP_MUL(doomSin(doomGame.player.angle), DOOM_PLAYER_SPEED * 2);
            doomMovePlayer(dx, dy);
            break;
        case 'T':
            dx = FP_MUL(doomCos(doomGame.player.angle), -DOOM_PLAYER_SPEED * 2);
            dy = FP_MUL(doomSin(doomGame.player.angle), -DOOM_PLAYER_SPEED * 2);
            doomMovePlayer(dx, dy);
            break;
        case 'A':
            doomShoot();
            break;
        case 'S':
            dx = FP_MUL(doomCos(doomGame.player.angle - 90), DOOM_PLAYER_SPEED * 2);
            dy = FP_MUL(doomSin(doomGame.player.angle - 90), DOOM_PLAYER_SPEED * 2);
            doomMovePlayer(dx, dy);
            break;
        case 'U':
            dx = FP_MUL(doomCos(doomGame.player.angle + 90), DOOM_PLAYER_SPEED * 2);
            dy = FP_MUL(doomSin(doomGame.player.angle + 90), DOOM_PLAYER_SPEED * 2);
            doomMovePlayer(dx, dy);
            break;
        case 'D':
            doomOpenDoor();
            break;
        case 'I':
            doomGame.player.angle -= 45;
            if (doomGame.player.angle < 0) doomGame.player.angle += 360;
            doomGame.needsRender = true;
            break;
        case 'N':
            doomGame.player.angle += 45;
            if (doomGame.player.angle >= 360) doomGame.player.angle -= 360;
            doomGame.needsRender = true;
            break;
    }
}

// ============================================
// Game Update
// ============================================

static void updateDoomProjectiles() {
    for (int i = 0; i < DOOM_MAX_PROJECTILES; i++) {
        DoomProjectile& proj = doomGame.projectiles[i];
        if (!proj.active) continue;

        proj.x += proj.dx;
        proj.y += proj.dy;

        if (doomIsWall(proj.x, proj.y)) {
            proj.active = false;
            continue;
        }

        if (proj.isPlayer) {
            for (int e = 0; e < doomGame.enemyCount; e++) {
                DoomEnemy& enemy = doomGame.enemies[e];
                if (!enemy.active) continue;

                int32_t distX = abs(proj.x - enemy.x);
                int32_t distY = abs(proj.y - enemy.y);

                if (distX < FP_HALF && distY < FP_HALF) {
                    enemy.health--;
                    enemy.hitTimer = 10;
                    proj.active = false;

                    if (enemy.health <= 0) {
                        enemy.active = false;
                        doomGame.player.kills++;
                        doomGame.player.score += 10 * (doomGame.difficulty + 1);
                    }
                    break;
                }
            }
        }
    }

    doomGame.needsRender = true;
}

static void updateDoomEnemies() {
    for (int e = 0; e < doomGame.enemyCount; e++) {
        DoomEnemy& enemy = doomGame.enemies[e];
        if (!enemy.active) continue;

        if (enemy.hitTimer > 0) {
            enemy.hitTimer--;
        }

        int32_t distX = abs(doomGame.player.x - enemy.x);
        int32_t distY = abs(doomGame.player.y - enemy.y);

        if (distX < FP_HALF && distY < FP_HALF) {
            int damage = (doomGame.difficulty == DOOM_HARD) ? 20 : 10;
            doomGame.player.health -= damage;

            if (doomGame.player.health <= 0) {
                doomGame.player.health = 0;
                doomGame.state = DOOM_STATE_GAME_OVER;
            }
        }
    }
}

void updateDoomGame() {
    if (!doomActive || doomGame.state != DOOM_STATE_PLAYING) return;

    unsigned long now = millis();

    bool newDit = (digitalRead(DIT_PIN) == PADDLE_ACTIVE) ||
                  (touchRead(TOUCH_DIT_PIN) > TOUCH_THRESHOLD);
    bool newDah = (digitalRead(DAH_PIN) == PADDLE_ACTIVE) ||
                  (touchRead(TOUCH_DAH_PIN) > TOUCH_THRESHOLD);

    if (newDit != doomGame.ditPressed) {
        if (newDit) {
            doomGame.ditPressTime = now;
        } else {
            doomGame.ditReleaseTime = now;
        }
    }
    if (newDah != doomGame.dahPressed) {
        if (newDah) {
            doomGame.dahPressTime = now;
        } else {
            doomGame.dahReleaseTime = now;
        }
    }

    doomGame.lastDitPressed = doomGame.ditPressed;
    doomGame.lastDahPressed = doomGame.dahPressed;
    doomGame.ditPressed = newDit;
    doomGame.dahPressed = newDah;

    if (doomKeyer) {
        doomKeyer->key(PADDLE_DIT, newDit);
        doomKeyer->key(PADDLE_DAH, newDah);
        doomKeyer->tick(now);
    }

    // Process paddle input for movement/turning
    processLiveKeyingInput();

    // Check for enemies in crosshairs and update target character
    updateEnemyInView();

    // Check if player typed the correct character to shoot
    processTypeToShoot();

    updateDoomProjectiles();
    updateDoomEnemies();

    // Continue tone playback if active (needed for I2S audio)
    if (doomLastToneState) {
        continueTone(cwTone);
    }

    doomGame.frameCount++;
    doomGame.lastFrameTime = now;
}

// ============================================
// Rendering Helpers (for LVGL integration)
// ============================================

static uint16_t doomGetWallColor(int wallType, bool isVertical) {
    uint16_t color;
    switch (wallType) {
        case CELL_WALL:
            color = isVertical ? 0x8410 : 0xA514;  // Gray
            break;
        case CELL_WALL_RED:
            color = isVertical ? 0xA000 : 0xC800;  // Red
            break;
        case CELL_WALL_BLUE:
            color = isVertical ? 0x0010 : 0x001F;  // Blue
            break;
        case CELL_DOOR:
            color = isVertical ? 0x8200 : 0xA280;  // Brown
            break;
        case CELL_EXIT_LOCKED:
            color = isVertical ? 0xF800 : 0xF800;  // Bright red - LOCKED
            break;
        case CELL_EXIT_UNLOCKED:
            color = isVertical ? 0x07E0 : 0x07E0;  // Bright green - UNLOCKED
            break;
        default:
            color = isVertical ? 0x8410 : 0xA514;
    }
    return color;
}

static int doomGetWallHeight(int32_t distance) {
    if (distance < FLOAT_TO_FP(0.1f)) distance = FLOAT_TO_FP(0.1f);
    int height = FP_TO_INT(FP_DIV(INT_TO_FP(DOOM_RENDER_HEIGHT), distance));
    if (height > DOOM_RENDER_HEIGHT * 2) height = DOOM_RENDER_HEIGHT * 2;
    return height;
}

// Save/Load high scores
static void loadDoomHighScores() {
    Preferences prefs;
    prefs.begin("cwdoom", true);
    doomGame.highScores[0] = prefs.getInt("hs_easy", 0);
    doomGame.highScores[1] = prefs.getInt("hs_med", 0);
    doomGame.highScores[2] = prefs.getInt("hs_hard", 0);
    prefs.end();
}

static void saveDoomHighScore() {
    int diff = (int)doomGame.difficulty;
    if (doomGame.player.score > doomGame.highScores[diff]) {
        doomGame.highScores[diff] = doomGame.player.score;

        Preferences prefs;
        prefs.begin("cwdoom", false);
        switch (diff) {
            case 0: prefs.putInt("hs_easy", doomGame.highScores[0]); break;
            case 1: prefs.putInt("hs_med", doomGame.highScores[1]); break;
            case 2: prefs.putInt("hs_hard", doomGame.highScores[2]); break;
        }
        prefs.end();
    }
}

static void stopDoomGame() {
    if (doomActive) {
        saveDoomHighScore();
    }
    doomActive = false;
    stopTone();
}

#endif // GAME_CW_DOOM_H
