/*
 * VAIL SUMMIT - First-run device onboarding wizard
 */

#ifndef LV_ONBOARDING_SCREENS_H
#define LV_ONBOARDING_SCREENS_H

#include <lvgl.h>
#include <WiFi.h>
#include <ctype.h>
#include "lv_theme_summit.h"
#include "lv_widgets_summit.h"
#include "lv_screen_manager.h"
#include "../core/config.h"
#include "../core/modes.h"
#include "../settings/settings_onboarding.h"
#include "../settings/settings_general.h"
#include "../network/vail_repeater.h"
#include "../storage/sd_card.h"

extern void onLVGLMenuSelect(int target_mode);
extern void setCurrentModeFromInt(int mode);

enum OnboardStep {
    ONBOARD_WELCOME = 0,
    ONBOARD_CALLSIGN,
    ONBOARD_WIFI_PROMPT,
    ONBOARD_SD,
    ONBOARD_PORTS,
    ONBOARD_DONE,
    ONBOARD_STEP_COUNT
};

struct OnboardPortLabel {
    const char* label;
    const char* arrow;
    int16_t arrow_x;
    int16_t arrow_y;
    int16_t label_x;
    int16_t label_y;
};

enum OnboardPending {
    OB_PEND_NONE = 0,
    OB_PEND_RELOAD,
    OB_PEND_HOME,
    OB_PEND_WIFI
};

bool onboardingActive = false;
bool onboardingLaunchFromSettings = false;

static int onboardStep = ONBOARD_WELCOME;
static lv_obj_t* onboard_callsign_ta = NULL;
static lv_obj_t* onboard_screen = NULL;
static lv_obj_t* onboard_nav_btns[6];
static int onboard_nav_btn_count = 0;
static NavGridContext onboard_nav_ctx = { onboard_nav_btns, &onboard_nav_btn_count, 2 };
static OnboardPending onboardPending = OB_PEND_NONE;

// Square outline sits at pos (95,58) size 140x84 inside the diagram card.
// Top edge: Power + USB-C (close together). Right edge: Audio / Key / Radio.
static const OnboardPortLabel onboardPortLabels[] = {
    { "Power",     LV_SYMBOL_DOWN, 116, 44,  92, 28 },
    { "USB-C",     LV_SYMBOL_DOWN, 166, 44, 146, 28 },
    { "Audio Out", LV_SYMBOL_LEFT, 240, 64, 254, 62 },
    { "Key Input", LV_SYMBOL_LEFT, 240, 92, 254, 90 },
    { "Radio Out", LV_SYMBOL_LEFT, 240, 118, 254, 116 },
};

lv_obj_t* createOnboardingScreen();

static void onboarding_show_skip_confirm();

static void onboarding_reload_screen() {
    clearNavigationGroup();
    lv_obj_t* screen = createOnboardingScreen();
    if (screen != NULL) {
        loadScreen(screen, SCREEN_ANIM_SLIDE_LEFT);
        setCurrentModeFromInt(MODE_ONBOARDING);
    }
}

// Defer screen transitions out of key/click handlers (lv_async_call).
static void onboarding_process_pending(void* unused) {
    (void)unused;
    OnboardPending p = onboardPending;
    onboardPending = OB_PEND_NONE;
    switch (p) {
        case OB_PEND_RELOAD:
            onboarding_reload_screen();
            break;
        case OB_PEND_HOME:
            onLVGLMenuSelect(MODE_HOME);
            break;
        case OB_PEND_WIFI:
            onboardingActive = true;
            onboard_callsign_ta = NULL;
            onLVGLMenuSelect(MODE_WIFI_SETTINGS);
            break;
        default:
            break;
    }
}

static void onboarding_schedule(OnboardPending p) {
    lv_indev_t* indev = getLVGLKeypad();
    if (indev != NULL) lv_indev_wait_release(indev);
    onboardPending = p;
    lv_async_call(onboarding_process_pending, NULL);
}

static void onboarding_save_callsign_if_entered() {
    if (onboard_callsign_ta == NULL) return;
    const char* text = lv_textarea_get_text(onboard_callsign_ta);
    if (text == NULL || text[0] == '\0') return;

    char buf[CALLSIGN_MAX_LEN];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (size_t i = 0; buf[i]; i++) {
        buf[i] = (char)toupper((unsigned char)buf[i]);
    }
    saveCallsign(buf);
    vailCallsign = buf;
}

