/*
 * CW Speeder Game
 *
 * A speed-typing game where players send Morse code matching a displayed
 * target word. Grey letters turn green as correctly keyed.
 *
 * Uses pattern-matching (not character decoding) - tracks individual
 * dit/dah elements and compares against pre-computed target pattern.
 */

#ifndef GAME_CW_SPEEDER_H
#define GAME_CW_SPEEDER_H

#include <Preferences.h>
#include "../core/config.h"
#include "../core/morse_code.h"
#include "../core/task_manager.h"  // For dual-core audio API
#include "../audio/i2s_audio.h"
#include "../lvgl/lv_screen_manager.h"
#include "../lvgl/lv_theme_summit.h"
#include "../lvgl/lv_widgets_summit.h"

// Forward declarations
extern void onLVGLBackNavigation();
extern void onLVGLMenuSelect(int mode);

// ============================================
// Word Challenges
// ============================================

struct WordChallenge {
    const char* id;
    const char* displayName;
    const char* word;
};

#define CS_NUM_CHALLENGES 6

const WordChallenge CS_WORD_CHALLENGES[CS_NUM_CHALLENGES] = {
    {"bensbestbentwire", "Ben's Best Bent Wire", "BENSBESTBENTWIRE"},
    {"mississippi", "Mississippi", "MISSISSIPPI"},
    {"quickbrownfox", "Quick Brown Fox", "THEQUICKBROWNFOXJUMPSOVERTHELAZYDOG"},
    {"packmybox", "Pack My Box", "PACKMYBOXWITHFIVEDOZENLIQUORJUGS"},
    {"cqcqcq", "CQ DX", "CQCQCQDX"},
    {"vailsummit", "VAIL Summit", "VAILSUMMIT"},
};

// ============================================
// Constants
// ============================================

#define CS_MAX_WORD_LENGTH 64
#define CS_MAX_PATTERN_LENGTH 256
#define CS_TIMING_BUFFER_SIZE 10
#define CS_WRONG_STATE_DELAY 2000   // ms to show WRONG before reset
#define CS_DEBOUNCE_MS 8            // Straight key debounce

// ============================================
// Pattern Matcher
// ============================================

class CWSpeedPatternMatcher {
private:
    char targetPattern[CS_MAX_PATTERN_LENGTH];
    int patternLength;
    int letterMilestones[CS_MAX_WORD_LENGTH];
    int numLetters;
    int patternIndex;

    // Adaptive timing
    float ditDurations[CS_TIMING_BUFFER_SIZE];
    float dahDurations[CS_TIMING_BUFFER_SIZE];
    int ditCount;
    int dahCount;
    float baseWPM;

    // Key timing
    unsigned long keyDownTime;
    bool keyIsDown;

public:
    // Callbacks
    void (*onLetterComplete)(int letterIndex);
    void (*onWrong)();
    void (*onAllComplete)();

    CWSpeedPatternMatcher() {
        reset();
        baseWPM = 15.0f;
        onLetterComplete = nullptr;
        onWrong = nullptr;
        onAllComplete = nullptr;
    }

    void setWPM(float wpm) {
        baseWPM = wpm;
    }

    void setTarget(const char* word) {
        // Clear buffers
        memset(targetPattern, 0, sizeof(targetPattern));
        memset(letterMilestones, 0, sizeof(letterMilestones));
        patternLength = 0;
        numLetters = 0;

        // Build pattern and milestones
        int pos = 0;
        for (int i = 0; word[i] != '\0' && numLetters < CS_MAX_WORD_LENGTH; i++) {
            char c = toupper(word[i]);
            if (c == ' ') continue;  // Skip spaces

            const char* morse = getMorseCode(c);
            if (morse != nullptr) {
                int morseLen = strlen(morse);
                if (pos + morseLen < CS_MAX_PATTERN_LENGTH) {
                    strcat(targetPattern, morse);
                    pos += morseLen;
                    letterMilestones[numLetters] = pos;
                    numLetters++;
                }
            }
        }
        patternLength = pos;

        Serial.printf("[CS] Pattern set: %s (%d elements, %d letters)\n",
                      targetPattern, patternLength, numLetters);
    }

