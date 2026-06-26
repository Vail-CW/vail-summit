/*
 * VAIL SUMMIT - LVGL Theme
 * Dynamic theme system supporting multiple color schemes
 * Uses Theme Manager for color/font switching
 */

#ifndef LV_THEME_SUMMIT_H
#define LV_THEME_SUMMIT_H

#include <lvgl.h>
#include "../core/config.h"
#include "lv_theme_manager.h"

// ============================================
// Legacy Color Macros (for backward compatibility)
// These now dynamically reference the active theme colors
// ============================================

#define LV_COLOR_BG_DEEP          (getThemeColors()->bg_deep)
#define LV_COLOR_BG_LAYER2        (getThemeColors()->bg_layer2)
#define LV_COLOR_BG_CARD          (getThemeColors()->card_primary)
#define LV_COLOR_BG_CARD_ALT      (getThemeColors()->card_secondary)
#define LV_COLOR_BG_CARD_ACTIVE   (getThemeColors()->card_focused)
#define LV_COLOR_BORDER_CARD      (getThemeColors()->card_border)
#define LV_COLOR_ACCENT_PRIMARY   (getThemeColors()->accent_primary)
#define LV_COLOR_ACCENT_SECONDARY (getThemeColors()->accent_secondary)
#define LV_COLOR_BORDER_ACCENT    (getThemeColors()->accent_glow)
#define LV_COLOR_TEXT_PRIMARY     (getThemeColors()->text_primary)
#define LV_COLOR_TEXT_SECONDARY   (getThemeColors()->text_secondary)
#define LV_COLOR_TEXT_TERTIARY    (getThemeColors()->text_tertiary)
#define LV_COLOR_TEXT_DISABLED    (getThemeColors()->text_disabled)
#define LV_COLOR_SUCCESS          (getThemeColors()->success)
#define LV_COLOR_WARNING          (getThemeColors()->warning)
#define LV_COLOR_ERROR            (getThemeColors()->error)
#define LV_COLOR_BORDER_SUBTLE    (getThemeColors()->border_subtle)
#define LV_COLOR_BORDER_LIGHT     (getThemeColors()->border_light)
#define LV_COLOR_ACCENT_MAGENTA   lv_color_make(186, 85, 211)

// ============================================
// Theme Styles
// ============================================

// Global styles (reusable)
static lv_style_t style_screen;
static lv_style_t style_card;
static lv_style_t style_card_focused;
static lv_style_t style_btn;
static lv_style_t style_btn_focused;
static lv_style_t style_btn_pressed;
static lv_style_t style_slider;
static lv_style_t style_slider_indicator;
static lv_style_t style_slider_knob;
static lv_style_t style_label_title;
static lv_style_t style_label_subtitle;
static lv_style_t style_label_body;
static lv_style_t style_list;
static lv_style_t style_list_btn;
static lv_style_t style_list_btn_focused;
static lv_style_t style_textarea;
static lv_style_t style_dropdown;
static lv_style_t style_switch;
static lv_style_t style_switch_checked;
static lv_style_t style_bar;
static lv_style_t style_bar_indicator;
static lv_style_t style_msgbox;
static lv_style_t style_status_bar;
static lv_style_t style_menu_card;
static lv_style_t style_menu_card_focused;
static lv_style_t style_menu_card_pressed;
static lv_style_t style_icon_circle;
static lv_style_t style_btn_disabled;
static lv_style_t style_textarea_focused;
static lv_style_t style_dropdown_focused;
static lv_style_t style_switch_focused;
static lv_style_t style_slider_knob_focused;
static lv_style_t style_scrollbar;

// Track if styles have been initialized at least once
static bool styles_first_init = false;

// ============================================
// Style Initialization Functions
// ============================================

/*
 * Initialize screen background style
 */