static void onboarding_finish_to_home(lv_event_t* e) {
    (void)e;
    markOnboardingComplete();
    onboardingActive = false;
    onboardingLaunchFromSettings = false;
    onboard_callsign_ta = NULL;
    onboarding_schedule(OB_PEND_HOME);
}

static void onboarding_skip_confirm_cb(lv_event_t* e) {
    (void)e;
    markOnboardingComplete();
    onboardingActive = false;
    onboardingLaunchFromSettings = false;
    onboard_callsign_ta = NULL;
    onboarding_schedule(OB_PEND_HOME);
}

static void onboarding_show_skip_confirm() {
    createConfirmDialog(
        "Skip setup?",
        "You can run this anytime from\nSettings > General > Device Tour",
        onboarding_skip_confirm_cb,
        nullptr);
}

static void onboarding_skip_btn_cb(lv_event_t* e) {
    (void)e;
    beep(TONE_MENU_NAV, BEEP_SHORT);
    onboarding_show_skip_confirm();
}

static bool onboarding_should_skip_wifi_step() {
    return WiFi.status() == WL_CONNECTED;
}

static void onboarding_advance_step() {
    if (onboardStep == ONBOARD_CALLSIGN) {
        onboarding_save_callsign_if_entered();
    }
    if (onboardStep >= ONBOARD_DONE) return;

    onboardStep++;
    if (onboardStep == ONBOARD_WIFI_PROMPT && onboarding_should_skip_wifi_step()) {
        onboardStep++;
    }
    onboarding_schedule(OB_PEND_RELOAD);
}

static void onboarding_next_btn_cb(lv_event_t* e) {
    (void)e;
    beep(TONE_SELECT, BEEP_MEDIUM);
    if (onboardStep == ONBOARD_DONE) {
        onboarding_finish_to_home(e);
        return;
    }
    onboarding_advance_step();
}

static void onboarding_wifi_connect_cb(lv_event_t* e) {
    (void)e;
    beep(TONE_SELECT, BEEP_MEDIUM);
    onboardStep = ONBOARD_SD;
    if (onboarding_should_skip_wifi_step()) {
        onboarding_schedule(OB_PEND_RELOAD);
        return;
    }
    onboarding_schedule(OB_PEND_WIFI);
}

static void onboarding_wifi_skip_cb(lv_event_t* e) {
    (void)e;
    beep(TONE_SELECT, BEEP_MEDIUM);
    onboardStep = ONBOARD_SD;
    onboarding_schedule(OB_PEND_RELOAD);
}

static void onboarding_btn_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_ENTER) {
        lv_event_send(lv_event_get_target(e), LV_EVENT_CLICKED, NULL);
        lv_event_stop_processing(e);
        return;
    }
    if (key == LV_KEY_ESC) {
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
        onboarding_show_skip_confirm();
    }
}

static void onboarding_callsign_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_ESC) {
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
        onboarding_show_skip_confirm();
        return;
    }
    if (key == LV_KEY_ENTER) {
        onboarding_save_callsign_if_entered();
        onboarding_advance_step();
        lv_event_stop_processing(e);
        return;
    }
    if (key == LV_KEY_DOWN && onboard_nav_btn_count > 0) {
        focusWidget(onboard_nav_btns[0]);
        lv_event_stop_processing(e);
    }
}

