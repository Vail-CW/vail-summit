/*
 * VAIL SUMMIT - LVGL Widget Factory
 * Reusable widget creation functions for consistent UI
 */

#ifndef LV_WIDGETS_SUMMIT_H
#define LV_WIDGETS_SUMMIT_H

#include <lvgl.h>
#include <WiFi.h>
#include "lv_theme_summit.h"
#include "lv_screen_manager.h"
#include "../core/config.h"
#include "../network/internet_check.h"

// External battery state from status_bar.h
extern int batteryPercent;

// ============================================
// Standard Footer Text Constants
// Using text labels since LVGL fonts don't include arrow symbols
// ============================================

// Menu navigation (up/down to move, enter to select)
#define FOOTER_NAV_ENTER_ESC        "UP/DN Navigate   ENTER Select   ESC Back"

// Menu navigation with volume shortcut hint
#define FOOTER_MENU_WITH_VOLUME     "UP/DN Navigate   ENTER Select   V Volume   ESC Back"

// Slider/value adjustment (left/right to change)
#define FOOTER_ADJUST_ESC           "L/R Adjust   ESC Back (auto-saves)"

// Combined navigation + adjustment
#define FOOTER_NAV_ADJUST_ESC       "UP/DN Navigate   L/R Adjust   ESC Back"

// Text input screens
#define FOOTER_TYPE_ENTER_ESC       "Type text   ENTER Save   ESC Cancel"

// Game screens
#define FOOTER_GAME_ESC             "SPACE Pause   ESC Exit"
#define FOOTER_GAME_SETTINGS        "UP/DN Navigate   L/R Adjust   ENTER Save   ESC Cancel"

// Context menu/confirmation
#define FOOTER_CONTEXT_MENU         "UP/DN Select   ENTER Confirm   ESC Cancel"
#define FOOTER_CONFIRM_DIALOG       "L/R Select   ENTER Confirm   ESC Cancel"

// Training modes
#define FOOTER_TRAINING_ACTIVE      "ENTER Submit   LEFT Replay   RIGHT Skip   ESC Exit"
#define FOOTER_TRAINING_WAIT        "Keying in progress...   ESC Exit"
#define FOOTER_TRAINING_AUTOPLAY    "Type Answer   SPACE Replay   ESC Back"

// ============================================
// Forward Declarations
// ============================================

// Widget creation functions
lv_obj_t* createMenuCard(lv_obj_t* parent, const char* icon, const char* title, lv_event_cb_t click_cb, void* user_data);
lv_obj_t* createSettingsRow(lv_obj_t* parent, const char* label, const char* value);
lv_obj_t* createValueSlider(lv_obj_t* parent, const char* label, int min, int max, int current, lv_event_cb_t change_cb);
lv_obj_t* createTextInput(lv_obj_t* parent, const char* placeholder, const char* initial_text, int max_length);
lv_obj_t* createStatsCard(lv_obj_t* parent, const char* title, const char** labels, const char** values, int count);
lv_obj_t* createStatusBar(lv_obj_t* parent);
lv_obj_t* createCompactStatusBar(lv_obj_t* parent);
lv_obj_t* createScrollableList(lv_obj_t* parent, int height);
lv_obj_t* createConfirmDialog(const char* title, const char* message, lv_event_cb_t confirm_cb, lv_event_cb_t cancel_cb);

// ============================================
// Menu Card Widget
// ============================================

/*
 * Create a menu card with icon circle, title, and right arrow
 * Used for main menu navigation
 *
 * Parameters:
 *   parent - Parent object to add card to
 *   icon - Single character for icon (e.g., "T" for Training)
 *   title - Menu item title text
 *   click_cb - Callback when card is selected
 *   user_data - User data passed to callback
 */