void initStyleScreen() {
    const ThemeColors* c = getThemeColors();
    const ThemeFonts* f = getThemeFonts();

    if (styles_first_init) lv_style_reset(&style_screen);
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, c->bg_deep);
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_style_set_text_color(&style_screen, c->text_primary);
    lv_style_set_text_font(&style_screen, f->font_input);
}

/*
 * Initialize card styles
 */
void initStyleCard() {
    const ThemeColors* c = getThemeColors();

    // Normal card
    if (styles_first_init) lv_style_reset(&style_card);
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, c->card_primary);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_border_color(&style_card, c->border_subtle);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_opa(&style_card, LV_OPA_50);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_pad_all(&style_card, 15);
    lv_style_set_shadow_width(&style_card, 0);

    // Focused card
    if (styles_first_init) lv_style_reset(&style_card_focused);
    lv_style_init(&style_card_focused);
    lv_style_set_bg_color(&style_card_focused, c->card_focused);
    lv_style_set_border_color(&style_card_focused, c->accent_primary);
    lv_style_set_border_width(&style_card_focused, 2);
    lv_style_set_border_opa(&style_card_focused, LV_OPA_COVER);
    lv_style_set_shadow_color(&style_card_focused, c->accent_primary);
    lv_style_set_shadow_width(&style_card_focused, 20);
    lv_style_set_shadow_opa(&style_card_focused, LV_OPA_30);
}

/*
 * Initialize button styles - Modern with glow effects
 */
void initStyleButton() {
    const ThemeColors* c = getThemeColors();
    const ThemeFonts* f = getThemeFonts();

    // Normal button
    if (styles_first_init) lv_style_reset(&style_btn);
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, c->card_primary);
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_border_color(&style_btn, c->border_subtle);
    lv_style_set_border_width(&style_btn, 2);
    lv_style_set_radius(&style_btn, 10);
    lv_style_set_pad_all(&style_btn, 20);
    lv_style_set_text_color(&style_btn, c->text_primary);
    lv_style_set_text_font(&style_btn, f->font_body);
    lv_style_set_shadow_width(&style_btn, 0);

    // Focused button - Vibrant with glow
    if (styles_first_init) lv_style_reset(&style_btn_focused);
    lv_style_init(&style_btn_focused);
    lv_style_set_bg_color(&style_btn_focused, c->accent_primary);
    lv_style_set_border_color(&style_btn_focused, c->accent_glow);
    lv_style_set_border_width(&style_btn_focused, 2);
    lv_style_set_text_color(&style_btn_focused, c->text_on_accent);
    lv_style_set_shadow_width(&style_btn_focused, 20);
    lv_style_set_shadow_color(&style_btn_focused, c->accent_primary);
    lv_style_set_shadow_opa(&style_btn_focused, LV_OPA_50);

    // Pressed button
    if (styles_first_init) lv_style_reset(&style_btn_pressed);
    lv_style_init(&style_btn_pressed);
    lv_style_set_bg_color(&style_btn_pressed, c->accent_glow);
    lv_style_set_text_color(&style_btn_pressed, c->bg_deep);

    // Disabled button - dimmed, no border emphasis
    if (styles_first_init) lv_style_reset(&style_btn_disabled);
    lv_style_init(&style_btn_disabled);
    lv_style_set_bg_opa(&style_btn_disabled, LV_OPA_30);
    lv_style_set_text_color(&style_btn_disabled, c->text_disabled);
    lv_style_set_border_color(&style_btn_disabled, c->border_subtle);
    lv_style_set_shadow_width(&style_btn_disabled, 0);
}

/*
 * Initialize slider styles
 */
