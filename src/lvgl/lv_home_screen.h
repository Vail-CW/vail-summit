/*
 * VAIL SUMMIT - Home Screen Dashboard
 *
 * The new root/boot screen. A glanceable dashboard rather than a menu:
 *   - Status strip:  callsign | mailbox | streak | WiFi | battery
 *   - Hero card:     "Continue Learning" (resumes the Vail course) with a
 *                    day-one "Start Here" fallback when there is no progress
 *   - Side widget:   live band conditions when available, else a practice
 *                    summary (streak / today / total) so it is useful offline
 *   - Launcher row:  five fixed destinations into the existing menu structure
 *
 * Phase 0 of the simplification effort (see docs/PRDs/summit-simplification-prd.md).
 * Wired to real data; nothing is deleted — the old grid menu remains reachable
 * via the "More" launcher (MODE_MAIN_MENU), whose ESC parent is now MODE_HOME.
 *
 * Navigation: a true 2-D feel (UP/DOWN between the hero and the launcher row,
 * LEFT/RIGHT within the row) built with the sanctioned grid_nav_handler — the
 * hero is represented as a full-width top row by repeating its pointer across
 * the geometry array's first row (see createHomeScreen).
 *
 * Back navigation: activating the hero or a launcher arms homeLaunchTarget so
 * ESC from the launched destination returns to the dashboard rather than
 * walking the legacy menu hierarchy (consumed in getParentModeInt).
 *
 * INCLUDE ORDER: this header must be included AFTER lv_menu_screens.h (for the
 * menu-select callback / grid_nav_handler / the widget factory) and after the
 * data-source headers, but BEFORE lv_mode_integration.h (which references
 * createHomeScreen() / cleanupHomeScreen() / homeLaunchTarget). See vail-summit.ino.
 */

#ifndef LV_HOME_SCREEN_H
#define LV_HOME_SCREEN_H

#include <lvgl.h>
#include <Preferences.h>
#include "lv_screen_manager.h"
#include "lv_widgets_summit.h"
#include "lv_theme_summit.h"
#include "../core/modes.h"
#include "../network/band_conditions.h"
#include "../training/training_vail_course_core.h"

// ============================================
// State (single-TU file scope)
// ============================================

static lv_obj_t* s_home_screen   = NULL;
static lv_timer_t* s_home_timer  = NULL;

// Dynamic refs updated by the refresh timer
static lv_obj_t* s_home_batt     = NULL;
static lv_obj_t* s_home_wifi     = NULL;
static lv_obj_t* s_home_mailbox  = NULL;
static lv_obj_t* s_home_streak   = NULL;
static lv_obj_t* s_home_side_l1  = NULL;  // side widget line 1
static lv_obj_t* s_home_side_l2  = NULL;  // side widget line 2
static lv_obj_t* s_home_side_l3  = NULL;  // side widget line 3
static lv_obj_t* s_home_side_ttl = NULL;  // side widget title

// Navigation geometry: 5-wide grid where row 0 is the hero (its pointer
// repeated across all 5 columns) and row 1 is the five launchers. Gives
// grid_nav_handler a uniform grid to reason about while the hero spans the top.
static lv_obj_t* s_home_nav[10];
static int s_home_nav_count = 0;
static NavGridContext s_home_nav_ctx = { s_home_nav, &s_home_nav_count, 5 };

// Armed when the hero/a launcher is activated; consumed in getParentModeInt so
// ESC from the launched mode returns to MODE_HOME. -1 = inactive.
int homeLaunchTarget = -1;

// Forward decls for getters that live in headers included before this one.
extern int batteryPercent;

// ============================================
// Data helpers
// ============================================

// Read the saved callsign straight from Preferences (namespace/key confirmed
// in settings_callsign.h). Keeps this screen independent of include order for
// that module. Falls back to "GUEST".
static void homeReadCallsign(char* out, size_t len) {
    Preferences p;
    p.begin("callsign", true);
    String v = p.getString("call", "");
    p.end();
    if (v.length() == 0) {
        strncpy(out, "GUEST", len - 1);
    } else {
        strncpy(out, v.c_str(), len - 1);
    }
    out[len - 1] = '\0';
}

