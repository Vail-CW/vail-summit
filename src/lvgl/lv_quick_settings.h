/*
 * Quick Settings Overlay
 *
 * A modal panel (on lv_layer_top) for instant live control of the four settings
 * a user reaches for most: Volume, Brightness, CW Speed, and Sidetone. Opened by
 * the global hotkey (V) from any pure-navigation menu; dismissed with ENTER/ESC.
 * It never changes currentMode — it floats above whatever screen is active.
 *
 * Input: while the overlay is open, handleGlobalHotkey() (which runs on EVERY
 * key before LVGL) routes all keys straight here via quickSettingsHandleKey().
 * We do NOT lean on LVGL's keypad-group/editing-mode semantics — those fight a
 * slider (edit mode swallows ESC; nav mode swallows the arrows), which is what
 * broke ESC and up/down navigation. Selection is tracked and drawn ourselves.
 */

#ifndef LV_QUICK_SETTINGS_H
#define LV_QUICK_SETTINGS_H

#include "lvgl.h"
#include "../core/config.h"

enum QSKind { QS_VOLUME, QS_BRIGHT, QS_SPEED, QS_TONE };

struct QSRow {
    QSKind      kind;
    int         min, max, step;
    const char* unit;
};

static lv_obj_t* s_qs_backdrop      = NULL;
static lv_obj_t* s_qs_rowObj[4]     = { NULL, NULL, NULL, NULL };
static lv_obj_t* s_qs_slider[4]     = { NULL, NULL, NULL, NULL };
static lv_obj_t* s_qs_valLabel[4]   = { NULL, NULL, NULL, NULL };
static QSRow     s_qs_rows[4];
static int       s_qs_index = 0;

bool isQuickSettingsOpen() { return s_qs_backdrop != NULL; }

// Read the live value of a setting.
static int qsCurrentValue(int i) {
    switch (s_qs_rows[i].kind) {
        case QS_VOLUME: return getVolume();
        case QS_BRIGHT: return getBrightness();
        case QS_SPEED:  return cwSpeed;
        case QS_TONE:   return cwTone;
    }
    return 0;
}

// Apply a value live to the underlying setting (and persist it).
static void qsApply(int i, int v) {
    switch (s_qs_rows[i].kind) {
        case QS_VOLUME: setVolume(v);     beep(TONE_MENU_NAV, 40); break;  // audible level
        case QS_BRIGHT: setBrightness(v);                          break;
        case QS_SPEED:  cwSpeed = v; markDeferredSave(saveCWSettings);             break;
        case QS_TONE:   cwTone  = v; markDeferredSave(saveCWSettings); beep(cwTone, 60); break;  // preview tone
    }
}