void initStyleSlider() {
    const ThemeColors* c = getThemeColors();

    // Slider track
    if (styles_first_init) lv_style_reset(&style_slider);
    lv_style_init(&style_slider);
    lv_style_set_bg_color(&style_slider, c->bg_layer2);
    lv_style_set_bg_opa(&style_slider, LV_OPA_COVER);
    lv_style_set_radius(&style_slider, 4);
    lv_style_set_pad_ver(&style_slider, -2);

    // Slider indicator (filled portion)
    if (styles_first_init) lv_style_reset(&style_slider_indicator);
    lv_style_init(&style_slider_indicator);
    lv_style_set_bg_color(&style_slider_indicator, c->accent_primary);
    lv_style_set_bg_opa(&style_slider_indicator, LV_OPA_COVER);
    lv_style_set_radius(&style_slider_indicator, 4);

    // Slider knob
    if (styles_first_init) lv_style_reset(&style_slider_knob);
    lv_style_init(&style_slider_knob);
    lv_style_set_bg_color(&style_slider_knob, c->text_primary);
    lv_style_set_bg_opa(&style_slider_knob, LV_OPA_COVER);
    lv_style_set_radius(&style_slider_knob, LV_RADIUS_CIRCLE);
    lv_style_set_pad_all(&style_slider_knob, 6);
    lv_style_set_shadow_color(&style_slider_knob, c->accent_primary);
    lv_style_set_shadow_width(&style_slider_knob, 10);
    lv_style_set_shadow_opa(&style_slider_knob, LV_OPA_40);

    // Focused slider knob - accent fill + stronger glow so keyboard focus is obvious
    if (styles_first_init) lv_style_reset(&style_slider_knob_focused);
    lv_style_init(&style_slider_knob_focused);
    lv_style_set_bg_color(&style_slider_knob_focused, c->accent_primary);
    lv_style_set_shadow_width(&style_slider_knob_focused, 18);
    lv_style_set_shadow_opa(&style_slider_knob_focused, LV_OPA_70);
}

/*
 * Initialize label styles
 */
void initStyleLabels() {
    const ThemeColors* c = getThemeColors();
    const ThemeFonts* f = getThemeFonts();

    // Title label (large)
    if (styles_first_init) lv_style_reset(&style_label_title);
    lv_style_init(&style_label_title);
    lv_style_set_text_color(&style_label_title, c->accent_primary);
    lv_style_set_text_font(&style_label_title, f->font_title);

    // Subtitle label (medium)
    if (styles_first_init) lv_style_reset(&style_label_subtitle);
    lv_style_init(&style_label_subtitle);
    lv_style_set_text_color(&style_label_subtitle, c->text_primary);
    lv_style_set_text_font(&style_label_subtitle, f->font_subtitle);

    // Body label (normal)
    if (styles_first_init) lv_style_reset(&style_label_body);
    lv_style_init(&style_label_body);
    lv_style_set_text_color(&style_label_body, c->text_secondary);
    lv_style_set_text_font(&style_label_body, f->font_body);
}

/*
 * Initialize list styles
 */
void initStyleList() {
    const ThemeColors* c = getThemeColors();

    // List container
    if (styles_first_init) lv_style_reset(&style_list);
    lv_style_init(&style_list);
    lv_style_set_bg_color(&style_list, c->bg_layer2);
    lv_style_set_bg_opa(&style_list, LV_OPA_COVER);
    lv_style_set_border_width(&style_list, 0);
    lv_style_set_radius(&style_list, 8);
    lv_style_set_pad_all(&style_list, 5);

    // List button (normal)
    if (styles_first_init) lv_style_reset(&style_list_btn);
    lv_style_init(&style_list_btn);
    lv_style_set_bg_color(&style_list_btn, c->card_secondary);
    lv_style_set_bg_opa(&style_list_btn, LV_OPA_COVER);
    lv_style_set_radius(&style_list_btn, 6);
    lv_style_set_pad_all(&style_list_btn, 10);
    lv_style_set_text_color(&style_list_btn, c->text_primary);

    // List button (focused) - the focus fill (card_focused) is the same teal as
    // the accent, so the text MUST switch to the dark on-accent color or it's
    // unreadable (light text on teal).
    if (styles_first_init) lv_style_reset(&style_list_btn_focused);
    lv_style_init(&style_list_btn_focused);
    lv_style_set_bg_color(&style_list_btn_focused, c->card_focused);
    lv_style_set_text_color(&style_list_btn_focused, c->text_on_accent);
    lv_style_set_border_color(&style_list_btn_focused, c->accent_primary);
    lv_style_set_border_width(&style_list_btn_focused, 2);
}

