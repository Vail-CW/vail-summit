/*
 * VAIL SUMMIT - Vail CW School (unified hub + practice modes)
 *
 * The single learning path. A small hub routes to:
 *   - Learn          (existing Vail course curriculum: MODE_VAIL_COURSE_MODULE_SELECT)
 *   - Daily Practice (MODE_SCHOOL_DAILY) - fixed adaptive mixed drill, builds streak
 *   - Copy Practice  (MODE_SCHOOL_COPY)  - open-ended listen & type
 *   - Send Practice  (MODE_SCHOOL_SEND)  - paddle + live decode (built separately)
 *
 * Daily and Copy share one "copy drill" engine here: play a weak-weighted
 * character from the learned-character pool, the user types what they hear,
 * immediate feedback, auto-advance. Every answer feeds the same 0-1000 mastery
 * as Learn (vailCourseRecordAnswer), so progress is unified like the web app.
 *
 * INCLUDE ORDER: after lv_menu_screens.h / lv_widgets / training_vail_course_core.h
 * and the audio task manager, but before lv_mode_integration.h (which references
 * createSchoolScreenForMode / createSchoolHubScreen / cleanupSchoolPractice).
 */

#ifndef LV_SCHOOL_SCREENS_H
#define LV_SCHOOL_SCREENS_H

#include <lvgl.h>
#include "lv_screen_manager.h"
#include "lv_widgets_summit.h"
#include "lv_theme_summit.h"
#include "../core/modes.h"
#include "../training/training_vail_course_core.h"

// ============================================
// Copy-drill state (shared by Daily + Copy)
// ============================================

enum SchoolDrillPhase { SDP_LISTEN, SDP_FEEDBACK, SDP_DONE };

static lv_obj_t*   s_school_screen   = NULL;
static lv_timer_t* s_school_tick     = NULL;   // 50ms state-machine tick
static String      s_drill_pool      = "";
static char        s_drill_target    = '?';
static int         s_drill_correct   = 0;
static int         s_drill_total     = 0;
static int         s_drill_goal      = 0;       // 0 = endless (Copy), N = Daily
static SchoolDrillPhase s_drill_phase = SDP_LISTEN;
static int         s_drill_fb_ms     = 0;       // feedback countdown (ms)

// widgets
static lv_obj_t* s_drill_char   = NULL;  // big character / "?"
static lv_obj_t* s_drill_prompt = NULL;
static lv_obj_t* s_drill_score  = NULL;

// ============================================
// Drill logic
// ============================================

static void schoolDrillUpdateScore() {
    if (!s_drill_score) return;
    int pct = (s_drill_total > 0) ? (s_drill_correct * 100 / s_drill_total) : 0;
    if (s_drill_goal > 0)
        lv_label_set_text_fmt(s_drill_score, "%d/%d   %d%%", s_drill_total, s_drill_goal, pct);
    else
        lv_label_set_text_fmt(s_drill_score, "%d/%d   %d%%", s_drill_correct, s_drill_total, pct);
}

static void schoolDrillPlayTarget() {
    char str[2] = { s_drill_target, '\0' };
    requestPlayMorseStringFarnsworth(str, vailCourseProgress.characterWPM,
                                     vailCourseProgress.effectiveWPM, TONE_SIDETONE);
}

static void schoolDrillNext() {
    if (s_drill_goal > 0 && s_drill_total >= s_drill_goal) {
        // Daily session complete
        s_drill_phase = SDP_DONE;
        if (s_drill_char) {
            lv_label_set_text(s_drill_char, LV_SYMBOL_OK);
            lv_obj_set_style_text_color(s_drill_char, LV_COLOR_SUCCESS, 0);
        }
        if (s_drill_prompt) {
            int pct = (s_drill_total > 0) ? (s_drill_correct * 100 / s_drill_total) : 0;
            lv_label_set_text_fmt(s_drill_prompt, "Daily done!  %d%%   ESC to finish", pct);
        }
        return;
    }
    s_drill_target = getVailCourseWeightedRandomChar(s_drill_pool);
    s_drill_phase = SDP_LISTEN;
    if (s_drill_char) {
        lv_label_set_text(s_drill_char, "?");
        lv_obj_set_style_text_color(s_drill_char, LV_COLOR_TEXT_TERTIARY, 0);
    }
    if (s_drill_prompt) {
        lv_label_set_text(s_drill_prompt, "Listen & type the character");
        lv_obj_set_style_text_color(s_drill_prompt, LV_COLOR_TEXT_SECONDARY, 0);
    }
    schoolDrillPlayTarget();
}

