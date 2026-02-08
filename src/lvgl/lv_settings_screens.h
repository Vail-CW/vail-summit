/*
 * VAIL SUMMIT - LVGL Settings Screens
 * Replaces LovyanGFX settings rendering with LVGL
 */

#ifndef LV_SETTINGS_SCREENS_H
#define LV_SETTINGS_SCREENS_H

#include <lvgl.h>
#include "lv_theme_summit.h"
#include "lv_widgets_summit.h"
#include "lv_screen_manager.h"
#include "../core/config.h"
#include "../core/modes.h"

// Note: Settings variables are accessed through the settings modules
// These extern declarations must match the actual types from the settings headers

// Volume settings (from i2s_audio.h)
extern int getVolume();
extern void setVolume(int vol);
extern bool getQuietBootEnabled();
extern void setQuietBootEnabled(bool enabled);
extern int getHeadphonesPreset();
extern void setHeadphonesPreset(int vol);
extern int getSpeakerPreset();
extern void setSpeakerPreset(int vol);
extern int getBootPreset();
extern void setBootPreset(int preset);
extern void applyHeadphonesPreset();
extern void applySpeakerPreset();

// Brightness settings (from settings_brightness.h)
extern int brightnessValue;
extern void applyBrightness(int val);
extern void saveBrightnessSettings();

// CW settings (from settings_cw.h)
extern int cwSpeed;
extern int cwTone;
extern void saveCWSettings();

// cwKeyType access functions (defined in main sketch to handle KeyType enum)
int getCwKeyTypeAsInt();
void setCwKeyTypeFromInt(int keyType);

// Callsign (from vail_repeater.h)
extern String vailCallsign;
extern void saveCallsign(const char* callsign);

// Web password settings (from settings_web_password.h)
extern char webPassword[];
extern bool webAuthEnabled;
extern void saveWebPassword(const char* password);
extern void clearWebPassword();

// ============================================
// Volume Settings Screen
// ============================================

static lv_obj_t* volume_screen = NULL;
static lv_obj_t* volume_slider = NULL;
static lv_obj_t* volume_value_label = NULL;

// Preset display labels (read-only)
static lv_obj_t* headphones_value_label = NULL;
static lv_obj_t* speaker_value_label = NULL;
static lv_obj_t* boot_preset_label = NULL;
static int boot_preset_index = 0;  // Local UI state for boot preset

// Boot preset names for selector display
static const char* boot_preset_names[] = {"Normal", "Low (10%)", "Headphones", "Speaker"};
static const int boot_preset_count = 4;

// Forward declaration for key acceleration
extern int getKeyAccelerationStep();

// Update preset value labels
static void update_preset_displays() {
    if (headphones_value_label != NULL) {
        lv_label_set_text_fmt(headphones_value_label, "h: Headphones %d%%", getHeadphonesPreset());
    }
    if (speaker_value_label != NULL) {
        lv_label_set_text_fmt(speaker_value_label, "s: Speaker %d%%", getSpeakerPreset());
    }
}

// Update boot preset selector display
static void update_boot_preset_display() {
    if (boot_preset_label != NULL) {
        lv_label_set_text_fmt(boot_preset_label, "< %s >", boot_preset_names[boot_preset_index]);
    }
}

static void volume_slider_event_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);

    // Update label
    if (volume_value_label != NULL) {
        lv_label_set_text_fmt(volume_value_label, "%d%%", value);
    }

    // Apply volume immediately for feedback
    setVolume(value);

    // Play test tone so user can hear the new volume level
    beep(TONE_MENU_NAV, BEEP_SHORT);
}

// Key handler for volume slider - applies acceleration for faster adjustment
// Number keys 1-9 = 10%-90%, 0 = 100%
// Preset shortcuts: h/s = apply preset, H/S (Shift) = save preset
static void volume_slider_key_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);
    lv_obj_t* slider = lv_event_get_target(e);

    // Number keys for quick percentage jumps
    if (key >= '0' && key <= '9') {
        int new_val;
        if (key == '0') {
            new_val = 100;  // 0 = 100%
        } else {
            new_val = (key - '0') * 10;  // 1=10%, 2=20%, ... 9=90%
        }
        lv_slider_set_value(slider, new_val, LV_ANIM_OFF);
        lv_event_send(slider, LV_EVENT_VALUE_CHANGED, NULL);
        lv_event_stop_bubbling(e);
        return;
    }

    // Preset shortcuts: h/s = apply, H/S = save
    if (key == 'h') {
        // Apply headphones preset
        applyHeadphonesPreset();
        lv_slider_set_value(slider, getVolume(), LV_ANIM_ON);
        if (volume_value_label != NULL) {
            lv_label_set_text_fmt(volume_value_label, "%d%%", getVolume());
        }
        beep(TONE_SELECT, BEEP_SHORT);
        lv_event_stop_processing(e);
        return;
    }

    if (key == 's') {
        // Apply speaker preset
        applySpeakerPreset();
        lv_slider_set_value(slider, getVolume(), LV_ANIM_ON);
        if (volume_value_label != NULL) {
            lv_label_set_text_fmt(volume_value_label, "%d%%", getVolume());
        }
        beep(TONE_SELECT, BEEP_SHORT);
        lv_event_stop_processing(e);
        return;
    }

    if (key == 'H') {
        // Save current volume as headphones preset
        setHeadphonesPreset(getVolume());
        update_preset_displays();
        beep(TONE_SELECT, BEEP_MEDIUM);
        lv_event_stop_processing(e);
        return;
    }

    if (key == 'S') {
        // Save current volume as speaker preset
        setSpeakerPreset(getVolume());
        update_preset_displays();
        beep(TONE_SELECT, BEEP_MEDIUM);
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        int step = getKeyAccelerationStep();
        int delta = (key == LV_KEY_RIGHT) ? step : -step;
        int current = lv_slider_get_value(slider);
        int new_val = current + delta;

        // Clamp to range
        int min_val = lv_slider_get_min_value(slider);
        int max_val = lv_slider_get_max_value(slider);
        if (new_val < min_val) new_val = min_val;
        if (new_val > max_val) new_val = max_val;

        lv_slider_set_value(slider, new_val, LV_ANIM_OFF);
        lv_event_send(slider, LV_EVENT_VALUE_CHANGED, NULL);

        // Prevent default slider handling
        lv_event_stop_bubbling(e);
    }

    // DOWN navigates to next widget
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        lv_group_focus_next(getLVGLInputGroup());
        lv_event_stop_processing(e);
        return;
    }

    // UP from slider - nothing above, block navigation
    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        lv_event_stop_processing(e);
        return;
    }
}

