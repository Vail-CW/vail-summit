/*
 * VAIL SUMMIT - LVGL Web Download Screen
 * Prompts user to download web files on first WiFi connection
 * Replaces legacy LovyanGFX-based UI with proper LVGL implementation
 */

#ifndef LV_WEB_DOWNLOAD_SCREEN_H
#define LV_WEB_DOWNLOAD_SCREEN_H

#include <lvgl.h>
#include "lv_theme_summit.h"
#include "lv_widgets_summit.h"
#include "lv_screen_manager.h"
#include "../web/server/web_file_downloader.h"
#include "../web/server/web_first_boot.h"
#include "../core/config.h"

// Forward declarations
extern void beep(int frequency, int duration);

// ============================================
// Screen State
// ============================================

enum WebDownloadUIState {
    WD_UI_IDLE,
    WD_UI_PROMPTING,
    WD_UI_DOWNLOADING,
    WD_UI_COMPLETE,
    WD_UI_ERROR
};

static WebDownloadUIState webDownloadUIState = WD_UI_IDLE;

// Screen and widget references
static lv_obj_t* web_download_screen = NULL;
static lv_obj_t* web_download_progress_bar = NULL;
static lv_obj_t* web_download_file_label = NULL;
static lv_obj_t* web_download_pct_label = NULL;
static lv_obj_t* web_download_status_label = NULL;
static lv_obj_t* web_download_footer = NULL;

// Track previous screen to return to
static lv_obj_t* previous_screen = NULL;

// ============================================
// Forward Declarations
// ============================================

void showWebFilesDownloadScreen();
void showWebFilesUpdateNotification();  // Notification-only (no download option)
void showWebFilesDownloadProgress();
void showWebFilesDownloadComplete(bool success, const char* message);
void updateWebDownloadProgressUI();
bool handleWebDownloadInput(char key);
bool isWebDownloadScreenActive();
void exitWebDownloadScreen();

// ============================================
// Helper Functions
// ============================================

/**
 * Create the title bar for web download screens
 */
static lv_obj_t* createWebDownloadTitleBar(lv_obj_t* parent, const char* title) {
    lv_obj_t* title_bar = lv_obj_create(parent);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title_label = lv_label_create(title_bar);
    lv_label_set_text(title_label, title);
    lv_obj_add_style(title_label, getStyleLabelTitle(), 0);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 10, 0);

    return title_bar;
}

/**
 * Create footer instruction text
 */