static void onboarding_style_btn(lv_obj_t* btn) {
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(btn, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_bg_color(btn, LV_COLOR_BG_CARD_ACTIVE, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_border_width(btn, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(btn, LV_COLOR_ACCENT_PRIMARY, LV_STATE_FOCUSED);
}

static lv_obj_t* onboarding_add_btn(lv_obj_t* parent, const char* text, lv_coord_t w,
                                    lv_event_cb_t cb) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_size(btn, w, 36);
    onboarding_style_btn(btn);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_color(lbl, LV_COLOR_TEXT_PRIMARY, LV_STATE_FOCUSED);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn, onboarding_btn_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(btn, grid_nav_handler, LV_EVENT_KEY, &onboard_nav_ctx);

    if (onboard_nav_btn_count < (int)(sizeof(onboard_nav_btns) / sizeof(onboard_nav_btns[0]))) {
        onboard_nav_btns[onboard_nav_btn_count++] = btn;
    }
    lv_group_add_obj(getLVGLInputGroup(), btn);
    return btn;
}

static void onboarding_focus_first_nav() {
    if (onboard_nav_btn_count > 0) {
        focusWidget(onboard_nav_btns[0]);
    }
}

static void onboarding_add_step_dots(lv_obj_t* screen, int current) {
    lv_obj_t* dots = lv_obj_create(screen);
    lv_obj_set_size(dots, SCREEN_WIDTH, 20);
    lv_obj_align(dots, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 2);
    lv_obj_set_style_bg_opa(dots, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dots, 0, 0);
    lv_obj_clear_flag(dots, LV_OBJ_FLAG_SCROLLABLE);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d/6", current + 1);
    lv_obj_t* step_lbl = lv_label_create(dots);
    lv_label_set_text(step_lbl, buf);
    lv_obj_set_style_text_font(step_lbl, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(step_lbl, LV_COLOR_TEXT_SECONDARY, 0);
}

static lv_obj_t* onboarding_add_corner_skip(lv_obj_t* screen) {
    lv_obj_t* skip = onboarding_add_btn(screen, "Skip", 64, onboarding_skip_btn_cb);
    lv_obj_set_height(skip, 26);
    lv_obj_t* lbl = lv_obj_get_child(skip, 0);
    if (lbl) {
        lv_obj_set_style_text_font(lbl, getThemeFonts()->font_small, 0);
        lv_obj_set_style_text_color(lbl, LV_COLOR_TEXT_SECONDARY, 0);
    }
    lv_obj_align(skip, LV_ALIGN_BOTTOM_LEFT, 6, -6);
    return skip;
}

static void onboarding_add_footer_buttons(lv_obj_t* screen, const char* next_label) {
    lv_obj_t* next = onboarding_add_btn(screen, next_label, 180, onboarding_next_btn_cb);
    lv_obj_align(next, LV_ALIGN_BOTTOM_MID, 0, -6);
    onboarding_add_corner_skip(screen);
}

static void onboarding_draw_ports_diagram(lv_obj_t* parent) {
    lv_obj_t* diagram = lv_obj_create(parent);
    lv_obj_set_size(diagram, SCREEN_WIDTH - 50, 200);
    lv_obj_align(diagram, LV_ALIGN_CENTER, 0, 0);
    applyCardStyle(diagram);
    lv_obj_set_style_pad_top(diagram, 2, 0);
    lv_obj_clear_flag(diagram, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* heading = lv_label_create(diagram);
    lv_label_set_text(heading, "Ports on your Summit");
    lv_obj_set_style_text_font(heading, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(heading, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 0);

    // Smaller device square, positioned so top + right edges have label room.
    lv_obj_t* outline = lv_obj_create(diagram);
    lv_obj_set_size(outline, 140, 84);
    lv_obj_set_pos(outline, 95, 58);
    lv_obj_set_style_bg_color(outline, LV_COLOR_BG_DEEP, 0);
    lv_obj_set_style_border_color(outline, LV_COLOR_BORDER_LIGHT, 0);
    lv_obj_set_style_border_width(outline, 2, 0);
    lv_obj_set_style_radius(outline, 8, 0);
    lv_obj_clear_flag(outline, LV_OBJ_FLAG_SCROLLABLE);

    for (size_t i = 0; i < sizeof(onboardPortLabels) / sizeof(onboardPortLabels[0]); i++) {
        const OnboardPortLabel* p = &onboardPortLabels[i];

        lv_obj_t* arrow = lv_label_create(diagram);
        lv_label_set_text(arrow, p->arrow);
        lv_obj_set_style_text_color(arrow, LV_COLOR_WARNING, 0);
        lv_obj_set_pos(arrow, p->arrow_x, p->arrow_y);

        lv_obj_t* name = lv_label_create(diagram);
        lv_label_set_text(name, p->label);
        lv_obj_set_style_text_font(name, getThemeFonts()->font_small, 0);
        lv_obj_set_style_text_color(name, LV_COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_pos(name, p->label_x, p->label_y);
    }

    lv_obj_t* legend = lv_label_create(diagram);
    lv_label_set_text(legend,
        "SD card slot is on the back of the display (optional, FAT32).");
    lv_obj_set_width(legend, lv_pct(100));
    lv_obj_set_style_text_font(legend, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(legend, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_align(legend, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(legend, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void startOnboarding(bool fromSettings) {
    onboardStep = ONBOARD_WELCOME;
    onboardingActive = false;
    onboardingLaunchFromSettings = fromSettings;
    onboard_callsign_ta = NULL;
    Serial.printf("[Onboard] Start (fromSettings=%d)\n", fromSettings);
}

void cleanupOnboardingScreen() {
    onboard_callsign_ta = NULL;
    onboard_screen = NULL;
}

void showOnboardingScreen() {
    clearNavigationGroup();
    lv_obj_t* screen = createOnboardingScreen();
    if (screen != NULL) {
        loadScreen(screen, SCREEN_ANIM_NONE);
        setCurrentModeFromInt(MODE_ONBOARDING);
    }
}

lv_obj_t* createOnboardingScreen() {
    onboard_callsign_ta = NULL;
    onboard_nav_btn_count = 0;

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    onboard_screen = screen;
    createCompactStatusBar(screen);

    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "DEVICE SETUP");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    onboarding_add_step_dots(screen, onboardStep);

    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_WIDTH - 40, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 54);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 24);
    applyCardStyle(content);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    const char* next_label = "Next";

    switch (onboardStep) {
        case ONBOARD_WELCOME: {
            lv_obj_t* heading = lv_label_create(content);
            lv_label_set_text(heading, "Welcome to your new Summit!");
            lv_obj_set_style_text_font(heading, getThemeFonts()->font_subtitle, 0);
            lv_obj_set_style_text_color(heading, LV_COLOR_ACCENT_PRIMARY, 0);
            lv_obj_set_style_text_align(heading, LV_TEXT_ALIGN_CENTER, 0);

            lv_obj_t* body = lv_label_create(content);
            lv_label_set_text(body, "Your portable CW trainer\nand practice device.");
            lv_obj_set_style_text_font(body, getThemeFonts()->font_body, 0);
            lv_obj_set_style_text_color(body, LV_COLOR_TEXT_SECONDARY, 0);
            lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
            break;
        }
        case ONBOARD_CALLSIGN: {
            lv_obj_t* prompt = lv_label_create(content);
            lv_label_set_text(prompt, "What should we call you?");
            lv_obj_set_style_text_font(prompt, getThemeFonts()->font_subtitle, 0);
            lv_obj_set_style_text_color(prompt, LV_COLOR_TEXT_PRIMARY, 0);

            lv_obj_t* hint = lv_label_create(content);
            lv_label_set_text(hint, "Callsign or name (optional)");
            lv_obj_set_style_text_font(hint, getThemeFonts()->font_small, 0);
            lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_SECONDARY, 0);

            onboard_callsign_ta = lv_textarea_create(content);
            lv_obj_set_size(onboard_callsign_ta, 260, 44);
            lv_textarea_set_one_line(onboard_callsign_ta, true);
            lv_textarea_set_max_length(onboard_callsign_ta, 12);
            lv_textarea_set_placeholder_text(onboard_callsign_ta, "e.g. W1ABC");
            if (vailCallsign.length() > 0 && vailCallsign != "GUEST") {
                lv_textarea_set_text(onboard_callsign_ta, vailCallsign.c_str());
            }
            applyTextareaStyle(onboard_callsign_ta);
            lv_obj_add_event_cb(onboard_callsign_ta, onboarding_callsign_key_handler, LV_EVENT_KEY, NULL);
            addNavigableWidget(onboard_callsign_ta);
            break;
        }
        case ONBOARD_WIFI_PROMPT: {
            lv_obj_t* heading = lv_label_create(content);
            if (onboarding_should_skip_wifi_step()) {
                lv_label_set_text(heading, "WiFi connected");
                lv_obj_set_style_text_color(heading, LV_COLOR_SUCCESS, 0);
            } else {
                lv_label_set_text(heading, "Connect to WiFi?");
                lv_obj_set_style_text_color(heading, LV_COLOR_ACCENT_PRIMARY, 0);
            }
            lv_obj_set_style_text_font(heading, getThemeFonts()->font_subtitle, 0);

            lv_obj_t* body = lv_label_create(content);
            if (onboarding_should_skip_wifi_step()) {
                lv_label_set_text(body, "You're already on a network.\nPress Next to continue.");
            } else {
                lv_label_set_text(body,
                    "Optional. Unlocks the web UI,\nweb file updates, and online\nfeatures from your network.");
            }
            lv_obj_set_style_text_font(body, getThemeFonts()->font_body, 0);
            lv_obj_set_style_text_color(body, LV_COLOR_TEXT_SECONDARY, 0);
            lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);

            if (!onboarding_should_skip_wifi_step()) {
                lv_obj_t* btn_row = lv_obj_create(content);
                lv_obj_set_size(btn_row, 360, 44);
                lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(btn_row, 0, 0);
                lv_obj_set_layout(btn_row, LV_LAYOUT_FLEX);
                lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

                onboarding_add_btn(btn_row, "Connect", 150, onboarding_wifi_connect_cb);
                onboarding_add_btn(btn_row, "Not now", 150, onboarding_wifi_skip_cb);
            }
            break;
        }
        case ONBOARD_SD: {
            if (!sdCardAvailable) {
                initSDCard();
            }

            lv_obj_t* heading = lv_label_create(content);
            if (sdCardAvailable) {
                char title[48];
                snprintf(title, sizeof(title), "SD card ready (%llu MB)", (unsigned long long)sdCardSize);
                lv_label_set_text(heading, title);
                lv_obj_set_style_text_color(heading, LV_COLOR_SUCCESS, 0);
            } else {
                lv_label_set_text(heading, "No SD card detected");
                lv_obj_set_style_text_color(heading, LV_COLOR_WARNING, 0);
            }
            lv_obj_set_style_text_font(heading, getThemeFonts()->font_subtitle, 0);
            lv_obj_set_style_text_align(heading, LV_TEXT_ALIGN_CENTER, 0);

            lv_obj_t* body = lv_label_create(content);
            if (sdCardAvailable) {
                lv_label_set_text(body,
                    "FAT32 card detected.\nUnlocks QSO logging, license study,\nand web UI file storage.");
            } else {
                lv_label_set_text(body,
                    "No problem. The slot is on the back\nof the display board. Add a 4-32 GB\nFAT32 card anytime. Features will\nprompt you when needed.");
            }
            lv_obj_set_style_text_font(body, getThemeFonts()->font_body, 0);
            lv_obj_set_style_text_color(body, LV_COLOR_TEXT_SECONDARY, 0);
            lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_line_space(body, 4, 0);
            break;
        }
        case ONBOARD_PORTS: {
            lv_obj_clean(content);
            lv_obj_set_size(content, SCREEN_WIDTH - 40, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 54);
            lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
            onboarding_draw_ports_diagram(content);
            onboarding_add_footer_buttons(screen, "Next");
            {
                lv_group_t* grp = getLVGLInputGroup();
                if (grp) lv_group_set_editing(grp, false);
            }
            onboarding_focus_first_nav();
            return screen;
        }
        case ONBOARD_DONE: {
            lv_obj_t* heading = lv_label_create(content);
            lv_label_set_text(heading, "You're all set!");
            lv_obj_set_style_text_font(heading, getThemeFonts()->font_subtitle, 0);
            lv_obj_set_style_text_color(heading, LV_COLOR_SUCCESS, 0);

            lv_obj_t* tips = lv_label_create(content);
            lv_label_set_text(tips,
                "Home dashboard launches features\n"
                "V opens quick settings\n"
                "Settings has everything else");
            lv_obj_set_style_text_font(tips, getThemeFonts()->font_body, 0);
            lv_obj_set_style_text_color(tips, LV_COLOR_TEXT_SECONDARY, 0);
            lv_obj_set_style_text_align(tips, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_line_space(tips, 6, 0);
            next_label = "Go to Home";
            break;
        }
        default:
            break;
    }

    if (onboardStep == ONBOARD_WIFI_PROMPT && !onboarding_should_skip_wifi_step()) {
        onboarding_add_corner_skip(screen);
    } else {
        onboarding_add_footer_buttons(screen, next_label);
    }

    if (onboardStep == ONBOARD_CALLSIGN && onboard_callsign_ta) {
        focusWidget(onboard_callsign_ta);
        lv_group_t* grp = getLVGLInputGroup();
        if (grp) lv_group_set_editing(grp, true);
    } else {
        lv_group_t* grp = getLVGLInputGroup();
        if (grp) lv_group_set_editing(grp, false);
        onboarding_focus_first_nav();
    }
    return screen;
}

#endif // LV_ONBOARDING_SCREENS_H
