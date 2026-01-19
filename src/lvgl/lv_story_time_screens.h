/*
 * VAIL SUMMIT - Morse Story Time LVGL Screens
 * Provides all UI screens for the Story Time game
 */

#ifndef LV_STORY_TIME_SCREENS_H
#define LV_STORY_TIME_SCREENS_H

#include <lvgl.h>
#include "lv_theme_summit.h"
#include "lv_widgets_summit.h"
#include "lv_screen_manager.h"
#include "../core/config.h"
#include "../games/game_story_time.h"
#include "../games/game_story_time_data.h"

// Forward declarations
extern void onLVGLBackNavigation();
extern void onLVGLMenuSelect(int target_mode);
extern int cwTone;
extern int cwSpeed;

// Mode constants (must match lv_mode_integration.h)
#define LVGL_MODE_STORY_TIME             89
#define LVGL_MODE_STORY_TIME_DIFFICULTY  90
#define LVGL_MODE_STORY_TIME_LIST        91
#define LVGL_MODE_STORY_TIME_LISTEN      92
#define LVGL_MODE_STORY_TIME_QUIZ        93
#define LVGL_MODE_STORY_TIME_RESULTS     94
#define LVGL_MODE_STORY_TIME_PROGRESS    95
#define LVGL_MODE_STORY_TIME_SETTINGS    96

// ============================================
// Screen State Variables
// ============================================

static lv_obj_t* st_screen = NULL;
static lv_obj_t* st_status_label = NULL;
static lv_obj_t* st_message_label = NULL;
static lv_obj_t* st_progress_label = NULL;

// Quiz screen elements
static lv_obj_t* st_question_label = NULL;
static lv_obj_t* st_answer_btns[4] = {NULL, NULL, NULL, NULL};
static lv_obj_t* st_question_progress = NULL;

// Results screen elements
static lv_obj_t* st_score_label = NULL;
static lv_obj_t* st_result_labels[5] = {NULL};

// Settings screen elements
static lv_obj_t* st_wpm_label = NULL;
static lv_obj_t* st_tone_label = NULL;

// Story list scroll position
static int st_scroll_index = 0;
static int st_selected_diff_index = 0;

// ============================================
// Cleanup Functions
// ============================================

static void cleanupStoryTimeScreenPointers() {
    st_screen = NULL;
    st_status_label = NULL;
    st_message_label = NULL;
    st_progress_label = NULL;
    st_question_label = NULL;
    st_question_progress = NULL;
    st_score_label = NULL;
    st_wpm_label = NULL;
    st_tone_label = NULL;

    for (int i = 0; i < 4; i++) {
        st_answer_btns[i] = NULL;
    }
    for (int i = 0; i < 5; i++) {
        st_result_labels[i] = NULL;
    }
}

// ============================================
// Main Menu Screen
// ============================================

static void st_menu_select_handler(lv_event_t* e) {
    int target = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    // Note: onLVGLMenuSelect already plays TONE_SELECT beep
    onLVGLMenuSelect(target);
}

static void st_menu_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ESC) {
        onLVGLBackNavigation();
        lv_event_stop_processing(e);
    }
}

lv_obj_t* createStoryTimeMenuScreen() {
    cleanupStoryTimeScreenPointers();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
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
    lv_label_set_text(title, "MORSE STORY TIME");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);

    // Subtitle
    lv_obj_t* subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "Listen. Comprehend. Learn.");
    lv_obj_set_style_text_color(subtitle, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(subtitle, getThemeFonts()->font_body, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 10);

    // Menu buttons
    const char* menu_items[] = {"Select Difficulty", "Progress", "Settings"};
    const int menu_targets[] = {LVGL_MODE_STORY_TIME_DIFFICULTY, LVGL_MODE_STORY_TIME_PROGRESS, LVGL_MODE_STORY_TIME_SETTINGS};

    int btn_y = HEADER_HEIGHT + 50;
    for (int i = 0; i < 3; i++) {
        lv_obj_t* btn = lv_btn_create(screen);
        lv_obj_set_size(btn, 280, 50);
        lv_obj_set_pos(btn, (SCREEN_WIDTH - 280) / 2, btn_y);
        lv_obj_set_user_data(btn, (void*)(intptr_t)menu_targets[i]);
        lv_obj_add_event_cb(btn, st_menu_select_handler, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(btn, st_menu_key_handler, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(btn, linear_nav_handler, LV_EVENT_KEY, NULL);
        applyButtonStyle(btn);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, menu_items[i]);
        lv_obj_center(label);

        addNavigableWidget(btn);
        btn_y += 60;
    }

    // Stats display
    lv_obj_t* stats = lv_label_create(screen);
    char stats_text[64];
    snprintf(stats_text, sizeof(stats_text), "%d stories completed | %d perfect scores",
             stProgress.totalStoriesCompleted, stProgress.totalPerfectScores);
    lv_label_set_text(stats, stats_text);
    lv_obj_set_style_text_color(stats, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_set_style_text_font(stats, getThemeFonts()->font_small, 0);
    lv_obj_align(stats, LV_ALIGN_BOTTOM_MID, 0, -50);

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "UP/DN Select   ENTER Choose   ESC Back");
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_small, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -15);

    st_screen = screen;
    return screen;
}

