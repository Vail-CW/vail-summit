#ifndef LV_WEB_MODE_SCREENS_H
#define LV_WEB_MODE_SCREENS_H

#include <WiFi.h>

// Forward declarations
extern void onLVGLBackNavigation();
extern bool webPracticeModeActive;
extern bool webHearItModeActive;
extern bool webMemoryChainModeActive;
extern AsyncWebSocket* practiceWebSocket;
extern AsyncWebSocket* hearItWebSocket;
extern AsyncWebSocket* memoryChainWebSocket;
extern void cleanupPracticeWebSocket();
extern void cleanupHearItWebSocket();
extern void cleanupMemoryChainWebSocket();

// ============================================================
// Shared stop button handler for all web mode screens
// ============================================================
static void web_mode_stop_handler(lv_event_t* e) {
    onLVGLBackNavigation();
}

// ============================================================
// Web Practice Mode
// ============================================================
static lv_timer_t* webPracticeStatusTimer = NULL;
static lv_obj_t* webPracticeStatusLabel = NULL;

static void web_practice_timer_cb(lv_timer_t* timer) {
    if (!webPracticeStatusLabel) return;
    if (webPracticeModeActive && practiceWebSocket && practiceWebSocket->count() > 0) {
        lv_label_set_text(webPracticeStatusLabel, "Browser Connected");
        lv_obj_set_style_text_color(webPracticeStatusLabel, lv_color_hex(0x10B981), 0);
    } else {
        lv_label_set_text(webPracticeStatusLabel, "Waiting for browser...");
        lv_obj_set_style_text_color(webPracticeStatusLabel, LV_COLOR_WARNING, 0);
    }
}

