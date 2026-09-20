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
#include "../core/firebase_availability.h"

// Forward declarations from main file
extern int currentSelection;

// ============================================
// Menu Data Structures
// ============================================

// Menu item structure for LVGL menus (LVGL 8.3.x — see extra_font_awesome_icons.h for FA + font docs)
struct LVMenuItem {
    const char* icon;
    const char* title;
    int target_mode;           // MenuMode enum value to switch to
    const lv_font_t* icon_font;  // NULL: Montserrat 24 + LV_SYMBOL_*; else UTF-8 + that font (e.g. ExtraFontAwesomeIcons)
    const char* desc;            // one line under the title on row menus; NULL renders title only
};

// Montserrat symbol row / Font Awesome UTF-8 row (LVGL 8 font + label pattern; extra_font_awesome_icons.h)
#define MENU_ITEM_LV(sym, title, mode)  { (sym), (title), (mode), NULL }
#define MENU_ITEM_FA(utf8, title, mode) { (utf8), (title), (mode), &ExtraFontAwesomeIcons }
// _D variants carry the description shown on row menus.
#define MENU_ITEM_LV_D(sym, title, mode, d)  { (sym), (title), (mode), NULL, (d) }
#define MENU_ITEM_FA_D(utf8, title, mode, d) { (utf8), (title), (mode), &ExtraFontAwesomeIcons, (d) }

// ============================================
// Menu Data
// Mode values from MenuMode enum in src/core/modes.h
// ============================================

// Main menu items - using LVGL symbols for modern look
static const LVMenuItem mainMenuItems[] = {
    MENU_ITEM_LV_D(LV_SYMBOL_AUDIO, "CW", MODE_CW_MENU, "Learn, practice, get on air"),
    MENU_ITEM_FA_D(FA_EXTRA_TOOLS, "Ham Tools", MODE_HAM_TOOLS_MENU, "Log, POTA, bands, satellites"),
    MENU_ITEM_LV_D(LV_SYMBOL_SETTINGS, "Settings", MODE_SETTINGS_MENU, "WiFi, audio, device")
};
#define MAIN_MENU_COUNT 3

// CW submenu items
static const LVMenuItem cwMenuItems[] = {
    MENU_ITEM_FA_D(FA_EXTRA_DUMBBELL, "Training", MODE_TRAINING_MENU, "Structured lessons"),
    MENU_ITEM_FA_D(FA_EXTRA_BOOK_OPEN, "Practice", MODE_PRACTICE, "Oscillator and decoder"),
    MENU_ITEM_FA_D(FA_EXTRA_COMMENTS, "Vail Repeater", MODE_VAIL_REPEATER, "CW over the internet"),
    MENU_ITEM_LV_D(LV_SYMBOL_ENVELOPE, "Morse Mailbox", MODE_MORSE_MAILBOX, "Send and receive notes"),
    MENU_ITEM_FA_D(FA_EXTRA_STICKY_NOTE, "Morse Notes", MODE_MORSE_NOTES_LIBRARY, "Record and play back"),
    MENU_ITEM_LV_D(LV_SYMBOL_BLUETOOTH, "Bluetooth", MODE_BLUETOOTH_MENU, "Keyer, MIDI, keyboard"),
    MENU_ITEM_LV_D(LV_SYMBOL_POWER, "Radio Output", MODE_RADIO_OUTPUT, "Key an external rig"),
    MENU_ITEM_LV_D(LV_SYMBOL_SAVE, "CW Memories", MODE_CW_MEMORIES, "Stored messages")
};
#define CW_MENU_COUNT 8

// Training submenu — collapsed to the single Vail CW School path. The other
// curricula (Vail Master, Hear It Type It, CW Academy, LICW) remain in the
// codebase but are no longer listed; the school hub is the one learning path.
static const LVMenuItem trainingMenuItems[] = {
    MENU_ITEM_FA_D(FA_EXTRA_SCHOOL, "Learn CW", MODE_SCHOOL_HUB, "Your course, start to finish")
};
#define TRAINING_MENU_COUNT 1

// Games submenu items
static const LVMenuItem gamesMenuItems[] = {
    MENU_ITEM_LV_D(LV_SYMBOL_PLAY, "Morse Shooter", MODE_MORSE_SHOOTER, "Shoot falling characters"),
    MENU_ITEM_LV_D(LV_SYMBOL_LOOP, "Memory Chain", MODE_MORSE_MEMORY, "Repeat a growing sequence"),
    MENU_ITEM_LV_D(LV_SYMBOL_AUDIO, "Spark Watch", MODE_SPARK_WATCH, "Maritime signal drama"),
    MENU_ITEM_LV_D(LV_SYMBOL_FILE, "Story Time", MODE_STORY_TIME, "Copy along with a story"),
    MENU_ITEM_LV_D(LV_SYMBOL_CHARGE, "CW Speeder", MODE_CW_SPEEDER_SELECT, "Push your top speed")
};
#define GAMES_MENU_COUNT 5