static lv_obj_t* createWebDownloadFooter(lv_obj_t* parent, const char* text) {
    lv_obj_t* footer = lv_obj_create(parent);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_color(footer, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* footer_label = lv_label_create(footer);
    lv_label_set_text(footer_label, text);
    lv_obj_set_style_text_color(footer_label, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(footer_label, getThemeFonts()->font_small, 0);
    lv_obj_center(footer_label);

    return footer;
}

// ============================================
// Prompt Screen
// ============================================

/**
 * Create an info row with label and value for the download screen
 */
static void createWebDownloadInfoRow(lv_obj_t* parent, const char* label, const char* value, lv_color_t valueColor) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_ver(row, 3, 0);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(lbl, getThemeFonts()->font_body, 0);

    lv_obj_t* val = lv_label_create(row);
    lv_label_set_text(val, value);
    lv_obj_set_style_text_color(val, valueColor, 0);
    lv_obj_set_style_text_font(val, getThemeFonts()->font_body, 0);
}

/**
 * Show the web files download/management screen
 * Shows version info and allows force download regardless of version
 */
void showWebFilesDownloadScreen() {
    Serial.println("[WebDownload] Showing download screen");

    // Store reference to return later
    previous_screen = lv_scr_act();

    // Create new screen
    web_download_screen = createScreen();
    applyScreenStyle(web_download_screen);

    // Clear navigation group for this screen
    clearNavigationGroup();

    // Title bar
    createWebDownloadTitleBar(web_download_screen, "WEB INTERFACE FILES");

    // Get version info
    String installedVer = getWebFilesVersion();
    installedVer.trim();
    bool hasFiles = !installedVer.isEmpty();
    bool versionsMatch = hasFiles && (installedVer == WEB_FILES_VERSION);

    // Main content card
    lv_obj_t* card = lv_obj_create(web_download_screen);
    lv_obj_set_size(card, 420, 170);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 10);
    applyCardStyle(card);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 15, 0);
    lv_obj_set_style_pad_row(card, 2, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Version info rows
    char expectedBuf[24];
    snprintf(expectedBuf, sizeof(expectedBuf), "v%s", WEB_FILES_VERSION);
    createWebDownloadInfoRow(card, "Expected:", expectedBuf, LV_COLOR_TEXT_PRIMARY);

    char installedBuf[24];
    if (hasFiles) {
        snprintf(installedBuf, sizeof(installedBuf), "v%s", installedVer.c_str());
    } else {
        snprintf(installedBuf, sizeof(installedBuf), "Not installed");
    }
    lv_color_t installedColor = versionsMatch ? LV_COLOR_TEXT_PRIMARY :
                                hasFiles ? LV_COLOR_WARNING : lv_color_hex(0xFF6666);
    createWebDownloadInfoRow(card, "Installed:", installedBuf, installedColor);

    // Status row
    const char* statusText;
    lv_color_t statusColor;
    if (versionsMatch) {
        statusText = "Up to date";
        statusColor = lv_color_hex(0x4CAF50);  // green
    } else if (hasFiles) {
        statusText = "Update available";
        statusColor = LV_COLOR_WARNING;
    } else {
        statusText = "Not installed";
        statusColor = lv_color_hex(0xFF6666);  // red
    }
    createWebDownloadInfoRow(card, "Status:", statusText, statusColor);

    // Description text
    lv_obj_t* desc = lv_label_create(card);
    lv_label_set_text(desc,
        "\nDevice will reboot to download\n"
        "the latest web interface files.");
    lv_obj_add_style(desc, getStyleLabelBody(), 0);
    lv_obj_set_style_text_color(desc, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_line_space(desc, 4, 0);

    // Button container
    lv_obj_t* btn_container = lv_obj_create(web_download_screen);
    lv_obj_set_size(btn_container, 420, 50);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, -(FOOTER_HEIGHT + 10));
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_layout(btn_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);

    // Grid navigation context for 2-column horizontal layout
    static lv_obj_t* download_buttons[2];
    static int download_button_count = 0;
    static NavGridContext download_grid_ctx = { download_buttons, &download_button_count, 2 };

    // Screen-specific KEY handler to intercept ESC before global handler
    // This prevents conflict with main loop's handleWebDownloadInput()
    auto screen_key_handler = [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_KEY) return;
        uint32_t key = lv_event_get_key(e);

        // ONLY intercept ESC - all other keys pass through to grid_nav_handler
        if (key == LV_KEY_ESC) {
            Serial.println("[WebDownload] Screen-specific ESC handler - stopping propagation");
            lv_event_stop_processing(e);  // Stop other handlers on this object
            lv_event_stop_bubbling(e);     // Stop parent handlers
            exitWebDownloadScreen();       // Handle locally (bypass main loop)
        }
        // All other keys (ENTER, arrows, etc.) continue to other handlers
    };

    // Download button
    lv_obj_t* btn_download = lv_btn_create(btn_container);
    lv_obj_set_size(btn_download, 160, 45);
    applyButtonStyle(btn_download);
    lv_obj_t* btn_download_label = lv_label_create(btn_download);
    lv_label_set_text(btn_download_label, LV_SYMBOL_DOWNLOAD " Download");
    lv_obj_center(btn_download_label);
    lv_obj_add_event_cb(btn_download, [](lv_event_t* e) {
        handleWebDownloadInput('Y');
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_download, screen_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(btn_download, grid_nav_handler, LV_EVENT_KEY, &download_grid_ctx);
    download_buttons[download_button_count++] = btn_download;
    addNavigableWidget(btn_download);

    // Back button
    lv_obj_t* btn_back = lv_btn_create(btn_container);
    lv_obj_set_size(btn_back, 160, 45);
    applyButtonStyle(btn_back);
    lv_obj_t* btn_back_label = lv_label_create(btn_back);
    lv_label_set_text(btn_back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(btn_back_label);
    lv_obj_add_event_cb(btn_back, [](lv_event_t* e) {
        handleWebDownloadInput('N');
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_back, screen_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(btn_back, grid_nav_handler, LV_EVENT_KEY, &download_grid_ctx);
    download_buttons[download_button_count++] = btn_back;
    addNavigableWidget(btn_back);

    // Footer
    web_download_footer = createWebDownloadFooter(web_download_screen, "ENTER: Download    ESC: Back");

    // Load screen
    loadScreen(web_download_screen, SCREEN_ANIM_FADE);

    // Focus download button
    focusWidget(btn_download);

    webDownloadUIState = WD_UI_PROMPTING;
    webFilesDownloadPromptShown = true;

    // Play notification sound
    beep(TONE_MENU_NAV, BEEP_MEDIUM);
}

// ============================================
// Update Notification Screen (Notification Only - No Download)
// ============================================

/**
 * Show notification that web files update is available
 * Directs user to WiFi Settings to download - no download option on this screen
 */
void showWebFilesUpdateNotification() {
    Serial.println("[WebDownload] Showing update notification screen");

    // Store reference to return later
    previous_screen = lv_scr_act();

    // Create new screen
    web_download_screen = createScreen();
    applyScreenStyle(web_download_screen);

    // Clear navigation group for this screen
    clearNavigationGroup();

    // Check if this is an update or fresh install
    bool isUpdate = isWebFilesUpdatePrompt();

    // Title bar
    createWebDownloadTitleBar(web_download_screen, isUpdate ? "Web Files Update Available" : "Web Files Missing");

    // Main content card
    lv_obj_t* card = lv_obj_create(web_download_screen);
    lv_obj_set_size(card, 420, 180);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, -10);
    applyCardStyle(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Info icon
    lv_obj_t* icon = lv_label_create(card);
    lv_label_set_text(icon, isUpdate ? LV_SYMBOL_REFRESH : LV_SYMBOL_DOWNLOAD);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(icon, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 10, 10);

    // Message text - directs user to WiFi Settings
    lv_obj_t* msg = lv_label_create(card);
    if (isUpdate) {
        lv_label_set_text(msg,
            "A new version of the web interface\n"
            "is available.\n\n"
            "To download, go to:\n"
            "Settings > WiFi > Web Files Update");
    } else {
        lv_label_set_text(msg,
            "SD card detected but web interface\n"
            "files are missing.\n\n"
            "To download, go to:\n"
            "Settings > WiFi > Web Files Update");
    }
    lv_obj_add_style(msg, getStyleLabelBody(), 0);
    lv_obj_set_style_text_line_space(msg, 4, 0);
    lv_obj_align(msg, LV_ALIGN_TOP_LEFT, 50, 10);

    // OK button
    lv_obj_t* btn_ok = lv_btn_create(web_download_screen);
    lv_obj_set_size(btn_ok, 120, 45);
    lv_obj_align(btn_ok, LV_ALIGN_CENTER, 0, 100);
    applyButtonStyle(btn_ok);
    lv_obj_t* btn_ok_label = lv_label_create(btn_ok);
    lv_label_set_text(btn_ok_label, "OK");
    lv_obj_center(btn_ok_label);

    // Screen-specific KEY handler to prevent global ESC handler conflict
    lv_obj_add_event_cb(btn_ok, [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_KEY) return;
        uint32_t key = lv_event_get_key(e);
        // ONLY intercept ESC - ENTER will trigger button click naturally
        if (key == LV_KEY_ESC) {
            Serial.println("[WebDownload] Notification screen ESC - stopping propagation");
            lv_event_stop_processing(e);
            lv_event_stop_bubbling(e);
            exitWebDownloadScreen();
        }
    }, LV_EVENT_KEY, NULL);

    addNavigableWidget(btn_ok);

    // Footer
    web_download_footer = createWebDownloadFooter(web_download_screen, "Press ENTER or ESC to dismiss");

    // Load screen
    loadScreen(web_download_screen, SCREEN_ANIM_FADE);

    // Focus OK button
    focusWidget(btn_ok);

    // Set state to complete (so any key dismisses)
    webDownloadUIState = WD_UI_COMPLETE;
    webFilesDownloadPromptShown = true;

    // Play notification sound
    beep(TONE_MENU_NAV, BEEP_MEDIUM);
}

// ============================================
// Progress Screen
// ============================================

/**
 * Transition to download progress screen
 */
void showWebFilesDownloadProgress() {
    Serial.println("[WebDownload] Showing download progress screen");

    // Clear previous screen content but keep the screen object
    lv_obj_clean(web_download_screen);
    clearNavigationGroup();

    // Title bar
    createWebDownloadTitleBar(web_download_screen, "Downloading Web Files");

    // Progress card
    lv_obj_t* card = lv_obj_create(web_download_screen);
    lv_obj_set_size(card, 420, 180);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    applyCardStyle(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Status label
    web_download_status_label = lv_label_create(card);
    lv_label_set_text(web_download_status_label, "Downloading web interface files...");
    lv_obj_add_style(web_download_status_label, getStyleLabelSubtitle(), 0);
    lv_obj_align(web_download_status_label, LV_ALIGN_TOP_MID, 0, 10);

    // File label
    web_download_file_label = lv_label_create(card);
    lv_label_set_text(web_download_file_label, "Fetching manifest...");
    lv_obj_add_style(web_download_file_label, getStyleLabelBody(), 0);
    lv_obj_align(web_download_file_label, LV_ALIGN_TOP_MID, 0, 40);

    // Progress bar
    web_download_progress_bar = lv_bar_create(card);
    lv_obj_set_size(web_download_progress_bar, 380, 25);
    lv_obj_align(web_download_progress_bar, LV_ALIGN_CENTER, 0, 15);
    lv_bar_set_range(web_download_progress_bar, 0, 100);
    lv_bar_set_value(web_download_progress_bar, 0, LV_ANIM_OFF);
    applyBarStyle(web_download_progress_bar);

    // Percentage label
    web_download_pct_label = lv_label_create(card);
    lv_label_set_text(web_download_pct_label, "0%");
    lv_obj_set_style_text_color(web_download_pct_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_set_style_text_font(web_download_pct_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_align(web_download_pct_label, LV_ALIGN_CENTER, 0, 55);

    // Footer
    web_download_footer = createWebDownloadFooter(web_download_screen, "Press ESC to cancel");

    webDownloadUIState = WD_UI_DOWNLOADING;

    // Force UI update
    lv_timer_handler();
}

// ============================================
// Complete Screen
// ============================================

/**
 * Show download completion screen
 */
void showWebFilesDownloadComplete(bool success, const char* message) {
    Serial.printf("[WebDownload] Download complete: %s - %s\n", success ? "SUCCESS" : "FAILED", message);

    // Clear screen content
    lv_obj_clean(web_download_screen);
    clearNavigationGroup();

    // Title bar
    createWebDownloadTitleBar(web_download_screen, success ? "Download Complete" : "Download Failed");

    // Result card
    lv_obj_t* card = lv_obj_create(web_download_screen);
    lv_obj_set_size(card, 420, 160);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    applyCardStyle(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Set border color based on result
    lv_obj_set_style_border_color(card, success ? LV_COLOR_SUCCESS : LV_COLOR_ERROR, 0);
    lv_obj_set_style_border_width(card, 2, 0);

    // Result icon
    lv_obj_t* icon = lv_label_create(card);
    lv_label_set_text(icon, success ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(icon, success ? LV_COLOR_SUCCESS : LV_COLOR_ERROR, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 15);

    // Result title
    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, success ? "Download Complete!" : "Download Failed");
    lv_obj_set_style_text_color(title, success ? LV_COLOR_SUCCESS : LV_COLOR_ERROR, 0);
    lv_obj_set_style_text_font(title, getThemeFonts()->font_subtitle, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    // Message/URL
    lv_obj_t* msg = lv_label_create(card);
    if (success) {
        lv_label_set_text(msg, "Web interface is now available at:\nhttp://vail-summit.local\n\nRemember to clear your browser cache!");
    } else {
        char msgBuf[128];
        snprintf(msgBuf, sizeof(msgBuf), "Error: %s", message);
        lv_label_set_text(msg, msgBuf);
    }
    lv_obj_add_style(msg, getStyleLabelBody(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(msg, 4, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, success ? 30 : 15);

    // For failed downloads, add a Retry button
    if (!success) {
        lv_obj_t* retry_btn = lv_btn_create(card);
        lv_obj_set_size(retry_btn, 100, 35);
        lv_obj_align(retry_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_style_bg_color(retry_btn, LV_COLOR_SUCCESS, 0);
        lv_obj_set_style_radius(retry_btn, 6, 0);

        lv_obj_t* retry_label = lv_label_create(retry_btn);
        lv_label_set_text(retry_label, "Retry");
        lv_obj_center(retry_label);

        lv_obj_add_event_cb(retry_btn, [](lv_event_t* e) {
            showWebFilesDownloadScreen();
        }, LV_EVENT_CLICKED, NULL);

        // Screen-specific KEY handler to prevent global ESC handler conflict
        lv_obj_add_event_cb(retry_btn, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_KEY) return;
            uint32_t key = lv_event_get_key(e);
            // ONLY intercept ESC - ENTER will trigger button click naturally
            if (key == LV_KEY_ESC) {
                Serial.println("[WebDownload] Error screen ESC - stopping propagation");
                lv_event_stop_processing(e);
                lv_event_stop_bubbling(e);
                exitWebDownloadScreen();
            }
        }, LV_EVENT_KEY, NULL);

        addNavigableWidget(retry_btn);
    }

    // Footer
    web_download_footer = createWebDownloadFooter(web_download_screen,
        success ? "Press any key to continue..." : "Press ESC to exit or click Retry");

    webDownloadUIState = success ? WD_UI_COMPLETE : WD_UI_ERROR;

    // Play result sound
    beep(success ? TONE_SELECT : TONE_ERROR, BEEP_LONG);
}

// ============================================
// Progress Update
// ============================================

/**
 * Update the download progress UI
 * Call from main loop during download
 */
void updateWebDownloadProgressUI() {
    if (webDownloadUIState != WD_UI_DOWNLOADING) return;
    if (web_download_progress_bar == NULL) return;

    // Calculate progress percentage
    int progress = 0;
    if (webDownloadProgress.totalFiles > 0) {
        progress = (webDownloadProgress.currentFile * 100) / webDownloadProgress.totalFiles;
    }

    // Update progress bar
    lv_bar_set_value(web_download_progress_bar, progress, LV_ANIM_OFF);

    // Update percentage label
    if (web_download_pct_label != NULL) {
        lv_label_set_text_fmt(web_download_pct_label, "%d%%", progress);
    }

    // Update file label
    if (web_download_file_label != NULL) {
        char fileText[64];
        if (webDownloadProgress.totalFiles > 0) {
            snprintf(fileText, sizeof(fileText), "File %d/%d: %s",
                     webDownloadProgress.currentFile,
                     webDownloadProgress.totalFiles,
                     webDownloadProgress.currentFileName.c_str());
        } else {
            snprintf(fileText, sizeof(fileText), "%s", webDownloadProgress.currentFileName.c_str());
        }
        lv_label_set_text(web_download_file_label, fileText);
    }
}

// ============================================
// Input Handling
// ============================================

/**
 * Handle keyboard input for the download screens
 * @return true if input was handled
 */
bool handleWebDownloadInput(char key) {
    if (!isWebDownloadScreenActive()) return false;

    switch (webDownloadUIState) {
        case WD_UI_PROMPTING:
            // Handle Y/N for download prompt
            if (key == 'y' || key == 'Y' || key == KEY_ENTER) {
                beep(TONE_SELECT, BEEP_MEDIUM);

                // Due to memory constraints, downloads must happen at boot
                // before LVGL is initialized. Set flag and reboot.
                Serial.println("[WebDownload] User requested download - rebooting for early boot download");

                // Show brief reboot message
                lv_obj_clean(web_download_screen);
                lv_obj_t* msg = lv_label_create(web_download_screen);
                lv_label_set_text(msg, "Rebooting to download...\n\nPlease wait.");
                lv_obj_add_style(msg, getStyleLabelSubtitle(), 0);
                lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_center(msg);
                lv_timer_handler();  // Force display update

                delay(500);

                // Request download and reboot
                requestWebDownloadAndReboot();
                // Note: Device will reboot here, code below won't execute
                return true;
            }
            else if (key == 'n' || key == 'N' || key == KEY_ESC) {
                beep(TONE_MENU_NAV, BEEP_SHORT);
                exitWebDownloadScreen();
                return true;
            }
            break;

        case WD_UI_DOWNLOADING:
            // Handle ESC to cancel download
            if (key == KEY_ESC) {
                cancelWebFileDownload();
                beep(TONE_ERROR, BEEP_MEDIUM);
                showWebFilesDownloadComplete(false, "Download cancelled");
                return true;
            }
            break;

        case WD_UI_COMPLETE:
        case WD_UI_ERROR:
            // Any key continues
            if (key != 0) {
                beep(TONE_MENU_NAV, BEEP_SHORT);
                exitWebDownloadScreen();
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

// ============================================
// Utility Functions
// ============================================

/**
 * Check if the web download screen is currently active
 */
bool isWebDownloadScreenActive() {
    return webDownloadUIState != WD_UI_IDLE && web_download_screen != NULL;
}

/**
 * Exit the web download screen and return to previous screen
 */
void exitWebDownloadScreen() {
    // Prevent double-exit
    if (webDownloadUIState == WD_UI_IDLE) {
        Serial.println("[WebDownload] Already exited, ignoring duplicate call");
        return;
    }

    Serial.println("[WebDownload] Exiting web download screen");
    Serial.printf("[WebDownload] Current state: %d, screen ptr: %p, previous: %p\n",
                  webDownloadUIState, web_download_screen, previous_screen);

    webDownloadUIState = WD_UI_IDLE;

    // Clean up widget references
    web_download_progress_bar = NULL;
    web_download_file_label = NULL;
    web_download_pct_label = NULL;
    web_download_status_label = NULL;
    web_download_footer = NULL;

    // Delete the screen
    if (web_download_screen != NULL) {
        Serial.println("[WebDownload] Deleting screen object");
        lv_obj_del(web_download_screen);
        web_download_screen = NULL;
    }

    // Return to previous screen if available
    if (previous_screen != NULL && lv_obj_is_valid(previous_screen)) {
        Serial.printf("[WebDownload] Loading previous screen: %p\n", previous_screen);
        lv_scr_load(previous_screen);
        previous_screen = NULL;
    } else {
        Serial.println("[WebDownload] WARNING: No valid previous screen to return to!");
    }

    Serial.println("[WebDownload] Exit complete");
}

#endif // LV_WEB_DOWNLOAD_SCREEN_H
