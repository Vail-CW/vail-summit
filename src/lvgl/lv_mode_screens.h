/*
 * VAIL SUMMIT - LVGL Mode Screens
 * Covers Radio, Vail Repeater, QSO Logger, Bluetooth, and other modes
 */

#ifndef LV_MODE_SCREENS_H
#define LV_MODE_SCREENS_H

#include <lvgl.h>
#include "lv_theme_summit.h"
#include "lv_widgets_summit.h"
#include "lv_screen_manager.h"
#include "lv_web_mode_screens.h"
#include "../core/config.h"
#include "../core/modes.h"

// Forward declaration for back navigation
extern void onLVGLBackNavigation();

// ============================================
// Radio Output Screen
// ============================================

// External references for radio settings
extern int cwSpeed;
extern int cwTone;
extern KeyType cwKeyType;
extern void saveCWSettings();

// Forward declarations for radio functions (defined in radio_output.h)
extern bool queueRadioMessage(const char* message);

// Radio mode - use values from radio_output.h (already included before this file)
// RadioMode enum: RADIO_MODE_SUMMIT_KEYER, RADIO_MODE_RADIO_KEYER
extern RadioMode radioMode;
extern void saveRadioSettings();

// CW Memories - use values from radio_cw_memories.h (already included before this file)
// CWMemoryPreset struct and CW_MEMORY_MAX_SLOTS defined there
extern CWMemoryPreset cwMemories[];

// Radio screen state
static lv_obj_t* radio_screen = NULL;
static lv_obj_t* radio_mode_label = NULL;
static lv_obj_t* radio_status_label = NULL;
static lv_obj_t* radio_wpm_label = NULL;
static lv_obj_t* radio_tone_label = NULL;
static lv_obj_t* radio_keytype_label = NULL;

// Action bar buttons
static lv_obj_t* radio_btn_mode = NULL;
static lv_obj_t* radio_btn_settings = NULL;
static lv_obj_t* radio_btn_memories = NULL;
static int radio_action_focus = 0;  // 0=Mode, 1=Settings, 2=Memories

// Overlay state
static lv_obj_t* radio_overlay = NULL;
static bool radio_settings_active = false;
static bool radio_memories_active = false;
static int radio_settings_selection = 0;  // 0=WPM, 1=KeyType, 2=Tone
static int radio_memory_selection = 0;    // 0-9 for memory slots

// Forward declarations
void createRadioSettingsOverlay();
void createRadioMemoriesOverlay();
void closeRadioOverlay();
void updateRadioSettingsDisplay();
void updateRadioMemoriesDisplay();

// Helper to get key type string
const char* getKeyTypeString(KeyType type) {
    switch(type) {
        case KEY_STRAIGHT: return "Straight";
        case KEY_IAMBIC_A: return "Iambic A";
        case KEY_IAMBIC_B: return "Iambic B";
        case KEY_ULTIMATIC: return "Ultimatic";
        default: return "Unknown";
    }
}