lv_obj_t* createMenuCard(lv_obj_t* parent, const char* icon, const char* title, lv_event_cb_t click_cb, void* user_data) {
    // Create card container
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, CARD_MAIN_WIDTH, CARD_MAIN_HEIGHT);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, 15, 0);

    // Apply menu card style
    applyMenuCardStyle(card);

    // Icon circle
    lv_obj_t* icon_circle = lv_obj_create(card);
    lv_obj_set_size(icon_circle, ICON_RADIUS * 2, ICON_RADIUS * 2);
    lv_obj_add_style(icon_circle, getStyleIconCircle(), 0);
    lv_obj_clear_flag(icon_circle, LV_OBJ_FLAG_SCROLLABLE);

    // Icon letter - use theme font for consistency
    lv_obj_t* icon_label = lv_label_create(icon_circle);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_font(icon_label, getThemeFonts()->font_title, 0);
    lv_obj_set_style_text_color(icon_label, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(icon_label);

    // Title label
    lv_obj_t* title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_add_style(title_label, getStyleLabelSubtitle(), 0);
    lv_obj_set_flex_grow(title_label, 1);

    // Right arrow indicator - use Montserrat for LVGL symbols
    lv_obj_t* arrow = lv_label_create(card);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(arrow, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_18, 0);

    // Make card focusable and add to navigation group
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    addNavigableWidget(card);

    // Add click event
    if (click_cb != NULL) {
        lv_obj_add_event_cb(card, click_cb, LV_EVENT_CLICKED, user_data);
    }

    return card;
}

// ============================================
// Settings Row Widget
// ============================================

/*
 * Create a settings row with label and value display
 * Used for showing current settings with option to edit
 *
 * Parameters:
 *   parent - Parent object
 *   label - Setting name
 *   value - Current value text
 */
lv_obj_t* createSettingsRow(lv_obj_t* parent, const char* label, const char* value) {
    // Row container
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(row, 12, 0);
    lv_obj_set_style_pad_column(row, 10, 0);

    // Apply card style
    applyCardStyle(row);

    // Label
    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);
    lv_obj_add_style(lbl, getStyleLabelBody(), 0);

    // Value (highlighted) - use theme font
    lv_obj_t* val = lv_label_create(row);
    lv_label_set_text(val, value);
    lv_obj_set_style_text_color(val, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(val, getThemeFonts()->font_input, 0);

    // Make focusable
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    addNavigableWidget(row);

    return row;
}

// ============================================
// Value Slider Widget
// ============================================

/*
 * Create a labeled slider for adjusting values
 * Used for volume, brightness, speed settings
 *
 * Parameters:
 *   parent - Parent object
 *   label - Setting name
 *   min - Minimum value
 *   max - Maximum value
 *   current - Current value
 *   change_cb - Callback when value changes
 */