static void schoolDrillAnswer(char typed) {
    if (s_drill_phase != SDP_LISTEN) return;
    bool correct = (toupper(typed) == toupper(s_drill_target));
    vailCourseRecordAnswer(s_drill_target, correct);
    s_drill_total++;
    if (correct) s_drill_correct++;
    schoolDrillUpdateScore();

    // Feedback: reveal the target character, colored by result
    s_drill_phase = SDP_FEEDBACK;
    s_drill_fb_ms = correct ? 600 : 1100;  // linger longer when wrong
    if (s_drill_char) {
        char str[2] = { s_drill_target, '\0' };
        lv_label_set_text(s_drill_char, str);
        lv_obj_set_style_text_color(s_drill_char, correct ? LV_COLOR_SUCCESS : LV_COLOR_ERROR, 0);
    }
    if (s_drill_prompt) {
        if (correct) lv_label_set_text(s_drill_prompt, "Correct!");
        else         lv_label_set_text_fmt(s_drill_prompt, "It was  %c", toupper(s_drill_target));
        lv_obj_set_style_text_color(s_drill_prompt, correct ? LV_COLOR_SUCCESS : LV_COLOR_ERROR, 0);
    }
}

// 50ms state-machine tick: drives feedback auto-advance.
static void schoolDrillTickCb(lv_timer_t* t) {
    (void)t;
    if (s_school_tick == NULL) return;
    if (lv_scr_act() != s_school_screen) return;
    if (s_drill_phase == SDP_FEEDBACK) {
        s_drill_fb_ms -= 50;
        if (s_drill_fb_ms <= 0) schoolDrillNext();
    }
}

// Key handler on the focusable input catcher.
static void schoolDrillKeyHandler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_ESC) return;        // let global ESC handler take it
    if (key == ' ') {                     // replay current character
        if (s_drill_phase == SDP_LISTEN) schoolDrillPlayTarget();
        lv_event_stop_processing(e);
        return;
    }
    if (key == '\t' || key == LV_KEY_NEXT) { lv_event_stop_processing(e); return; }
    // Accept a typed answer character (letters, digits, punctuation we teach)
    char c = (char)key;
    if (getVailCourseCharIndex(c) >= 0) {
        schoolDrillAnswer(c);
        lv_event_stop_processing(e);
    }
}

// ============================================
// Drill screen builder (Daily + Copy)
// ============================================

static void schoolScreenDeleteCb(lv_event_t* e) {
    (void)e;
    if (s_school_tick) { lv_timer_del(s_school_tick); s_school_tick = NULL; }
    s_school_screen = NULL;
    s_drill_char = s_drill_prompt = s_drill_score = NULL;
}