// Update action bar button focus styling
void updateRadioActionBarFocus() {
    // Reset all buttons
    if (radio_btn_mode) {
        lv_obj_set_style_border_color(radio_btn_mode, LV_COLOR_BORDER_SUBTLE, 0);
        lv_obj_set_style_border_width(radio_btn_mode, 1, 0);
    }
    if (radio_btn_settings) {
        lv_obj_set_style_border_color(radio_btn_settings, LV_COLOR_BORDER_SUBTLE, 0);
        lv_obj_set_style_border_width(radio_btn_settings, 1, 0);
    }
    if (radio_btn_memories) {
        lv_obj_set_style_border_color(radio_btn_memories, LV_COLOR_BORDER_SUBTLE, 0);
        lv_obj_set_style_border_width(radio_btn_memories, 1, 0);
    }

    // Highlight focused button
    lv_obj_t* focused = NULL;
    switch(radio_action_focus) {
        case 0: focused = radio_btn_mode; break;
        case 1: focused = radio_btn_settings; break;
        case 2: focused = radio_btn_memories; break;
    }
    if (focused) {
        lv_obj_set_style_border_color(focused, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_set_style_border_width(focused, 2, 0);
    }
}

// Settings overlay row labels for highlighting
static lv_obj_t* settings_row_wpm = NULL;
static lv_obj_t* settings_row_keytype = NULL;
static lv_obj_t* settings_row_tone = NULL;
static lv_obj_t* settings_val_wpm = NULL;
static lv_obj_t* settings_val_keytype = NULL;
static lv_obj_t* settings_val_tone = NULL;

// Memory overlay display elements
static lv_obj_t* memory_rows[5] = {NULL};  // Show 5 at a time
static lv_obj_t* memory_labels[5] = {NULL};
static int memory_scroll_offset = 0;

void updateRadioSettingsDisplay() {
    // Update selection highlight
    lv_color_t normal_bg = LV_COLOR_BG_LAYER2;
    lv_color_t selected_bg = getThemeColors()->card_secondary;  // Theme highlight

    if (settings_row_wpm) {
        lv_obj_set_style_bg_color(settings_row_wpm,
            radio_settings_selection == 0 ? selected_bg : normal_bg, 0);
    }
    if (settings_row_keytype) {
        lv_obj_set_style_bg_color(settings_row_keytype,
            radio_settings_selection == 1 ? selected_bg : normal_bg, 0);
    }
    if (settings_row_tone) {
        lv_obj_set_style_bg_color(settings_row_tone,
            radio_settings_selection == 2 ? selected_bg : normal_bg, 0);
    }

    // Update values
    if (settings_val_wpm) lv_label_set_text_fmt(settings_val_wpm, "%d", cwSpeed);
    if (settings_val_keytype) lv_label_set_text(settings_val_keytype, getKeyTypeString(cwKeyType));
    if (settings_val_tone) lv_label_set_text_fmt(settings_val_tone, "%d Hz", cwTone);
}

void updateRadioMemoriesDisplay() {
    // Calculate scroll offset
    if (radio_memory_selection >= memory_scroll_offset + 5) {
        memory_scroll_offset = radio_memory_selection - 4;
    } else if (radio_memory_selection < memory_scroll_offset) {
        memory_scroll_offset = radio_memory_selection;
    }

    for (int i = 0; i < 5; i++) {
        int slot = memory_scroll_offset + i;
        if (slot >= CW_MEMORY_MAX_SLOTS) break;

        bool isSelected = (slot == radio_memory_selection);

        if (memory_rows[i]) {
            lv_obj_set_style_bg_color(memory_rows[i],
                isSelected ? getThemeColors()->card_secondary : LV_COLOR_BG_LAYER2, 0);
        }

        if (memory_labels[i]) {
            char buf[32];
            if (cwMemories[slot].isEmpty) {
                snprintf(buf, sizeof(buf), "%d. (empty)", slot + 1);
                lv_obj_set_style_text_color(memory_labels[i],
                    isSelected ? LV_COLOR_TEXT_SECONDARY : LV_COLOR_TEXT_DISABLED, 0);
            } else {
                snprintf(buf, sizeof(buf), "%d. %s", slot + 1, cwMemories[slot].label);
                lv_obj_set_style_text_color(memory_labels[i],
                    isSelected ? LV_COLOR_TEXT_PRIMARY : LV_COLOR_ACCENT_PRIMARY, 0);
            }
            lv_label_set_text(memory_labels[i], buf);
        }
    }
}

void closeRadioOverlay() {
    if (radio_overlay) {
        lv_obj_del(radio_overlay);
        radio_overlay = NULL;
    }
    radio_settings_active = false;
    radio_memories_active = false;

    // Clear row references
    settings_row_wpm = NULL;
    settings_row_keytype = NULL;
    settings_row_tone = NULL;
    settings_val_wpm = NULL;
    settings_val_keytype = NULL;
    settings_val_tone = NULL;
    for (int i = 0; i < 5; i++) {
        memory_rows[i] = NULL;
        memory_labels[i] = NULL;
    }
}

void createRadioSettingsOverlay() {
    if (radio_overlay) return;  // Already open

    radio_settings_active = true;
    radio_settings_selection = 0;

    // Create overlay container
    radio_overlay = lv_obj_create(radio_screen);
    lv_obj_set_size(radio_overlay, 320, 220);
    lv_obj_center(radio_overlay);
    lv_obj_set_style_bg_color(radio_overlay, LV_COLOR_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(radio_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(radio_overlay, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(radio_overlay, 2, 0);
    lv_obj_set_style_radius(radio_overlay, 12, 0);
    lv_obj_set_style_pad_all(radio_overlay, 15, 0);
    lv_obj_clear_flag(radio_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(radio_overlay);
    lv_label_set_text(title, "KEYER SETTINGS");
    lv_obj_set_style_text_color(title, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(title, getThemeFonts()->font_subtitle, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // WPM row
    settings_row_wpm = lv_obj_create(radio_overlay);
    lv_obj_set_size(settings_row_wpm, 280, 40);
    lv_obj_set_pos(settings_row_wpm, 5, 35);
    lv_obj_set_style_bg_color(settings_row_wpm, getThemeColors()->card_secondary, 0);
    lv_obj_set_style_bg_opa(settings_row_wpm, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(settings_row_wpm, 6, 0);
    lv_obj_set_style_border_width(settings_row_wpm, 0, 0);
    lv_obj_set_style_pad_hor(settings_row_wpm, 10, 0);
    lv_obj_clear_flag(settings_row_wpm, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* wpm_lbl = lv_label_create(settings_row_wpm);
    lv_label_set_text(wpm_lbl, "Speed (WPM)");
    lv_obj_set_style_text_color(wpm_lbl, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(wpm_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    settings_val_wpm = lv_label_create(settings_row_wpm);
    lv_label_set_text_fmt(settings_val_wpm, "%d", cwSpeed);
    lv_obj_set_style_text_color(settings_val_wpm, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(settings_val_wpm, getThemeFonts()->font_input, 0);
    lv_obj_align(settings_val_wpm, LV_ALIGN_RIGHT_MID, 0, 0);

    // Key Type row
    settings_row_keytype = lv_obj_create(radio_overlay);
    lv_obj_set_size(settings_row_keytype, 280, 40);
    lv_obj_set_pos(settings_row_keytype, 5, 80);
    lv_obj_set_style_bg_color(settings_row_keytype, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_bg_opa(settings_row_keytype, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(settings_row_keytype, 6, 0);
    lv_obj_set_style_border_width(settings_row_keytype, 0, 0);
    lv_obj_set_style_pad_hor(settings_row_keytype, 10, 0);
    lv_obj_clear_flag(settings_row_keytype, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* keytype_lbl = lv_label_create(settings_row_keytype);
    lv_label_set_text(keytype_lbl, "Key Type");
    lv_obj_set_style_text_color(keytype_lbl, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(keytype_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    settings_val_keytype = lv_label_create(settings_row_keytype);
    lv_label_set_text(settings_val_keytype, getKeyTypeString(cwKeyType));
    lv_obj_set_style_text_color(settings_val_keytype, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(settings_val_keytype, getThemeFonts()->font_input, 0);
    lv_obj_align(settings_val_keytype, LV_ALIGN_RIGHT_MID, 0, 0);

    // Tone row
    settings_row_tone = lv_obj_create(radio_overlay);
    lv_obj_set_size(settings_row_tone, 280, 40);
    lv_obj_set_pos(settings_row_tone, 5, 125);
    lv_obj_set_style_bg_color(settings_row_tone, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_bg_opa(settings_row_tone, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(settings_row_tone, 6, 0);
    lv_obj_set_style_border_width(settings_row_tone, 0, 0);
    lv_obj_set_style_pad_hor(settings_row_tone, 10, 0);
    lv_obj_clear_flag(settings_row_tone, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* tone_lbl = lv_label_create(settings_row_tone);
    lv_label_set_text(tone_lbl, "Sidetone");
    lv_obj_set_style_text_color(tone_lbl, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(tone_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    settings_val_tone = lv_label_create(settings_row_tone);
    lv_label_set_text_fmt(settings_val_tone, "%d Hz", cwTone);
    lv_obj_set_style_text_color(settings_val_tone, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(settings_val_tone, getThemeFonts()->font_input, 0);
    lv_obj_align(settings_val_tone, LV_ALIGN_RIGHT_MID, 0, 0);

    // Footer hint
    lv_obj_t* hint = lv_label_create(radio_overlay);
    lv_label_set_text(hint, "UP/DN Select   L/R Adjust   ESC Back");
    lv_obj_set_style_text_color(hint, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(hint, getThemeFonts()->font_small, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void createRadioMemoriesOverlay() {
    if (radio_overlay) return;  // Already open

    radio_memories_active = true;
    radio_memory_selection = 0;
    memory_scroll_offset = 0;

    // Create overlay container
    radio_overlay = lv_obj_create(radio_screen);
    lv_obj_set_size(radio_overlay, 320, 220);
    lv_obj_center(radio_overlay);
    lv_obj_set_style_bg_color(radio_overlay, LV_COLOR_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(radio_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(radio_overlay, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(radio_overlay, 2, 0);
    lv_obj_set_style_radius(radio_overlay, 12, 0);
    lv_obj_set_style_pad_all(radio_overlay, 15, 0);
    lv_obj_clear_flag(radio_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(radio_overlay);
    lv_label_set_text(title, "CW MEMORIES");
    lv_obj_set_style_text_color(title, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(title, getThemeFonts()->font_subtitle, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // Create 5 visible rows
    for (int i = 0; i < 5; i++) {
        memory_rows[i] = lv_obj_create(radio_overlay);
        lv_obj_set_size(memory_rows[i], 280, 30);
        lv_obj_set_pos(memory_rows[i], 5, 30 + (i * 32));
        lv_obj_set_style_bg_color(memory_rows[i], LV_COLOR_BG_LAYER2, 0);
        lv_obj_set_style_bg_opa(memory_rows[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(memory_rows[i], 4, 0);
        lv_obj_set_style_border_width(memory_rows[i], 0, 0);
        lv_obj_set_style_pad_hor(memory_rows[i], 10, 0);
        lv_obj_clear_flag(memory_rows[i], LV_OBJ_FLAG_SCROLLABLE);

        memory_labels[i] = lv_label_create(memory_rows[i]);
        lv_label_set_text(memory_labels[i], "");
        lv_obj_set_style_text_font(memory_labels[i], getThemeFonts()->font_body, 0);
        lv_obj_align(memory_labels[i], LV_ALIGN_LEFT_MID, 0, 0);
    }

    // Footer hint
    lv_obj_t* hint = lv_label_create(radio_overlay);
    lv_label_set_text(hint, "UP/DN Select   ENTER Send   ESC Back");
    lv_obj_set_style_text_color(hint, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(hint, getThemeFonts()->font_small, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Initialize display
    updateRadioMemoriesDisplay();
}

// Key event callback for Radio Output - handles action bar and overlays
static void radio_key_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);
    Serial.printf("[Radio LVGL] Key event: %lu (0x%02lX)\n", key, key);

    // Handle settings overlay
    if (radio_settings_active) {
        switch(key) {
            case LV_KEY_ESC:
                closeRadioOverlay();
                beep(TONE_MENU_NAV, BEEP_SHORT);
                lv_event_stop_processing(e);
                return;
            case LV_KEY_UP:
            case LV_KEY_PREV:
                if (radio_settings_selection > 0) {
                    radio_settings_selection--;
                    updateRadioSettingsDisplay();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                }
                lv_event_stop_processing(e);
                return;
            case LV_KEY_DOWN:
            case LV_KEY_NEXT:
                if (radio_settings_selection < 2) {
                    radio_settings_selection++;
                    updateRadioSettingsDisplay();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                }
                lv_event_stop_processing(e);
                return;
            case LV_KEY_LEFT:
                // Decrease value
                if (radio_settings_selection == 0) {
                    // WPM
                    if (cwSpeed > 5) {
                        cwSpeed--;
                        markDeferredSave(saveCWSettings);
                        updateRadioSettingsDisplay();
                        if (radio_wpm_label) lv_label_set_text_fmt(radio_wpm_label, "%d WPM", cwSpeed);
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                } else if (radio_settings_selection == 1) {
                    // Key Type (cycle backward: Ultimatic -> Iambic B -> Iambic A -> Straight)
                    if (cwKeyType == KEY_ULTIMATIC) cwKeyType = KEY_IAMBIC_B;
                    else if (cwKeyType == KEY_IAMBIC_B) cwKeyType = KEY_IAMBIC_A;
                    else if (cwKeyType == KEY_IAMBIC_A) cwKeyType = KEY_STRAIGHT;
                    else cwKeyType = KEY_ULTIMATIC;
                    markDeferredSave(saveCWSettings);
                    updateRadioSettingsDisplay();
                    if (radio_keytype_label) lv_label_set_text(radio_keytype_label, getKeyTypeString(cwKeyType));
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                } else if (radio_settings_selection == 2) {
                    // Tone
                    if (cwTone > 400) {
                        cwTone -= 50;
                        markDeferredSave(saveCWSettings);
                        updateRadioSettingsDisplay();
                        if (radio_tone_label) lv_label_set_text_fmt(radio_tone_label, "%d Hz", cwTone);
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                }
                lv_event_stop_processing(e);
                return;
            case LV_KEY_RIGHT:
                // Increase value
                if (radio_settings_selection == 0) {
                    // WPM
                    if (cwSpeed < 40) {
                        cwSpeed++;
                        markDeferredSave(saveCWSettings);
                        updateRadioSettingsDisplay();
                        if (radio_wpm_label) lv_label_set_text_fmt(radio_wpm_label, "%d WPM", cwSpeed);
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                } else if (radio_settings_selection == 1) {
                    // Key Type (cycle forward: Straight -> Iambic A -> Iambic B -> Ultimatic)
                    if (cwKeyType == KEY_STRAIGHT) cwKeyType = KEY_IAMBIC_A;
                    else if (cwKeyType == KEY_IAMBIC_A) cwKeyType = KEY_IAMBIC_B;
                    else if (cwKeyType == KEY_IAMBIC_B) cwKeyType = KEY_ULTIMATIC;
                    else cwKeyType = KEY_STRAIGHT;
                    markDeferredSave(saveCWSettings);
                    updateRadioSettingsDisplay();
                    if (radio_keytype_label) lv_label_set_text(radio_keytype_label, getKeyTypeString(cwKeyType));
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                } else if (radio_settings_selection == 2) {
                    // Tone
                    if (cwTone < 1000) {
                        cwTone += 50;
                        markDeferredSave(saveCWSettings);
                        updateRadioSettingsDisplay();
                        if (radio_tone_label) lv_label_set_text_fmt(radio_tone_label, "%d Hz", cwTone);
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                }
                lv_event_stop_processing(e);
                return;
        }
        return;
    }

    // Handle memories overlay
    if (radio_memories_active) {
        switch(key) {
            case LV_KEY_ESC:
                closeRadioOverlay();
                beep(TONE_MENU_NAV, BEEP_SHORT);
                lv_event_stop_processing(e);
                return;
            case LV_KEY_UP:
            case LV_KEY_PREV:
                if (radio_memory_selection > 0) {
                    radio_memory_selection--;
                    updateRadioMemoriesDisplay();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                }
                lv_event_stop_processing(e);
                return;
            case LV_KEY_DOWN:
            case LV_KEY_NEXT:
                if (radio_memory_selection < CW_MEMORY_MAX_SLOTS - 1) {
                    radio_memory_selection++;
                    updateRadioMemoriesDisplay();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                }
                lv_event_stop_processing(e);
                return;
            case LV_KEY_ENTER:
                // Send selected memory
                if (!cwMemories[radio_memory_selection].isEmpty) {
                    bool success = queueRadioMessage(cwMemories[radio_memory_selection].message);
                    closeRadioOverlay();
                    if (success) {
                        beep(TONE_SUCCESS, BEEP_MEDIUM);
                        if (radio_status_label) {
                            lv_label_set_text(radio_status_label, "Sending memory...");
                        }
                    } else {
                        beep(TONE_ERROR, BEEP_SHORT);
                    }
                } else {
                    beep(TONE_ERROR, BEEP_SHORT);
                }
                lv_event_stop_processing(e);
                return;
        }
        return;
    }

    // Handle main screen action bar
    switch(key) {
        case LV_KEY_ESC:
            onLVGLBackNavigation();
            lv_event_stop_processing(e);
            break;
        case LV_KEY_LEFT:
            if (radio_action_focus > 0) {
                radio_action_focus--;
                updateRadioActionBarFocus();
                beep(TONE_MENU_NAV, BEEP_SHORT);
            }
            lv_event_stop_processing(e);
            break;
        case LV_KEY_RIGHT:
            if (radio_action_focus < 2) {
                radio_action_focus++;
                updateRadioActionBarFocus();
                beep(TONE_MENU_NAV, BEEP_SHORT);
            }
            lv_event_stop_processing(e);
            break;
        case LV_KEY_ENTER:
            // Activate focused action
            if (radio_action_focus == 0) {
                // Toggle mode
                if (radioMode == RADIO_MODE_SUMMIT_KEYER) {
                    radioMode = RADIO_MODE_RADIO_KEYER;
                } else {
                    radioMode = RADIO_MODE_SUMMIT_KEYER;
                }
                saveRadioSettings();
                if (radio_mode_label) {
                    lv_label_set_text(radio_mode_label,
                        radioMode == RADIO_MODE_SUMMIT_KEYER ? "Summit Keyer" : "Radio Keyer");
                }
                beep(TONE_SUCCESS, BEEP_SHORT);
            } else if (radio_action_focus == 1) {
                // Open settings
                createRadioSettingsOverlay();
                beep(TONE_SELECT, BEEP_SHORT);
            } else if (radio_action_focus == 2) {
                // Open memories
                createRadioMemoriesOverlay();
                beep(TONE_SELECT, BEEP_SHORT);
            }
            lv_event_stop_processing(e);
            break;
    }
}

lv_obj_t* createRadioOutputScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "RADIO OUTPUT");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // Mode card - shows current keyer mode
    lv_obj_t* mode_card = lv_obj_create(screen);
    lv_obj_set_size(mode_card, SCREEN_WIDTH - 40, 70);
    lv_obj_set_pos(mode_card, 20, HEADER_HEIGHT + 10);
    lv_obj_set_layout(mode_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(mode_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mode_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    applyCardStyle(mode_card);

    lv_obj_t* mode_title = lv_label_create(mode_card);
    lv_label_set_text(mode_title, "Keyer Mode");
    lv_obj_add_style(mode_title, getStyleLabelBody(), 0);

    radio_mode_label = lv_label_create(mode_card);
    lv_label_set_text(radio_mode_label, radioMode == RADIO_MODE_SUMMIT_KEYER ? "Summit Keyer" : "Radio Keyer");
    lv_obj_set_style_text_font(radio_mode_label, getThemeFonts()->font_title, 0);
    lv_obj_set_style_text_color(radio_mode_label, LV_COLOR_ACCENT_PRIMARY, 0);

    // Settings display row - WPM, Key Type, Tone
    lv_obj_t* settings_card = lv_obj_create(screen);
    lv_obj_set_size(settings_card, SCREEN_WIDTH - 40, 50);
    lv_obj_set_pos(settings_card, 20, HEADER_HEIGHT + 90);
    lv_obj_set_layout(settings_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(settings_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(settings_card, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    applyCardStyle(settings_card);

    // WPM display
    lv_obj_t* wpm_container = lv_obj_create(settings_card);
    lv_obj_set_size(wpm_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(wpm_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wpm_container, 0, 0);
    lv_obj_set_style_pad_all(wpm_container, 0, 0);
    lv_obj_clear_flag(wpm_container, LV_OBJ_FLAG_SCROLLABLE);

    radio_wpm_label = lv_label_create(wpm_container);
    lv_label_set_text_fmt(radio_wpm_label, "%d WPM", cwSpeed);
    lv_obj_set_style_text_color(radio_wpm_label, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(radio_wpm_label, getThemeFonts()->font_body, 0);

    // Key Type display
    lv_obj_t* keytype_container = lv_obj_create(settings_card);
    lv_obj_set_size(keytype_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(keytype_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(keytype_container, 0, 0);
    lv_obj_set_style_pad_all(keytype_container, 0, 0);
    lv_obj_clear_flag(keytype_container, LV_OBJ_FLAG_SCROLLABLE);

    radio_keytype_label = lv_label_create(keytype_container);
    lv_label_set_text(radio_keytype_label, getKeyTypeString(cwKeyType));
    lv_obj_set_style_text_color(radio_keytype_label, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(radio_keytype_label, getThemeFonts()->font_body, 0);

    // Tone display
    lv_obj_t* tone_container = lv_obj_create(settings_card);
    lv_obj_set_size(tone_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(tone_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tone_container, 0, 0);
    lv_obj_set_style_pad_all(tone_container, 0, 0);
    lv_obj_clear_flag(tone_container, LV_OBJ_FLAG_SCROLLABLE);

    radio_tone_label = lv_label_create(tone_container);
    lv_label_set_text_fmt(radio_tone_label, "%d Hz", cwTone);
    lv_obj_set_style_text_color(radio_tone_label, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(radio_tone_label, getThemeFonts()->font_body, 0);

    // Status text
    radio_status_label = lv_label_create(screen);
    lv_label_set_text(radio_status_label, "Ready - Use paddle to key radio");
    lv_obj_add_style(radio_status_label, getStyleLabelBody(), 0);
    lv_obj_set_pos(radio_status_label, 20, HEADER_HEIGHT + 150);

    // Action bar container
    lv_obj_t* action_bar = lv_obj_create(screen);
    lv_obj_set_size(action_bar, SCREEN_WIDTH - 40, 50);
    lv_obj_set_pos(action_bar, 20, SCREEN_HEIGHT - FOOTER_HEIGHT - 60);
    lv_obj_set_layout(action_bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(action_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(action_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(action_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(action_bar, 0, 0);
    lv_obj_set_style_pad_all(action_bar, 0, 0);
    lv_obj_clear_flag(action_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Mode button
    radio_btn_mode = lv_obj_create(action_bar);
    lv_obj_set_size(radio_btn_mode, 120, 40);
    lv_obj_set_style_bg_color(radio_btn_mode, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_radius(radio_btn_mode, 8, 0);
    lv_obj_set_style_border_color(radio_btn_mode, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(radio_btn_mode, 2, 0);
    lv_obj_clear_flag(radio_btn_mode, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* mode_btn_lbl = lv_label_create(radio_btn_mode);
    lv_label_set_text(mode_btn_lbl, "Mode");
    lv_obj_set_style_text_color(mode_btn_lbl, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(mode_btn_lbl, getThemeFonts()->font_body, 0);
    lv_obj_center(mode_btn_lbl);

    // Settings button
    radio_btn_settings = lv_obj_create(action_bar);
    lv_obj_set_size(radio_btn_settings, 120, 40);
    lv_obj_set_style_bg_color(radio_btn_settings, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_radius(radio_btn_settings, 8, 0);
    lv_obj_set_style_border_color(radio_btn_settings, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_border_width(radio_btn_settings, 1, 0);
    lv_obj_clear_flag(radio_btn_settings, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* settings_btn_lbl = lv_label_create(radio_btn_settings);
    lv_label_set_text(settings_btn_lbl, "Settings");
    lv_obj_set_style_text_color(settings_btn_lbl, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(settings_btn_lbl, getThemeFonts()->font_body, 0);
    lv_obj_center(settings_btn_lbl);

    // Memories button
    radio_btn_memories = lv_obj_create(action_bar);
    lv_obj_set_size(radio_btn_memories, 120, 40);
    lv_obj_set_style_bg_color(radio_btn_memories, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_radius(radio_btn_memories, 8, 0);
    lv_obj_set_style_border_color(radio_btn_memories, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_border_width(radio_btn_memories, 1, 0);
    lv_obj_clear_flag(radio_btn_memories, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* memories_btn_lbl = lv_label_create(radio_btn_memories);
    lv_label_set_text(memories_btn_lbl, "Memories");
    lv_obj_set_style_text_color(memories_btn_lbl, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(memories_btn_lbl, getThemeFonts()->font_body, 0);
    lv_obj_center(memories_btn_lbl);

    // Reset action bar focus state
    radio_action_focus = 0;

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, "L/R Select   ENTER Activate   ESC Back");
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    // Invisible focus container for keyboard input
    lv_obj_t* focus_container = lv_obj_create(screen);
    lv_obj_set_size(focus_container, 1, 1);
    lv_obj_set_pos(focus_container, -10, -10);
    lv_obj_set_style_bg_opa(focus_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus_container, 0, 0);
    lv_obj_set_style_outline_width(focus_container, 0, 0);
    lv_obj_set_style_outline_width(focus_container, 0, LV_STATE_FOCUSED);
    lv_obj_clear_flag(focus_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(focus_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus_container, radio_key_event_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus_container);
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }
    lv_group_focus_obj(focus_container);

    radio_screen = screen;

    // Reset overlay state
    radio_overlay = NULL;
    radio_settings_active = false;
    radio_memories_active = false;

    return screen;
}

void updateRadioMode(const char* mode) {
    if (radio_mode_label != NULL) {
        lv_label_set_text(radio_mode_label, mode);
    }
}

void updateRadioWPM(int wpm) {
    if (radio_wpm_label != NULL) {
        lv_label_set_text_fmt(radio_wpm_label, "%d WPM", wpm);
    }
}

void updateRadioStatus(const char* status) {
    if (radio_status_label != NULL) {
        lv_label_set_text(radio_status_label, status);
    }
}

void cleanupRadioOutputScreen() {
    closeRadioOverlay();
    radio_screen = NULL;
    radio_mode_label = NULL;
    radio_status_label = NULL;
    radio_wpm_label = NULL;
    radio_tone_label = NULL;
    radio_keytype_label = NULL;
    radio_btn_mode = NULL;
    radio_btn_settings = NULL;
    radio_btn_memories = NULL;
}

// ============================================
// CW Memories Screen - Full LVGL Implementation
// ============================================

// Forward declarations
extern void previewCWMemory(int slot);
extern void saveCWMemory(int slot);
extern void deleteCWMemory(int slot);
extern bool isValidMorseMessage(const char* message);

// CW Memories screen state
static lv_obj_t* cwmem_screen = NULL;
static lv_obj_t* cwmem_rows[5] = {NULL};      // Show 5 slots at a time
static lv_obj_t* cwmem_labels[5] = {NULL};    // Labels for each visible row
static int cwmem_selection = 0;               // Currently selected slot (0-9)
static int cwmem_scroll_offset = 0;           // Scroll offset for displaying

// Overlay state
static lv_obj_t* cwmem_overlay = NULL;
static bool cwmem_context_active = false;     // Context menu open
static bool cwmem_edit_active = false;        // Edit overlay open
static bool cwmem_delete_active = false;      // Delete confirmation open
static int cwmem_context_selection = 0;       // Context menu selection

// Edit state
static bool cwmem_editing_label = true;       // true=label, false=message
static char cwmem_edit_label[16] = {0};       // Label buffer (max 15 chars)
static char cwmem_edit_message[101] = {0};    // Message buffer (max 100 chars)
static lv_obj_t* cwmem_edit_textarea = NULL;
static lv_obj_t* cwmem_edit_title = NULL;
static lv_obj_t* cwmem_edit_prompt = NULL;
static lv_obj_t* cwmem_edit_counter = NULL;

// Forward declarations
void updateCWMemoriesDisplay();
void closeCWMemOverlay();
void createCWMemContextMenu();
void createCWMemEditOverlay();
void createCWMemDeleteConfirm();
static void cwmem_key_event_cb(lv_event_t* e);

// Update the visible memory slots display
void updateCWMemoriesDisplay() {
    // Adjust scroll offset if selection moves outside visible range
    if (cwmem_selection >= cwmem_scroll_offset + 5) {
        cwmem_scroll_offset = cwmem_selection - 4;
    } else if (cwmem_selection < cwmem_scroll_offset) {
        cwmem_scroll_offset = cwmem_selection;
    }

    for (int i = 0; i < 5; i++) {
        int slot = cwmem_scroll_offset + i;
        if (slot >= CW_MEMORY_MAX_SLOTS) break;

        bool isSelected = (slot == cwmem_selection);

        if (cwmem_rows[i]) {
            lv_obj_set_style_bg_color(cwmem_rows[i],
                isSelected ? getThemeColors()->card_secondary : LV_COLOR_BG_LAYER2, 0);
            lv_obj_set_style_border_color(cwmem_rows[i],
                isSelected ? LV_COLOR_ACCENT_PRIMARY : LV_COLOR_BORDER_SUBTLE, 0);
            lv_obj_set_style_border_width(cwmem_rows[i], isSelected ? 2 : 1, 0);
        }

        if (cwmem_labels[i]) {
            char buf[40];
            if (cwMemories[slot].isEmpty) {
                snprintf(buf, sizeof(buf), "%d.  (Empty)", slot + 1);
                lv_obj_set_style_text_color(cwmem_labels[i],
                    isSelected ? LV_COLOR_TEXT_SECONDARY : LV_COLOR_TEXT_DISABLED, 0);
            } else {
                snprintf(buf, sizeof(buf), "%d.  %s", slot + 1, cwMemories[slot].label);
                lv_obj_set_style_text_color(cwmem_labels[i],
                    isSelected ? LV_COLOR_TEXT_PRIMARY : LV_COLOR_ACCENT_PRIMARY, 0);
            }
            lv_label_set_text(cwmem_labels[i], buf);
        }
    }
}

// Close any open overlay
void closeCWMemOverlay() {
    if (cwmem_overlay) {
        lv_obj_del(cwmem_overlay);
        cwmem_overlay = NULL;
    }
    cwmem_context_active = false;
    cwmem_edit_active = false;
    cwmem_delete_active = false;
    cwmem_edit_textarea = NULL;
    cwmem_edit_title = NULL;
    cwmem_edit_prompt = NULL;
    cwmem_edit_counter = NULL;
}

// Create context menu overlay
void createCWMemContextMenu() {
    if (cwmem_overlay) return;

    cwmem_context_active = true;
    cwmem_context_selection = 0;

    bool isEmpty = cwMemories[cwmem_selection].isEmpty;
    int numOptions = isEmpty ? 2 : 4;
    int overlayHeight = isEmpty ? 130 : 210;

    // Create overlay
    cwmem_overlay = lv_obj_create(cwmem_screen);
    lv_obj_set_size(cwmem_overlay, 280, overlayHeight);
    lv_obj_center(cwmem_overlay);
    lv_obj_set_style_bg_color(cwmem_overlay, LV_COLOR_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(cwmem_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cwmem_overlay, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(cwmem_overlay, 2, 0);
    lv_obj_set_style_radius(cwmem_overlay, 12, 0);
    lv_obj_set_style_pad_all(cwmem_overlay, 15, 0);
    lv_obj_clear_flag(cwmem_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(cwmem_overlay);
    if (isEmpty) {
        lv_label_set_text_fmt(title, "SLOT %d - EMPTY", cwmem_selection + 1);
    } else {
        lv_label_set_text_fmt(title, "SLOT %d", cwmem_selection + 1);
    }
    lv_obj_set_style_text_color(title, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(title, getThemeFonts()->font_subtitle, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // Show label if not empty
    if (!isEmpty) {
        lv_obj_t* label = lv_label_create(cwmem_overlay);
        lv_label_set_text(label, cwMemories[cwmem_selection].label);
        lv_obj_set_style_text_color(label, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(label, getThemeFonts()->font_body, 0);
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 24);
    }

    // Menu options
    const char* options[4];
    if (isEmpty) {
        options[0] = "Create";
        options[1] = "Cancel";
    } else {
        options[0] = "Preview";
        options[1] = "Edit";
        options[2] = "Delete";
        options[3] = "Cancel";
    }

    int startY = isEmpty ? 40 : 55;
    for (int i = 0; i < numOptions; i++) {
        lv_obj_t* opt = lv_label_create(cwmem_overlay);
        lv_label_set_text(opt, options[i]);
        lv_obj_set_style_text_font(opt, getThemeFonts()->font_body, 0);
        lv_obj_set_style_text_color(opt,
            (i == cwmem_context_selection) ? LV_COLOR_TEXT_PRIMARY : LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_align(opt, LV_ALIGN_TOP_MID, 0, startY + (i * 28));
    }

    // Footer hint
    lv_obj_t* hint = lv_label_create(cwmem_overlay);
    lv_label_set_text(hint, FOOTER_CONTEXT_MENU);
    lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_set_style_text_font(hint, getThemeFonts()->font_small, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);
}

// Update context menu selection display
void updateCWMemContextDisplay() {
    if (!cwmem_overlay || !cwmem_context_active) return;

    bool isEmpty = cwMemories[cwmem_selection].isEmpty;
    int numOptions = isEmpty ? 2 : 4;
    int startY = isEmpty ? 40 : 55;

    const char* options[4];
    if (isEmpty) {
        options[0] = "Create";
        options[1] = "Cancel";
    } else {
        options[0] = "Preview";
        options[1] = "Edit";
        options[2] = "Delete";
        options[3] = "Cancel";
    }

    // Recreate overlay to update highlighting
    closeCWMemOverlay();
    cwmem_context_active = true;

    int overlayHeight = isEmpty ? 130 : 210;

    cwmem_overlay = lv_obj_create(cwmem_screen);
    lv_obj_set_size(cwmem_overlay, 280, overlayHeight);
    lv_obj_center(cwmem_overlay);
    lv_obj_set_style_bg_color(cwmem_overlay, LV_COLOR_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(cwmem_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cwmem_overlay, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(cwmem_overlay, 2, 0);
    lv_obj_set_style_radius(cwmem_overlay, 12, 0);
    lv_obj_set_style_pad_all(cwmem_overlay, 15, 0);
    lv_obj_clear_flag(cwmem_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(cwmem_overlay);
    if (isEmpty) {
        lv_label_set_text_fmt(title, "SLOT %d - EMPTY", cwmem_selection + 1);
    } else {
        lv_label_set_text_fmt(title, "SLOT %d", cwmem_selection + 1);
    }
    lv_obj_set_style_text_color(title, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(title, getThemeFonts()->font_subtitle, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    if (!isEmpty) {
        lv_obj_t* label = lv_label_create(cwmem_overlay);
        lv_label_set_text(label, cwMemories[cwmem_selection].label);
        lv_obj_set_style_text_color(label, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(label, getThemeFonts()->font_body, 0);
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 24);
    }

    for (int i = 0; i < numOptions; i++) {
        lv_obj_t* opt = lv_label_create(cwmem_overlay);
        lv_label_set_text(opt, options[i]);
        lv_obj_set_style_text_font(opt, getThemeFonts()->font_body, 0);
        if (i == cwmem_context_selection) {
            lv_obj_set_style_text_color(opt, LV_COLOR_TEXT_PRIMARY, 0);
            // Add selection indicator
            lv_label_set_text_fmt(opt, "> %s", options[i]);
        } else {
            lv_obj_set_style_text_color(opt, LV_COLOR_TEXT_SECONDARY, 0);
        }
        lv_obj_align(opt, LV_ALIGN_TOP_MID, 0, startY + (i * 28));
    }

    lv_obj_t* hint = lv_label_create(cwmem_overlay);
    lv_label_set_text(hint, FOOTER_CONTEXT_MENU);
    lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_set_style_text_font(hint, getThemeFonts()->font_small, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);
}

// Create edit overlay for label or message
void createCWMemEditOverlay() {
    if (cwmem_overlay) return;

    cwmem_edit_active = true;
    cwmem_editing_label = true;

    // If creating new, clear buffers; if editing, load existing
    if (cwMemories[cwmem_selection].isEmpty) {
        cwmem_edit_label[0] = '\0';
        cwmem_edit_message[0] = '\0';
    } else {
        strlcpy(cwmem_edit_label, cwMemories[cwmem_selection].label, sizeof(cwmem_edit_label));
        strlcpy(cwmem_edit_message, cwMemories[cwmem_selection].message, sizeof(cwmem_edit_message));
    }

    cwmem_overlay = lv_obj_create(cwmem_screen);
    lv_obj_set_size(cwmem_overlay, 400, 210);
    lv_obj_center(cwmem_overlay);
    lv_obj_set_style_bg_color(cwmem_overlay, LV_COLOR_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(cwmem_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cwmem_overlay, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(cwmem_overlay, 2, 0);
    lv_obj_set_style_radius(cwmem_overlay, 12, 0);
    lv_obj_set_style_pad_all(cwmem_overlay, 15, 0);
    lv_obj_clear_flag(cwmem_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    cwmem_edit_title = lv_label_create(cwmem_overlay);
    bool isNew = cwMemories[cwmem_selection].isEmpty;
    lv_label_set_text(cwmem_edit_title, isNew ? "CREATE PRESET" : "EDIT PRESET");
    lv_obj_set_style_text_color(cwmem_edit_title, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(cwmem_edit_title, getThemeFonts()->font_subtitle, 0);
    lv_obj_align(cwmem_edit_title, LV_ALIGN_TOP_MID, 0, 0);

    // Prompt
    cwmem_edit_prompt = lv_label_create(cwmem_overlay);
    lv_label_set_text(cwmem_edit_prompt, "Label (max 15 chars):");
    lv_obj_set_style_text_color(cwmem_edit_prompt, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(cwmem_edit_prompt, getThemeFonts()->font_body, 0);
    lv_obj_align(cwmem_edit_prompt, LV_ALIGN_TOP_LEFT, 5, 28);

    // Text area
    cwmem_edit_textarea = lv_textarea_create(cwmem_overlay);
    lv_obj_set_size(cwmem_edit_textarea, 360, 70);
    lv_obj_align(cwmem_edit_textarea, LV_ALIGN_TOP_MID, 0, 52);
    lv_textarea_set_one_line(cwmem_edit_textarea, true);
    lv_textarea_set_max_length(cwmem_edit_textarea, 15);
    lv_textarea_set_text(cwmem_edit_textarea, cwmem_edit_label);
    lv_obj_set_style_bg_color(cwmem_edit_textarea, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_color(cwmem_edit_textarea, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(cwmem_edit_textarea, 1, 0);
    lv_obj_set_style_text_color(cwmem_edit_textarea, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(cwmem_edit_textarea, getThemeFonts()->font_input, 0);

    // Character counter - align right below textarea
    cwmem_edit_counter = lv_label_create(cwmem_overlay);
    lv_label_set_text_fmt(cwmem_edit_counter, "%d / 15 chars", strlen(cwmem_edit_label));
    lv_obj_set_style_text_color(cwmem_edit_counter, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_set_style_text_font(cwmem_edit_counter, getThemeFonts()->font_small, 0);
    lv_obj_align(cwmem_edit_counter, LV_ALIGN_TOP_RIGHT, -5, 125);

    // Footer hint
    lv_obj_t* hint = lv_label_create(cwmem_overlay);
    lv_label_set_text(hint, "Type text   ENTER Next/Save   ESC Cancel");
    lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_set_style_text_font(hint, getThemeFonts()->font_small, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);
}

// Switch to message editing
void switchToMessageEdit() {
    if (!cwmem_overlay || !cwmem_edit_active) return;

    cwmem_editing_label = false;

    // Update prompt
    if (cwmem_edit_prompt) {
        lv_label_set_text(cwmem_edit_prompt, "Message (max 100 chars):");
    }

    // Update textarea
    if (cwmem_edit_textarea) {
        lv_textarea_set_one_line(cwmem_edit_textarea, false);
        lv_textarea_set_max_length(cwmem_edit_textarea, 100);
        lv_textarea_set_text(cwmem_edit_textarea, cwmem_edit_message);
        lv_obj_set_size(cwmem_edit_textarea, 360, 70);
    }

    // Update counter
    if (cwmem_edit_counter) {
        lv_label_set_text_fmt(cwmem_edit_counter, "%d / 100 chars", strlen(cwmem_edit_message));
    }
}

// Update character counter during editing
void updateCWMemEditCounter() {
    if (!cwmem_edit_counter || !cwmem_edit_textarea) return;

    const char* text = lv_textarea_get_text(cwmem_edit_textarea);
    int len = strlen(text);

    if (cwmem_editing_label) {
        lv_label_set_text_fmt(cwmem_edit_counter, "%d / 15 chars", len);
    } else {
        lv_label_set_text_fmt(cwmem_edit_counter, "%d / 100 chars", len);
    }
}

// Create delete confirmation overlay
void createCWMemDeleteConfirm() {
    if (cwmem_overlay) return;

    cwmem_delete_active = true;
    // Note: cwmem_context_selection is set by caller, not here (to avoid reset on redraw)

    cwmem_overlay = lv_obj_create(cwmem_screen);
    lv_obj_set_size(cwmem_overlay, 300, 190);
    lv_obj_center(cwmem_overlay);
    lv_obj_set_style_bg_color(cwmem_overlay, LV_COLOR_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(cwmem_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cwmem_overlay, LV_COLOR_ERROR, 0);
    lv_obj_set_style_border_width(cwmem_overlay, 2, 0);
    lv_obj_set_style_radius(cwmem_overlay, 12, 0);
    lv_obj_set_style_pad_all(cwmem_overlay, 15, 0);
    lv_obj_clear_flag(cwmem_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(cwmem_overlay);
    lv_label_set_text(title, "DELETE PRESET?");
    lv_obj_set_style_text_color(title, LV_COLOR_ERROR, 0);
    lv_obj_set_style_text_font(title, getThemeFonts()->font_subtitle, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // Label being deleted
    lv_obj_t* label = lv_label_create(cwmem_overlay);
    lv_label_set_text_fmt(label, "\"%s\"", cwMemories[cwmem_selection].label);
    lv_obj_set_style_text_color(label, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(label, getThemeFonts()->font_body, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 28);

    // Warning
    lv_obj_t* warn = lv_label_create(cwmem_overlay);
    lv_label_set_text(warn, "This cannot be undone");
    lv_obj_set_style_text_color(warn, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_set_style_text_font(warn, getThemeFonts()->font_small, 0);
    lv_obj_align(warn, LV_ALIGN_TOP_MID, 0, 50);

    // Yes button
    lv_obj_t* yes = lv_label_create(cwmem_overlay);
    lv_label_set_text(yes, cwmem_context_selection == 0 ? "> Yes, Delete" : "  Yes, Delete");
    lv_obj_set_style_text_color(yes,
        cwmem_context_selection == 0 ? LV_COLOR_ERROR : LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(yes, getThemeFonts()->font_body, 0);
    lv_obj_align(yes, LV_ALIGN_TOP_MID, 0, 80);

    // No button
    lv_obj_t* no = lv_label_create(cwmem_overlay);
    lv_label_set_text(no, cwmem_context_selection == 1 ? "> No, Cancel" : "  No, Cancel");
    lv_obj_set_style_text_color(no,
        cwmem_context_selection == 1 ? LV_COLOR_TEXT_PRIMARY : LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(no, getThemeFonts()->font_body, 0);
    lv_obj_align(no, LV_ALIGN_TOP_MID, 0, 108);

    // Footer hint
    lv_obj_t* hint = lv_label_create(cwmem_overlay);
    lv_label_set_text(hint, "UP/DN Select   ENTER Confirm   ESC Cancel");
    lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_set_style_text_font(hint, getThemeFonts()->font_small, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);
}

// Update delete confirmation display
void updateCWMemDeleteDisplay() {
    // Recreate the overlay with updated selection
    closeCWMemOverlay();
    createCWMemDeleteConfirm();
}

// Key event handler for CW Memories screen
static void cwmem_key_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);
    Serial.printf("[CWMem LVGL] Key event: %lu (0x%02lX)\n", key, key);

    // Handle delete confirmation
    if (cwmem_delete_active) {
        switch(key) {
            case LV_KEY_ESC:
                closeCWMemOverlay();
                beep(TONE_MENU_NAV, BEEP_SHORT);
                lv_event_stop_processing(e);
                return;
            case LV_KEY_UP:
            case LV_KEY_PREV:
            case LV_KEY_DOWN:
            case LV_KEY_NEXT:
                cwmem_context_selection = (cwmem_context_selection == 0) ? 1 : 0;
                updateCWMemDeleteDisplay();
                beep(TONE_MENU_NAV, BEEP_SHORT);
                lv_event_stop_processing(e);
                return;
            case LV_KEY_ENTER:
                if (cwmem_context_selection == 0) {
                    // Delete confirmed
                    deleteCWMemory(cwmem_selection);
                    beep(TONE_SUCCESS, BEEP_MEDIUM);
                } else {
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                }
                closeCWMemOverlay();
                updateCWMemoriesDisplay();
                lv_event_stop_processing(e);
                return;
        }
        return;
    }

    // Handle edit mode
    if (cwmem_edit_active) {
        switch(key) {
            case LV_KEY_ESC:
                closeCWMemOverlay();
                beep(TONE_MENU_NAV, BEEP_SHORT);
                lv_event_stop_processing(e);
                return;
            case LV_KEY_ENTER:
                if (cwmem_editing_label) {
                    // Save label and switch to message
                    const char* text = lv_textarea_get_text(cwmem_edit_textarea);
                    if (strlen(text) == 0) {
                        beep(TONE_ERROR, BEEP_SHORT);
                        lv_event_stop_processing(e);
                        return;
                    }
                    strlcpy(cwmem_edit_label, text, sizeof(cwmem_edit_label));
                    switchToMessageEdit();
                    beep(TONE_SELECT, BEEP_SHORT);
                } else {
                    // Save message and complete
                    const char* text = lv_textarea_get_text(cwmem_edit_textarea);
                    if (strlen(text) == 0) {
                        beep(TONE_ERROR, BEEP_SHORT);
                        lv_event_stop_processing(e);
                        return;
                    }
                    if (!isValidMorseMessage(text)) {
                        beep(TONE_ERROR, BEEP_LONG);
                        lv_event_stop_processing(e);
                        return;
                    }
                    strlcpy(cwmem_edit_message, text, sizeof(cwmem_edit_message));

                    // Save to memory slot
                    strlcpy(cwMemories[cwmem_selection].label, cwmem_edit_label, 16);
                    strlcpy(cwMemories[cwmem_selection].message, cwmem_edit_message, 101);
                    cwMemories[cwmem_selection].isEmpty = false;
                    saveCWMemory(cwmem_selection);

                    closeCWMemOverlay();
                    updateCWMemoriesDisplay();
                    beep(TONE_SUCCESS, BEEP_MEDIUM);
                }
                lv_event_stop_processing(e);
                return;
            case LV_KEY_BACKSPACE:
                // Handle backspace
                if (cwmem_edit_textarea) {
                    lv_textarea_del_char(cwmem_edit_textarea);
                    updateCWMemEditCounter();
                }
                lv_event_stop_processing(e);
                return;
            default:
                // Handle printable characters (32-126)
                if (key >= 32 && key <= 126) {
                    if (cwmem_edit_textarea) {
                        // Auto-uppercase for morse consistency
                        char c = (char)key;
                        if (c >= 'a' && c <= 'z') {
                            c = c - 'a' + 'A';
                        }
                        char str[2] = {c, '\0'};
                        lv_textarea_add_text(cwmem_edit_textarea, str);
                        updateCWMemEditCounter();
                    }
                    lv_event_stop_processing(e);
                }
                return;
        }
        return;
    }

    // Handle context menu
    if (cwmem_context_active) {
        bool isEmpty = cwMemories[cwmem_selection].isEmpty;
        int maxOptions = isEmpty ? 2 : 4;

        switch(key) {
            case LV_KEY_ESC:
                closeCWMemOverlay();
                beep(TONE_MENU_NAV, BEEP_SHORT);
                lv_event_stop_processing(e);
                return;
            case LV_KEY_UP:
            case LV_KEY_PREV:
                if (cwmem_context_selection > 0) {
                    cwmem_context_selection--;
                    updateCWMemContextDisplay();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                }
                lv_event_stop_processing(e);
                return;
            case LV_KEY_DOWN:
            case LV_KEY_NEXT:
                if (cwmem_context_selection < maxOptions - 1) {
                    cwmem_context_selection++;
                    updateCWMemContextDisplay();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                }
                lv_event_stop_processing(e);
                return;
            case LV_KEY_ENTER:
                if (isEmpty) {
                    // Empty slot: Create or Cancel
                    if (cwmem_context_selection == 0) {
                        closeCWMemOverlay();
                        createCWMemEditOverlay();
                        beep(TONE_SELECT, BEEP_SHORT);
                    } else {
                        closeCWMemOverlay();
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                } else {
                    // Occupied slot: Preview, Edit, Delete, Cancel
                    switch(cwmem_context_selection) {
                        case 0:  // Preview
                            closeCWMemOverlay();
                            previewCWMemory(cwmem_selection);
                            beep(TONE_SELECT, BEEP_SHORT);
                            break;
                        case 1:  // Edit
                            closeCWMemOverlay();
                            createCWMemEditOverlay();
                            beep(TONE_SELECT, BEEP_SHORT);
                            break;
                        case 2:  // Delete
                            closeCWMemOverlay();
                            cwmem_context_selection = 1;  // Default to "No" (set once when opening)
                            createCWMemDeleteConfirm();
                            beep(TONE_MENU_NAV, BEEP_SHORT);
                            break;
                        case 3:  // Cancel
                            closeCWMemOverlay();
                            beep(TONE_MENU_NAV, BEEP_SHORT);
                            break;
                    }
                }
                lv_event_stop_processing(e);
                return;
        }
        return;
    }

    // Handle main screen navigation
    switch(key) {
        case LV_KEY_ESC:
            onLVGLBackNavigation();
            lv_event_stop_processing(e);
            break;
        case LV_KEY_UP:
        case LV_KEY_PREV:
            if (cwmem_selection > 0) {
                cwmem_selection--;
                updateCWMemoriesDisplay();
                beep(TONE_MENU_NAV, BEEP_SHORT);
            }
            lv_event_stop_processing(e);
            break;
        case LV_KEY_DOWN:
        case LV_KEY_NEXT:
            if (cwmem_selection < CW_MEMORY_MAX_SLOTS - 1) {
                cwmem_selection++;
                updateCWMemoriesDisplay();
                beep(TONE_MENU_NAV, BEEP_SHORT);
            }
            lv_event_stop_processing(e);
            break;
        case LV_KEY_ENTER:
            createCWMemContextMenu();
            beep(TONE_SELECT, BEEP_SHORT);
            lv_event_stop_processing(e);
            break;
    }
}

lv_obj_t* createCWMemoriesScreen() {
    // Reset state
    cwmem_selection = 0;
    cwmem_scroll_offset = 0;
    cwmem_context_active = false;
    cwmem_edit_active = false;
    cwmem_delete_active = false;
    cwmem_overlay = NULL;

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "CW MEMORIES");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // Memory slots container (5 visible)
    lv_obj_t* list = lv_obj_create(screen);
    lv_obj_set_size(list, SCREEN_WIDTH - 40, 200);  // Height for 5 rows
    lv_obj_set_pos(list, 20, HEADER_HEIGHT + 10);
    lv_obj_set_layout(list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 5, 0);
    lv_obj_set_style_pad_all(list, 5, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    // Create 5 visible slot rows
    for (int i = 0; i < 5; i++) {
        cwmem_rows[i] = lv_obj_create(list);
        lv_obj_set_size(cwmem_rows[i], SCREEN_WIDTH - 60, 36);
        lv_obj_set_style_bg_color(cwmem_rows[i], LV_COLOR_BG_LAYER2, 0);
        lv_obj_set_style_bg_opa(cwmem_rows[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(cwmem_rows[i], 6, 0);
        lv_obj_set_style_border_color(cwmem_rows[i], LV_COLOR_BORDER_SUBTLE, 0);
        lv_obj_set_style_border_width(cwmem_rows[i], 1, 0);
        lv_obj_set_style_pad_hor(cwmem_rows[i], 12, 0);
        lv_obj_clear_flag(cwmem_rows[i], LV_OBJ_FLAG_SCROLLABLE);

        cwmem_labels[i] = lv_label_create(cwmem_rows[i]);
        lv_label_set_text(cwmem_labels[i], "");
        lv_obj_set_style_text_font(cwmem_labels[i], getThemeFonts()->font_body, 0);
        lv_obj_align(cwmem_labels[i], LV_ALIGN_LEFT_MID, 0, 0);
    }

    // Focus container for key events
    lv_obj_t* focus_container = lv_obj_create(screen);
    lv_obj_set_size(focus_container, 1, 1);
    lv_obj_set_pos(focus_container, 0, 0);
    lv_obj_set_style_bg_opa(focus_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus_container, 0, 0);
    lv_obj_add_flag(focus_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus_container, cwmem_key_event_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus_container);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate   ENTER Menu   ESC Back");
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    cwmem_screen = screen;

    // Initialize display with actual memory data
    updateCWMemoriesDisplay();

    return screen;
}

// Reset CW Memories static widget pointers on back-navigation so stale
// pointers are never dereferenced after the screen is deleted.
void cleanupCWMemoriesScreen() {
    closeCWMemOverlay();
    cwmem_screen = NULL;
    for (int i = 0; i < 5; i++) {
        cwmem_rows[i] = NULL;
        cwmem_labels[i] = NULL;
    }
}

// ============================================
// Vail Repeater Screen - Full LVGL Implementation
// ============================================

// External references from vail_repeater.h
extern VailState vailState;
extern String vailChannel;
extern String vailCallsign;
extern int connectedClients;
extern std::vector<ChatMessage> chatHistory;
extern std::vector<RoomInfo> activeRooms;
extern std::vector<UserInfo> connectedUsers;
extern String chatInput;
extern String roomInput;
extern int cwSpeed;
extern void connectToVail(String channel);
extern void disconnectFromVail();
extern void sendChatMessage(String message);
extern void addChatMessage(String callsign, String message);
extern void saveCWSettings();

// Vail screen static variables
static lv_obj_t* vail_screen = NULL;
static lv_obj_t* vail_chat_textarea = NULL;
static lv_obj_t* vail_status_label = NULL;  // Kept for compatibility but may be NULL
static lv_obj_t* vail_status_indicator = NULL;  // Small colored dot for status
static lv_obj_t* vail_loading_overlay = NULL;  // Loading overlay with spinner
// vail_room_label is the room name shown in the title bar (next to the
// receiver-lamp dot), so the current room is always glanceable without
// having to look at the chat panel header.
static lv_obj_t* vail_room_label = NULL;
static lv_obj_t* vail_wpm_label = NULL;
static lv_obj_t* vail_strip_batt = NULL;    // status strip battery glyph
static lv_obj_t* vail_strip_wifi = NULL;    // status strip WiFi glyph
static lv_obj_t* vail_listen_badge = NULL;   // "TX OFF" strip badge when listen-only
static lv_obj_t* vail_onair_pill = NULL;     // Red "ON AIR" pill, lit while transmitting
static lv_obj_t* vail_tx_strip_label = NULL; // TX strip text: keying goes out vs local-only

// Operating view + Settings sub-view. Operating view (1) is the default and
// the equivalent of the web repeater's main screen — chat history, user
// list, decoded text (on Decoder room only), TX morse visualizer.
// Settings (2) is reached via 'S' hotkey. Room picker, chat compose, etc.
// are modal overlays opened from the operating view via single-key hotkeys.
static lv_obj_t* vail_chat_panel = NULL;     // Operating view
static lv_obj_t* vail_settings_panel = NULL; // Settings sub-view (view 2)
static lv_obj_t* vail_decoded_row_bg = NULL;
static lv_obj_t* vail_decoded_row_label = NULL;
static lv_obj_t* vail_users_label = NULL;       // Side-pane operator list (newline-joined)
static lv_obj_t* vail_side_ops_title = NULL;    // Side-pane kicker: "OPERATORS (n)"
static lv_obj_t* vail_tile_listen_icon = NULL;  // Listen tile keycap (color reflects state)
static lv_obj_t* vail_tile_listen_label = NULL; // Listen tile label (text reflects state)
static int vail_current_view = 1;  // 1=Operating (chat panel), 2=Settings

// Hero-card chat heights: full when the decoded row is hidden, reduced on the
// Decoder room where the decoded-text row occupies the card's bottom strip.
#define VAIL_CHAT_H_FULL 110
#define VAIL_CHAT_H_DECODER 64

// Settings screen rows.
// Decoded-row visibility is no longer a toggle; it's automatic when joined
// to the dedicated "Decoder" room (matches vailmorse.com behavior).
#define VAIL_SETTINGS_ROW_COUNT 3
static lv_obj_t* vail_srow_containers[VAIL_SETTINGS_ROW_COUNT];
static lv_obj_t* vail_srow_values[VAIL_SETTINGS_ROW_COUNT];
static int vail_settings_focus = 0;
static const char* vail_keytype_names[] = {"Straight", "Iambic A", "Iambic B", "Ultimatic"};

// Legacy modal (kept for potential reuse, no longer opened from UI)
static lv_obj_t* vail_settings_modal = NULL;
static lv_obj_t* vail_settings_value_label = NULL;
static lv_obj_t* vail_settings_title_label = NULL;
static int vail_settings_modal_type = 0;
static int vail_settings_temp_value = 0;


// Overlay elements
static lv_obj_t* vail_room_overlay = NULL;
static lv_obj_t* vail_room_list = NULL;
static lv_obj_t* vail_room_input_textarea = NULL;
static lv_obj_t* vail_chat_input_overlay = NULL;  // Legacy - keeping for now
static lv_obj_t* vail_chat_input_textarea = NULL;
static lv_obj_t* vail_user_list_overlay = NULL;
static lv_obj_t* vail_user_list = NULL;

// View state: 1=operating view (default), 2=rooms overlay, 3=users overlay,
// 4=callsign required, 5=speed modal, 6=tone modal, 7=keytype modal,
// 9=settings sub-view, 10=chat compose modal.
static int vail_view_mode = 1;
static int vail_room_selection = 0;
static int vail_user_scroll = 0;
static size_t vail_last_chat_count = 0;
static bool vail_custom_room_mode = false;
static bool vail_callsign_required = false;  // True if user needs to set callsign
static lv_obj_t* vail_callsign_overlay = NULL;
static VailState lastKnownVailState = VAIL_DISCONNECTED;  // Track connection state changes
#define VAIL_MAX_INPUT_LEN 20  // Caps morse airtime to ~13s at 18 WPM
static String vail_chat_input_text = "";  // Text being typed in chat view

// Deferred room reconnect: disconnect happens immediately in the key handler,
// the reconnect fires from updateVailScreenLVGL() after a 250ms settle so the
// audio-critical loop never blocks on delay().
static char vail_pending_room[64] = "";
static unsigned long vail_reconnect_at_ms = 0;

// The room picker shows the server's active-rooms list (rooms with users in
// them) plus a pinned "Decoder" entry when the server list doesn't include
// it — Decoder is a special always-available room (RX morse→text decoding)
// that would otherwise only appear while someone happens to be in it.
static bool vailRoomsHasDecoder() {
    for (size_t i = 0; i < activeRooms.size(); i++) {
        if (activeRooms[i].name == "Decoder") return true;
    }
    return false;
}

// Number of joinable rooms shown (server rooms + pinned Decoder). The
// "Custom room..." option sits at this index.
static int vailRoomListCount() {
    return (int)activeRooms.size() + (vailRoomsHasDecoder() ? 0 : 1);
}

static const char* vailRoomNameAt(int i) {
    if (i < (int)activeRooms.size()) return activeRooms[i].name.c_str();
    return "Decoder";  // the pinned entry
}

// Forward declarations
static void showVailRoomOverlay();
static void hideVailRoomOverlay();
static void showVailChatInputOverlay();
static void hideVailChatInputOverlay();
static void showVailUserListOverlay();
static void hideVailUserListOverlay();
static void updateVailRoomList();
static void updateVailUserList();
static void showVailCallsignRequiredOverlay();
static bool checkVailCallsignRequired();
static void switchVailView(int view);
static void showVailSettingsModal(int type);
static void hideVailSettingsModal();
static void updateVailSettingsDisplay();
static void updateVailFooter();
// Switch between Operating view (1) and Settings sub-view (2).
// vail_view_mode drives the key handler; settings uses 9 to avoid
// colliding with the room overlay which owns mode 2.
static void switchVailView(int view) {
    vail_current_view = view;
    vail_view_mode = (view == 2) ? 9 : view;
    vailChatMode = false;  // Chat compose only happens in the modal overlay now

    if (vail_chat_panel != NULL)     lv_obj_add_flag(vail_chat_panel,     LV_OBJ_FLAG_HIDDEN);
    if (vail_settings_panel != NULL) lv_obj_add_flag(vail_settings_panel, LV_OBJ_FLAG_HIDDEN);

    switch (view) {
        case 1: if (vail_chat_panel != NULL)     lv_obj_clear_flag(vail_chat_panel,     LV_OBJ_FLAG_HIDDEN); break;
        case 2: if (vail_settings_panel != NULL) lv_obj_clear_flag(vail_settings_panel, LV_OBJ_FLAG_HIDDEN); break;
    }

    updateVailFooter();
}

// Refresh the break-in (TX enabled vs listen-only) state indicators: the
// Break-In tile in the bottom row, the TX strip between the panes and the
// tiles, and the "TX OFF" badge in the status strip — so the operator can
// always tell whether their keying will go on the air.
static void updateVailFooter() {
    if (vail_tile_listen_label != NULL) {
        lv_label_set_text(vail_tile_listen_label, vailListenOnly ? "Break-In OFF" : "Break-In ON");
    }
    if (vail_tile_listen_icon != NULL) {
        lv_obj_set_style_text_color(vail_tile_listen_icon,
            vailListenOnly ? LV_COLOR_WARNING : LV_COLOR_ACCENT_PRIMARY, 0);
    }
    if (vail_tx_strip_label != NULL) {
        if (vailListenOnly) {
            lv_label_set_text(vail_tx_strip_label, "Break-in OFF: keying stays local");
            lv_obj_set_style_text_color(vail_tx_strip_label, LV_COLOR_WARNING, 0);
        } else {
            lv_label_set_text(vail_tx_strip_label, "TX ready: paddle sends CW to the room");
            lv_obj_set_style_text_color(vail_tx_strip_label, LV_COLOR_SUCCESS, 0);
        }
    }
    if (vail_listen_badge != NULL) {
        if (vailListenOnly) lv_obj_clear_flag(vail_listen_badge, LV_OBJ_FLAG_HIDDEN);
        else                lv_obj_add_flag(vail_listen_badge, LV_OBJ_FLAG_HIDDEN);
    }
}

// Update settings display labels
static void updateVailSettingsDisplay() {
    if (vail_wpm_label != NULL) {
        char wpm_str[16];
        snprintf(wpm_str, sizeof(wpm_str), "%d WPM", cwSpeed);
        lv_label_set_text(vail_wpm_label, wpm_str);
    }
}

// Status-strip battery/WiFi glyphs (mirrors home-screen strip behaviour).
static void vailApplyStripBattery(lv_obj_t* lbl) {
    if (!lbl) return;
    if (batteryPercent > 80)      { lv_label_set_text(lbl, LV_SYMBOL_BATTERY_FULL);  lv_obj_set_style_text_color(lbl, LV_COLOR_SUCCESS, 0); }
    else if (batteryPercent > 60) { lv_label_set_text(lbl, LV_SYMBOL_BATTERY_3);     lv_obj_set_style_text_color(lbl, LV_COLOR_SUCCESS, 0); }
    else if (batteryPercent > 40) { lv_label_set_text(lbl, LV_SYMBOL_BATTERY_2);     lv_obj_set_style_text_color(lbl, LV_COLOR_ACCENT_PRIMARY, 0); }
    else if (batteryPercent > 20) { lv_label_set_text(lbl, LV_SYMBOL_BATTERY_1);     lv_obj_set_style_text_color(lbl, LV_COLOR_WARNING, 0); }
    else                          { lv_label_set_text(lbl, LV_SYMBOL_BATTERY_EMPTY); lv_obj_set_style_text_color(lbl, LV_COLOR_ERROR, 0); }
}

static void vailApplyStripWifi(lv_obj_t* lbl) {
    if (!lbl) return;
    InternetStatus s = getInternetStatus();
    if (s == INET_CONNECTED || s == INET_CHECKING) lv_obj_set_style_text_color(lbl, LV_COLOR_SUCCESS, 0);
    else if (s == INET_WIFI_ONLY)                  lv_obj_set_style_text_color(lbl, LV_COLOR_WARNING, 0);
    else                                           lv_obj_set_style_text_color(lbl, LV_COLOR_ERROR, 0);
}

// One bottom-row hint tile: keycap letter over action label, styled like the
// home dashboard's launcher tiles. Purely visual — actions fire via the
// single-key hotkeys handled in vail_key_event_cb, not via focus/click.
static lv_obj_t* vailMakeHintTile(lv_obj_t* parent, const char* keycap, const char* name,
                                  int x, int y, int w, int h,
                                  lv_obj_t** keycap_out, lv_obj_t** name_out) {
    lv_obj_t* tile = lv_obj_create(parent);
    lv_obj_set_size(tile, w, h);
    lv_obj_set_pos(tile, x, y);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    applyMenuCardStyle(tile);
    lv_obj_set_style_pad_all(tile, 4, 0);

    lv_obj_t* kc = lv_label_create(tile);
    lv_label_set_text(kc, keycap);
    lv_obj_set_style_text_font(kc, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(kc, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(kc, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t* nm = lv_label_create(tile);
    lv_label_set_text(nm, name);
    lv_obj_set_style_text_font(nm, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(nm, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(nm, LV_ALIGN_BOTTOM_MID, 0, -2);

    if (keycap_out) *keycap_out = kc;
    if (name_out)   *name_out = nm;
    return tile;
}

// (updateLandingButtonFocus removed — landing screen retired in favor of the
//  always-visible operating view.)

// Refresh all settings row value labels from current globals.
// Settings: Speed (WPM), Tone (Hz), Key Type.
// (RX decoded row is no longer a setting — it's automatic on the Decoder room.)
static void refreshVailSettingsValues() {
    if (vail_srow_values[0] == NULL) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d WPM", cwSpeed);
    lv_label_set_text(vail_srow_values[0], buf);
    snprintf(buf, sizeof(buf), "%d Hz", cwTone);
    lv_label_set_text(vail_srow_values[1], buf);
    lv_label_set_text(vail_srow_values[2], vail_keytype_names[cwKeyType]);
}

// Highlight the focused settings row
static void refreshVailSettingsFocus() {
    for (int i = 0; i < VAIL_SETTINGS_ROW_COUNT; i++) {
        if (vail_srow_containers[i] == NULL) continue;
        bool sel = (i == vail_settings_focus);
        lv_obj_set_style_bg_color(vail_srow_containers[i],
            sel ? LV_COLOR_BG_LAYER2 : LV_COLOR_BG_DEEP, 0);
        if (vail_srow_values[i] != NULL) {
            lv_obj_set_style_text_color(vail_srow_values[i],
                sel ? LV_COLOR_ACCENT_PRIMARY : LV_COLOR_TEXT_SECONDARY, 0);
        }
    }
}

// Adjust the focused settings row by delta (+1 or -1)
static void adjustVailSettingsRow(int delta) {
    switch (vail_settings_focus) {
        case 0:
            cwSpeed = constrain(cwSpeed + delta, 5, 40);
            if (vailKeyer) vailKeyer->setDitDuration(DIT_DURATION(cwSpeed));
            markDeferredSave(saveCWSettings);
            break;
        case 1: cwTone = constrain(cwTone + delta * 50, 400, 1200); markDeferredSave(saveCWSettings); break;
        case 2:
            cwKeyType = (KeyType)((cwKeyType + delta + 4) % 4);
            vailKeyer = getKeyer(cwKeyType);
            vailKeyer->reset();
            vailKeyer->setDitDuration(DIT_DURATION(cwSpeed));
            vailKeyer->setTxCallback(vailKeyerCallback);
            markDeferredSave(saveCWSettings);
            break;    }
    refreshVailSettingsValues();
    updateVailSettingsDisplay();
}

// Show settings modal for adjusting speed, tone, or key type
static void showVailSettingsModal(int type) {
    // type: 1=speed, 2=tone, 3=keytype
    vail_settings_modal_type = type;

    // Set initial temp value
    switch (type) {
        case 1: vail_settings_temp_value = cwSpeed; break;
        case 2: vail_settings_temp_value = cwTone; break;
        case 3: vail_settings_temp_value = cwKeyType; break;
    }

    // Create modal if not exists
    if (vail_settings_modal == NULL) {
        vail_settings_modal = lv_obj_create(vail_screen);
        lv_obj_set_size(vail_settings_modal, 280, 140);
        lv_obj_center(vail_settings_modal);
        lv_obj_set_style_bg_color(vail_settings_modal, LV_COLOR_BG_LAYER2, 0);
        lv_obj_set_style_border_color(vail_settings_modal, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_set_style_border_width(vail_settings_modal, 2, 0);
        lv_obj_set_style_radius(vail_settings_modal, 8, 0);
        lv_obj_set_style_pad_all(vail_settings_modal, 15, 0);
        lv_obj_clear_flag(vail_settings_modal, LV_OBJ_FLAG_SCROLLABLE);

        // Title
        vail_settings_title_label = lv_label_create(vail_settings_modal);
        lv_obj_set_style_text_font(vail_settings_title_label, getThemeFonts()->font_title, 0);
        lv_obj_set_style_text_color(vail_settings_title_label, LV_COLOR_TEXT_PRIMARY, 0);
        lv_obj_align(vail_settings_title_label, LV_ALIGN_TOP_MID, 0, 0);

        // Value
        vail_settings_value_label = lv_label_create(vail_settings_modal);
        lv_obj_set_style_text_font(vail_settings_value_label, getThemeFonts()->font_title, 0);
        lv_obj_set_style_text_color(vail_settings_value_label, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_align(vail_settings_value_label, LV_ALIGN_CENTER, 0, 0);

        // Help text
        lv_obj_t* help = lv_label_create(vail_settings_modal);
        lv_label_set_text(help, "L/R Setting   UP/DN Adjust   ENTER Save   ESC Cancel");
        lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
        lv_obj_set_style_text_color(help, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    // Set title and value based on type
    const char* title = "";
    char value_str[32];

    switch (type) {
        case 1:
            title = "SPEED";
            snprintf(value_str, sizeof(value_str), "%d WPM", vail_settings_temp_value);
            break;
        case 2:
            title = "TONE";
            snprintf(value_str, sizeof(value_str), "%d Hz", vail_settings_temp_value);
            break;
        case 3:
            title = "KEY TYPE";
            snprintf(value_str, sizeof(value_str), "%s", vail_keytype_names[vail_settings_temp_value]);
            break;    }

    lv_label_set_text(vail_settings_title_label, title);
    lv_label_set_text(vail_settings_value_label, value_str);

    lv_obj_clear_flag(vail_settings_modal, LV_OBJ_FLAG_HIDDEN);
    vail_view_mode = 4 + type;  // 5=speed, 6=tone, 7=keytype
}

// Hide settings modal
static void hideVailSettingsModal() {
    if (vail_settings_modal != NULL) {
        lv_obj_add_flag(vail_settings_modal, LV_OBJ_FLAG_HIDDEN);
    }
    vail_settings_modal_type = 0;
    vail_view_mode = vail_current_view;  // Return to current view (0 or 1)
}

// Adjust current setting value
static void adjustVailSetting(int delta) {
    char value_str[32];

    switch (vail_settings_modal_type) {
        case 1:  // Speed
            vail_settings_temp_value = constrain(vail_settings_temp_value + delta, 5, 40);
            snprintf(value_str, sizeof(value_str), "%d WPM", vail_settings_temp_value);
            break;
        case 2:  // Tone
            vail_settings_temp_value = constrain(vail_settings_temp_value + delta * 50, 400, 1200);
            snprintf(value_str, sizeof(value_str), "%d Hz", vail_settings_temp_value);
            break;
        case 3:  // Key type
            vail_settings_temp_value = (vail_settings_temp_value + delta + 4) % 4;
            snprintf(value_str, sizeof(value_str), "%s", vail_keytype_names[vail_settings_temp_value]);
            break;    }

    if (vail_settings_value_label != NULL) {
        lv_label_set_text(vail_settings_value_label, value_str);
    }
}

// Confirm and apply setting
static void confirmVailSetting() {
    switch (vail_settings_modal_type) {
        case 1: cwSpeed = vail_settings_temp_value; markDeferredSave(saveCWSettings); break;
        case 2: cwTone = vail_settings_temp_value; markDeferredSave(saveCWSettings); break;
        case 3: cwKeyType = (KeyType)vail_settings_temp_value; markDeferredSave(saveCWSettings); break;    }
    hideVailSettingsModal();
    updateVailSettingsDisplay();
    beep(TONE_SUCCESS, BEEP_SHORT);
}

// Key event callback for Vail Repeater keyboard input
static void vail_key_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);
    Serial.printf("[Vail LVGL] Key event: %lu (0x%02lX), view_mode: %d, current_view: %d\n", key, key, vail_view_mode, vail_current_view);

    // Handle based on current view mode
    switch (vail_view_mode) {
        case 1: // Operating view — chat panel default; hotkeys for sub-views
            switch(key) {
                case LV_KEY_ESC:
                    // ESC exits Vail Repeater entirely (paddle keying continues
                    // to work everywhere else; this is the explicit "leave the
                    // room" action).
                    disconnectFromVail();
                    onLVGLBackNavigation();
                    lv_event_stop_processing(e);
                    break;
                case LV_KEY_UP:
                case LV_KEY_PREV:
                    if (vail_chat_textarea) {
                        lv_obj_scroll_by(vail_chat_textarea, 0, 30, LV_ANIM_OFF);
                    }
                    break;
                case LV_KEY_DOWN:
                case LV_KEY_NEXT:
                    if (vail_chat_textarea) {
                        lv_obj_scroll_by(vail_chat_textarea, 0, -30, LV_ANIM_OFF);
                    }
                    break;
                case 'c': case 'C':
                    // Open chat compose modal (text-only, sent as Text payload)
                    showVailChatInputOverlay();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    break;
                case 'r': case 'R':
                    // Open room picker overlay
                    showVailRoomOverlay();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    break;
                case 's': case 'S':
                    // Open settings sub-view
                    switchVailView(2);
                    refreshVailSettingsFocus();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    break;
                case 'l': case 'L':
                    // Toggle Listen-Only mode (block outgoing TX, paddle still
                    // produces local sidetone)
                    vailListenOnly = !vailListenOnly;
                    markDeferredSave(saveVailSettings);
                    updateVailFooter();
                    beep(vailListenOnly ? TONE_ERROR : TONE_SUCCESS, BEEP_SHORT);
                    break;
                // Other keys ignored — paddle keying always works in background
                // regardless of which letter the operator presses.
            }
            break;

        case 9: // Settings sub-view
            switch(key) {
                case LV_KEY_ESC:
                    switchVailView(1);  // Back to operating view
                    lv_event_stop_processing(e);
                    break;
                case LV_KEY_UP:
                case LV_KEY_PREV:
                    vail_settings_focus = (vail_settings_focus + VAIL_SETTINGS_ROW_COUNT - 1) % VAIL_SETTINGS_ROW_COUNT;
                    refreshVailSettingsFocus();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    break;
                case LV_KEY_DOWN:
                case LV_KEY_NEXT:
                    vail_settings_focus = (vail_settings_focus + 1) % VAIL_SETTINGS_ROW_COUNT;
                    refreshVailSettingsFocus();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    break;
                case LV_KEY_LEFT:
                    adjustVailSettingsRow(-1);
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    break;
                case LV_KEY_RIGHT:
                case LV_KEY_ENTER:
                    adjustVailSettingsRow(1);
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    break;
            }
            break;

        case 10: // Chat compose modal — typing into vail_chat_input_text
            switch(key) {
                case LV_KEY_ESC:
                    vail_chat_input_text = "";
                    hideVailChatInputOverlay();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    lv_event_stop_processing(e);
                    break;
                case LV_KEY_ENTER:
                    if (vail_chat_input_text.length() > 0) {
                        addChatMessage(vailCallsign, vail_chat_input_text);
                        sendChatMessage(vail_chat_input_text);
                        vail_chat_input_text = "";
                        beep(TONE_SUCCESS, BEEP_SHORT);
                    }
                    hideVailChatInputOverlay();
                    break;
                case LV_KEY_BACKSPACE:
                    if (vail_chat_input_text.length() > 0) {
                        vail_chat_input_text.remove(vail_chat_input_text.length() - 1);
                        if (vail_chat_input_textarea) {
                            lv_textarea_set_text(vail_chat_input_textarea, vail_chat_input_text.c_str());
                        }
                    }
                    break;
                default:
                    if (key >= 32 && key < 127) {
                        if ((int)vail_chat_input_text.length() < VAIL_MAX_INPUT_LEN) {
                            vail_chat_input_text += (char)key;
                            if (vail_chat_input_textarea) {
                                lv_textarea_set_text(vail_chat_input_textarea, vail_chat_input_text.c_str());
                            }
                        } else {
                            beep(TONE_ERROR, BEEP_SHORT);
                        }
                    }
                    break;
            }
            break;

        case 2: // Room selection overlay
            switch(key) {
                case LV_KEY_ESC:
                    if (vail_custom_room_mode) {
                        // Exit custom room input, back to room list
                        vail_custom_room_mode = false;
                        roomInput = "";
                        if (vail_room_input_textarea) {
                            lv_obj_add_flag(vail_room_input_textarea, LV_OBJ_FLAG_HIDDEN);
                        }
                        if (vail_room_list) {
                            lv_obj_clear_flag(vail_room_list, LV_OBJ_FLAG_HIDDEN);
                        }
                    } else {
                        // Return to main info panel, not exit mode
                        hideVailRoomOverlay();
                    }
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    lv_event_stop_processing(e);  // Prevent ESC from propagating to exit mode
                    break;
                case LV_KEY_UP:
                case LV_KEY_PREV:
                    if (!vail_custom_room_mode && vail_room_selection > 0) {
                        vail_room_selection--;
                        updateVailRoomList();
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                    lv_event_stop_processing(e);
                    break;
                case LV_KEY_DOWN:
                case LV_KEY_NEXT:
                    if (!vail_custom_room_mode) {
                        int maxSelection = vailRoomListCount(); // index of "Custom room..."
                        if (vail_room_selection < maxSelection) {
                            vail_room_selection++;
                            updateVailRoomList();
                            beep(TONE_MENU_NAV, BEEP_SHORT);
                        }
                    }
                    lv_event_stop_processing(e);
                    break;
                case LV_KEY_ENTER:
                    if (vail_custom_room_mode) {
                        // Connect to custom room
                        if (roomInput.length() > 0) {
                            hideVailRoomOverlay();
                            // Disconnect now, reconnect after 250ms settle
                            // (deferred via updateVailScreenLVGL, no blocking)
                            disconnectFromVail();
                            strlcpy(vail_pending_room, roomInput.c_str(), sizeof(vail_pending_room));
                            vail_reconnect_at_ms = millis() + 250;
                            roomInput = "";
                            beep(TONE_SUCCESS, BEEP_SHORT);
                        }
                    } else if (vail_room_selection == vailRoomListCount()) {
                        // Selected "Custom room..." option
                        vail_custom_room_mode = true;
                        roomInput = "";
                        if (vail_room_list) {
                            lv_obj_add_flag(vail_room_list, LV_OBJ_FLAG_HIDDEN);
                        }
                        if (vail_room_input_textarea) {
                            lv_obj_clear_flag(vail_room_input_textarea, LV_OBJ_FLAG_HIDDEN);
                            lv_textarea_set_text(vail_room_input_textarea, "");
                        }
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    } else if (vail_room_selection < vailRoomListCount()) {
                        // Connect to selected room. Copy the name BEFORE
                        // disconnecting: disconnectFromVail() clears
                        // activeRooms, so reading the entry afterwards is a
                        // use-after-free (crashed on every list-pick join).
                        strlcpy(vail_pending_room, vailRoomNameAt(vail_room_selection),
                                sizeof(vail_pending_room));
                        hideVailRoomOverlay();
                        // Disconnect now, reconnect after 250ms settle
                        // (deferred via updateVailScreenLVGL, no blocking)
                        disconnectFromVail();
                        vail_reconnect_at_ms = millis() + 250;
                        beep(TONE_SUCCESS, BEEP_SHORT);
                    }
                    lv_event_stop_processing(e);
                    break;
                default:
                    // Handle text input for custom room
                    if (vail_custom_room_mode) {
                        if (key == LV_KEY_BACKSPACE) {
                            if (roomInput.length() > 0) {
                                roomInput.remove(roomInput.length() - 1);
                                if (vail_room_input_textarea) {
                                    lv_textarea_set_text(vail_room_input_textarea, roomInput.c_str());
                                }
                            }
                        } else if (key >= 32 && key < 127 && roomInput.length() < 30) {
                            roomInput += (char)key;
                            if (vail_room_input_textarea) {
                                lv_textarea_set_text(vail_room_input_textarea, roomInput.c_str());
                            }
                        }
                        lv_event_stop_processing(e);
                    }
                    break;
            }
            break;

        case 3: // User list overlay
            switch(key) {
                case LV_KEY_ESC:
                    // Return to main info panel, not exit mode
                    hideVailUserListOverlay();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    lv_event_stop_processing(e);  // Prevent ESC from propagating to exit mode
                    break;
                case LV_KEY_UP:
                case LV_KEY_PREV:
                    if (vail_user_scroll > 0) {
                        vail_user_scroll--;
                        updateVailUserList();
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                    lv_event_stop_processing(e);
                    break;
                case LV_KEY_DOWN:
                case LV_KEY_NEXT:
                    if (vail_user_scroll < (int)connectedUsers.size() - 1) {
                        vail_user_scroll++;
                        updateVailUserList();
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                    lv_event_stop_processing(e);
                    break;
            }
            break;

        case 4: // Callsign required overlay - only ESC to exit
            switch(key) {
                case LV_KEY_ESC:
                    // Exit back to menu - don't connect without callsign
                    onLVGLBackNavigation();
                    lv_event_stop_processing(e);
                    break;
                default:
                    // Ignore all other keys - user must exit and set callsign first
                    break;
            }
            break;

        case 5:  // Speed modal
        case 6:  // Tone modal
        case 7:  // Key type modal
        case 8:  // Morse row modal
            switch(key) {
                case LV_KEY_ESC:
                    hideVailSettingsModal();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    lv_event_stop_processing(e);
                    break;
                case LV_KEY_ENTER:
                    confirmVailSetting();
                    lv_event_stop_processing(e);
                    break;
                case LV_KEY_UP:
                case LV_KEY_PREV:
                    adjustVailSetting(1);
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    lv_event_stop_processing(e);
                    break;
                case LV_KEY_DOWN:
                case LV_KEY_NEXT:
                    adjustVailSetting(-1);
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    lv_event_stop_processing(e);
                    break;
                case LV_KEY_LEFT:
                    // Previous setting (cycle: 1→4→3→2→1)
                    confirmVailSetting();
                    showVailSettingsModal((vail_settings_modal_type - 2 + 4) % 4 + 1);
                    lv_event_stop_processing(e);
                    break;
                case LV_KEY_RIGHT:
                    // Next setting (cycle: 1→2→3→4→1)
                    confirmVailSetting();
                    showVailSettingsModal(vail_settings_modal_type % 4 + 1);
                    lv_event_stop_processing(e);
                    break;
            }
            break;
    }
}

// Create overlay for room selection
static void showVailRoomOverlay() {
    if (vail_room_overlay != NULL) {
        lv_obj_clear_flag(vail_room_overlay, LV_OBJ_FLAG_HIDDEN);
        vail_view_mode = 2;  // Room selection overlay
        vail_room_selection = 0;
        vail_custom_room_mode = false;
        updateVailRoomList();
        return;
    }

    // Create overlay background
    vail_room_overlay = lv_obj_create(vail_screen);
    lv_obj_set_size(vail_room_overlay, 360, 220);
    lv_obj_center(vail_room_overlay);
    lv_obj_set_style_bg_color(vail_room_overlay, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_color(vail_room_overlay, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(vail_room_overlay, 2, 0);
    lv_obj_set_style_radius(vail_room_overlay, 8, 0);
    lv_obj_set_style_pad_all(vail_room_overlay, 10, 0);
    lv_obj_clear_flag(vail_room_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(vail_room_overlay);
    lv_label_set_text(title, "SELECT ROOM");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_title, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // Room list container
    vail_room_list = lv_obj_create(vail_room_overlay);
    lv_obj_set_size(vail_room_list, 340, 140);
    lv_obj_align(vail_room_list, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(vail_room_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vail_room_list, 0, 0);
    lv_obj_set_style_pad_all(vail_room_list, 5, 0);
    lv_obj_set_flex_flow(vail_room_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(vail_room_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Custom room input textarea (initially hidden)
    vail_room_input_textarea = lv_textarea_create(vail_room_overlay);
    lv_obj_set_size(vail_room_input_textarea, 300, 40);
    lv_obj_align(vail_room_input_textarea, LV_ALIGN_TOP_MID, 0, 80);
    lv_textarea_set_placeholder_text(vail_room_input_textarea, "Enter room name...");
    lv_textarea_set_one_line(vail_room_input_textarea, true);
    applyTextareaStyle(vail_room_input_textarea);
    lv_obj_add_flag(vail_room_input_textarea, LV_OBJ_FLAG_HIDDEN);

    // Footer help text
    lv_obj_t* help = lv_label_create(vail_room_overlay);
    lv_label_set_text(help, FOOTER_NAV_ENTER_ESC);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, -5);

    vail_view_mode = 2;  // Room selection overlay
    vail_room_selection = 0;
    updateVailRoomList();
}

static void hideVailRoomOverlay() {
    if (vail_room_overlay != NULL) {
        lv_obj_add_flag(vail_room_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    vail_view_mode = vail_current_view;  // Return to current view (0 or 1)
    vail_custom_room_mode = false;
}

static void updateVailRoomList() {
    if (vail_room_list == NULL) return;

    // Clear existing items
    lv_obj_clean(vail_room_list);

    // Add room items (server rooms + pinned Decoder when absent)
    int room_count = vailRoomListCount();
    for (int i = 0; i < room_count && i < 6; i++) {
        lv_obj_t* item = lv_label_create(vail_room_list);
        char item_text[64];
        if (i < (int)activeRooms.size()) {
            snprintf(item_text, sizeof(item_text), "%s %s (%d users)",
                     (i == vail_room_selection) ? ">" : " ",
                     activeRooms[i].name.c_str(),
                     activeRooms[i].users);
        } else {
            snprintf(item_text, sizeof(item_text), "%s Decoder",
                     (i == vail_room_selection) ? ">" : " ");
        }
        lv_label_set_text(item, item_text);
        lv_obj_set_style_text_font(item, getThemeFonts()->font_body, 0);
        if (i == vail_room_selection) {
            lv_obj_set_style_text_color(item, LV_COLOR_ACCENT_PRIMARY, 0);
        } else {
            lv_obj_set_style_text_color(item, LV_COLOR_TEXT_PRIMARY, 0);
        }
    }

    // Add "Custom room..." option
    lv_obj_t* custom_item = lv_label_create(vail_room_list);
    char custom_text[32];
    snprintf(custom_text, sizeof(custom_text), "%s Custom room...",
             (vail_room_selection == vailRoomListCount()) ? ">" : " ");
    lv_label_set_text(custom_item, custom_text);
    lv_obj_set_style_text_font(custom_item, getThemeFonts()->font_body, 0);
    if (vail_room_selection == vailRoomListCount()) {
        lv_obj_set_style_text_color(custom_item, LV_COLOR_ACCENT_PRIMARY, 0);
    } else {
        lv_obj_set_style_text_color(custom_item, LV_COLOR_TEXT_SECONDARY, 0);
    }
}

// Create overlay for chat input (text-only chat compose modal opened by C key
// from the operating view).
static void showVailChatInputOverlay() {
    vail_chat_input_text = "";
    if (vail_chat_input_overlay != NULL) {
        lv_obj_clear_flag(vail_chat_input_overlay, LV_OBJ_FLAG_HIDDEN);
        if (vail_chat_input_textarea) {
            lv_textarea_set_text(vail_chat_input_textarea, "");
        }
        chatInput = "";
        vail_view_mode = 10;  // chat compose modal
        return;
    }

    // Create overlay background
    vail_chat_input_overlay = lv_obj_create(vail_screen);
    lv_obj_set_size(vail_chat_input_overlay, 400, 140);
    lv_obj_center(vail_chat_input_overlay);
    lv_obj_set_style_bg_color(vail_chat_input_overlay, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_color(vail_chat_input_overlay, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(vail_chat_input_overlay, 2, 0);
    lv_obj_set_style_radius(vail_chat_input_overlay, 8, 0);
    lv_obj_set_style_pad_all(vail_chat_input_overlay, 15, 0);
    lv_obj_clear_flag(vail_chat_input_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(vail_chat_input_overlay);
    lv_label_set_text(title, "SEND MESSAGE");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_title, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // Chat input textarea
    vail_chat_input_textarea = lv_textarea_create(vail_chat_input_overlay);
    lv_obj_set_size(vail_chat_input_textarea, 360, 45);
    lv_obj_align(vail_chat_input_textarea, LV_ALIGN_TOP_MID, 0, 35);
    lv_textarea_set_placeholder_text(vail_chat_input_textarea, "Type your message...");
    lv_textarea_set_one_line(vail_chat_input_textarea, true);
    lv_textarea_set_max_length(vail_chat_input_textarea, 40);
    applyTextareaStyle(vail_chat_input_textarea);

    // Footer help text
    lv_obj_t* help = lv_label_create(vail_chat_input_overlay);
    lv_label_set_text(help, "Type message   ENTER Send   ESC Cancel");
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, 0);

    chatInput = "";
    vail_view_mode = 10;  // chat compose modal
}

static void hideVailChatInputOverlay() {
    if (vail_chat_input_overlay != NULL) {
        lv_obj_add_flag(vail_chat_input_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    vail_view_mode = vail_current_view;  // Return to current view (0 or 1)
}

// Create overlay for user list
static void showVailUserListOverlay() {
    if (vail_user_list_overlay != NULL) {
        lv_obj_clear_flag(vail_user_list_overlay, LV_OBJ_FLAG_HIDDEN);
        vail_view_mode = 3;
        vail_user_scroll = 0;
        updateVailUserList();
        return;
    }

    // Create overlay background
    vail_user_list_overlay = lv_obj_create(vail_screen);
    lv_obj_set_size(vail_user_list_overlay, 320, 200);
    lv_obj_center(vail_user_list_overlay);
    lv_obj_set_style_bg_color(vail_user_list_overlay, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_color(vail_user_list_overlay, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(vail_user_list_overlay, 2, 0);
    lv_obj_set_style_radius(vail_user_list_overlay, 8, 0);
    lv_obj_set_style_pad_all(vail_user_list_overlay, 10, 0);
    lv_obj_clear_flag(vail_user_list_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(vail_user_list_overlay);
    lv_label_set_text(title, "CONNECTED USERS");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_title, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // User list container
    vail_user_list = lv_obj_create(vail_user_list_overlay);
    lv_obj_set_size(vail_user_list, 300, 120);
    lv_obj_align(vail_user_list, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(vail_user_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vail_user_list, 0, 0);
    lv_obj_set_style_pad_all(vail_user_list, 5, 0);
    lv_obj_set_flex_flow(vail_user_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(vail_user_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Footer help text
    lv_obj_t* help = lv_label_create(vail_user_list_overlay);
    lv_label_set_text(help, "UP/DN Scroll   ESC Back");
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, -5);

    vail_view_mode = 3;
    vail_user_scroll = 0;
    updateVailUserList();
}

static void hideVailUserListOverlay() {
    if (vail_user_list_overlay != NULL) {
        lv_obj_add_flag(vail_user_list_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    vail_view_mode = vail_current_view;  // Return to current view (0 or 1)
}

static void updateVailUserList() {
    if (vail_user_list == NULL) return;

    // Clear existing items
    lv_obj_clean(vail_user_list);

    if (connectedUsers.empty()) {
        lv_obj_t* empty_label = lv_label_create(vail_user_list);
        lv_label_set_text(empty_label, "No users connected");
        lv_obj_set_style_text_color(empty_label, LV_COLOR_TEXT_SECONDARY, 0);
        return;
    }

    // Show up to 5 users starting from scroll position
    for (size_t i = vail_user_scroll; i < connectedUsers.size() && i < (size_t)(vail_user_scroll + 5); i++) {
        lv_obj_t* item = lv_label_create(vail_user_list);
        char item_text[48];
        // Convert MIDI note to approximate Hz for display
        int freq = (int)(440.0 * pow(2.0, (connectedUsers[i].txTone - 69) / 12.0));
        snprintf(item_text, sizeof(item_text), "%s (%d Hz)", connectedUsers[i].callsign.c_str(), freq);
        lv_label_set_text(item, item_text);
        lv_obj_set_style_text_font(item, getThemeFonts()->font_body, 0);
        lv_obj_set_style_text_color(item, LV_COLOR_TEXT_PRIMARY, 0);
    }
}

// Check if callsign is required (returns true if user needs to set callsign)
static bool checkVailCallsignRequired() {
    // Check if callsign is empty or still the default "GUEST"
    return (vailCallsign.length() == 0 || vailCallsign == "GUEST");
}

// Show overlay prompting user to set callsign
static void showVailCallsignRequiredOverlay() {
    if (vail_callsign_overlay != NULL) {
        lv_obj_clear_flag(vail_callsign_overlay, LV_OBJ_FLAG_HIDDEN);
        vail_view_mode = 4;
        return;
    }

    // Create overlay background
    vail_callsign_overlay = lv_obj_create(vail_screen);
    lv_obj_set_size(vail_callsign_overlay, 380, 180);
    lv_obj_center(vail_callsign_overlay);
    lv_obj_set_style_bg_color(vail_callsign_overlay, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_color(vail_callsign_overlay, LV_COLOR_WARNING, 0);
    lv_obj_set_style_border_width(vail_callsign_overlay, 2, 0);
    lv_obj_set_style_radius(vail_callsign_overlay, 8, 0);
    lv_obj_set_style_pad_all(vail_callsign_overlay, 15, 0);
    lv_obj_clear_flag(vail_callsign_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Warning icon and title
    lv_obj_t* title = lv_label_create(vail_callsign_overlay);
    lv_label_set_text(title, LV_SYMBOL_WARNING "  CALLSIGN REQUIRED");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_title, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_WARNING, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // Message
    lv_obj_t* msg1 = lv_label_create(vail_callsign_overlay);
    lv_label_set_text(msg1, "You must set your callsign before");
    lv_obj_set_style_text_font(msg1, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(msg1, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(msg1, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t* msg2 = lv_label_create(vail_callsign_overlay);
    lv_label_set_text(msg2, "using the Vail Repeater.");
    lv_obj_set_style_text_font(msg2, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(msg2, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(msg2, LV_ALIGN_TOP_MID, 0, 60);

    // Instructions
    lv_obj_t* instr = lv_label_create(vail_callsign_overlay);
    lv_label_set_text(instr, "Go to: Settings > General > Callsign");
    lv_obj_set_style_text_font(instr, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(instr, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(instr, LV_ALIGN_TOP_MID, 0, 95);

    // Footer help text
    lv_obj_t* help = lv_label_create(vail_callsign_overlay);
    lv_label_set_text(help, "ESC Back");
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, -5);

    vail_view_mode = 4;
    vail_callsign_required = true;
}

lv_obj_t* createVailRepeaterScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Reset state — default to operating view (1)
    vail_view_mode = 1;
    vail_current_view = 1;
    vail_last_chat_count = 0;
    vail_room_overlay = NULL;
    vail_chat_input_overlay = NULL;
    vail_user_list_overlay = NULL;
    vail_callsign_overlay = NULL;
    vail_callsign_required = false;
    vail_loading_overlay = NULL;
    vail_status_indicator = NULL;
    vail_chat_panel = NULL;
    vail_settings_modal = NULL;
    vail_chat_input_text = "";
    vail_pending_room[0] = '\0';
    vail_reconnect_at_ms = 0;
    lastKnownVailState = VAIL_DISCONNECTED;
    vail_settings_focus = 0;
    vail_settings_panel = NULL;
    for (int i = 0; i < VAIL_SETTINGS_ROW_COUNT; i++) {
        vail_srow_containers[i] = NULL;
        vail_srow_values[i] = NULL;
    }
    vail_decoded_row_bg = NULL;
    vail_decoded_row_label = NULL;
    vail_users_label = NULL;
    vail_side_ops_title = NULL;
    vail_room_label = NULL;
    vail_strip_batt = NULL;
    vail_strip_wifi = NULL;
    vail_listen_badge = NULL;
    vail_onair_pill = NULL;
    vail_tx_strip_label = NULL;
    vail_tile_listen_icon = NULL;
    vail_tile_listen_label = NULL;

    // ---- Status strip (matches the home dashboard) ----
    // lamp * VAIL REPEATER * room ... [TX OFF] [WPM] [WiFi] [Battery]
    lv_obj_t* strip = lv_obj_create(screen);
    lv_obj_set_size(strip, SCREEN_WIDTH, 30);
    lv_obj_set_pos(strip, 0, 0);
    lv_obj_set_style_bg_color(strip, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(strip, 0, 0);
    lv_obj_set_style_radius(strip, 0, 0);
    lv_obj_set_style_pad_all(strip, 0, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

    // Left cluster: receiver lamp + title + current room (transparent flex row)
    lv_obj_t* strip_left = lv_obj_create(strip);
    lv_obj_set_size(strip_left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(strip_left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(strip_left, 0, 0);
    lv_obj_set_style_pad_all(strip_left, 0, 0);
    lv_obj_set_style_pad_gap(strip_left, 8, 0);
    lv_obj_set_layout(strip_left, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(strip_left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(strip_left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(strip_left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(strip_left, LV_ALIGN_LEFT_MID, 12, 0);

    // Receiver lamp dot
    vail_status_indicator = lv_obj_create(strip_left);
    lv_obj_set_size(vail_status_indicator, 12, 12);
    lv_obj_set_style_radius(vail_status_indicator, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(vail_status_indicator, LV_COLOR_WARNING, 0);
    lv_obj_set_style_bg_opa(vail_status_indicator, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vail_status_indicator, 0, 0);
    lv_obj_clear_flag(vail_status_indicator, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(strip_left);
    lv_label_set_text(title, "VAIL REPEATER");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);

    // Current room name (always glanceable in the strip)
    vail_room_label = lv_label_create(strip_left);
    lv_label_set_text(vail_room_label, vailChannel.c_str());
    lv_obj_set_style_text_color(vail_room_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(vail_room_label, getThemeFonts()->font_small, 0);

    // Right cluster: battery, WiFi, WPM, listen-only badge (absolute, like home)
    vail_strip_batt = lv_label_create(strip);
    lv_obj_set_style_text_font(vail_strip_batt, &lv_font_montserrat_20, 0);
    lv_obj_align(vail_strip_batt, LV_ALIGN_RIGHT_MID, -10, 0);
    vailApplyStripBattery(vail_strip_batt);

    vail_strip_wifi = lv_label_create(strip);
    lv_label_set_text(vail_strip_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(vail_strip_wifi, &lv_font_montserrat_20, 0);
    lv_obj_align(vail_strip_wifi, LV_ALIGN_RIGHT_MID, -44, 0);
    vailApplyStripWifi(vail_strip_wifi);

    vail_wpm_label = lv_label_create(strip);
    lv_obj_set_style_text_font(vail_wpm_label, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(vail_wpm_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(vail_wpm_label, LV_ALIGN_RIGHT_MID, -78, 0);
    updateVailSettingsDisplay();

    vail_listen_badge = lv_label_create(strip);
    lv_label_set_text(vail_listen_badge, "TX OFF");
    lv_obj_set_style_text_font(vail_listen_badge, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(vail_listen_badge, LV_COLOR_WARNING, 0);
    lv_obj_align(vail_listen_badge, LV_ALIGN_RIGHT_MID, -136, 0);
    lv_obj_add_flag(vail_listen_badge, LV_OBJ_FLAG_HIDDEN);

    vail_status_label = NULL;

    // Main content area: everything below the strip (panes + tile row)
    int content_top = 38;
    int content_height = SCREEN_HEIGHT - content_top;

    // ============================================
    // OPERATING VIEW (View 1) - Always-visible default
    // Dashboard layout (mirrors the home screen):
    //   - Hero card (left):  "ON THE AIR" kicker, chat/activity history,
    //                        decoded-text strip (Decoder room only)
    //   - Side card (right): operator list ("OPERATORS (n)")
    //   - Tile row (bottom): hotkey hint tiles (C/R/S/L/ESC)
    // ============================================
    vail_chat_panel = lv_obj_create(screen);
    lv_obj_set_size(vail_chat_panel, SCREEN_WIDTH, content_height);
    lv_obj_set_pos(vail_chat_panel, 0, content_top);
    lv_obj_set_style_bg_opa(vail_chat_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vail_chat_panel, 0, 0);
    lv_obj_set_style_pad_all(vail_chat_panel, 0, 0);
    lv_obj_clear_flag(vail_chat_panel, LV_OBJ_FLAG_SCROLLABLE);
    // Chat panel is the default visible view — no add_flag(HIDDEN) here.

    // ---- Hero card: text chat ----
    const int paneH = 160;
    lv_obj_t* hero = lv_obj_create(vail_chat_panel);
    lv_obj_set_size(hero, 286, paneH);
    lv_obj_set_pos(hero, 8, 0);
    lv_obj_clear_flag(hero, LV_OBJ_FLAG_SCROLLABLE);
    applyCardStyle(hero);
    lv_obj_set_style_pad_all(hero, 14, 0);

    lv_obj_t* hero_kicker = lv_label_create(hero);
    lv_label_set_text(hero_kicker, "TEXT CHAT");
    lv_obj_set_style_text_font(hero_kicker, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(hero_kicker, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(hero_kicker, LV_ALIGN_TOP_LEFT, 0, 0);

    // Chat/activity history fills the card; shrinks when the decoded-text
    // strip is visible (Decoder room). Borderless so it blends into the card.
    bool onDecoder = vailIsOnDecoderChannel();
    vail_chat_textarea = lv_textarea_create(hero);
    lv_obj_set_size(vail_chat_textarea, 258, onDecoder ? VAIL_CHAT_H_DECODER : VAIL_CHAT_H_FULL);
    lv_obj_align(vail_chat_textarea, LV_ALIGN_TOP_LEFT, 0, 22);
    lv_textarea_set_text(vail_chat_textarea, "");
    lv_textarea_set_placeholder_text(vail_chat_textarea, "Messages appear here...");
    lv_obj_set_style_bg_opa(vail_chat_textarea, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vail_chat_textarea, 0, 0);
    lv_obj_set_style_pad_all(vail_chat_textarea, 0, 0);
    lv_obj_set_style_text_font(vail_chat_textarea, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(vail_chat_textarea, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_clear_flag(vail_chat_textarea, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_textarea_set_cursor_click_pos(vail_chat_textarea, false);

    // Decoded TEXT strip pinned to the bottom of the hero card: a single
    // stream of decoded characters — both the operator's own keying and
    // received morse. Visible ONLY on the dedicated "Decoder" room (matches
    // vailmorse.com behavior). Accent border + label so it reads as a
    // first-class element, not background trim.
    vail_decoded_row_bg = lv_obj_create(hero);
    lv_obj_set_size(vail_decoded_row_bg, 258, 42);
    lv_obj_align(vail_decoded_row_bg, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_pad_all(vail_decoded_row_bg, 0, 0);
    lv_obj_set_style_bg_color(vail_decoded_row_bg, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_color(vail_decoded_row_bg, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(vail_decoded_row_bg, 1, 0);
    lv_obj_set_style_radius(vail_decoded_row_bg, 4, 0);
    lv_obj_clear_flag(vail_decoded_row_bg, LV_OBJ_FLAG_SCROLLABLE);
    if (!onDecoder) lv_obj_add_flag(vail_decoded_row_bg, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* decoded_kicker = lv_label_create(vail_decoded_row_bg);
    lv_label_set_text(decoded_kicker, "DECODED");
    lv_obj_set_style_text_font(decoded_kicker, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(decoded_kicker, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(decoded_kicker, LV_ALIGN_TOP_LEFT, 6, 1);

    vail_decoded_row_label = lv_label_create(vail_decoded_row_bg);
    lv_label_set_text(vail_decoded_row_label, "");
    lv_obj_set_style_text_font(vail_decoded_row_label, &font_special_elite_18, 0);
    lv_obj_set_style_text_color(vail_decoded_row_label, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(vail_decoded_row_label, LV_ALIGN_BOTTOM_LEFT, 6, -2);

    // ---- Side card: operators in the room ----
    lv_obj_t* side = lv_obj_create(vail_chat_panel);
    lv_obj_set_size(side, 170, paneH);
    lv_obj_set_pos(side, 302, 0);
    lv_obj_clear_flag(side, LV_OBJ_FLAG_SCROLLABLE);
    applyCardStyle(side);
    lv_obj_set_style_pad_all(side, 12, 0);

    vail_side_ops_title = lv_label_create(side);
    lv_label_set_text(vail_side_ops_title, "OPERATORS");
    lv_obj_set_style_text_font(vail_side_ops_title, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(vail_side_ops_title, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(vail_side_ops_title, LV_ALIGN_TOP_LEFT, 0, 0);

    vail_users_label = lv_label_create(side);
    lv_obj_set_size(vail_users_label, 146, 106);
    lv_obj_align(vail_users_label, LV_ALIGN_TOP_LEFT, 0, 26);
    lv_label_set_text(vail_users_label, "");
    lv_label_set_long_mode(vail_users_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(vail_users_label, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(vail_users_label, getThemeFonts()->font_body, 0);

    // ---- TX strip: between the panes and the tile row, on every room ----
    // Tells the operator whether keying goes on the air (break-in state) and
    // hosts the red "ON AIR" pill that lights while transmitting. Text and
    // colors are driven by updateVailFooter(); the pill by updateVailScreenLVGL.
    {
        lv_obj_t* tx_strip = lv_obj_create(vail_chat_panel);
        lv_obj_set_size(tx_strip, SCREEN_WIDTH - 16, 32);
        lv_obj_set_pos(tx_strip, 8, paneH + 6);
        lv_obj_set_style_bg_color(tx_strip, LV_COLOR_BG_LAYER2, 0);
        lv_obj_set_style_bg_opa(tx_strip, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(tx_strip, 0, 0);
        lv_obj_set_style_radius(tx_strip, 6, 0);
        lv_obj_set_style_pad_all(tx_strip, 0, 0);
        lv_obj_clear_flag(tx_strip, LV_OBJ_FLAG_SCROLLABLE);

        vail_tx_strip_label = lv_label_create(tx_strip);
        lv_obj_set_style_text_font(vail_tx_strip_label, getThemeFonts()->font_small, 0);
        lv_obj_align(vail_tx_strip_label, LV_ALIGN_LEFT_MID, 12, 0);

        // "ON AIR" pill — lit bright red while the operator's keying is going
        // out on the network. Visibility is driven from updateVailScreenLVGL
        // with a short hold so it stays solid across inter-element gaps
        // instead of flickering per dit.
        vail_onair_pill = lv_obj_create(tx_strip);
        lv_obj_set_size(vail_onair_pill, 78, 24);
        lv_obj_align(vail_onair_pill, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_set_style_bg_color(vail_onair_pill, LV_COLOR_ERROR, 0);
        lv_obj_set_style_bg_opa(vail_onair_pill, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(vail_onair_pill, 0, 0);
        lv_obj_set_style_radius(vail_onair_pill, 12, 0);
        lv_obj_set_style_pad_all(vail_onair_pill, 0, 0);
        lv_obj_clear_flag(vail_onair_pill, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(vail_onair_pill, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t* onair_lbl = lv_label_create(vail_onair_pill);
        lv_label_set_text(onair_lbl, "ON AIR");
        lv_obj_set_style_text_font(onair_lbl, getThemeFonts()->font_small, 0);
        lv_obj_set_style_text_color(onair_lbl, lv_color_white(), 0);
        lv_obj_center(onair_lbl);
    }

    // ---- Bottom tile row: hotkey hints styled like home launchers ----
    {
        const int tileY = paneH + 44;  // panel-relative; screen y = 242
        const int tileH = 70;
        const int gap   = 6;
        const int tileW = (SCREEN_WIDTH - (gap * 6)) / 5;
        int tx = gap;
        vailMakeHintTile(vail_chat_panel, "C",   "Chat",     tx, tileY, tileW, tileH, NULL, NULL); tx += tileW + gap;
        vailMakeHintTile(vail_chat_panel, "R",   "Rooms",    tx, tileY, tileW, tileH, NULL, NULL); tx += tileW + gap;
        vailMakeHintTile(vail_chat_panel, "S",   "Settings", tx, tileY, tileW, tileH, NULL, NULL); tx += tileW + gap;
        vailMakeHintTile(vail_chat_panel, "L",   "Break-In", tx, tileY, tileW, tileH,
                         &vail_tile_listen_icon, &vail_tile_listen_label);                         tx += tileW + gap;
        vailMakeHintTile(vail_chat_panel, "ESC", "Exit",     tx, tileY, tileW, tileH, NULL, NULL);
    }

    // (Chat compose input is an on-demand modal opened with C, not an
    // always-visible row. See showVailChatInputOverlay().)

    // ============================================
    // SETTINGS PANEL (View 2) - Initially hidden
    // ============================================
    vail_settings_panel = lv_obj_create(screen);
    lv_obj_set_size(vail_settings_panel, SCREEN_WIDTH, content_height);
    lv_obj_set_pos(vail_settings_panel, 0, content_top);
    lv_obj_set_style_bg_opa(vail_settings_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vail_settings_panel, 0, 0);
    lv_obj_set_style_pad_all(vail_settings_panel, 0, 0);
    lv_obj_clear_flag(vail_settings_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(vail_settings_panel, LV_OBJ_FLAG_HIDDEN);

    static const char* srow_names[VAIL_SETTINGS_ROW_COUNT] = {"Speed", "Tone", "Key Type"};
    const int settings_hint_h = 26;
    int row_h = (content_height - settings_hint_h) / VAIL_SETTINGS_ROW_COUNT;

    // Hint footer inside the settings panel (the operating view's hints live
    // on the bottom tiles, which this panel covers)
    lv_obj_t* settings_hint = lv_label_create(vail_settings_panel);
    lv_label_set_text(settings_hint, FOOTER_NAV_ADJUST_ESC);
    lv_obj_set_style_text_font(settings_hint, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(settings_hint, LV_COLOR_WARNING, 0);
    lv_obj_align(settings_hint, LV_ALIGN_BOTTOM_MID, 0, -4);

    for (int i = 0; i < VAIL_SETTINGS_ROW_COUNT; i++) {
        vail_srow_containers[i] = lv_obj_create(vail_settings_panel);
        lv_obj_set_size(vail_srow_containers[i], SCREEN_WIDTH, row_h);
        lv_obj_set_pos(vail_srow_containers[i], 0, i * row_h);
        lv_obj_set_style_bg_color(vail_srow_containers[i],
            i == 0 ? LV_COLOR_BG_LAYER2 : LV_COLOR_BG_DEEP, 0);
        lv_obj_set_style_bg_opa(vail_srow_containers[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(vail_srow_containers[i], 0, 0);
        lv_obj_set_style_border_side(vail_srow_containers[i], LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(vail_srow_containers[i], LV_COLOR_BG_LAYER2, 0);
        lv_obj_set_style_border_width(vail_srow_containers[i], 1, 0);
        lv_obj_set_style_pad_hor(vail_srow_containers[i], 20, 0);
        lv_obj_set_style_pad_ver(vail_srow_containers[i], 0, 0);
        lv_obj_set_layout(vail_srow_containers[i], LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(vail_srow_containers[i], LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(vail_srow_containers[i],
            LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(vail_srow_containers[i], LV_OBJ_FLAG_SCROLLABLE);

        // Left: setting name
        lv_obj_t* name_lbl = lv_label_create(vail_srow_containers[i]);
        lv_label_set_text(name_lbl, srow_names[i]);
        lv_obj_set_style_text_font(name_lbl, getThemeFonts()->font_body, 0);
        lv_obj_set_style_text_color(name_lbl, LV_COLOR_TEXT_PRIMARY, 0);

        // Right: current value
        vail_srow_values[i] = lv_label_create(vail_srow_containers[i]);
        lv_obj_set_style_text_font(vail_srow_values[i], getThemeFonts()->font_body, 0);
        lv_obj_set_style_text_color(vail_srow_values[i],
            i == 0 ? LV_COLOR_ACCENT_PRIMARY : LV_COLOR_TEXT_SECONDARY, 0);
    }
    refreshVailSettingsValues();

    // Sync the Listen tile + strip badge with the saved Listen-Only state.
    updateVailFooter();

    // Invisible focus container for keyboard input
    lv_obj_t* focus_container = lv_obj_create(screen);
    lv_obj_set_size(focus_container, 1, 1);
    lv_obj_set_pos(focus_container, -10, -10);
    lv_obj_set_style_bg_opa(focus_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus_container, 0, 0);
    lv_obj_set_style_outline_width(focus_container, 0, 0);
    lv_obj_set_style_outline_width(focus_container, 0, LV_STATE_FOCUSED);
    lv_obj_clear_flag(focus_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(focus_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus_container, vail_key_event_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus_container);
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }
    lv_group_focus_obj(focus_container);

    // Loading overlay with spinner
    vail_loading_overlay = lv_obj_create(screen);
    lv_obj_set_size(vail_loading_overlay, SCREEN_WIDTH, content_height);
    lv_obj_set_pos(vail_loading_overlay, 0, content_top);
    lv_obj_set_style_bg_color(vail_loading_overlay, LV_COLOR_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(vail_loading_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vail_loading_overlay, 0, 0);
    lv_obj_clear_flag(vail_loading_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* spinner = lv_spinner_create(vail_loading_overlay, 1000, 60);
    lv_obj_set_size(spinner, 50, 50);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_arc_color(spinner, LV_COLOR_ACCENT_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, LV_COLOR_BG_LAYER2, LV_PART_MAIN);

    lv_obj_t* loading_label = lv_label_create(vail_loading_overlay);
    lv_label_set_text(loading_label, "Connecting to Vail Repeater...");
    lv_obj_set_style_text_color(loading_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(loading_label, getThemeFonts()->font_body, 0);
    lv_obj_align(loading_label, LV_ALIGN_CENTER, 0, 35);

    vail_screen = screen;

    // Check if callsign is required
    if (checkVailCallsignRequired()) {
        showVailCallsignRequiredOverlay();
    }

    return screen;
}

// Update Vail screen elements from global state
// Call this from main loop when in Vail mode
void updateVailScreenLVGL() {
    if (vail_screen == NULL) return;

    // Deferred room reconnect (scheduled by the room-switch handlers)
    if (vail_reconnect_at_ms != 0 && millis() >= vail_reconnect_at_ms) {
        vail_reconnect_at_ms = 0;
        connectToVail(String(vail_pending_room));
    }

    // Detect connection drop and reset overlay state
    if (vailState == VAIL_DISCONNECTED && lastKnownVailState != VAIL_DISCONNECTED) {
        // Connection dropped — close any open overlays and reset back to the
        // operating view so the next key press is handled and the user sees
        // the up-to-date footer hints.
        vail_view_mode = 1;
        vail_custom_room_mode = false;
        if (vail_room_overlay != NULL) {
            lv_obj_add_flag(vail_room_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        if (vail_chat_input_overlay != NULL) {
            lv_obj_add_flag(vail_chat_input_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        if (vail_user_list_overlay != NULL) {
            lv_obj_add_flag(vail_user_list_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        Serial.println("[Vail LVGL] Connection dropped - overlay states reset");
    }
    lastKnownVailState = vailState;

    // Update status indicator color — receiver lamp state machine.
    // Mirrors the web repeater's recv-lamp behavior:
    //   gray    = disconnected
    //   yellow  = connecting
    //   cyan    = connected, idle
    //   orange  = currently receiving (someone else's keying playing back)
    //   green   = recently transmitted (own keying lit this within ~1.5s)
    //   red     = connection error
    static unsigned long lastTxLitMs = 0;
    if (vailIsTransmitting) lastTxLitMs = millis();
    bool recentlyTxd = (millis() - lastTxLitMs) < 1500;

    lv_color_t status_color;
    switch (vailState) {
        case VAIL_DISCONNECTED: status_color = LV_COLOR_TEXT_SECONDARY; break;
        case VAIL_CONNECTING:   status_color = LV_COLOR_WARNING;        break;
        case VAIL_CONNECTED:
            if (recentlyTxd)       status_color = LV_COLOR_SUCCESS;     // green: just keyed
            else if (isPlaying)    status_color = LV_COLOR_WARNING;     // orange-ish: RX
            else                   status_color = LV_COLOR_ACCENT_PRIMARY; // cyan: idle
            break;
        case VAIL_ERROR:        status_color = LV_COLOR_ERROR;          break;
        default:                status_color = LV_COLOR_TEXT_SECONDARY;
    }

    // Update indicator dot color
    if (vail_status_indicator != NULL) {
        lv_obj_set_style_bg_color(vail_status_indicator, status_color, 0);
    }

    // "ON AIR" pill: solid while keying, with an 800ms hold so it doesn't
    // flicker across inter-element and inter-character gaps. Toggled only on
    // state change to avoid needless redraws on the audio-critical loop.
    if (vail_onair_pill != NULL) {
        bool onAir = lastTxLitMs != 0 && (millis() - lastTxLitMs) < 800;
        bool pillVisible = !lv_obj_has_flag(vail_onair_pill, LV_OBJ_FLAG_HIDDEN);
        if (onAir != pillVisible) {
            if (onAir) lv_obj_clear_flag(vail_onair_pill, LV_OBJ_FLAG_HIDDEN);
            else       lv_obj_add_flag(vail_onair_pill, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Status strip battery/WiFi refresh (throttled; matches home's 3s cadence)
    static unsigned long lastStripRefreshMs = 0;
    if (millis() - lastStripRefreshMs > 3000) {
        lastStripRefreshMs = millis();
        vailApplyStripBattery(vail_strip_batt);
        vailApplyStripWifi(vail_strip_wifi);
    }

    // Show/hide loading overlay based on connection state
    if (vail_loading_overlay != NULL) {
        if (vailState == VAIL_CONNECTING) {
            // Show loading overlay while connecting
            lv_obj_clear_flag(vail_loading_overlay, LV_OBJ_FLAG_HIDDEN);
        } else {
            // Hide loading overlay when connected, disconnected, or error
            lv_obj_add_flag(vail_loading_overlay, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Update room name label in the status strip
    if (vail_room_label != NULL) {
        lv_label_set_text(vail_room_label, vailChannel.c_str());
    }

    // Sync chat messages if new ones arrived
    if (chatHistory.size() != vail_last_chat_count && vail_chat_textarea != NULL) {
        if (chatHistory.size() > vail_last_chat_count) {
            // Only append new messages (more efficient than rebuilding entire history)
            for (size_t i = vail_last_chat_count; i < chatHistory.size(); i++) {
                lv_textarea_add_text(vail_chat_textarea, chatHistory[i].callsign.c_str());
                lv_textarea_add_text(vail_chat_textarea, ": ");
                lv_textarea_add_text(vail_chat_textarea, chatHistory[i].message.c_str());
                lv_textarea_add_text(vail_chat_textarea, "\n");
            }
        } else {
            // History was cleared (e.g., room change) - rebuild from scratch
            String chatText = "";
            for (size_t i = 0; i < chatHistory.size(); i++) {
                chatText += chatHistory[i].callsign;
                chatText += ": ";
                chatText += chatHistory[i].message;
                chatText += "\n";
            }
            lv_textarea_set_text(vail_chat_textarea, chatText.c_str());
        }
        // Scroll to bottom
        lv_textarea_set_cursor_pos(vail_chat_textarea, LV_TEXTAREA_CURSOR_LAST);
        vail_last_chat_count = chatHistory.size();
    }

    // Update RX decoded TEXT row (characters decoded from incoming audio on
    // the Decoder room). Shows text only - never dot/dash symbols.
    if (vail_decoded_row_label != NULL && vail_current_view == 1 && vailDecodedNeedsUpdate) {
        vailDecodedNeedsUpdate = false;
        char buf[VAIL_DECODE_SLOTS + 1];
        int n = 0;
        for (; n < vailDecodedCount && n < VAIL_DECODE_SLOTS; n++) {
            buf[n] = vailDecodedEntries[n].ch;
        }
        buf[n] = '\0';
        lv_label_set_text(vail_decoded_row_label, buf);
    }

    // (Auto-decode-to-chat-input pipeline removed — chat is text-only via the
    //  on-demand compose modal. Operator's own keying is heard locally and
    //  transmitted on the air, but does not auto-type into chat.)

    // Side-pane operator list — rebuild whenever connectedUsers changes.
    // Title shows the count ("OPERATORS (3)"); the list shows up to five
    // callsigns, one per line, then "+N more".
    if (vail_users_label != NULL && vail_current_view == 1) {
        static size_t lastUserCount = SIZE_MAX;
        static char lastFirstUser[16] = "";  // crude change-detection, heap-free
        size_t curCount = connectedUsers.size();
        const char* curFirst = curCount > 0 ? connectedUsers[0].callsign.c_str() : "";
        if (curCount != lastUserCount || strncmp(curFirst, lastFirstUser, sizeof(lastFirstUser) - 1) != 0) {
            lastUserCount = curCount;
            strlcpy(lastFirstUser, curFirst, sizeof(lastFirstUser));

            if (vail_side_ops_title != NULL) {
                char ttl[32];
                snprintf(ttl, sizeof(ttl), "OPERATORS (%u)", (unsigned)curCount);
                lv_label_set_text(vail_side_ops_title, ttl);
            }

            if (curCount == 0) {
                lv_label_set_text(vail_users_label, "No one else here");
            } else {
                const size_t maxLines = 5;
                char buf[160];
                int off = 0;
                size_t shown = curCount < maxLines ? curCount : maxLines;
                for (size_t i = 0; i < shown && off < (int)sizeof(buf) - 16; i++) {
                    off += snprintf(buf + off, sizeof(buf) - off,
                        "%s%s", i == 0 ? "" : "\n", connectedUsers[i].callsign.c_str());
                }
                if (curCount > shown) {
                    snprintf(buf + off, sizeof(buf) - off, "\n+%u more",
                        (unsigned)(curCount - shown));
                }
                lv_label_set_text(vail_users_label, buf);
            }
        }
    }

    // Decoded row visibility tracks the room: only on the dedicated "Decoder"
    // room (matches vailmorse.com behavior). Re-checked each frame so a room
    // change immediately hides/shows the row; the chat history resizes to
    // give back / make room for the strip at the bottom of the hero card.
    if (vail_decoded_row_bg != NULL) {
        bool wantVisible = vailIsOnDecoderChannel();
        bool isVisible = !lv_obj_has_flag(vail_decoded_row_bg, LV_OBJ_FLAG_HIDDEN);
        if (wantVisible != isVisible) {
            if (wantVisible) lv_obj_clear_flag(vail_decoded_row_bg, LV_OBJ_FLAG_HIDDEN);
            else             lv_obj_add_flag(vail_decoded_row_bg, LV_OBJ_FLAG_HIDDEN);
            if (vail_chat_textarea != NULL) {
                lv_obj_set_height(vail_chat_textarea,
                    wantVisible ? VAIL_CHAT_H_DECODER : VAIL_CHAT_H_FULL);
            }
        }
    }
}

// Legacy compatibility functions
void updateVailStatus(const char* status, bool connected) {
    // Now handled by updateVailScreenLVGL()
}

void updateVailCallsign(const char* callsign) {
    // Now handled by updateVailScreenLVGL()
}

void appendVailMessage(const char* message) {
    // Messages are now synced from chatHistory in updateVailScreenLVGL()
}

// Reset all Vail Repeater static widget pointers on back-navigation.
// updateVailScreenLVGL() early-returns once vail_screen is NULL, so the
// poll path can never touch deleted widgets.
void cleanupVailRepeaterScreen() {
    vail_reconnect_at_ms = 0;
    vail_pending_room[0] = '\0';
    vail_screen = NULL;
    vail_chat_textarea = NULL;
    vail_status_label = NULL;
    vail_status_indicator = NULL;
    vail_loading_overlay = NULL;
    vail_room_label = NULL;
    vail_wpm_label = NULL;
    vail_strip_batt = NULL;
    vail_strip_wifi = NULL;
    vail_listen_badge = NULL;
    vail_onair_pill = NULL;
    vail_tx_strip_label = NULL;
    vail_tile_listen_icon = NULL;
    vail_tile_listen_label = NULL;
    vail_chat_panel = NULL;
    vail_settings_panel = NULL;
    vail_decoded_row_bg = NULL;
    vail_decoded_row_label = NULL;
    vail_users_label = NULL;
    vail_side_ops_title = NULL;
    for (int i = 0; i < VAIL_SETTINGS_ROW_COUNT; i++) {
        vail_srow_containers[i] = NULL;
        vail_srow_values[i] = NULL;
    }
    vail_settings_modal = NULL;
    vail_settings_value_label = NULL;
    vail_settings_title_label = NULL;
    vail_room_overlay = NULL;
    vail_room_list = NULL;
    vail_room_input_textarea = NULL;
    vail_chat_input_overlay = NULL;
    vail_chat_input_textarea = NULL;
    vail_user_list_overlay = NULL;
    vail_user_list = NULL;
    vail_callsign_overlay = NULL;
}

// ============================================
// QSO Logger Entry Screen (Enhanced)
// Full form with all QSO fields
// ============================================

static lv_obj_t* qso_entry_screen = NULL;
static lv_obj_t* qso_entry_focus_container = NULL;
static lv_obj_t* qso_callsign_input = NULL;
static lv_obj_t* qso_freq_input = NULL;
static lv_obj_t* qso_mode_row = NULL;
static lv_obj_t* qso_mode_label = NULL;
static lv_obj_t* qso_rst_sent_input = NULL;
static lv_obj_t* qso_rst_rcvd_input = NULL;
static lv_obj_t* qso_date_input = NULL;
static lv_obj_t* qso_time_input = NULL;
static lv_obj_t* qso_my_grid_input = NULL;
static lv_obj_t* qso_my_pota_input = NULL;
static lv_obj_t* qso_notes_input = NULL;

static int qso_entry_mode_index = 0;  // 0=CW, 1=SSB, etc.
static const char* qso_mode_names[] = {"CW", "SSB", "FM", "AM", "FT8", "FT4", "RTTY", "PSK31"};
static const int qso_mode_count = 8;

// Focus state for arrow key navigation
static int qso_entry_focus = 0;  // Current focused field index
static const int QSO_ENTRY_FIELD_COUNT = 10;  // Total navigable fields

// Forward declarations for QSO operations
extern Preferences qsoPrefs;
extern bool saveQSO(const QSO& qso);
extern String frequencyToBand(float freq);
extern String getDefaultRST(const char* mode);
extern void formatCurrentDateTime(char* dateOut, char* timeOut);

// Forward declaration for confirmation screen (defined later in file)
lv_obj_t* createQSOSaveConfirmScreen(const QSO& savedQso);

// Get current date/time string
static void getCurrentDateTimeStrings(char* dateOut, char* timeOut) {
    // Use formatCurrentDateTime from qso_logger_validation.h
    formatCurrentDateTime(dateOut, timeOut);
}

// Load operator settings (grid, pota) into form
static void loadOperatorSettingsToForm() {
    extern Preferences qsoPrefs;
    qsoPrefs.begin("qso_operator", true);  // Read-only

    char grid[9] = "";
    char pota[11] = "";
    qsoPrefs.getString("grid", grid, sizeof(grid));
    qsoPrefs.getString("pota_ref", pota, sizeof(pota));

    qsoPrefs.end();

    if (qso_my_grid_input != NULL && strlen(grid) > 0) {
        lv_textarea_set_text(qso_my_grid_input, grid);
    }
    if (qso_my_pota_input != NULL && strlen(pota) > 0) {
        lv_textarea_set_text(qso_my_pota_input, pota);
    }
}

// Update mode display and RST defaults
static void updateQSOEntryMode() {
    if (qso_mode_label != NULL) {
        lv_label_set_text_fmt(qso_mode_label, "< %s >", qso_mode_names[qso_entry_mode_index]);
    }

    // Update RST defaults based on mode
    String defaultRST = getDefaultRST(qso_mode_names[qso_entry_mode_index]);
    if (qso_rst_sent_input != NULL) {
        lv_textarea_set_text(qso_rst_sent_input, defaultRST.c_str());
    }
    if (qso_rst_rcvd_input != NULL) {
        lv_textarea_set_text(qso_rst_rcvd_input, defaultRST.c_str());
    }
}

// Update focus to the current field
static void qso_entry_update_focus() {
    // Array of all navigable widgets in order
    lv_obj_t* fields[] = {
        qso_callsign_input,
        qso_freq_input,
        qso_mode_row,
        qso_rst_sent_input,
        qso_rst_rcvd_input,
        qso_date_input,
        qso_time_input,
        qso_my_grid_input,
        qso_my_pota_input,
        qso_notes_input
    };

    // Focus the correct widget in LVGL's group
    lv_group_t* group = getLVGLInputGroup();
    if (group && qso_entry_focus >= 0 && qso_entry_focus < QSO_ENTRY_FIELD_COUNT) {
        lv_obj_t* target = fields[qso_entry_focus];
        if (target != NULL) {
            lv_group_focus_obj(target);
        }
    }
}

// Flag to prevent re-entry during save/navigation
static bool qso_entry_navigating = false;

// Key handler for QSO entry form navigation
static void qso_entry_key_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    // Prevent re-entry if we're already navigating away
    if (qso_entry_navigating) {
        lv_event_stop_processing(e);
        return;
    }

    uint32_t key = lv_event_get_key(e);

    // ESC - cancel and exit
    if (key == LV_KEY_ESC) {
        qso_entry_navigating = true;
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);  // Prevent global_esc_handler from also firing
        onLVGLBackNavigation();
        return;
    }

    // Handle UP - previous field
    if (key == LV_KEY_UP) {
        lv_event_stop_bubbling(e);
        if (qso_entry_focus > 0) {
            qso_entry_focus--;
            qso_entry_update_focus();
            beep(TONE_MENU_NAV, BEEP_SHORT);
        }
        return;
    }

    // Handle DOWN / TAB - next field
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        lv_event_stop_bubbling(e);
        if (qso_entry_focus < QSO_ENTRY_FIELD_COUNT - 1) {
            qso_entry_focus++;
            qso_entry_update_focus();
            beep(TONE_MENU_NAV, BEEP_SHORT);
        }
        return;
    }

    // LEFT/RIGHT on mode selector (field index 2)
    if (qso_entry_focus == 2) {
        if (key == LV_KEY_LEFT) {
            qso_entry_mode_index = (qso_entry_mode_index - 1 + qso_mode_count) % qso_mode_count;
            updateQSOEntryMode();
            lv_event_stop_bubbling(e);
            return;
        }
        if (key == LV_KEY_RIGHT) {
            qso_entry_mode_index = (qso_entry_mode_index + 1) % qso_mode_count;
            updateQSOEntryMode();
            lv_event_stop_bubbling(e);
            return;
        }
    }

    // ENTER - save QSO
    if (key == LV_KEY_ENTER) {
        // Collect all field values
        const char* callsign = lv_textarea_get_text(qso_callsign_input);
        const char* freqStr = lv_textarea_get_text(qso_freq_input);
        const char* rst_sent = lv_textarea_get_text(qso_rst_sent_input);
        const char* rst_rcvd = lv_textarea_get_text(qso_rst_rcvd_input);
        const char* dateStr = lv_textarea_get_text(qso_date_input);
        const char* timeStr = lv_textarea_get_text(qso_time_input);
        const char* my_grid = lv_textarea_get_text(qso_my_grid_input);
        const char* my_pota = lv_textarea_get_text(qso_my_pota_input);
        const char* notes = lv_textarea_get_text(qso_notes_input);

        // Validate required fields
        if (strlen(callsign) < 3) {
            beep(600, 100);  // Error - need callsign
            lv_group_focus_obj(qso_callsign_input);
            lv_event_stop_bubbling(e);
            return;
        }

        // Create QSO struct
        QSO qso;
        memset(&qso, 0, sizeof(QSO));

        qso.id = millis();
        strlcpy(qso.callsign, callsign, sizeof(qso.callsign));

        // Convert frequency to MHz
        float freqKHz = atof(freqStr);
        qso.frequency = freqKHz / 1000.0;  // kHz to MHz

        // Get band from frequency
        String band = frequencyToBand(qso.frequency);
        strlcpy(qso.band, band.c_str(), sizeof(qso.band));

        strlcpy(qso.mode, qso_mode_names[qso_entry_mode_index], sizeof(qso.mode));
        strlcpy(qso.rst_sent, rst_sent, sizeof(qso.rst_sent));
        strlcpy(qso.rst_rcvd, rst_rcvd, sizeof(qso.rst_rcvd));
        strlcpy(qso.date, dateStr, sizeof(qso.date));
        strlcpy(qso.time_on, timeStr, sizeof(qso.time_on));
        strlcpy(qso.my_gridsquare, my_grid, sizeof(qso.my_gridsquare));
        strlcpy(qso.my_pota_ref, my_pota, sizeof(qso.my_pota_ref));
        strlcpy(qso.notes, notes, sizeof(qso.notes));

        // Save QSO
        if (saveQSO(qso)) {
            beep(1000, 100);  // Success
            lv_event_stop_processing(e);
            // Show confirmation screen with saved QSO summary
            lv_obj_t* confirmScreen = createQSOSaveConfirmScreen(qso);
            loadScreen(confirmScreen, SCREEN_ANIM_FADE);
        } else {
            // Save failed - SD card may have been removed
            beep(400, 200);  // Error beep
            lv_obj_t* msgbox = lv_msgbox_create(NULL, "Save Failed",
                "Could not save QSO.\nCheck SD card.", NULL, true);
            lv_obj_center(msgbox);
        }
        return;
    }

    lv_event_stop_bubbling(e);
}

lv_obj_t* createQSOLogEntryScreen() {
    clearNavigationGroup();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Reset navigation flag and mode index
    qso_entry_navigating = false;
    qso_entry_mode_index = 0;

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "NEW QSO");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    createCompactStatusBar(screen);

    // Scrollable form container
    lv_obj_t* form = lv_obj_create(screen);
    lv_obj_set_size(form, SCREEN_WIDTH - 20, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 10);
    lv_obj_set_pos(form, 10, HEADER_HEIGHT + 5);
    lv_obj_set_layout(form, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(form, 8, 0);
    lv_obj_set_style_pad_all(form, 10, 0);
    lv_obj_set_style_bg_opa(form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(form, 0, 0);
    lv_obj_add_flag(form, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(form, LV_SCROLLBAR_MODE_AUTO);

    // Row 1: Callsign
    lv_obj_t* call_label = lv_label_create(form);
    lv_label_set_text(call_label, "Callsign *");
    lv_obj_set_style_text_color(call_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(call_label, getThemeFonts()->font_small, 0);

    qso_callsign_input = lv_textarea_create(form);
    lv_obj_set_size(qso_callsign_input, lv_pct(100), 35);
    lv_textarea_set_one_line(qso_callsign_input, true);
    lv_textarea_set_max_length(qso_callsign_input, 12);
    lv_textarea_set_placeholder_text(qso_callsign_input, "W1ABC");
    applyTextareaStyle(qso_callsign_input);
    lv_obj_add_event_cb(qso_callsign_input, qso_entry_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(qso_callsign_input);

    // Row 2: Frequency + Mode (side by side)
    lv_obj_t* freq_mode_row = lv_obj_create(form);
    lv_obj_set_size(freq_mode_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(freq_mode_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(freq_mode_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(freq_mode_row, 15, 0);
    lv_obj_set_style_bg_opa(freq_mode_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(freq_mode_row, 0, 0);
    lv_obj_set_style_pad_all(freq_mode_row, 0, 0);
    lv_obj_clear_flag(freq_mode_row, LV_OBJ_FLAG_SCROLLABLE);

    // Frequency column
    lv_obj_t* freq_col = lv_obj_create(freq_mode_row);
    lv_obj_set_size(freq_col, lv_pct(55), LV_SIZE_CONTENT);
    lv_obj_set_layout(freq_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(freq_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(freq_col, 3, 0);
    lv_obj_set_style_bg_opa(freq_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(freq_col, 0, 0);
    lv_obj_set_style_pad_all(freq_col, 0, 0);
    lv_obj_clear_flag(freq_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* freq_label = lv_label_create(freq_col);
    lv_label_set_text(freq_label, "Frequency (kHz)");
    lv_obj_set_style_text_color(freq_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(freq_label, getThemeFonts()->font_small, 0);

    qso_freq_input = lv_textarea_create(freq_col);
    lv_obj_set_size(qso_freq_input, lv_pct(100), 35);
    lv_textarea_set_one_line(qso_freq_input, true);
    lv_textarea_set_max_length(qso_freq_input, 10);
    lv_textarea_set_text(qso_freq_input, "7030");
    applyTextareaStyle(qso_freq_input);
    lv_obj_add_event_cb(qso_freq_input, qso_entry_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(qso_freq_input);

    // Mode column
    lv_obj_t* mode_col = lv_obj_create(freq_mode_row);
    lv_obj_set_size(mode_col, lv_pct(40), LV_SIZE_CONTENT);
    lv_obj_set_layout(mode_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(mode_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(mode_col, 3, 0);
    lv_obj_set_style_bg_opa(mode_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mode_col, 0, 0);
    lv_obj_set_style_pad_all(mode_col, 0, 0);
    lv_obj_clear_flag(mode_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* mode_title = lv_label_create(mode_col);
    lv_label_set_text(mode_title, "Mode");
    lv_obj_set_style_text_color(mode_title, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(mode_title, getThemeFonts()->font_small, 0);

    qso_mode_row = lv_obj_create(mode_col);
    lv_obj_set_size(qso_mode_row, lv_pct(100), 35);
    lv_obj_set_style_bg_color(qso_mode_row, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_radius(qso_mode_row, 6, 0);
    lv_obj_set_style_border_width(qso_mode_row, 1, 0);
    lv_obj_set_style_border_color(qso_mode_row, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_pad_all(qso_mode_row, 5, 0);
    // Add focus styling - cyan border and slight glow when focused
    lv_obj_set_style_border_color(qso_mode_row, LV_COLOR_ACCENT_PRIMARY, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(qso_mode_row, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(qso_mode_row, LV_COLOR_ACCENT_PRIMARY, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(qso_mode_row, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(qso_mode_row, LV_OPA_50, LV_STATE_FOCUSED);
    lv_obj_clear_flag(qso_mode_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(qso_mode_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(qso_mode_row, qso_entry_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(qso_mode_row);

    qso_mode_label = lv_label_create(qso_mode_row);
    lv_label_set_text_fmt(qso_mode_label, "< %s >", qso_mode_names[qso_entry_mode_index]);
    lv_obj_set_style_text_font(qso_mode_label, getThemeFonts()->font_body, 0);
    lv_obj_center(qso_mode_label);

    // Row 3: RST Sent + RST Rcvd (side by side)
    lv_obj_t* rst_row = lv_obj_create(form);
    lv_obj_set_size(rst_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(rst_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(rst_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(rst_row, 15, 0);
    lv_obj_set_style_bg_opa(rst_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rst_row, 0, 0);
    lv_obj_set_style_pad_all(rst_row, 0, 0);
    lv_obj_clear_flag(rst_row, LV_OBJ_FLAG_SCROLLABLE);

    // RST Sent column
    lv_obj_t* rst_sent_col = lv_obj_create(rst_row);
    lv_obj_set_size(rst_sent_col, lv_pct(45), LV_SIZE_CONTENT);
    lv_obj_set_layout(rst_sent_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(rst_sent_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(rst_sent_col, 3, 0);
    lv_obj_set_style_bg_opa(rst_sent_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rst_sent_col, 0, 0);
    lv_obj_set_style_pad_all(rst_sent_col, 0, 0);
    lv_obj_clear_flag(rst_sent_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* rst_sent_label = lv_label_create(rst_sent_col);
    lv_label_set_text(rst_sent_label, "RST Sent");
    lv_obj_set_style_text_color(rst_sent_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(rst_sent_label, getThemeFonts()->font_small, 0);

    qso_rst_sent_input = lv_textarea_create(rst_sent_col);
    lv_obj_set_size(qso_rst_sent_input, lv_pct(100), 35);
    lv_textarea_set_one_line(qso_rst_sent_input, true);
    lv_textarea_set_max_length(qso_rst_sent_input, 3);
    lv_textarea_set_text(qso_rst_sent_input, "599");
    applyTextareaStyle(qso_rst_sent_input);
    lv_obj_add_event_cb(qso_rst_sent_input, qso_entry_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(qso_rst_sent_input);

    // RST Rcvd column
    lv_obj_t* rst_rcvd_col = lv_obj_create(rst_row);
    lv_obj_set_size(rst_rcvd_col, lv_pct(45), LV_SIZE_CONTENT);
    lv_obj_set_layout(rst_rcvd_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(rst_rcvd_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(rst_rcvd_col, 3, 0);
    lv_obj_set_style_bg_opa(rst_rcvd_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rst_rcvd_col, 0, 0);
    lv_obj_set_style_pad_all(rst_rcvd_col, 0, 0);
    lv_obj_clear_flag(rst_rcvd_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* rst_rcvd_label = lv_label_create(rst_rcvd_col);
    lv_label_set_text(rst_rcvd_label, "RST Rcvd");
    lv_obj_set_style_text_color(rst_rcvd_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(rst_rcvd_label, getThemeFonts()->font_small, 0);

    qso_rst_rcvd_input = lv_textarea_create(rst_rcvd_col);
    lv_obj_set_size(qso_rst_rcvd_input, lv_pct(100), 35);
    lv_textarea_set_one_line(qso_rst_rcvd_input, true);
    lv_textarea_set_max_length(qso_rst_rcvd_input, 3);
    lv_textarea_set_text(qso_rst_rcvd_input, "599");
    applyTextareaStyle(qso_rst_rcvd_input);
    lv_obj_add_event_cb(qso_rst_rcvd_input, qso_entry_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(qso_rst_rcvd_input);

    // Row 4: Date + Time (side by side)
    lv_obj_t* datetime_row = lv_obj_create(form);
    lv_obj_set_size(datetime_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(datetime_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(datetime_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(datetime_row, 15, 0);
    lv_obj_set_style_bg_opa(datetime_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(datetime_row, 0, 0);
    lv_obj_set_style_pad_all(datetime_row, 0, 0);
    lv_obj_clear_flag(datetime_row, LV_OBJ_FLAG_SCROLLABLE);

    // Date column
    lv_obj_t* date_col = lv_obj_create(datetime_row);
    lv_obj_set_size(date_col, lv_pct(55), LV_SIZE_CONTENT);
    lv_obj_set_layout(date_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(date_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(date_col, 3, 0);
    lv_obj_set_style_bg_opa(date_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(date_col, 0, 0);
    lv_obj_set_style_pad_all(date_col, 0, 0);
    lv_obj_clear_flag(date_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* date_label = lv_label_create(date_col);
    lv_label_set_text(date_label, "Date (YYYYMMDD)");
    lv_obj_set_style_text_color(date_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(date_label, getThemeFonts()->font_small, 0);

    qso_date_input = lv_textarea_create(date_col);
    lv_obj_set_size(qso_date_input, lv_pct(100), 35);
    lv_textarea_set_one_line(qso_date_input, true);
    lv_textarea_set_max_length(qso_date_input, 8);
    applyTextareaStyle(qso_date_input);
    lv_obj_add_event_cb(qso_date_input, qso_entry_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(qso_date_input);

    // Time column
    lv_obj_t* time_col = lv_obj_create(datetime_row);
    lv_obj_set_size(time_col, lv_pct(40), LV_SIZE_CONTENT);
    lv_obj_set_layout(time_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(time_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(time_col, 3, 0);
    lv_obj_set_style_bg_opa(time_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(time_col, 0, 0);
    lv_obj_set_style_pad_all(time_col, 0, 0);
    lv_obj_clear_flag(time_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* time_label = lv_label_create(time_col);
    lv_label_set_text(time_label, "Time UTC");
    lv_obj_set_style_text_color(time_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(time_label, getThemeFonts()->font_small, 0);

    qso_time_input = lv_textarea_create(time_col);
    lv_obj_set_size(qso_time_input, lv_pct(100), 35);
    lv_textarea_set_one_line(qso_time_input, true);
    lv_textarea_set_max_length(qso_time_input, 4);
    applyTextareaStyle(qso_time_input);
    lv_obj_add_event_cb(qso_time_input, qso_entry_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(qso_time_input);

    // Auto-fill date and time
    char dateStr[12], timeStr[8];
    getCurrentDateTimeStrings(dateStr, timeStr);
    lv_textarea_set_text(qso_date_input, dateStr);
    lv_textarea_set_text(qso_time_input, timeStr);

    // Row 5: My Grid + My POTA (side by side)
    lv_obj_t* my_loc_row = lv_obj_create(form);
    lv_obj_set_size(my_loc_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(my_loc_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(my_loc_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(my_loc_row, 15, 0);
    lv_obj_set_style_bg_opa(my_loc_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(my_loc_row, 0, 0);
    lv_obj_set_style_pad_all(my_loc_row, 0, 0);
    lv_obj_clear_flag(my_loc_row, LV_OBJ_FLAG_SCROLLABLE);

    // My Grid column
    lv_obj_t* my_grid_col = lv_obj_create(my_loc_row);
    lv_obj_set_size(my_grid_col, lv_pct(45), LV_SIZE_CONTENT);
    lv_obj_set_layout(my_grid_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(my_grid_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(my_grid_col, 3, 0);
    lv_obj_set_style_bg_opa(my_grid_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(my_grid_col, 0, 0);
    lv_obj_set_style_pad_all(my_grid_col, 0, 0);
    lv_obj_clear_flag(my_grid_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* my_grid_label = lv_label_create(my_grid_col);
    lv_label_set_text(my_grid_label, "My Grid");
    lv_obj_set_style_text_color(my_grid_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(my_grid_label, getThemeFonts()->font_small, 0);

    qso_my_grid_input = lv_textarea_create(my_grid_col);
    lv_obj_set_size(qso_my_grid_input, lv_pct(100), 35);
    lv_textarea_set_one_line(qso_my_grid_input, true);
    lv_textarea_set_max_length(qso_my_grid_input, 6);
    lv_textarea_set_placeholder_text(qso_my_grid_input, "EN52wa");
    applyTextareaStyle(qso_my_grid_input);
    lv_obj_add_event_cb(qso_my_grid_input, qso_entry_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(qso_my_grid_input);

    // My POTA column
    lv_obj_t* my_pota_col = lv_obj_create(my_loc_row);
    lv_obj_set_size(my_pota_col, lv_pct(50), LV_SIZE_CONTENT);
    lv_obj_set_layout(my_pota_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(my_pota_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(my_pota_col, 3, 0);
    lv_obj_set_style_bg_opa(my_pota_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(my_pota_col, 0, 0);
    lv_obj_set_style_pad_all(my_pota_col, 0, 0);
    lv_obj_clear_flag(my_pota_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* my_pota_label = lv_label_create(my_pota_col);
    lv_label_set_text(my_pota_label, "My POTA");
    lv_obj_set_style_text_color(my_pota_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(my_pota_label, getThemeFonts()->font_small, 0);

    qso_my_pota_input = lv_textarea_create(my_pota_col);
    lv_obj_set_size(qso_my_pota_input, lv_pct(100), 35);
    lv_textarea_set_one_line(qso_my_pota_input, true);
    lv_textarea_set_max_length(qso_my_pota_input, 10);
    lv_textarea_set_placeholder_text(qso_my_pota_input, "US-2256");
    applyTextareaStyle(qso_my_pota_input);
    lv_obj_add_event_cb(qso_my_pota_input, qso_entry_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(qso_my_pota_input);

    // Row 6: Notes
    lv_obj_t* notes_label = lv_label_create(form);
    lv_label_set_text(notes_label, "Notes");
    lv_obj_set_style_text_color(notes_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(notes_label, getThemeFonts()->font_small, 0);

    qso_notes_input = lv_textarea_create(form);
    lv_obj_set_size(qso_notes_input, lv_pct(100), 35);
    lv_textarea_set_one_line(qso_notes_input, true);
    lv_textarea_set_max_length(qso_notes_input, 60);
    lv_textarea_set_placeholder_text(qso_notes_input, "Optional notes");
    applyTextareaStyle(qso_notes_input);
    lv_obj_add_event_cb(qso_notes_input, qso_entry_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(qso_notes_input);

    // Load operator settings (grid, pota) from preferences
    loadOperatorSettingsToForm();

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate   " LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " Mode   ENTER Save   ESC Cancel");
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    // Reset focus state and focus first field
    qso_entry_focus = 0;
    lv_group_t* group = getLVGLInputGroup();
    if (group) {
        lv_group_focus_obj(qso_callsign_input);
    }

    qso_entry_screen = screen;
    return screen;
}

// Reset QSO entry static widget pointers on back-navigation
void cleanupQSOEntryScreen() {
    qso_entry_screen = NULL;
    qso_entry_focus_container = NULL;
    qso_callsign_input = NULL;
    qso_freq_input = NULL;
    qso_mode_row = NULL;
    qso_mode_label = NULL;
    qso_rst_sent_input = NULL;
    qso_rst_rcvd_input = NULL;
    qso_date_input = NULL;
    qso_time_input = NULL;
    qso_my_grid_input = NULL;
    qso_my_pota_input = NULL;
    qso_notes_input = NULL;
}

// ============================================
// QSO Save Confirmation Screen
// Shows summary of saved QSO with options
// ============================================

// Static variables for the saved QSO data (used by confirmation screen)
static QSO qso_saved_qso;
static bool qso_confirm_ready = false;  // Prevent immediate button activation
static unsigned long qso_confirm_show_time = 0;  // Time when screen was shown

// Forward declaration for getTotalLogs
extern int getTotalLogs();

// Key event callback for confirmation screen
static void qso_confirm_key_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_ESC) {
        // Exit to QSO Logger menu
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
        onLVGLBackNavigation();
    } else if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        // Navigate between buttons using left/right arrows
        lv_group_t* group = getLVGLInputGroup();
        if (group) {
            if (key == LV_KEY_LEFT) {
                lv_group_focus_prev(group);
            } else {
                lv_group_focus_next(group);
            }
            lv_event_stop_bubbling(e);
        }
    }
}

// Button event callback for Log Another
static void qso_confirm_log_another_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    // Ignore clicks within 300ms of screen showing (prevents ENTER key leak-through)
    if (millis() - qso_confirm_show_time < 300) return;

    beep(800, 50);
    // Create new QSO entry screen
    lv_obj_t* entryScreen = createQSOLogEntryScreen();
    loadScreen(entryScreen, SCREEN_ANIM_SLIDE_LEFT);
}

// Button event callback for Exit
static void qso_confirm_exit_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    // Ignore clicks within 300ms of screen showing (prevents ENTER key leak-through)
    if (millis() - qso_confirm_show_time < 300) return;

    beep(800, 50);
    // Navigate back to QSO Logger menu
    onLVGLBackNavigation();
}

lv_obj_t* createQSOSaveConfirmScreen(const QSO& savedQso) {
    clearNavigationGroup();

    // Store the saved QSO for display
    memcpy(&qso_saved_qso, &savedQso, sizeof(QSO));

    // Record when the screen was shown to prevent immediate button activation
    qso_confirm_show_time = millis();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, LV_SYMBOL_OK " QSO SAVED!");
    lv_obj_set_style_text_color(title, LV_COLOR_SUCCESS, 0);
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    createCompactStatusBar(screen);

    // Content container - centered
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_WIDTH - 40, SCREEN_HEIGHT - HEADER_HEIGHT - 80);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 10);
    lv_obj_set_style_bg_color(content, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(content, 1, 0);
    lv_obj_set_style_border_color(content, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 15, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // QSO Summary
    // Callsign - large and prominent
    lv_obj_t* callsign_label = lv_label_create(content);
    lv_label_set_text_fmt(callsign_label, "%s", qso_saved_qso.callsign);
    lv_obj_set_style_text_font(callsign_label, getThemeFonts()->font_large, 0);
    lv_obj_set_style_text_color(callsign_label, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(callsign_label, LV_ALIGN_TOP_MID, 0, 0);

    // Frequency and Band
    lv_obj_t* freq_label = lv_label_create(content);
    lv_label_set_text_fmt(freq_label, "%.3f MHz (%s)", qso_saved_qso.frequency, qso_saved_qso.band);
    lv_obj_set_style_text_color(freq_label, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(freq_label, LV_ALIGN_TOP_MID, 0, 35);

    // Mode
    lv_obj_t* mode_label = lv_label_create(content);
    lv_label_set_text_fmt(mode_label, "Mode: %s", qso_saved_qso.mode);
    lv_obj_set_style_text_color(mode_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(mode_label, LV_ALIGN_TOP_MID, 0, 60);

    // RST
    lv_obj_t* rst_label = lv_label_create(content);
    lv_label_set_text_fmt(rst_label, "RST: %s / %s", qso_saved_qso.rst_sent, qso_saved_qso.rst_rcvd);
    lv_obj_set_style_text_color(rst_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(rst_label, LV_ALIGN_TOP_MID, 0, 85);

    // Total logs count
    lv_obj_t* total_label = lv_label_create(content);
    lv_label_set_text_fmt(total_label, "Total QSOs logged: %d", getTotalLogs());
    lv_obj_set_style_text_color(total_label, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(total_label, getThemeFonts()->font_small, 0);
    lv_obj_align(total_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Button row at bottom
    lv_obj_t* btn_row = lv_obj_create(screen);
    lv_obj_set_size(btn_row, SCREEN_WIDTH - 40, 50);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_layout(btn_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    // Log Another button
    lv_obj_t* btn_another = lv_btn_create(btn_row);
    lv_obj_set_size(btn_another, 180, 40);
    applyMenuCardStyle(btn_another);
    lv_obj_add_event_cb(btn_another, qso_confirm_log_another_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_another, qso_confirm_key_event_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(btn_another);

    lv_obj_t* btn_another_label = lv_label_create(btn_another);
    lv_label_set_text(btn_another_label, "Log Another");
    lv_obj_center(btn_another_label);

    // Exit button
    lv_obj_t* btn_exit = lv_btn_create(btn_row);
    lv_obj_set_size(btn_exit, 180, 40);
    applyMenuCardStyle(btn_exit);
    lv_obj_add_event_cb(btn_exit, qso_confirm_exit_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_exit, qso_confirm_key_event_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(btn_exit);

    lv_obj_t* btn_exit_label = lv_label_create(btn_exit);
    lv_label_set_text(btn_exit_label, "Exit");
    lv_obj_center(btn_exit_label);

    // Focus the "Log Another" button by default
    lv_group_t* group = getLVGLInputGroup();
    if (group) {
        lv_group_focus_obj(btn_another);
    }

    return screen;
}

// ============================================
// Bluetooth HID Screen
// ============================================

static lv_obj_t* bt_hid_screen = NULL;
static lv_obj_t* bt_hid_status_label = NULL;
static lv_obj_t* bt_hid_device_name_label = NULL;
static lv_obj_t* bt_hid_dit_indicator = NULL;
static lv_obj_t* bt_hid_dah_indicator = NULL;
static lv_obj_t* bt_hid_keyer_label = NULL;
static lv_obj_t* bt_hid_typing_label = NULL;
static lv_obj_t* bt_hid_footer_label = NULL;

// Forward declarations (defined in ble_hid.h)
extern void cycleBTHIDKeyerMode(int direction);
extern bool btHIDTypingEnabled;
extern void setBTHIDTyping(bool enabled);
extern bool sendBTHIDKeystroke(uint32_t lvglKey);

// Key event callback for BT HID keyboard input.
// Normal mode: arrows change keyer, UP/DOWN enables typing, ESC exits.
// Typing mode: every key is forwarded to the host; ESC stops typing.
static void bt_hid_key_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (btHIDTypingEnabled) {
        if (key == LV_KEY_ESC) {
            setBTHIDTyping(false);
        } else {
            sendBTHIDKeystroke(key);
        }
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_ESC) {
        onLVGLBackNavigation();
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_LEFT) {
        cycleBTHIDKeyerMode(-1);  // Previous keyer mode
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_RIGHT) {
        cycleBTHIDKeyerMode(1);   // Next keyer mode
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_UP || key == LV_KEY_DOWN) {
        setBTHIDTyping(true);     // Start forwarding CardKB keys to the host
        lv_event_stop_processing(e);
    }
}

lv_obj_t* createBTHIDScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "BT KEYBOARD");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // === Single Centered Card ===
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, 400, 210);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 5);
    applyCardStyle(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 15, 0);

    // Row 1: Bluetooth icon + Device name (top, centered)
    lv_obj_t* name_row = lv_obj_create(card);
    lv_obj_set_size(name_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(name_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(name_row, 0, 0);
    lv_obj_set_style_pad_all(name_row, 0, 0);
    lv_obj_align(name_row, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(name_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* bt_icon = lv_label_create(name_row);
    lv_label_set_text(bt_icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_font(bt_icon, getThemeFonts()->font_large, 0);  // Theme font includes symbols
    lv_obj_set_style_text_color(bt_icon, LV_COLOR_ACCENT_SECONDARY, 0);
    lv_obj_align(bt_icon, LV_ALIGN_LEFT_MID, 100, 0);

    bt_hid_device_name_label = lv_label_create(name_row);
    lv_label_set_text(bt_hid_device_name_label, "VAIL-SUMMIT-XXXXXX");
    lv_obj_set_style_text_color(bt_hid_device_name_label, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(bt_hid_device_name_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_align(bt_hid_device_name_label, LV_ALIGN_LEFT_MID, 140, 0);

    // Row 2: Connection status (centered, colored)
    bt_hid_status_label = lv_label_create(card);
    lv_label_set_text(bt_hid_status_label, "Advertising...");
    lv_obj_set_style_text_font(bt_hid_status_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(bt_hid_status_label, LV_COLOR_WARNING, 0);
    lv_obj_align(bt_hid_status_label, LV_ALIGN_TOP_MID, 0, 35);

    // Row 3: Keyer mode selector (< Passthrough >)
    lv_obj_t* keyer_row = lv_obj_create(card);
    lv_obj_set_size(keyer_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(keyer_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(keyer_row, 0, 0);
    lv_obj_set_style_pad_all(keyer_row, 0, 0);
    lv_obj_align(keyer_row, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_clear_flag(keyer_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* keyer_title = lv_label_create(keyer_row);
    lv_label_set_text(keyer_title, "Keyer:");
    lv_obj_set_style_text_color(keyer_title, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(keyer_title, getThemeFonts()->font_body, 0);
    lv_obj_align(keyer_title, LV_ALIGN_LEFT_MID, 70, 0);

    bt_hid_keyer_label = lv_label_create(keyer_row);
    lv_label_set_text(bt_hid_keyer_label, "< Passthrough >");
    lv_obj_set_style_text_color(bt_hid_keyer_label, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(bt_hid_keyer_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_align(bt_hid_keyer_label, LV_ALIGN_LEFT_MID, 145, 0);

    // Row 4: Key mapping (DIT -> Left Ctrl, DAH -> Right Ctrl)
    lv_obj_t* mapping_row = lv_obj_create(card);
    lv_obj_set_size(mapping_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(mapping_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mapping_row, 0, 0);
    lv_obj_set_style_pad_all(mapping_row, 0, 0);
    lv_obj_align(mapping_row, LV_ALIGN_TOP_MID, 0, 95);
    lv_obj_clear_flag(mapping_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* dit_mapping = lv_label_create(mapping_row);
    lv_label_set_text(dit_mapping, "DIT " LV_SYMBOL_RIGHT " Left Ctrl");
    lv_obj_set_style_text_color(dit_mapping, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(dit_mapping, getThemeFonts()->font_small, 0);
    lv_obj_align(dit_mapping, LV_ALIGN_LEFT_MID, 50, 0);

    lv_obj_t* dah_mapping = lv_label_create(mapping_row);
    lv_label_set_text(dah_mapping, "DAH " LV_SYMBOL_RIGHT " Right Ctrl");
    lv_obj_set_style_text_color(dah_mapping, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(dah_mapping, getThemeFonts()->font_small, 0);
    lv_obj_align(dah_mapping, LV_ALIGN_LEFT_MID, 220, 0);

    // Row 5: Paddle LED indicators at bottom
    lv_obj_t* indicator_row = lv_obj_create(card);
    lv_obj_set_size(indicator_row, lv_pct(100), 50);
    lv_obj_set_style_bg_opa(indicator_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(indicator_row, 0, 0);
    lv_obj_set_style_pad_all(indicator_row, 0, 0);
    lv_obj_align(indicator_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(indicator_row, LV_OBJ_FLAG_SCROLLABLE);

    // DIT indicator (left side)
    bt_hid_dit_indicator = lv_led_create(indicator_row);
    lv_led_set_color(bt_hid_dit_indicator, LV_COLOR_SUCCESS);
    lv_obj_set_size(bt_hid_dit_indicator, 30, 30);
    lv_obj_align(bt_hid_dit_indicator, LV_ALIGN_LEFT_MID, 100, 0);
    lv_led_off(bt_hid_dit_indicator);

    lv_obj_t* dit_label = lv_label_create(indicator_row);
    lv_label_set_text(dit_label, "DIT");
    lv_obj_set_style_text_color(dit_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(dit_label, getThemeFonts()->font_body, 0);
    lv_obj_align(dit_label, LV_ALIGN_LEFT_MID, 140, 0);

    // DAH indicator (right side)
    bt_hid_dah_indicator = lv_led_create(indicator_row);
    lv_led_set_color(bt_hid_dah_indicator, LV_COLOR_SUCCESS);
    lv_obj_set_size(bt_hid_dah_indicator, 30, 30);
    lv_obj_align(bt_hid_dah_indicator, LV_ALIGN_LEFT_MID, 210, 0);
    lv_led_off(bt_hid_dah_indicator);

    lv_obj_t* dah_label = lv_label_create(indicator_row);
    lv_label_set_text(dah_label, "DAH");
    lv_obj_set_style_text_color(dah_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(dah_label, getThemeFonts()->font_body, 0);
    lv_obj_align(dah_label, LV_ALIGN_LEFT_MID, 250, 0);

    // Typing mode status (right side of indicator row)
    bt_hid_typing_label = lv_label_create(indicator_row);
    lv_label_set_text(bt_hid_typing_label, "Typing: OFF");
    lv_obj_set_style_text_color(bt_hid_typing_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(bt_hid_typing_label, getThemeFonts()->font_body, 0);
    lv_obj_align(bt_hid_typing_label, LV_ALIGN_RIGHT_MID, -5, 0);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    bt_hid_footer_label = lv_label_create(footer);
    lv_label_set_text(bt_hid_footer_label, LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " Keyer    " LV_SYMBOL_UP " Typing    Paddle to key    ESC Exit");
    lv_obj_set_style_text_color(bt_hid_footer_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(bt_hid_footer_label, getThemeFonts()->font_small, 0);
    lv_obj_center(bt_hid_footer_label);

    // Invisible focus container for keyboard handling
    lv_obj_t* focus_container = lv_obj_create(screen);
    lv_obj_set_size(focus_container, 1, 1);
    lv_obj_set_pos(focus_container, -10, -10);
    lv_obj_set_style_bg_opa(focus_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus_container, 0, 0);
    lv_obj_set_style_outline_width(focus_container, 0, 0);
    lv_obj_set_style_outline_width(focus_container, 0, LV_STATE_FOCUSED);
    lv_obj_clear_flag(focus_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(focus_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus_container, bt_hid_key_event_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus_container);
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }
    lv_group_focus_obj(focus_container);

    bt_hid_screen = screen;
    return screen;
}

void updateBTHIDStatus(const char* status, bool connected) {
    if (bt_hid_status_label != NULL) {
        lv_label_set_text(bt_hid_status_label, status);
        if (connected) {
            lv_obj_set_style_text_color(bt_hid_status_label, LV_COLOR_SUCCESS, 0);
        } else {
            lv_obj_set_style_text_color(bt_hid_status_label, LV_COLOR_WARNING, 0);
        }
    }
}

void updateBTHIDDeviceName(const char* name) {
    if (bt_hid_device_name_label != NULL) {
        lv_label_set_text(bt_hid_device_name_label, name);
    }
}

void updateBTHIDPaddleIndicators(bool ditPressed, bool dahPressed) {
    if (bt_hid_dit_indicator != NULL) {
        if (ditPressed) {
            lv_led_on(bt_hid_dit_indicator);
        } else {
            lv_led_off(bt_hid_dit_indicator);
        }
    }
    if (bt_hid_dah_indicator != NULL) {
        if (dahPressed) {
            lv_led_on(bt_hid_dah_indicator);
        } else {
            lv_led_off(bt_hid_dah_indicator);
        }
    }
}

void updateBTHIDKeyerMode(const char* mode) {
    if (bt_hid_keyer_label != NULL) {
        char buf[32];
        snprintf(buf, sizeof(buf), "< %s >", mode);
        lv_label_set_text(bt_hid_keyer_label, buf);
    }
}

void updateBTHIDTypingMode(bool enabled) {
    if (bt_hid_typing_label != NULL) {
        lv_label_set_text(bt_hid_typing_label, enabled ? "Typing: ON" : "Typing: OFF");
        lv_obj_set_style_text_color(bt_hid_typing_label,
            enabled ? LV_COLOR_SUCCESS : LV_COLOR_TEXT_SECONDARY, 0);
    }
    if (bt_hid_footer_label != NULL) {
        lv_label_set_text(bt_hid_footer_label, enabled
            ? "Keys are sent to the host    ESC Stop typing"
            : LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " Keyer    " LV_SYMBOL_UP " Typing    Paddle to key    ESC Exit");
    }
}

void cleanupBTHIDScreen() {
    bt_hid_screen = NULL;
    bt_hid_status_label = NULL;
    bt_hid_device_name_label = NULL;
    bt_hid_dit_indicator = NULL;
    bt_hid_dah_indicator = NULL;
    bt_hid_keyer_label = NULL;
    bt_hid_typing_label = NULL;
    bt_hid_footer_label = NULL;
}

// ============================================
// BT MIDI Screen (Mode 34)
// BLE MIDI keyer for Vail Adapter compatible tools.
// The host controls keyer program (Program Change) and speed (CC1).
// ============================================

static lv_obj_t* bt_midi_screen = NULL;
static lv_obj_t* bt_midi_status_label = NULL;
static lv_obj_t* bt_midi_device_name_label = NULL;
static lv_obj_t* bt_midi_keyer_label = NULL;
static lv_obj_t* bt_midi_speed_label = NULL;
static lv_obj_t* bt_midi_key_indicator = NULL;

// Key event callback for BT MIDI (ESC to exit; keyer/speed are host-controlled)
static void bt_midi_key_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_ESC) {
        onLVGLBackNavigation();
        lv_event_stop_processing(e);
    }
}

lv_obj_t* createBTMIDIScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "BT MIDI");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // === Single Centered Card ===
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, 400, 210);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 5);
    applyCardStyle(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 15, 0);

    // Row 1: Bluetooth icon + Device name (top, centered)
    lv_obj_t* name_row = lv_obj_create(card);
    lv_obj_set_size(name_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(name_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(name_row, 0, 0);
    lv_obj_set_style_pad_all(name_row, 0, 0);
    lv_obj_align(name_row, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(name_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* bt_icon = lv_label_create(name_row);
    lv_label_set_text(bt_icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_font(bt_icon, getThemeFonts()->font_large, 0);
    lv_obj_set_style_text_color(bt_icon, LV_COLOR_ACCENT_SECONDARY, 0);
    lv_obj_align(bt_icon, LV_ALIGN_LEFT_MID, 100, 0);

    bt_midi_device_name_label = lv_label_create(name_row);
    lv_label_set_text(bt_midi_device_name_label, "VAIL-SUMMIT-XXXXXX");
    lv_obj_set_style_text_color(bt_midi_device_name_label, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(bt_midi_device_name_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_align(bt_midi_device_name_label, LV_ALIGN_LEFT_MID, 140, 0);

    // Row 2: Connection status (centered, colored)
    bt_midi_status_label = lv_label_create(card);
    lv_label_set_text(bt_midi_status_label, "Advertising...");
    lv_obj_set_style_text_font(bt_midi_status_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(bt_midi_status_label, LV_COLOR_WARNING, 0);
    lv_obj_align(bt_midi_status_label, LV_ALIGN_TOP_MID, 0, 35);

    // Row 3: Keyer program (host-controlled)
    lv_obj_t* keyer_row = lv_obj_create(card);
    lv_obj_set_size(keyer_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(keyer_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(keyer_row, 0, 0);
    lv_obj_set_style_pad_all(keyer_row, 0, 0);
    lv_obj_align(keyer_row, LV_ALIGN_TOP_MID, 0, 62);
    lv_obj_clear_flag(keyer_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* keyer_title = lv_label_create(keyer_row);
    lv_label_set_text(keyer_title, "Keyer:");
    lv_obj_set_style_text_color(keyer_title, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(keyer_title, getThemeFonts()->font_body, 0);
    lv_obj_align(keyer_title, LV_ALIGN_LEFT_MID, 70, 0);

    bt_midi_keyer_label = lv_label_create(keyer_row);
    lv_label_set_text(bt_midi_keyer_label, "Iambic B");
    lv_obj_set_style_text_color(bt_midi_keyer_label, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(bt_midi_keyer_label, getThemeFonts()->font_body, 0);
    lv_obj_align(bt_midi_keyer_label, LV_ALIGN_LEFT_MID, 130, 0);

    lv_obj_t* speed_title = lv_label_create(keyer_row);
    lv_label_set_text(speed_title, "Speed:");
    lv_obj_set_style_text_color(speed_title, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(speed_title, getThemeFonts()->font_body, 0);
    lv_obj_align(speed_title, LV_ALIGN_LEFT_MID, 225, 0);

    bt_midi_speed_label = lv_label_create(keyer_row);
    lv_label_set_text(bt_midi_speed_label, "20 WPM");
    lv_obj_set_style_text_color(bt_midi_speed_label, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(bt_midi_speed_label, getThemeFonts()->font_body, 0);
    lv_obj_align(bt_midi_speed_label, LV_ALIGN_LEFT_MID, 287, 0);

    // Row 4: Hint about host control
    lv_obj_t* hint = lv_label_create(card);
    lv_label_set_text(hint, "Host sets keyer and speed via MIDI (Vail Adapter spec)");
    lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(hint, getThemeFonts()->font_small, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 95);

    // Row 5: Keying LED indicator at bottom
    lv_obj_t* indicator_row = lv_obj_create(card);
    lv_obj_set_size(indicator_row, lv_pct(100), 50);
    lv_obj_set_style_bg_opa(indicator_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(indicator_row, 0, 0);
    lv_obj_set_style_pad_all(indicator_row, 0, 0);
    lv_obj_align(indicator_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(indicator_row, LV_OBJ_FLAG_SCROLLABLE);

    bt_midi_key_indicator = lv_led_create(indicator_row);
    lv_led_set_color(bt_midi_key_indicator, LV_COLOR_SUCCESS);
    lv_obj_set_size(bt_midi_key_indicator, 30, 30);
    lv_obj_align(bt_midi_key_indicator, LV_ALIGN_LEFT_MID, 140, 0);
    lv_led_off(bt_midi_key_indicator);

    lv_obj_t* key_label = lv_label_create(indicator_row);
    lv_label_set_text(key_label, "KEY");
    lv_obj_set_style_text_color(key_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(key_label, getThemeFonts()->font_body, 0);
    lv_obj_align(key_label, LV_ALIGN_LEFT_MID, 180, 0);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, "Pair in computer Bluetooth settings    Paddle to key    ESC Exit");
    lv_obj_set_style_text_color(help, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    // Invisible focus container for keyboard handling
    lv_obj_t* focus_container = lv_obj_create(screen);
    lv_obj_set_size(focus_container, 1, 1);
    lv_obj_set_pos(focus_container, -10, -10);
    lv_obj_set_style_bg_opa(focus_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus_container, 0, 0);
    lv_obj_set_style_outline_width(focus_container, 0, 0);
    lv_obj_set_style_outline_width(focus_container, 0, LV_STATE_FOCUSED);
    lv_obj_clear_flag(focus_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(focus_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus_container, bt_midi_key_event_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus_container);
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }
    lv_group_focus_obj(focus_container);

    bt_midi_screen = screen;
    return screen;
}

void updateBTMIDIStatus(const char* status, bool connected) {
    if (bt_midi_status_label != NULL) {
        lv_label_set_text(bt_midi_status_label, status);
        lv_obj_set_style_text_color(bt_midi_status_label,
            connected ? LV_COLOR_SUCCESS : LV_COLOR_WARNING, 0);
    }
}

void updateBTMIDIDeviceName(const char* name) {
    if (bt_midi_device_name_label != NULL) {
        lv_label_set_text(bt_midi_device_name_label, name);
    }
}

void updateBTMIDIInfo(int wpm, bool fromMidi, const char* keyerName) {
    if (bt_midi_keyer_label != NULL) {
        lv_label_set_text(bt_midi_keyer_label, keyerName);
    }
    if (bt_midi_speed_label != NULL) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%d WPM%s", wpm, fromMidi ? " (MIDI)" : "");
        lv_label_set_text(bt_midi_speed_label, buf);
    }
}

void updateBTMIDIKeyIndicator(bool keying) {
    if (bt_midi_key_indicator != NULL) {
        if (keying) {
            lv_led_on(bt_midi_key_indicator);
        } else {
            lv_led_off(bt_midi_key_indicator);
        }
    }
}

void cleanupBTMIDIScreen() {
    bt_midi_screen = NULL;
    bt_midi_status_label = NULL;
    bt_midi_device_name_label = NULL;
    bt_midi_keyer_label = NULL;
    bt_midi_speed_label = NULL;
    bt_midi_key_indicator = NULL;
}

// ============================================
// QSO Logger Settings Screen (Mode 40)
// Configure location (Grid Square or POTA Park)
// ============================================

static lv_obj_t* logger_settings_screen = NULL;
static lv_obj_t* logger_settings_focus_container = NULL;
static lv_obj_t* logger_mode_row = NULL;
static lv_obj_t* logger_mode_value = NULL;
static lv_obj_t* logger_location_row = NULL;
static lv_obj_t* logger_location_input = NULL;
static lv_obj_t* logger_qth_row = NULL;
static lv_obj_t* logger_qth_input = NULL;
static lv_obj_t* logger_pota_status_card = NULL;
static lv_obj_t* logger_pota_name_label = NULL;
static lv_obj_t* logger_pota_location_label = NULL;
static lv_obj_t* logger_pota_grid_label = NULL;
static lv_obj_t* logger_footer_label = NULL;

static int logger_settings_focus = 0;  // 0=mode, 1=location, 2=qth (grid mode only)
static int logger_location_mode = 0;   // 0=Grid Square, 1=POTA Park

static const char* logger_mode_names[] = {"Grid Square", "POTA Park"};

// Forward declarations from qso_logger_settings.h
extern LoggerSettingsState loggerSettings;
extern void saveLoggerLocation();
extern void loadLoggerLocation();

// Forward declaration from qso_logger_validation.h
extern bool validateGridSquare(const char* grid);
extern bool validatePOTAReference(const char* ref);

// Forward declaration from pota_api.h
extern bool lookupPOTAPark(const char* reference, POTAPark& park);

// Update visual focus indicators for logger settings
static void updateLoggerSettingsFocus() {
    // Mode row styling
    if (logger_settings_focus == 0) {
        lv_obj_set_style_bg_color(logger_mode_row, LV_COLOR_BG_CARD, 0);
        lv_obj_set_style_bg_opa(logger_mode_row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(logger_mode_row, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_set_style_border_width(logger_mode_row, 2, 0);
        lv_obj_set_style_text_color(logger_mode_value, LV_COLOR_ACCENT_PRIMARY, 0);
    } else {
        lv_obj_set_style_bg_color(logger_mode_row, LV_COLOR_BG_LAYER2, 0);
        lv_obj_set_style_bg_opa(logger_mode_row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(logger_mode_row, LV_COLOR_BORDER_SUBTLE, 0);
        lv_obj_set_style_border_width(logger_mode_row, 1, 0);
        lv_obj_set_style_text_color(logger_mode_value, LV_COLOR_TEXT_PRIMARY, 0);
    }

    // Location row styling
    if (logger_settings_focus == 1) {
        lv_obj_set_style_border_color(logger_location_row, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_set_style_border_width(logger_location_row, 2, 0);
    } else {
        lv_obj_set_style_border_color(logger_location_row, LV_COLOR_BORDER_SUBTLE, 0);
        lv_obj_set_style_border_width(logger_location_row, 1, 0);
    }

    // QTH row styling (only visible in Grid mode)
    if (logger_qth_row != NULL && logger_location_mode == 0) {
        if (logger_settings_focus == 2) {
            lv_obj_set_style_border_color(logger_qth_row, LV_COLOR_ACCENT_PRIMARY, 0);
            lv_obj_set_style_border_width(logger_qth_row, 2, 0);
        } else {
            lv_obj_set_style_border_color(logger_qth_row, LV_COLOR_BORDER_SUBTLE, 0);
            lv_obj_set_style_border_width(logger_qth_row, 1, 0);
        }
    }
}

// Update footer text based on current focus and mode
static void updateLoggerSettingsFooter() {
    if (logger_footer_label == NULL) return;

    if (logger_settings_focus == 0) {
        lv_label_set_text(logger_footer_label, "L/R Change Mode   UP/DN Navigate   ESC Back");
    } else if (logger_settings_focus == 1) {
        if (logger_location_mode == 1) { // POTA mode
            lv_label_set_text(logger_footer_label, "Type ref   ENTER Lookup   UP/DN Navigate   ESC Back");
        } else { // Grid mode
            lv_label_set_text(logger_footer_label, "Type grid   UP/DN Navigate   ESC Back (auto-saves)");
        }
    } else if (logger_settings_focus == 2) {
        lv_label_set_text(logger_footer_label, "Type QTH   UP/DN Navigate   ESC Back (auto-saves)");
    }
}

// Update POTA status card visibility and content
static void updateLoggerPOTAStatus() {
    if (logger_pota_status_card == NULL) return;

    if (logger_location_mode == 0) {
        // Grid mode - hide POTA status, show QTH row
        lv_obj_add_flag(logger_pota_status_card, LV_OBJ_FLAG_HIDDEN);
        if (logger_qth_row != NULL) {
            lv_obj_clear_flag(logger_qth_row, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        // POTA mode - show POTA status, hide QTH row
        lv_obj_clear_flag(logger_pota_status_card, LV_OBJ_FLAG_HIDDEN);
        if (logger_qth_row != NULL) {
            lv_obj_add_flag(logger_qth_row, LV_OBJ_FLAG_HIDDEN);
        }

        // Update status card content
        if (loggerSettings.potaLookupDone) {
            if (loggerSettings.potaLookupSuccess && loggerSettings.potaPark.valid) {
                // Success - green card
                lv_obj_set_style_bg_color(logger_pota_status_card, lv_color_hex(0x0A3020), 0);
                lv_obj_set_style_border_color(logger_pota_status_card, LV_COLOR_SUCCESS, 0);
                lv_label_set_text_fmt(logger_pota_name_label, "Park Found: %s", loggerSettings.potaPark.name);
                lv_obj_set_style_text_color(logger_pota_name_label, LV_COLOR_SUCCESS, 0);
                lv_label_set_text_fmt(logger_pota_location_label, "Location: %s", loggerSettings.potaPark.locationDesc);
                lv_label_set_text_fmt(logger_pota_grid_label, "Grid: %s", loggerSettings.potaPark.grid6);
            } else {
                // Failed - red card
                lv_obj_set_style_bg_color(logger_pota_status_card, lv_color_hex(0x300A0A), 0);
                lv_obj_set_style_border_color(logger_pota_status_card, LV_COLOR_ERROR, 0);
                lv_label_set_text(logger_pota_name_label, "Park Not Found");
                lv_obj_set_style_text_color(logger_pota_name_label, LV_COLOR_ERROR, 0);
                lv_label_set_text(logger_pota_location_label, "Check reference or try again");
                lv_label_set_text(logger_pota_grid_label, "");
            }
        } else {
            // Not looked up yet - neutral card
            lv_obj_set_style_bg_color(logger_pota_status_card, LV_COLOR_BG_LAYER2, 0);
            lv_obj_set_style_border_color(logger_pota_status_card, LV_COLOR_BORDER_SUBTLE, 0);
            lv_label_set_text(logger_pota_name_label, "Enter POTA reference and press ENTER");
            lv_obj_set_style_text_color(logger_pota_name_label, LV_COLOR_TEXT_SECONDARY, 0);
            lv_label_set_text(logger_pota_location_label, "Format: US-1234 or K-1234");
            lv_label_set_text(logger_pota_grid_label, "");
        }
    }
}

// Key event handler for logger settings
static void logger_settings_key_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    // ESC - save and exit
    if (key == LV_KEY_ESC) {
        saveLoggerLocation();
        onLVGLBackNavigation();
        lv_event_stop_bubbling(e);
        return;
    }

    // UP/DOWN navigation
    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        if (logger_settings_focus > 0) {
            logger_settings_focus--;
            // In POTA mode, skip QTH field (field 2)
            if (logger_location_mode == 1 && logger_settings_focus == 2) {
                logger_settings_focus = 1;
            }
        }
        updateLoggerSettingsFocus();
        updateLoggerSettingsFooter();
        lv_event_stop_bubbling(e);
        return;
    }

    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        int maxField = (logger_location_mode == 0) ? 2 : 1;  // Grid has 3 fields, POTA has 2
        if (logger_settings_focus < maxField) {
            logger_settings_focus++;
        }
        updateLoggerSettingsFocus();
        updateLoggerSettingsFooter();
        lv_event_stop_bubbling(e);
        return;
    }

    // LEFT/RIGHT - change mode (only on mode row)
    if (logger_settings_focus == 0 && (key == LV_KEY_LEFT || key == LV_KEY_RIGHT)) {
        logger_location_mode = (logger_location_mode + 1) % 2;
        loggerSettings.inputMode = (LocationInputMode)logger_location_mode;
        loggerSettings.potaLookupDone = false;  // Reset lookup on mode change
        lv_label_set_text_fmt(logger_mode_value, "< %s >", logger_mode_names[logger_location_mode]);
        updateLoggerPOTAStatus();
        updateLoggerSettingsFooter();
        lv_event_stop_bubbling(e);
        return;
    }

    // ENTER - lookup POTA park (only on location field in POTA mode)
    if (key == LV_KEY_ENTER && logger_settings_focus == 1 && logger_location_mode == 1) {
        const char* ref = lv_textarea_get_text(logger_location_input);
        strlcpy(loggerSettings.potaInput, ref, sizeof(loggerSettings.potaInput));

        if (validatePOTAReference(loggerSettings.potaInput)) {
            Serial.println("Looking up POTA park...");
            loggerSettings.potaLookupSuccess = lookupPOTAPark(loggerSettings.potaInput, loggerSettings.potaPark);
            loggerSettings.potaLookupDone = true;

            if (loggerSettings.potaLookupSuccess) {
                beep(1000, 100);  // Success beep
                saveLoggerLocation();
            } else {
                beep(600, 100);  // Error beep
            }
            updateLoggerPOTAStatus();
        } else {
            beep(600, 100);  // Invalid format beep
        }
        lv_event_stop_bubbling(e);
        return;
    }

    lv_event_stop_bubbling(e);
}

// Text area event handler to sync input with state
static void logger_location_input_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        const char* text = lv_textarea_get_text(logger_location_input);
        if (logger_location_mode == 0) {
            strlcpy(loggerSettings.gridInput, text, sizeof(loggerSettings.gridInput));
        } else {
            strlcpy(loggerSettings.potaInput, text, sizeof(loggerSettings.potaInput));
            loggerSettings.potaLookupDone = false;  // Reset lookup when text changes
            updateLoggerPOTAStatus();
        }
    }
}

static void logger_qth_input_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        const char* text = lv_textarea_get_text(logger_qth_input);
        strlcpy(loggerSettings.qthInput, text, sizeof(loggerSettings.qthInput));
    }
}

lv_obj_t* createQSOLoggerSettingsScreen() {
    clearNavigationGroup();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Load saved settings
    loadLoggerLocation();
    logger_location_mode = loggerSettings.inputMode;
    logger_settings_focus = 0;

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "LOGGER SETTINGS");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar
    createCompactStatusBar(screen);

    // Content container
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_WIDTH - 40, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 20);
    lv_obj_set_pos(content, 20, HEADER_HEIGHT + 10);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    // Tight spacing: with 10px row gaps + 10px padding the QTH row ran past
    // the bottom of the 320px screen.
    lv_obj_set_style_pad_row(content, 5, 0);
    lv_obj_set_style_pad_all(content, 5, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // Row 1: Location Mode selector
    lv_obj_t* mode_label = lv_label_create(content);
    lv_label_set_text(mode_label, "Location Mode");
    lv_obj_set_style_text_color(mode_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(mode_label, getThemeFonts()->font_small, 0);

    logger_mode_row = lv_obj_create(content);
    lv_obj_set_size(logger_mode_row, lv_pct(100), 40);
    lv_obj_set_style_bg_color(logger_mode_row, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_radius(logger_mode_row, 8, 0);
    lv_obj_set_style_border_width(logger_mode_row, 1, 0);
    lv_obj_set_style_border_color(logger_mode_row, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_pad_all(logger_mode_row, 8, 0);
    lv_obj_clear_flag(logger_mode_row, LV_OBJ_FLAG_SCROLLABLE);

    logger_mode_value = lv_label_create(logger_mode_row);
    lv_label_set_text_fmt(logger_mode_value, "< %s >", logger_mode_names[logger_location_mode]);
    lv_obj_set_style_text_font(logger_mode_value, getThemeFonts()->font_subtitle, 0);
    lv_obj_center(logger_mode_value);

    // Row 2: Location input (Grid or POTA ref)
    lv_obj_t* location_label = lv_label_create(content);
    if (logger_location_mode == 0) {
        lv_label_set_text(location_label, "Grid Square (e.g., EN52wa)");
    } else {
        lv_label_set_text(location_label, "POTA Reference (e.g., US-2256)");
    }
    lv_obj_set_style_text_color(location_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(location_label, getThemeFonts()->font_small, 0);

    logger_location_row = lv_obj_create(content);
    lv_obj_set_size(logger_location_row, lv_pct(100), 50);
    lv_obj_set_style_bg_color(logger_location_row, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_radius(logger_location_row, 8, 0);
    lv_obj_set_style_border_width(logger_location_row, 1, 0);
    lv_obj_set_style_border_color(logger_location_row, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_pad_all(logger_location_row, 5, 0);
    lv_obj_clear_flag(logger_location_row, LV_OBJ_FLAG_SCROLLABLE);

    logger_location_input = lv_textarea_create(logger_location_row);
    lv_obj_set_size(logger_location_input, lv_pct(100), 40);
    lv_textarea_set_one_line(logger_location_input, true);
    lv_textarea_set_max_length(logger_location_input, logger_location_mode == 0 ? 6 : 10);
    if (logger_location_mode == 0) {
        lv_textarea_set_text(logger_location_input, loggerSettings.gridInput);
        lv_textarea_set_placeholder_text(logger_location_input, "EN52wa");
    } else {
        lv_textarea_set_text(logger_location_input, loggerSettings.potaInput);
        lv_textarea_set_placeholder_text(logger_location_input, "US-2256");
    }
    applyTextareaStyle(logger_location_input);
    lv_obj_add_event_cb(logger_location_input, logger_location_input_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(logger_location_input);

    // Row 3: QTH input (Grid mode only)
    lv_obj_t* qth_label = lv_label_create(content);
    lv_label_set_text(qth_label, "QTH (Optional)");
    lv_obj_set_style_text_color(qth_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(qth_label, getThemeFonts()->font_small, 0);

    logger_qth_row = lv_obj_create(content);
    lv_obj_set_size(logger_qth_row, lv_pct(100), 50);
    lv_obj_set_style_bg_color(logger_qth_row, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_radius(logger_qth_row, 8, 0);
    lv_obj_set_style_border_width(logger_qth_row, 1, 0);
    lv_obj_set_style_border_color(logger_qth_row, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_pad_all(logger_qth_row, 5, 0);
    lv_obj_clear_flag(logger_qth_row, LV_OBJ_FLAG_SCROLLABLE);

    logger_qth_input = lv_textarea_create(logger_qth_row);
    lv_obj_set_size(logger_qth_input, lv_pct(100), 40);
    lv_textarea_set_one_line(logger_qth_input, true);
    lv_textarea_set_max_length(logger_qth_input, 40);
    lv_textarea_set_text(logger_qth_input, loggerSettings.qthInput);
    lv_textarea_set_placeholder_text(logger_qth_input, "City, State");
    applyTextareaStyle(logger_qth_input);
    lv_obj_add_event_cb(logger_qth_input, logger_qth_input_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(logger_qth_input);

    // Row 4: POTA status card (POTA mode only)
    logger_pota_status_card = lv_obj_create(content);
    lv_obj_set_size(logger_pota_status_card, lv_pct(100), 70);
    lv_obj_set_style_bg_color(logger_pota_status_card, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_radius(logger_pota_status_card, 8, 0);
    lv_obj_set_style_border_width(logger_pota_status_card, 1, 0);
    lv_obj_set_style_border_color(logger_pota_status_card, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_pad_all(logger_pota_status_card, 8, 0);
    lv_obj_clear_flag(logger_pota_status_card, LV_OBJ_FLAG_SCROLLABLE);

    logger_pota_name_label = lv_label_create(logger_pota_status_card);
    lv_label_set_text(logger_pota_name_label, "");
    lv_obj_set_style_text_font(logger_pota_name_label, getThemeFonts()->font_body, 0);
    lv_obj_align(logger_pota_name_label, LV_ALIGN_TOP_LEFT, 0, 0);

    logger_pota_location_label = lv_label_create(logger_pota_status_card);
    lv_label_set_text(logger_pota_location_label, "");
    lv_obj_set_style_text_font(logger_pota_location_label, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(logger_pota_location_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(logger_pota_location_label, LV_ALIGN_TOP_LEFT, 0, 20);

    logger_pota_grid_label = lv_label_create(logger_pota_status_card);
    lv_label_set_text(logger_pota_grid_label, "");
    lv_obj_set_style_text_font(logger_pota_grid_label, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(logger_pota_grid_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(logger_pota_grid_label, LV_ALIGN_TOP_LEFT, 0, 38);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    logger_footer_label = lv_label_create(footer);
    lv_label_set_text(logger_footer_label, "L/R Change Mode   UP/DN Navigate   ESC Back");
    lv_obj_set_style_text_color(logger_footer_label, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(logger_footer_label, getThemeFonts()->font_small, 0);
    lv_obj_center(logger_footer_label);

    // Invisible focus container for key handling
    logger_settings_focus_container = lv_obj_create(screen);
    lv_obj_set_size(logger_settings_focus_container, 0, 0);
    lv_obj_set_style_bg_opa(logger_settings_focus_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(logger_settings_focus_container, 0, 0);
    lv_obj_add_flag(logger_settings_focus_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(logger_settings_focus_container, logger_settings_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(logger_settings_focus_container);

    // Add textareas to nav group (allows keyboard input to them)
    addNavigableWidget(logger_location_input);
    addNavigableWidget(logger_qth_input);

    // Enable edit mode for direct key handling
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }

    // Initialize display state
    updateLoggerSettingsFocus();
    updateLoggerPOTAStatus();
    updateLoggerSettingsFooter();

    logger_settings_screen = screen;
    return screen;
}

// Reset logger settings static widget pointers on back-navigation
void cleanupQSOLoggerSettingsScreen() {
    logger_settings_screen = NULL;
    logger_settings_focus_container = NULL;
    logger_mode_row = NULL;
    logger_mode_value = NULL;
    logger_location_row = NULL;
    logger_location_input = NULL;
    logger_qth_row = NULL;
    logger_qth_input = NULL;
    logger_pota_status_card = NULL;
    logger_pota_name_label = NULL;
    logger_pota_location_label = NULL;
    logger_pota_grid_label = NULL;
    logger_footer_label = NULL;
}

// ============================================
// QSO Statistics Screen (Mode 39)
// Display QSO logging statistics
// ============================================

static lv_obj_t* qso_stats_screen = NULL;
static lv_obj_t* qso_stats_focus_container = NULL;
static lv_obj_t* qso_stats_scroll_container = NULL;
static lv_obj_t* qso_stats_total_label = NULL;
static lv_obj_t* qso_stats_unique_label = NULL;
static lv_obj_t* qso_stats_active_label = NULL;
static lv_obj_t* qso_stats_last_label = NULL;

// Forward declarations from qso_logger_statistics.h
extern QSOStatistics stats;
extern void calculateStatistics();

// Key event handler for statistics screen (scroll + ESC)
static void qso_stats_key_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ESC) {
        onLVGLBackNavigation();
        lv_event_stop_bubbling(e);
        return;
    }

    // Scroll with UP/DOWN
    if (qso_stats_scroll_container != NULL) {
        if (key == LV_KEY_UP || key == LV_KEY_PREV) {
            lv_obj_scroll_by(qso_stats_scroll_container, 0, 30, LV_ANIM_ON);
            lv_event_stop_bubbling(e);
            return;
        }
        if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
            lv_obj_scroll_by(qso_stats_scroll_container, 0, -30, LV_ANIM_ON);
            lv_event_stop_bubbling(e);
            return;
        }
    }

    lv_event_stop_bubbling(e);
}

// Format date from YYYYMMDD to MM/DD/YY
static void formatDateShort(const char* yyyymmdd, char* out, size_t outSize) {
    if (strlen(yyyymmdd) >= 8) {
        snprintf(out, outSize, "%c%c/%c%c/%c%c",
                 yyyymmdd[4], yyyymmdd[5],   // MM
                 yyyymmdd[6], yyyymmdd[7],   // DD
                 yyyymmdd[2], yyyymmdd[3]);  // YY
    } else {
        strlcpy(out, yyyymmdd, outSize);
    }
}

lv_obj_t* createQSOStatisticsScreen() {
    clearNavigationGroup();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Calculate statistics from QSO files
    calculateStatistics();

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "QSO STATISTICS");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar
    createCompactStatusBar(screen);

    // Scrollable content container
    qso_stats_scroll_container = lv_obj_create(screen);
    lv_obj_set_size(qso_stats_scroll_container, SCREEN_WIDTH - 20, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 10);
    lv_obj_set_pos(qso_stats_scroll_container, 10, HEADER_HEIGHT + 5);
    lv_obj_set_layout(qso_stats_scroll_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(qso_stats_scroll_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(qso_stats_scroll_container, 8, 0);
    lv_obj_set_style_pad_all(qso_stats_scroll_container, 5, 0);
    lv_obj_set_style_bg_opa(qso_stats_scroll_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(qso_stats_scroll_container, 0, 0);
    lv_obj_add_flag(qso_stats_scroll_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(qso_stats_scroll_container, LV_SCROLLBAR_MODE_AUTO);

    if (stats.totalQSOs == 0) {
        // No data message
        lv_obj_t* no_data = lv_label_create(qso_stats_scroll_container);
        lv_label_set_text(no_data, "No QSO data available");
        lv_obj_set_style_text_color(no_data, LV_COLOR_WARNING, 0);
        lv_obj_set_style_text_font(no_data, getThemeFonts()->font_subtitle, 0);
        lv_obj_set_width(no_data, lv_pct(100));
        lv_obj_set_style_text_align(no_data, LV_TEXT_ALIGN_CENTER, 0);
    } else {
        // Stats cards row (2x2 grid)
        lv_obj_t* cards_row = lv_obj_create(qso_stats_scroll_container);
        lv_obj_set_size(cards_row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_layout(cards_row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(cards_row, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_column(cards_row, 10, 0);
        lv_obj_set_style_pad_row(cards_row, 8, 0);
        lv_obj_set_style_pad_all(cards_row, 0, 0);
        lv_obj_set_style_bg_opa(cards_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(cards_row, 0, 0);
        lv_obj_clear_flag(cards_row, LV_OBJ_FLAG_SCROLLABLE);

        // Card 1: Total QSOs
        lv_obj_t* total_card = lv_obj_create(cards_row);
        lv_obj_set_size(total_card, 210, 55);
        applyCardStyle(total_card);
        lv_obj_set_style_pad_all(total_card, 8, 0);
        lv_obj_clear_flag(total_card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* total_title = lv_label_create(total_card);
        lv_label_set_text(total_title, "Total QSOs");
        lv_obj_set_style_text_color(total_title, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(total_title, getThemeFonts()->font_small, 0);
        lv_obj_align(total_title, LV_ALIGN_TOP_LEFT, 0, 0);

        qso_stats_total_label = lv_label_create(total_card);
        lv_label_set_text_fmt(qso_stats_total_label, "%d", stats.totalQSOs);
        lv_obj_set_style_text_color(qso_stats_total_label, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_set_style_text_font(qso_stats_total_label, getThemeFonts()->font_subtitle, 0);
        lv_obj_align(qso_stats_total_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        // Card 2: Unique Callsigns
        lv_obj_t* unique_card = lv_obj_create(cards_row);
        lv_obj_set_size(unique_card, 210, 55);
        applyCardStyle(unique_card);
        lv_obj_set_style_pad_all(unique_card, 8, 0);
        lv_obj_clear_flag(unique_card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* unique_title = lv_label_create(unique_card);
        lv_label_set_text(unique_title, "Unique Calls");
        lv_obj_set_style_text_color(unique_title, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(unique_title, getThemeFonts()->font_small, 0);
        lv_obj_align(unique_title, LV_ALIGN_TOP_LEFT, 0, 0);

        qso_stats_unique_label = lv_label_create(unique_card);
        lv_label_set_text_fmt(qso_stats_unique_label, "%d", stats.uniqueCallsigns);
        lv_obj_set_style_text_color(qso_stats_unique_label, LV_COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(qso_stats_unique_label, getThemeFonts()->font_subtitle, 0);
        lv_obj_align(qso_stats_unique_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        // Card 3: Most Active Date
        lv_obj_t* active_card = lv_obj_create(cards_row);
        lv_obj_set_size(active_card, 210, 55);
        applyCardStyle(active_card);
        lv_obj_set_style_pad_all(active_card, 8, 0);
        lv_obj_clear_flag(active_card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* active_title = lv_label_create(active_card);
        lv_label_set_text(active_title, "Most Active Day");
        lv_obj_set_style_text_color(active_title, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(active_title, getThemeFonts()->font_small, 0);
        lv_obj_align(active_title, LV_ALIGN_TOP_LEFT, 0, 0);

        qso_stats_active_label = lv_label_create(active_card);
        if (strlen(stats.mostActiveDate) > 0) {
            char dateStr[12];
            formatDateShort(stats.mostActiveDate, dateStr, sizeof(dateStr));
            lv_label_set_text_fmt(qso_stats_active_label, "%s (%d)", dateStr, stats.mostActiveDateCount);
        } else {
            lv_label_set_text(qso_stats_active_label, "-");
        }
        lv_obj_set_style_text_color(qso_stats_active_label, LV_COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(qso_stats_active_label, getThemeFonts()->font_body, 0);
        lv_obj_align(qso_stats_active_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        // Card 4: Last QSO
        lv_obj_t* last_card = lv_obj_create(cards_row);
        lv_obj_set_size(last_card, 210, 55);
        applyCardStyle(last_card);
        lv_obj_set_style_pad_all(last_card, 8, 0);
        lv_obj_clear_flag(last_card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* last_title = lv_label_create(last_card);
        lv_label_set_text(last_title, "Last QSO");
        lv_obj_set_style_text_color(last_title, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(last_title, getThemeFonts()->font_small, 0);
        lv_obj_align(last_title, LV_ALIGN_TOP_LEFT, 0, 0);

        qso_stats_last_label = lv_label_create(last_card);
        if (strlen(stats.lastQSODate) > 0) {
            char dateStr[12];
            formatDateShort(stats.lastQSODate, dateStr, sizeof(dateStr));
            lv_label_set_text(qso_stats_last_label, dateStr);
        } else {
            lv_label_set_text(qso_stats_last_label, "-");
        }
        lv_obj_set_style_text_color(qso_stats_last_label, LV_COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(qso_stats_last_label, getThemeFonts()->font_body, 0);
        lv_obj_align(qso_stats_last_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        // Bands section
        if (stats.bandCount > 0) {
            lv_obj_t* bands_section = lv_obj_create(qso_stats_scroll_container);
            lv_obj_set_size(bands_section, lv_pct(100), LV_SIZE_CONTENT);
            lv_obj_set_layout(bands_section, LV_LAYOUT_FLEX);
            lv_obj_set_flex_flow(bands_section, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_row(bands_section, 4, 0);
            lv_obj_set_style_pad_all(bands_section, 8, 0);
            applyCardStyle(bands_section);
            lv_obj_clear_flag(bands_section, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* bands_title = lv_label_create(bands_section);
            lv_label_set_text(bands_title, "Bands");
            lv_obj_set_style_text_color(bands_title, LV_COLOR_ACCENT_PRIMARY, 0);
            lv_obj_set_style_text_font(bands_title, getThemeFonts()->font_body, 0);

            for (int i = 0; i < stats.bandCount && i < 10; i++) {
                lv_obj_t* band_row = lv_obj_create(bands_section);
                lv_obj_set_size(band_row, lv_pct(100), 20);
                lv_obj_set_style_bg_opa(band_row, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(band_row, 0, 0);
                lv_obj_set_style_pad_all(band_row, 0, 0);
                lv_obj_clear_flag(band_row, LV_OBJ_FLAG_SCROLLABLE);

                lv_obj_t* band_name = lv_label_create(band_row);
                lv_label_set_text_fmt(band_name, "%s:", stats.bandStats[i].band);
                lv_obj_set_style_text_font(band_name, getThemeFonts()->font_small, 0);
                lv_obj_align(band_name, LV_ALIGN_LEFT_MID, 0, 0);

                // Bar graph
                int barWidth = (stats.bandStats[i].count * 200) / stats.totalQSOs;
                if (barWidth < 4 && stats.bandStats[i].count > 0) barWidth = 4;

                lv_obj_t* bar = lv_obj_create(band_row);
                lv_obj_set_size(bar, barWidth, 12);
                lv_obj_set_style_bg_color(bar, LV_COLOR_ACCENT_PRIMARY, 0);
                lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
                lv_obj_set_style_radius(bar, 2, 0);
                lv_obj_set_style_border_width(bar, 0, 0);
                lv_obj_align(bar, LV_ALIGN_LEFT_MID, 45, 0);

                lv_obj_t* band_count = lv_label_create(band_row);
                lv_label_set_text_fmt(band_count, "%d", stats.bandStats[i].count);
                lv_obj_set_style_text_font(band_count, getThemeFonts()->font_small, 0);
                lv_obj_align(band_count, LV_ALIGN_LEFT_MID, 50 + barWidth + 5, 0);
            }
        }

        // Modes section
        if (stats.modeCount > 0) {
            lv_obj_t* modes_section = lv_obj_create(qso_stats_scroll_container);
            lv_obj_set_size(modes_section, lv_pct(100), LV_SIZE_CONTENT);
            lv_obj_set_layout(modes_section, LV_LAYOUT_FLEX);
            lv_obj_set_flex_flow(modes_section, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_row(modes_section, 4, 0);
            lv_obj_set_style_pad_all(modes_section, 8, 0);
            applyCardStyle(modes_section);
            lv_obj_clear_flag(modes_section, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* modes_title = lv_label_create(modes_section);
            lv_label_set_text(modes_title, "Modes");
            lv_obj_set_style_text_color(modes_title, LV_COLOR_SUCCESS, 0);
            lv_obj_set_style_text_font(modes_title, getThemeFonts()->font_body, 0);

            for (int i = 0; i < stats.modeCount && i < 8; i++) {
                lv_obj_t* mode_row = lv_obj_create(modes_section);
                lv_obj_set_size(mode_row, lv_pct(100), 20);
                lv_obj_set_style_bg_opa(mode_row, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(mode_row, 0, 0);
                lv_obj_set_style_pad_all(mode_row, 0, 0);
                lv_obj_clear_flag(mode_row, LV_OBJ_FLAG_SCROLLABLE);

                lv_obj_t* mode_name = lv_label_create(mode_row);
                lv_label_set_text_fmt(mode_name, "%s:", stats.modeStats[i].mode);
                lv_obj_set_style_text_font(mode_name, getThemeFonts()->font_small, 0);
                lv_obj_align(mode_name, LV_ALIGN_LEFT_MID, 0, 0);

                // Bar graph
                int barWidth = (stats.modeStats[i].count * 200) / stats.totalQSOs;
                if (barWidth < 4 && stats.modeStats[i].count > 0) barWidth = 4;

                lv_obj_t* bar = lv_obj_create(mode_row);
                lv_obj_set_size(bar, barWidth, 12);
                lv_obj_set_style_bg_color(bar, LV_COLOR_SUCCESS, 0);
                lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
                lv_obj_set_style_radius(bar, 2, 0);
                lv_obj_set_style_border_width(bar, 0, 0);
                lv_obj_align(bar, LV_ALIGN_LEFT_MID, 45, 0);

                lv_obj_t* mode_count = lv_label_create(mode_row);
                lv_label_set_text_fmt(mode_count, "%d", stats.modeStats[i].count);
                lv_obj_set_style_text_font(mode_count, getThemeFonts()->font_small, 0);
                lv_obj_align(mode_count, LV_ALIGN_LEFT_MID, 50 + barWidth + 5, 0);
            }
        }
    }

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* footer_label = lv_label_create(footer);
    lv_label_set_text(footer_label, "UP/DN Scroll   ESC Back");
    lv_obj_set_style_text_color(footer_label, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(footer_label, getThemeFonts()->font_small, 0);
    lv_obj_center(footer_label);

    // Invisible focus container for key handling
    qso_stats_focus_container = lv_obj_create(screen);
    lv_obj_set_size(qso_stats_focus_container, 0, 0);
    lv_obj_set_style_bg_opa(qso_stats_focus_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(qso_stats_focus_container, 0, 0);
    lv_obj_add_flag(qso_stats_focus_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(qso_stats_focus_container, qso_stats_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(qso_stats_focus_container);

    // Enable edit mode
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }

    qso_stats_screen = screen;
    return screen;
}

// Reset QSO statistics static widget pointers on back-navigation
void cleanupQSOStatisticsScreen() {
    qso_stats_screen = NULL;
    qso_stats_focus_container = NULL;
    qso_stats_scroll_container = NULL;
    qso_stats_total_label = NULL;
    qso_stats_unique_label = NULL;
    qso_stats_active_label = NULL;
    qso_stats_last_label = NULL;
}

// ============================================
// QSO View Logs Screen (Mode 38)
// Browse and view saved QSO logs
// ============================================

static lv_obj_t* view_logs_screen = NULL;
static lv_obj_t* view_logs_focus_container = NULL;
static lv_obj_t* view_logs_list_container = NULL;
static lv_obj_t* view_logs_count_label = NULL;
static lv_obj_t** view_logs_rows = NULL;
static int view_logs_row_count = 0;
static int view_logs_selected = 0;
static int view_logs_scroll_offset = 0;

// QSO Detail - index of currently viewed QSO
static int qso_detail_index = -1;

// Deferred popup creation (avoid stack overflow in callbacks)
static int qso_pending_detail_index = -1;  // -1 = no pending, >=0 = show popup for this index

#define VIEW_LOGS_MAX_VISIBLE 6
#define VIEW_LOGS_ROW_HEIGHT 40

// Forward declarations from qso_logger_view.h
extern ViewState viewState;
extern void loadQSOsForView();
extern void freeQSOsFromView();
extern bool deleteCurrentQSO();

// Forward declarations for detail screen
static void qso_detail_key_cb(lv_event_t* e);
lv_obj_t* createQSODetailScreen(int qsoIndex);

// Update row visual styling based on selection
static void updateViewLogsRowStyles() {
    for (int i = 0; i < view_logs_row_count; i++) {
        int qsoIndex = view_logs_scroll_offset + i;
        if (view_logs_rows[i] == NULL) continue;

        if (qsoIndex == view_logs_selected) {
            lv_obj_set_style_bg_color(view_logs_rows[i], LV_COLOR_BG_CARD, 0);
            lv_obj_set_style_border_color(view_logs_rows[i], LV_COLOR_ACCENT_PRIMARY, 0);
            lv_obj_set_style_border_width(view_logs_rows[i], 2, 0);
        } else {
            lv_obj_set_style_bg_color(view_logs_rows[i], LV_COLOR_BG_LAYER2, 0);
            lv_obj_set_style_border_color(view_logs_rows[i], LV_COLOR_BORDER_SUBTLE, 0);
            lv_obj_set_style_border_width(view_logs_rows[i], 1, 0);
        }
    }
}

// Rebuild the visible rows based on scroll offset
static void rebuildViewLogsList();

// ============================================
// QSO Detail Popup (Modal overlay on list screen)
// ============================================

// Static buffer for formatted strings (avoids stack allocation)
static char qso_detail_fmt_buf[256];

// QSO detail popup object (modal on top of list screen)
static lv_obj_t* qso_detail_popup = NULL;

// Forward declarations
static void qso_popup_key_cb(lv_event_t* e);
static void qso_delete_msgbox_cb(lv_event_t* e);
static void qso_delete_msgbox_key_cb(lv_event_t* e);

// Show QSO detail as a popup modal on the current screen
void showQSODetailPopup(int qsoIndex) {
    // Validate input
    if (qsoIndex < 0 || qsoIndex >= viewState.totalQSOs || viewState.qsos == nullptr) {
        return;
    }

    qso_detail_index = qsoIndex;
    QSO& qso = viewState.qsos[qsoIndex];

    // Close any existing popup
    if (qso_detail_popup != NULL) {
        lv_obj_del(qso_detail_popup);
        qso_detail_popup = NULL;
    }

    // Get current screen
    lv_obj_t* scr = lv_scr_act();
    if (scr == NULL) return;

    // Create container with basic styling (no fonts yet)
    qso_detail_popup = lv_obj_create(scr);
    if (qso_detail_popup == NULL) return;

    lv_obj_set_size(qso_detail_popup, 400, 220);
    lv_obj_center(qso_detail_popup);

    // Container styling
    lv_obj_set_style_bg_color(qso_detail_popup, LV_COLOR_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(qso_detail_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(qso_detail_popup, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(qso_detail_popup, 2, 0);
    lv_obj_set_style_radius(qso_detail_popup, 8, 0);
    lv_obj_set_style_pad_all(qso_detail_popup, 15, 0);
    lv_obj_clear_flag(qso_detail_popup, LV_OBJ_FLAG_SCROLLABLE);

    // Build text in a static buffer using snprintf (not LVGL formatting)
    static char popup_text[512];
    snprintf(popup_text, sizeof(popup_text),
        "Callsign: %s\n"
        "Date: %s  Time: %s\n"
        "Freq: %.3f MHz  Band: %s\n"
        "Mode: %s\n"
        "RST Sent: %s  Rcvd: %s\n"
        "\n[D] Delete  [ESC] Back",
        qso.callsign,
        qso.date, qso.time_on,
        qso.frequency, qso.band,
        qso.mode,
        qso.rst_sent, qso.rst_rcvd
    );

    // Create label with QSO content
    lv_obj_t* content = lv_label_create(qso_detail_popup);
    lv_label_set_text(content, popup_text);
    lv_obj_set_style_text_color(content, LV_COLOR_TEXT_PRIMARY, 0);

    // Add key handler
    lv_obj_add_event_cb(qso_detail_popup, qso_popup_key_cb, LV_EVENT_KEY, NULL);

    // Make focusable and add to group
    lv_obj_add_flag(qso_detail_popup, LV_OBJ_FLAG_CLICKABLE);
    lv_group_t* group = getLVGLInputGroup();
    if (group) {
        lv_group_add_obj(group, qso_detail_popup);
        lv_group_focus_obj(qso_detail_popup);
    }
}

// Close the QSO detail popup
void closeQSODetailPopup() {
    if (qso_detail_popup != NULL) {
        // Remove from group before deleting
        lv_group_t* group = getLVGLInputGroup();
        if (group) {
            lv_group_remove_obj(qso_detail_popup);
        }
        lv_obj_del(qso_detail_popup);
        qso_detail_popup = NULL;
    }
    qso_detail_index = -1;
}

// Key handler for QSO detail popup
static void qso_popup_key_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ESC) {
        closeQSODetailPopup();
        lv_event_stop_bubbling(e);
        return;
    }

    if (key == 'D' || key == 'd') {
        // Show delete confirmation
        if (qso_detail_index >= 0 && qso_detail_index < viewState.totalQSOs) {
            const QSO* qso = &viewState.qsos[qso_detail_index];
            snprintf(qso_detail_fmt_buf, sizeof(qso_detail_fmt_buf),
                     "Delete %s?", qso->callsign);

            static const char* btns[] = {"Yes", "No", ""};
            lv_obj_t* mbox = lv_msgbox_create(NULL, "Delete QSO", qso_detail_fmt_buf, btns, false);
            lv_obj_center(mbox);
            lv_obj_add_event_cb(mbox, qso_delete_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);

            // The CardKB is the only input: the msgbox buttons must join the
            // input group and take focus, otherwise Yes/No can never be
            // selected (the focused detail popup swallows every key).
            lv_obj_t* btnm = lv_msgbox_get_btns(mbox);
            lv_obj_add_event_cb(btnm, qso_delete_msgbox_key_cb, LV_EVENT_KEY, mbox);
            lv_group_t* group = getLVGLInputGroup();
            if (group) {
                lv_group_add_obj(group, btnm);
                lv_group_focus_obj(btnm);
                lv_group_set_editing(group, true);  // arrows move between Yes/No
            }
        }
        lv_event_stop_bubbling(e);
        return;
    }

    lv_event_stop_bubbling(e);
}

// Delete confirmation message box callback
static void qso_delete_msgbox_cb(lv_event_t* e) {
    lv_obj_t* mbox = lv_event_get_current_target(e);
    const char* btn_text = lv_msgbox_get_active_btn_text(mbox);

    if (btn_text && strcmp(btn_text, "Yes") == 0) {
        // Perform delete
        viewState.selectedIndex = qso_detail_index;
        bool success = deleteCurrentQSO();

        // Close both the msgbox and the detail popup
        lv_msgbox_close(mbox);
        closeQSODetailPopup();
        lv_group_t* group = getLVGLInputGroup();
        if (group) lv_group_set_editing(group, false);

        if (success) {
            beep(1000, 100);  // Success beep

            // Reload QSO list and rebuild the display
            freeQSOsFromView();
            loadQSOsForView();

            // Adjust selection if needed
            if (view_logs_selected >= viewState.totalQSOs) {
                view_logs_selected = viewState.totalQSOs - 1;
            }
            if (view_logs_selected < 0) view_logs_selected = 0;
            view_logs_scroll_offset = 0;

            // Update the title count and rebuild list
            if (view_logs_count_label) {
                lv_label_set_text_fmt(view_logs_count_label, "VIEW LOGS (%d)", viewState.totalQSOs);
            }
            rebuildViewLogsList();
        } else {
            beep(600, 200);   // Error beep
        }
    } else {
        // Cancel - just close msgbox (keep popup open)
        lv_msgbox_close(mbox);
        lv_group_t* group = getLVGLInputGroup();
        if (group) {
            lv_group_set_editing(group, false);
            if (qso_detail_popup) lv_group_focus_obj(qso_detail_popup);
        }
    }
}

// Key handler on the msgbox button matrix: ESC cancels the dialog and
// returns focus to the QSO detail popup.
static void qso_delete_msgbox_key_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    if (lv_event_get_key(e) != LV_KEY_ESC) return;

    lv_obj_t* mbox = (lv_obj_t*)lv_event_get_user_data(e);
    lv_msgbox_close(mbox);
    lv_group_t* group = getLVGLInputGroup();
    if (group) {
        lv_group_set_editing(group, false);
        if (qso_detail_popup) lv_group_focus_obj(qso_detail_popup);
    }
    lv_event_stop_bubbling(e);
}

// Process any pending QSO detail popup (call from main loop)
void processQSOViewLogsPending() {
    if (qso_pending_detail_index >= 0) {
        int idx = qso_pending_detail_index;
        qso_pending_detail_index = -1;  // Clear flag first
        showQSODetailPopup(idx);
    }
}

// Key event handler for view logs (list screen only - detail is separate screen)
static void view_logs_key_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    // ESC - go back to QSO logger menu
    if (key == LV_KEY_ESC) {
        freeQSOsFromView();
        if (view_logs_rows != NULL) {
            delete[] view_logs_rows;
            view_logs_rows = NULL;
        }
        view_logs_row_count = 0;
        onLVGLBackNavigation();
        lv_event_stop_bubbling(e);
        return;
    }

    // Up arrow - move selection up
    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        if (view_logs_selected > 0) {
            view_logs_selected--;
            if (view_logs_selected < view_logs_scroll_offset) {
                view_logs_scroll_offset = view_logs_selected;
                rebuildViewLogsList();
            } else {
                viewState.selectedIndex = view_logs_selected;
                updateViewLogsRowStyles();
            }
        }
        lv_event_stop_bubbling(e);
        return;
    }

    // Down arrow - move selection down
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        if (view_logs_selected < viewState.totalQSOs - 1) {
            view_logs_selected++;
            if (view_logs_selected >= view_logs_scroll_offset + VIEW_LOGS_MAX_VISIBLE) {
                view_logs_scroll_offset = view_logs_selected - VIEW_LOGS_MAX_VISIBLE + 1;
                rebuildViewLogsList();
            } else {
                viewState.selectedIndex = view_logs_selected;
                updateViewLogsRowStyles();
            }
        }
        lv_event_stop_bubbling(e);
        return;
    }

    // Enter - defer screen creation to avoid stack overflow in callback
    if (key == LV_KEY_ENTER) {
        if (viewState.totalQSOs > 0 && viewState.qsos != nullptr) {
            // Set flag for deferred creation in main loop
            qso_pending_detail_index = view_logs_selected;
        }
        lv_event_stop_bubbling(e);
        return;
    }

    lv_event_stop_bubbling(e);
}

// Rebuild the visible list rows
static void rebuildViewLogsList() {
    // Clear existing rows
    if (view_logs_list_container != NULL) {
        lv_obj_clean(view_logs_list_container);
    }

    // Determine how many rows to show
    int visibleCount = min(VIEW_LOGS_MAX_VISIBLE, viewState.totalQSOs - view_logs_scroll_offset);
    if (visibleCount <= 0) return;

    // Allocate row pointers
    if (view_logs_rows != NULL) {
        delete[] view_logs_rows;
    }
    view_logs_rows = new lv_obj_t*[visibleCount];
    view_logs_row_count = visibleCount;

    for (int i = 0; i < visibleCount; i++) {
        int qsoIndex = view_logs_scroll_offset + i;
        QSO& qso = viewState.qsos[qsoIndex];

        // Create row container
        lv_obj_t* row = lv_obj_create(view_logs_list_container);
        lv_obj_set_size(row, lv_pct(100), VIEW_LOGS_ROW_HEIGHT);
        lv_obj_set_style_bg_color(row, LV_COLOR_BG_LAYER2, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, LV_COLOR_BORDER_SUBTLE, 0);
        lv_obj_set_style_pad_all(row, 6, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        view_logs_rows[i] = row;

        // Date (left)
        char dateStr[12];
        formatDateShort(qso.date, dateStr, sizeof(dateStr));
        lv_obj_t* date_label = lv_label_create(row);
        lv_label_set_text(date_label, dateStr);
        lv_obj_set_style_text_font(date_label, getThemeFonts()->font_small, 0);
        lv_obj_set_style_text_color(date_label, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_align(date_label, LV_ALIGN_LEFT_MID, 0, 0);

        // Callsign (center)
        lv_obj_t* call_label = lv_label_create(row);
        lv_label_set_text(call_label, qso.callsign);
        lv_obj_set_style_text_font(call_label, getThemeFonts()->font_subtitle, 0);
        lv_obj_set_style_text_color(call_label, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_align(call_label, LV_ALIGN_LEFT_MID, 70, 0);

        // Band/Mode (right)
        lv_obj_t* band_label = lv_label_create(row);
        lv_label_set_text_fmt(band_label, "%s %s", qso.band, qso.mode);
        lv_obj_set_style_text_font(band_label, getThemeFonts()->font_small, 0);
        lv_obj_set_style_text_color(band_label, LV_COLOR_WARNING, 0);
        lv_obj_align(band_label, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    viewState.selectedIndex = view_logs_selected;
    updateViewLogsRowStyles();
}

lv_obj_t* createQSOViewLogsScreen() {
    clearNavigationGroup();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Set screen reference early so overlay creation works
    view_logs_screen = screen;

    // Initialize state
    view_logs_selected = 0;
    view_logs_scroll_offset = 0;
    view_logs_rows = NULL;
    view_logs_row_count = 0;
    qso_detail_index = -1;
    qso_pending_detail_index = -1;

    // Load QSOs from storage
    loadQSOsForView();

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    view_logs_count_label = lv_label_create(title_bar);
    lv_label_set_text_fmt(view_logs_count_label, "VIEW LOGS (%d)", viewState.totalQSOs);
    lv_obj_add_style(view_logs_count_label, getStyleLabelTitle(), 0);
    lv_obj_align(view_logs_count_label, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar
    createCompactStatusBar(screen);

    // List container
    view_logs_list_container = lv_obj_create(screen);
    lv_obj_set_size(view_logs_list_container, SCREEN_WIDTH - 20, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 10);
    lv_obj_set_pos(view_logs_list_container, 10, HEADER_HEIGHT + 5);
    lv_obj_set_layout(view_logs_list_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(view_logs_list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(view_logs_list_container, 5, 0);
    lv_obj_set_style_pad_all(view_logs_list_container, 5, 0);
    lv_obj_set_style_bg_opa(view_logs_list_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view_logs_list_container, 0, 0);
    lv_obj_clear_flag(view_logs_list_container, LV_OBJ_FLAG_SCROLLABLE);

    if (viewState.totalQSOs == 0) {
        // No logs message
        lv_obj_t* no_logs = lv_label_create(view_logs_list_container);
        lv_label_set_text(no_logs, "No QSO logs found");
        lv_obj_set_style_text_color(no_logs, LV_COLOR_WARNING, 0);
        lv_obj_set_style_text_font(no_logs, getThemeFonts()->font_subtitle, 0);
        lv_obj_set_width(no_logs, lv_pct(100));
        lv_obj_set_style_text_align(no_logs, LV_TEXT_ALIGN_CENTER, 0);
    } else {
        rebuildViewLogsList();
    }

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* footer_label = lv_label_create(footer);
    lv_label_set_text(footer_label, "UP/DN Select   ENTER View   ESC Back");
    lv_obj_set_style_text_color(footer_label, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(footer_label, getThemeFonts()->font_small, 0);
    lv_obj_center(footer_label);

    // Invisible focus container for key handling
    view_logs_focus_container = lv_obj_create(screen);
    lv_obj_set_size(view_logs_focus_container, 0, 0);
    lv_obj_set_style_bg_opa(view_logs_focus_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view_logs_focus_container, 0, 0);
    lv_obj_add_flag(view_logs_focus_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(view_logs_focus_container, view_logs_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(view_logs_focus_container);

    // Enable edit mode
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }

    return screen;
}

// Reset view-logs state on back-navigation. Idempotent: the ESC key handler
// also frees the row array and QSO buffer before navigating, so every guard
// here must tolerate already-freed state.
void cleanupQSOViewLogsScreen() {
    closeQSODetailPopup();
    if (view_logs_rows != NULL) {
        delete[] view_logs_rows;
        view_logs_rows = NULL;
    }
    view_logs_row_count = 0;
    qso_pending_detail_index = -1;
    freeQSOsFromView();
    view_logs_screen = NULL;
    view_logs_focus_container = NULL;
    view_logs_list_container = NULL;
    view_logs_count_label = NULL;
}

// ============================================
// Screen Selector
// ============================================

lv_obj_t* createModeScreenForMode(int mode) {
    switch (mode) {
        case MODE_RADIO_OUTPUT:
            return createRadioOutputScreen();
        case MODE_CW_MEMORIES:
            return createCWMemoriesScreen();
        case MODE_VAIL_REPEATER:
            return createVailRepeaterScreen();
        case MODE_BT_HID:
            return createBTHIDScreen();
        case MODE_BT_MIDI:
            return createBTMIDIScreen();
        case MODE_QSO_LOG_ENTRY:
            return createQSOLogEntryScreen();
        case MODE_QSO_VIEW_LOGS:
            return createQSOViewLogsScreen();
        case MODE_QSO_STATISTICS:
            return createQSOStatisticsScreen();
        case MODE_QSO_LOGGER_SETTINGS:
            return createQSOLoggerSettingsScreen();
        case MODE_WEB_PRACTICE:
            return createWebPracticeModeScreen();
        case MODE_WEB_MEMORY_CHAIN:
            return createWebMemoryChainModeScreen();
        case MODE_WEB_HEAR_IT:
            return createWebHearItModeScreen();
        default:
            Serial.printf("[ModeScreens] Unknown mode: %d\n", mode);
            return NULL;
    }
}

#endif // LV_MODE_SCREENS_H
