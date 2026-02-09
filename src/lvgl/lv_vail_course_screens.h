/*
 * VAIL SUMMIT - Vail CW Course LVGL Screens
 * Module selection, lesson screens, and practice UI for CW School training
 */

#ifndef LV_VAIL_COURSE_SCREENS_H
#define LV_VAIL_COURSE_SCREENS_H

#include <lvgl.h>
#include "lv_theme_summit.h"
#include "lv_widgets_summit.h"
#include "lv_screen_manager.h"
#include "../core/config.h"
#include "../core/modes.h"
#include "../training/training_vail_course_core.h"
#include "../settings/settings_cwschool.h"

// Forward declarations for mode switching
extern void setCurrentModeFromInt(int mode);
extern void onLVGLMenuSelect(int target_mode);  // Proper navigation with screen loading

// ============================================
// Screen State
// ============================================

static lv_obj_t* vail_course_module_buttons[MODULE_COUNT];
static int vail_course_selected_module = 0;

// Navigation context for module grid (3-column grid)
static int vail_course_module_button_count = 0;
static NavGridContext vail_course_module_nav_ctx = {
    vail_course_module_buttons, &vail_course_module_button_count, 3
};

// ============================================
// Module Selection Screen
// ============================================

static void vail_course_module_click_handler(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    int moduleIdx = (int)(intptr_t)lv_event_get_user_data(e);

    // Check if module is unlocked
    if (!isVailCourseModuleUnlocked((VailCourseModule)moduleIdx)) {
        Serial.printf("[VailCourse] Module %d is locked\n", moduleIdx);
        return;
    }

    vail_course_selected_module = moduleIdx;
    vailCourseProgress.currentModule = (VailCourseModule)moduleIdx;

    // Navigate to lesson select for this module
    onLVGLMenuSelect(MODE_VAIL_COURSE_LESSON_SELECT);
}

/*
 * Create module selection screen (4x3 grid)
 */
lv_obj_t* createVailCourseModuleSelectScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Vail CW Course - Modules");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Account status in header
    lv_obj_t* status = lv_label_create(header);
    if (isCWSchoolLinked()) {
        lv_label_set_text(status, getCWSchoolAccountDisplay().c_str());
        lv_obj_set_style_text_color(status, LV_COLOR_SUCCESS, 0);
    } else {
        lv_label_set_text(status, "Offline Mode");
        lv_obj_set_style_text_color(status, LV_COLOR_TEXT_TERTIARY, 0);
    }
    lv_obj_set_style_text_font(status, getThemeFonts()->font_body, 0);
    lv_obj_align(status, LV_ALIGN_RIGHT_MID, -15, 0);

    // Module grid container
    lv_obj_t* grid = lv_obj_create(screen);
    lv_obj_set_size(grid, LV_PCT(95), 210);
    lv_obj_align(grid, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 5, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(grid, 8, 0);
    lv_obj_set_style_pad_column(grid, 8, 0);
    lv_obj_add_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    // Create module buttons (4 rows x 3 columns)
    vail_course_module_button_count = 0;
    for (int i = 0; i < MODULE_COUNT; i++) {
        bool unlocked = isVailCourseModuleUnlocked((VailCourseModule)i);
        bool completed = isVailCourseModuleCompleted((VailCourseModule)i);

        lv_obj_t* btn = lv_btn_create(grid);
        lv_obj_set_size(btn, 145, 65);

        if (unlocked) {
            applyMenuCardStyle(btn);
            if (completed) {
                lv_obj_set_style_bg_color(btn, LV_COLOR_SUCCESS, 0);
                lv_obj_set_style_bg_color(btn, LV_COLOR_ACCENT_GREEN, LV_STATE_FOCUSED);
            }
        } else {
            lv_obj_set_style_bg_color(btn, LV_COLOR_TEXT_DISABLED, 0);
            lv_obj_set_style_radius(btn, 8, 0);
            lv_obj_add_state(btn, LV_STATE_DISABLED);
        }

        // Module name
        lv_obj_t* lbl = lv_label_create(btn);
        char text[64];
        if (completed) snprintf(text, sizeof(text), LV_SYMBOL_OK " %s", vailCourseModuleNames[i]);
        else if (!unlocked) snprintf(text, sizeof(text), LV_SYMBOL_CLOSE " %s", vailCourseModuleNames[i]);
        else snprintf(text, sizeof(text), "%s", vailCourseModuleNames[i]);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_font(lbl, getThemeFonts()->font_body, 0);
        lv_obj_center(lbl);

        if (unlocked) {
            lv_obj_add_event_cb(btn, vail_course_module_click_handler, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            lv_obj_add_event_cb(btn, grid_nav_handler, LV_EVENT_KEY, &vail_course_module_nav_ctx);
            addNavigableWidget(btn);
            // Store button at sequential index for proper grid navigation
            vail_course_module_buttons[vail_course_module_button_count] = btn;
            vail_course_module_button_count++;
        }
    }

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "Arrows Navigate   ENTER Select   ESC Back");
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -5);

    return screen;
}

// ============================================
// Lesson Selection Screen
// ============================================

static void vail_course_lesson_click_handler(lv_event_t* e) {
    int lessonNum = (int)(intptr_t)lv_event_get_user_data(e);

    vailCourseProgress.currentLesson = lessonNum;
    vailCourseProgress.currentPhase = PHASE_INTRO;

    // Navigate to lesson practice
    onLVGLMenuSelect(MODE_VAIL_COURSE_LESSON);
}

lv_obj_t* createVailCourseLessonSelectScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    VailCourseModule module = vailCourseProgress.currentModule;

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    char titleText[64];
    snprintf(titleText, sizeof(titleText), "%s - Lessons", vailCourseModuleNames[module]);
    lv_label_set_text(title, titleText);
    lv_obj_set_style_text_font(title, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Characters info
    lv_obj_t* chars_label = lv_label_create(header);
    char charsText[64];
    if (strlen(vailCourseModuleChars[module]) == 0) {
        strcpy(charsText, "Review");
    } else {
        snprintf(charsText, sizeof(charsText), "Chars: %s", vailCourseModuleChars[module]);
    }
    lv_label_set_text(chars_label, charsText);
    lv_obj_set_style_text_font(chars_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(chars_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(chars_label, LV_ALIGN_RIGHT_MID, -15, 0);

    // Lesson list
    lv_obj_t* list = lv_obj_create(screen);
    lv_obj_set_size(list, 400, 180);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 10, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, 10, 0);

    int lessonCount = vailCourseLessonCounts[module];
    int lessonsCompleted = getVailCourseLessonsCompleted(module);

    for (int i = 1; i <= lessonCount; i++) {
        bool completed = (i <= lessonsCompleted);
        bool current = (i == lessonsCompleted + 1 || (lessonsCompleted == 0 && i == 1));

        lv_obj_t* btn = lv_btn_create(list);
        lv_obj_set_size(btn, 350, 50);
        applyMenuCardStyle(btn);

        if (completed) {
            lv_obj_set_style_bg_color(btn, LV_COLOR_SUCCESS, 0);
            lv_obj_set_style_bg_color(btn, LV_COLOR_ACCENT_GREEN, LV_STATE_FOCUSED);
        } else if (current) {
            lv_obj_set_style_bg_color(btn, LV_COLOR_CARD_CYAN, 0);
            lv_obj_set_style_bg_color(btn, LV_COLOR_CARD_BLUE, LV_STATE_FOCUSED);
        }

        lv_obj_t* lbl = lv_label_create(btn);
        char lessonText[64];
        if (completed) snprintf(lessonText, sizeof(lessonText), LV_SYMBOL_OK " Lesson %d", i);
        else if (current) snprintf(lessonText, sizeof(lessonText), "Lesson %d (Current)", i);
        else snprintf(lessonText, sizeof(lessonText), "Lesson %d", i);
        lv_label_set_text(lbl, lessonText);
        lv_obj_set_style_text_font(lbl, getThemeFonts()->font_input, 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, vail_course_lesson_click_handler, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, linear_nav_handler, LV_EVENT_KEY, NULL);
        addNavigableWidget(btn);
    }

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "ENTER Start Lesson   ESC Back");
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -5);

    return screen;
}

// ============================================
// Lesson Practice State
// ============================================

// Lesson state machine
struct VailCourseLessonState {
    // Phase progress
    int phaseItemIndex;          // Current item within phase
    int phaseItemCount;          // Total items in current phase
    int phaseCorrect;            // Correct answers in phase
    int phaseTotal;              // Total answers in phase

    // Current character/group being tested
    char currentChar;            // Single character being played
    char currentGroup[16];       // Character group (for PHASE_GROUPS)
    char availableChars[64];     // Characters available for this lesson

    // Playback state
    int playbackCount;           // Times character has been played
    bool waitingForInput;        // Waiting for user's answer
    bool showingFeedback;        // Showing correct/incorrect feedback
    unsigned long feedbackTime;  // When feedback started

    // Intro phase state
    int introCharIndex;          // Which new character we're introducing

    // Group input accumulation (for PHASE_GROUPS)
    char groupInputBuffer[16];   // Accumulated characters for group answer

    // UI elements
    lv_obj_t* screen;
    lv_obj_t* phase_label;
    lv_obj_t* progress_label;
    lv_obj_t* main_label;        // Large character display
    lv_obj_t* feedback_label;    // Correct/Incorrect
    lv_obj_t* score_label;       // X/Y correct
    lv_obj_t* prompt_label;      // Instructions
    lv_obj_t* footer_label;
    lv_obj_t* group_input_label; // Display user's accumulated input (for groups)
};

static VailCourseLessonState lessonState = {0};

// Thresholds
#define VAIL_LESSON_SOLO_COUNT 5       // Items per solo phase
#define VAIL_LESSON_MIXED_COUNT 10     // Items per mixed phase
#define VAIL_LESSON_GROUP_COUNT 5      // Items per group phase
#define VAIL_LESSON_PASS_THRESHOLD 80  // 80% to pass
#define VAIL_FEEDBACK_DURATION_MS 1500 // Feedback display time

// Auto-play timer for advancing after feedback / intro replays
static lv_timer_t* vail_course_autoplay_timer = NULL;

// Forward declarations
void updateVailCourseLessonUI();
void advanceVailCoursePhase();
void startVailCourseLessonPhase();
void playCurrentCharacter();
void checkVailCourseLessonAnswer(char answer);
void advanceVailCourseLessonItem();

// Cleanup Vail Course timers on back-navigation
void cleanupVailCourseLesson() {
    if (vail_course_autoplay_timer) {
        lv_timer_del(vail_course_autoplay_timer);
        vail_course_autoplay_timer = NULL;
    }
}

// Cancel any pending auto-play timer
static void cancelVailCourseAutoplayTimer() {
    if (vail_course_autoplay_timer) {
        lv_timer_del(vail_course_autoplay_timer);
        vail_course_autoplay_timer = NULL;
    }
}

// Auto-play callback: advance to next item and play it (SOLO/MIXED/GROUPS)
static void vail_course_autoplay_cb(lv_timer_t* timer) {
    lv_timer_del(timer);
    vail_course_autoplay_timer = NULL;
    advanceVailCourseLessonItem();
    // Only auto-play if we haven't moved to result phase
    if (vailCourseProgress.currentPhase != PHASE_RESULT) {
        playCurrentCharacter();
    }
    updateVailCourseLessonUI();
}