    void reset() {
        patternIndex = 0;
        keyDownTime = 0;
        keyIsDown = false;
        // Keep adaptive timing - user's speed doesn't change between attempts
    }

    void fullReset() {
        reset();
        ditCount = 0;
        dahCount = 0;
        memset(ditDurations, 0, sizeof(ditDurations));
        memset(dahDurations, 0, sizeof(dahDurations));
    }

    float getThreshold() const {
        float baseDit = 1200.0f / baseWPM;

        // Calculate adaptive dit from buffers
        float adaptiveDit = baseDit;
        int totalSamples = ditCount + dahCount;

        if (totalSamples >= 3) {
            float sum = 0;
            float weightSum = 0;

            for (int i = 0; i < ditCount; i++) {
                float weight = (float)(i + 1);
                sum += ditDurations[i] * weight;
                weightSum += weight;
            }
            for (int i = 0; i < dahCount; i++) {
                float weight = (float)(i + 1);
                sum += dahDurations[i] * weight;
                weightSum += weight;
            }

            if (weightSum > 0) {
                adaptiveDit = sum / weightSum;
            }
        }

        // Blend 70% adaptive + 30% base
        float blendedDit = adaptiveDit * 0.7f + baseDit * 0.3f;

        // Clamp to reasonable range (40-300ms, roughly 5-50 WPM)
        blendedDit = max(40.0f, min(300.0f, blendedDit));

        return blendedDit * 2.0f;  // Threshold at 2x dit duration
    }

    void keyDown() {
        keyDownTime = millis();
        keyIsDown = true;
    }

    void keyUp() {
        if (!keyIsDown || keyDownTime == 0) return;

        unsigned long now = millis();
        float duration = (float)(now - keyDownTime);
        keyIsDown = false;
        keyDownTime = 0;

        // Classify element
        float threshold = getThreshold();
        char element = (duration < threshold) ? '.' : '-';

        Serial.printf("[CS] Element: '%c' (dur=%.0fms, thresh=%.0fms) at pos %d/%d\n",
                      element, duration, threshold, patternIndex, patternLength);

        // Update adaptive timing
        if (element == '.') {
            if (ditCount < CS_TIMING_BUFFER_SIZE) {
                ditDurations[ditCount++] = duration;
            } else {
                // Shift buffer
                for (int i = 0; i < CS_TIMING_BUFFER_SIZE - 1; i++) {
                    ditDurations[i] = ditDurations[i + 1];
                }
                ditDurations[CS_TIMING_BUFFER_SIZE - 1] = duration;
            }
        } else {
            // Dah: store effective dit length (duration / 3)
            float effectiveDit = duration / 3.0f;
            if (dahCount < CS_TIMING_BUFFER_SIZE) {
                dahDurations[dahCount++] = effectiveDit;
            } else {
                for (int i = 0; i < CS_TIMING_BUFFER_SIZE - 1; i++) {
                    dahDurations[i] = dahDurations[i + 1];
                }
                dahDurations[CS_TIMING_BUFFER_SIZE - 1] = effectiveDit;
            }
        }

        // Check against expected pattern
        if (patternIndex >= patternLength) return;

        char expected = targetPattern[patternIndex];

        if (element == expected) {
            patternIndex++;

            // Check if we hit a letter milestone
            for (int i = 0; i < numLetters; i++) {
                if (letterMilestones[i] == patternIndex) {
                    Serial.printf("[CS] Letter %d complete!\n", i);
                    if (onLetterComplete) onLetterComplete(i);
                    break;
                }
            }

            // Check if all complete
            if (patternIndex >= patternLength) {
                Serial.println("[CS] Pattern complete!");
                if (onAllComplete) onAllComplete();
            }
        } else {
            Serial.printf("[CS] WRONG! Expected '%c' got '%c'\n", expected, element);
            if (onWrong) onWrong();
        }
    }

    int getPatternIndex() const { return patternIndex; }
    int getPatternLength() const { return patternLength; }
    int getNumLetters() const { return numLetters; }
    int getCurrentLetterIndex() const {
        for (int i = 0; i < numLetters; i++) {
            if (letterMilestones[i] > patternIndex) {
                return i;
            }
        }
        return numLetters - 1;
    }
};

