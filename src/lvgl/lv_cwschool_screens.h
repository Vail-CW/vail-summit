/*
 * VAIL SUMMIT - CW School LVGL Screens
 * Device linking and account management for CW School integration
 */

#ifndef LV_CWSCHOOL_SCREENS_H
#define LV_CWSCHOOL_SCREENS_H

#include <lvgl.h>
#include "lv_theme_summit.h"
#include "lv_widgets_summit.h"
#include "lv_screen_manager.h"
#include "../core/config.h"
#include "../network/cwschool_link.h"
#include "../network/internet_check.h"

// Forward declaration for mode switching
extern void setCurrentModeFromInt(int mode);

// ============================================
// Screen State
// ============================================

// Link screen state
static lv_obj_t* cwschool_link_screen = NULL;
static lv_obj_t* cwschool_code_label = NULL;
static lv_obj_t* cwschool_status_label = NULL;
static lv_obj_t* cwschool_timer_label = NULL;
static lv_timer_t* cwschool_link_timer = NULL;

// Account screen state
static lv_obj_t* cwschool_account_screen = NULL;

// ============================================
// Linear Navigation Handler
// ============================================

static void cwschool_linear_nav_handler(lv_event_t* e) {
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
// Device Linking Screen
// ============================================

// Timer callback for link polling
static void cwschool_link_timer_cb(lv_timer_t* timer) {
    Serial.println("[CWSchool] Link timer callback fired");

    // Check link state
    int result = checkCWSchoolDeviceCode();
    Serial.printf("[CWSchool] checkDeviceCode returned: %d, state: %d\n", result, (int)getCWSchoolLinkState());

    // Update timer display
    int remaining = getCWSchoolLinkRemainingSeconds();
    if (remaining > 0 && cwschool_timer_label) {
        char buf[32];
        int mins = remaining / 60;
        int secs = remaining % 60;
        snprintf(buf, sizeof(buf), "Expires in %d:%02d", mins, secs);
        lv_label_set_text(cwschool_timer_label, buf);
    }

    // Update status based on result
    if (cwschool_status_label) {
        CWSchoolLinkState state = getCWSchoolLinkState();
        switch (state) {
            case CWSCHOOL_LINK_WAITING_FOR_USER:
                lv_label_set_text(cwschool_status_label, "Waiting for link...");
                lv_obj_set_style_text_color(cwschool_status_label, LV_COLOR_WARNING, 0);
                break;
            case CWSCHOOL_LINK_CHECKING:
                lv_label_set_text(cwschool_status_label, "Checking...");
                lv_obj_set_style_text_color(cwschool_status_label, LV_COLOR_ACCENT_CYAN, 0);
                break;
            case CWSCHOOL_LINK_EXCHANGING_TOKEN:
                lv_label_set_text(cwschool_status_label, "Linking account...");
                lv_obj_set_style_text_color(cwschool_status_label, LV_COLOR_ACCENT_CYAN, 0);
                break;
            case CWSCHOOL_LINK_SUCCESS: {
                Serial.println("[CWSchool] SUCCESS state - stopping timer and navigating");
                char successBuf[64];
                snprintf(successBuf, sizeof(successBuf), "Linked as %s!", getCWSchoolAccountDisplay().c_str());
                lv_label_set_text(cwschool_status_label, successBuf);
                lv_obj_set_style_text_color(cwschool_status_label, LV_COLOR_SUCCESS, 0);
                // Stop timer
                if (cwschool_link_timer) {
                    Serial.println("[CWSchool] Deleting link timer (SUCCESS)");
                    lv_timer_del(cwschool_link_timer);
                    cwschool_link_timer = NULL;
                }
                // Navigate to account screen after 2 seconds
                lv_timer_create([](lv_timer_t* t) {
                    setCurrentModeFromInt(152);  // MODE_CWSCHOOL_ACCOUNT
                    lv_timer_del(t);
                }, 2000, NULL);
                break;
            }
            case CWSCHOOL_LINK_EXPIRED:
                Serial.println("[CWSchool] EXPIRED state - stopping timer");
                lv_label_set_text(cwschool_status_label, "Code expired. Press ENTER to retry.");
                lv_obj_set_style_text_color(cwschool_status_label, LV_COLOR_ERROR, 0);
                if (cwschool_link_timer) {
                    Serial.println("[CWSchool] Deleting link timer (EXPIRED)");
                    lv_timer_del(cwschool_link_timer);
                    cwschool_link_timer = NULL;
                }
                break;
            case CWSCHOOL_LINK_ERROR: {
                Serial.printf("[CWSchool] ERROR state: %s\n", getCWSchoolLinkError().c_str());
                String errMsg = "Error: " + getCWSchoolLinkError();
                lv_label_set_text(cwschool_status_label, errMsg.c_str());
                lv_obj_set_style_text_color(cwschool_status_label, LV_COLOR_ERROR, 0);
                if (cwschool_link_timer) {
                    Serial.println("[CWSchool] Deleting link timer (ERROR)");
                    lv_timer_del(cwschool_link_timer);
                    cwschool_link_timer = NULL;
                }
                break;
            }
            default:
                break;
        }
    }
}

// Handle ENTER key to retry on error/expired
static void cwschool_link_key_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_ENTER) {
        CWSchoolLinkState state = getCWSchoolLinkState();
        if (state == CWSCHOOL_LINK_EXPIRED || state == CWSCHOOL_LINK_ERROR) {
            // Retry - request new code
            resetCWSchoolLinkState();
            if (requestCWSchoolDeviceCode()) {
                // Update display
                if (cwschool_code_label) {
                    lv_label_set_text(cwschool_code_label, getCWSchoolLinkCode().c_str());
                }
                if (cwschool_status_label) {
                    lv_label_set_text(cwschool_status_label, "Waiting for link...");
                    lv_obj_set_style_text_color(cwschool_status_label, LV_COLOR_WARNING, 0);
                }
                // Restart timer
                if (cwschool_link_timer) {
                    lv_timer_del(cwschool_link_timer);
                }
                cwschool_link_timer = lv_timer_create(cwschool_link_timer_cb, 5000, NULL);
            }
        }
    }
}