/*
 * Initialize textarea style
 */
void initStyleTextarea() {
    const ThemeColors* c = getThemeColors();
    const ThemeFonts* f = getThemeFonts();

    if (styles_first_init) lv_style_reset(&style_textarea);
    lv_style_init(&style_textarea);
    lv_style_set_bg_color(&style_textarea, c->bg_layer2);
    lv_style_set_bg_opa(&style_textarea, LV_OPA_COVER);
    lv_style_set_border_color(&style_textarea, c->border_subtle);
    lv_style_set_border_width(&style_textarea, 1);
    lv_style_set_radius(&style_textarea, 6);
    lv_style_set_pad_all(&style_textarea, 10);
    lv_style_set_text_color(&style_textarea, c->text_primary);
    lv_style_set_text_font(&style_textarea, f->font_input);

    // Focused textarea - accent border marks keyboard focus
    if (styles_first_init) lv_style_reset(&style_textarea_focused);
    lv_style_init(&style_textarea_focused);
    lv_style_set_border_color(&style_textarea_focused, c->accent_primary);
    lv_style_set_border_width(&style_textarea_focused, 2);
    lv_style_set_border_opa(&style_textarea_focused, LV_OPA_COVER);
}

/*
 * Initialize dropdown style
 * Note: Uses Montserrat font to ensure LVGL symbols (arrows) display correctly
 *       regardless of theme (Special Elite font lacks symbol glyphs)
 */
void initStyleDropdown() {
    const ThemeColors* c = getThemeColors();

    if (styles_first_init) lv_style_reset(&style_dropdown);
    lv_style_init(&style_dropdown);
    lv_style_set_bg_color(&style_dropdown, c->card_primary);
    lv_style_set_bg_opa(&style_dropdown, LV_OPA_COVER);
    lv_style_set_border_color(&style_dropdown, c->border_subtle);
    lv_style_set_border_width(&style_dropdown, 1);
    lv_style_set_radius(&style_dropdown, 6);
    lv_style_set_pad_all(&style_dropdown, 10);
    lv_style_set_text_color(&style_dropdown, c->text_primary);
    // Use theme font - Special Elite fonts now include LVGL symbols
    lv_style_set_text_font(&style_dropdown, getThemeFonts()->font_body);

    // Focused dropdown - accent border marks keyboard focus
    if (styles_first_init) lv_style_reset(&style_dropdown_focused);
    lv_style_init(&style_dropdown_focused);
    lv_style_set_border_color(&style_dropdown_focused, c->accent_primary);
    lv_style_set_border_width(&style_dropdown_focused, 2);
    lv_style_set_border_opa(&style_dropdown_focused, LV_OPA_COVER);
}

/*
 * Initialize switch styles
 */
void initStyleSwitch() {
    const ThemeColors* c = getThemeColors();

    // Normal switch (off)
    if (styles_first_init) lv_style_reset(&style_switch);
    lv_style_init(&style_switch);
    lv_style_set_bg_color(&style_switch, c->bg_layer2);
    lv_style_set_bg_opa(&style_switch, LV_OPA_COVER);
    lv_style_set_radius(&style_switch, LV_RADIUS_CIRCLE);

    // Checked switch (on)
    if (styles_first_init) lv_style_reset(&style_switch_checked);
    lv_style_init(&style_switch_checked);
    lv_style_set_bg_color(&style_switch_checked, c->accent_primary);

    // Focused switch - accent outline marks keyboard focus
    if (styles_first_init) lv_style_reset(&style_switch_focused);
    lv_style_init(&style_switch_focused);
    lv_style_set_outline_color(&style_switch_focused, c->accent_primary);
    lv_style_set_outline_width(&style_switch_focused, 2);
    lv_style_set_outline_pad(&style_switch_focused, 2);
}