// Intro phase auto-replay callback: play character up to 3x then advance
static void vail_course_intro_timer_cb(lv_timer_t* timer) {
    lv_timer_del(timer);
    vail_course_autoplay_timer = NULL;

    if (lessonState.playbackCount < 3) {
        playCurrentCharacter();  // Plays and increments playbackCount
        // Schedule next replay
        vail_course_autoplay_timer = lv_timer_create(vail_course_intro_timer_cb, 1500, NULL);
    } else {
        // Done with 3 plays, advance to next character or phase
        advanceVailCourseLessonItem();
        updateVailCourseLessonUI();
        // If still in intro, start auto-play chain for the next character
        if (vailCourseProgress.currentPhase == PHASE_INTRO) {
            vail_course_autoplay_timer = lv_timer_create(vail_course_intro_timer_cb, 1000, NULL);
        }
    }
}

// Async playback functions from task_manager.h
extern void requestPlayMorseStringFarnsworth(const char* str, int characterWPM, int effectiveWPM, int toneHz);
extern bool isMorsePlaybackActive();
extern bool isMorsePlaybackComplete();
extern void cancelMorsePlayback();
extern void resetMorsePlayback();

// Get a random character from the available set
char getRandomVailCourseChar() {
    int len = strlen(lessonState.availableChars);
    if (len == 0) return 'E';
    int idx = random(len);
    return lessonState.availableChars[idx];
}