/*
 * Create device linking screen
 */
lv_obj_t* createCWSchoolLinkScreen() {
    // Check internet first
    if (getInternetStatus() != INET_CONNECTED) {
        // Show error screen
        lv_obj_t* screen = createScreen();
        applyScreenStyle(screen);

        lv_obj_t* content = lv_obj_create(screen);
        lv_obj_set_size(content, 400, 200);
        lv_obj_center(content);
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(content, 0, 0);
        lv_obj_set_style_pad_row(content, 15, 0);

        lv_obj_t* icon = lv_label_create(content);
        lv_label_set_text(icon, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(icon, LV_COLOR_WARNING, 0);

        lv_obj_t* msg = lv_label_create(content);
        lv_label_set_text(msg, "No Internet Connection");
        lv_obj_set_style_text_font(msg, getThemeFonts()->font_title, 0);
        lv_obj_set_style_text_color(msg, LV_COLOR_TEXT_PRIMARY, 0);

        lv_obj_t* hint = lv_label_create(content);
        lv_label_set_text(hint, "Connect to WiFi first, then try again");
        lv_obj_set_style_text_font(hint, getThemeFonts()->font_body, 0);
        lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_SECONDARY, 0);

        // Invisible focusable for ESC
        lv_obj_t* focus = lv_obj_create(screen);
        lv_obj_set_size(focus, 1, 1);
        lv_obj_set_style_bg_opa(focus, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(focus, 0, 0);
        lv_obj_add_flag(focus, LV_OBJ_FLAG_CLICKABLE);
        addNavigableWidget(focus);

        return screen;
    }

    // Request device code
    if (!requestCWSchoolDeviceCode()) {
        // Show error
        lv_obj_t* screen = createScreen();
        applyScreenStyle(screen);

        lv_obj_t* content = lv_obj_create(screen);
        lv_obj_set_size(content, 400, 150);
        lv_obj_center(content);
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(content, 0, 0);
        lv_obj_set_style_pad_row(content, 10, 0);

        lv_obj_t* msg = lv_label_create(content);
        lv_label_set_text(msg, "Failed to get device code");
        lv_obj_set_style_text_font(msg, getThemeFonts()->font_title, 0);
        lv_obj_set_style_text_color(msg, LV_COLOR_ERROR, 0);

        lv_obj_t* err = lv_label_create(content);
        lv_label_set_text(err, getCWSchoolLinkError().c_str());
        lv_obj_set_style_text_font(err, getThemeFonts()->font_body, 0);
        lv_obj_set_style_text_color(err, LV_COLOR_TEXT_SECONDARY, 0);

        lv_obj_t* hint = lv_label_create(content);
        lv_label_set_text(hint, "Press ESC to go back");
        lv_obj_set_style_text_font(hint, getThemeFonts()->font_body, 0);
        lv_obj_set_style_text_color(hint, LV_COLOR_WARNING, 0);

        lv_obj_t* focus = lv_obj_create(screen);
        lv_obj_set_size(focus, 1, 1);
        lv_obj_set_style_bg_opa(focus, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(focus, 0, 0);
        addNavigableWidget(focus);

        return screen;
    }

    // Create link screen
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
    lv_label_set_text(title, "Link CW School Account");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Main content
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, 440, 200);
    lv_obj_center(content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(content, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(content, 1, 0);
    lv_obj_set_style_border_color(content, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_radius(content, 10, 0);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_style_pad_row(content, 12, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // Instructions
    lv_obj_t* instr = lv_label_create(content);
    lv_label_set_text(instr, "Visit vail.school/link-device");
    lv_obj_set_style_text_font(instr, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(instr, LV_COLOR_TEXT_SECONDARY, 0);

    lv_obj_t* instr2 = lv_label_create(content);
    lv_label_set_text(instr2, "and enter this code:");
    lv_obj_set_style_text_font(instr2, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(instr2, LV_COLOR_TEXT_SECONDARY, 0);

    // Code display (large, bold)
    cwschool_code_label = lv_label_create(content);
    lv_label_set_text(cwschool_code_label, getCWSchoolLinkCode().c_str());
    lv_obj_set_style_text_font(cwschool_code_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(cwschool_code_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_set_style_text_letter_space(cwschool_code_label, 8, 0);

    // Status
    cwschool_status_label = lv_label_create(content);
    lv_label_set_text(cwschool_status_label, "Waiting for link...");
    lv_obj_set_style_text_font(cwschool_status_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(cwschool_status_label, LV_COLOR_WARNING, 0);

    // Timer
    cwschool_timer_label = lv_label_create(content);
    int remaining = getCWSchoolLinkRemainingSeconds();
    char timerBuf[32];
    snprintf(timerBuf, sizeof(timerBuf), "Expires in %d:%02d", remaining / 60, remaining % 60);
    lv_label_set_text(cwschool_timer_label, timerBuf);
    lv_obj_set_style_text_font(cwschool_timer_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(cwschool_timer_label, LV_COLOR_TEXT_TERTIARY, 0);

    // Invisible focusable for keyboard input
    lv_obj_t* focus = lv_obj_create(screen);
    lv_obj_set_size(focus, 1, 1);
    lv_obj_set_style_bg_opa(focus, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus, 0, 0);
    lv_obj_add_flag(focus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus, cwschool_link_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus);

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "ESC Cancel");
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -10);

    // Start polling timer (every 5 seconds)
    if (cwschool_link_timer) {
        lv_timer_del(cwschool_link_timer);
    }
    cwschool_link_timer = lv_timer_create(cwschool_link_timer_cb, 5000, NULL);

    cwschool_link_screen = screen;
    return screen;
}

// ============================================
// Account Screen
// ============================================

static void cwschool_unlink_confirm(lv_event_t* e) {
    clearCWSchoolCredentials();
    setCurrentModeFromInt(150);  // MODE_CWSCHOOL (will show link screen)
}

/*
 * Create account info/unlink screen
 */
lv_obj_t* createCWSchoolAccountScreen() {
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
    lv_label_set_text(title, "CW School Account");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Account info card
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, 400, 180);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(card, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 20, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Linked status row
    lv_obj_t* status_row = lv_obj_create(card);
    lv_obj_set_size(status_row, LV_PCT(100), 35);
    lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_row, 0, 0);
    lv_obj_clear_flag(status_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(status_row, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* status_lbl = lv_label_create(status_row);
    lv_label_set_text(status_lbl, "Status:");
    lv_obj_set_style_text_font(status_lbl, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(status_lbl, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(status_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* status_val = lv_label_create(status_row);
    lv_label_set_text(status_val, isCWSchoolLinked() ? "Linked" : "Not linked");
    lv_obj_set_style_text_font(status_val, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(status_val, isCWSchoolLinked() ? LV_COLOR_SUCCESS : LV_COLOR_ERROR, 0);
    lv_obj_align(status_val, LV_ALIGN_RIGHT_MID, 0, 0);

    // Callsign row
    lv_obj_t* cs_row = lv_obj_create(card);
    lv_obj_set_size(cs_row, LV_PCT(100), 35);
    lv_obj_set_style_bg_opa(cs_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cs_row, 0, 0);
    lv_obj_clear_flag(cs_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(cs_row, LV_ALIGN_TOP_LEFT, 0, 40);

    lv_obj_t* cs_lbl = lv_label_create(cs_row);
    lv_label_set_text(cs_lbl, "Linked as:");
    lv_obj_set_style_text_font(cs_lbl, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(cs_lbl, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(cs_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* cs_val = lv_label_create(cs_row);
    lv_label_set_text(cs_val, getCWSchoolAccountDisplay().c_str());
    lv_obj_set_style_text_font(cs_val, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(cs_val, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(cs_val, LV_ALIGN_RIGHT_MID, 0, 0);

    // Display name row (if available)
    String displayName = getCWSchoolDisplayName();
    if (displayName.length() > 0) {
        lv_obj_t* name_row = lv_obj_create(card);
        lv_obj_set_size(name_row, LV_PCT(100), 35);
        lv_obj_set_style_bg_opa(name_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(name_row, 0, 0);
        lv_obj_clear_flag(name_row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(name_row, LV_ALIGN_TOP_LEFT, 0, 80);

        lv_obj_t* name_lbl = lv_label_create(name_row);
        lv_label_set_text(name_lbl, "Display Name:");
        lv_obj_set_style_text_font(name_lbl, getThemeFonts()->font_body, 0);
        lv_obj_set_style_text_color(name_lbl, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* name_val = lv_label_create(name_row);
        lv_label_set_text(name_val, displayName.c_str());
        lv_obj_set_style_text_font(name_val, getThemeFonts()->font_body, 0);
        lv_obj_set_style_text_color(name_val, LV_COLOR_TEXT_PRIMARY, 0);
        lv_obj_align(name_val, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    // Device ID row (truncated)
    lv_obj_t* dev_row = lv_obj_create(card);
    lv_obj_set_size(dev_row, LV_PCT(100), 35);
    lv_obj_set_style_bg_opa(dev_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dev_row, 0, 0);
    lv_obj_clear_flag(dev_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(dev_row, LV_ALIGN_TOP_LEFT, 0, 120);

    lv_obj_t* dev_lbl = lv_label_create(dev_row);
    lv_label_set_text(dev_lbl, "Device ID:");
    lv_obj_set_style_text_font(dev_lbl, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(dev_lbl, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(dev_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    String devId = getCWSchoolDeviceId();
    if (devId.length() > 15) {
        devId = devId.substring(0, 12) + "...";
    }
    lv_obj_t* dev_val = lv_label_create(dev_row);
    lv_label_set_text(dev_val, devId.c_str());
    lv_obj_set_style_text_font(dev_val, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(dev_val, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(dev_val, LV_ALIGN_RIGHT_MID, 0, 0);

    // Unlink button
    if (isCWSchoolLinked()) {
        lv_obj_t* unlink_btn = lv_btn_create(screen);
        lv_obj_set_size(unlink_btn, 200, 50);
        lv_obj_align(unlink_btn, LV_ALIGN_BOTTOM_MID, 0, -60);
        lv_obj_set_style_bg_color(unlink_btn, LV_COLOR_ERROR, 0);
        lv_obj_set_style_bg_color(unlink_btn, lv_color_hex(0xFACB), LV_STATE_FOCUSED);
        lv_obj_set_style_radius(unlink_btn, 8, 0);

        lv_obj_t* unlink_lbl = lv_label_create(unlink_btn);
        lv_label_set_text(unlink_lbl, "Unlink Device");
        lv_obj_set_style_text_font(unlink_lbl, getThemeFonts()->font_input, 0);
        lv_obj_center(unlink_lbl);

        lv_obj_add_event_cb(unlink_btn, cwschool_unlink_confirm, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(unlink_btn, cwschool_linear_nav_handler, LV_EVENT_KEY, NULL);
        addNavigableWidget(unlink_btn);
    } else {
        // Link button if not linked
        lv_obj_t* link_btn = lv_btn_create(screen);
        lv_obj_set_size(link_btn, 200, 50);
        lv_obj_align(link_btn, LV_ALIGN_BOTTOM_MID, 0, -60);
        lv_obj_set_style_bg_color(link_btn, LV_COLOR_SUCCESS, 0);
        lv_obj_set_style_bg_color(link_btn, LV_COLOR_ACCENT_GREEN, LV_STATE_FOCUSED);
        lv_obj_set_style_radius(link_btn, 8, 0);

        lv_obj_t* link_lbl = lv_label_create(link_btn);
        lv_label_set_text(link_lbl, "Link Account");
        lv_obj_set_style_text_font(link_lbl, getThemeFonts()->font_input, 0);
        lv_obj_center(link_lbl);

        lv_obj_add_event_cb(link_btn, [](lv_event_t* e) {
            setCurrentModeFromInt(151);  // MODE_CWSCHOOL_LINK
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(link_btn, cwschool_linear_nav_handler, LV_EVENT_KEY, NULL);
        addNavigableWidget(link_btn);
    }

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, isCWSchoolLinked() ? "ENTER Unlink   ESC Back" : "ENTER Link   ESC Back");
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -5);

    cwschool_account_screen = screen;
    return screen;
}

// ============================================
// Main Menu Screen (entry point)
// ============================================

/*
 * Create CW School main menu screen
 */
lv_obj_t* createCWSchoolMenuScreen() {
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
    lv_label_set_text(title, "Vail CW School");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status indicator in header
    lv_obj_t* status_indicator = lv_label_create(header);
    if (isCWSchoolLinked()) {
        lv_label_set_text(status_indicator, getCWSchoolAccountDisplay().c_str());
        lv_obj_set_style_text_color(status_indicator, LV_COLOR_SUCCESS, 0);
    } else {
        lv_label_set_text(status_indicator, "Not linked");
        lv_obj_set_style_text_color(status_indicator, LV_COLOR_TEXT_TERTIARY, 0);
    }
    lv_obj_set_style_text_font(status_indicator, getThemeFonts()->font_body, 0);
    lv_obj_align(status_indicator, LV_ALIGN_RIGHT_MID, -15, 0);

    // Menu container
    lv_obj_t* menu_container = lv_obj_create(screen);
    lv_obj_set_size(menu_container, 400, 200);
    lv_obj_center(menu_container);
    lv_obj_set_style_bg_opa(menu_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(menu_container, 0, 0);
    lv_obj_set_style_pad_all(menu_container, 10, 0);
    lv_obj_set_flex_flow(menu_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(menu_container, 15, 0);
    lv_obj_clear_flag(menu_container, LV_OBJ_FLAG_SCROLLABLE);

    // Account button
    lv_obj_t* account_btn = lv_btn_create(menu_container);
    lv_obj_set_size(account_btn, 350, 55);
    lv_obj_set_style_bg_color(account_btn, LV_COLOR_CARD_TEAL, 0);
    lv_obj_set_style_bg_color(account_btn, LV_COLOR_CARD_CYAN, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(account_btn, 10, 0);

    lv_obj_t* account_lbl = lv_label_create(account_btn);
    lv_label_set_text(account_lbl, isCWSchoolLinked() ? LV_SYMBOL_OK " Account Settings" : LV_SYMBOL_PLUS " Link Account");
    lv_obj_set_style_text_font(account_lbl, getThemeFonts()->font_input, 0);
    lv_obj_center(account_lbl);

    lv_obj_add_event_cb(account_btn, [](lv_event_t* e) {
        if (isCWSchoolLinked()) {
            setCurrentModeFromInt(152);  // MODE_CWSCHOOL_ACCOUNT
        } else {
            setCurrentModeFromInt(151);  // MODE_CWSCHOOL_LINK
        }
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(account_btn, cwschool_linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(account_btn);

    // Training button - navigate to Vail Course module selection
    lv_obj_t* training_btn = lv_btn_create(menu_container);
    lv_obj_set_size(training_btn, 350, 55);
    lv_obj_set_style_bg_color(training_btn, LV_COLOR_CARD_BLUE, 0);
    lv_obj_set_style_bg_color(training_btn, LV_COLOR_CARD_CYAN, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(training_btn, 10, 0);

    lv_obj_t* training_lbl = lv_label_create(training_btn);
    lv_label_set_text(training_lbl, LV_SYMBOL_AUDIO " Start Training");
    lv_obj_set_style_text_font(training_lbl, getThemeFonts()->font_input, 0);
    lv_obj_center(training_lbl);

    // Training works offline too - always enabled
    lv_obj_add_event_cb(training_btn, [](lv_event_t* e) {
        setCurrentModeFromInt(160);  // MODE_VAIL_COURSE_MODULE_SELECT
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(training_btn, cwschool_linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(training_btn);

    // Progress button - show course progress
    lv_obj_t* progress_btn = lv_btn_create(menu_container);
    lv_obj_set_size(progress_btn, 350, 55);
    lv_obj_set_style_bg_color(progress_btn, LV_COLOR_CARD_MINT, 0);
    lv_obj_set_style_bg_color(progress_btn, LV_COLOR_CARD_CYAN, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(progress_btn, 10, 0);

    lv_obj_t* progress_lbl = lv_label_create(progress_btn);
    lv_label_set_text(progress_lbl, LV_SYMBOL_CHARGE " View Progress");
    lv_obj_set_style_text_font(progress_lbl, getThemeFonts()->font_input, 0);
    lv_obj_center(progress_lbl);

    // Progress view works offline too
    lv_obj_add_event_cb(progress_btn, [](lv_event_t* e) {
        setCurrentModeFromInt(163);  // MODE_VAIL_COURSE_PROGRESS
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(progress_btn, cwschool_linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(progress_btn);

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "Arrows Navigate   ENTER Select   ESC Back");
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -5);

    return screen;
}

// ============================================
// Cleanup Functions
// ============================================

void cleanupCWSchoolLinkScreen() {
    Serial.println("[CWSchool] cleanupCWSchoolLinkScreen called");
    if (cwschool_link_timer) {
        Serial.println("[CWSchool] Deleting link timer (cleanup)");
        lv_timer_del(cwschool_link_timer);
        cwschool_link_timer = NULL;
    }
    cwschool_code_label = NULL;
    cwschool_status_label = NULL;
    cwschool_timer_label = NULL;
    cwschool_link_screen = NULL;
}

// ============================================
// Mode Handler Integration
// ============================================

/*
 * Handle CW School mode navigation
 * Called from main mode handler in vail-summit.ino
 */
bool handleCWSchoolMode(int mode) {
    lv_obj_t* screen = NULL;

    switch (mode) {
        case 150:  // MODE_CWSCHOOL
            // Show main menu
            screen = createCWSchoolMenuScreen();
            break;

        case 151:  // MODE_CWSCHOOL_LINK
            cleanupCWSchoolLinkScreen();
            screen = createCWSchoolLinkScreen();
            break;

        case 152:  // MODE_CWSCHOOL_ACCOUNT
            screen = createCWSchoolAccountScreen();
            break;

        // Future modes for training, progress, etc.
        // case 153:  // MODE_CWSCHOOL_TRAINING
        // case 154:  // MODE_CWSCHOOL_PROGRESS

        default:
            return false;  // Not a CW School mode
    }

    if (screen) {
        loadScreen(screen, SCREEN_ANIM_FADE);
        return true;
    }

    return false;
}

#endif // LV_CWSCHOOL_SCREENS_H