// True if the user has made ANY progress in the Vail course (else day-one).
static bool homeHasTrainingProgress() {
    return !(vailCourseProgress.currentModule == MODULE_LETTERS_1 &&
             vailCourseProgress.currentLesson == 1 &&
             vailCourseProgress.modulesCompleted == 0);
}

static int homeCountCompletedModules() {
    int n = 0;
    uint32_t bits = vailCourseProgress.modulesCompleted;
    while (bits) { n += (bits & 1); bits >>= 1; }
    return n;
}

// Apply battery glyph + color to a label (mirrors createHeader behaviour).
static void homeApplyBattery(lv_obj_t* lbl) {
    if (!lbl) return;
    if (batteryPercent > 80)      { lv_label_set_text(lbl, LV_SYMBOL_BATTERY_FULL);  lv_obj_set_style_text_color(lbl, LV_COLOR_SUCCESS, 0); }
    else if (batteryPercent > 60) { lv_label_set_text(lbl, LV_SYMBOL_BATTERY_3);     lv_obj_set_style_text_color(lbl, LV_COLOR_SUCCESS, 0); }
    else if (batteryPercent > 40) { lv_label_set_text(lbl, LV_SYMBOL_BATTERY_2);     lv_obj_set_style_text_color(lbl, LV_COLOR_ACCENT_PRIMARY, 0); }
    else if (batteryPercent > 20) { lv_label_set_text(lbl, LV_SYMBOL_BATTERY_1);     lv_obj_set_style_text_color(lbl, LV_COLOR_WARNING, 0); }
    else                          { lv_label_set_text(lbl, LV_SYMBOL_BATTERY_EMPTY); lv_obj_set_style_text_color(lbl, LV_COLOR_ERROR, 0); }
}

static void homeApplyWifi(lv_obj_t* lbl) {
    if (!lbl) return;
    InternetStatus s = getInternetStatus();
    if (s == INET_CONNECTED || s == INET_CHECKING) lv_obj_set_style_text_color(lbl, LV_COLOR_SUCCESS, 0);
    else if (s == INET_WIFI_ONLY)                   lv_obj_set_style_text_color(lbl, LV_COLOR_WARNING, 0);
    else                                            lv_obj_set_style_text_color(lbl, LV_COLOR_ERROR, 0);
}

static void homeApplyStreak(lv_obj_t* lbl) {
    if (!lbl) return;
    int streak = getPracticeStreak();
    lv_label_set_text_fmt(lbl, "%dd", streak);
    lv_obj_set_style_text_color(lbl, streak > 0 ? LV_COLOR_ACCENT_PRIMARY : LV_COLOR_TEXT_TERTIARY, 0);
}

static void homeApplyMailbox(lv_obj_t* lbl) {
    if (!lbl) return;
    if (hasUnreadMailboxMessages()) lv_obj_clear_flag(lbl, LV_OBJ_FLAG_HIDDEN);
    else                            lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
}

// Fill the side widget: live bands if we have valid cached data, else a
// practice summary that is always available (useful offline).
static void homeFillSideWidget() {
    if (!s_home_side_ttl) return;

    if (bandConditionsData.valid) {
        lv_label_set_text(s_home_side_ttl, "ON THE BANDS");
        if (s_home_side_l1)
            lv_label_set_text_fmt(s_home_side_l1, "20m  %s",
                getBandConditionText(bandConditionsData.hf_30m_20m.day));
        if (s_home_side_l2)
            lv_label_set_text_fmt(s_home_side_l2, "40m  %s",
                getBandConditionText(bandConditionsData.hf_80m_40m.day));
        if (s_home_side_l3)
            lv_label_set_text_fmt(s_home_side_l3, "SFI %d   K %d",
                bandConditionsData.solar.solarFlux, bandConditionsData.solar.kIndex);
    } else {
        lv_label_set_text(s_home_side_ttl, "YOUR PRACTICE");
        if (s_home_side_l1)
            lv_label_set_text_fmt(s_home_side_l1, "Streak  %dd", getPracticeStreak());
        if (s_home_side_l2)
            lv_label_set_text_fmt(s_home_side_l2, "Today   %lum",
                (unsigned long)(getTodayPracticeSeconds() / 60));
        if (s_home_side_l3)
            lv_label_set_text_fmt(s_home_side_l3, "Total   %luh",
                (unsigned long)(getTotalPracticeSeconds() / 3600));
    }
}