// ============================================
// Difficulty Selection Screen
// ============================================

static void st_difficulty_select_handler(lv_event_t* e) {
    int diff = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    stSession.selectedDifficulty = (StoryDifficulty)diff;
    st_selected_diff_index = diff;
    st_scroll_index = 0;
    // Note: onLVGLMenuSelect already plays TONE_SELECT beep
    onLVGLMenuSelect(LVGL_MODE_STORY_TIME_LIST);
}

lv_obj_t* createStoryTimeDifficultyScreen() {
    cleanupStoryTimeScreenPointers();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    createCompactStatusBar(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "SELECT DIFFICULTY");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_center(title);

    // Difficulty buttons
    const char* diff_names[] = {"Tutorial", "Easy", "Medium", "Hard", "Expert"};
    const lv_color_t diff_colors[] = {
        LV_COLOR_ACCENT_CYAN,
        LV_COLOR_ACCENT_GREEN,
        LV_COLOR_WARNING,
        LV_COLOR_ERROR,
        LV_COLOR_ACCENT_MAGENTA
    };

    int btn_y = HEADER_HEIGHT + 15;
    for (int i = 0; i < 5; i++) {
        int count = getStoryCountByDifficulty((StoryDifficulty)i);
        int completed = stProgress.completedByDifficulty[i];

        lv_obj_t* btn = lv_btn_create(screen);
        lv_obj_set_size(btn, 300, 45);
        lv_obj_set_pos(btn, (SCREEN_WIDTH - 300) / 2, btn_y);
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, st_difficulty_select_handler, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(btn, st_menu_key_handler, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(btn, linear_nav_handler, LV_EVENT_KEY, NULL);
        applyButtonStyle(btn);

        // Difficulty name
        lv_obj_t* name = lv_label_create(btn);
        lv_label_set_text(name, diff_names[i]);
        lv_obj_set_style_text_color(name, diff_colors[i], 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 10, 0);

        // Progress text
        char prog_text[32];
        snprintf(prog_text, sizeof(prog_text), "%d/%d", completed, count);
        lv_obj_t* prog = lv_label_create(btn);
        lv_label_set_text(prog, prog_text);
        lv_obj_set_style_text_color(prog, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_align(prog, LV_ALIGN_RIGHT_MID, -10, 0);

        addNavigableWidget(btn);
        btn_y += 50;
    }

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "UP/DN Select   ENTER Choose   ESC Back");
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_small, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -15);

    st_screen = screen;
    return screen;
}

// ============================================
// Story List Screen
// ============================================

static void st_story_select_handler(lv_event_t* e) {
    int index = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    const StoryData* story = getStoryByDifficultyAndIndex(stSession.selectedDifficulty, index);
    if (story) {
        int globalIndex = getGlobalStoryIndex(stSession.selectedDifficulty, index);
        stSelectStory(story, globalIndex);
        // Note: onLVGLMenuSelect already plays TONE_SELECT beep
        onLVGLMenuSelect(LVGL_MODE_STORY_TIME_LISTEN);
    }
}

