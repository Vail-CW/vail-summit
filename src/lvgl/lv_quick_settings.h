/*
 * Quick Settings Overlay
 *
 * A modal panel (on lv_layer_top) giving instant live control of the four
 * settings a user reaches for most: Volume, Brightness, CW Speed, and Sidetone.
 * Opened by the global hotkey (V) from any pure-navigation menu and dismissed
 * with ENTER or ESC. It never changes currentMode — it floats above whatever
 * screen is active and restores focus exactly where it was on close.
 *
 * Modality mirrors createAlertDialog(): a fullscreen backdrop swallows stray
 * events and the keypad indev is swapped onto a private group for the duration.
 */

#ifndef LV_QUICK_SETTINGS_H
#define LV_QUICK_SETTINGS_H

#include "lvgl.h"
#include "../core/config.h"

// ============================================
// State
// ============================================

enum QSKind { QS_VOLUME, QS_BRIGHT, QS_SPEED, QS_TONE };

struct QSRow {
    QSKind      kind;
    lv_obj_t*   valLabel;   // label showing the numeric value + unit
    int         step;       // increment per LEFT/RIGHT press
    const char* unit;       // "%", "wpm", "Hz"
};

static lv_obj_t*    s_qs_backdrop   = NULL;
static lv_group_t*  s_qs_group      = NULL;
static lv_group_t*  s_qs_prev_group = NULL;
static QSRow        s_qs_rows[4];

bool isQuickSettingsOpen() { return s_qs_backdrop != NULL; }

// ============================================
// Apply a value live to the underlying setting
// ============================================

static void qsApply(QSRow* row, int v) {
    switch (row->kind) {
        case QS_VOLUME:
            setVolume(v);                 // applies + persists
            beep(TONE_MENU_NAV, 50);      // audible volume feedback
            break;
        case QS_BRIGHT:
            setBrightness(v);             // applies + persists
            break;
        case QS_SPEED:
            cwSpeed = v;
            saveCWSettings();
            break;
        case QS_TONE:
            cwTone = v;
            saveCWSettings();
            beep(cwTone, 80);             // preview the new sidetone
            break;
    }
}

// ============================================
// Teardown
// ============================================

void closeQuickSettings() {
    if (s_qs_backdrop == NULL) return;

    lv_indev_t* indev = getLVGLKeypad();
    if (indev != NULL && s_qs_group != NULL) {
        lv_indev_set_group(indev, s_qs_prev_group);
        lv_group_del(s_qs_group);
        s_qs_group = NULL;
        s_qs_prev_group = NULL;
    }

    lv_obj_del(s_qs_backdrop);
    s_qs_backdrop = NULL;

    // Discard the in-flight key release so the ENTER that closed us does not
    // synthesize a CLICKED on the focused widget below.
    if (indev != NULL) lv_indev_wait_release(indev);
}

// ============================================
// Slider event handlers (shared by all four rows)
// ============================================

static void qsSliderChangedCb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    QSRow* row = (QSRow*)lv_obj_get_user_data(slider);
    if (row == NULL) return;
    int v = lv_slider_get_value(slider);
    if (row->valLabel) lv_label_set_text_fmt(row->valLabel, "%d %s", v, row->unit);
    qsApply(row, v);
}

static void qsSliderKeyCb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    QSRow* row = (QSRow*)lv_obj_get_user_data(slider);
    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        int delta = (key == LV_KEY_RIGHT) ? row->step : -row->step;
        int v = lv_slider_get_value(slider) + delta;
        int lo = lv_slider_get_min_value(slider);
        int hi = lv_slider_get_max_value(slider);
        if (v < lo) v = lo;
        if (v > hi) v = hi;
        lv_slider_set_value(slider, v, LV_ANIM_OFF);
        lv_event_send(slider, LV_EVENT_VALUE_CHANGED, NULL);
        lv_event_stop_processing(e);
        return;
    }
    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        if (s_qs_group) lv_group_focus_prev(s_qs_group);
        lv_event_stop_processing(e);
        return;
    }
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        if (s_qs_group) lv_group_focus_next(s_qs_group);
        lv_event_stop_processing(e);
        return;
    }
    if (key == LV_KEY_ENTER || key == LV_KEY_ESC) {
        closeQuickSettings();
        lv_event_stop_processing(e);
        return;
    }
}