// ============================================
// Refresh timer
// ============================================

static void homeRefreshTimerCb(lv_timer_t* t) {
    (void)t;
    if (s_home_timer == NULL) return;
    if (lv_scr_act() != s_home_screen) return;  // guard against transition window
    homeApplyBattery(s_home_batt);
    homeApplyWifi(s_home_wifi);
    homeApplyStreak(s_home_streak);
    homeApplyMailbox(s_home_mailbox);
    homeFillSideWidget();
}

// Tie timer lifetime to the screen: fires on both forward and back navigation
// when loadScreen() auto-deletes the old screen. Belt-and-suspenders with the
// cleanupTable entry; both null the pointer so neither double-frees.
static void homeScreenDeleteCb(lv_event_t* e) {
    (void)e;
    if (s_home_timer) { lv_timer_del(s_home_timer); s_home_timer = NULL; }
    s_home_screen  = NULL;
    s_home_batt = s_home_wifi = s_home_mailbox = s_home_streak = NULL;
    s_home_side_l1 = s_home_side_l2 = s_home_side_l3 = s_home_side_ttl = NULL;
}

// ============================================
// Activation handler (hero + launchers)
// ============================================

// Arm the back-to-home override, then route via the menu-select callback.
static void homeLauncherClickHandler(lv_event_t* e) {
    lv_obj_t* t = lv_event_get_target(e);
    int target = (int)(intptr_t)lv_obj_get_user_data(t);
    homeLaunchTarget = target;
    if (menu_select_callback != NULL) menu_select_callback(target);
}

// ============================================
// Build helpers
// ============================================