lv_obj_t* createStoryTimeListScreen() {
    cleanupStoryTimeScreenPointers();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    createCompactStatusBar(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    char title_text[32];
    snprintf(title_text, sizeof(title_text), "%s STORIES",
             getDifficultyLabel(stSession.selectedDifficulty));
    lv_label_set_text(title, title_text);
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_center(title);

    // Scrollable list container
    lv_obj_t* list = lv_obj_create(screen);
    lv_obj_set_size(list, SCREEN_WIDTH - 20, SCREEN_HEIGHT - HEADER_HEIGHT - 50);
    lv_obj_set_pos(list, 10, HEADER_HEIGHT + 5);
    lv_obj_set_layout(list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 5, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    int count = getStoryCountByDifficulty(stSession.selectedDifficulty);

    for (int i = 0; i < count; i++) {
        const StoryData* story = getStoryByDifficultyAndIndex(stSession.selectedDifficulty, i);
        if (!story) continue;

        StoryProgress prog = stGetStoryProgress(story->id);

        lv_obj_t* btn = lv_btn_create(list);
        lv_obj_set_size(btn, SCREEN_WIDTH - 40, 48);
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, st_story_select_handler, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(btn, st_menu_key_handler, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(btn, linear_nav_handler, LV_EVENT_KEY, NULL);
        applyButtonStyle(btn);

        // Story title
        lv_obj_t* title_lbl = lv_label_create(btn);
        lv_label_set_text(title_lbl, story->title);
        lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 5, -8);

        // Word count
        char info[32];
        snprintf(info, sizeof(info), "%d words", story->wordCount);
        lv_obj_t* info_lbl = lv_label_create(btn);
        lv_label_set_text(info_lbl, info);
        lv_obj_set_style_text_color(info_lbl, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(info_lbl, getThemeFonts()->font_small, 0);
        lv_obj_align(info_lbl, LV_ALIGN_LEFT_MID, 5, 10);

        // Completion indicator
        if (prog.completed) {
            lv_obj_t* check = lv_label_create(btn);
            if (prog.bestScore == 5) {
                lv_label_set_text(check, LV_SYMBOL_OK " Perfect");
                lv_obj_set_style_text_color(check, LV_COLOR_ACCENT_GREEN, 0);
            } else {
                char score_text[16];
                snprintf(score_text, sizeof(score_text), LV_SYMBOL_OK " %d/5", prog.bestScore);
                lv_label_set_text(check, score_text);
                lv_obj_set_style_text_color(check, LV_COLOR_ACCENT_CYAN, 0);
            }
            lv_obj_align(check, LV_ALIGN_RIGHT_MID, -5, 0);
        }

        addNavigableWidget(btn);
    }

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "UP/DN Select   ENTER Listen   ESC Back");
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_small, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -15);

    st_screen = screen;
    return screen;
}

// ============================================
// Listening Screen
// ============================================

static void st_play_handler(lv_event_t* e) {
    beep(TONE_SELECT, BEEP_SHORT);

    if (stSession.playPhase == ST_PLAY_PLAYING) {
        // Pause
        stPausePlayback();
        if (st_status_label) lv_label_set_text(st_status_label, "PAUSED");
    } else if (stSession.playPhase == ST_PLAY_PAUSED) {
        // Resume
        if (st_status_label) lv_label_set_text(st_status_label, "PLAYING...");
        lv_timer_handler();
        stResumePlayback();
        if (stSession.playPhase == ST_PLAY_COMPLETE) {
            if (st_status_label) lv_label_set_text(st_status_label, "COMPLETE");
        }
    } else {
        // Start
        if (st_status_label) lv_label_set_text(st_status_label, "PLAYING...");
        lv_timer_handler();
        stPlayStoryMorse();
        if (stSession.playPhase == ST_PLAY_COMPLETE) {
            if (st_status_label) lv_label_set_text(st_status_label, "COMPLETE");
        }
    }
}

static void st_restart_handler(lv_event_t* e) {
    beep(TONE_SELECT, BEEP_SHORT);
    if (st_status_label) lv_label_set_text(st_status_label, "PLAYING...");
    lv_timer_handler();
    stRestartPlayback();
    if (stSession.playPhase == ST_PLAY_COMPLETE) {
        if (st_status_label) lv_label_set_text(st_status_label, "COMPLETE");
    }
}

static void st_quiz_handler(lv_event_t* e) {
    if (stSession.hasListenedOnce) {
        stStopPlayback();
        stSession.currentQuestion = 0;
        stSession.correctAnswers = 0;
        // Note: onLVGLMenuSelect already plays TONE_SELECT beep
        onLVGLMenuSelect(LVGL_MODE_STORY_TIME_QUIZ);
    } else {
        beep(300, 150);  // Error beep - must listen first
    }
}

static void st_listen_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ESC) {
        stStopPlayback();
        onLVGLBackNavigation();
        lv_event_stop_processing(e);
    } else if (key == ' ') {
        st_play_handler(e);
        lv_event_stop_processing(e);
    } else if (key == 'r' || key == 'R') {
        st_restart_handler(e);
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_ENTER || key == 0x0D || key == 0x0A) {
        st_quiz_handler(e);
        lv_event_stop_processing(e);
    }
}

