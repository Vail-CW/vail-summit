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
 * Show the web files download prompt screen
 * Called when SD card present, WiFi connected, and web files missing
 */
void showWebFilesDownloadScreen() {
    Serial.println("[WebDownload] Showing download prompt screen");

    // Store reference to return later
    previous_screen = lv_scr_act();

    // Create new screen
    web_download_screen = createScreen();
    applyScreenStyle(web_download_screen);

    // Clear navigation group for this screen
    clearNavigationGroup();

    // Title bar
    createWebDownloadTitleBar(web_download_screen, "Web Interface Setup");

    // Main content card
    lv_obj_t* card = lv_obj_create(web_download_screen);
    lv_obj_set_size(card, 420, 160);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, -10);
    applyCardStyle(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Info icon
    lv_obj_t* icon = lv_label_create(card);
    lv_label_set_text(icon, LV_SYMBOL_DOWNLOAD);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(icon, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 10, 10);

    // Message text
    lv_obj_t* msg = lv_label_create(card);
    lv_label_set_text(msg,
        "SD card detected but web interface\n"
        "files are missing.\n\n"
        "Download web interface files\n"
        "from the internet?");
    lv_obj_add_style(msg, getStyleLabelBody(), 0);
    lv_obj_set_style_text_line_space(msg, 4, 0);
    lv_obj_align(msg, LV_ALIGN_TOP_LEFT, 50, 10);

    // Button container
    lv_obj_t* btn_container = lv_obj_create(web_download_screen);
    lv_obj_set_size(btn_container, 420, 50);
    lv_obj_align(btn_container, LV_ALIGN_CENTER, 0, 90);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_layout(btn_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);

    // Download button
    lv_obj_t* btn_download = lv_btn_create(btn_container);
    lv_obj_set_size(btn_download, 160, 45);
    applyButtonStyle(btn_download);
    lv_obj_t* btn_download_label = lv_label_create(btn_download);
    lv_label_set_text(btn_download_label, "Download (Y)");
    lv_obj_center(btn_download_label);
    addNavigableWidget(btn_download);

    // Skip button
    lv_obj_t* btn_skip = lv_btn_create(btn_container);
    lv_obj_set_size(btn_skip, 160, 45);
    applyButtonStyle(btn_skip);
    lv_obj_t* btn_skip_label = lv_label_create(btn_skip);
    lv_label_set_text(btn_skip_label, "Skip (N)");
    lv_obj_center(btn_skip_label);
    addNavigableWidget(btn_skip);

    // Footer
    web_download_footer = createWebDownloadFooter(web_download_screen, "Y: Download Now   N: Skip (don't ask again)");

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
        lv_label_set_text(msg, "Web interface is now available at:\nhttp://vail-summit.local");
    } else {
        char msgBuf[128];
        snprintf(msgBuf, sizeof(msgBuf), "Error: %s\nYou can try again via Settings menu", message);
        lv_label_set_text(msg, msgBuf);
    }
    lv_obj_add_style(msg, getStyleLabelBody(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(msg, 4, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, 25);

    // Footer
    web_download_footer = createWebDownloadFooter(web_download_screen, "Press any key to continue...");

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
                showWebFilesDownloadProgress();

                // Start the download
                webFilesDownloading = true;
                bool success = downloadWebFilesFromGitHub();
                webFilesDownloading = false;

                // Show result
                showWebFilesDownloadComplete(success,
                    success ? "" : webDownloadProgress.errorMessage.c_str());
                return true;
            }
            else if (key == 'n' || key == 'N' || key == KEY_ESC) {
                beep(TONE_MENU_NAV, BEEP_SHORT);
                declineWebFilesDownload();
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
    Serial.println("[WebDownload] Exiting web download screen");

    webDownloadUIState = WD_UI_IDLE;

    // Clean up widget references
    web_download_progress_bar = NULL;
    web_download_file_label = NULL;
    web_download_pct_label = NULL;
    web_download_status_label = NULL;
    web_download_footer = NULL;

    // Delete the screen
    if (web_download_screen != NULL) {
        lv_obj_del(web_download_screen);
        web_download_screen = NULL;
    }

    // Return to previous screen if available
    if (previous_screen != NULL && lv_obj_is_valid(previous_screen)) {
        lv_scr_load(previous_screen);
        previous_screen = NULL;
    }
}

#endif // LV_WEB_DOWNLOAD_SCREEN_H