// One launcher tile: icon over label, clickable. Nav wiring is done centrally
// in createHomeScreen so the whole dashboard shares one grid context.
static lv_obj_t* homeMakeLauncher(lv_obj_t* parent, const char* icon, const char* label,
                                  int target_mode, int x, int y, int w, int h) {
    lv_obj_t* tile = lv_obj_create(parent);
    lv_obj_set_size(tile, w, h);
    lv_obj_set_pos(tile, x, y);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    applyMenuCardStyle(tile);
    lv_obj_set_style_pad_all(tile, 4, 0);

    lv_obj_t* ic = lv_label_create(tile);
    lv_label_set_text(ic, icon);
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ic, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t* lb = lv_label_create(tile);
    lv_label_set_text(lb, label);
    lv_obj_set_style_text_font(lb, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(lb, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(lb, LV_ALIGN_BOTTOM_MID, 0, -2);

    lv_obj_set_user_data(tile, (void*)(intptr_t)target_mode);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tile, homeLauncherClickHandler, LV_EVENT_CLICKED, NULL);
    return tile;
}

// Attach the shared grid nav handler + group membership to a focusable.
static void homeAttachNav(lv_obj_t* w) {
    lv_obj_add_event_cb(w, grid_nav_handler, LV_EVENT_KEY, &s_home_nav_ctx);
    addNavigableWidget(w);
}

// ============================================
// Screen
// ============================================

lv_obj_t* createHomeScreen() {
    clearNavigationGroup();
    homeScreenDeleteCb(NULL);  // reset stale refs from a prior instance
    homeLaunchTarget = -1;     // back home: disarm any stale override

    // Load latest training progress for the hero card.
    loadVailCourseProgress();

    lv_obj_t* screen = createScreen();
    s_home_screen = screen;
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, homeScreenDeleteCb, LV_EVENT_DELETE, NULL);

    // ---- Status strip ----
    lv_obj_t* strip = lv_obj_create(screen);
    lv_obj_set_size(strip, SCREEN_WIDTH, 30);
    lv_obj_set_pos(strip, 0, 0);
    lv_obj_set_style_bg_color(strip, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(strip, 0, 0);
    lv_obj_set_style_radius(strip, 0, 0);
    lv_obj_set_style_pad_all(strip, 0, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

    char call[12];
    homeReadCallsign(call, sizeof(call));
    lv_obj_t* call_lbl = lv_label_create(strip);
    lv_label_set_text(call_lbl, call);
    lv_obj_set_style_text_font(call_lbl, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(call_lbl, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(call_lbl, LV_ALIGN_LEFT_MID, 12, 0);

    s_home_batt = lv_label_create(strip);
    lv_obj_set_style_text_font(s_home_batt, &lv_font_montserrat_20, 0);
    lv_obj_align(s_home_batt, LV_ALIGN_RIGHT_MID, -10, 0);
    homeApplyBattery(s_home_batt);

    s_home_wifi = lv_label_create(strip);
    lv_label_set_text(s_home_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(s_home_wifi, &lv_font_montserrat_20, 0);
    lv_obj_align(s_home_wifi, LV_ALIGN_RIGHT_MID, -44, 0);
    homeApplyWifi(s_home_wifi);

    s_home_streak = lv_label_create(strip);
    lv_obj_set_style_text_font(s_home_streak, getThemeFonts()->font_body, 0);
    lv_obj_align(s_home_streak, LV_ALIGN_RIGHT_MID, -78, 0);
    homeApplyStreak(s_home_streak);

    s_home_mailbox = lv_label_create(strip);
    lv_label_set_text(s_home_mailbox, LV_SYMBOL_ENVELOPE);
    lv_obj_set_style_text_font(s_home_mailbox, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_home_mailbox, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(s_home_mailbox, LV_ALIGN_RIGHT_MID, -120, 0);
    homeApplyMailbox(s_home_mailbox);

    // ---- Hero card (Continue Learning) ----
    const int contentY = 38;
    const int contentH = 196;
    lv_obj_t* hero = lv_obj_create(screen);
    lv_obj_set_size(hero, 286, contentH);
    lv_obj_set_pos(hero, 8, contentY);
    lv_obj_clear_flag(hero, LV_OBJ_FLAG_SCROLLABLE);
    applyMenuCardStyle(hero);
    lv_obj_set_style_pad_all(hero, 14, 0);

    bool hasProgress = homeHasTrainingProgress();

    lv_obj_t* hero_kicker = lv_label_create(hero);
    lv_label_set_text(hero_kicker, hasProgress ? "CONTINUE LEARNING" : "START HERE");
    lv_obj_set_style_text_font(hero_kicker, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(hero_kicker, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(hero_kicker, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* hero_title = lv_label_create(hero);
    if (hasProgress) {
        lv_label_set_text_fmt(hero_title, "%s",
            vailCourseModuleNames[vailCourseProgress.currentModule]);
    } else {
        lv_label_set_text(hero_title, "Learn Morse Code");
    }
    lv_obj_set_style_text_font(hero_title, getThemeFonts()->font_title, 0);
    lv_obj_set_style_text_color(hero_title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(hero_title, LV_ALIGN_TOP_LEFT, 0, 22);

    lv_obj_t* hero_sub = lv_label_create(hero);
    if (hasProgress) {
        lv_label_set_text_fmt(hero_sub, "Lesson %d", vailCourseProgress.currentLesson);
    } else {
        lv_label_set_text(hero_sub, "From zero. One sound at a time.");
    }
    lv_obj_set_style_text_font(hero_sub, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(hero_sub, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(hero_sub, LV_ALIGN_TOP_LEFT, 0, 58);

    // Progress bar: completed modules out of total. Only meaningful once the
    // user has started — on day one an empty gauge reads as discouraging, so
    // we omit it and let the "Start Here" card breathe.
    if (hasProgress) {
        lv_obj_t* bar = lv_bar_create(hero);
        lv_obj_set_size(bar, 258, 10);
        lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 96);
        lv_bar_set_range(bar, 0, (int)MODULE_COUNT);
        lv_bar_set_value(bar, homeCountCompletedModules(), LV_ANIM_OFF);
        lv_obj_add_style(bar, getStyleBar(), 0);
        lv_obj_add_style(bar, getStyleBarIndicator(), LV_PART_INDICATOR);

        lv_obj_t* hero_prog = lv_label_create(hero);
        lv_label_set_text_fmt(hero_prog, "%d / %d modules", homeCountCompletedModules(), (int)MODULE_COUNT);
        lv_obj_set_style_text_font(hero_prog, getThemeFonts()->font_small, 0);
        lv_obj_set_style_text_color(hero_prog, LV_COLOR_TEXT_TERTIARY, 0);
        lv_obj_align(hero_prog, LV_ALIGN_TOP_LEFT, 0, 112);
    }

    lv_obj_t* hero_hint = lv_label_create(hero);
    lv_label_set_text(hero_hint, hasProgress ? LV_SYMBOL_PLAY " Resume lesson"
                                             : LV_SYMBOL_PLAY " Start learning");
    lv_obj_set_style_text_font(hero_hint, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(hero_hint, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(hero_hint, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Hero is the primary action: resume/start the Vail course module picker.
    lv_obj_set_user_data(hero, (void*)(intptr_t)MODE_SCHOOL_HUB);
    lv_obj_add_flag(hero, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hero, homeLauncherClickHandler, LV_EVENT_CLICKED, NULL);

    // ---- Side widget (bands / practice) ----
    lv_obj_t* side = lv_obj_create(screen);
    lv_obj_set_size(side, 170, contentH);
    lv_obj_set_pos(side, 302, contentY);
    lv_obj_clear_flag(side, LV_OBJ_FLAG_SCROLLABLE);
    applyCardStyle(side);
    lv_obj_set_style_pad_all(side, 12, 0);

    s_home_side_ttl = lv_label_create(side);
    lv_obj_set_style_text_font(s_home_side_ttl, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(s_home_side_ttl, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(s_home_side_ttl, LV_ALIGN_TOP_LEFT, 0, 0);

    s_home_side_l1 = lv_label_create(side);
    lv_obj_set_style_text_font(s_home_side_l1, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(s_home_side_l1, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(s_home_side_l1, LV_ALIGN_TOP_LEFT, 0, 34);

    s_home_side_l2 = lv_label_create(side);
    lv_obj_set_style_text_font(s_home_side_l2, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(s_home_side_l2, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(s_home_side_l2, LV_ALIGN_TOP_LEFT, 0, 70);

    s_home_side_l3 = lv_label_create(side);
    lv_obj_set_style_text_font(s_home_side_l3, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(s_home_side_l3, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(s_home_side_l3, LV_ALIGN_TOP_LEFT, 0, 110);

    homeFillSideWidget();

    // ---- Launcher row (fixed five) ----
    const int rowY = 242;
    const int rowH = 70;
    const int gap  = 6;
    const int tileW = (SCREEN_WIDTH - (gap * 6)) / 5;  // 5 tiles, 6 gaps
    lv_obj_t* launchers[5];
    int x = gap;
    launchers[0] = homeMakeLauncher(screen, LV_SYMBOL_AUDIO, "Practice", MODE_PRACTICE,        x, rowY, tileW, rowH); x += tileW + gap;
    launchers[1] = homeMakeLauncher(screen, LV_SYMBOL_WIFI,  "Repeater", MODE_VAIL_REPEATER,   x, rowY, tileW, rowH); x += tileW + gap;
    launchers[2] = homeMakeLauncher(screen, LV_SYMBOL_SAVE,  "Log QSO",  MODE_QSO_LOGGER_MENU, x, rowY, tileW, rowH); x += tileW + gap;
    launchers[3] = homeMakeLauncher(screen, LV_SYMBOL_PLAY,  "Games",    MODE_GAMES_MENU,      x, rowY, tileW, rowH); x += tileW + gap;
    launchers[4] = homeMakeLauncher(screen, LV_SYMBOL_LIST,  "More",     MODE_MAIN_MENU,       x, rowY, tileW, rowH);

    // Build the 2-D nav geometry: hero spans the top row (pointer repeated
    // across all 5 columns), launchers occupy the bottom row. The hero and
    // each launcher are added to the input group exactly once.
    s_home_nav_count = 0;
    for (int i = 0; i < 5; i++) s_home_nav[s_home_nav_count++] = hero;
    for (int i = 0; i < 5; i++) s_home_nav[s_home_nav_count++] = launchers[i];

    homeAttachNav(hero);
    for (int i = 0; i < 5; i++) homeAttachNav(launchers[i]);

    // Default focus on the hero so ENTER from cold boot resumes/starts learning.
    focusWidget(hero);

    // Periodic refresh of live values; tied to screen lifetime via DELETE cb.
    s_home_timer = lv_timer_create(homeRefreshTimerCb, 3000, NULL);

    return screen;
}

// Cleanup entry (cleanupTable). Safe alongside the DELETE handler — both null
// the timer pointer so neither double-frees.
void cleanupHomeScreen() {
    if (s_home_timer) { lv_timer_del(s_home_timer); s_home_timer = NULL; }
}

#endif // LV_HOME_SCREEN_H