// Get characters for current lesson (fills buffer)
void getVailCourseLessonChars(char* buf, int bufSize) {
    VailCourseModule module = vailCourseProgress.currentModule;

    // For words and callsigns modules, use all letters
    if (module == MODULE_WORDS_COMMON || module == MODULE_CALLSIGNS) {
        strncpy(buf, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", bufSize - 1);
    } else {
        // Get all characters up to and including this module
        // getVailCourseCumulativeChars returns a String - convert immediately
        String chars = getVailCourseCumulativeChars(module);
        strncpy(buf, chars.c_str(), bufSize - 1);
    }
    buf[bufSize - 1] = '\0';
}

// Get the new characters for this module (for intro phase)
const char* getVailCourseNewChars() {
    return vailCourseModuleChars[vailCourseProgress.currentModule];
}

// Generate a random group of characters (fills buffer)
void generateVailCourseGroup(char* buf, int length) {
    for (int i = 0; i < length && i < 15; i++) {
        buf[i] = getRandomVailCourseChar();
    }
    buf[length < 15 ? length : 15] = '\0';
}

// ============================================
// Group Input Accumulation (for PHASE_GROUPS)
// ============================================

// Clear group input buffer
void clearVailCourseGroupInput() {
    lessonState.groupInputBuffer[0] = '\0';
}

// Add character to group input
void addVailCourseGroupInputChar(char c) {
    int len = strlen(lessonState.groupInputBuffer);
    if (len < (int)sizeof(lessonState.groupInputBuffer) - 1) {
        lessonState.groupInputBuffer[len] = toupper(c);
        lessonState.groupInputBuffer[len + 1] = '\0';
    }
    updateVailCourseLessonUI();
}

// Remove last character from group input
void backspaceVailCourseGroupInput() {
    int len = strlen(lessonState.groupInputBuffer);
    if (len > 0) {
        lessonState.groupInputBuffer[len - 1] = '\0';
        updateVailCourseLessonUI();
    }
}

// Submit group input for validation
void submitVailCourseGroupAnswer() {
    if (!lessonState.waitingForInput) return;
    if (strlen(lessonState.groupInputBuffer) == 0) return;

    lessonState.waitingForInput = false;
    lessonState.phaseTotal++;

    // Compare full group (case-insensitive)
    bool correct = (strcasecmp(lessonState.groupInputBuffer, lessonState.currentGroup) == 0);

    if (correct) {
        lessonState.phaseCorrect++;

        // Update mastery for each character in the group
        for (size_t i = 0; i < strlen(lessonState.currentGroup); i++) {
            int charIdx = getVailCourseCharIndex(lessonState.currentGroup[i]);
            if (charIdx >= 0) {
                vailCourseProgress.charMastery[charIdx].attempts++;
                vailCourseProgress.charMastery[charIdx].correct++;
                vailCourseProgress.charMastery[charIdx].mastery =
                    min(1000, vailCourseProgress.charMastery[charIdx].mastery + 50);
            }
        }
    } else {
        // Penalize all chars in group for incorrect
        for (size_t i = 0; i < strlen(lessonState.currentGroup); i++) {
            int charIdx = getVailCourseCharIndex(lessonState.currentGroup[i]);
            if (charIdx >= 0) {
                vailCourseProgress.charMastery[charIdx].attempts++;
                vailCourseProgress.charMastery[charIdx].mastery =
                    max(0, vailCourseProgress.charMastery[charIdx].mastery - 25);
            }
        }
    }

    // Update session stats
    vailCourseProgress.sessionTotal++;
    if (correct) vailCourseProgress.sessionCorrect++;

    // Show feedback
    lessonState.showingFeedback = true;
    lessonState.feedbackTime = millis();

    recordPracticeActivity();
    updateVailCourseLessonUI();

    // Auto-advance after feedback delay
    cancelVailCourseAutoplayTimer();
    vail_course_autoplay_timer = lv_timer_create(vail_course_autoplay_cb, 1200, NULL);
}

// ============================================
// Lesson Phase State Machine
// ============================================

void startVailCourseLessonPhase() {
    cancelVailCourseAutoplayTimer();
    VailCoursePhase phase = vailCourseProgress.currentPhase;

    lessonState.phaseItemIndex = 0;
    lessonState.phaseCorrect = 0;
    lessonState.phaseTotal = 0;
    lessonState.playbackCount = 0;
    lessonState.waitingForInput = false;
    lessonState.showingFeedback = false;
    clearVailCourseGroupInput();  // Clear group input buffer
    getVailCourseLessonChars(lessonState.availableChars, sizeof(lessonState.availableChars));

    switch (phase) {
        case PHASE_INTRO:
            {
                // Get NEW characters introduced in THIS lesson only
                String newChars = getVailCourseNewCharsForLesson(
                    vailCourseProgress.currentModule,
                    vailCourseProgress.currentLesson
                );

                lessonState.introCharIndex = 0;
                lessonState.phaseItemCount = newChars.length();

                if (lessonState.phaseItemCount == 0) {
                    // No new chars (review lesson or words/callsigns) - skip to solo
                    vailCourseProgress.currentPhase = PHASE_SOLO;
                    startVailCourseLessonPhase();
                    return;
                }

                // Store new chars for this intro phase
                strncpy(lessonState.availableChars, newChars.c_str(), sizeof(lessonState.availableChars) - 1);
                lessonState.availableChars[sizeof(lessonState.availableChars) - 1] = '\0';
                lessonState.currentChar = lessonState.availableChars[0];

                Serial.printf("[VailCourse] INTRO phase: %d new chars: %s\n",
                              lessonState.phaseItemCount, lessonState.availableChars);
            }
            break;

        case PHASE_SOLO:
            {
                // Practice ONLY newly introduced characters for THIS lesson
                String newChars = getVailCourseNewCharsForLesson(
                    vailCourseProgress.currentModule,
                    vailCourseProgress.currentLesson
                );

                lessonState.phaseItemCount = VAIL_LESSON_SOLO_COUNT;
                strncpy(lessonState.availableChars, newChars.c_str(), sizeof(lessonState.availableChars) - 1);
                lessonState.availableChars[sizeof(lessonState.availableChars) - 1] = '\0';

                if (strlen(lessonState.availableChars) == 0) {
                    // No new chars - skip to mixed
                    vailCourseProgress.currentPhase = PHASE_MIXED;
                    startVailCourseLessonPhase();
                    return;
                }

                lessonState.currentChar = getRandomVailCourseChar();

                Serial.printf("[VailCourse] SOLO phase: %zu chars available: %s\n",
                              strlen(lessonState.availableChars),
                              lessonState.availableChars);
            }
            break;

        case PHASE_MIXED:
            {
                // Practice ALL characters learned up to current lesson
                lessonState.phaseItemCount = VAIL_LESSON_MIXED_COUNT;
                {
                    String chars = getVailCourseCharsForLesson(
                        vailCourseProgress.currentModule,
                        vailCourseProgress.currentLesson
                    );
                    strncpy(lessonState.availableChars, chars.c_str(), sizeof(lessonState.availableChars) - 1);
                    lessonState.availableChars[sizeof(lessonState.availableChars) - 1] = '\0';
                }
                lessonState.currentChar = getRandomVailCourseChar();

                Serial.printf("[VailCourse] MIXED phase: %zu chars available: %s\n",
                              strlen(lessonState.availableChars),
                              lessonState.availableChars);
            }
            break;

        case PHASE_GROUPS:
            {
                // Practice character groups using all learned chars
                lessonState.phaseItemCount = VAIL_LESSON_GROUP_COUNT;
                {
                    String chars = getVailCourseCharsForLesson(
                        vailCourseProgress.currentModule,
                        vailCourseProgress.currentLesson
                    );
                    strncpy(lessonState.availableChars, chars.c_str(), sizeof(lessonState.availableChars) - 1);
                    lessonState.availableChars[sizeof(lessonState.availableChars) - 1] = '\0';
                }
                generateVailCourseGroup(lessonState.currentGroup, 2 + random(3)); // 2-4 chars

                Serial.printf("[VailCourse] GROUPS phase: %zu chars available: %s\n",
                              strlen(lessonState.availableChars),
                              lessonState.availableChars);
            }
            break;

        case PHASE_RESULT:
            // Show final results
            break;

        default:
            break;
    }

    updateVailCourseLessonUI();
    recordPracticeActivity(); // Mark activity at phase start
}

void advanceVailCoursePhase() {
    VailCoursePhase currentPhase = vailCourseProgress.currentPhase;

    // Calculate pass/fail for this phase
    int percentage = (lessonState.phaseTotal > 0)
        ? (lessonState.phaseCorrect * 100 / lessonState.phaseTotal)
        : 0;

    Serial.printf("[VailCourse] Phase %d complete: %d/%d (%d%%)\n",
                  (int)currentPhase, lessonState.phaseCorrect, lessonState.phaseTotal, percentage);

    // Advance to next phase
    switch (currentPhase) {
        case PHASE_INTRO:
            vailCourseProgress.currentPhase = PHASE_SOLO;
            break;
        case PHASE_SOLO:
            vailCourseProgress.currentPhase = PHASE_MIXED;
            break;
        case PHASE_MIXED:
            vailCourseProgress.currentPhase = PHASE_GROUPS;
            break;
        case PHASE_GROUPS:
            vailCourseProgress.currentPhase = PHASE_RESULT;
            break;
        case PHASE_RESULT:
            // Stay on result
            return;
        default:
            break;
    }

    startVailCourseLessonPhase();
}

void playCurrentCharacter() {
    VailCoursePhase phase = vailCourseProgress.currentPhase;
    int charWPM = vailCourseProgress.characterWPM;
    int effWPM = vailCourseProgress.effectiveWPM;

    if (phase == PHASE_GROUPS) {
        requestPlayMorseStringFarnsworth(lessonState.currentGroup, charWPM, effWPM, TONE_SIDETONE);
    } else {
        char charStr[2] = { lessonState.currentChar, '\0' };
        requestPlayMorseStringFarnsworth(charStr, charWPM, effWPM, TONE_SIDETONE);
    }

    lessonState.playbackCount++;
    lessonState.waitingForInput = true;
    recordPracticeActivity(); // Mark activity on playback
}

void checkVailCourseLessonAnswer(char answer) {
    if (!lessonState.waitingForInput) return;

    lessonState.waitingForInput = false;
    lessonState.phaseTotal++;

    VailCoursePhase phase = vailCourseProgress.currentPhase;
    bool correct = false;

    if (phase == PHASE_GROUPS) {
        // Groups use accumulation - shouldn't reach here
        // Handled by submitVailCourseGroupAnswer() instead
        Serial.println("[VailCourse] Error: checkAnswer called during PHASE_GROUPS");
        return;
    } else {
        correct = (toupper(answer) == lessonState.currentChar);
    }

    if (correct) {
        lessonState.phaseCorrect++;

        // Update mastery for this character
        int charIdx = getVailCourseCharIndex(lessonState.currentChar);
        if (charIdx >= 0) {
            vailCourseProgress.charMastery[charIdx].attempts++;
            vailCourseProgress.charMastery[charIdx].correct++;
            // Increase mastery (capped at 1000)
            vailCourseProgress.charMastery[charIdx].mastery =
                min(1000, vailCourseProgress.charMastery[charIdx].mastery + 50);
        }
    } else {
        // Update mastery for incorrect
        int charIdx = getVailCourseCharIndex(lessonState.currentChar);
        if (charIdx >= 0) {
            vailCourseProgress.charMastery[charIdx].attempts++;
            // Decrease mastery (but not below 0)
            vailCourseProgress.charMastery[charIdx].mastery =
                max(0, vailCourseProgress.charMastery[charIdx].mastery - 25);
        }
    }

    // Update session stats
    vailCourseProgress.sessionTotal++;
    if (correct) vailCourseProgress.sessionCorrect++;

    // Show feedback
    lessonState.showingFeedback = true;
    lessonState.feedbackTime = millis();

    recordPracticeActivity(); // Mark activity on answer

    updateVailCourseLessonUI();

    // Auto-advance after feedback delay
    cancelVailCourseAutoplayTimer();
    vail_course_autoplay_timer = lv_timer_create(vail_course_autoplay_cb, 1200, NULL);
}

void advanceVailCourseLessonItem() {
    lessonState.phaseItemIndex++;
    lessonState.playbackCount = 0;
    lessonState.showingFeedback = false;
    clearVailCourseGroupInput();  // Clear group input for next item

    VailCoursePhase phase = vailCourseProgress.currentPhase;

    // Check if phase is complete
    if (lessonState.phaseItemIndex >= lessonState.phaseItemCount) {
        advanceVailCoursePhase();
        return;
    }

    // Set up next item
    switch (phase) {
        case PHASE_INTRO:
            // Move to next new character in THIS lesson
            lessonState.introCharIndex++;
            if (lessonState.introCharIndex < (int)strlen(lessonState.availableChars)) {
                lessonState.currentChar = lessonState.availableChars[lessonState.introCharIndex];
            }
            break;

        case PHASE_SOLO:
        case PHASE_MIXED:
            lessonState.currentChar = getRandomVailCourseChar();
            break;

        case PHASE_GROUPS:
            generateVailCourseGroup(lessonState.currentGroup, 2 + random(3));
            break;

        default:
            break;
    }

    updateVailCourseLessonUI();
}

// ============================================
// Lesson Key Handler
// ============================================

static void vail_course_lesson_key_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    // Track key state to prevent processing same key twice (press + release)
    static uint32_t last_key = 0;
    static bool key_was_pressed = false;

    uint32_t key = lv_event_get_key(e);

    // Detect key release by checking if same key comes again
    if (key == last_key && key_was_pressed) {
        // This is the release event - ignore it
        key_was_pressed = false;
        last_key = 0;
        return;
    }

    // This is a new key press
    last_key = key;
    key_was_pressed = true;

    VailCoursePhase phase = vailCourseProgress.currentPhase;

    // Block TAB
    if (key == '\t' || key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }

    recordPracticeActivity(); // Record any key press as activity

    // Handle result phase
    if (phase == PHASE_RESULT) {
        if (key == LV_KEY_ENTER || key == ' ') {
            cancelVailCourseAutoplayTimer();
            // Calculate overall score
            int totalCorrect = vailCourseProgress.sessionCorrect;
            int totalTotal = vailCourseProgress.sessionTotal;
            int percentage = (totalTotal > 0) ? (totalCorrect * 100 / totalTotal) : 0;

            if (percentage >= VAIL_LESSON_PASS_THRESHOLD) {
                // Pass - unlock next lesson
                completeVailCourseLesson(vailCourseProgress.currentModule, vailCourseProgress.currentLesson);
                saveVailCourseProgress();
            }

            // Go back to lesson select
            endVailCourseSession();
            onLVGLMenuSelect(MODE_VAIL_COURSE_LESSON_SELECT);
        }
        return;
    }

    // Handle intro phase
    if (phase == PHASE_INTRO) {
        if (key == ' ' || key == LV_KEY_ENTER) {
            cancelVailCourseAutoplayTimer();
            if (!isMorsePlaybackActive()) {
                if (lessonState.playbackCount < 3) {
                    // Play character and start auto-replay chain
                    playCurrentCharacter();
                    vail_course_autoplay_timer = lv_timer_create(vail_course_intro_timer_cb, 1500, NULL);
                } else {
                    // Done with this character, move to next
                    advanceVailCourseLessonItem();
                    updateVailCourseLessonUI();
                    // If still in intro, start auto-play for the next character
                    if (vailCourseProgress.currentPhase == PHASE_INTRO) {
                        vail_course_autoplay_timer = lv_timer_create(vail_course_intro_timer_cb, 1000, NULL);
                    }
                }
            }
        }
        return;
    }

    // Handle practice phases (SOLO, MIXED, GROUPS)
    if (lessonState.showingFeedback) {
        // During feedback, space/enter cancels auto-advance and immediately advances
        if (key == ' ' || key == LV_KEY_ENTER) {
            cancelVailCourseAutoplayTimer();
            advanceVailCourseLessonItem();
            // Auto-play the next item immediately
            if (vailCourseProgress.currentPhase != PHASE_RESULT) {
                playCurrentCharacter();
            }
            updateVailCourseLessonUI();
        }
        return;
    }

    if (!lessonState.waitingForInput) {
        // Not waiting - space/enter plays character (manual replay)
        if (key == ' ' || key == LV_KEY_ENTER) {
            cancelVailCourseAutoplayTimer();
            if (!isMorsePlaybackActive()) {
                playCurrentCharacter();
            }
        }
        return;
    }

    // Waiting for input
    if (vailCourseProgress.currentPhase == PHASE_GROUPS) {
        // Group input mode: accumulate characters
        if (isalnum(key) || key == '.' || key == ',' || key == '?' || key == '/') {
            addVailCourseGroupInputChar((char)key);
        } else if (key == LV_KEY_BACKSPACE || key == '\b') {
            backspaceVailCourseGroupInput();
        } else if (key == LV_KEY_ENTER) {
            // ENTER submits group answer (space is reserved for playback)
            submitVailCourseGroupAnswer();
        }
    } else {
        // Single character mode: immediate validation
        if (isalnum(key) || key == '.' || key == ',' || key == '?' || key == '/') {
            checkVailCourseLessonAnswer((char)key);
        }
    }
}