// ============================================
// Game State
// ============================================

enum CWSpeedState {
    CS_STATE_IDLE,      // Waiting for first keypress
    CS_STATE_PLAYING,   // Timer running
    CS_STATE_WRONG,     // Red flash, waiting to reset
    CS_STATE_COMPLETE   // Show final time
};

struct CWSpeedGame {
    CWSpeedState state;

    // Current word
    int selectedChallenge;
    char targetWord[CS_MAX_WORD_LENGTH];
    int wordLength;

    // Timing
    unsigned long gameStartTime;
    unsigned long wrongStateTime;
    unsigned long bestTime;       // In milliseconds

    // Pattern matcher
    CWSpeedPatternMatcher matcher;

    // Keyer state (for iambic)
    bool keyerActive;
    bool sendingDit;
    bool sendingDah;
    bool inSpacing;
    bool ditMemory;
    bool dahMemory;
    unsigned long elementStart;
    bool lastToneState;
    unsigned long lastStateChange;

    // Straight key debounce
    bool debouncedKeyState;
    unsigned long keyLastChangeTime;
    bool keyLastRawState;
};

static CWSpeedGame csGame;
static Preferences csPrefs;

// ============================================
// LVGL Screen Elements - Word Select
// ============================================

static lv_obj_t* cs_select_screen = NULL;
static lv_obj_t* cs_select_list = NULL;
static lv_obj_t* cs_select_buttons[CS_NUM_CHALLENGES] = {NULL};

// ============================================
// LVGL Screen Elements - Game
// ============================================

static lv_obj_t* cs_game_screen = NULL;
static lv_obj_t* cs_timer_label = NULL;
static lv_obj_t* cs_letters_container = NULL;
static lv_obj_t* cs_letter_labels[CS_MAX_WORD_LENGTH] = {NULL};
static lv_obj_t* cs_status_label = NULL;
static lv_obj_t* cs_best_label = NULL;

// ============================================
// Forward Declarations
// ============================================

void csResetGame();
void csUpdateTimer(unsigned long ms);
void csUpdateStatus(const char* status);
void csUpdateBestTime();
void csSetLetterColor(int index, lv_color_t color);
void csResetLetterColors();

// ============================================
// Pattern Matcher Callbacks
// ============================================

void csOnLetterComplete(int letterIndex) {
    csSetLetterColor(letterIndex, LV_COLOR_SUCCESS);
    beep(1200, 30);  // Quick success chirp
}

void csOnWrong() {
    csGame.state = CS_STATE_WRONG;
    csGame.wrongStateTime = millis();

    // Flash all letters red
    for (int i = 0; i < csGame.wordLength; i++) {
        csSetLetterColor(i, LV_COLOR_ERROR);
    }
    csUpdateStatus("WRONG!");
    requestStopTone();  // Non-blocking stop via dual-core audio
    beep(400, 200);
}

void csOnAllComplete() {
    csGame.state = CS_STATE_COMPLETE;
    unsigned long finalTime = millis() - csGame.gameStartTime;

    requestStopTone();  // Non-blocking stop via dual-core audio

    // Check for new best time
    bool newBest = (finalTime < csGame.bestTime || csGame.bestTime == 0);
    if (newBest) {
        csGame.bestTime = finalTime;
        // Save best time
        csPrefs.begin("cwspeed", false);
        char key[32];
        snprintf(key, sizeof(key), "best_%d", csGame.selectedChallenge);
        csPrefs.putULong(key, csGame.bestTime);
        csPrefs.end();

        csUpdateStatus("NEW BEST!");
    } else {
        csUpdateStatus("COMPLETE!");
    }

    csUpdateTimer(finalTime);
    csUpdateBestTime();
    beep(1000, 300);

    Serial.printf("[CS] Complete! Time: %lu ms (best: %lu ms)\n",
                  finalTime, csGame.bestTime);
}

// ============================================
// Preferences
// ============================================

void csLoadPrefs() {
    csPrefs.begin("cwspeed", true);
    csGame.selectedChallenge = csPrefs.getInt("selected", 0);

    // Load best time for selected challenge
    char key[32];
    snprintf(key, sizeof(key), "best_%d", csGame.selectedChallenge);
    csGame.bestTime = csPrefs.getULong(key, 0);
    csPrefs.end();
}