// Settings submenu items
static const LVMenuItem settingsMenuItems[] = {
    MENU_ITEM_LV_D(LV_SYMBOL_HOME, "Device Settings", MODE_DEVICE_SETTINGS_MENU, "WiFi, audio, system"),
    MENU_ITEM_LV_D(LV_SYMBOL_AUDIO, "CW Settings", MODE_CW_SETTINGS, "Speed, tone, key type")
};
#define SETTINGS_MENU_COUNT 2

// Device settings submenu items
static const LVMenuItem deviceSettingsMenuItems[] = {
    MENU_ITEM_LV_D(LV_SYMBOL_WIFI, "WiFi", MODE_WIFI_SUBMENU, "Join a network"),
    MENU_ITEM_LV_D(LV_SYMBOL_SETTINGS, "General", MODE_GENERAL_SUBMENU, "Volume, brightness, theme"),
    MENU_ITEM_LV_D(LV_SYMBOL_BLUETOOTH, "Bluetooth", MODE_DEVICE_BT_SUBMENU, "Keyer, MIDI, keyboard"),
    MENU_ITEM_LV_D(LV_SYMBOL_HOME, "System Info", MODE_SYSTEM_INFO, "Version, storage, battery"),
    MENU_ITEM_LV_D(LV_SYMBOL_TRASH, "Factory Reset", MODE_FACTORY_RESET, "Erase everything")
};
#define DEVICE_SETTINGS_COUNT 5

// WiFi submenu items
static const LVMenuItem wifiSubmenuItems[] = {
    MENU_ITEM_LV_D(LV_SYMBOL_WIFI, "WiFi Setup", MODE_WIFI_SETTINGS, "Join a network"),
    MENU_ITEM_LV_D(LV_SYMBOL_EYE_CLOSE, "Web Password", MODE_WEB_PASSWORD_SETTINGS, "Protect the web interface"),
    MENU_ITEM_LV_D(LV_SYMBOL_DOWNLOAD, "Web Files", MODE_WEB_FILES_UPDATE, "Update the browser pages")
};
#define WIFI_SUBMENU_COUNT 3

// General submenu items
static const LVMenuItem generalSubmenuItems[] = {
    MENU_ITEM_LV_D(LV_SYMBOL_HOME, "Device Tour", MODE_ONBOARDING, "Walk through the basics"),
    MENU_ITEM_LV_D(LV_SYMBOL_CALL, "Callsign", MODE_CALLSIGN_SETTINGS, "Your call, used everywhere"),
    MENU_ITEM_LV_D(LV_SYMBOL_VOLUME_MAX, "Volume", MODE_VOLUME_SETTINGS, "Speaker and headphone levels"),
    MENU_ITEM_LV_D(LV_SYMBOL_IMAGE, "Brightness", MODE_BRIGHTNESS_SETTINGS, "Screen backlight"),
    MENU_ITEM_LV_D(LV_SYMBOL_EYE_OPEN, "UI Theme", MODE_THEME_SETTINGS, "Summit or Enigma look")
};
#define GENERAL_SUBMENU_COUNT 5

// Ham Tools submenu items
static const LVMenuItem hamToolsMenuItems[] = {
    MENU_ITEM_LV_D(LV_SYMBOL_SAVE, "QSO Logger", MODE_QSO_LOGGER_MENU, "Record your contacts"),
    MENU_ITEM_FA_D(FA_EXTRA_TREE, "POTA", MODE_POTA_MENU, "Parks on the air"),
    MENU_ITEM_LV_D(LV_SYMBOL_LIST, "Band Plans", MODE_BAND_PLANS, "Frequencies by licence class"),
    MENU_ITEM_FA_D(FA_EXTRA_CLOUD_SUN_RAIN, "Band Conditions", MODE_PROPAGATION, "Solar and propagation now"),
    MENU_ITEM_FA_D(FA_EXTRA_SATELLITE_DISH, "Satellites", MODE_SAT_MENU, "Passes and tracking"),
    MENU_ITEM_LV_D(LV_SYMBOL_CHARGE, "Antennas", MODE_ANTENNAS, "Reference and calculators"),
    MENU_ITEM_FA_D(FA_EXTRA_EDIT, "License Study", MODE_LICENSE_SELECT, "Practice exam questions"),
    MENU_ITEM_LV_D(LV_SYMBOL_ENVELOPE, "Summit Chat", MODE_SUMMIT_CHAT, "Message other Summits")
};
#define HAM_TOOLS_COUNT 8