lv_obj_t* createWebPracticeModeScreen() {
    clearNavigationGroup();
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "WEB PRACTICE");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // === Centered Card ===
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, 400, 170);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    applyCardStyle(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 15, 0);

    // Icon + title row
    lv_obj_t* icon = lv_label_create(card);
    lv_label_set_text(icon, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_font(icon, getThemeFonts()->font_large, 0);
    lv_obj_set_style_text_color(icon, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, -80, 0);

    lv_obj_t* mode_title = lv_label_create(card);
    lv_label_set_text(mode_title, "Morse Practice Active");
    lv_obj_set_style_text_font(mode_title, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(mode_title, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(mode_title, LV_ALIGN_TOP_MID, 20, 5);

    // Info text
    lv_obj_t* info = lv_label_create(card);
    lv_label_set_text(info, "Keying from web browser\nDecoded text shows in browser");
    lv_obj_set_style_text_font(info, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(info, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 40);

    // IP address
    lv_obj_t* ip_label = lv_label_create(card);
    char ip_text[48];
    snprintf(ip_text, sizeof(ip_text), "IP: %s", WiFi.localIP().toString().c_str());
    lv_label_set_text(ip_label, ip_text);
    lv_obj_set_style_text_font(ip_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(ip_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(ip_label, LV_ALIGN_TOP_MID, 0, 80);

    // Connection status
    webPracticeStatusLabel = lv_label_create(card);
    lv_label_set_text(webPracticeStatusLabel, "Waiting for browser...");
    lv_obj_set_style_text_font(webPracticeStatusLabel, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(webPracticeStatusLabel, LV_COLOR_WARNING, 0);
    lv_obj_align(webPracticeStatusLabel, LV_ALIGN_TOP_MID, 0, 105);

    // STOP button
    lv_obj_t* stop_btn = lv_btn_create(screen);
    lv_obj_set_size(stop_btn, 200, 45);
    lv_obj_align(stop_btn, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xE74C3C), 0);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xC0392B), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(stop_btn, 8, 0);
    lv_obj_set_style_border_color(stop_btn, LV_COLOR_ACCENT_CYAN, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(stop_btn, 2, LV_STATE_FOCUSED);

    lv_obj_t* btn_label = lv_label_create(stop_btn);
    lv_label_set_text(btn_label, "STOP");
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(btn_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(stop_btn, web_mode_stop_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(stop_btn, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(stop_btn);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, "ENTER Stop   ESC Back");
    lv_obj_set_style_text_color(help, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    // Status update timer
    webPracticeStatusTimer = lv_timer_create(web_practice_timer_cb, 1000, NULL);

    return screen;
}

void cleanupWebPracticeMode() {
    if (webPracticeStatusTimer) {
        lv_timer_del(webPracticeStatusTimer);
        webPracticeStatusTimer = NULL;
    }
    cleanupPracticeWebSocket();
    webPracticeModeActive = false;
    webPracticeStatusLabel = NULL;
    extern MorseDecoderAdaptive webPracticeDecoder;
    webPracticeDecoder.reset();
}

// ============================================================
// Web Hear It Mode
// ============================================================
static lv_timer_t* webHearItStatusTimer = NULL;
static lv_obj_t* webHearItStatusLabel = NULL;

static void web_hear_it_timer_cb(lv_timer_t* timer) {
    if (!webHearItStatusLabel) return;
    if (webHearItModeActive && hearItWebSocket && hearItWebSocket->count() > 0) {
        lv_label_set_text(webHearItStatusLabel, "Browser Connected");
        lv_obj_set_style_text_color(webHearItStatusLabel, lv_color_hex(0x10B981), 0);
    } else {
        lv_label_set_text(webHearItStatusLabel, "Waiting for browser...");
        lv_obj_set_style_text_color(webHearItStatusLabel, LV_COLOR_WARNING, 0);
    }
}

lv_obj_t* createWebHearItModeScreen() {
    clearNavigationGroup();
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "WEB HEAR IT");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // === Centered Card ===
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, 400, 170);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    applyCardStyle(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 15, 0);

    // Icon + title row
    lv_obj_t* icon = lv_label_create(card);
    lv_label_set_text(icon, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(icon, getThemeFonts()->font_large, 0);
    lv_obj_set_style_text_color(icon, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, -100, 0);

    lv_obj_t* mode_title = lv_label_create(card);
    lv_label_set_text(mode_title, "Hear It Type It Active");
    lv_obj_set_style_text_font(mode_title, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(mode_title, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(mode_title, LV_ALIGN_TOP_MID, 10, 5);

    // Info text
    lv_obj_t* info = lv_label_create(card);
    lv_label_set_text(info, "Training running in browser");
    lv_obj_set_style_text_font(info, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(info, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 40);

    // IP address
    lv_obj_t* ip_label = lv_label_create(card);
    char ip_text[48];
    snprintf(ip_text, sizeof(ip_text), "IP: %s", WiFi.localIP().toString().c_str());
    lv_label_set_text(ip_label, ip_text);
    lv_obj_set_style_text_font(ip_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(ip_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(ip_label, LV_ALIGN_TOP_MID, 0, 70);

    // Connection status
    webHearItStatusLabel = lv_label_create(card);
    lv_label_set_text(webHearItStatusLabel, "Waiting for browser...");
    lv_obj_set_style_text_font(webHearItStatusLabel, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(webHearItStatusLabel, LV_COLOR_WARNING, 0);
    lv_obj_align(webHearItStatusLabel, LV_ALIGN_TOP_MID, 0, 95);

    // STOP button
    lv_obj_t* stop_btn = lv_btn_create(screen);
    lv_obj_set_size(stop_btn, 200, 45);
    lv_obj_align(stop_btn, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xE74C3C), 0);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xC0392B), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(stop_btn, 8, 0);
    lv_obj_set_style_border_color(stop_btn, LV_COLOR_ACCENT_CYAN, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(stop_btn, 2, LV_STATE_FOCUSED);

    lv_obj_t* btn_label = lv_label_create(stop_btn);
    lv_label_set_text(btn_label, "STOP");
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(btn_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(stop_btn, web_mode_stop_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(stop_btn, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(stop_btn);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, "ENTER Stop   ESC Back");
    lv_obj_set_style_text_color(help, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    // Status update timer
    webHearItStatusTimer = lv_timer_create(web_hear_it_timer_cb, 1000, NULL);

    return screen;
}

void cleanupWebHearItMode() {
    if (webHearItStatusTimer) {
        lv_timer_del(webHearItStatusTimer);
        webHearItStatusTimer = NULL;
    }
    cleanupHearItWebSocket();
    webHearItModeActive = false;
    webHearItStatusLabel = NULL;
}

// ============================================================
// Web Memory Chain Mode
// ============================================================
static lv_timer_t* webMemoryChainStatusTimer = NULL;
static lv_obj_t* webMemoryChainStatusLabel = NULL;

static void web_memory_chain_timer_cb(lv_timer_t* timer) {
    if (!webMemoryChainStatusLabel) return;
    if (webMemoryChainModeActive && memoryChainWebSocket && memoryChainWebSocket->count() > 0) {
        lv_label_set_text(webMemoryChainStatusLabel, "Browser Connected");
        lv_obj_set_style_text_color(webMemoryChainStatusLabel, lv_color_hex(0x10B981), 0);
    } else {
        lv_label_set_text(webMemoryChainStatusLabel, "Waiting for browser...");
        lv_obj_set_style_text_color(webMemoryChainStatusLabel, LV_COLOR_WARNING, 0);
    }
}

lv_obj_t* createWebMemoryChainModeScreen() {
    clearNavigationGroup();
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "WEB MEMORY CHAIN");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // === Centered Card ===
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, 400, 170);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    applyCardStyle(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 15, 0);

    // Icon + title row
    lv_obj_t* icon = lv_label_create(card);
    lv_label_set_text(icon, LV_SYMBOL_SHUFFLE);
    lv_obj_set_style_text_font(icon, getThemeFonts()->font_large, 0);
    lv_obj_set_style_text_color(icon, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, -90, 0);

    lv_obj_t* mode_title = lv_label_create(card);
    lv_label_set_text(mode_title, "Memory Chain Active");
    lv_obj_set_style_text_font(mode_title, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(mode_title, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(mode_title, LV_ALIGN_TOP_MID, 15, 5);

    // Info text
    lv_obj_t* info = lv_label_create(card);
    lv_label_set_text(info, "Game running in browser");
    lv_obj_set_style_text_font(info, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(info, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 40);

    // IP address
    lv_obj_t* ip_label = lv_label_create(card);
    char ip_text[48];
    snprintf(ip_text, sizeof(ip_text), "IP: %s", WiFi.localIP().toString().c_str());
    lv_label_set_text(ip_label, ip_text);
    lv_obj_set_style_text_font(ip_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(ip_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(ip_label, LV_ALIGN_TOP_MID, 0, 70);

    // Connection status
    webMemoryChainStatusLabel = lv_label_create(card);
    lv_label_set_text(webMemoryChainStatusLabel, "Waiting for browser...");
    lv_obj_set_style_text_font(webMemoryChainStatusLabel, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(webMemoryChainStatusLabel, LV_COLOR_WARNING, 0);
    lv_obj_align(webMemoryChainStatusLabel, LV_ALIGN_TOP_MID, 0, 95);

    // STOP button
    lv_obj_t* stop_btn = lv_btn_create(screen);
    lv_obj_set_size(stop_btn, 200, 45);
    lv_obj_align(stop_btn, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xE74C3C), 0);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xC0392B), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(stop_btn, 8, 0);
    lv_obj_set_style_border_color(stop_btn, LV_COLOR_ACCENT_CYAN, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(stop_btn, 2, LV_STATE_FOCUSED);

    lv_obj_t* btn_label = lv_label_create(stop_btn);
    lv_label_set_text(btn_label, "STOP");
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(btn_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(stop_btn, web_mode_stop_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(stop_btn, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(stop_btn);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, "ENTER Stop   ESC Back");
    lv_obj_set_style_text_color(help, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    // Status update timer
    webMemoryChainStatusTimer = lv_timer_create(web_memory_chain_timer_cb, 1000, NULL);

    return screen;
}

void cleanupWebMemoryChainMode() {
    if (webMemoryChainStatusTimer) {
        lv_timer_del(webMemoryChainStatusTimer);
        webMemoryChainStatusTimer = NULL;
    }
    cleanupMemoryChainWebSocket();
    webMemoryChainModeActive = false;
    webMemoryChainStatusLabel = NULL;
    extern MorseDecoderAdaptive webMemoryChainDecoder;
    webMemoryChainDecoder.reset();
}

#endif // LV_WEB_MODE_SCREENS_H