lv_obj_t* createValueSlider(lv_obj_t* parent, const char* label, int min, int max, int current, lv_event_cb_t change_cb) {
    // Container
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_size(container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container, 15, 0);
    lv_obj_set_style_pad_row(container, 10, 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    // Header row (label + value)
    lv_obj_t* header = lv_obj_create(container);
    lv_obj_set_size(header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Label
    lv_obj_t* lbl = lv_label_create(header);
    lv_label_set_text(lbl, label);
    lv_obj_add_style(lbl, getStyleLabelSubtitle(), 0);

    // Value display - use theme font
    lv_obj_t* val = lv_label_create(header);
    lv_label_set_text_fmt(val, "%d", current);
    lv_obj_set_style_text_color(val, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(val, getThemeFonts()->font_subtitle, 0);

    // Slider
    lv_obj_t* slider = lv_slider_create(container);
    lv_obj_set_width(slider, lv_pct(100));
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, current, LV_ANIM_OFF);
    applySliderStyle(slider);

    // Store value label reference in slider user data for updates
    lv_obj_set_user_data(slider, val);

    // Add change callback
    if (change_cb != NULL) {
        lv_obj_add_event_cb(slider, change_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    // Update value label on change (internal handler)
    lv_obj_add_event_cb(slider, [](lv_event_t* e) {
        lv_obj_t* slider = lv_event_get_target(e);
        lv_obj_t* val = (lv_obj_t*)lv_obj_get_user_data(slider);
        if (val != NULL) {
            lv_label_set_text_fmt(val, "%d", lv_slider_get_value(slider));
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Make slider focusable
    addNavigableWidget(slider);

    return container;
}

// ============================================
// Text Input Widget
// ============================================

/*
 * Create a text input field with cursor
 * Used for callsign, password, notes
 *
 * Parameters:
 *   parent - Parent object
 *   placeholder - Placeholder text when empty
 *   initial_text - Initial text content
 *   max_length - Maximum character length
 */
lv_obj_t* createTextInput(lv_obj_t* parent, const char* placeholder, const char* initial_text, int max_length) {
    lv_obj_t* ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, lv_pct(100), LV_SIZE_CONTENT);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, max_length);
    lv_textarea_set_placeholder_text(ta, placeholder);

    if (initial_text != NULL && strlen(initial_text) > 0) {
        lv_textarea_set_text(ta, initial_text);
    }

    lv_obj_add_style(ta, getStyleTextarea(), 0);

    // Make focusable
    addNavigableWidget(ta);

    return ta;
}

// ============================================
// Stats Card Widget
// ============================================

/*
 * Create a statistics display card
 * Used for training stats, QSO stats overlay
 *
 * Parameters:
 *   parent - Parent object
 *   title - Card title
 *   labels - Array of stat labels
 *   values - Array of stat values
 *   count - Number of stats
 */
lv_obj_t* createStatsCard(lv_obj_t* parent, const char* title, const char** labels, const char** values, int count) {
    // Card container
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 300, LV_SIZE_CONTENT);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 15, 0);
    lv_obj_set_style_pad_row(card, 8, 0);
    applyCardStyle(card);

    // Title
    lv_obj_t* title_lbl = lv_label_create(card);
    lv_label_set_text(title_lbl, title);
    lv_obj_add_style(title_lbl, getStyleLabelTitle(), 0);

    // Stats rows
    for (int i = 0; i < count; i++) {
        lv_obj_t* row = lv_obj_create(card);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_add_style(lbl, getStyleLabelBody(), 0);

        lv_obj_t* val = lv_label_create(row);
        lv_label_set_text(val, values[i]);
        lv_obj_set_style_text_color(val, LV_COLOR_ACCENT_PRIMARY, 0);
    }

    return card;
}

// ============================================
// Status Bar Widget
// ============================================

/*
 * Create a full-width status bar with title, WiFi, and battery icons
 * Used at top of screen for main views
 *
 * Parameters:
 *   parent - Parent screen object
 */
lv_obj_t* createStatusBar(lv_obj_t* parent) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_layout(bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_style(bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    // Left section (title or mode indicator)
    lv_obj_t* left = lv_obj_create(bar);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);

    lv_obj_t* title = lv_label_create(left);
    lv_label_set_text(title, "VAIL SUMMIT");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);

    // Right section - battery icon only (use Montserrat for LVGL symbols)
    lv_obj_t* battery_icon = lv_label_create(bar);
    lv_obj_set_style_text_font(battery_icon, &lv_font_montserrat_24, 0);

    if (batteryPercent > 80) {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_FULL);
        lv_obj_set_style_text_color(battery_icon, LV_COLOR_SUCCESS, 0);
    } else if (batteryPercent > 60) {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_3);
        lv_obj_set_style_text_color(battery_icon, LV_COLOR_SUCCESS, 0);
    } else if (batteryPercent > 40) {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_2);
        lv_obj_set_style_text_color(battery_icon, LV_COLOR_ACCENT_PRIMARY, 0);
    } else if (batteryPercent > 20) {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_1);
        lv_obj_set_style_text_color(battery_icon, LV_COLOR_WARNING, 0);
    } else {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_EMPTY);
        lv_obj_set_style_text_color(battery_icon, LV_COLOR_ERROR, 0);
    }

    return bar;
}

/*
 * Create status icons (WiFi + battery) for top-right of any screen
 * Matches the header icons exactly for consistency
 *
 * Parameters:
 *   parent - Parent screen or header object
 */
lv_obj_t* createCompactStatusBar(lv_obj_t* parent) {
    // WiFi icon - use Montserrat for LVGL symbols
    // Color indicates connectivity state:
    //   - Green: Full internet connectivity (or checking - optimistic)
    //   - Orange: WiFi connected but no internet verified
    //   - Red: Disconnected
    lv_obj_t* wifi_icon = lv_label_create(parent);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_20, 0);
    InternetStatus inetStatus = getInternetStatus();
    if (inetStatus == INET_CONNECTED || inetStatus == INET_CHECKING) {
        lv_obj_set_style_text_color(wifi_icon, LV_COLOR_SUCCESS, 0);
    } else if (inetStatus == INET_WIFI_ONLY) {
        lv_obj_set_style_text_color(wifi_icon, LV_COLOR_WARNING, 0);
    } else {
        lv_obj_set_style_text_color(wifi_icon, LV_COLOR_ERROR, 0);
    }
    lv_obj_align(wifi_icon, LV_ALIGN_TOP_RIGHT, -50, 8);

    // Battery icon - use Montserrat for LVGL symbols
    lv_obj_t* batt_icon = lv_label_create(parent);
    lv_obj_set_style_text_font(batt_icon, &lv_font_montserrat_20, 0);
    lv_obj_align(batt_icon, LV_ALIGN_TOP_RIGHT, -10, 8);

    if (batteryPercent > 80) {
        lv_label_set_text(batt_icon, LV_SYMBOL_BATTERY_FULL);
        lv_obj_set_style_text_color(batt_icon, LV_COLOR_SUCCESS, 0);
    } else if (batteryPercent > 60) {
        lv_label_set_text(batt_icon, LV_SYMBOL_BATTERY_3);
        lv_obj_set_style_text_color(batt_icon, LV_COLOR_SUCCESS, 0);
    } else if (batteryPercent > 40) {
        lv_label_set_text(batt_icon, LV_SYMBOL_BATTERY_2);
        lv_obj_set_style_text_color(batt_icon, LV_COLOR_ACCENT_PRIMARY, 0);
    } else if (batteryPercent > 20) {
        lv_label_set_text(batt_icon, LV_SYMBOL_BATTERY_1);
        lv_obj_set_style_text_color(batt_icon, LV_COLOR_WARNING, 0);
    } else {
        lv_label_set_text(batt_icon, LV_SYMBOL_BATTERY_EMPTY);
        lv_obj_set_style_text_color(batt_icon, LV_COLOR_ERROR, 0);
    }

    return batt_icon;
}