/*
 * Initialize progress bar styles
 */
void initStyleBar() {
    const ThemeColors* c = getThemeColors();

    // Bar background
    if (styles_first_init) lv_style_reset(&style_bar);
    lv_style_init(&style_bar);
    lv_style_set_bg_color(&style_bar, c->bg_layer2);
    lv_style_set_bg_opa(&style_bar, LV_OPA_COVER);
    lv_style_set_radius(&style_bar, 4);

    // Bar indicator
    if (styles_first_init) lv_style_reset(&style_bar_indicator);
    lv_style_init(&style_bar_indicator);
    lv_style_set_bg_color(&style_bar_indicator, c->accent_primary);
    lv_style_set_bg_opa(&style_bar_indicator, LV_OPA_COVER);
    lv_style_set_radius(&style_bar_indicator, 4);
}

/*
 * Initialize message box style
 */
void initStyleMsgbox() {
    const ThemeColors* c = getThemeColors();

    if (styles_first_init) lv_style_reset(&style_msgbox);
    lv_style_init(&style_msgbox);
    lv_style_set_bg_color(&style_msgbox, c->card_primary);
    lv_style_set_bg_opa(&style_msgbox, LV_OPA_COVER);
    lv_style_set_border_color(&style_msgbox, c->accent_glow);
    lv_style_set_border_width(&style_msgbox, 2);
    lv_style_set_radius(&style_msgbox, 12);
    lv_style_set_pad_all(&style_msgbox, 20);
    lv_style_set_shadow_color(&style_msgbox, lv_color_black());
    lv_style_set_shadow_width(&style_msgbox, 30);
    lv_style_set_shadow_opa(&style_msgbox, LV_OPA_50);
}

/*
 * Initialize status bar style
 */
void initStyleStatusBar() {
    const ThemeColors* c = getThemeColors();

    if (styles_first_init) lv_style_reset(&style_status_bar);
    lv_style_init(&style_status_bar);
    lv_style_set_bg_color(&style_status_bar, c->bg_layer2);
    lv_style_set_bg_opa(&style_status_bar, LV_OPA_COVER);
    lv_style_set_pad_hor(&style_status_bar, 10);
    lv_style_set_pad_ver(&style_status_bar, 5);
}

/*
 * Initialize menu card styles (for main menu navigation)
 * Modern design with vibrant focus states
 */
void initStyleMenuCard() {
    const ThemeColors* c = getThemeColors();

    // Normal menu card - dark with subtle border
    if (styles_first_init) lv_style_reset(&style_menu_card);
    lv_style_init(&style_menu_card);
    lv_style_set_bg_color(&style_menu_card, c->card_primary);
    lv_style_set_bg_opa(&style_menu_card, LV_OPA_COVER);
    lv_style_set_border_color(&style_menu_card, c->border_subtle);
    lv_style_set_border_width(&style_menu_card, 2);
    lv_style_set_radius(&style_menu_card, 10);
    lv_style_set_pad_all(&style_menu_card, 5);  // Minimal padding to maximize text space
    lv_style_set_shadow_width(&style_menu_card, 0);

    // Focused menu card - bold accent OUTLINE, no solid fill, so the element's
    // text stays readable. Base card background shows through; a soft accent
    // glow plus the thick border mark focus. (Per UX feedback.)
    if (styles_first_init) lv_style_reset(&style_menu_card_focused);
    lv_style_init(&style_menu_card_focused);
    lv_style_set_border_color(&style_menu_card_focused, c->accent_primary);
    lv_style_set_border_width(&style_menu_card_focused, 3);
    lv_style_set_border_opa(&style_menu_card_focused, LV_OPA_COVER);
    lv_style_set_shadow_width(&style_menu_card_focused, 12);
    lv_style_set_shadow_color(&style_menu_card_focused, c->accent_primary);
    lv_style_set_shadow_opa(&style_menu_card_focused, LV_OPA_40);

    // Pressed menu card - solid fill flash acknowledges ENTER before the
    // screen transition starts (text colors stay readable on card_focused).
    if (styles_first_init) lv_style_reset(&style_menu_card_pressed);
    lv_style_init(&style_menu_card_pressed);
    lv_style_set_bg_color(&style_menu_card_pressed, c->card_focused);
    lv_style_set_border_color(&style_menu_card_pressed, c->accent_glow);
    lv_style_set_border_width(&style_menu_card_pressed, 3);
    lv_style_set_shadow_width(&style_menu_card_pressed, 20);
    lv_style_set_shadow_color(&style_menu_card_pressed, c->accent_primary);
    lv_style_set_shadow_opa(&style_menu_card_pressed, LV_OPA_60);
}