void csSaveSelectedChallenge() {
    csPrefs.begin("cwspeed", false);
    csPrefs.putInt("selected", csGame.selectedChallenge);
    csPrefs.end();
}

void csLoadBestTimeForChallenge(int challenge) {
    csPrefs.begin("cwspeed", true);
    char key[32];
    snprintf(key, sizeof(key), "best_%d", challenge);
    csGame.bestTime = csPrefs.getULong(key, 0);
    csPrefs.end();
}

// ============================================
// UI Update Functions
// ============================================

void csUpdateTimer(unsigned long ms) {
    if (cs_timer_label == NULL) return;

    int minutes = ms / 60000;
    int seconds = (ms / 1000) % 60;
    int millis_part = ms % 1000;

    lv_label_set_text_fmt(cs_timer_label, "%02d:%02d.%03d",
                          minutes, seconds, millis_part);
}

void csUpdateStatus(const char* status) {
    if (cs_status_label != NULL) {
        lv_label_set_text(cs_status_label, status);
    }
}

void csUpdateBestTime() {
    if (cs_best_label == NULL) return;

    if (csGame.bestTime == 0) {
        lv_label_set_text(cs_best_label, "Best: --");
    } else {
        int seconds = csGame.bestTime / 1000;
        int millis_part = csGame.bestTime % 1000;
        lv_label_set_text_fmt(cs_best_label, "Best: %d.%03ds", seconds, millis_part);
    }
}

void csSetLetterColor(int index, lv_color_t color) {
    if (index >= 0 && index < csGame.wordLength && cs_letter_labels[index] != NULL) {
        lv_obj_set_style_text_color(cs_letter_labels[index], color, 0);
    }
}

void csResetLetterColors() {
    for (int i = 0; i < csGame.wordLength; i++) {
        csSetLetterColor(i, LV_COLOR_TEXT_DISABLED);
    }
}

// Delay that keeps LVGL running
void csDelayWithUI(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        lv_timer_handler();
        delay(5);
    }
}

// ============================================
// Game Reset
// ============================================

void csResetGame() {
    csGame.state = CS_STATE_IDLE;
    csGame.gameStartTime = 0;
    csGame.matcher.reset();

    // Reset keyer state
    csGame.keyerActive = false;
    csGame.sendingDit = false;
    csGame.sendingDah = false;
    csGame.inSpacing = false;
    csGame.ditMemory = false;
    csGame.dahMemory = false;
    csGame.lastToneState = false;
    csGame.lastStateChange = 0;
    csGame.debouncedKeyState = false;
    csGame.keyLastChangeTime = 0;
    csGame.keyLastRawState = false;

    requestStopTone();  // Non-blocking stop via dual-core audio
    csResetLetterColors();
    csUpdateTimer(0);
    csUpdateStatus("GET READY");
}

// ============================================
// Key Event Handler - Game
// ============================================

static void cs_game_key_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ESC) {
        requestStopTone();  // Non-blocking stop via dual-core audio
        onLVGLBackNavigation();
        lv_event_stop_processing(e);
    }
    else if (key == ' ') {
        // Space = reset game
        csResetGame();
    }
}

// ============================================
// Word Select Screen Creation
// ============================================

static void cs_select_btn_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    int challenge = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("[CS] Selected challenge: %d\n", challenge);

    csGame.selectedChallenge = challenge;
    csSaveSelectedChallenge();
    csLoadBestTimeForChallenge(challenge);

    // Navigate to game screen (LVGL_MODE_CW_SPEEDER = 135)
    onLVGLMenuSelect(135);
}

static void cs_select_key_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ESC) {
        onLVGLBackNavigation();
        lv_event_stop_processing(e);
    }
}