// ============================================
// Lesson UI Update
// ============================================

void updateVailCourseLessonUI() {
    if (!lessonState.screen) return;

    VailCoursePhase phase = vailCourseProgress.currentPhase;

    // Update phase label
    if (lessonState.phase_label) {
        lv_label_set_text(lessonState.phase_label, vailCoursePhaseNames[phase]);
    }

    // Update progress label
    if (lessonState.progress_label) {
        if (phase != PHASE_RESULT) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d/%d", lessonState.phaseItemIndex + 1, lessonState.phaseItemCount);
            lv_label_set_text(lessonState.progress_label, buf);
        } else {
            lv_label_set_text(lessonState.progress_label, "");
        }
    }

    // Update main display based on phase
    if (lessonState.main_label) {
        switch (phase) {
            case PHASE_INTRO:
                {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%c", lessonState.currentChar);
                    lv_label_set_text(lessonState.main_label, buf);
                    lv_obj_set_style_text_color(lessonState.main_label, LV_COLOR_ACCENT_CYAN, 0);
                }
                break;

            case PHASE_SOLO:
            case PHASE_MIXED:
                if (lessonState.showingFeedback) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%c", lessonState.currentChar);
                    lv_label_set_text(lessonState.main_label, buf);
                } else if (lessonState.waitingForInput) {
                    lv_label_set_text(lessonState.main_label, "?");
                    lv_obj_set_style_text_color(lessonState.main_label, LV_COLOR_WARNING, 0);
                } else {
                    lv_label_set_text(lessonState.main_label, "...");
                    lv_obj_set_style_text_color(lessonState.main_label, LV_COLOR_TEXT_SECONDARY, 0);
                }
                break;

            case PHASE_GROUPS:
                if (lessonState.showingFeedback) {
                    // Show correct answer
                    lv_label_set_text(lessonState.main_label, lessonState.currentGroup);

                    // Show what user typed for comparison
                    if (lessonState.group_input_label) {
                        char inputText[32];
                        snprintf(inputText, sizeof(inputText), "You typed: %s", lessonState.groupInputBuffer);
                        lv_label_set_text(lessonState.group_input_label, inputText);
                        lv_obj_clear_flag(lessonState.group_input_label, LV_OBJ_FLAG_HIDDEN);
                    }
                } else if (lessonState.waitingForInput) {
                    lv_label_set_text(lessonState.main_label, "???");
                    lv_obj_set_style_text_color(lessonState.main_label, LV_COLOR_WARNING, 0);

                    // Show current input with cursor
                    if (lessonState.group_input_label) {
                        char inputDisplay[32];
                        if (strlen(lessonState.groupInputBuffer) == 0) {
                            strcpy(inputDisplay, "(Type answer)");
                        } else {
                            snprintf(inputDisplay, sizeof(inputDisplay), "%s_", lessonState.groupInputBuffer);
                        }
                        lv_label_set_text(lessonState.group_input_label, inputDisplay);
                        lv_obj_set_style_text_color(lessonState.group_input_label, LV_COLOR_ACCENT_CYAN, 0);
                        lv_obj_clear_flag(lessonState.group_input_label, LV_OBJ_FLAG_HIDDEN);
                    }
                } else {
                    lv_label_set_text(lessonState.main_label, "...");
                    lv_obj_set_style_text_color(lessonState.main_label, LV_COLOR_TEXT_SECONDARY, 0);

                    if (lessonState.group_input_label) {
                        lv_obj_add_flag(lessonState.group_input_label, LV_OBJ_FLAG_HIDDEN);
                    }
                }
                break;

            case PHASE_RESULT:
                {
                    int totalCorrect = vailCourseProgress.sessionCorrect;
                    int totalTotal = vailCourseProgress.sessionTotal;
                    int percentage = (totalTotal > 0) ? (totalCorrect * 100 / totalTotal) : 0;

                    if (percentage >= VAIL_LESSON_PASS_THRESHOLD) {
                        lv_label_set_text(lessonState.main_label, "PASS!");
                        lv_obj_set_style_text_color(lessonState.main_label, LV_COLOR_SUCCESS, 0);
                    } else {
                        lv_label_set_text(lessonState.main_label, "TRY AGAIN");
                        lv_obj_set_style_text_color(lessonState.main_label, LV_COLOR_ERROR, 0);
                    }
                }
                break;

            default:
                break;
        }
    }

    // Update feedback label
    if (lessonState.feedback_label) {
        if (lessonState.showingFeedback) {
            bool lastWasCorrect = (lessonState.phaseCorrect == lessonState.phaseTotal);
            if (lastWasCorrect) {
                lv_label_set_text(lessonState.feedback_label, "Correct!");
                lv_obj_set_style_text_color(lessonState.feedback_label, LV_COLOR_SUCCESS, 0);
                lv_obj_set_style_text_color(lessonState.main_label, LV_COLOR_SUCCESS, 0);
            } else {
                lv_label_set_text(lessonState.feedback_label, "Incorrect");
                lv_obj_set_style_text_color(lessonState.feedback_label, LV_COLOR_ERROR, 0);
                lv_obj_set_style_text_color(lessonState.main_label, LV_COLOR_ERROR, 0);
            }
            lv_obj_clear_flag(lessonState.feedback_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(lessonState.feedback_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Update score label
    if (lessonState.score_label) {
        if (phase == PHASE_RESULT) {
            int totalCorrect = vailCourseProgress.sessionCorrect;
            int totalTotal = vailCourseProgress.sessionTotal;
            int percentage = (totalTotal > 0) ? (totalCorrect * 100 / totalTotal) : 0;
            char buf[64];
            snprintf(buf, sizeof(buf), "%d/%d correct (%d%%)", totalCorrect, totalTotal, percentage);
            lv_label_set_text(lessonState.score_label, buf);
        } else if (lessonState.phaseTotal > 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d/%d", lessonState.phaseCorrect, lessonState.phaseTotal);
            lv_label_set_text(lessonState.score_label, buf);
        } else {
            lv_label_set_text(lessonState.score_label, "");
        }
    }

    // Update prompt label
    if (lessonState.prompt_label) {
        switch (phase) {
            case PHASE_INTRO:
                if (lessonState.playbackCount == 0) {
                    lv_label_set_text(lessonState.prompt_label, "Press SPACE to hear this character");
                } else if (lessonState.playbackCount < 3) {
                    lv_label_set_text(lessonState.prompt_label, "Listening...");
                } else {
                    lv_label_set_text(lessonState.prompt_label, "Moving to next...");
                }
                break;

            case PHASE_SOLO:
            case PHASE_MIXED:
                if (lessonState.showingFeedback) {
                    lv_label_set_text(lessonState.prompt_label, "");
                } else if (lessonState.waitingForInput) {
                    lv_label_set_text(lessonState.prompt_label, "Type your answer");
                } else {
                    lv_label_set_text(lessonState.prompt_label, "Press SPACE to play");
                }
                break;

            case PHASE_GROUPS:
                if (lessonState.showingFeedback) {
                    lv_label_set_text(lessonState.prompt_label, "");
                } else if (lessonState.waitingForInput) {
                    lv_label_set_text(lessonState.prompt_label, "Type full group, then ENTER to submit");
                } else {
                    lv_label_set_text(lessonState.prompt_label, "Press SPACE to play");
                }
                break;

            case PHASE_RESULT:
                lv_label_set_text(lessonState.prompt_label, "Press ENTER to continue");
                break;

            default:
                break;
        }
    }

    // Update footer
    if (lessonState.footer_label) {
        if (phase == PHASE_RESULT) {
            lv_label_set_text(lessonState.footer_label, "ENTER Continue   ESC Back");
        } else if (phase == PHASE_GROUPS) {
            lv_label_set_text(lessonState.footer_label, "Type Group   ENTER Submit   SPACE Replay   ESC Back");
        } else {
            lv_label_set_text(lessonState.footer_label, FOOTER_TRAINING_AUTOPLAY);
        }
    }
}

// ============================================
// Lesson Screen Creation
// ============================================

lv_obj_t* createVailCourseLessonScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    lessonState.screen = screen;

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    char titleText[64];
    snprintf(titleText, sizeof(titleText), "%s - Lesson %d",
             vailCourseModuleNames[vailCourseProgress.currentModule],
             vailCourseProgress.currentLesson);
    lv_label_set_text(title, titleText);
    lv_obj_set_style_text_font(title, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Phase indicator
    lessonState.phase_label = lv_label_create(header);
    lv_label_set_text(lessonState.phase_label, vailCoursePhaseNames[vailCourseProgress.currentPhase]);
    lv_obj_set_style_text_font(lessonState.phase_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(lessonState.phase_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(lessonState.phase_label, LV_ALIGN_RIGHT_MID, -15, 0);

    // Main content area
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, 420, 180);
    lv_obj_center(content);
    applyCardStyle(content);
    lv_obj_set_style_pad_all(content, 15, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // Progress indicator (top right of content)
    lessonState.progress_label = lv_label_create(content);
    lv_label_set_text(lessonState.progress_label, "1/5");
    lv_obj_set_style_text_font(lessonState.progress_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(lessonState.progress_label, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(lessonState.progress_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    // Main character display (large, centered)
    lessonState.main_label = lv_label_create(content);
    lv_label_set_text(lessonState.main_label, "...");
    lv_obj_set_style_text_font(lessonState.main_label, getThemeFonts()->font_title, 0);
    lv_obj_set_style_text_color(lessonState.main_label, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(lessonState.main_label, LV_ALIGN_CENTER, 0, -15);

    // Feedback label (below main)
    lessonState.feedback_label = lv_label_create(content);
    lv_label_set_text(lessonState.feedback_label, "");
    lv_obj_set_style_text_font(lessonState.feedback_label, getThemeFonts()->font_input, 0);
    lv_obj_align(lessonState.feedback_label, LV_ALIGN_CENTER, 0, 30);
    lv_obj_add_flag(lessonState.feedback_label, LV_OBJ_FLAG_HIDDEN);

    // Group input display (for PHASE_GROUPS, below main label)
    lessonState.group_input_label = lv_label_create(content);
    lv_label_set_text(lessonState.group_input_label, "");
    lv_obj_set_style_text_font(lessonState.group_input_label, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(lessonState.group_input_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(lessonState.group_input_label, LV_ALIGN_CENTER, 0, 15);
    lv_obj_add_flag(lessonState.group_input_label, LV_OBJ_FLAG_HIDDEN);

    // Score label (bottom left)
    lessonState.score_label = lv_label_create(content);
    lv_label_set_text(lessonState.score_label, "");
    lv_obj_set_style_text_font(lessonState.score_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(lessonState.score_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lessonState.score_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Prompt label (bottom center)
    lessonState.prompt_label = lv_label_create(content);
    lv_label_set_text(lessonState.prompt_label, "Press SPACE to start");
    lv_obj_set_style_text_font(lessonState.prompt_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(lessonState.prompt_label, LV_COLOR_WARNING, 0);
    lv_obj_align(lessonState.prompt_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Invisible focus container for keyboard input
    lv_obj_t* focus = lv_obj_create(screen);
    lv_obj_set_size(focus, 1, 1);
    lv_obj_set_pos(focus, -10, -10);
    lv_obj_set_style_bg_opa(focus, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus, 0, 0);
    lv_obj_set_style_outline_width(focus, 0, 0);
    lv_obj_set_style_outline_width(focus, 0, LV_STATE_FOCUSED);
    lv_obj_add_flag(focus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus, vail_course_lesson_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus);

    // Footer
    lessonState.footer_label = lv_label_create(screen);
    lv_label_set_text(lessonState.footer_label, FOOTER_TRAINING_AUTOPLAY);
    lv_obj_set_style_text_font(lessonState.footer_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(lessonState.footer_label, LV_COLOR_WARNING, 0);
    lv_obj_align(lessonState.footer_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    // Initialize lesson state and start first phase
    startVailCourseSession();
    vailCourseProgress.currentPhase = PHASE_INTRO;
    startVailCourseLessonPhase();

    return screen;
}

// ============================================
// Progress Overview Screen
// ============================================

lv_obj_t* createVailCourseProgressScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Vail CW Course - Progress");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Stats container
    lv_obj_t* stats = lv_obj_create(screen);
    lv_obj_set_size(stats, 400, 180);
    lv_obj_center(stats);
    lv_obj_set_style_bg_color(stats, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(stats, 1, 0);
    lv_obj_set_style_border_color(stats, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_radius(stats, 10, 0);
    lv_obj_set_style_pad_all(stats, 20, 0);
    lv_obj_clear_flag(stats, LV_OBJ_FLAG_SCROLLABLE);

    // Count completed modules
    int modulesComplete = 0;
    for (int i = 0; i < MODULE_COUNT; i++) {
        if (isVailCourseModuleCompleted((VailCourseModule)i)) modulesComplete++;
    }

    // Stats text
    char statsBuf[256];
    snprintf(statsBuf, sizeof(statsBuf),
             "Modules Completed: %d / %d\n\n"
             "Current Module: %s\n"
             "Current Lesson: %d\n\n"
             "Practice Time Today: %s\n"
             "Total Practice Time: %s",
             modulesComplete, MODULE_COUNT,
             vailCourseModuleNames[vailCourseProgress.currentModule],
             vailCourseProgress.currentLesson,
             formatPracticeTime(getTodayPracticeSeconds()).c_str(),
             formatPracticeTime(getTotalPracticeSeconds()).c_str());

    lv_obj_t* stats_label = lv_label_create(stats);
    lv_label_set_text(stats_label, statsBuf);
    lv_obj_set_style_text_font(stats_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(stats_label, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(stats_label);

    // Invisible focusable for ESC
    lv_obj_t* focus = lv_obj_create(screen);
    lv_obj_set_size(focus, 1, 1);
    lv_obj_set_style_bg_opa(focus, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus, 0, 0);
    lv_obj_add_flag(focus, LV_OBJ_FLAG_CLICKABLE);
    addNavigableWidget(focus);

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "ESC Back");
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -5);

    return screen;
}

// ============================================
// Mode Handler Integration
// ============================================

/*
 * Handle Vail Course mode navigation
 * Called from main mode handler in lv_mode_integration.h
 */
bool handleVailCourseMode(int mode) {
    // Cancel any pending auto-play timer when switching modes
    cancelVailCourseAutoplayTimer();

    lv_obj_t* screen = NULL;

    switch (mode) {
        case MODE_VAIL_COURSE_MODULE_SELECT:
            screen = createVailCourseModuleSelectScreen();
            break;

        case MODE_VAIL_COURSE_LESSON_SELECT:
            screen = createVailCourseLessonSelectScreen();
            break;

        case MODE_VAIL_COURSE_LESSON:
            screen = createVailCourseLessonScreen();
            break;

        case MODE_VAIL_COURSE_PROGRESS:
            screen = createVailCourseProgressScreen();
            break;

        default:
            return false;  // Not a Vail Course mode
    }

    if (screen) {
        loadScreen(screen, SCREEN_ANIM_FADE);
        return true;
    }

    return false;
}

#endif // LV_VAIL_COURSE_SCREENS_H