/*
 * Initialize icon circle style (for menu card icons)
 */
void initStyleIconCircle() {
    const ThemeColors* c = getThemeColors();

    if (styles_first_init) lv_style_reset(&style_icon_circle);
    lv_style_init(&style_icon_circle);
    lv_style_set_bg_color(&style_icon_circle, c->accent_primary);
    lv_style_set_bg_opa(&style_icon_circle, LV_OPA_30);
    lv_style_set_radius(&style_icon_circle, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&style_icon_circle, 0);
}

/*
 * Initialize scrollbar style (themed, visible on dark backgrounds)
 */
void initStyleScrollbar() {
    const ThemeColors* c = getThemeColors();

    if (styles_first_init) lv_style_reset(&style_scrollbar);
    lv_style_init(&style_scrollbar);
    lv_style_set_bg_color(&style_scrollbar, c->border_light);
    lv_style_set_bg_opa(&style_scrollbar, LV_OPA_60);
    lv_style_set_radius(&style_scrollbar, LV_RADIUS_CIRCLE);
    lv_style_set_width(&style_scrollbar, 4);
}

// ============================================
// Theme Initialization
// ============================================

/*
 * Initialize all VAIL SUMMIT theme styles
 * Can be called multiple times for theme switching
 */
void initSummitTheme() {
    Serial.printf("[Theme] Initializing theme styles (theme: %s)...\n",
                  getThemeName(getCurrentTheme()));

    // Initialize all styles
    initStyleScreen();
    initStyleCard();
    initStyleButton();
    initStyleSlider();
    initStyleLabels();
    initStyleList();
    initStyleTextarea();
    initStyleDropdown();
    initStyleSwitch();
    initStyleBar();
    initStyleMsgbox();
    initStyleStatusBar();
    initStyleMenuCard();
    initStyleIconCircle();
    initStyleScrollbar();

    // Mark that styles have been initialized at least once
    // (future calls will use lv_style_reset before lv_style_init)
    styles_first_init = true;

    Serial.println("[Theme] Theme initialization complete");
}

// ============================================
// Style Getter Functions
// ============================================

