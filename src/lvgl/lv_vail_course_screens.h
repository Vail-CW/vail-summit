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

// ============================================
// Linear Navigation Handler
// ============================================

static void vail_course_linear_nav_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    // Block TAB and horizontal navigation in vertical lists
    if (key == '\t' || key == LV_KEY_NEXT || key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        lv_event_stop_processing(e);
        return;
    }

    // Auto-scroll to focused item on vertical navigation
    if (key == LV_KEY_UP || key == LV_KEY_DOWN || key == LV_KEY_PREV) {
        lv_obj_t* target = lv_event_get_target(e);
        if (target) {
            lv_obj_scroll_to_view(target, LV_ANIM_ON);
        }
    }
}

// ============================================
// Module Grid Navigation (3-column grid)
// ============================================

static void vail_course_grid_nav_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    // Block TAB
    if (key == '\t' || key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }

    lv_obj_t* current = lv_event_get_target(e);
    if (!current) return;

    // Find current index
    int currentIdx = -1;
    for (int i = 0; i < MODULE_COUNT; i++) {
        if (vail_course_module_buttons[i] == current) {
            currentIdx = i;
            break;
        }
    }
    if (currentIdx < 0) return;

    int newIdx = currentIdx;
    int cols = 3;

    // Handle arrow keys for 3-column grid
    if (key == LV_KEY_LEFT) {
        if (currentIdx % cols > 0) newIdx = currentIdx - 1;
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_RIGHT) {
        if (currentIdx % cols < cols - 1 && currentIdx < MODULE_COUNT - 1) newIdx = currentIdx + 1;
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        if (currentIdx >= cols) newIdx = currentIdx - cols;
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_DOWN) {
        if (currentIdx + cols < MODULE_COUNT) newIdx = currentIdx + cols;
        lv_event_stop_processing(e);
    }

    // Focus new button
    if (newIdx != currentIdx && vail_course_module_buttons[newIdx]) {
        lv_group_t* group = getLVGLInputGroup();
        if (group) {
            lv_group_focus_obj(vail_course_module_buttons[newIdx]);
            lv_obj_scroll_to_view(vail_course_module_buttons[newIdx], LV_ANIM_ON);
        }
    }
}

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
    onLVGLMenuSelect(161);  // LVGL_MODE_VAIL_COURSE_LESSON_SELECT
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
    for (int i = 0; i < MODULE_COUNT; i++) {
        bool unlocked = isVailCourseModuleUnlocked((VailCourseModule)i);
        bool completed = isVailCourseModuleCompleted((VailCourseModule)i);

        lv_obj_t* btn = lv_btn_create(grid);
        lv_obj_set_size(btn, 145, 45);

        if (unlocked) {
            if (completed) {
                lv_obj_set_style_bg_color(btn, LV_COLOR_SUCCESS, 0);
                lv_obj_set_style_bg_color(btn, LV_COLOR_ACCENT_GREEN, LV_STATE_FOCUSED);
            } else {
                lv_obj_set_style_bg_color(btn, LV_COLOR_CARD_BLUE, 0);
                lv_obj_set_style_bg_color(btn, LV_COLOR_CARD_CYAN, LV_STATE_FOCUSED);
            }
        } else {
            lv_obj_set_style_bg_color(btn, LV_COLOR_TEXT_DISABLED, 0);
            lv_obj_add_state(btn, LV_STATE_DISABLED);
        }

        lv_obj_set_style_radius(btn, 8, 0);

        // Module name
        lv_obj_t* lbl = lv_label_create(btn);
        String text = vailCourseModuleNames[i];
        if (completed) text = LV_SYMBOL_OK " " + text;
        else if (!unlocked) text = LV_SYMBOL_CLOSE " " + text;
        lv_label_set_text(lbl, text.c_str());
        lv_obj_set_style_text_font(lbl, getThemeFonts()->font_body, 0);
        lv_obj_center(lbl);

        if (unlocked) {
            lv_obj_add_event_cb(btn, vail_course_module_click_handler, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            lv_obj_add_event_cb(btn, vail_course_grid_nav_handler, LV_EVENT_KEY, NULL);
            addNavigableWidget(btn);
        }

        vail_course_module_buttons[i] = btn;
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
    onLVGLMenuSelect(162);  // LVGL_MODE_VAIL_COURSE_LESSON
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
    String titleText = String(vailCourseModuleNames[module]) + " - Lessons";
    lv_label_set_text(title, titleText.c_str());
    lv_obj_set_style_text_font(title, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Characters info
    lv_obj_t* chars_label = lv_label_create(header);
    String charsText = "Chars: " + String(vailCourseModuleChars[module]);
    if (strlen(vailCourseModuleChars[module]) == 0) {
        charsText = "Review";
    }
    lv_label_set_text(chars_label, charsText.c_str());
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

        if (completed) {
            lv_obj_set_style_bg_color(btn, LV_COLOR_SUCCESS, 0);
            lv_obj_set_style_bg_color(btn, LV_COLOR_ACCENT_GREEN, LV_STATE_FOCUSED);
        } else if (current) {
            lv_obj_set_style_bg_color(btn, LV_COLOR_CARD_CYAN, 0);
            lv_obj_set_style_bg_color(btn, LV_COLOR_CARD_BLUE, LV_STATE_FOCUSED);
        } else {
            lv_obj_set_style_bg_color(btn, LV_COLOR_CARD_BLUE, 0);
            lv_obj_set_style_bg_color(btn, LV_COLOR_CARD_CYAN, LV_STATE_FOCUSED);
        }
        lv_obj_set_style_radius(btn, 8, 0);

        lv_obj_t* lbl = lv_label_create(btn);
        String lessonText = "Lesson " + String(i);
        if (completed) lessonText = LV_SYMBOL_OK " " + lessonText;
        else if (current) lessonText += " (Current)";
        lv_label_set_text(lbl, lessonText.c_str());
        lv_obj_set_style_text_font(lbl, getThemeFonts()->font_input, 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, vail_course_lesson_click_handler, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, vail_course_linear_nav_handler, LV_EVENT_KEY, NULL);
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
    String currentGroup;         // Character group (for PHASE_GROUPS)
    String availableChars;       // Characters available for this lesson

    // Playback state
    int playbackCount;           // Times character has been played
    bool waitingForInput;        // Waiting for user's answer
    bool showingFeedback;        // Showing correct/incorrect feedback
    unsigned long feedbackTime;  // When feedback started

    // Intro phase state
    int introCharIndex;          // Which new character we're introducing

    // Group input accumulation (for PHASE_GROUPS)
    String groupInputBuffer;     // Accumulated characters for group answer

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

// Forward declarations
void updateVailCourseLessonUI();
void advanceVailCoursePhase();
void startVailCourseLessonPhase();
void playCurrentCharacter();
void checkVailCourseLessonAnswer(char answer);

// Async playback functions from task_manager.h
extern void requestPlayMorseStringFarnsworth(const char* str, int characterWPM, int effectiveWPM, int toneHz);
extern bool isMorsePlaybackActive();
extern bool isMorsePlaybackComplete();
extern void cancelMorsePlayback();
extern void resetMorsePlayback();

// Get a random character from the available set
char getRandomVailCourseChar() {
    if (lessonState.availableChars.length() == 0) return 'E';
    int idx = random(lessonState.availableChars.length());
    return lessonState.availableChars[idx];
}

// Get characters for current lesson
String getVailCourseLessonChars() {
    VailCourseModule module = vailCourseProgress.currentModule;

    // Get all characters up to and including this module
    String chars = getVailCourseCumulativeChars(module);

    // For words and callsigns modules, use all letters
    if (module == MODULE_WORDS_COMMON || module == MODULE_CALLSIGNS) {
        chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    }

    return chars;
}

// Get the new characters for this module (for intro phase)
String getVailCourseNewChars() {
    return String(vailCourseModuleChars[vailCourseProgress.currentModule]);
}

// Generate a random group of characters
String generateVailCourseGroup(int length) {
    String group = "";
    for (int i = 0; i < length; i++) {
        group += getRandomVailCourseChar();
    }
    return group;
}

// ============================================
// Group Input Accumulation (for PHASE_GROUPS)
// ============================================

// Clear group input buffer
void clearVailCourseGroupInput() {
    lessonState.groupInputBuffer = "";
}

// Add character to group input
void addVailCourseGroupInputChar(char c) {
    lessonState.groupInputBuffer += toupper(c);
    updateVailCourseLessonUI();
}

// Remove last character from group input
void backspaceVailCourseGroupInput() {
    if (lessonState.groupInputBuffer.length() > 0) {
        lessonState.groupInputBuffer.remove(lessonState.groupInputBuffer.length() - 1);
        updateVailCourseLessonUI();
    }
}

// Submit group input for validation
void submitVailCourseGroupAnswer() {
    if (!lessonState.waitingForInput) return;
    if (lessonState.groupInputBuffer.length() == 0) return;

    lessonState.waitingForInput = false;
    lessonState.phaseTotal++;

    // Compare full group (case-insensitive)
    bool correct = lessonState.groupInputBuffer.equalsIgnoreCase(lessonState.currentGroup);

    if (correct) {
        lessonState.phaseCorrect++;

        // Update mastery for each character in the group
        for (size_t i = 0; i < lessonState.currentGroup.length(); i++) {
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
        for (size_t i = 0; i < lessonState.currentGroup.length(); i++) {
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
}

// ============================================
// Lesson Phase State Machine
// ============================================

void startVailCourseLessonPhase() {
    VailCoursePhase phase = vailCourseProgress.currentPhase;

    lessonState.phaseItemIndex = 0;
    lessonState.phaseCorrect = 0;
    lessonState.phaseTotal = 0;
    lessonState.playbackCount = 0;
    lessonState.waitingForInput = false;
    lessonState.showingFeedback = false;
    clearVailCourseGroupInput();  // Clear group input buffer
    lessonState.availableChars = getVailCourseLessonChars();

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
                lessonState.availableChars = newChars;
                lessonState.currentChar = newChars[0];

                Serial.printf("[VailCourse] INTRO phase: %d new chars: %s\n",
                              lessonState.phaseItemCount, newChars.c_str());
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
                lessonState.availableChars = newChars;

                if (lessonState.availableChars.length() == 0) {
                    // No new chars - skip to mixed
                    vailCourseProgress.currentPhase = PHASE_MIXED;
                    startVailCourseLessonPhase();
                    return;
                }

                lessonState.currentChar = getRandomVailCourseChar();

                Serial.printf("[VailCourse] SOLO phase: %d chars available: %s\n",
                              lessonState.availableChars.length(),
                              lessonState.availableChars.c_str());
            }
            break;

        case PHASE_MIXED:
            {
                // Practice ALL characters learned up to current lesson
                lessonState.phaseItemCount = VAIL_LESSON_MIXED_COUNT;
                lessonState.availableChars = getVailCourseCharsForLesson(
                    vailCourseProgress.currentModule,
                    vailCourseProgress.currentLesson
                );
                lessonState.currentChar = getRandomVailCourseChar();

                Serial.printf("[VailCourse] MIXED phase: %d chars available: %s\n",
                              lessonState.availableChars.length(),
                              lessonState.availableChars.c_str());
            }
            break;

        case PHASE_GROUPS:
            {
                // Practice character groups using all learned chars
                lessonState.phaseItemCount = VAIL_LESSON_GROUP_COUNT;
                lessonState.availableChars = getVailCourseCharsForLesson(
                    vailCourseProgress.currentModule,
                    vailCourseProgress.currentLesson
                );
                lessonState.currentGroup = generateVailCourseGroup(2 + random(3)); // 2-4 chars

                Serial.printf("[VailCourse] GROUPS phase: %d chars available: %s\n",
                              lessonState.availableChars.length(),
                              lessonState.availableChars.c_str());
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
        requestPlayMorseStringFarnsworth(lessonState.currentGroup.c_str(), charWPM, effWPM, TONE_SIDETONE);
    } else {
        String charStr;
        charStr += lessonState.currentChar;
        requestPlayMorseStringFarnsworth(charStr.c_str(), charWPM, effWPM, TONE_SIDETONE);
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
            if (lessonState.introCharIndex < lessonState.availableChars.length()) {
                lessonState.currentChar = lessonState.availableChars[lessonState.introCharIndex];
            }
            break;

        case PHASE_SOLO:
        case PHASE_MIXED:
            lessonState.currentChar = getRandomVailCourseChar();
            break;

        case PHASE_GROUPS:
            lessonState.currentGroup = generateVailCourseGroup(2 + random(3));
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

    uint32_t key = lv_event_get_key(e);
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
            onLVGLMenuSelect(161);  // Lesson select
        }
        return;
    }

    // Handle intro phase
    if (phase == PHASE_INTRO) {
        if (key == ' ' || key == LV_KEY_ENTER) {
            if (!isMorsePlaybackActive()) {
                if (lessonState.playbackCount < 3) {
                    // Play character
                    playCurrentCharacter();
                } else {
                    // Done with this character, move to next
                    advanceVailCourseLessonItem();
                }
            }
        }
        return;
    }

    // Handle practice phases (SOLO, MIXED, GROUPS)
    if (lessonState.showingFeedback) {
        // During feedback, space/enter advances
        if (key == ' ' || key == LV_KEY_ENTER) {
            advanceVailCourseLessonItem();
        }
        return;
    }

    if (!lessonState.waitingForInput) {
        // Not waiting - space/enter plays character
        if (key == ' ' || key == LV_KEY_ENTER) {
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
                    lv_label_set_text(lessonState.main_label, lessonState.currentGroup.c_str());

                    // Show what user typed for comparison
                    if (lessonState.group_input_label) {
                        String inputText = "You typed: " + lessonState.groupInputBuffer;
                        lv_label_set_text(lessonState.group_input_label, inputText.c_str());
                        lv_obj_clear_flag(lessonState.group_input_label, LV_OBJ_FLAG_HIDDEN);
                    }
                } else if (lessonState.waitingForInput) {
                    lv_label_set_text(lessonState.main_label, "???");
                    lv_obj_set_style_text_color(lessonState.main_label, LV_COLOR_WARNING, 0);

                    // Show current input with cursor
                    if (lessonState.group_input_label) {
                        String inputDisplay = lessonState.groupInputBuffer;
                        if (inputDisplay.length() == 0) {
                            inputDisplay = "(Type answer)";
                        } else {
                            inputDisplay += "_";  // Cursor indicator
                        }
                        lv_label_set_text(lessonState.group_input_label, inputDisplay.c_str());
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
                    lv_label_set_text(lessonState.prompt_label, "Press SPACE to hear again");
                } else {
                    lv_label_set_text(lessonState.prompt_label, "Press SPACE to continue");
                }
                break;

            case PHASE_SOLO:
            case PHASE_MIXED:
                if (lessonState.showingFeedback) {
                    lv_label_set_text(lessonState.prompt_label, "Press SPACE to continue");
                } else if (lessonState.waitingForInput) {
                    lv_label_set_text(lessonState.prompt_label, "Type your answer");
                } else {
                    lv_label_set_text(lessonState.prompt_label, "Press SPACE to play");
                }
                break;

            case PHASE_GROUPS:
                if (lessonState.showingFeedback) {
                    lv_label_set_text(lessonState.prompt_label, "Press SPACE to continue");
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
        } else {
            lv_label_set_text(lessonState.footer_label, "SPACE Play   Type Answer   ESC Back");
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
    String titleText = String(vailCourseModuleNames[vailCourseProgress.currentModule]) +
                       " - Lesson " + String(vailCourseProgress.currentLesson);
    lv_label_set_text(title, titleText.c_str());
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
    lv_obj_set_style_bg_color(content, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(content, 1, 0);
    lv_obj_set_style_border_color(content, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_radius(content, 10, 0);
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
    lv_label_set_text(lessonState.footer_label, "SPACE Play   Type Answer   ESC Back");
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
    lv_obj_t* screen = NULL;

    switch (mode) {
        case 160:  // LVGL_MODE_VAIL_COURSE_MODULE_SELECT
            screen = createVailCourseModuleSelectScreen();
            break;

        case 161:  // LVGL_MODE_VAIL_COURSE_LESSON_SELECT
            screen = createVailCourseLessonSelectScreen();
            break;

        case 162:  // LVGL_MODE_VAIL_COURSE_LESSON
            screen = createVailCourseLessonScreen();
            break;

        case 163:  // LVGL_MODE_VAIL_COURSE_PROGRESS
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