lv_obj_t* createStoryTimeListenScreen() {
    cleanupStoryTimeScreenPointers();

    if (!stSession.currentStory) {
        return createComingSoonScreen("NO STORY SELECTED");
    }

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    createCompactStatusBar(screen);

    // Title bar with story name
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, stSession.currentStory->title);
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_center(title);

    // Difficulty badge
    lv_obj_t* diff_badge = lv_label_create(screen);
    char badge_text[32];
    snprintf(badge_text, sizeof(badge_text), "%s | %d words",
             getDifficultyLabel(stSession.currentStory->difficulty),
             stSession.currentStory->wordCount);
    lv_label_set_text(diff_badge, badge_text);
    lv_obj_set_style_text_color(diff_badge, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(diff_badge, getThemeFonts()->font_small, 0);
    lv_obj_align(diff_badge, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 5);

    // Status display
    lv_obj_t* status_card = lv_obj_create(screen);
    lv_obj_set_size(status_card, SCREEN_WIDTH - 40, 80);
    lv_obj_set_pos(status_card, 20, HEADER_HEIGHT + 35);
    applyCardStyle(status_card);

    st_status_label = lv_label_create(status_card);
    const char* initial_status = "READY";
    if (stSession.playPhase == ST_PLAY_COMPLETE) initial_status = "COMPLETE";
    else if (stSession.playPhase == ST_PLAY_PAUSED) initial_status = "PAUSED";
    lv_label_set_text(st_status_label, initial_status);
    lv_obj_set_style_text_font(st_status_label, getThemeFonts()->font_title, 0);
    lv_obj_set_style_text_color(st_status_label, LV_COLOR_ACCENT_GREEN, 0);
    lv_obj_center(st_status_label);

    // Play count indicator
    lv_obj_t* play_count = lv_label_create(screen);
    char count_text[32];
    snprintf(count_text, sizeof(count_text), "Plays: %d", stSession.playCount);
    lv_label_set_text(play_count, count_text);
    lv_obj_set_style_text_color(play_count, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(play_count, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 120);

    // WPM display
    lv_obj_t* wpm_display = lv_label_create(screen);
    char wpm_text[32];
    snprintf(wpm_text, sizeof(wpm_text), "%d WPM", stSession.playbackWPM);
    lv_label_set_text(wpm_display, wpm_text);
    lv_obj_set_style_text_color(wpm_display, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(wpm_display, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 140);

    // Control buttons
    int btn_y = SCREEN_HEIGHT - 120;

    lv_obj_t* play_btn = lv_btn_create(screen);
    lv_obj_set_size(play_btn, 140, 45);
    lv_obj_set_pos(play_btn, 30, btn_y);
    lv_obj_add_event_cb(play_btn, st_play_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(play_btn, st_listen_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(play_btn, linear_nav_handler, LV_EVENT_KEY, NULL);
    applyButtonStyle(play_btn);
    lv_obj_t* play_lbl = lv_label_create(play_btn);
    lv_label_set_text(play_lbl, LV_SYMBOL_PLAY " Play");
    lv_obj_center(play_lbl);
    addNavigableWidget(play_btn);

    lv_obj_t* restart_btn = lv_btn_create(screen);
    lv_obj_set_size(restart_btn, 140, 45);
    lv_obj_set_pos(restart_btn, SCREEN_WIDTH - 170, btn_y);
    lv_obj_add_event_cb(restart_btn, st_restart_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(restart_btn, st_listen_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(restart_btn, linear_nav_handler, LV_EVENT_KEY, NULL);
    applyButtonStyle(restart_btn);
    lv_obj_t* restart_lbl = lv_label_create(restart_btn);
    lv_label_set_text(restart_lbl, LV_SYMBOL_REFRESH " Restart");
    lv_obj_center(restart_lbl);
    addNavigableWidget(restart_btn);

    // Quiz button
    lv_obj_t* quiz_btn = lv_btn_create(screen);
    lv_obj_set_size(quiz_btn, SCREEN_WIDTH - 60, 45);
    lv_obj_set_pos(quiz_btn, 30, btn_y + 55);
    lv_obj_add_event_cb(quiz_btn, st_quiz_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(quiz_btn, st_listen_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(quiz_btn, linear_nav_handler, LV_EVENT_KEY, NULL);
    applyButtonStyle(quiz_btn);
    lv_obj_t* quiz_lbl = lv_label_create(quiz_btn);
    if (stSession.hasListenedOnce) {
        lv_label_set_text(quiz_lbl, LV_SYMBOL_RIGHT " Take Quiz");
    } else {
        lv_label_set_text(quiz_lbl, "Listen first to unlock quiz");
        lv_obj_set_style_text_color(quiz_lbl, LV_COLOR_TEXT_DISABLED, 0);
    }
    lv_obj_center(quiz_lbl);
    addNavigableWidget(quiz_btn);

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "SPACE Play/Pause   R Restart   ESC Back");
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_small, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -10);

    st_screen = screen;
    return screen;
}

// ============================================
// Quiz Screen
// ============================================

static void st_answer_handler(lv_event_t* e) {
    int answer = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));

    // Submit answer
    stSubmitAnswer(stSession.currentQuestion, answer);

    // Visual feedback
    lv_obj_t* btn = lv_event_get_target(e);
    const StoryQuestion* q = &stSession.currentStory->questions[stSession.currentQuestion];
    bool correct = (answer == q->correctIndex);

    if (correct) {
        lv_obj_set_style_bg_color(btn, LV_COLOR_ACCENT_GREEN, 0);
    } else {
        lv_obj_set_style_bg_color(btn, LV_COLOR_ERROR, 0);
        // Highlight correct answer
        if (st_answer_btns[q->correctIndex]) {
            lv_obj_set_style_bg_color(st_answer_btns[q->correctIndex], LV_COLOR_ACCENT_GREEN, 0);
        }
    }
    lv_timer_handler();

    // Brief delay for feedback
    stDelayWithUI(ST_FEEDBACK_DELAY_MS);

    // Move to next question or results
    stSession.currentQuestion++;
    if (stSession.currentQuestion >= ST_MAX_QUESTIONS) {
        stFinishQuiz();
        onLVGLMenuSelect(LVGL_MODE_STORY_TIME_RESULTS);
    } else {
        // Reload quiz screen for next question
        onLVGLMenuSelect(LVGL_MODE_STORY_TIME_QUIZ);
    }
}

static void st_quiz_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ESC) {
        // Confirm exit? For now, just go back to listen
        onLVGLMenuSelect(LVGL_MODE_STORY_TIME_LISTEN);
        lv_event_stop_processing(e);
    }
}

lv_obj_t* createStoryTimeQuizScreen() {
    cleanupStoryTimeScreenPointers();

    if (!stSession.currentStory || stSession.currentQuestion >= ST_MAX_QUESTIONS) {
        return createComingSoonScreen("QUIZ ERROR");
    }

    const StoryQuestion* q = &stSession.currentStory->questions[stSession.currentQuestion];

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    createCompactStatusBar(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    char title_text[32];
    snprintf(title_text, sizeof(title_text), "Question %d of %d",
             stSession.currentQuestion + 1, ST_MAX_QUESTIONS);
    lv_label_set_text(title, title_text);
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_center(title);

    // Progress dots
    lv_obj_t* progress_container = lv_obj_create(screen);
    lv_obj_set_size(progress_container, 150, 20);
    lv_obj_set_pos(progress_container, (SCREEN_WIDTH - 150) / 2, HEADER_HEIGHT + 5);
    lv_obj_set_layout(progress_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(progress_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(progress_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(progress_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(progress_container, 0, 0);
    lv_obj_set_style_pad_column(progress_container, 10, 0);

    for (int i = 0; i < ST_MAX_QUESTIONS; i++) {
        lv_obj_t* dot = lv_obj_create(progress_container);
        lv_obj_set_size(dot, 12, 12);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot, 0, 0);

        if (i < stSession.currentQuestion) {
            // Answered - show if correct or wrong
            if (stSession.selectedAnswers[i] == stSession.currentStory->questions[i].correctIndex) {
                lv_obj_set_style_bg_color(dot, LV_COLOR_ACCENT_GREEN, 0);
            } else {
                lv_obj_set_style_bg_color(dot, LV_COLOR_ERROR, 0);
            }
        } else if (i == stSession.currentQuestion) {
            // Current
            lv_obj_set_style_bg_color(dot, LV_COLOR_ACCENT_CYAN, 0);
        } else {
            // Future
            lv_obj_set_style_bg_color(dot, LV_COLOR_BG_LAYER2, 0);
        }
    }

    // Question text
    st_question_label = lv_label_create(screen);
    lv_label_set_text(st_question_label, q->question);
    lv_label_set_long_mode(st_question_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(st_question_label, SCREEN_WIDTH - 40);
    lv_obj_set_style_text_font(st_question_label, getThemeFonts()->font_body, 0);
    lv_obj_align(st_question_label, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 35);

    // Answer buttons
    int btn_y = HEADER_HEIGHT + 90;
    const char* letters[] = {"A", "B", "C", "D"};

    for (int i = 0; i < 4; i++) {
        lv_obj_t* btn = lv_btn_create(screen);
        lv_obj_set_size(btn, SCREEN_WIDTH - 40, 42);
        lv_obj_set_pos(btn, 20, btn_y);
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, st_answer_handler, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(btn, st_quiz_key_handler, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(btn, linear_nav_handler, LV_EVENT_KEY, NULL);
        applyButtonStyle(btn);

        // Letter label
        lv_obj_t* letter = lv_label_create(btn);
        lv_label_set_text(letter, letters[i]);
        lv_obj_set_style_text_color(letter, LV_COLOR_ACCENT_CYAN, 0);
        lv_obj_align(letter, LV_ALIGN_LEFT_MID, 10, 0);

        // Answer text
        lv_obj_t* answer = lv_label_create(btn);
        lv_label_set_text(answer, q->options[i]);
        lv_label_set_long_mode(answer, LV_LABEL_LONG_DOT);
        lv_obj_set_width(answer, SCREEN_WIDTH - 100);
        lv_obj_align(answer, LV_ALIGN_LEFT_MID, 35, 0);

        st_answer_btns[i] = btn;
        addNavigableWidget(btn);
        btn_y += 47;
    }

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "UP/DN Select   ENTER Answer   ESC Back");
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_small, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -10);

    st_screen = screen;
    return screen;
}

// ============================================
// Results Screen
// ============================================

static void st_retry_handler(lv_event_t* e) {
    // Reset quiz state and go back to listen
    stSession.hasListenedOnce = true;  // Keep this so they can go straight to quiz
    // Note: onLVGLMenuSelect already plays TONE_SELECT beep
    onLVGLMenuSelect(LVGL_MODE_STORY_TIME_LISTEN);
}

static void st_next_handler(lv_event_t* e) {
    // Find next story at same difficulty
    int currentDiffIndex = getStoryIndexInDifficulty(
        stSession.selectedDifficulty, stSession.storyIndex);
    int count = getStoryCountByDifficulty(stSession.selectedDifficulty);

    if (currentDiffIndex >= 0 && currentDiffIndex < count - 1) {
        const StoryData* next = getStoryByDifficultyAndIndex(
            stSession.selectedDifficulty, currentDiffIndex + 1);
        if (next) {
            int globalIndex = getGlobalStoryIndex(stSession.selectedDifficulty, currentDiffIndex + 1);
            stSelectStory(next, globalIndex);
            // Note: onLVGLMenuSelect already plays TONE_SELECT beep
            onLVGLMenuSelect(LVGL_MODE_STORY_TIME_LISTEN);
            return;
        }
    }

    // No next story, go to list
    // Note: onLVGLMenuSelect already plays TONE_SELECT beep
    onLVGLMenuSelect(LVGL_MODE_STORY_TIME_LIST);
}

static void st_back_to_list_handler(lv_event_t* e) {
    // Note: onLVGLMenuSelect already plays TONE_SELECT beep
    onLVGLMenuSelect(LVGL_MODE_STORY_TIME_LIST);
}

lv_obj_t* createStoryTimeResultsScreen() {
    cleanupStoryTimeScreenPointers();

    if (!stSession.currentStory) {
        return createComingSoonScreen("NO RESULTS");
    }

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    createCompactStatusBar(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "RESULTS");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_center(title);

    // Score display
    st_score_label = lv_label_create(screen);
    char score_text[64];
    bool perfect = (stSession.correctAnswers == ST_MAX_QUESTIONS);
    if (perfect) {
        snprintf(score_text, sizeof(score_text), "%d/%d - PERFECT!",
                 stSession.correctAnswers, ST_MAX_QUESTIONS);
        lv_obj_set_style_text_color(st_score_label, LV_COLOR_ACCENT_GREEN, 0);
    } else {
        snprintf(score_text, sizeof(score_text), "%d/%d Correct",
                 stSession.correctAnswers, ST_MAX_QUESTIONS);
        lv_obj_set_style_text_color(st_score_label, LV_COLOR_ACCENT_CYAN, 0);
    }
    lv_label_set_text(st_score_label, score_text);
    lv_obj_set_style_text_font(st_score_label, getThemeFonts()->font_title, 0);
    lv_obj_align(st_score_label, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 15);

    // Star rating
    lv_obj_t* stars = lv_label_create(screen);
    char star_text[32] = "";
    for (int i = 0; i < stSession.correctAnswers; i++) {
        strcat(star_text, LV_SYMBOL_OK " ");
    }
    lv_label_set_text(stars, star_text);
    lv_obj_set_style_text_color(stars, LV_COLOR_WARNING, 0);
    lv_obj_align(stars, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 50);

    // Per-question breakdown
    int y = HEADER_HEIGHT + 80;
    for (int i = 0; i < ST_MAX_QUESTIONS; i++) {
        lv_obj_t* row = lv_label_create(screen);
        char row_text[64];
        bool correct = (stSession.selectedAnswers[i] ==
                       stSession.currentStory->questions[i].correctIndex);
        snprintf(row_text, sizeof(row_text), "%s Q%d: %s",
                 correct ? LV_SYMBOL_OK : "X",
                 i + 1,
                 correct ? "Correct" : "Wrong");
        lv_label_set_text(row, row_text);
        lv_obj_set_style_text_color(row, correct ? LV_COLOR_ACCENT_GREEN : LV_COLOR_ERROR, 0);
        lv_obj_set_style_text_font(row, getThemeFonts()->font_small, 0);
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 30, y);
        y += 20;
    }

    // Action buttons
    int btn_y = SCREEN_HEIGHT - 110;

    if (!perfect) {
        lv_obj_t* retry_btn = lv_btn_create(screen);
        lv_obj_set_size(retry_btn, 140, 40);
        lv_obj_set_pos(retry_btn, 30, btn_y);
        lv_obj_add_event_cb(retry_btn, st_retry_handler, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(retry_btn, st_menu_key_handler, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(retry_btn, linear_nav_handler, LV_EVENT_KEY, NULL);
        applyButtonStyle(retry_btn);
        lv_obj_t* retry_lbl = lv_label_create(retry_btn);
        lv_label_set_text(retry_lbl, "Retry Story");
        lv_obj_center(retry_lbl);
        addNavigableWidget(retry_btn);
    }

    lv_obj_t* next_btn = lv_btn_create(screen);
    lv_obj_set_size(next_btn, 140, 40);
    lv_obj_set_pos(next_btn, perfect ? (SCREEN_WIDTH - 140) / 2 : SCREEN_WIDTH - 170, btn_y);
    lv_obj_add_event_cb(next_btn, st_next_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(next_btn, st_menu_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(next_btn, linear_nav_handler, LV_EVENT_KEY, NULL);
    applyButtonStyle(next_btn);
    lv_obj_t* next_lbl = lv_label_create(next_btn);
    lv_label_set_text(next_lbl, "Next Story");
    lv_obj_center(next_lbl);
    addNavigableWidget(next_btn);

    lv_obj_t* list_btn = lv_btn_create(screen);
    lv_obj_set_size(list_btn, SCREEN_WIDTH - 60, 40);
    lv_obj_set_pos(list_btn, 30, btn_y + 50);
    lv_obj_add_event_cb(list_btn, st_back_to_list_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(list_btn, st_menu_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(list_btn, linear_nav_handler, LV_EVENT_KEY, NULL);
    applyButtonStyle(list_btn);
    lv_obj_t* list_lbl = lv_label_create(list_btn);
    lv_label_set_text(list_lbl, "Back to Story List");
    lv_obj_center(list_lbl);
    addNavigableWidget(list_btn);

    st_screen = screen;
    return screen;
}

// ============================================
// Progress Screen
// ============================================

lv_obj_t* createStoryTimeProgressScreen() {
    cleanupStoryTimeScreenPointers();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    createCompactStatusBar(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "YOUR PROGRESS");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_center(title);

    // Stats card
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, SCREEN_WIDTH - 40, 180);
    lv_obj_set_pos(card, 20, HEADER_HEIGHT + 15);
    applyCardStyle(card);

    int y = 10;

    // Total completed
    lv_obj_t* completed = lv_label_create(card);
    char completed_text[64];
    snprintf(completed_text, sizeof(completed_text), "Stories Completed: %d / %d",
             stProgress.totalStoriesCompleted, getStoryCount());
    lv_label_set_text(completed, completed_text);
    lv_obj_align(completed, LV_ALIGN_TOP_LEFT, 10, y);
    y += 25;

    // Perfect scores
    lv_obj_t* perfect = lv_label_create(card);
    char perfect_text[32];
    snprintf(perfect_text, sizeof(perfect_text), "Perfect Scores: %d",
             stProgress.totalPerfectScores);
    lv_label_set_text(perfect, perfect_text);
    lv_obj_set_style_text_color(perfect, LV_COLOR_ACCENT_GREEN, 0);
    lv_obj_align(perfect, LV_ALIGN_TOP_LEFT, 10, y);
    y += 25;

    // Question accuracy
    int accuracy = 0;
    if (stProgress.totalQuestionsAttempted > 0) {
        accuracy = (stProgress.totalQuestionsCorrect * 100) / stProgress.totalQuestionsAttempted;
    }
    lv_obj_t* acc = lv_label_create(card);
    char acc_text[32];
    snprintf(acc_text, sizeof(acc_text), "Accuracy: %d%%", accuracy);
    lv_label_set_text(acc, acc_text);
    lv_obj_set_style_text_color(acc, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(acc, LV_ALIGN_TOP_LEFT, 10, y);
    y += 35;

    // Per-difficulty breakdown
    lv_obj_t* diff_header = lv_label_create(card);
    lv_label_set_text(diff_header, "By Difficulty:");
    lv_obj_set_style_text_color(diff_header, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(diff_header, LV_ALIGN_TOP_LEFT, 10, y);
    y += 20;

    const char* diff_names[] = {"Tutorial", "Easy", "Medium", "Hard", "Expert"};
    for (int i = 0; i < 5; i++) {
        int count = getStoryCountByDifficulty((StoryDifficulty)i);
        lv_obj_t* diff = lv_label_create(card);
        char diff_text[48];
        snprintf(diff_text, sizeof(diff_text), "%s: %d/%d",
                 diff_names[i], stProgress.completedByDifficulty[i], count);
        lv_label_set_text(diff, diff_text);
        lv_obj_set_style_text_font(diff, getThemeFonts()->font_small, 0);
        lv_obj_align(diff, LV_ALIGN_TOP_LEFT, 10 + (i < 3 ? 0 : 150), y + (i % 3) * 18);
    }

    // Focus widget for key handling
    lv_obj_t* focus = lv_obj_create(screen);
    lv_obj_set_size(focus, 1, 1);
    lv_obj_set_pos(focus, -10, -10);
    lv_obj_set_style_bg_opa(focus, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus, 0, 0);
    lv_obj_set_style_outline_width(focus, 0, 0);
    lv_obj_add_flag(focus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus, st_menu_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(focus, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus);

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "ESC Back");
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_small, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -15);

    st_screen = screen;
    return screen;
}

// ============================================
// Settings Screen
// ============================================

static void st_wpm_dec_handler(lv_event_t* e) {
    if (stProgress.preferredWPM > 5) {
        stProgress.preferredWPM--;
        stSession.playbackWPM = stProgress.preferredWPM;
        if (st_wpm_label) {
            char text[16];
            snprintf(text, sizeof(text), "%d WPM", stProgress.preferredWPM);
            lv_label_set_text(st_wpm_label, text);
        }
        stSaveSettings();
        beep(TONE_MENU_NAV, BEEP_SHORT);
    }
}

static void st_wpm_inc_handler(lv_event_t* e) {
    if (stProgress.preferredWPM < 30) {
        stProgress.preferredWPM++;
        stSession.playbackWPM = stProgress.preferredWPM;
        if (st_wpm_label) {
            char text[16];
            snprintf(text, sizeof(text), "%d WPM", stProgress.preferredWPM);
            lv_label_set_text(st_wpm_label, text);
        }
        stSaveSettings();
        beep(TONE_MENU_NAV, BEEP_SHORT);
    }
}

static void st_tone_dec_handler(lv_event_t* e) {
    if (stProgress.preferredTone > 400) {
        stProgress.preferredTone -= 50;
        stSession.toneFrequency = stProgress.preferredTone;
        if (st_tone_label) {
            char text[16];
            snprintf(text, sizeof(text), "%d Hz", stProgress.preferredTone);
            lv_label_set_text(st_tone_label, text);
        }
        stSaveSettings();
        beep(stProgress.preferredTone, 100);
    }
}

static void st_tone_inc_handler(lv_event_t* e) {
    if (stProgress.preferredTone < 900) {
        stProgress.preferredTone += 50;
        stSession.toneFrequency = stProgress.preferredTone;
        if (st_tone_label) {
            char text[16];
            snprintf(text, sizeof(text), "%d Hz", stProgress.preferredTone);
            lv_label_set_text(st_tone_label, text);
        }
        stSaveSettings();
        beep(stProgress.preferredTone, 100);
    }
}

lv_obj_t* createStoryTimeSettingsScreen() {
    cleanupStoryTimeScreenPointers();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    createCompactStatusBar(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "PLAYBACK SETTINGS");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_center(title);

    int row_y = HEADER_HEIGHT + 30;

    // WPM setting
    lv_obj_t* wpm_row = lv_obj_create(screen);
    lv_obj_set_size(wpm_row, SCREEN_WIDTH - 40, 50);
    lv_obj_set_pos(wpm_row, 20, row_y);
    applyCardStyle(wpm_row);

    lv_obj_t* wpm_title = lv_label_create(wpm_row);
    lv_label_set_text(wpm_title, "Speed:");
    lv_obj_align(wpm_title, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* wpm_dec = lv_btn_create(wpm_row);
    lv_obj_set_size(wpm_dec, 40, 35);
    lv_obj_align(wpm_dec, LV_ALIGN_RIGHT_MID, -120, 0);
    lv_obj_add_event_cb(wpm_dec, st_wpm_dec_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(wpm_dec, st_menu_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(wpm_dec, linear_nav_handler, LV_EVENT_KEY, NULL);
    applyButtonStyle(wpm_dec);
    lv_obj_t* dec_lbl = lv_label_create(wpm_dec);
    lv_label_set_text(dec_lbl, "-");
    lv_obj_center(dec_lbl);
    addNavigableWidget(wpm_dec);

    st_wpm_label = lv_label_create(wpm_row);
    char wpm_text[16];
    snprintf(wpm_text, sizeof(wpm_text), "%d WPM", stProgress.preferredWPM);
    lv_label_set_text(st_wpm_label, wpm_text);
    lv_obj_set_style_text_color(st_wpm_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(st_wpm_label, LV_ALIGN_RIGHT_MID, -55, 0);

    lv_obj_t* wpm_inc = lv_btn_create(wpm_row);
    lv_obj_set_size(wpm_inc, 40, 35);
    lv_obj_align(wpm_inc, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(wpm_inc, st_wpm_inc_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(wpm_inc, st_menu_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(wpm_inc, linear_nav_handler, LV_EVENT_KEY, NULL);
    applyButtonStyle(wpm_inc);
    lv_obj_t* inc_lbl = lv_label_create(wpm_inc);
    lv_label_set_text(inc_lbl, "+");
    lv_obj_center(inc_lbl);
    addNavigableWidget(wpm_inc);

    row_y += 60;

    // Tone setting
    lv_obj_t* tone_row = lv_obj_create(screen);
    lv_obj_set_size(tone_row, SCREEN_WIDTH - 40, 50);
    lv_obj_set_pos(tone_row, 20, row_y);
    applyCardStyle(tone_row);

    lv_obj_t* tone_title = lv_label_create(tone_row);
    lv_label_set_text(tone_title, "Tone:");
    lv_obj_align(tone_title, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* tone_dec = lv_btn_create(tone_row);
    lv_obj_set_size(tone_dec, 40, 35);
    lv_obj_align(tone_dec, LV_ALIGN_RIGHT_MID, -120, 0);
    lv_obj_add_event_cb(tone_dec, st_tone_dec_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(tone_dec, st_menu_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(tone_dec, linear_nav_handler, LV_EVENT_KEY, NULL);
    applyButtonStyle(tone_dec);
    lv_obj_t* tone_dec_lbl = lv_label_create(tone_dec);
    lv_label_set_text(tone_dec_lbl, "-");
    lv_obj_center(tone_dec_lbl);
    addNavigableWidget(tone_dec);

    st_tone_label = lv_label_create(tone_row);
    char tone_text[16];
    snprintf(tone_text, sizeof(tone_text), "%d Hz", stProgress.preferredTone);
    lv_label_set_text(st_tone_label, tone_text);
    lv_obj_set_style_text_color(st_tone_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(st_tone_label, LV_ALIGN_RIGHT_MID, -55, 0);

    lv_obj_t* tone_inc = lv_btn_create(tone_row);
    lv_obj_set_size(tone_inc, 40, 35);
    lv_obj_align(tone_inc, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(tone_inc, st_tone_inc_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(tone_inc, st_menu_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(tone_inc, linear_nav_handler, LV_EVENT_KEY, NULL);
    applyButtonStyle(tone_inc);
    lv_obj_t* tone_inc_lbl = lv_label_create(tone_inc);
    lv_label_set_text(tone_inc_lbl, "+");
    lv_obj_center(tone_inc_lbl);
    addNavigableWidget(tone_inc);

    // Info text
    lv_obj_t* info = lv_label_create(screen);
    lv_label_set_text(info, "Adjust speed and tone for morse playback.\nSettings are saved automatically.");
    lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info, SCREEN_WIDTH - 60);
    lv_obj_set_style_text_color(info, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(info, getThemeFonts()->font_small, 0);
    lv_obj_align(info, LV_ALIGN_CENTER, 0, 30);

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "LEFT/RIGHT Adjust   ESC Back");
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_small, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -15);

    st_screen = screen;
    return screen;
}

// ============================================
// Screen Factory Function
// ============================================

lv_obj_t* createStoryTimeScreenForMode(int mode) {
    switch (mode) {
        case LVGL_MODE_STORY_TIME:
            return createStoryTimeMenuScreen();
        case LVGL_MODE_STORY_TIME_DIFFICULTY:
            return createStoryTimeDifficultyScreen();
        case LVGL_MODE_STORY_TIME_LIST:
            return createStoryTimeListScreen();
        case LVGL_MODE_STORY_TIME_LISTEN:
            return createStoryTimeListenScreen();
        case LVGL_MODE_STORY_TIME_QUIZ:
            return createStoryTimeQuizScreen();
        case LVGL_MODE_STORY_TIME_RESULTS:
            return createStoryTimeResultsScreen();
        case LVGL_MODE_STORY_TIME_PROGRESS:
            return createStoryTimeProgressScreen();
        case LVGL_MODE_STORY_TIME_SETTINGS:
            return createStoryTimeSettingsScreen();
        default:
            return NULL;
    }
}

#endif // LV_STORY_TIME_SCREENS_H