lv_style_t* getStyleScreen() { return &style_screen; }
lv_style_t* getStyleCard() { return &style_card; }
lv_style_t* getStyleCardFocused() { return &style_card_focused; }
lv_style_t* getStyleBtn() { return &style_btn; }
lv_style_t* getStyleBtnFocused() { return &style_btn_focused; }
lv_style_t* getStyleBtnPressed() { return &style_btn_pressed; }
lv_style_t* getStyleSlider() { return &style_slider; }
lv_style_t* getStyleSliderIndicator() { return &style_slider_indicator; }
lv_style_t* getStyleSliderKnob() { return &style_slider_knob; }
lv_style_t* getStyleLabelTitle() { return &style_label_title; }
lv_style_t* getStyleLabelSubtitle() { return &style_label_subtitle; }
lv_style_t* getStyleLabelBody() { return &style_label_body; }
lv_style_t* getStyleList() { return &style_list; }
lv_style_t* getStyleListBtn() { return &style_list_btn; }
lv_style_t* getStyleListBtnFocused() { return &style_list_btn_focused; }
lv_style_t* getStyleTextarea() { return &style_textarea; }
lv_style_t* getStyleDropdown() { return &style_dropdown; }
lv_style_t* getStyleSwitch() { return &style_switch; }
lv_style_t* getStyleSwitchChecked() { return &style_switch_checked; }
lv_style_t* getStyleBar() { return &style_bar; }
lv_style_t* getStyleBarIndicator() { return &style_bar_indicator; }
lv_style_t* getStyleMsgbox() { return &style_msgbox; }
lv_style_t* getStyleStatusBar() { return &style_status_bar; }
lv_style_t* getStyleMenuCard() { return &style_menu_card; }
lv_style_t* getStyleMenuCardFocused() { return &style_menu_card_focused; }
lv_style_t* getStyleMenuCardPressed() { return &style_menu_card_pressed; }
lv_style_t* getStyleIconCircle() { return &style_icon_circle; }
lv_style_t* getStyleBtnDisabled() { return &style_btn_disabled; }
lv_style_t* getStyleTextareaFocused() { return &style_textarea_focused; }
lv_style_t* getStyleDropdownFocused() { return &style_dropdown_focused; }
lv_style_t* getStyleSwitchFocused() { return &style_switch_focused; }
lv_style_t* getStyleSliderKnobFocused() { return &style_slider_knob_focused; }
lv_style_t* getStyleScrollbar() { return &style_scrollbar; }

// ============================================
// Helper Functions
// ============================================

/*
 * Apply screen style to an object
 */
void applyScreenStyle(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_screen, 0);
}

/*
 * Apply card style with focus states
 */
void applyCardStyle(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_card, 0);
    lv_obj_add_style(obj, &style_card_focused, LV_STATE_FOCUSED);
}

/*
 * Apply button style with all states
 */
void applyButtonStyle(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_btn, 0);
    lv_obj_add_style(obj, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(obj, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(obj, &style_btn_disabled, LV_STATE_DISABLED);
}

/*
 * Apply slider style (with focused knob state)
 */
void applySliderStyle(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_slider, LV_PART_MAIN);
    lv_obj_add_style(obj, &style_slider_indicator, LV_PART_INDICATOR);
    lv_obj_add_style(obj, &style_slider_knob, LV_PART_KNOB);
    lv_obj_add_style(obj, &style_slider_knob_focused, LV_PART_KNOB | LV_STATE_FOCUSED);
}

/*
 * Apply textarea style (with focused state)
 */
void applyTextareaStyle(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_textarea, 0);
    lv_obj_add_style(obj, &style_textarea_focused, LV_STATE_FOCUSED);
}

/*
 * Apply dropdown style (with focused state)
 */
void applyDropdownStyle(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_dropdown, 0);
    lv_obj_add_style(obj, &style_dropdown_focused, LV_STATE_FOCUSED);
}

/*
 * Apply switch style (with checked + focused states)
 */
void applySwitchStyle(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_switch, 0);
    lv_obj_add_style(obj, &style_switch_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(obj, &style_switch_focused, LV_STATE_FOCUSED);
}

/*
 * Apply menu card style with focus + pressed states
 */
void applyMenuCardStyle(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_menu_card, 0);
    lv_obj_add_style(obj, &style_menu_card_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(obj, &style_menu_card_pressed, LV_STATE_PRESSED);
}

/*
 * Apply list style
 */
void applyListStyle(lv_obj_t* list) {
    lv_obj_add_style(list, &style_list, LV_PART_MAIN);
    lv_obj_add_style(list, &style_scrollbar, LV_PART_SCROLLBAR);
}

/*
 * Apply bar/progress style
 */
void applyBarStyle(lv_obj_t* bar) {
    lv_obj_add_style(bar, &style_bar, LV_PART_MAIN);
    lv_obj_add_style(bar, &style_bar_indicator, LV_PART_INDICATOR);
}

#endif // LV_THEME_SUMMIT_H
