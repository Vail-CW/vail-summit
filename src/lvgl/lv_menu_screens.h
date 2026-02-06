/*
 * VAIL SUMMIT - LVGL Menu Screens
 * Replaces LovyanGFX menu rendering with LVGL
 */

#ifndef LV_MENU_SCREENS_H
#define LV_MENU_SCREENS_H

#include <lvgl.h>
#include <WiFi.h>
#include "lv_theme_summit.h"
#include "lv_widgets_summit.h"
#include "lv_screen_manager.h"
#include "../core/config.h"
#include "../core/modes.h"

// Forward declarations from main file
extern int currentSelection;

// ============================================
// Menu Data Structures
// ============================================

// Menu item structure for LVGL menus
struct LVMenuItem {
    const char* icon;
    const char* title;
    int target_mode;  // MenuMode enum value to switch to
};

// ============================================
// Menu Data
// Mode values from MenuMode enum in src/core/modes.h
// ============================================

// Main menu items - using LVGL symbols for modern look
static const LVMenuItem mainMenuItems[] = {
    {LV_SYMBOL_AUDIO, "CW", MODE_CW_MENU},
    {LV_SYMBOL_PLAY, "Games", MODE_GAMES_MENU},
    {LV_SYMBOL_DIRECTORY, "Ham Tools", MODE_HAM_TOOLS_MENU},
    {LV_SYMBOL_SETTINGS, "Settings", MODE_SETTINGS_MENU}
};
#define MAIN_MENU_COUNT 4

// CW submenu items
static const LVMenuItem cwMenuItems[] = {
    {LV_SYMBOL_EDIT, "Training", MODE_TRAINING_MENU},
    {LV_SYMBOL_REFRESH, "Practice", MODE_PRACTICE},
    {LV_SYMBOL_UPLOAD, "Vail Repeater", MODE_VAIL_REPEATER},
    {LV_SYMBOL_ENVELOPE, "Morse Mailbox", MODE_MORSE_MAILBOX},
    {LV_SYMBOL_AUDIO, "Morse Notes", MODE_MORSE_NOTES_LIBRARY},
    {LV_SYMBOL_BLUETOOTH, "Bluetooth", MODE_BLUETOOTH_MENU},
    {LV_SYMBOL_POWER, "Radio Output", MODE_RADIO_OUTPUT},
    {LV_SYMBOL_SAVE, "CW Memories", MODE_CW_MEMORIES}
};
#define CW_MENU_COUNT 8

// Training submenu items
static const LVMenuItem trainingMenuItems[] = {
    {LV_SYMBOL_EDIT, "Vail Master", MODE_VAIL_MASTER},
    {LV_SYMBOL_AUDIO, "Hear It Type It", MODE_HEAR_IT_MENU},
    {LV_SYMBOL_SHUFFLE, "CW School", MODE_CWSCHOOL},
    {LV_SYMBOL_FILE, "CW Academy", MODE_CW_ACADEMY_TRACK_SELECT},
    {LV_SYMBOL_SHUFFLE, "LICW Training", MODE_LICW_CAROUSEL_SELECT}
};
#define TRAINING_MENU_COUNT 5

// Games submenu items
static const LVMenuItem gamesMenuItems[] = {
    {LV_SYMBOL_PLAY, "Morse Shooter", MODE_MORSE_SHOOTER},
    {LV_SYMBOL_LOOP, "Memory Chain", MODE_MORSE_MEMORY},
    {LV_SYMBOL_AUDIO, "Spark Watch", MODE_SPARK_WATCH},
    {LV_SYMBOL_FILE, "Story Time", MODE_STORY_TIME},
    {LV_SYMBOL_CHARGE, "CW Speeder", MODE_CW_SPEEDER_SELECT}
};
#define GAMES_MENU_COUNT 5

// Settings submenu items
static const LVMenuItem settingsMenuItems[] = {
    {LV_SYMBOL_HOME, "Device Settings", MODE_DEVICE_SETTINGS_MENU},
    {LV_SYMBOL_AUDIO, "CW Settings", MODE_CW_SETTINGS}
};
#define SETTINGS_MENU_COUNT 2

// Device settings submenu items
static const LVMenuItem deviceSettingsMenuItems[] = {
    {LV_SYMBOL_WIFI, "WiFi", MODE_WIFI_SUBMENU},
    {LV_SYMBOL_SETTINGS, "General", MODE_GENERAL_SUBMENU},
    {LV_SYMBOL_BLUETOOTH, "Bluetooth", MODE_DEVICE_BT_SUBMENU},
    {LV_SYMBOL_HOME, "System Info", MODE_SYSTEM_INFO}
};
#define DEVICE_SETTINGS_COUNT 4