static lv_obj_t* schoolBuildDrillScreen(const char* title, int goal) {
    clearNavigationGroup();
    schoolScreenDeleteCb(NULL);  // reset stale refs

    // Build the learned-character pool; fall back to the first Koch letters so
    // the mode is demonstrable on a brand-new device.
    s_drill_pool = getVailCourseLearnedChars();
    if (s_drill_pool.length() == 0) s_drill_pool = "KMRSUA";
    s_drill_correct = 0;
    s_drill_total = 0;
    s_drill_goal = goal;
    s_drill_phase = SDP_LISTEN;

    lv_obj_t* screen = createScreen();
    s_school_screen = screen;
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, schoolScreenDeleteCb, LV_EVENT_DELETE, NULL);

    // Title (top-left) + score (top-right)
    lv_obj_t* ttl = lv_label_create(screen);
    lv_label_set_text(ttl, title);
    lv_obj_set_style_text_font(ttl, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(ttl, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, 14, 10);

    s_drill_score = lv_label_create(screen);
    lv_obj_set_style_text_font(s_drill_score, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(s_drill_score, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(s_drill_score, LV_ALIGN_TOP_RIGHT, -14, 12);
    schoolDrillUpdateScore();

    // Big character (center)
    s_drill_char = lv_label_create(screen);
    lv_label_set_text(s_drill_char, "?");
    lv_obj_set_style_text_font(s_drill_char, getThemeFonts()->font_large, 0);
    lv_obj_set_style_text_color(s_drill_char, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(s_drill_char, LV_ALIGN_CENTER, 0, -16);

    // Prompt (below center)
    s_drill_prompt = lv_label_create(screen);
    lv_label_set_text(s_drill_prompt, "Listen & type the character");
    lv_obj_set_style_text_font(s_drill_prompt, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(s_drill_prompt, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(s_drill_prompt, LV_ALIGN_CENTER, 0, 30);

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "SPACE Replay    ESC Exit");
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(footer, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -8);

    // Invisible focusable input catcher (receives typed answers + ESC)
    lv_obj_t* catcher = lv_obj_create(screen);
    lv_obj_set_size(catcher, 1, 1);
    lv_obj_set_style_bg_opa(catcher, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(catcher, 0, 0);
    lv_obj_clear_flag(catcher, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(catcher, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(catcher, schoolDrillKeyHandler, LV_EVENT_KEY, NULL);
    addNavigableWidget(catcher);
    focusWidget(catcher);

    // Start: play the first character and begin the tick.
    schoolDrillNext();
    s_school_tick = lv_timer_create(schoolDrillTickCb, 50, NULL);

    return screen;
}

lv_obj_t* createSchoolDailyScreen() { return schoolBuildDrillScreen("DAILY PRACTICE", 15); }
lv_obj_t* createSchoolCopyScreen()  { return schoolBuildDrillScreen("COPY PRACTICE", 0); }

void cleanupSchoolPractice() {
    if (s_school_tick) { lv_timer_del(s_school_tick); s_school_tick = NULL; }
}

// ============================================
// School hub
// ============================================

// One hub row: title + description, routes via the menu callback.
static lv_obj_t* schoolHubTile(lv_obj_t* parent, const char* title, const char* desc, int target_mode) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 440, 54);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    applyMenuCardStyle(card);
    lv_obj_set_style_pad_all(card, 8, 0);

    lv_obj_t* t = lv_label_create(card);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(t, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 4, -10);

    lv_obj_t* d = lv_label_create(card);
    lv_label_set_text(d, desc);
    lv_obj_set_style_text_font(d, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(d, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(d, LV_ALIGN_LEFT_MID, 4, 12);

    lv_obj_t* arrow = lv_label_create(card);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(arrow, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_18, 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -4, 0);

    lv_obj_set_user_data(card, (void*)(intptr_t)target_mode);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, menu_item_click_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(card);
    return card;
}

lv_obj_t* createSchoolHubScreen() {
    clearNavigationGroup();
    loadVailCourseProgress();

    lv_obj_t* screen = createScreen();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hdr = lv_label_create(screen);
    lv_label_set_text(hdr, "LEARN CW");
    lv_obj_set_style_text_font(hdr, getThemeFonts()->font_title, 0);
    lv_obj_set_style_text_color(hdr, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 16, 12);

    // Learn: show resume position if there is progress
    char learnDesc[48];
    bool hasProg = !(vailCourseProgress.currentModule == MODULE_LETTERS_1 &&
                     vailCourseProgress.currentLesson == 1 &&
                     vailCourseProgress.modulesCompleted == 0);
    if (hasProg)
        snprintf(learnDesc, sizeof(learnDesc), "Resume: %s  L%d",
                 vailCourseModuleNames[vailCourseProgress.currentModule],
                 vailCourseProgress.currentLesson);
    else
        snprintf(learnDesc, sizeof(learnDesc), "Structured lessons from zero");

    lv_obj_t* first = schoolHubTile(screen, "Learn",          learnDesc,                    MODE_VAIL_COURSE_MODULE_SELECT);
    lv_obj_set_pos(first, 16, 52);
    lv_obj_t* t2 = schoolHubTile(screen, "Daily Practice", "Mixed review - builds streak",  MODE_SCHOOL_DAILY);
    lv_obj_set_pos(t2, 16, 112);
    lv_obj_t* t3 = schoolHubTile(screen, "Copy Practice",  "Listen & type, open-ended",     MODE_SCHOOL_COPY);
    lv_obj_set_pos(t3, 16, 172);
    lv_obj_t* t4 = schoolHubTile(screen, "Send Practice",  "Key it - live decode",          MODE_SCHOOL_SEND);
    lv_obj_set_pos(t4, 16, 232);

    focusWidget(first);
    return screen;
}

// ============================================
// Send Practice (paddle + live decode)
// ============================================
// Reuses the proven practice oscillator pipeline (keyer + adaptive decoder)
// from training_practice.h. We show a target character; the user keys it on
// their paddle/straight key; the decoder produces a character; we compare and
// score against the same mastery as the copy modes. Engine lifecycle:
//   init    (initializeModeInt): startPracticeMode(tft) + schoolSendInit()
//   poll    (pollTable):         schoolSendPoll() -> updatePracticeOscillator() + compare
//   cleanup (cleanupTable):      schoolSendCleanup() -> practiceHandleEsc()

enum SchoolSendPhase { SSP_WAIT, SSP_FEEDBACK };

static lv_obj_t* s_send_screen  = NULL;
static lv_obj_t* s_send_target  = NULL;  // big target character
static lv_obj_t* s_send_prompt  = NULL;
static lv_obj_t* s_send_result  = NULL;  // "You sent: X"
static lv_obj_t* s_send_score   = NULL;
static String    s_send_pool    = "";
static char      s_send_target_ch = '?';
static int       s_send_correct = 0;
static int       s_send_total   = 0;
static SchoolSendPhase s_send_phase = SSP_WAIT;
static unsigned long   s_send_fb_until = 0;
static bool      s_send_active  = false;

static void schoolSendUpdateScore() {
    if (!s_send_score) return;
    int pct = (s_send_total > 0) ? (s_send_correct * 100 / s_send_total) : 0;
    lv_label_set_text_fmt(s_send_score, "%d/%d   %d%%", s_send_correct, s_send_total, pct);
}

static void schoolSendNewTarget() {
    s_send_target_ch = getVailCourseWeightedRandomChar(s_send_pool);
    s_send_phase = SSP_WAIT;
    practiceHandleClear();  // reset decoder + decoded text for this target
    if (s_send_target) {
        char str[2] = { s_send_target_ch, '\0' };
        lv_label_set_text(s_send_target, str);
        lv_obj_set_style_text_color(s_send_target, LV_COLOR_ACCENT_PRIMARY, 0);
    }
    if (s_send_prompt) {
        lv_label_set_text(s_send_prompt, "Send this character");
        lv_obj_set_style_text_color(s_send_prompt, LV_COLOR_TEXT_SECONDARY, 0);
    }
    if (s_send_result) lv_label_set_text(s_send_result, "");
}

// Called from initializeModeInt AFTER startPracticeMode(tft) has set up the
// keyer/decoder/audio for the current CW settings.
void schoolSendInit() {
    s_send_pool = getVailCourseLearnedChars();
    if (s_send_pool.length() == 0) s_send_pool = "KMRSUA";
    s_send_correct = 0;
    s_send_total = 0;
    s_send_active = true;
    schoolSendUpdateScore();
    schoolSendNewTarget();
}

// Called every main-loop iteration while in MODE_SCHOOL_SEND (pollTable).
void schoolSendPoll() {
    if (!s_send_active) return;
    updatePracticeOscillator();  // drive keyer + decoder

    if (s_send_phase == SSP_WAIT) {
        // A decoded character has arrived for this target (buffer was cleared
        // when the target was set, so any content is the user's attempt). The
        // decoder appends spaces/newlines for letter and word gaps, so scan back
        // for the last non-whitespace char and ignore a still-empty decode.
        int di = decodedText.length() - 1;
        while (di >= 0 && isspace((unsigned char)decodedText[di])) di--;
        if (di >= 0) {
            char sent = decodedText[di];
            bool correct = (toupper(sent) == toupper(s_send_target_ch));
            vailCourseRecordAnswer(s_send_target_ch, correct);
            s_send_total++;
            if (correct) s_send_correct++;
            schoolSendUpdateScore();

            s_send_phase = SSP_FEEDBACK;
            s_send_fb_until = millis() + (correct ? 700 : 1300);
            if (s_send_target)
                lv_obj_set_style_text_color(s_send_target, correct ? LV_COLOR_SUCCESS : LV_COLOR_ERROR, 0);
            if (s_send_result) {
                char str[2] = { (char)toupper(sent), '\0' };
                lv_label_set_text_fmt(s_send_result, "You sent  %s", str);
                lv_obj_set_style_text_color(s_send_result, correct ? LV_COLOR_SUCCESS : LV_COLOR_ERROR, 0);
            }
            if (s_send_prompt) {
                lv_label_set_text(s_send_prompt, correct ? "Correct!" : "Try the next one");
                lv_obj_set_style_text_color(s_send_prompt, correct ? LV_COLOR_SUCCESS : LV_COLOR_TEXT_SECONDARY, 0);
            }
        }
    } else if (s_send_phase == SSP_FEEDBACK) {
        if (millis() >= s_send_fb_until) schoolSendNewTarget();
    }
}

void schoolSendCleanup() {
    s_send_active = false;
    practiceHandleEsc();  // stop tone, tick timer, keyer; flush decoder
}

// SPACE plays the target as an audible reference (does not feed the decoder,
// which only listens to keyer events). ESC falls through to the global handler.
static void schoolSendKeyHandler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);
    if (key == ' ') {
        char str[2] = { s_send_target_ch, '\0' };
        requestPlayMorseStringFarnsworth(str, vailCourseProgress.characterWPM,
                                         vailCourseProgress.effectiveWPM, TONE_SIDETONE);
        lv_event_stop_processing(e);
    } else if (key == '\t' || key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
    }
}

static void schoolSendDeleteCb(lv_event_t* e) {
    (void)e;
    s_send_active = false;
    s_send_screen = NULL;
    s_send_target = s_send_prompt = s_send_result = s_send_score = NULL;
}

lv_obj_t* createSchoolSendScreen() {
    clearNavigationGroup();
    schoolSendDeleteCb(NULL);  // reset stale refs

    lv_obj_t* screen = createScreen();
    s_send_screen = screen;
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, schoolSendDeleteCb, LV_EVENT_DELETE, NULL);

    lv_obj_t* ttl = lv_label_create(screen);
    lv_label_set_text(ttl, "SEND PRACTICE");
    lv_obj_set_style_text_font(ttl, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(ttl, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, 14, 10);

    s_send_score = lv_label_create(screen);
    lv_obj_set_style_text_font(s_send_score, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(s_send_score, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(s_send_score, LV_ALIGN_TOP_RIGHT, -14, 12);

    s_send_target = lv_label_create(screen);
    lv_label_set_text(s_send_target, "?");
    lv_obj_set_style_text_font(s_send_target, getThemeFonts()->font_large, 0);
    lv_obj_set_style_text_color(s_send_target, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(s_send_target, LV_ALIGN_CENTER, 0, -22);

    s_send_prompt = lv_label_create(screen);
    lv_label_set_text(s_send_prompt, "Send this character");
    lv_obj_set_style_text_font(s_send_prompt, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(s_send_prompt, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(s_send_prompt, LV_ALIGN_CENTER, 0, 24);

    s_send_result = lv_label_create(screen);
    lv_label_set_text(s_send_result, "");
    lv_obj_set_style_text_font(s_send_result, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(s_send_result, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(s_send_result, LV_ALIGN_CENTER, 0, 56);

    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "Key it on your paddle    SPACE Hear    ESC Exit");
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(footer, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -8);

    // Focusable catcher: SPACE (reference) + ESC (global back handler).
    lv_obj_t* catcher = lv_obj_create(screen);
    lv_obj_set_size(catcher, 1, 1);
    lv_obj_set_style_bg_opa(catcher, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(catcher, 0, 0);
    lv_obj_clear_flag(catcher, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(catcher, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(catcher, schoolSendKeyHandler, LV_EVENT_KEY, NULL);
    addNavigableWidget(catcher);
    focusWidget(catcher);

    // Note: startPracticeMode(tft) + schoolSendInit() run from initializeModeInt
    // (where the LGFX display handle is in scope). The screen exists first so its
    // label refs are ready when schoolSendInit() sets the first target.
    return screen;
}

// Dispatcher used by createScreenForModeInt.
lv_obj_t* createSchoolScreenForMode(int mode) {
    switch (mode) {
        case MODE_SCHOOL_HUB:   return createSchoolHubScreen();
        case MODE_SCHOOL_DAILY: return createSchoolDailyScreen();
        case MODE_SCHOOL_COPY:  return createSchoolCopyScreen();
        case MODE_SCHOOL_SEND:  return createSchoolSendScreen();
        default: return NULL;
    }
}

#endif // LV_SCHOOL_SCREENS_H