// Bluetooth submenu items
static const LVMenuItem bluetoothMenuItems[] = {
    MENU_ITEM_LV_D(LV_SYMBOL_KEYBOARD, "HID (Keyboard)", MODE_BT_HID, "Key software as a keyboard"),
    MENU_ITEM_LV_D(LV_SYMBOL_AUDIO, "MIDI", MODE_BT_MIDI, "Send CW as BLE MIDI")
};
#define BLUETOOTH_MENU_COUNT 2

// Device settings Bluetooth submenu items
static const LVMenuItem deviceBTSubmenuItems[] = {
    MENU_ITEM_LV_D(LV_SYMBOL_KEYBOARD, "External Keyboard", MODE_BT_KEYBOARD_SETTINGS, "Pair a Bluetooth keyboard")
};
#define DEVICE_BT_SUBMENU_COUNT 1

// QSO Logger submenu items
static const LVMenuItem qsoLoggerMenuItems[] = {
    MENU_ITEM_LV_D(LV_SYMBOL_PLUS, "New Log Entry", MODE_QSO_LOG_ENTRY, "Log a contact now"),
    MENU_ITEM_LV_D(LV_SYMBOL_LIST, "View Logs", MODE_QSO_VIEW_LOGS, "Browse and export"),
    MENU_ITEM_LV_D(LV_SYMBOL_IMAGE, "Statistics", MODE_QSO_STATISTICS, "Totals, bands, modes"),
    MENU_ITEM_LV_D(LV_SYMBOL_SETTINGS, "Logger Settings", MODE_QSO_LOGGER_SETTINGS, "Operator and defaults")
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
static NavGridContext menu_row_nav_ctx = { menu_buttons, &menu_button_count, 1 };

// ============================================
// Focus Memory
// ============================================
// When the user backs out of a screen into a menu, focus returns to the item
// they originally selected instead of resetting to the first item.

// Index to focus when the next menu screen is created (-1 = first item)
static int menu_focus_restore_index = -1;

void setMenuFocusRestoreIndex(int idx) {
    menu_focus_restore_index = idx;
}

/*
 * Get the index of the currently focused menu button, or -1 if focus
 * is not on one of this menu's buttons.
 */
int getMenuFocusedIndex() {
    lv_group_t* group = getLVGLInputGroup();
    if (group == NULL) return -1;
    lv_obj_t* focused = lv_group_get_focused(group);
    if (focused == NULL) return -1;
    for (int i = 0; i < menu_button_count; i++) {
        if (menu_buttons[i] == focused) return i;
    }
    return -1;
}

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

    lv_obj_t* lbl_title = lv_label_create(header);
    lv_label_set_text(lbl_title, title);
    lv_obj_add_style(lbl_title, getStyleLabelTitle(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 15, 0);

    // Mailbox icon (envelope) - shows when unread messages exist
    mailbox_status_icon = lv_label_create(header);
    lv_label_set_text(mailbox_status_icon, LV_SYMBOL_ENVELOPE);
    lv_obj_set_style_text_font(mailbox_status_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(mailbox_status_icon, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(mailbox_status_icon, LV_ALIGN_RIGHT_MID, -85, 0);
    // Hide by default - only show when there are unread messages (requires Firebase key)
    if (!morseMailboxFirebaseConfigured() || !isMailboxLinked() || !hasUnreadMailboxMessages()) {
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
        lv_obj_set_style_text_color(batt_icon, LV_COLOR_ACCENT_PRIMARY, 0);
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

    if (morseMailboxFirebaseConfigured() && isMailboxLinked() && hasUnreadMailboxMessages()) {
        lv_obj_clear_flag(mailbox_status_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(mailbox_status_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

/*
 * Create a generic menu screen with modern LVGL layout
 * Uses lv_btn for menu items with proper focus handling
 */
// Row menu. Everything one level or more below the top menu uses this: same
// header bar, but full width rows with an icon chip, the title, an optional
// description and a chevron. Four rows fit per screen, so the longest menus
// (CW and Ham Tools, 8 items) are two pages.
//
// The old three part footer hint is gone on purpose. Arrows, ENTER and ESC are
// learned in seconds and do not need permanent space on every menu; that
// guidance lives in onboarding and in help instead.
lv_obj_t* createMenuScreen(const char* title, const LVMenuItem* items, int item_count) {
    for (int i = 0; i < MAX_MENU_BUTTONS; i++) menu_buttons[i] = NULL;
    menu_button_count = 0;

    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, LV_COLOR_BG_DEEP, 0);

    createHeader(screen, title);

    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, LV_PCT(100), SCREEN_HEIGHT - MENU_HEADER_HEIGHT - 12);
    lv_obj_set_pos(content, 0, MENU_HEADER_HEIGHT + 6);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_hor(content, 20, 0);
    lv_obj_set_style_pad_ver(content, 0, 0);
    lv_obj_set_style_pad_row(content, 7, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_style(content, getStyleScrollbar(), LV_PART_SCROLLBAR);

    for (int i = 0; i < item_count && i < MAX_MENU_BUTTONS; i++) {
        lv_obj_t* row = lv_btn_create(content);
        lv_obj_set_size(row, LV_PCT(100), 54);
        lv_obj_set_style_bg_color(row, LV_COLOR_BG_CARD, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_set_style_border_width(row, 2, 0);
        lv_obj_set_style_border_color(row, LV_COLOR_BORDER_SUBTLE, 0);
        lv_obj_set_style_border_color(row, LV_COLOR_ACCENT_PRIMARY, LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_pad_hor(row, 14, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* chip = lv_obj_create(row);
        lv_obj_set_size(chip, 34, 34);
        lv_obj_set_style_radius(chip, 9, 0);
        lv_obj_set_style_bg_color(chip, LV_COLOR_BG_CARD_ALT, 0);
        lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(chip, 0, 0);
        lv_obj_set_style_pad_all(chip, 0, 0);
        lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(chip, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(chip, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* icon = lv_label_create(chip);
        lv_label_set_text(icon, items[i].icon);
        lv_obj_set_style_text_font(icon,
            items[i].icon_font ? items[i].icon_font : &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(icon, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_center(icon);

        bool has_desc = (items[i].desc != NULL && items[i].desc[0] != '\0');

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, items[i].title);
        lv_obj_set_style_text_font(lbl, getThemeFonts()->font_input, 0);
        lv_obj_set_style_text_color(lbl, LV_COLOR_TEXT_PRIMARY, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 46, has_desc ? -9 : 0);

        if (has_desc) {
            lv_obj_t* sub = lv_label_create(row);
            lv_label_set_text(sub, items[i].desc);
            lv_obj_set_style_text_font(sub, getThemeFonts()->font_small, 0);
            lv_obj_set_style_text_color(sub, LV_COLOR_TEXT_SECONDARY, 0);
            lv_obj_align(sub, LV_ALIGN_LEFT_MID, 46, 10);
        }

        lv_obj_t* chev = lv_label_create(row);
        lv_label_set_text(chev, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_font(chev, getThemeFonts()->font_body, 0);
        lv_obj_set_style_text_color(chev, LV_COLOR_TEXT_TERTIARY, 0);
        lv_obj_set_style_text_color(chev, LV_COLOR_ACCENT_PRIMARY, LV_STATE_FOCUSED);
        lv_obj_align(chev, LV_ALIGN_RIGHT_MID, 0, 0);

        lv_obj_set_user_data(row, (void*)(intptr_t)items[i].target_mode);
        lv_obj_add_event_cb(row, menu_item_click_handler, LV_EVENT_CLICKED,
                            (void*)(intptr_t)items[i].target_mode);
        lv_obj_add_event_cb(row, grid_nav_handler, LV_EVENT_KEY, &menu_row_nav_ctx);
        menu_buttons[i] = row;
        menu_button_count++;
        addNavigableWidget(row);   // always last
    }

    if (menu_focus_restore_index > 0 && menu_focus_restore_index < menu_button_count &&
        menu_buttons[menu_focus_restore_index] != NULL) {
        lv_group_focus_obj(menu_buttons[menu_focus_restore_index]);
        lv_obj_scroll_to_view(menu_buttons[menu_focus_restore_index], LV_ANIM_OFF);
    } else {
        lv_obj_scroll_to_y(content, 0, LV_ANIM_OFF);
    }
    menu_focus_restore_index = -1;

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
    return createMenuScreen("MORE", mainMenuItems, MAIN_MENU_COUNT);
}

/*
 * Create CW menu screen
 */
lv_obj_t* createCWMenuScreen() {
    static LVMenuItem filtered[CW_MENU_COUNT];
    int n = 0;
    for (int i = 0; i < CW_MENU_COUNT; i++) {
        if (cwMenuItems[i].target_mode == MODE_MORSE_MAILBOX && !morseMailboxFirebaseConfigured()) {
            continue;
        }
        filtered[n++] = cwMenuItems[i];
    }
    return createMenuScreen("CW", filtered, n);
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
 * Create Device Settings Bluetooth submenu screen
 */
lv_obj_t* createDeviceBTSubmenuScreen() {
    return createMenuScreen("BLUETOOTH", deviceBTSubmenuItems, DEVICE_BT_SUBMENU_COUNT);
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