// WiFi submenu items
static const LVMenuItem wifiSubmenuItems[] = {
    {LV_SYMBOL_WIFI, "WiFi Setup", MODE_WIFI_SETTINGS},
    {LV_SYMBOL_EYE_CLOSE, "Web Password", MODE_WEB_PASSWORD_SETTINGS},
    {LV_SYMBOL_DOWNLOAD, "Web Files", MODE_WEB_FILES_UPDATE}
};
#define WIFI_SUBMENU_COUNT 3

// General submenu items
static const LVMenuItem generalSubmenuItems[] = {
    {LV_SYMBOL_CALL, "Callsign", MODE_CALLSIGN_SETTINGS},
    {LV_SYMBOL_VOLUME_MAX, "Volume", MODE_VOLUME_SETTINGS},
    {LV_SYMBOL_IMAGE, "Brightness", MODE_BRIGHTNESS_SETTINGS},
    {LV_SYMBOL_EYE_OPEN, "UI Theme", MODE_THEME_SETTINGS}
};
#define GENERAL_SUBMENU_COUNT 4

// Ham Tools submenu items
static const LVMenuItem hamToolsMenuItems[] = {
    {LV_SYMBOL_SAVE, "QSO Logger", MODE_QSO_LOGGER_MENU},
    {LV_SYMBOL_GPS, "POTA", MODE_POTA_MENU},
    {LV_SYMBOL_LIST, "Band Plans", MODE_BAND_PLANS},
    {LV_SYMBOL_REFRESH, "Band Conditions", MODE_PROPAGATION},
    {LV_SYMBOL_CHARGE, "Antennas", MODE_ANTENNAS},
    {LV_SYMBOL_FILE, "License Study", MODE_LICENSE_SELECT},
    {LV_SYMBOL_ENVELOPE, "Summit Chat", MODE_SUMMIT_CHAT}
};
#define HAM_TOOLS_COUNT 7

// Bluetooth submenu items
static const LVMenuItem bluetoothMenuItems[] = {
    {LV_SYMBOL_KEYBOARD, "HID (Keyboard)", MODE_BT_HID},
    {LV_SYMBOL_AUDIO, "MIDI", MODE_BT_MIDI}
};
#define BLUETOOTH_MENU_COUNT 2

// QSO Logger submenu items
static const LVMenuItem qsoLoggerMenuItems[] = {
    {LV_SYMBOL_PLUS, "New Log Entry", MODE_QSO_LOG_ENTRY},
    {LV_SYMBOL_LIST, "View Logs", MODE_QSO_VIEW_LOGS},
    {LV_SYMBOL_IMAGE, "Statistics", MODE_QSO_STATISTICS},
    {LV_SYMBOL_SETTINGS, "Logger Settings", MODE_QSO_LOGGER_SETTINGS}
};
#define QSO_LOGGER_COUNT 4

// ============================================
// Screen Objects
// ============================================

static lv_obj_t* current_menu_screen = NULL;
static lv_obj_t* menu_list = NULL;
static lv_obj_t* status_bar = NULL;
static lv_obj_t* wifi_status_icon = NULL;  // Global reference for dynamic updates
static int current_menu_item_count = 0;

// Callback for menu item selection (to be set by main app)
typedef void (*MenuSelectCallback)(int target_mode);
static MenuSelectCallback menu_select_callback = NULL;

// ============================================
// Menu Item Click Handler
// ============================================

static void menu_item_click_handler(lv_event_t* e) {
    lv_obj_t* target = lv_event_get_target(e);
    int target_mode = (int)(intptr_t)lv_obj_get_user_data(target);

    if (menu_select_callback != NULL) {
        menu_select_callback(target_mode);
    }
}

// ============================================
// 2D Grid Navigation
// ============================================

// Store menu button references for grid navigation
#define MAX_MENU_BUTTONS 16
static lv_obj_t* menu_buttons[MAX_MENU_BUTTONS] = {NULL};
static int menu_button_count = 0;

// Navigation context for menu grid (2 columns)
static NavGridContext menu_nav_ctx = { menu_buttons, &menu_button_count, 2 };

// ============================================
// Create Menu Screen
// ============================================

// Menu header height constant (used for content positioning)
static const int MENU_HEADER_HEIGHT = 50;