// Key handler for boot preset selector - LEFT/RIGHT to cycle, UP/DOWN to navigate
static void boot_preset_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    // LEFT/RIGHT cycles through boot options
    if (key == LV_KEY_LEFT) {
        boot_preset_index = (boot_preset_index - 1 + boot_preset_count) % boot_preset_count;
        setBootPreset(boot_preset_index);
        update_boot_preset_display();
        beep(TONE_MENU_NAV, BEEP_SHORT);
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_RIGHT) {
        boot_preset_index = (boot_preset_index + 1) % boot_preset_count;
        setBootPreset(boot_preset_index);
        update_boot_preset_display();
        beep(TONE_MENU_NAV, BEEP_SHORT);
        lv_event_stop_processing(e);
        return;
    }

    // UP navigates to previous widget
    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        lv_group_focus_prev(getLVGLInputGroup());
        lv_event_stop_processing(e);
        return;
    }

    // DOWN from boot preset - nothing below, block navigation
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }

    // Block ENTER - no action needed, L/R already handles it
    if (key == LV_KEY_ENTER) {
        lv_event_stop_processing(e);
        return;
    }
}

lv_obj_t* createVolumeSettingsScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "VOLUME");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // Content area - taller to fit presets
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_WIDTH - 40, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 20);
    lv_obj_set_pos(content, 20, HEADER_HEIGHT + 10);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 8, 0);
    lv_obj_set_style_pad_all(content, 12, 0);
    applyCardStyle(content);

    // Volume value (large display)
    volume_value_label = lv_label_create(content);
    lv_label_set_text_fmt(volume_value_label, "%d%%", getVolume());
    lv_obj_set_style_text_font(volume_value_label, getThemeFonts()->font_large, 0);
    lv_obj_set_style_text_color(volume_value_label, LV_COLOR_ACCENT_CYAN, 0);

    // Volume slider
    volume_slider = lv_slider_create(content);
    lv_obj_set_width(volume_slider, lv_pct(100));
    lv_slider_set_range(volume_slider, VOLUME_MIN, VOLUME_MAX);
    lv_slider_set_value(volume_slider, getVolume(), LV_ANIM_OFF);
    applySliderStyle(volume_slider);
    lv_obj_add_event_cb(volume_slider, volume_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(volume_slider, volume_slider_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(volume_slider);

    // Preset display row (read-only, not focusable)
    lv_obj_t* presets_row = lv_obj_create(content);
    lv_obj_set_size(presets_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(presets_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(presets_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(presets_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(presets_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(presets_row, 0, 0);
    lv_obj_set_style_pad_all(presets_row, 4, 0);

    // Headphones preset display
    headphones_value_label = lv_label_create(presets_row);
    lv_label_set_text_fmt(headphones_value_label, "h: Headphones %d%%", getHeadphonesPreset());
    lv_obj_set_style_text_font(headphones_value_label, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(headphones_value_label, LV_COLOR_TEXT_SECONDARY, 0);

    // Speaker preset display
    speaker_value_label = lv_label_create(presets_row);
    lv_label_set_text_fmt(speaker_value_label, "s: Speaker %d%%", getSpeakerPreset());
    lv_obj_set_style_text_font(speaker_value_label, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(speaker_value_label, LV_COLOR_TEXT_SECONDARY, 0);

    // Boot Volume selector row
    lv_obj_t* boot_row = lv_obj_create(content);
    lv_obj_set_size(boot_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(boot_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(boot_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(boot_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(boot_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(boot_row, 0, 0);
    lv_obj_set_style_pad_all(boot_row, 0, 0);
    lv_obj_set_style_pad_top(boot_row, 4, 0);

    lv_obj_t* boot_text = lv_label_create(boot_row);
    lv_label_set_text(boot_text, "Boot Volume");
    lv_obj_add_style(boot_text, getStyleLabelBody(), 0);

    // Boot preset selector button (arrow-style like CW Settings)
    lv_obj_t* boot_btn = lv_btn_create(boot_row);
    lv_obj_set_size(boot_btn, 110, 28);
    lv_obj_set_style_bg_color(boot_btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_color(boot_btn, lv_color_hex(0x555555), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(boot_btn, 4, 0);
    lv_obj_set_style_border_width(boot_btn, 1, 0);
    lv_obj_set_style_border_color(boot_btn, lv_color_hex(0x666666), 0);
    lv_obj_set_style_border_color(boot_btn, LV_COLOR_ACCENT_CYAN, LV_STATE_FOCUSED);
    lv_obj_set_style_pad_all(boot_btn, 4, 0);

    // Initialize boot preset index from saved preference
    boot_preset_index = getBootPreset();

    // Label inside button showing "< Normal >"
    boot_preset_label = lv_label_create(boot_btn);
    lv_obj_center(boot_preset_label);
    lv_obj_set_style_text_font(boot_preset_label, getThemeFonts()->font_small, 0);
    update_boot_preset_display();

    lv_obj_add_event_cb(boot_btn, boot_preset_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(boot_btn);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, "L/R: Volume   h/s: Recall Preset   Shift+H/S: Save Preset");
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    volume_screen = screen;
    return screen;
}

// ============================================
// Brightness Settings Screen
// ============================================

static lv_obj_t* brightness_screen = NULL;
static lv_obj_t* brightness_slider = NULL;
static lv_obj_t* brightness_value_label = NULL;

static void brightness_slider_event_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);

    // Update label
    if (brightness_value_label != NULL) {
        lv_label_set_text_fmt(brightness_value_label, "%d%%", value);
    }

    // Apply brightness immediately
    applyBrightness(value);
    saveBrightnessSettings();
}

// Key handler for brightness slider - applies acceleration for faster adjustment
// Number keys 1-9 = 10%-90%, 0 = 100%
static void brightness_slider_key_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);
    lv_obj_t* slider = lv_event_get_target(e);

    // Number keys for quick percentage jumps
    if (key >= '0' && key <= '9') {
        int new_val;
        if (key == '0') {
            new_val = 100;  // 0 = 100%
        } else {
            new_val = (key - '0') * 10;  // 1=10%, 2=20%, ... 9=90%
        }
        lv_slider_set_value(slider, new_val, LV_ANIM_OFF);
        lv_event_send(slider, LV_EVENT_VALUE_CHANGED, NULL);
        lv_event_stop_bubbling(e);
        return;
    }

    if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        int step = getKeyAccelerationStep();
        int delta = (key == LV_KEY_RIGHT) ? step : -step;
        int current = lv_slider_get_value(slider);
        int new_val = current + delta;

        // Clamp to range
        int min_val = lv_slider_get_min_value(slider);
        int max_val = lv_slider_get_max_value(slider);
        if (new_val < min_val) new_val = min_val;
        if (new_val > max_val) new_val = max_val;

        lv_slider_set_value(slider, new_val, LV_ANIM_OFF);
        lv_event_send(slider, LV_EVENT_VALUE_CHANGED, NULL);

        // Prevent default slider handling
        lv_event_stop_bubbling(e);
    }
}

lv_obj_t* createBrightnessSettingsScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "BRIGHTNESS");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // Content area
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_WIDTH - 60, 160);
    lv_obj_center(content);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 20, 0);
    applyCardStyle(content);

    // Brightness value - use theme font
    brightness_value_label = lv_label_create(content);
    lv_label_set_text_fmt(brightness_value_label, "%d%%", brightnessValue);
    lv_obj_set_style_text_font(brightness_value_label, getThemeFonts()->font_large, 0);
    lv_obj_set_style_text_color(brightness_value_label, LV_COLOR_ACCENT_CYAN, 0);

    // Brightness slider
    brightness_slider = lv_slider_create(content);
    lv_obj_set_width(brightness_slider, SCREEN_WIDTH - 120);
    lv_slider_set_range(brightness_slider, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
    lv_slider_set_value(brightness_slider, brightnessValue, LV_ANIM_OFF);
    applySliderStyle(brightness_slider);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // Add key handler for acceleration support
    lv_obj_add_event_cb(brightness_slider, brightness_slider_key_cb, LV_EVENT_KEY, NULL);

    addNavigableWidget(brightness_slider);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, FOOTER_ADJUST_ESC);  // Use standardized footer
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    brightness_screen = screen;
    return screen;
}

// ============================================
// CW Settings Screen
// ============================================

static lv_obj_t* cw_settings_screen = NULL;
static lv_obj_t* cw_speed_slider = NULL;
static lv_obj_t* cw_tone_slider = NULL;
static lv_obj_t* cw_keytype_value = NULL;  // Changed from dropdown to label
static lv_obj_t* cw_speed_value = NULL;
static lv_obj_t* cw_tone_value = NULL;

// CW Settings focus state and row references
static int cw_settings_focus = 0;  // 0=Speed, 1=Tone, 2=Key Type
static lv_obj_t* cw_focus_container = NULL;
static lv_obj_t* cw_speed_row = NULL;
static lv_obj_t* cw_tone_row = NULL;
static lv_obj_t* cw_keytype_row = NULL;

// Key type names for selector display
static const char* cw_keytype_names[] = {"Straight", "Iambic A", "Iambic B", "Ultimatic"};
static const int cw_keytype_count = 4;

// Musical note frequencies in the CW tone range (400-1200 Hz)
// A4 = 440 Hz standard tuning, includes all semitones (chromatic scale)
static const int cw_note_frequencies[] = {
    400,   // G4 (392 Hz rounded up)
    415,   // G#4/Ab4
    440,   // A4
    466,   // A#4/Bb4
    494,   // B4
    523,   // C5
    554,   // C#5/Db5
    587,   // D5
    622,   // D#5/Eb5
    659,   // E5
    698,   // F5
    740,   // F#5/Gb5
    784,   // G5
    831,   // G#5/Ab5
    880,   // A5
    932,   // A#5/Bb5
    988,   // B5
    1047,  // C6
    1109,  // C#6/Db6
    1175   // D6
};
static const int cw_note_count = sizeof(cw_note_frequencies) / sizeof(cw_note_frequencies[0]);

// Snap a frequency to the nearest musical note
static int snapToNearestNote(int freq) {
    int closest = cw_note_frequencies[0];
    int minDiff = abs(freq - closest);

    for (int i = 1; i < cw_note_count; i++) {
        int diff = abs(freq - cw_note_frequencies[i]);
        if (diff < minDiff) {
            minDiff = diff;
            closest = cw_note_frequencies[i];
        }
    }
    return closest;
}

// Update visual focus indicator for CW settings rows
static void cw_update_focus() {
    // Speed row styling (focus == 0)
    if (cw_speed_row) {
        if (cw_settings_focus == 0) {
            lv_obj_set_style_bg_color(cw_speed_row, LV_COLOR_CARD_TEAL, 0);
            lv_obj_set_style_bg_opa(cw_speed_row, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(cw_speed_row, LV_COLOR_ACCENT_CYAN, 0);
            lv_obj_set_style_border_width(cw_speed_row, 2, 0);
        } else {
            lv_obj_set_style_bg_opa(cw_speed_row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(cw_speed_row, 0, 0);
        }
    }
    if (cw_speed_slider) {
        if (cw_settings_focus == 0) {
            lv_obj_add_state(cw_speed_slider, LV_STATE_FOCUSED);
        } else {
            lv_obj_clear_state(cw_speed_slider, LV_STATE_FOCUSED);
        }
    }

    // Tone row styling (focus == 1)
    if (cw_tone_row) {
        if (cw_settings_focus == 1) {
            lv_obj_set_style_bg_color(cw_tone_row, LV_COLOR_CARD_TEAL, 0);
            lv_obj_set_style_bg_opa(cw_tone_row, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(cw_tone_row, LV_COLOR_ACCENT_CYAN, 0);
            lv_obj_set_style_border_width(cw_tone_row, 2, 0);
        } else {
            lv_obj_set_style_bg_opa(cw_tone_row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(cw_tone_row, 0, 0);
        }
    }
    if (cw_tone_slider) {
        if (cw_settings_focus == 1) {
            lv_obj_add_state(cw_tone_slider, LV_STATE_FOCUSED);
        } else {
            lv_obj_clear_state(cw_tone_slider, LV_STATE_FOCUSED);
        }
    }

    // Key type row styling (focus == 2)
    if (cw_keytype_row) {
        if (cw_settings_focus == 2) {
            lv_obj_set_style_bg_color(cw_keytype_row, LV_COLOR_CARD_TEAL, 0);
            lv_obj_set_style_bg_opa(cw_keytype_row, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(cw_keytype_row, LV_COLOR_ACCENT_CYAN, 0);
            lv_obj_set_style_border_width(cw_keytype_row, 2, 0);
        } else {
            lv_obj_set_style_bg_opa(cw_keytype_row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(cw_keytype_row, 0, 0);
        }
    }
    if (cw_keytype_value) {
        lv_obj_set_style_text_color(cw_keytype_value,
            cw_settings_focus == 2 ? LV_COLOR_ACCENT_CYAN : LV_COLOR_TEXT_SECONDARY, 0);
    }
}

// Forward declaration for back navigation
extern void onLVGLBackNavigation();

// Unified key handler for CW settings - handles all navigation
static void cw_settings_key_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    // Block Tab key - we don't want LVGL's default group navigation
    if (key == LV_KEY_NEXT || key == LV_KEY_PREV) {
        lv_event_stop_bubbling(e);
        return;
    }

    // Handle ESC for back navigation
    if (key == LV_KEY_ESC) {
        lv_event_stop_bubbling(e);  // Prevent double-navigation
        onLVGLBackNavigation();
        return;
    }

    // Handle UP/DOWN for navigation between settings
    if (key == LV_KEY_UP) {
        lv_event_stop_bubbling(e);
        if (cw_settings_focus > 0) {
            cw_settings_focus--;
            cw_update_focus();
        }
        return;
    }
    if (key == LV_KEY_DOWN) {
        lv_event_stop_bubbling(e);
        if (cw_settings_focus < 2) {
            cw_settings_focus++;
            cw_update_focus();
        }
        return;
    }

    // Handle LEFT/RIGHT for value adjustment based on current focus
    if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        lv_event_stop_bubbling(e);
        if (cw_settings_focus == 0 && cw_speed_slider) {
            // Speed slider - adjust WPM with acceleration
            int step = getKeyAccelerationStep();
            int delta = (key == LV_KEY_RIGHT) ? step : -step;
            int current = lv_slider_get_value(cw_speed_slider);
            int new_val = current + delta;

            // Clamp to range
            if (new_val < WPM_MIN) new_val = WPM_MIN;
            if (new_val > WPM_MAX) new_val = WPM_MAX;

            lv_slider_set_value(cw_speed_slider, new_val, LV_ANIM_OFF);
            lv_event_send(cw_speed_slider, LV_EVENT_VALUE_CHANGED, NULL);
        }
        else if (cw_settings_focus == 1 && cw_tone_slider) {
            // Tone slider - move to next/previous musical note
            int current = lv_slider_get_value(cw_tone_slider);
            int new_val = current;

            // Find current note index
            int current_idx = 0;
            int minDiff = abs(current - cw_note_frequencies[0]);
            for (int i = 1; i < cw_note_count; i++) {
                int diff = abs(current - cw_note_frequencies[i]);
                if (diff < minDiff) {
                    minDiff = diff;
                    current_idx = i;
                }
            }

            // Move to next/previous note
            if (key == LV_KEY_RIGHT && current_idx < cw_note_count - 1) {
                new_val = cw_note_frequencies[current_idx + 1];
            } else if (key == LV_KEY_LEFT && current_idx > 0) {
                new_val = cw_note_frequencies[current_idx - 1];
            }

            if (new_val != current) {
                lv_slider_set_value(cw_tone_slider, new_val, LV_ANIM_OFF);
                lv_event_send(cw_tone_slider, LV_EVENT_VALUE_CHANGED, NULL);
            }
        }
        else if (cw_settings_focus == 2 && cw_keytype_value) {
            // Key type - cycle through options using arrow selector
            int current = getCwKeyTypeAsInt();

            if (key == LV_KEY_RIGHT) {
                current = (current + 1) % cw_keytype_count;
            } else {
                current = (current - 1 + cw_keytype_count) % cw_keytype_count;
            }

            // Update display and save
            lv_label_set_text_fmt(cw_keytype_value, "< %s >", cw_keytype_names[current]);
            setCwKeyTypeFromInt(current);
            saveCWSettings();
        }
        return;
    }

    // Handle ENTER - no action needed, all settings use LEFT/RIGHT
    if (key == LV_KEY_ENTER) {
        lv_event_stop_bubbling(e);
        return;
    }
}

static void cw_speed_event_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    cwSpeed = lv_slider_get_value(slider);
    if (cw_speed_value != NULL) {
        lv_label_set_text_fmt(cw_speed_value, "%d WPM", cwSpeed);
    }
    saveCWSettings();
    // Play preview tone at current CW frequency to confirm change
    beep(cwTone, 100);
}


static void cw_tone_event_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    cwTone = lv_slider_get_value(slider);
    if (cw_tone_value != NULL) {
        lv_label_set_text_fmt(cw_tone_value, "%d Hz", cwTone);
    }
    saveCWSettings();
    // Play preview tone
    beep(cwTone, 100);
}


lv_obj_t* createCWSettingsScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "CW SETTINGS");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // Content container
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_WIDTH - 40, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 20);
    lv_obj_set_pos(content, 20, HEADER_HEIGHT + 10);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 8, 0);
    lv_obj_set_style_pad_all(content, 10, 0);
    applyCardStyle(content);
    lv_obj_add_flag(content, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // Prevent slider clipping

    // Create an invisible focus container to receive all key events
    // This bypasses LVGL's widget-level key handling
    cw_focus_container = lv_obj_create(content);
    lv_obj_set_size(cw_focus_container, 0, 0);
    lv_obj_set_style_bg_opa(cw_focus_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cw_focus_container, 0, 0);
    lv_obj_clear_flag(cw_focus_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cw_focus_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cw_focus_container, cw_settings_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(cw_focus_container);

    // Put group in edit mode - this makes UP/DOWN keys go to the widget instead of being consumed by LVGL
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }

    // Ensure focus is on our container
    lv_group_focus_obj(cw_focus_container);

    // Reset focus state
    cw_settings_focus = 0;

    // Speed setting row (with styling for focus highlight)
    cw_speed_row = lv_obj_create(content);
    lv_obj_set_size(cw_speed_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(cw_speed_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cw_speed_row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cw_speed_row, 5, 0);
    lv_obj_set_style_bg_opa(cw_speed_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cw_speed_row, 0, 0);
    lv_obj_set_style_pad_all(cw_speed_row, 8, 0);
    lv_obj_set_style_radius(cw_speed_row, 6, 0);
    lv_obj_clear_flag(cw_speed_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cw_speed_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // Prevent slider knob clipping

    lv_obj_t* speed_header = lv_obj_create(cw_speed_row);
    lv_obj_set_size(speed_header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(speed_header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(speed_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(speed_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(speed_header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(speed_header, 0, 0);
    lv_obj_set_style_pad_all(speed_header, 0, 0);

    lv_obj_t* speed_label = lv_label_create(speed_header);
    lv_label_set_text(speed_label, "Speed");
    lv_obj_add_style(speed_label, getStyleLabelSubtitle(), 0);

    cw_speed_value = lv_label_create(speed_header);
    lv_label_set_text_fmt(cw_speed_value, "%d WPM", cwSpeed);
    lv_obj_set_style_text_color(cw_speed_value, LV_COLOR_ACCENT_CYAN, 0);

    cw_speed_slider = lv_slider_create(cw_speed_row);
    lv_obj_set_width(cw_speed_slider, lv_pct(100));
    lv_slider_set_range(cw_speed_slider, WPM_MIN, WPM_MAX);
    lv_slider_set_value(cw_speed_slider, cwSpeed, LV_ANIM_OFF);
    applySliderStyle(cw_speed_slider);
    lv_obj_add_event_cb(cw_speed_slider, cw_speed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // No addNavigableWidget - focus container handles all navigation

    // Tone setting row (with styling for focus highlight)
    cw_tone_row = lv_obj_create(content);
    lv_obj_set_size(cw_tone_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(cw_tone_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cw_tone_row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cw_tone_row, 5, 0);
    lv_obj_set_style_bg_opa(cw_tone_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cw_tone_row, 0, 0);
    lv_obj_set_style_pad_all(cw_tone_row, 8, 0);
    lv_obj_set_style_radius(cw_tone_row, 6, 0);
    lv_obj_clear_flag(cw_tone_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cw_tone_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // Prevent slider knob clipping

    lv_obj_t* tone_header = lv_obj_create(cw_tone_row);
    lv_obj_set_size(tone_header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(tone_header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tone_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tone_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(tone_header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tone_header, 0, 0);
    lv_obj_set_style_pad_all(tone_header, 0, 0);

    lv_obj_t* tone_label = lv_label_create(tone_header);
    lv_label_set_text(tone_label, "Tone");
    lv_obj_add_style(tone_label, getStyleLabelSubtitle(), 0);

    cw_tone_value = lv_label_create(tone_header);
    lv_label_set_text_fmt(cw_tone_value, "%d Hz", cwTone);
    lv_obj_set_style_text_color(cw_tone_value, LV_COLOR_ACCENT_CYAN, 0);

    cw_tone_slider = lv_slider_create(cw_tone_row);
    lv_obj_set_width(cw_tone_slider, lv_pct(100));
    lv_slider_set_range(cw_tone_slider, 400, 1200);
    lv_slider_set_value(cw_tone_slider, cwTone, LV_ANIM_OFF);
    applySliderStyle(cw_tone_slider);
    lv_obj_add_event_cb(cw_tone_slider, cw_tone_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // No addNavigableWidget - focus container handles all navigation

    // Key type setting row (with styling for focus highlight)
    cw_keytype_row = lv_obj_create(content);
    lv_obj_set_size(cw_keytype_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(cw_keytype_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cw_keytype_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cw_keytype_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(cw_keytype_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cw_keytype_row, 0, 0);
    lv_obj_set_style_pad_all(cw_keytype_row, 8, 0);
    lv_obj_set_style_radius(cw_keytype_row, 6, 0);
    lv_obj_clear_flag(cw_keytype_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* keytype_label = lv_label_create(cw_keytype_row);
    lv_label_set_text(keytype_label, "Key Type");
    lv_obj_add_style(keytype_label, getStyleLabelSubtitle(), 0);

    // Key type value - shows "< Straight >" style selector (like Hear It Type It)
    cw_keytype_value = lv_label_create(cw_keytype_row);
    lv_label_set_text_fmt(cw_keytype_value, "< %s >", cw_keytype_names[getCwKeyTypeAsInt()]);
    lv_obj_set_style_text_color(cw_keytype_value, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_set_style_text_font(cw_keytype_value, getThemeFonts()->font_subtitle, 0);

    // Set initial focus styling
    cw_update_focus();

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, FOOTER_NAV_ADJUST_ESC);  // Use standardized footer
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    cw_settings_screen = screen;
    return screen;
}

// ============================================
// Callsign Settings Screen
// ============================================

static lv_obj_t* callsign_screen = NULL;
static lv_obj_t* callsign_textarea = NULL;

// Key handler for callsign textarea - handles ENTER to save
static void callsign_textarea_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ENTER) {
        // Save callsign
        if (callsign_textarea != NULL) {
            const char* text = lv_textarea_get_text(callsign_textarea);
            if (text != NULL && strlen(text) > 0) {
                // Convert to uppercase
                String callsign = String(text);
                callsign.toUpperCase();

                // Save using existing function
                saveCallsign(callsign.c_str());
                vailCallsign = callsign;  // Update global

                // Note: onLVGLBackNavigation() already plays a nav beep
                Serial.printf("[Callsign] Saved: %s\n", callsign.c_str());
            }
        }
        // Navigate back
        onLVGLBackNavigation();
        lv_event_stop_bubbling(e);
    }
    // ESC is handled by the global back navigation system
}

lv_obj_t* createCallsignSettingsScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "CALLSIGN");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // Content
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_WIDTH - 60, 140);
    lv_obj_center(content);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 15, 0);
    applyCardStyle(content);

    // Label
    lv_obj_t* label = lv_label_create(content);
    lv_label_set_text(label, "Enter your callsign:");
    lv_obj_add_style(label, getStyleLabelSubtitle(), 0);

    // Text area
    callsign_textarea = lv_textarea_create(content);
    lv_obj_set_size(callsign_textarea, 250, 50);
    lv_textarea_set_one_line(callsign_textarea, true);
    lv_textarea_set_max_length(callsign_textarea, 12);
    lv_textarea_set_placeholder_text(callsign_textarea, "e.g. W1ABC");
    lv_textarea_set_text(callsign_textarea, vailCallsign.c_str());
    lv_obj_add_style(callsign_textarea, getStyleTextarea(), 0);
    lv_obj_set_style_text_font(callsign_textarea, getThemeFonts()->font_subtitle, 0);
    // Add key handler to process ENTER for save
    lv_obj_add_event_cb(callsign_textarea, callsign_textarea_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(callsign_textarea);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, FOOTER_TYPE_ENTER_ESC);  // Use standardized footer
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    // Auto-focus the callsign textarea for immediate input
    focusWidget(callsign_textarea);

    callsign_screen = screen;
    return screen;
}

// Get callsign from text area (call before leaving screen)
const char* getCallsignFromTextarea() {
    if (callsign_textarea != NULL) {
        return lv_textarea_get_text(callsign_textarea);
    }
    return "";
}

// ============================================
// Web Password Settings Screen
// ============================================

static lv_obj_t* web_password_screen = NULL;
static lv_obj_t* web_password_textarea = NULL;
static lv_obj_t* web_password_toggle_btn = NULL;
static lv_obj_t* web_password_toggle_label = NULL;
static lv_obj_t* web_password_field_container = NULL;
static lv_obj_t* web_password_error_label = NULL;
static bool web_password_enabled_state = false;

// Forward declarations
static void update_web_password_display();

// Update the toggle button display and show/hide password field
static void update_web_password_display() {
    if (web_password_toggle_label != NULL) {
        lv_label_set_text(web_password_toggle_label, web_password_enabled_state ? "ENABLED" : "DISABLED");
        lv_obj_set_style_text_color(web_password_toggle_label,
            web_password_enabled_state ? LV_COLOR_ACCENT_GREEN : LV_COLOR_WARNING, 0);
    }
    // Show/hide password field based on state
    if (web_password_field_container != NULL) {
        if (web_password_enabled_state) {
            lv_obj_clear_flag(web_password_field_container, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(web_password_field_container, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Key handler for web password toggle button - ENTER to toggle, ESC to save and exit
static void web_password_toggle_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    // ESC - save current state and exit
    if (key == LV_KEY_ESC) {
        lv_event_stop_processing(e);

        if (web_password_enabled_state) {
            // Enabled - check if we have a valid password
            if (web_password_textarea != NULL) {
                const char* text = lv_textarea_get_text(web_password_textarea);
                // Trim non-printable characters from password
                String password = "";
                for (int i = 0; text[i] != '\0'; i++) {
                    if (text[i] >= 32 && text[i] <= 126) {
                        password += text[i];
                    }
                }

                if (password.length() >= 8 && password.length() <= 16) {
                    // Valid password - save it
                    strncpy(webPassword, password.c_str(), 16);
                    webPassword[16] = '\0';
                    webAuthEnabled = true;
                    saveWebPassword(webPassword);
                    beep(TONE_SELECT, BEEP_MEDIUM);
                    Serial.println("[WebPW] Password saved on exit");
                } else if (strlen(webPassword) >= 8) {
                    // Keep existing password
                    webAuthEnabled = true;
                    beep(TONE_SELECT, BEEP_SHORT);
                    Serial.println("[WebPW] Keeping existing password");
                } else {
                    // No valid password - disable protection
                    webPassword[0] = '\0';
                    webAuthEnabled = false;
                    clearWebPassword();
                    beep(TONE_ERROR, BEEP_SHORT);
                    Serial.println("[WebPW] No valid password, disabling");
                }
            }
        } else {
            // Disabled - clear password
            webPassword[0] = '\0';
            webAuthEnabled = false;
            clearWebPassword();
            beep(TONE_SELECT, BEEP_SHORT);
            Serial.println("[WebPW] Password protection disabled");
        }

        onLVGLBackNavigation();
        return;
    }

    // ENTER toggles the setting
    if (key == LV_KEY_ENTER) {
        web_password_enabled_state = !web_password_enabled_state;
        update_web_password_display();
        beep(TONE_SELECT, BEEP_SHORT);

        // Focus moves to textarea when enabled
        if (web_password_enabled_state && web_password_textarea != NULL) {
            lv_group_focus_obj(web_password_textarea);
        }

        lv_event_stop_processing(e);
        return;
    }

    // UP - can't go up (first widget)
    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        lv_event_stop_processing(e);
        return;
    }

    // DOWN - move to textarea if enabled
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        if (web_password_enabled_state && web_password_textarea != NULL) {
            lv_group_focus_obj(web_password_textarea);
        }
        lv_event_stop_processing(e);
        return;
    }

    // Block other keys
    lv_event_stop_processing(e);
}

// Key handler for web password textarea - ESC to save and exit
static void web_password_field_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    // ESC - validate, save, and exit
    if (key == LV_KEY_ESC) {
        lv_event_stop_processing(e);

        if (web_password_textarea != NULL) {
            const char* text = lv_textarea_get_text(web_password_textarea);
            // Trim non-printable characters from password
            String password = "";
            for (int i = 0; text[i] != '\0'; i++) {
                if (text[i] >= 32 && text[i] <= 126) {
                    password += text[i];
                }
            }

            if (password.length() >= 8 && password.length() <= 16) {
                // Valid password - save it
                strncpy(webPassword, password.c_str(), 16);
                webPassword[16] = '\0';
                webAuthEnabled = true;
                saveWebPassword(webPassword);
                beep(TONE_SELECT, BEEP_MEDIUM);
                Serial.println("[WebPW] Password saved on exit");
            } else if (strlen(webPassword) >= 8) {
                // Keep existing password
                webAuthEnabled = true;
                beep(TONE_SELECT, BEEP_SHORT);
                Serial.println("[WebPW] Keeping existing password");
            } else {
                // No valid password - disable protection
                webPassword[0] = '\0';
                webAuthEnabled = false;
                clearWebPassword();
                beep(TONE_ERROR, BEEP_SHORT);
                Serial.println("[WebPW] No valid password, disabling");
            }
        }

        onLVGLBackNavigation();
        return;
    }

    // UP - move back to toggle button
    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        if (web_password_error_label != NULL) {
            lv_obj_add_flag(web_password_error_label, LV_OBJ_FLAG_HIDDEN);
        }
        if (web_password_toggle_btn != NULL) {
            lv_group_focus_obj(web_password_toggle_btn);
        }
        lv_event_stop_processing(e);
        return;
    }

    // DOWN - can't go down (last widget)
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }

    // Block ENTER from doing anything special (just types in textarea)
    // User must press ESC to save and exit
}

lv_obj_t* createWebPasswordSettingsScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "WEB PASSWORD");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // Content card
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_WIDTH - 60, 180);
    lv_obj_center(content);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 15, 0);
    lv_obj_set_style_pad_all(content, 20, 0);
    applyCardStyle(content);

    // Toggle button row
    lv_obj_t* toggle_row = lv_obj_create(content);
    lv_obj_set_size(toggle_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(toggle_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(toggle_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(toggle_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(toggle_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(toggle_row, 0, 0);
    lv_obj_set_style_pad_all(toggle_row, 0, 0);

    lv_obj_t* toggle_text = lv_label_create(toggle_row);
    lv_label_set_text(toggle_text, "Password Protection");
    lv_obj_add_style(toggle_text, getStyleLabelSubtitle(), 0);

    // Create toggle button (like quiet boot)
    web_password_toggle_btn = lv_btn_create(toggle_row);
    lv_obj_set_size(web_password_toggle_btn, 80, 28);
    lv_obj_set_style_bg_color(web_password_toggle_btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_color(web_password_toggle_btn, lv_color_hex(0x555555), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(web_password_toggle_btn, 4, 0);
    lv_obj_set_style_border_width(web_password_toggle_btn, 1, 0);
    lv_obj_set_style_border_color(web_password_toggle_btn, lv_color_hex(0x666666), 0);
    lv_obj_set_style_border_color(web_password_toggle_btn, LV_COLOR_ACCENT_CYAN, LV_STATE_FOCUSED);
    lv_obj_set_style_pad_all(web_password_toggle_btn, 4, 0);

    // Initialize state from saved preference
    web_password_enabled_state = webAuthEnabled;

    // Label inside button showing ENABLED/DISABLED
    web_password_toggle_label = lv_label_create(web_password_toggle_btn);
    lv_obj_center(web_password_toggle_label);
    lv_obj_set_style_text_font(web_password_toggle_label, getThemeFonts()->font_small, 0);

    // Add key handler for toggle button
    lv_obj_add_event_cb(web_password_toggle_btn, web_password_toggle_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(web_password_toggle_btn);

    // Password field container (hidden when disabled)
    web_password_field_container = lv_obj_create(content);
    lv_obj_set_size(web_password_field_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(web_password_field_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(web_password_field_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(web_password_field_container, 8, 0);
    lv_obj_set_style_bg_opa(web_password_field_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(web_password_field_container, 0, 0);
    lv_obj_set_style_pad_all(web_password_field_container, 0, 0);

    // Password label
    lv_obj_t* pw_label = lv_label_create(web_password_field_container);
    lv_label_set_text(pw_label, "Password (8-16 characters):");
    lv_obj_add_style(pw_label, getStyleLabelBody(), 0);

    // Password text area
    web_password_textarea = lv_textarea_create(web_password_field_container);
    lv_obj_set_size(web_password_textarea, lv_pct(100), 45);
    lv_textarea_set_one_line(web_password_textarea, true);
    lv_textarea_set_max_length(web_password_textarea, 16);
    lv_textarea_set_placeholder_text(web_password_textarea, "Enter password");
    lv_textarea_set_password_mode(web_password_textarea, true);
    if (strlen(webPassword) > 0) {
        lv_textarea_set_text(web_password_textarea, webPassword);
    }
    lv_obj_add_style(web_password_textarea, getStyleTextarea(), 0);

    // Add key handler for textarea
    lv_obj_add_event_cb(web_password_textarea, web_password_field_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(web_password_textarea);

    // Error message label (hidden by default)
    web_password_error_label = lv_label_create(web_password_field_container);
    lv_label_set_text(web_password_error_label, "");
    lv_obj_set_style_text_color(web_password_error_label, LV_COLOR_ERROR, 0);
    lv_obj_set_style_text_font(web_password_error_label, getThemeFonts()->font_small, 0);
    lv_obj_add_flag(web_password_error_label, LV_OBJ_FLAG_HIDDEN);

    // Update display (shows/hides password field based on state)
    update_web_password_display();

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, "ENTER Toggle   ESC Save & Exit");
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    web_password_screen = screen;

    // Focus the toggle button
    focusWidget(web_password_toggle_btn);

    return screen;
}

// ============================================
// WiFi Settings Screen
// ============================================

// Include the full WiFi setup screen implementation
#include "lv_wifi_screen.h"

// Forward declaration for back navigation
extern void onLVGLBackNavigation();

// Delegate to the new WiFi setup screen
lv_obj_t* createWiFiSettingsScreen() {
    return createWiFiSetupScreen();
}

// ============================================
// Theme Settings Screen
// ============================================

#include "../settings/settings_theme.h"

static lv_obj_t* theme_settings_screen = NULL;
static lv_obj_t* theme_dropdown = NULL;

static void theme_dropdown_event_cb(lv_event_t* e) {
    lv_obj_t* dd = lv_event_get_target(e);
    int selected = lv_dropdown_get_selected(dd);

    ThemeType newTheme = (selected == 0) ? THEME_SUMMIT : THEME_ENIGMA;

    // Save and apply theme
    saveThemeSetting(newTheme);
    setTheme(newTheme);
}

lv_obj_t* createThemeSettingsScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "UI THEME");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // Content card
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_WIDTH - 60, 200);
    lv_obj_center(content);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 20, 0);
    applyCardStyle(content);

    // Label
    lv_obj_t* label = lv_label_create(content);
    lv_label_set_text(label, "Select UI Theme:");
    lv_obj_add_style(label, getStyleLabelSubtitle(), 0);

    // Theme dropdown
    theme_dropdown = lv_dropdown_create(content);
    lv_dropdown_set_options(theme_dropdown, "Summit (Default)\nEnigma (Military)");
    lv_dropdown_set_selected(theme_dropdown, getCurrentTheme() == THEME_SUMMIT ? 0 : 1);
    lv_obj_set_width(theme_dropdown, 280);
    lv_obj_add_style(theme_dropdown, getStyleDropdown(), 0);
    lv_obj_add_event_cb(theme_dropdown, theme_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    addNavigableWidget(theme_dropdown);

    // Theme description
    lv_obj_t* desc = lv_label_create(content);
    if (getCurrentTheme() == THEME_SUMMIT) {
        lv_label_set_text(desc, "Modern dark theme with cyan accents");
    } else {
        lv_label_set_text(desc, "Military-inspired with brass accents\nand typewriter font");
    }
    lv_obj_add_style(desc, getStyleLabelBody(), 0);
    lv_obj_set_style_text_align(desc, LV_TEXT_ALIGN_CENTER, 0);

    // Hint
    lv_obj_t* hint = lv_label_create(content);
    lv_label_set_text(hint, "Theme applies immediately");
    lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_set_style_text_font(hint, getThemeFonts()->font_small, 0);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, "UP/DN Select   ENTER Apply   ESC Back");
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    theme_settings_screen = screen;
    return screen;
}

// ============================================
// System Info Screen
// ============================================

static lv_obj_t* system_info_screen = NULL;
static lv_obj_t* system_info_scroll_container = NULL;
static lv_obj_t* system_info_focus_container = NULL;

// Helper to format uptime as HH:MM:SS
static void formatUptime(unsigned long ms, char* buf, size_t bufSize) {
    unsigned long totalSecs = ms / 1000;
    unsigned long hours = totalSecs / 3600;
    unsigned long minutes = (totalSecs % 3600) / 60;
    unsigned long seconds = totalSecs % 60;
    snprintf(buf, bufSize, "%lu:%02lu:%02lu", hours, minutes, seconds);
}

// Helper to create an info row (label: value)
static void createInfoRow(lv_obj_t* parent, const char* label, const char* value) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_ver(row, 4, 0);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(lbl, getThemeFonts()->font_body, 0);

    lv_obj_t* val = lv_label_create(row);
    lv_label_set_text(val, value);
    lv_obj_set_style_text_color(val, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(val, getThemeFonts()->font_body, 0);
}

// Key callback for System Info screen scrolling
static void system_info_key_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ESC) {
        onLVGLBackNavigation();
        lv_event_stop_bubbling(e);
        return;
    }

    // Scroll with UP/DOWN
    if (system_info_scroll_container != NULL) {
        if (key == LV_KEY_UP || key == LV_KEY_PREV) {
            lv_obj_scroll_by(system_info_scroll_container, 0, 30, LV_ANIM_ON);
            lv_event_stop_bubbling(e);
            return;
        }
        if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
            lv_obj_scroll_by(system_info_scroll_container, 0, -30, LV_ANIM_ON);
            lv_event_stop_bubbling(e);
            return;
        }
    }

    lv_event_stop_bubbling(e);
}

lv_obj_t* createSystemInfoScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "SYSTEM INFO");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // Content card (scrollable)
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_WIDTH - 40, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 20);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 10);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 8, 0);
    lv_obj_set_style_pad_all(content, 20, 0);
    applyCardStyle(content);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    system_info_scroll_container = content;

    // Firmware version (prominent)
    lv_obj_t* version_label = lv_label_create(content);
    lv_label_set_text_fmt(version_label, "v%s", FIRMWARE_VERSION);
    lv_obj_set_style_text_color(version_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_set_style_text_font(version_label, getThemeFonts()->font_title, 0);
    lv_obj_set_width(version_label, lv_pct(100));
    lv_obj_set_style_text_align(version_label, LV_TEXT_ALIGN_CENTER, 0);

    // Build date
    lv_obj_t* date_label = lv_label_create(content);
    lv_label_set_text_fmt(date_label, "Built: %s", FIRMWARE_DATE);
    lv_obj_set_style_text_color(date_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(date_label, getThemeFonts()->font_small, 0);
    lv_obj_set_width(date_label, lv_pct(100));
    lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_CENTER, 0);

    // Web files version
    lv_obj_t* web_ver_label = lv_label_create(content);
    String webVer = getWebFilesVersion();
    webVer.trim();
    if (webVer.isEmpty()) {
        lv_label_set_text(web_ver_label, "Web: Not installed");
        lv_obj_set_style_text_color(web_ver_label, LV_COLOR_WARNING, 0);
    } else if (webVer == WEB_FILES_VERSION) {
        lv_label_set_text_fmt(web_ver_label, "Web: v%s", webVer.c_str());
        lv_obj_set_style_text_color(web_ver_label, LV_COLOR_TEXT_SECONDARY, 0);
    } else {
        lv_label_set_text_fmt(web_ver_label, "Web: v%s (expected v%s)", webVer.c_str(), WEB_FILES_VERSION);
        lv_obj_set_style_text_color(web_ver_label, LV_COLOR_WARNING, 0);
    }
    lv_obj_set_style_text_font(web_ver_label, getThemeFonts()->font_small, 0);
    lv_obj_set_width(web_ver_label, lv_pct(100));
    lv_obj_set_style_text_align(web_ver_label, LV_TEXT_ALIGN_CENTER, 0);

    // Spacer
    lv_obj_t* spacer = lv_obj_create(content);
    lv_obj_set_size(spacer, lv_pct(100), 10);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);

    // System info rows
    createInfoRow(content, "Device:", FIRMWARE_NAME);
    createInfoRow(content, "Chip:", "ESP32-S3");

    // Free heap
    char heapBuf[32];
    snprintf(heapBuf, sizeof(heapBuf), "%lu KB", ESP.getFreeHeap() / 1024);
    createInfoRow(content, "Free Heap:", heapBuf);

    // PSRAM
    if (psramFound()) {
        char psramBuf[32];
        snprintf(psramBuf, sizeof(psramBuf), "%lu KB", ESP.getFreePsram() / 1024);
        createInfoRow(content, "Free PSRAM:", psramBuf);
    } else {
        createInfoRow(content, "PSRAM:", "Not available");
    }

    // Uptime
    char uptimeBuf[32];
    formatUptime(millis(), uptimeBuf, sizeof(uptimeBuf));
    createInfoRow(content, "Uptime:", uptimeBuf);

    // Invisible focus container for key handling
    system_info_focus_container = lv_obj_create(screen);
    lv_obj_set_size(system_info_focus_container, 0, 0);
    lv_obj_set_style_bg_opa(system_info_focus_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(system_info_focus_container, 0, 0);
    lv_obj_add_flag(system_info_focus_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(system_info_focus_container, system_info_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(system_info_focus_container);

    // Enable edit mode to receive key events
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, "UP/DN Scroll   ESC Back");
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    system_info_screen = screen;
    return screen;
}

// ============================================
// Screen Selector
// ============================================

lv_obj_t* createSettingsScreenForMode(int mode) {
    switch (mode) {
        case MODE_VOLUME_SETTINGS:
            return createVolumeSettingsScreen();
        case MODE_BRIGHTNESS_SETTINGS:
            return createBrightnessSettingsScreen();
        case MODE_CW_SETTINGS:
            return createCWSettingsScreen();
        case MODE_CALLSIGN_SETTINGS:
            return createCallsignSettingsScreen();
        case MODE_WEB_PASSWORD_SETTINGS:
            return createWebPasswordSettingsScreen();
        case MODE_WIFI_SETTINGS:
            return createWiFiSettingsScreen();
        case MODE_THEME_SETTINGS:
            return createThemeSettingsScreen();
        case MODE_SYSTEM_INFO:
            return createSystemInfoScreen();
        default:
            Serial.printf("[SettingsScreens] Unknown settings mode: %d\n", mode);
            return NULL;
    }
}

#endif // LV_SETTINGS_SCREENS_H