/*
 * Create a split title bar label: mainTitle in title font, " // subTitle" in subtitle font.
 * Renders both on one visual line inside a transparent flex-row container.
 * Returns the container object.
 *
 * Parameters:
 *   parent    - Title bar object (must use manual/absolute layout, not flex)
 *   mainTitle - Primary section name (e.g. "CW ACADEMY")
 *   subTitle  - Secondary section name (e.g. "COPY PRACTICE")
 */
lv_obj_t* createSplitTitleLabel(lv_obj_t* parent, const char* mainTitle, const char* subTitle) {
    lv_obj_t* ctn = lv_obj_create(parent);
    lv_obj_set_size(ctn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ctn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctn, 0, 0);
    lv_obj_set_style_pad_all(ctn, 0, 0);
    lv_obj_set_layout(ctn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ctn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ctn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(ctn, LV_ALIGN_LEFT_MID, 15, 0);

    lv_obj_t* main_lbl = lv_label_create(ctn);
    lv_label_set_text(main_lbl, mainTitle);
    lv_obj_add_style(main_lbl, getStyleLabelTitle(), 0);

    lv_obj_t* sub_lbl = lv_label_create(ctn);
    char sub_text[80];
    snprintf(sub_text, sizeof(sub_text), " // %s", subTitle);
    for (char* p = sub_text + 4; *p; p++) *p = toupper((unsigned char)*p);
    lv_label_set_text(sub_lbl, sub_text);
    lv_obj_set_style_text_font(sub_lbl, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(sub_lbl, LV_COLOR_TEXT_SECONDARY, 0);

    return ctn;
}

// ============================================
// Scrollable List Widget
// ============================================

/*
 * Create a scrollable list container
 * Used for menu lists, settings lists, log entries
 *
 * Parameters:
 *   parent - Parent object
 *   height - List height in pixels
 */
lv_obj_t* createScrollableList(lv_obj_t* parent, int height) {
    lv_obj_t* list = lv_obj_create(parent);
    lv_obj_set_size(list, lv_pct(100), height);
    lv_obj_set_layout(list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 5, 0);
    lv_obj_set_style_pad_row(list, 5, 0);
    applyListStyle(list);

    // Enable scrolling
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    return list;
}

// ============================================
// Confirm Dialog Widget
// ============================================

/*
 * Create a confirmation dialog with Yes/No buttons
 *
 * Parameters:
 *   title - Dialog title
 *   message - Message text
 *   confirm_cb - Callback for Yes button
 *   cancel_cb - Callback for No button
 */
// Saved indev group so dismissal can restore prior navigation context.
static lv_group_t* s_confirm_prev_group = NULL;
static lv_group_t* s_confirm_dialog_group = NULL;
static lv_indev_t* s_confirm_modal_indevs[8];
static int s_confirm_modal_indev_count = 0;

struct ConfirmDialogContext {
    lv_event_cb_t confirm_cb;
    lv_event_cb_t cancel_cb;
};

static void closeConfirmDialog(lv_obj_t* backdrop) {
    if (backdrop == NULL) return;

    if (s_confirm_dialog_group != NULL) {
        for (int i = 0; i < s_confirm_modal_indev_count; i++) {
            if (s_confirm_modal_indevs[i] != NULL) {
                lv_indev_set_group(s_confirm_modal_indevs[i], s_confirm_prev_group);
            }
        }
        s_confirm_modal_indev_count = 0;
        lv_group_del(s_confirm_dialog_group);
        s_confirm_dialog_group = NULL;
        s_confirm_prev_group = NULL;
    }

    ConfirmDialogContext* ctx = (ConfirmDialogContext*)lv_obj_get_user_data(backdrop);
    if (ctx != NULL) {
        delete ctx;
        lv_obj_set_user_data(backdrop, NULL);
    }

    lv_obj_del(backdrop);
}

lv_obj_t* createConfirmDialog(const char* title, const char* message, lv_event_cb_t confirm_cb, lv_event_cb_t cancel_cb) {
    // Fullscreen backdrop absorbs all clicks/keys outside dialog.
    lv_obj_t* backdrop = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(backdrop);
    lv_obj_set_size(backdrop, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(backdrop, 0, 0);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);

    ConfirmDialogContext* ctx = new ConfirmDialogContext();
    ctx->confirm_cb = confirm_cb;
    ctx->cancel_cb = cancel_cb;
    lv_obj_set_user_data(backdrop, ctx);

    lv_obj_add_event_cb(backdrop, [](lv_event_t* e) {
        if (lv_event_get_target(e) != lv_event_get_current_target(e)) return;
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    }, LV_EVENT_ALL, NULL);

    lv_obj_t* mbox = lv_msgbox_create(backdrop, title, message, NULL, false);
    lv_obj_center(mbox);
    lv_obj_add_style(mbox, getStyleMsgbox(), 0);

    lv_obj_t* footer = lv_obj_create(mbox);
    lv_obj_remove_style_all(footer);
    lv_obj_set_size(footer, lv_pct(100), 60);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* yes_btn = lv_obj_create(footer);
    lv_obj_set_size(yes_btn, 100, 34);
    lv_obj_add_flag(yes_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(yes_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(yes_btn, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_bg_color(yes_btn, LV_COLOR_BG_CARD_ACTIVE, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(yes_btn, 8, 0);
    lv_obj_set_style_border_width(yes_btn, 1, 0);
    lv_obj_set_style_border_color(yes_btn, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_border_width(yes_btn, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(yes_btn, LV_COLOR_ACCENT_PRIMARY, LV_STATE_FOCUSED);
    lv_obj_t* yes_lbl = lv_label_create(yes_btn);
    lv_label_set_text(yes_lbl, "Yes");
    lv_obj_set_style_text_color(yes_lbl, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(yes_lbl);

    lv_obj_t* no_btn = lv_obj_create(footer);
    lv_obj_set_size(no_btn, 100, 34);
    lv_obj_add_flag(no_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(no_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(no_btn, LV_COLOR_BG_CARD_ALT, 0);
    lv_obj_set_style_bg_color(no_btn, LV_COLOR_BG_CARD_ACTIVE, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(no_btn, 8, 0);
    lv_obj_set_style_border_width(no_btn, 1, 0);
    lv_obj_set_style_border_color(no_btn, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_border_width(no_btn, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(no_btn, LV_COLOR_ACCENT_PRIMARY, LV_STATE_FOCUSED);
    lv_obj_t* no_lbl = lv_label_create(no_btn);
    lv_label_set_text(no_lbl, "No");
    lv_obj_set_style_text_color(no_lbl, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(no_lbl);

    auto activate_yes = [](lv_event_t* e) {
        lv_obj_t* btn = lv_event_get_target(e);
        lv_obj_t* backdrop_local = lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(btn)));
        ConfirmDialogContext* ctx_local = (ConfirmDialogContext*)lv_obj_get_user_data(backdrop_local);
        if (ctx_local != NULL && ctx_local->confirm_cb != NULL) {
            ctx_local->confirm_cb(e);
        }
        closeConfirmDialog(backdrop_local);
        lv_indev_t* indev = getLVGLKeypad();
        if (indev != NULL) lv_indev_wait_release(indev);
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    };

    auto activate_no = [](lv_event_t* e) {
        lv_obj_t* btn = lv_event_get_target(e);
        lv_obj_t* backdrop_local = lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(btn)));
        ConfirmDialogContext* ctx_local = (ConfirmDialogContext*)lv_obj_get_user_data(backdrop_local);
        if (ctx_local != NULL && ctx_local->cancel_cb != NULL) {
            ctx_local->cancel_cb(e);
        }
        closeConfirmDialog(backdrop_local);
        lv_indev_t* indev = getLVGLKeypad();
        if (indev != NULL) lv_indev_wait_release(indev);
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    };

    lv_obj_add_event_cb(yes_btn, activate_yes, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(no_btn, activate_no, LV_EVENT_CLICKED, NULL);

    // Key handling: arrows switch focus, ENTER selects, ESC cancels.
    lv_obj_add_event_cb(yes_btn, [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_KEY) return;
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) {
            if (s_confirm_dialog_group != NULL) {
                lv_group_focus_next(s_confirm_dialog_group);
            }
            lv_event_stop_processing(e);
        } else if (key == LV_KEY_ENTER) {
            lv_event_send(lv_event_get_target(e), LV_EVENT_CLICKED, NULL);
        } else if (key == LV_KEY_ESC) {
            if (s_confirm_dialog_group != NULL) {
                lv_group_focus_next(s_confirm_dialog_group);
                lv_obj_t* focused = lv_group_get_focused(s_confirm_dialog_group);
                if (focused != NULL) {
                    lv_event_send(focused, LV_EVENT_CLICKED, NULL);
                }
            }
        }
    }, LV_EVENT_KEY, NULL);

    lv_obj_add_event_cb(no_btn, [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_KEY) return;
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_LEFT || key == LV_KEY_UP) {
            if (s_confirm_dialog_group != NULL) {
                lv_group_focus_prev(s_confirm_dialog_group);
            }
            lv_event_stop_processing(e);
        } else if (key == LV_KEY_ENTER || key == LV_KEY_ESC) {
            lv_event_send(lv_event_get_target(e), LV_EVENT_CLICKED, NULL);
        }
    }, LV_EVENT_KEY, NULL);

    // Private modal group prevents background navigation drift.
    s_confirm_prev_group = getLVGLInputGroup();
    s_confirm_dialog_group = lv_group_create();
    lv_group_add_obj(s_confirm_dialog_group, yes_btn);
    lv_group_add_obj(s_confirm_dialog_group, no_btn);
    lv_group_focus_obj(no_btn);  // default safe option

    // Attach all keypad/encoder input devices to modal group.
    s_confirm_modal_indev_count = 0;
    lv_indev_t* indev = lv_indev_get_next(NULL);
    while (indev != NULL && s_confirm_modal_indev_count < 8) {
        lv_indev_type_t indev_type = lv_indev_get_type(indev);
        if (indev_type == LV_INDEV_TYPE_KEYPAD || indev_type == LV_INDEV_TYPE_ENCODER) {
            lv_indev_set_group(indev, s_confirm_dialog_group);
            s_confirm_modal_indevs[s_confirm_modal_indev_count++] = indev;
        }
        indev = lv_indev_get_next(indev);
    }

    return backdrop;
}

// ============================================
// Alert Dialog Widget
// ============================================

/*
 * Create a modal alert dialog with OK button that closes on Enter/click.
 *
 * Modality is enforced by a fullscreen backdrop on lv_layer_top() that swallows
 * every event landing outside the dialog, and by swapping the keypad indev onto
 * a private group while the alert is open. This prevents clicks/keys from
 * falling through to the screen below and prevents the underlying focus from
 * drifting in response to the dismiss.
 *
 * Parameters:
 *   title    - Dialog title
 *   message  - Message text
 *   on_close - Optional callback when dialog is closed (can be NULL)
 */

// Saved indev group so dismissal can restore the prior navigation context.
static lv_group_t* s_alert_prev_group = NULL;
static lv_group_t* s_alert_dialog_group = NULL;

// Tear down: invoke on_close, restore the keypad group, delete the backdrop
// (which removes the msgbox child along with it).
static void closeAlertDialog(lv_obj_t* backdrop, lv_event_t* e) {
    if (backdrop == NULL) return;

    lv_obj_t* mbox = lv_obj_get_child(backdrop, 0);
    lv_event_cb_t close_cb = (mbox != NULL)
        ? (lv_event_cb_t)lv_obj_get_user_data(mbox)
        : NULL;
    if (close_cb != NULL) close_cb(e);

    lv_indev_t* indev = getLVGLKeypad();
    if (indev != NULL && s_alert_dialog_group != NULL) {
        lv_indev_set_group(indev, s_alert_prev_group);
        lv_group_del(s_alert_dialog_group);
        s_alert_dialog_group = NULL;
        s_alert_prev_group = NULL;
    }

    lv_obj_del(backdrop);
}

lv_obj_t* createAlertDialog(const char* title, const char* message, lv_event_cb_t on_close = NULL) {
    // Fullscreen backdrop absorbs every event that does not target the msgbox.
    lv_obj_t* backdrop = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(backdrop);
    lv_obj_set_size(backdrop, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(backdrop, 0, 0);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);

    // Stop every event whose target IS the backdrop. Events on the msgbox/OK
    // button do not bubble by default, so they are unaffected.
    lv_obj_add_event_cb(backdrop, [](lv_event_t* e) {
        if (lv_event_get_target(e) != lv_event_get_current_target(e)) return;
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    }, LV_EVENT_ALL, NULL);

    // Msgbox is a child of the backdrop so it draws above and dies with it.
    lv_obj_t* mbox = lv_msgbox_create(backdrop, title, message, NULL, false);
    lv_obj_center(mbox);
    lv_obj_add_style(mbox, getStyleMsgbox(), 0);

    lv_obj_set_user_data(mbox, (void*)on_close);

    lv_obj_t* footer = lv_obj_create(mbox);
    lv_obj_remove_style_all(footer);
    lv_obj_set_size(footer, lv_pct(100), 60);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btn = lv_obj_create(footer);
    lv_obj_set_size(btn, 120, 40);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(btn, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_border_width(btn, 0, 0);

    // Button stores the backdrop so dismissal can find and delete it.
    lv_obj_set_user_data(btn, (void*)backdrop);

    lv_obj_t* btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "OK");
    lv_obj_set_style_text_color(btn_label, lv_color_black(), 0);
    lv_obj_center(btn_label);

    // KEY handler: ANY key dismisses without invoking on_close (ESC-style).
    // Also discard the rest of the in-flight press via lv_indev_wait_release()
    // so the Enter release does not synthesize a CLICKED on the menu item that
    // opened the alert, which would otherwise re-trigger it in a loop.
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        lv_obj_t* b = lv_event_get_target(e);
        lv_obj_t* backdrop = (lv_obj_t*)lv_obj_get_user_data(b);
        closeAlertDialog(backdrop, NULL);
        lv_indev_t* indev = getLVGLKeypad();
        if (indev != NULL) lv_indev_wait_release(indev);
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    }, LV_EVENT_KEY, NULL);

    // CLICKED handler: ESC-style dismiss — never invokes on_close.
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        lv_obj_t* b = lv_event_get_target(e);
        lv_obj_t* backdrop = (lv_obj_t*)lv_obj_get_user_data(b);
        closeAlertDialog(backdrop, NULL);
        lv_indev_t* indev = getLVGLKeypad();
        if (indev != NULL) lv_indev_wait_release(indev);
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    }, LV_EVENT_CLICKED, NULL);

    // Swap the keypad onto a private group containing only the OK button.
    // Arrow keys can't move focus on the screen below, and Enter/ESC reach
    // only this dialog. Restored in closeAlertDialog().
    lv_indev_t* indev = getLVGLKeypad();
    if (indev != NULL) {
        s_alert_prev_group = getLVGLInputGroup();
        s_alert_dialog_group = lv_group_create();
        lv_group_set_wrap(s_alert_dialog_group, false);
        lv_group_add_obj(s_alert_dialog_group, btn);
        lv_indev_set_group(indev, s_alert_dialog_group);
        lv_group_focus_obj(btn);
    }

    return mbox;
}

/*
 * Create a loading overlay with spinner and message
 * Returns the overlay object - call lv_obj_del() to remove it
 *
 * Parameters:
 *   message - Loading message to display
 */
lv_obj_t* createLoadingOverlay(const char* message) {
    // Create semi-transparent overlay
    lv_obj_t* overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Create centered card
    lv_obj_t* card = lv_obj_create(overlay);
    lv_obj_set_size(card, 280, 120);
    lv_obj_center(card);
    applyCardStyle(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Spinner (using animated arc)
    lv_obj_t* spinner = lv_spinner_create(card, 1000, 60);
    lv_obj_set_size(spinner, 40, 40);
    lv_obj_align(spinner, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_arc_color(spinner, LV_COLOR_ACCENT_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, LV_COLOR_BG_LAYER2, LV_PART_MAIN);

    // Message label
    lv_obj_t* label = lv_label_create(card);
    lv_label_set_text(label, message);
    lv_obj_add_style(label, getStyleLabelBody(), 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -15);

    // Force immediate render
    lv_timer_handler();

    return overlay;
}

// ============================================
// Specialized Widget Helpers
// ============================================

/*
 * Create a decoder display box (monospace text for decoded morse)
 */
lv_obj_t* createDecoderBox(lv_obj_t* parent, int width, int height) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_bg_color(box, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_color(box, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_set_style_pad_all(box, 10, 0);

    lv_obj_t* text = lv_label_create(box);
    lv_label_set_text(text, "");
    lv_obj_set_style_text_font(text, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(text, LV_COLOR_SUCCESS, 0);
    lv_label_set_long_mode(text, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(text, width - 20);

    return box;
}

/*
 * Create a WPM display indicator
 */
lv_obj_t* createWPMIndicator(lv_obj_t* parent, int initial_wpm) {
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_size(container, 100, 50);
    lv_obj_set_style_bg_color(container, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_radius(container, 8, 0);
    lv_obj_set_style_pad_all(container, 5, 0);

    lv_obj_t* label = lv_label_create(container);
    lv_label_set_text(label, "WPM");
    lv_obj_set_style_text_font(label, getThemeFonts()->font_small, 0);
    lv_obj_set_style_text_color(label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* value = lv_label_create(container);
    lv_label_set_text_fmt(value, "%d", initial_wpm);
    lv_obj_set_style_text_font(value, getThemeFonts()->font_title, 0);
    lv_obj_set_style_text_color(value, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(value, LV_ALIGN_BOTTOM_MID, 0, 0);

    return container;
}

/*
 * Create a game score display
 */
lv_obj_t* createScoreDisplay(lv_obj_t* parent, const char* label, int initial_score) {
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_size(container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(container, 8, 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);

    lv_obj_t* lbl = lv_label_create(container);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(lbl, getThemeFonts()->font_body, 0);

    lv_obj_t* val = lv_label_create(container);
    lv_label_set_text_fmt(val, "%d", initial_score);
    lv_obj_set_style_text_color(val, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_set_style_text_font(val, getThemeFonts()->font_subtitle, 0);

    return container;
}

/*
 * Create lives display (hearts)
 */
lv_obj_t* createLivesDisplay(lv_obj_t* parent, int lives, int max_lives) {
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_size(container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(container, 4, 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);

    for (int i = 0; i < max_lives; i++) {
        lv_obj_t* heart = lv_label_create(container);
        if (i < lives) {
            lv_label_set_text(heart, LV_SYMBOL_OK);  // Filled heart substitute
            lv_obj_set_style_text_color(heart, LV_COLOR_ERROR, 0);
        } else {
            lv_label_set_text(heart, LV_SYMBOL_CLOSE);  // Empty heart substitute
            lv_obj_set_style_text_color(heart, LV_COLOR_TEXT_DISABLED, 0);
        }
        lv_obj_set_style_text_font(heart, &lv_font_montserrat_18, 0);  // Montserrat for LVGL symbols
    }

    return container;
}

/*
 * Create a character progress grid (for Koch method or Hear It Type It)
 */
lv_obj_t* createCharacterGrid(lv_obj_t* parent, const char* characters, bool* unlocked, int count) {
    lv_obj_t* grid = lv_obj_create(parent);
    lv_obj_set_size(grid, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(grid, 10, 0);
    lv_obj_set_style_pad_gap(grid, 8, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);

    for (int i = 0; i < count; i++) {
        lv_obj_t* cell = lv_obj_create(grid);
        lv_obj_set_size(cell, 40, 40);
        lv_obj_set_style_radius(cell, 6, 0);

        if (unlocked[i]) {
            lv_obj_set_style_bg_color(cell, LV_COLOR_SUCCESS, 0);
        } else {
            lv_obj_set_style_bg_color(cell, LV_COLOR_BG_LAYER2, 0);
        }

        char ch[2] = {characters[i], '\0'};
        lv_obj_t* lbl = lv_label_create(cell);
        lv_label_set_text(lbl, ch);
        lv_obj_center(lbl);
        lv_obj_set_style_text_font(lbl, getThemeFonts()->font_subtitle, 0);

        if (unlocked[i]) {
            lv_obj_set_style_text_color(lbl, LV_COLOR_BG_DEEP, 0);
        } else {
            lv_obj_set_style_text_color(lbl, LV_COLOR_TEXT_DISABLED, 0);
        }
    }

    return grid;
}

#endif // LV_WIDGETS_SUMMIT_H