// Status bar globals from status_bar.h
extern int batteryPercent;
extern bool wifiConnected;

// Mailbox status icon (for unread indicator)
static lv_obj_t* mailbox_status_icon = NULL;

// Forward declaration for mailbox unread check
extern bool hasUnreadMailboxMessages();
extern bool isMailboxLinked();

/*
 * Create a modern header bar with title and status icons (battery, WiFi, Mailbox)
 */
lv_obj_t* createHeader(lv_obj_t* parent, const char* title) {
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_size(header, LV_PCT(100), MENU_HEADER_HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 10, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Title - use theme font
    lv_obj_t* lbl_title = lv_label_create(header);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_font(lbl_title, getThemeFonts()->font_input, 0);  // Theme font
    lv_obj_set_style_text_color(lbl_title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 5, 0);

    // Mailbox icon (envelope) - shows when unread messages exist
    mailbox_status_icon = lv_label_create(header);
    lv_label_set_text(mailbox_status_icon, LV_SYMBOL_ENVELOPE);
    lv_obj_set_style_text_font(mailbox_status_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(mailbox_status_icon, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(mailbox_status_icon, LV_ALIGN_RIGHT_MID, -85, 0);
    // Hide by default - only show when there are unread messages
    if (!isMailboxLinked() || !hasUnreadMailboxMessages()) {
        lv_obj_add_flag(mailbox_status_icon, LV_OBJ_FLAG_HIDDEN);
    }

    // WiFi icon - use Montserrat for LVGL symbols
    // Color indicates connectivity state:
    //   - Green: Full internet connectivity (or checking - optimistic)
    //   - Orange: WiFi connected but no internet verified
    //   - Red: Disconnected
    wifi_status_icon = lv_label_create(header);  // Store globally for dynamic updates
    lv_label_set_text(wifi_status_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(wifi_status_icon, &lv_font_montserrat_20, 0);
    InternetStatus inetStatus = getInternetStatus();
    if (inetStatus == INET_CONNECTED || inetStatus == INET_CHECKING) {
        lv_obj_set_style_text_color(wifi_status_icon, LV_COLOR_SUCCESS, 0);
    } else if (inetStatus == INET_WIFI_ONLY) {
        lv_obj_set_style_text_color(wifi_status_icon, LV_COLOR_WARNING, 0);
    } else {
        lv_obj_set_style_text_color(wifi_status_icon, LV_COLOR_ERROR, 0);
    }
    lv_obj_align(wifi_status_icon, LV_ALIGN_RIGHT_MID, -50, 0);

    // Battery icon - use Montserrat for LVGL symbols
    lv_obj_t* batt_icon = lv_label_create(header);
    lv_obj_set_style_text_font(batt_icon, &lv_font_montserrat_20, 0);
    lv_obj_align(batt_icon, LV_ALIGN_RIGHT_MID, -10, 0);

    if (batteryPercent > 80) {
        lv_label_set_text(batt_icon, LV_SYMBOL_BATTERY_FULL);
        lv_obj_set_style_text_color(batt_icon, LV_COLOR_SUCCESS, 0);
    } else if (batteryPercent > 60) {
        lv_label_set_text(batt_icon, LV_SYMBOL_BATTERY_3);
        lv_obj_set_style_text_color(batt_icon, LV_COLOR_SUCCESS, 0);
    } else if (batteryPercent > 40) {
        lv_label_set_text(batt_icon, LV_SYMBOL_BATTERY_2);
        lv_obj_set_style_text_color(batt_icon, LV_COLOR_ACCENT_CYAN, 0);
    } else if (batteryPercent > 20) {
        lv_label_set_text(batt_icon, LV_SYMBOL_BATTERY_1);
        lv_obj_set_style_text_color(batt_icon, LV_COLOR_WARNING, 0);
    } else {
        lv_label_set_text(batt_icon, LV_SYMBOL_BATTERY_EMPTY);
        lv_obj_set_style_text_color(batt_icon, LV_COLOR_ERROR, 0);
    }

    return header;
}

/*
 * Update the mailbox status icon visibility based on unread messages
 * Call this after polling or reading messages
 */
void updateMailboxStatusIcon() {
    if (mailbox_status_icon == NULL || !lv_obj_is_valid(mailbox_status_icon)) {
        return;  // No icon to update or icon was deleted
    }

    if (isMailboxLinked() && hasUnreadMailboxMessages()) {
        lv_obj_clear_flag(mailbox_status_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(mailbox_status_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

/*
 * Create a generic menu screen with modern LVGL layout
 * Uses lv_btn for menu items with proper focus handling
 */
lv_obj_t* createMenuScreen(const char* title, const LVMenuItem* items, int item_count) {
    // Clear menu button tracking array for 2D navigation
    for (int i = 0; i < MAX_MENU_BUTTONS; i++) {
        menu_buttons[i] = NULL;
    }
    menu_button_count = 0;

    // Create screen with dark background
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, LV_COLOR_BG_DEEP, 0);

    // Header
    createHeader(screen, title);

    // Content area - positioned below header with proper spacing
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, LV_PCT(100), SCREEN_HEIGHT - MENU_HEADER_HEIGHT - FOOTER_HEIGHT - 10);
    lv_obj_set_pos(content, 0, MENU_HEADER_HEIGHT + 5);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 10, 0);
    lv_obj_set_style_pad_row(content, 10, 0);  // Gap between rows
    lv_obj_set_style_pad_column(content, 20, 0);  // Gap between columns
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW_WRAP);
    // Use START for main axis to prevent items being pushed above container
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Enable vertical scrolling for menus with many items
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

    // Create menu buttons
    for (int i = 0; i < item_count && i < MAX_MENU_BUTTONS; i++) {
        // Create button with proper styling
        // Size: 200x80 to fit more items on screen (3 rows visible at once)
        lv_obj_t* btn = lv_btn_create(content);
        lv_obj_set_size(btn, 200, 80);

        // Apply styles
        lv_obj_add_style(btn, getStyleMenuCard(), 0);
        lv_obj_add_style(btn, getStyleMenuCardFocused(), LV_STATE_FOCUSED);

        // Container for icon and text
        lv_obj_t* col = lv_obj_create(btn);
        lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(col, 0, 0);
        lv_obj_set_style_pad_all(col, 0, 0);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(col, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);

        // Icon (use symbol) - always use Montserrat for LVGL symbols
        lv_obj_t* icon = lv_label_create(col);
        lv_label_set_text(icon, items[i].icon);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);  // Montserrat has LVGL symbols
        lv_obj_set_style_text_color(icon, LV_COLOR_TEXT_PRIMARY, 0);

        // Text - use theme font for menu labels
        lv_obj_t* lbl = lv_label_create(col);
        lv_label_set_text(lbl, items[i].title);
        lv_obj_set_style_text_font(lbl, getThemeFonts()->font_input, 0);  // Theme font
        lv_obj_set_style_text_color(lbl, LV_COLOR_TEXT_PRIMARY, 0);

        // Store target mode and add click handler
        lv_obj_set_user_data(btn, (void*)(intptr_t)items[i].target_mode);
        lv_obj_add_event_cb(btn, menu_item_click_handler, LV_EVENT_CLICKED, (void*)(intptr_t)items[i].target_mode);

        // Add 2D grid navigation handler for all arrow keys
        lv_obj_add_event_cb(btn, grid_nav_handler, LV_EVENT_KEY, &menu_nav_ctx);

        // Store button reference for 2D navigation
        menu_buttons[i] = btn;
        menu_button_count++;

        // Add to navigation group
        addNavigableWidget(btn);
    }

    // Ensure content scroll starts at top (fixes items appearing above screen)
    lv_obj_scroll_to_y(content, 0, LV_ANIM_OFF);

    // Footer hint - use menu footer with volume shortcut hint
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, FOOTER_MENU_WITH_VOLUME);
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_body, 0);  // Theme font
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);  // Orange for visibility
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -5);

    current_menu_item_count = item_count;

    return screen;
}

