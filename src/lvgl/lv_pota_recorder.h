/*
 * VAIL SUMMIT - POTA Recorder Screens
 * Setup and recording screens for POTA QSO logging
 */

#ifndef LV_POTA_RECORDER_H
#define LV_POTA_RECORDER_H

#include <lvgl.h>
#include "lv_theme_summit.h"
#include "lv_widgets_summit.h"
#include "lv_screen_manager.h"
#include "../pota/pota_recorder.h"
#include "../pota/pota_qso_parser.h"
#include "../radio/radio_output.h"
#include "../settings/settings_cw.h"
#include "../core/config.h"

// Forward declarations
extern void onLVGLMenuSelect(int target_mode);
extern int getCurrentModeAsInt();
extern void setCurrentModeFromInt(int mode);
extern void onLVGLBackNavigation();

// Mode constants
#define POTA_RECORDER_MODE_SETUP    137
#define POTA_RECORDER_MODE_ACTIVE   136

// ============================================
// Screen State
// ============================================

static lv_obj_t* pota_rec_screen = NULL;

// Setup screen widgets
static lv_obj_t* pota_rec_callsign_input = NULL;
static lv_obj_t* pota_rec_park_input = NULL;
static lv_obj_t* pota_rec_start_btn = NULL;
static lv_obj_t* pota_rec_keyer_warning = NULL;

// Active recording screen widgets
static lv_obj_t* pota_rec_text_area = NULL;
static lv_obj_t* pota_rec_call_label = NULL;
static lv_obj_t* pota_rec_rst_label = NULL;
static lv_obj_t* pota_rec_qth_label = NULL;
static lv_obj_t* pota_rec_state_label = NULL;
static lv_obj_t* pota_rec_count_label = NULL;
static lv_obj_t* pota_rec_time_label = NULL;
static lv_obj_t* pota_rec_stop_btn = NULL;

// Update timer
static lv_timer_t* pota_rec_timer = NULL;

// ============================================
// Forward Declarations
// ============================================

lv_obj_t* createPOTARecorderSetupScreen();
lv_obj_t* createPOTARecorderScreen();
void cleanupPOTARecorderScreen();
static void pota_rec_update_cb(lv_timer_t* timer);

// ============================================
// Helper Functions
// ============================================

static void pota_rec_keyboard_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* kb = (lv_obj_t*)lv_event_get_user_data(e);
    lv_obj_t* ta = lv_keyboard_get_textarea(kb);

    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(ta, LV_STATE_FOCUSED);
    }
}

static void pota_rec_ta_focus_cb(lv_event_t* e) {
    lv_obj_t* ta = lv_event_get_target(e);
    lv_obj_t* kb = (lv_obj_t*)lv_event_get_user_data(e);

    if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================================
// Setup Screen
// ============================================

static void pota_rec_start_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    // Get values from inputs
    const char* callsign = lv_textarea_get_text(pota_rec_callsign_input);
    const char* park = lv_textarea_get_text(pota_rec_park_input);

    // Validate
    if (strlen(callsign) < 3) {
        beep(TONE_ERROR, BEEP_LONG);
        return;
    }
    if (strlen(park) < 5) {
        beep(TONE_ERROR, BEEP_LONG);
        return;
    }

    // Save settings
    setPOTACallsign(callsign);
    setPOTAPark(park);
    savePOTASettings();

    // Check radio keyer mode
    if (radioMode != RADIO_MODE_SUMMIT_KEYER) {
        // Show warning and offer to switch
        static const char* btns[] = {"Switch Mode", "Cancel", ""};
        lv_obj_t* msgbox = lv_msgbox_create(NULL, "Keyer Mode",
            "POTA Recorder requires Summit Keyer mode.\n"
            "Switch to Summit Keyer now?", btns, false);
        lv_obj_center(msgbox);
        lv_obj_add_style(msgbox, getStyleMsgbox(), 0);

        lv_obj_add_event_cb(msgbox, [](lv_event_t* e) {
            uint16_t btn = lv_msgbox_get_active_btn((lv_obj_t*)lv_event_get_current_target(e));
            if (btn == 0) {
                // Switch to Summit Keyer
                radioMode = RADIO_MODE_SUMMIT_KEYER;
                saveRadioSettings();
                beep(TONE_SUCCESS, BEEP_MEDIUM);

                // Navigate to recorder
                onLVGLMenuSelect(POTA_RECORDER_MODE_ACTIVE);
            }
            lv_msgbox_close((lv_obj_t*)lv_event_get_current_target(e));
        }, LV_EVENT_VALUE_CHANGED, NULL);

        lv_obj_t* btns_obj = lv_msgbox_get_btns(msgbox);
        addNavigableWidget(btns_obj);
        return;
    }

    beep(TONE_SUCCESS, BEEP_MEDIUM);

    // Navigate to active recording screen
    onLVGLMenuSelect(POTA_RECORDER_MODE_ACTIVE);
}