lv_obj_t* createCWSpeedSelectScreen() {
    // Reset pointers
    cs_select_screen = NULL;
    cs_select_list = NULL;
    for (int i = 0; i < CS_NUM_CHALLENGES; i++) {
        cs_select_buttons[i] = NULL;
    }

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Status bar
    createCompactStatusBar(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_layout(title_bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(title_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "CW SPEEDER");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);

    // Subtitle
    lv_obj_t* subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "Select a Challenge");
    lv_obj_add_style(subtitle, getStyleLabelBody(), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 10);

    // List container
    cs_select_list = lv_obj_create(screen);
    lv_obj_set_size(cs_select_list, SCREEN_WIDTH - 40, SCREEN_HEIGHT - HEADER_HEIGHT - 80);
    lv_obj_set_pos(cs_select_list, 20, HEADER_HEIGHT + 40);
    lv_obj_set_layout(cs_select_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cs_select_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cs_select_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cs_select_list, 8, 0);
    lv_obj_set_style_pad_all(cs_select_list, 10, 0);
    lv_obj_set_style_bg_opa(cs_select_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cs_select_list, 0, 0);

    // Create buttons for each challenge
    for (int i = 0; i < CS_NUM_CHALLENGES; i++) {
        lv_obj_t* btn = lv_btn_create(cs_select_list);
        lv_obj_set_size(btn, SCREEN_WIDTH - 80, 40);
        lv_obj_add_style(btn, getStyleBtn(), 0);
        lv_obj_add_event_cb(btn, cs_select_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, cs_select_key_event_cb, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(btn, linear_nav_handler, LV_EVENT_KEY, NULL);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, CS_WORD_CHALLENGES[i].displayName);
        lv_obj_center(label);

        cs_select_buttons[i] = btn;
        addNavigableWidget(btn);
    }

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, "ENTER: Select   ESC: Back");
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    // Focus first button
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_focus_obj(cs_select_buttons[0]);
    }

    cs_select_screen = screen;
    return screen;
}

// ============================================
// Game Screen Creation
// ============================================