// ============================================
// Specific Menu Screen Creators
// ============================================

/*
 * Create main menu screen
 */
lv_obj_t* createMainMenuScreen() {
    return createMenuScreen("VAIL SUMMIT", mainMenuItems, MAIN_MENU_COUNT);
}

/*
 * Create CW menu screen
 */
lv_obj_t* createCWMenuScreen() {
    return createMenuScreen("CW", cwMenuItems, CW_MENU_COUNT);
}

/*
 * Create Training menu screen
 */
lv_obj_t* createTrainingMenuScreen() {
    return createMenuScreen("TRAINING", trainingMenuItems, TRAINING_MENU_COUNT);
}

/*
 * Create Games menu screen
 */
lv_obj_t* createGamesMenuScreen() {
    return createMenuScreen("GAMES", gamesMenuItems, GAMES_MENU_COUNT);
}

/*
 * Create Settings menu screen
 */
lv_obj_t* createSettingsMenuScreen() {
    return createMenuScreen("SETTINGS", settingsMenuItems, SETTINGS_MENU_COUNT);
}

/*
 * Create Device Settings menu screen
 */
lv_obj_t* createDeviceSettingsMenuScreen() {
    return createMenuScreen("DEVICE SETTINGS", deviceSettingsMenuItems, DEVICE_SETTINGS_COUNT);
}