// Draw the selection highlight (outline on the selected row).
static void qsUpdateSelection() {
    for (int i = 0; i < 4; i++) {
        if (!s_qs_rowObj[i]) continue;
        bool sel = (i == s_qs_index);
        lv_obj_set_style_outline_width(s_qs_rowObj[i], sel ? 3 : 0, 0);
        lv_obj_set_style_outline_color(s_qs_rowObj[i], LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_set_style_outline_pad(s_qs_rowObj[i], 2, 0);
    }
}

// Adjust the currently selected setting by +/- step (steps = acceleration multiplier).
static void qsAdjust(int dir, int steps = 1) {
    int i = s_qs_index;
    QSRow& r = s_qs_rows[i];
    int v = qsCurrentValue(i) + dir * r.step * steps;
    if (v < r.min) v = r.min;
    if (v > r.max) v = r.max;
    qsApply(i, v);
    if (s_qs_slider[i])   lv_slider_set_value(s_qs_slider[i], v, LV_ANIM_OFF);
    if (s_qs_valLabel[i]) lv_label_set_text_fmt(s_qs_valLabel[i], "%d %s", v, r.unit);
}

// Key-repeat acceleration for held LEFT/RIGHT: consecutive repeats of the same
// key (arriving within the CardKB auto-repeat cadence) ramp the step 1x->2x->4x.
static char     s_qs_accel_key = 0;
static uint32_t s_qs_accel_last_ms = 0;
static int      s_qs_accel_count = 0;

static int qsAccelSteps(char key) {
    uint32_t now = millis();
    if (key == s_qs_accel_key && (now - s_qs_accel_last_ms) < 300) {
        s_qs_accel_count++;
    } else {
        s_qs_accel_key = key;
        s_qs_accel_count = 0;
    }
    s_qs_accel_last_ms = now;
    if (s_qs_accel_count >= 12) return 4;
    if (s_qs_accel_count >= 4)  return 2;
    return 1;
}

void closeQuickSettings() {
    if (!s_qs_backdrop) return;
    lv_obj_del(s_qs_backdrop);
    s_qs_backdrop = NULL;
    for (int i = 0; i < 4; i++) { s_qs_rowObj[i] = s_qs_slider[i] = s_qs_valLabel[i] = NULL; }
    // Discard the in-flight key release so ENTER doesn't synthesize a CLICK below.
    lv_indev_t* indev = getLVGLKeypad();
    if (indev) lv_indev_wait_release(indev);
}

// Handle one raw CardKB key while the overlay is open. Called from
// handleGlobalHotkey(), which consumes the key so LVGL never sees it.
void quickSettingsHandleKey(char key) {
    if (!s_qs_backdrop) return;
    if (key == KEY_UP) {
        if (s_qs_index > 0) { s_qs_index--; qsUpdateSelection(); beep(TONE_MENU_NAV, 25); }
    } else if (key == KEY_DOWN) {
        if (s_qs_index < 3) { s_qs_index++; qsUpdateSelection(); beep(TONE_MENU_NAV, 25); }
    } else if (key == KEY_LEFT) {
        qsAdjust(-1, qsAccelSteps(key));
    } else if (key == KEY_RIGHT) {
        qsAdjust(+1, qsAccelSteps(key));
    } else if (key == KEY_ENTER || key == KEY_ENTER_ALT || key == KEY_ESC) {
        closeQuickSettings();
    }
    // Any other key (including V) is ignored while the overlay is open.
}

static void qsBuildRow(lv_obj_t* parent, int i, const char* label) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 6, 0);
    lv_obj_set_style_pad_row(row, 2, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    s_qs_rowObj[i] = row;

    lv_obj_t* header = lv_obj_create(row);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(header);
    lv_label_set_text(lbl, label);
    lv_obj_add_style(lbl, getStyleLabelBody(), 0);

    s_qs_valLabel[i] = lv_label_create(header);
    lv_obj_set_style_text_color(s_qs_valLabel[i], LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(s_qs_valLabel[i], getThemeFonts()->font_body, 0);

    lv_obj_t* slider = lv_slider_create(row);
    lv_obj_set_width(slider, lv_pct(100));
    lv_obj_set_height(slider, 10);
    lv_slider_set_range(slider, s_qs_rows[i].min, s_qs_rows[i].max);
    applySliderStyle(slider);
    s_qs_slider[i] = slider;

    int v = qsCurrentValue(i);
    lv_slider_set_value(slider, v, LV_ANIM_OFF);
    lv_label_set_text_fmt(s_qs_valLabel[i], "%d %s", v, s_qs_rows[i].unit);
}

void openQuickSettings() {
    if (s_qs_backdrop) return;

    // Fullscreen dimmed backdrop.
    lv_obj_t* backdrop = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(backdrop);
    lv_obj_set_size(backdrop, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    s_qs_backdrop = backdrop;

    // Card sized to its content, centered, comfortably inside the 480px screen.
    lv_obj_t* card = lv_obj_create(backdrop);
    lv_obj_set_width(card, 440);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_center(card);
    applyCardStyle(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_style_pad_row(card, 6, 0);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "Quick Settings");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);

    s_qs_rows[0] = (QSRow){ QS_VOLUME, VOLUME_MIN,     VOLUME_MAX,     5,  "%" };
    s_qs_rows[1] = (QSRow){ QS_BRIGHT, BRIGHTNESS_MIN, BRIGHTNESS_MAX, 5,  "%" };
    s_qs_rows[2] = (QSRow){ QS_SPEED,  WPM_MIN,        WPM_MAX,        1,  "wpm" };
    s_qs_rows[3] = (QSRow){ QS_TONE,   400,            1200,           50, "Hz" };

    qsBuildRow(card, 0, "Volume");
    qsBuildRow(card, 1, "Brightness");
    qsBuildRow(card, 2, "CW Speed");
    qsBuildRow(card, 3, "Sidetone");

    lv_obj_t* hint = lv_label_create(card);
    lv_label_set_text(hint, "Up/Dn select   L/R adjust   ESC close");
    lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(hint, getThemeFonts()->font_small, 0);

    s_qs_index = 0;
    qsUpdateSelection();
}

#endif // LV_QUICK_SETTINGS_H