lv_obj_t* createCWSpeedGameScreen() {
    // Reset pointers
    cs_game_screen = NULL;
    cs_timer_label = NULL;
    cs_letters_container = NULL;
    cs_status_label = NULL;
    cs_best_label = NULL;
    for (int i = 0; i < CS_MAX_WORD_LENGTH; i++) {
        cs_letter_labels[i] = NULL;
    }

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Status bar
    createCompactStatusBar(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_layout(title_bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(title_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    // Show the selected challenge name in the title
    lv_label_set_text(title, CS_WORD_CHALLENGES[csGame.selectedChallenge].displayName);
    lv_obj_add_style(title, getStyleLabelTitle(), 0);

    // Timer display (large, centered)
    cs_timer_label = lv_label_create(screen);
    lv_label_set_text(cs_timer_label, "00:00.000");
    lv_obj_set_style_text_font(cs_timer_label, getThemeFonts()->font_large, 0);
    lv_obj_set_style_text_color(cs_timer_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(cs_timer_label, LV_ALIGN_CENTER, 0, -60);

    // Letters container
    cs_letters_container = lv_obj_create(screen);
    lv_obj_set_size(cs_letters_container, SCREEN_WIDTH - 20, 50);
    lv_obj_set_layout(cs_letters_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cs_letters_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cs_letters_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cs_letters_container, 2, 0);
    lv_obj_set_style_bg_opa(cs_letters_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cs_letters_container, 0, 0);
    lv_obj_clear_flag(cs_letters_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(cs_letters_container, LV_ALIGN_CENTER, 0, 0);

    // Create letter labels based on selected word
    const char* word = CS_WORD_CHALLENGES[csGame.selectedChallenge].word;
    csGame.wordLength = strlen(word);
    strncpy(csGame.targetWord, word, CS_MAX_WORD_LENGTH - 1);
    csGame.targetWord[CS_MAX_WORD_LENGTH - 1] = '\0';

    // Choose font size based on word length
    const lv_font_t* letterFont = getThemeFonts()->font_title;
    if (csGame.wordLength > 20) {
        letterFont = getThemeFonts()->font_body;
    } else if (csGame.wordLength > 12) {
        letterFont = getThemeFonts()->font_subtitle;
    }

    for (int i = 0; i < csGame.wordLength && i < CS_MAX_WORD_LENGTH; i++) {
        cs_letter_labels[i] = lv_label_create(cs_letters_container);
        char buf[2] = {csGame.targetWord[i], '\0'};
        lv_label_set_text(cs_letter_labels[i], buf);
        lv_obj_set_style_text_color(cs_letter_labels[i], LV_COLOR_TEXT_DISABLED, 0);
        lv_obj_set_style_text_font(cs_letter_labels[i], letterFont, 0);
    }

    // Status label
    cs_status_label = lv_label_create(screen);
    lv_label_set_text(cs_status_label, "GET READY");
    lv_obj_set_style_text_font(cs_status_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(cs_status_label, LV_COLOR_ACCENT_GREEN, 0);
    lv_obj_align(cs_status_label, LV_ALIGN_CENTER, 0, 50);

    // Best time label
    cs_best_label = lv_label_create(screen);
    lv_obj_add_style(cs_best_label, getStyleLabelBody(), 0);
    lv_obj_align(cs_best_label, LV_ALIGN_CENTER, 0, 80);
    csUpdateBestTime();

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, "Paddle: Start   SPACE: Reset   ESC: Back");
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    // Focus container for keyboard input
    lv_obj_t* focus = lv_obj_create(screen);
    lv_obj_set_size(focus, 1, 1);
    lv_obj_set_pos(focus, -10, -10);
    lv_obj_set_style_bg_opa(focus, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus, 0, 0);
    lv_obj_set_style_outline_width(focus, 0, 0);
    lv_obj_set_style_outline_width(focus, 0, LV_STATE_FOCUSED);
    lv_obj_clear_flag(focus, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(focus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus, cs_game_key_event_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus);

    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }
    lv_group_focus_obj(focus);

    cs_game_screen = screen;
    return screen;
}

// ============================================
// Straight Key Handler
// ============================================

void csStraightKeyHandler(bool rawKeyDown) {
    extern int cwTone;
    unsigned long now = millis();
    bool toneOn = isAudioTonePlaying();  // Use dual-core status check

    // Software debouncing
    if (rawKeyDown != csGame.keyLastRawState) {
        csGame.keyLastChangeTime = now;
        csGame.keyLastRawState = rawKeyDown;
    }

    if ((now - csGame.keyLastChangeTime) >= CS_DEBOUNCE_MS) {
        csGame.debouncedKeyState = rawKeyDown;
    }

    bool keyDown = csGame.debouncedKeyState;

    if (keyDown && !toneOn) {
        // Key down - start tone
        if (!csGame.lastToneState) {
            csGame.matcher.keyDown();
            csGame.lastStateChange = now;
            csGame.lastToneState = true;
        }
        requestStartTone(cwTone);  // Non-blocking via dual-core audio
    }
    else if (keyDown && toneOn) {
        // Tone continues automatically on audio core
    }
    else if (!keyDown && toneOn) {
        // Key up - stop tone
        if (csGame.lastToneState) {
            csGame.matcher.keyUp();
            csGame.lastStateChange = now;
            csGame.lastToneState = false;
        }
        requestStopTone();  // Non-blocking via dual-core audio
    }
}

// ============================================
// Iambic Keyer Handler
// ============================================

void csIambicHandler(bool ditPressed, bool dahPressed) {
    extern int cwTone, cwSpeed;
    MorseTiming timing(cwSpeed);
    unsigned long ditDuration = timing.ditDuration;
    unsigned long now = millis();

    // Start new element
    if (!csGame.keyerActive && !csGame.inSpacing) {
        bool startDit = csGame.ditMemory || ditPressed;
        bool startDah = csGame.dahMemory || dahPressed;

        if (startDit || startDah) {
            csGame.ditMemory = false;
            csGame.dahMemory = false;

            if (startDit && startDah) {
                // Squeeze - alternate
                csGame.sendingDit = !csGame.sendingDah;
                csGame.sendingDah = !csGame.sendingDit;
            } else {
                csGame.sendingDit = startDit;
                csGame.sendingDah = startDah;
            }

            csGame.keyerActive = true;
            csGame.elementStart = now;

            // Record key down for pattern matcher
            csGame.matcher.keyDown();
            csGame.lastStateChange = now;
            csGame.lastToneState = true;

            requestStartTone(cwTone);  // Non-blocking via dual-core audio
        }
    }
    // Sending element
    else if (csGame.keyerActive && !csGame.inSpacing) {
        unsigned long duration = csGame.sendingDit ? ditDuration : (ditDuration * 3);

        // Tone continues automatically on audio core - no need to call continueTone

        // Check paddle memory
        if (ditPressed && dahPressed) {
            if (csGame.sendingDit) csGame.dahMemory = true;
            else csGame.ditMemory = true;
        } else if (csGame.sendingDit && dahPressed) {
            csGame.dahMemory = true;
        } else if (csGame.sendingDah && ditPressed) {
            csGame.ditMemory = true;
        }

        // Element complete?
        if (now - csGame.elementStart >= duration) {
            requestStopTone();  // Non-blocking via dual-core audio

            // Record key up for pattern matcher
            csGame.matcher.keyUp();
            csGame.lastStateChange = now;
            csGame.lastToneState = false;

            csGame.keyerActive = false;
            csGame.inSpacing = true;
            csGame.elementStart = now;
        }
    }
    // Inter-element spacing
    else if (csGame.inSpacing) {
        if (now - csGame.elementStart >= ditDuration) {
            csGame.inSpacing = false;

            // Check for next element
            if (csGame.ditMemory) {
                csGame.ditMemory = false;
            } else if (csGame.dahMemory) {
                csGame.dahMemory = false;
            } else if (ditPressed) {
                csGame.ditMemory = true;
            } else if (dahPressed) {
                csGame.dahMemory = true;
            }
        }
    }
}

// ============================================
// Main Update Function
// ============================================

void cwSpeedUpdate() {
    unsigned long now = millis();

    switch (csGame.state) {
        case CS_STATE_IDLE:
            // Nothing to do, waiting for input
            break;

        case CS_STATE_PLAYING:
            // Update timer display
            csUpdateTimer(now - csGame.gameStartTime);
            break;

        case CS_STATE_WRONG:
            // Wait for delay then reset
            if (now - csGame.wrongStateTime > CS_WRONG_STATE_DELAY) {
                csResetGame();
                csUpdateStatus("TRY AGAIN");
            }
            break;

        case CS_STATE_COMPLETE:
            // Nothing to do, waiting for user to reset
            break;
    }
}

// ============================================
// Paddle Input Handler
// ============================================

void cwSpeedHandlePaddle(bool ditPressed, bool dahPressed) {
    // Only accept input during idle or playing
    if (csGame.state != CS_STATE_IDLE && csGame.state != CS_STATE_PLAYING) {
        return;
    }

    // First keypress starts the game
    if (csGame.state == CS_STATE_IDLE && (ditPressed || dahPressed)) {
        csGame.state = CS_STATE_PLAYING;
        csGame.gameStartTime = millis();
        csUpdateStatus("GO!");
    }

    // Route to appropriate handler
    extern KeyType cwKeyType;
    if (cwKeyType == KEY_STRAIGHT) {
        csStraightKeyHandler(ditPressed);
    } else {
        csIambicHandler(ditPressed, dahPressed);
    }
}

// ============================================
// Game Initialization
// ============================================

void cwSpeedSelectStart() {
    Serial.println("[CS] ========================================");
    Serial.println("[CS] STARTING CW SPEEDER - WORD SELECT");
    Serial.println("[CS] ========================================");

    csLoadPrefs();
}

void cwSpeedGameStart() {
    Serial.println("[CS] ========================================");
    Serial.println("[CS] STARTING CW SPEEDER - GAME");
    Serial.println("[CS] ========================================");

    // Load preferences if not already loaded
    csLoadPrefs();

    // Set up pattern matcher
    extern int cwSpeed;
    csGame.matcher.setWPM((float)cwSpeed);
    csGame.matcher.setTarget(csGame.targetWord);
    csGame.matcher.onLetterComplete = csOnLetterComplete;
    csGame.matcher.onWrong = csOnWrong;
    csGame.matcher.onAllComplete = csOnAllComplete;

    // Initialize game state
    csResetGame();

    Serial.printf("[CS] Challenge: %s (%d letters)\n",
                  CS_WORD_CHALLENGES[csGame.selectedChallenge].displayName,
                  csGame.wordLength);
    Serial.printf("[CS] Best time: %lu ms\n", csGame.bestTime);
}

#endif // GAME_CW_SPEEDER_H