static void pota_rec_setup_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ESC) {
        onLVGLBackNavigation();
        lv_event_stop_processing(e);
    }
}

lv_obj_t* createPOTARecorderSetupScreen() {
    clearNavigationGroup();

    // Initialize recorder if needed
    initPOTARecorder();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    pota_rec_screen = screen;

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_add_style(header, getStyleStatusBar(), 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "POTA Recorder Setup");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    createCompactStatusBar(screen);

    // Content area
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_WIDTH - 20, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 20);
    lv_obj_set_pos(content, 10, HEADER_HEIGHT + 10);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 8, 0);

    // Callsign input
    lv_obj_t* call_row = lv_obj_create(content);
    lv_obj_set_size(call_row, SCREEN_WIDTH - 40, 50);
    lv_obj_set_style_bg_opa(call_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(call_row, 0, 0);
    lv_obj_set_style_pad_all(call_row, 0, 0);

    lv_obj_t* call_label = lv_label_create(call_row);
    lv_label_set_text(call_label, "My Callsign:");
    lv_obj_set_style_text_font(call_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(call_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(call_label, LV_ALIGN_TOP_LEFT, 0, 0);

    pota_rec_callsign_input = lv_textarea_create(call_row);
    lv_textarea_set_one_line(pota_rec_callsign_input, true);
    lv_textarea_set_max_length(pota_rec_callsign_input, 10);
    lv_textarea_set_placeholder_text(pota_rec_callsign_input, "W1ABC");
    lv_obj_set_size(pota_rec_callsign_input, 150, 35);
    lv_obj_align(pota_rec_callsign_input, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_text_font(pota_rec_callsign_input, &lv_font_montserrat_16, 0);

    // Pre-fill with saved value
    if (strlen(getPOTACallsign()) > 0) {
        lv_textarea_set_text(pota_rec_callsign_input, getPOTACallsign());
    }

    // Park reference input
    lv_obj_t* park_row = lv_obj_create(content);
    lv_obj_set_size(park_row, SCREEN_WIDTH - 40, 50);
    lv_obj_set_style_bg_opa(park_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(park_row, 0, 0);
    lv_obj_set_style_pad_all(park_row, 0, 0);

    lv_obj_t* park_label = lv_label_create(park_row);
    lv_label_set_text(park_label, "Park Reference:");
    lv_obj_set_style_text_font(park_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(park_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(park_label, LV_ALIGN_TOP_LEFT, 0, 0);

    pota_rec_park_input = lv_textarea_create(park_row);
    lv_textarea_set_one_line(pota_rec_park_input, true);
    lv_textarea_set_max_length(pota_rec_park_input, 10);
    lv_textarea_set_placeholder_text(pota_rec_park_input, "K-1234");
    lv_obj_set_size(pota_rec_park_input, 150, 35);
    lv_obj_align(pota_rec_park_input, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_text_font(pota_rec_park_input, &lv_font_montserrat_16, 0);

    // Pre-fill with saved value
    if (strlen(getPOTAPark()) > 0) {
        lv_textarea_set_text(pota_rec_park_input, getPOTAPark());
    }

    // Keyer mode warning
    if (radioMode != RADIO_MODE_SUMMIT_KEYER) {
        pota_rec_keyer_warning = lv_label_create(content);
        lv_label_set_text(pota_rec_keyer_warning, LV_SYMBOL_WARNING " Radio Keyer mode - will switch to Summit Keyer");
        lv_obj_set_style_text_font(pota_rec_keyer_warning, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(pota_rec_keyer_warning, lv_color_make(255, 165, 0), 0);  // Orange
    }

    // Start button
    pota_rec_start_btn = lv_btn_create(content);
    lv_obj_set_size(pota_rec_start_btn, 180, 50);
    lv_obj_set_style_bg_color(pota_rec_start_btn, LV_COLOR_ACCENT_GREEN, 0);
    lv_obj_set_style_bg_color(pota_rec_start_btn, LV_COLOR_ACCENT_CYAN, LV_STATE_FOCUSED);

    lv_obj_t* btn_label = lv_label_create(pota_rec_start_btn);
    lv_label_set_text(btn_label, LV_SYMBOL_PLAY " Start Recording");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_16, 0);
    lv_obj_center(btn_label);

    // Create on-screen keyboard (hidden initially)
    lv_obj_t* kb = lv_keyboard_create(screen);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(kb, SCREEN_WIDTH, 140);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Connect keyboard to text areas
    lv_obj_add_event_cb(pota_rec_callsign_input, pota_rec_ta_focus_cb, LV_EVENT_FOCUSED, kb);
    lv_obj_add_event_cb(pota_rec_park_input, pota_rec_ta_focus_cb, LV_EVENT_FOCUSED, kb);
    lv_obj_add_event_cb(kb, pota_rec_keyboard_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, pota_rec_keyboard_cb, LV_EVENT_CANCEL, NULL);

    // Add text areas to navigation FIRST (order: callsign -> park -> start button)
    lv_obj_add_event_cb(pota_rec_callsign_input, pota_rec_setup_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(pota_rec_callsign_input, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(pota_rec_callsign_input);

    lv_obj_add_event_cb(pota_rec_park_input, pota_rec_setup_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(pota_rec_park_input, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(pota_rec_park_input);

    // Add start button to navigation LAST
    lv_obj_add_event_cb(pota_rec_start_btn, pota_rec_start_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(pota_rec_start_btn, pota_rec_setup_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(pota_rec_start_btn, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(pota_rec_start_btn);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_color(footer, getThemeColors()->bg_deep, 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* footer_text = lv_label_create(footer);
    lv_label_set_text(footer_text, LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate   ENTER Select   ESC Back");
    lv_obj_set_style_text_font(footer_text, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(footer_text, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(footer_text, LV_ALIGN_CENTER, 0, 0);

    return screen;
}

// ============================================
// Active Recording Screen
// ============================================

static void pota_rec_stop_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    stopPOTARecorder();
    beep(TONE_SELECT, BEEP_MEDIUM);
    onLVGLBackNavigation();
}

static void pota_rec_active_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ESC) {
        stopPOTARecorder();
        onLVGLBackNavigation();
        lv_event_stop_processing(e);
    }
}

static void pota_rec_update_cb(lv_timer_t* timer) {
    if (!isPOTARecorderActive()) return;

    // Update decoded text
    if (pota_rec_text_area) {
        const char* text = getPOTADecodedText();
        lv_textarea_set_text(pota_rec_text_area, text);
        // Scroll to end
        lv_textarea_set_cursor_pos(pota_rec_text_area, LV_TEXTAREA_CURSOR_LAST);
    }

    // Update QSO info from parser
    POTAQSOParser* parser = getPOTAParser();
    if (parser) {
        if (pota_rec_call_label) {
            const char* call = parser->getCurrentCallsign();
            if (call && strlen(call) > 0) {
                lv_label_set_text(pota_rec_call_label, call);
            } else {
                lv_label_set_text(pota_rec_call_label, "---");
            }
        }

        if (pota_rec_rst_label) {
            const char* rst = parser->getCurrentRST();
            if (rst && strlen(rst) > 0) {
                lv_label_set_text_fmt(pota_rec_rst_label, "RST: %s", rst);
            } else {
                lv_label_set_text(pota_rec_rst_label, "RST: ---");
            }
        }

        if (pota_rec_qth_label) {
            const char* qth = parser->getCurrentState();
            if (qth && strlen(qth) > 0) {
                lv_label_set_text_fmt(pota_rec_qth_label, "QTH: %s", qth);
            } else {
                lv_label_set_text(pota_rec_qth_label, "QTH: ---");
            }
        }

        if (pota_rec_state_label) {
            lv_label_set_text(pota_rec_state_label, parser->getStateString());

            // Color based on state
            POTAQSOState state = parser->getState();
            if (state == POTAQSOState::QSO_COMPLETE) {
                lv_obj_set_style_text_color(pota_rec_state_label, LV_COLOR_ACCENT_GREEN, 0);
            } else if (state == POTAQSOState::IDLE) {
                lv_obj_set_style_text_color(pota_rec_state_label, LV_COLOR_TEXT_TERTIARY, 0);
            } else {
                lv_obj_set_style_text_color(pota_rec_state_label, LV_COLOR_ACCENT_CYAN, 0);
            }
        }
    }

    // Update session stats
    if (pota_rec_count_label) {
        lv_label_set_text_fmt(pota_rec_count_label, "QSOs: %d", getPOTASessionQSOCount());
    }

    if (pota_rec_time_label) {
        unsigned long dur = getPOTASessionDuration();
        int mins = dur / 60;
        int secs = dur % 60;
        lv_label_set_text_fmt(pota_rec_time_label, "%02d:%02d", mins, secs);
    }
}

lv_obj_t* createPOTARecorderScreen() {
    clearNavigationGroup();

    // Start recording
    startPOTARecorder();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    pota_rec_screen = screen;

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_add_style(header, getStyleStatusBar(), 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text_fmt(title, LV_SYMBOL_AUDIO " %s", getPOTAPark());
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Recording indicator
    lv_obj_t* rec_indicator = lv_label_create(header);
    lv_label_set_text(rec_indicator, LV_SYMBOL_OK " REC");
    lv_obj_set_style_text_color(rec_indicator, lv_color_make(255, 80, 80), 0);  // Red
    lv_obj_set_style_text_font(rec_indicator, &lv_font_montserrat_14, 0);
    lv_obj_align(rec_indicator, LV_ALIGN_RIGHT_MID, -80, 0);

    createCompactStatusBar(screen);

    // Decoded text area
    pota_rec_text_area = lv_textarea_create(screen);
    lv_obj_set_size(pota_rec_text_area, SCREEN_WIDTH - 20, 60);
    lv_obj_set_pos(pota_rec_text_area, 10, HEADER_HEIGHT + 5);
    lv_textarea_set_placeholder_text(pota_rec_text_area, "Decoded CW will appear here...");
    lv_obj_set_style_text_font(pota_rec_text_area, &lv_font_montserrat_14, 0);
    lv_obj_clear_flag(pota_rec_text_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(pota_rec_text_area, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    // QSO info panel
    lv_obj_t* qso_panel = lv_obj_create(screen);
    lv_obj_set_size(qso_panel, SCREEN_WIDTH - 20, 80);
    lv_obj_set_pos(qso_panel, 10, HEADER_HEIGHT + 70);
    applyCardStyle(qso_panel);
    lv_obj_clear_flag(qso_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Callsign (large, yellow)
    pota_rec_call_label = lv_label_create(qso_panel);
    lv_label_set_text(pota_rec_call_label, "---");
    lv_obj_set_style_text_font(pota_rec_call_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(pota_rec_call_label, lv_color_make(255, 220, 50), 0);  // Yellow
    lv_obj_align(pota_rec_call_label, LV_ALIGN_LEFT_MID, 15, -10);

    // RST
    pota_rec_rst_label = lv_label_create(qso_panel);
    lv_label_set_text(pota_rec_rst_label, "RST: ---");
    lv_obj_set_style_text_font(pota_rec_rst_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pota_rec_rst_label, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(pota_rec_rst_label, LV_ALIGN_LEFT_MID, 15, 20);

    // QTH
    pota_rec_qth_label = lv_label_create(qso_panel);
    lv_label_set_text(pota_rec_qth_label, "QTH: ---");
    lv_obj_set_style_text_font(pota_rec_qth_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pota_rec_qth_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(pota_rec_qth_label, LV_ALIGN_RIGHT_MID, -15, 20);

    // Parser state
    pota_rec_state_label = lv_label_create(qso_panel);
    lv_label_set_text(pota_rec_state_label, "IDLE");
    lv_obj_set_style_text_font(pota_rec_state_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pota_rec_state_label, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(pota_rec_state_label, LV_ALIGN_RIGHT_MID, -15, -10);

    // Session stats panel
    lv_obj_t* stats_panel = lv_obj_create(screen);
    lv_obj_set_size(stats_panel, SCREEN_WIDTH - 20, 40);
    lv_obj_set_pos(stats_panel, 10, HEADER_HEIGHT + 155);
    lv_obj_set_style_bg_opa(stats_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stats_panel, 0, 0);
    lv_obj_clear_flag(stats_panel, LV_OBJ_FLAG_SCROLLABLE);

    pota_rec_count_label = lv_label_create(stats_panel);
    lv_label_set_text(pota_rec_count_label, "QSOs: 0");
    lv_obj_set_style_text_font(pota_rec_count_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(pota_rec_count_label, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(pota_rec_count_label, LV_ALIGN_LEFT_MID, 10, 0);

    pota_rec_time_label = lv_label_create(stats_panel);
    lv_label_set_text(pota_rec_time_label, "00:00");
    lv_obj_set_style_text_font(pota_rec_time_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(pota_rec_time_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(pota_rec_time_label, LV_ALIGN_RIGHT_MID, -10, 0);

    // Stop button
    pota_rec_stop_btn = lv_btn_create(screen);
    lv_obj_set_size(pota_rec_stop_btn, 180, 45);
    lv_obj_set_pos(pota_rec_stop_btn, (SCREEN_WIDTH - 180) / 2, SCREEN_HEIGHT - FOOTER_HEIGHT - 55);
    lv_obj_set_style_bg_color(pota_rec_stop_btn, lv_color_make(180, 60, 60), 0);  // Red
    lv_obj_set_style_bg_color(pota_rec_stop_btn, lv_color_make(255, 165, 0), LV_STATE_FOCUSED);  // Orange

    lv_obj_t* stop_label = lv_label_create(pota_rec_stop_btn);
    lv_label_set_text(stop_label, LV_SYMBOL_STOP " Stop Recording");
    lv_obj_set_style_text_font(stop_label, &lv_font_montserrat_14, 0);
    lv_obj_center(stop_label);

    lv_obj_add_event_cb(pota_rec_stop_btn, pota_rec_stop_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(pota_rec_stop_btn, pota_rec_active_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(pota_rec_stop_btn, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(pota_rec_stop_btn);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_color(footer, getThemeColors()->bg_deep, 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* footer_text = lv_label_create(footer);
    lv_label_set_text(footer_text, "Key CW normally - QSOs auto-log on 73");
    lv_obj_set_style_text_font(footer_text, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(footer_text, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(footer_text, LV_ALIGN_CENTER, 0, 0);

    // Start update timer
    if (pota_rec_timer) {
        lv_timer_del(pota_rec_timer);
    }
    pota_rec_timer = lv_timer_create(pota_rec_update_cb, 200, NULL);

    return screen;
}

void cleanupPOTARecorderScreen() {
    if (pota_rec_timer) {
        lv_timer_del(pota_rec_timer);
        pota_rec_timer = NULL;
    }
    pota_rec_screen = NULL;
    pota_rec_callsign_input = NULL;
    pota_rec_park_input = NULL;
    pota_rec_start_btn = NULL;
    pota_rec_keyer_warning = NULL;
    pota_rec_text_area = NULL;
    pota_rec_call_label = NULL;
    pota_rec_rst_label = NULL;
    pota_rec_qth_label = NULL;
    pota_rec_state_label = NULL;
    pota_rec_count_label = NULL;
    pota_rec_time_label = NULL;
    pota_rec_stop_btn = NULL;
}

#endif // LV_POTA_RECORDER_H