/*
 * Create WiFi submenu screen
 */
lv_obj_t* createWiFiSubmenuScreen() {
    return createMenuScreen("WIFI", wifiSubmenuItems, WIFI_SUBMENU_COUNT);
}

/*
 * Create General submenu screen
 */
lv_obj_t* createGeneralSubmenuScreen() {
    return createMenuScreen("GENERAL", generalSubmenuItems, GENERAL_SUBMENU_COUNT);
}

/*
 * Create Ham Tools menu screen
 */
lv_obj_t* createHamToolsMenuScreen() {
    return createMenuScreen("HAM TOOLS", hamToolsMenuItems, HAM_TOOLS_COUNT);
}

/*
 * Create Bluetooth menu screen
 */
lv_obj_t* createBluetoothMenuScreen() {
    return createMenuScreen("BLUETOOTH", bluetoothMenuItems, BLUETOOTH_MENU_COUNT);
}

/*
 * Create QSO Logger menu screen
 */
lv_obj_t* createQSOLoggerMenuScreen() {
    return createMenuScreen("QSO LOGGER", qsoLoggerMenuItems, QSO_LOGGER_COUNT);
}

// ============================================
// Coming Soon Screen
// ============================================

/*
 * Create a "Coming Soon" placeholder screen
 */
lv_obj_t* createComingSoonScreen(const char* feature_name) {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Centered content
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, 400, 200);
    lv_obj_center(content);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 15, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);

    // Feature name
    lv_obj_t* title = lv_label_create(content);
    lv_label_set_text(title, feature_name);
    lv_obj_add_style(title, getStyleLabelTitle(), 0);

    // Coming Soon text - use theme font
    lv_obj_t* coming = lv_label_create(content);
    lv_label_set_text(coming, "Coming Soon");
    lv_obj_set_style_text_color(coming, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(coming, getThemeFonts()->font_title, 0);  // Theme font

    // Description
    lv_obj_t* desc = lv_label_create(content);
    lv_label_set_text(desc, "This feature is under development");
    lv_obj_add_style(desc, getStyleLabelBody(), 0);

    // ESC instruction - use theme font
    lv_obj_t* esc = lv_label_create(content);
    lv_label_set_text(esc, "Press ESC to go back");
    lv_obj_set_style_text_color(esc, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(esc, getThemeFonts()->font_body, 0);  // Theme font

    // Invisible focusable container for ESC key handling
    // Without a navigable widget, ESC events are never processed
    lv_obj_t* focus_target = lv_obj_create(screen);
    lv_obj_set_size(focus_target, 1, 1);
    lv_obj_set_style_bg_opa(focus_target, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus_target, 0, 0);
    lv_obj_add_flag(focus_target, LV_OBJ_FLAG_CLICKABLE);
    addNavigableWidget(focus_target);

    return screen;
}

// ============================================
// Menu Navigation API
// ============================================

/*
 * Set the callback for menu item selection
 */
void setMenuSelectCallback(MenuSelectCallback callback) {
    menu_select_callback = callback;
}

/*
 * Get the current menu screen
 */
lv_obj_t* getCurrentMenuScreen() {
    return current_menu_screen;
}

/*
 * Update the WiFi status icon color based on current internet status
 * Call this when internet status changes to update the display immediately
 */
void updateWiFiStatusIcon() {
    if (wifi_status_icon == NULL || !lv_obj_is_valid(wifi_status_icon)) {
        return;  // No icon to update or icon was deleted
    }

    InternetStatus inetStatus = getInternetStatus();
    if (inetStatus == INET_CONNECTED || inetStatus == INET_CHECKING) {
        lv_obj_set_style_text_color(wifi_status_icon, LV_COLOR_SUCCESS, 0);
    } else if (inetStatus == INET_WIFI_ONLY) {
        lv_obj_set_style_text_color(wifi_status_icon, LV_COLOR_WARNING, 0);
    } else {
        lv_obj_set_style_text_color(wifi_status_icon, LV_COLOR_ERROR, 0);
    }
}

#endif // LV_MENU_SCREENS_H