// ============================================
// One slider row builder
// ============================================

static lv_obj_t* qsBuildRow(lv_obj_t* parent, QSRow* row, const char* label,
                            int min, int max, int current) {
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_width(col, lv_pct(100));
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 3, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* header = lv_obj_create(col);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(header);
    lv_label_set_text(lbl, label);
    lv_obj_add_style(lbl, getStyleLabelSubtitle(), 0);

    row->valLabel = lv_label_create(header);
    lv_label_set_text_fmt(row->valLabel, "%d %s", current, row->unit);
    lv_obj_set_style_text_color(row->valLabel, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(row->valLabel, getThemeFonts()->font_subtitle, 0);

    lv_obj_t* slider = lv_slider_create(col);
    lv_obj_set_width(slider, lv_pct(100));
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, current, LV_ANIM_OFF);
    applySliderStyle(slider);
    lv_obj_set_user_data(slider, row);
    lv_obj_add_event_cb(slider, qsSliderChangedCb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, qsSliderKeyCb, LV_EVENT_KEY, NULL);

    lv_group_add_obj(s_qs_group, slider);
    return slider;
}

// ============================================
// Open
// ============================================

void openQuickSettings() {
    if (s_qs_backdrop != NULL) return;  // already open

    // Fullscreen backdrop swallows events not aimed at the panel.
    lv_obj_t* backdrop = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(backdrop);
    lv_obj_set_size(backdrop, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(backdrop, 0, 0);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
    lv_obj_add_event_cb(backdrop, [](lv_event_t* e) {
        if (lv_event_get_target(e) != lv_event_get_current_target(e)) return;
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    }, LV_EVENT_ALL, NULL);
    s_qs_backdrop = backdrop;

    // Private group for the panel's sliders.
    s_qs_group = lv_group_create();
    lv_group_set_wrap(s_qs_group, false);

    // Card
    lv_obj_t* card = lv_obj_create(backdrop);
    lv_obj_set_size(card, 420, 286);
    lv_obj_center(card);
    applyCardStyle(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "Quick Settings");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);

    s_qs_rows[0] = (QSRow){ QS_VOLUME, NULL, 5,  "%" };
    s_qs_rows[1] = (QSRow){ QS_BRIGHT, NULL, 5,  "%" };
    s_qs_rows[2] = (QSRow){ QS_SPEED,  NULL, 1,  "wpm" };
    s_qs_rows[3] = (QSRow){ QS_TONE,   NULL, 50, "Hz" };

    lv_obj_t* firstSlider = qsBuildRow(card, &s_qs_rows[0], "Volume",
                                       VOLUME_MIN, VOLUME_MAX, getVolume());
    qsBuildRow(card, &s_qs_rows[1], "Brightness",
               BRIGHTNESS_MIN, BRIGHTNESS_MAX, getBrightness());
    qsBuildRow(card, &s_qs_rows[2], "CW Speed",
               WPM_MIN, WPM_MAX, cwSpeed);
    qsBuildRow(card, &s_qs_rows[3], "Sidetone",
               400, 1200, cwTone);

    lv_obj_t* hint = lv_label_create(card);
    lv_label_set_text(hint, LV_SYMBOL_LEFT "/" LV_SYMBOL_RIGHT " adjust   "
                            LV_SYMBOL_UP "/" LV_SYMBOL_DOWN " move   ENTER/ESC close");
    lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(hint, getThemeFonts()->font_small, 0);

    // Swap the keypad onto the private group; arrow keys can no longer move
    // focus on the screen below. Restored in closeQuickSettings().
    lv_indev_t* indev = getLVGLKeypad();
    if (indev != NULL) {
        s_qs_prev_group = getLVGLInputGroup();
        lv_indev_set_group(indev, s_qs_group);
    }
    if (firstSlider) lv_group_focus_obj(firstSlider);
}

#endif // LV_QUICK_SETTINGS_H
