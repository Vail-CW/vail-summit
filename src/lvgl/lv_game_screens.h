/*
 * VAIL SUMMIT - LVGL Game Screens
 * Provides LVGL UI for games (Morse Shooter, Memory Chain)
 */

#ifndef LV_GAME_SCREENS_H
#define LV_GAME_SCREENS_H

#include <lvgl.h>
#include "lv_theme_summit.h"
#include "lv_widgets_summit.h"
#include "lv_screen_manager.h"
#include "../core/config.h"
#include "../core/modes.h"
#include "../settings/settings_cw.h"  // For KeyType enum and cwKeyType/cwSpeed/cwTone
#include "lv_spark_watch_screens.h"   // Spark Watch game screens

// Forward declaration for back navigation
extern void onLVGLBackNavigation();

// Forward declaration for game over overlay (defined later in this file)
lv_obj_t* createGameOverOverlay(lv_obj_t* parent, int final_score, bool is_high_score);

// ============================================
// Morse Shooter Game Screen
// ============================================

// Key event callback for Morse Shooter keyboard input
static void shooter_key_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);
    Serial.printf("[Shooter LVGL] Key event: %lu (0x%02lX)\n", key, key);

    if (key == LV_KEY_ESC) {
        onLVGLBackNavigation();
        lv_event_stop_processing(e);  // Prevent global ESC handler from also firing
    }
}

static lv_obj_t* shooter_screen = NULL;
static lv_obj_t* shooter_canvas = NULL;
static lv_obj_t* shooter_score_label = NULL;
static lv_obj_t* shooter_lives_container = NULL;
static lv_obj_t* shooter_decoded_label = NULL;
static lv_obj_t* shooter_combo_label = NULL;     // Combo multiplier display
static lv_obj_t* shooter_letter_labels[8] = {NULL};  // Pool of falling letter objects (increased for maxLetters up to 8)

// Canvas buffer for game graphics
static lv_color_t* shooter_canvas_buf = NULL;

// Forward declarations for cleanup and effects
static void cleanupShooterScreenPointers();

// Static variables for effects, game over, and settings screens
// All initialized to NULL to prevent crashes on first use
static lv_obj_t* shooter_laser_line = NULL;
static lv_obj_t* shooter_explosion_container = NULL;
static lv_obj_t* shooter_explosion_circles[4] = {NULL, NULL, NULL, NULL};
static lv_obj_t* shooter_game_over_overlay = NULL;
static lv_obj_t* shooter_settings_screen = NULL;
static lv_obj_t* shooter_mode_row = NULL;
static lv_obj_t* shooter_mode_value = NULL;
static lv_obj_t* shooter_preset_row = NULL;
static lv_obj_t* shooter_preset_value = NULL;
static lv_obj_t* shooter_speed_row = NULL;
static lv_obj_t* shooter_speed_value = NULL;
static lv_obj_t* shooter_tone_row = NULL;
static lv_obj_t* shooter_tone_value = NULL;
static lv_obj_t* shooter_key_row = NULL;
static lv_obj_t* shooter_key_value = NULL;
static lv_obj_t* shooter_lives_row = NULL;
static lv_obj_t* shooter_lives_value = NULL;
static lv_obj_t* shooter_highscore_value = NULL;
static lv_obj_t* shooter_start_btn = NULL;

// Reset all shooter screen static pointers (called before creating new screen)
// This prevents crashes from stale pointers after screen deletion
static void cleanupShooterScreenPointers() {
    // Game screen pointers
    shooter_screen = NULL;
    shooter_canvas = NULL;
    shooter_score_label = NULL;
    shooter_lives_container = NULL;
    shooter_decoded_label = NULL;
    shooter_combo_label = NULL;
    for (int i = 0; i < 8; i++) {
        shooter_letter_labels[i] = NULL;
    }
    // Note: Don't free shooter_canvas_buf here - it's reused across screen recreations

    // Effect pointers
    shooter_laser_line = NULL;
    shooter_explosion_container = NULL;
    for (int i = 0; i < 4; i++) {
        shooter_explosion_circles[i] = NULL;
    }

    // Game over overlay
    shooter_game_over_overlay = NULL;
}

// Reset settings screen pointers
static void cleanupShooterSettingsPointers() {
    shooter_settings_screen = NULL;
    shooter_mode_row = NULL;
    shooter_mode_value = NULL;
    shooter_preset_row = NULL;
    shooter_preset_value = NULL;
    shooter_speed_row = NULL;
    shooter_speed_value = NULL;
    shooter_tone_row = NULL;
    shooter_tone_value = NULL;
    shooter_key_row = NULL;
    shooter_key_value = NULL;
    shooter_lives_row = NULL;
    shooter_lives_value = NULL;
    shooter_highscore_value = NULL;
    shooter_start_btn = NULL;
}

lv_obj_t* createMorseShooterScreen() {
    // Clean up any stale pointers from previous screen
    cleanupShooterScreenPointers();
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // HUD - top bar
    lv_obj_t* hud = lv_obj_create(screen);
    lv_obj_set_size(hud, SCREEN_WIDTH, 40);
    lv_obj_set_pos(hud, 0, 0);
    lv_obj_set_layout(hud, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(hud, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hud, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(hud, 15, 0);
    lv_obj_add_style(hud, getStyleStatusBar(), 0);
    lv_obj_clear_flag(hud, LV_OBJ_FLAG_SCROLLABLE);

    // Score
    lv_obj_t* score_container = lv_obj_create(hud);
    lv_obj_set_size(score_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(score_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(score_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(score_container, 5, 0);
    lv_obj_set_style_bg_opa(score_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(score_container, 0, 0);
    lv_obj_set_style_pad_all(score_container, 0, 0);

    lv_obj_t* score_title = lv_label_create(score_container);
    lv_label_set_text(score_title, "Score:");
    lv_obj_set_style_text_color(score_title, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(score_title, getThemeFonts()->font_body, 0);

    shooter_score_label = lv_label_create(score_container);
    lv_label_set_text(shooter_score_label, "0");
    lv_obj_set_style_text_color(shooter_score_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_set_style_text_font(shooter_score_label, getThemeFonts()->font_subtitle, 0);

    // Combo display (between score and lives)
    lv_obj_t* combo_container = lv_obj_create(hud);
    lv_obj_set_size(combo_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(combo_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(combo_container, 0, 0);
    lv_obj_set_style_pad_all(combo_container, 0, 0);

    shooter_combo_label = lv_label_create(combo_container);
    lv_label_set_text(shooter_combo_label, "");  // Hidden initially
    lv_obj_set_style_text_color(shooter_combo_label, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(shooter_combo_label, getThemeFonts()->font_subtitle, 0);

    // Lives (hearts) - support up to 5 lives
    shooter_lives_container = lv_obj_create(hud);
    lv_obj_set_size(shooter_lives_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(shooter_lives_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(shooter_lives_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(shooter_lives_container, 3, 0);
    lv_obj_set_style_bg_opa(shooter_lives_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(shooter_lives_container, 0, 0);
    lv_obj_set_style_pad_all(shooter_lives_container, 0, 0);

    // Initialize 5 heart icons (max supported lives)
    for (int i = 0; i < 5; i++) {
        lv_obj_t* heart = lv_label_create(shooter_lives_container);
        lv_label_set_text(heart, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(heart, LV_COLOR_TEXT_DISABLED, 0);  // Start hidden/gray
        lv_obj_set_style_text_font(heart, getThemeFonts()->font_body, 0);  // Smaller font for 5 hearts
    }

    // Game canvas area (for scenery)
    shooter_canvas = lv_canvas_create(screen);
    lv_obj_set_pos(shooter_canvas, 0, 40);

    // Allocate canvas buffer in PSRAM if available
    if (shooter_canvas_buf == NULL) {
        size_t buf_size = SCREEN_WIDTH * (SCREEN_HEIGHT - 80) * sizeof(lv_color_t);
        if (psramFound()) {
            shooter_canvas_buf = (lv_color_t*)ps_malloc(buf_size);
        } else {
            shooter_canvas_buf = (lv_color_t*)malloc(buf_size);
        }
    }

    if (shooter_canvas_buf != NULL) {
        lv_canvas_set_buffer(shooter_canvas, shooter_canvas_buf, SCREEN_WIDTH, SCREEN_HEIGHT - 80, LV_IMG_CF_TRUE_COLOR);
        lv_canvas_fill_bg(shooter_canvas, LV_COLOR_BG_DEEP, LV_OPA_COVER);
    }

    // Create falling letter labels (object pool - supports up to 8)
    for (int i = 0; i < 8; i++) {
        shooter_letter_labels[i] = lv_label_create(screen);
        lv_label_set_text(shooter_letter_labels[i], "");
        lv_obj_set_style_text_font(shooter_letter_labels[i], getThemeFonts()->font_large, 0);
        lv_obj_set_style_text_color(shooter_letter_labels[i], LV_COLOR_WARNING, 0);
        lv_obj_add_flag(shooter_letter_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Decoded text display (bottom HUD)
    lv_obj_t* bottom_hud = lv_obj_create(screen);
    lv_obj_set_size(bottom_hud, SCREEN_WIDTH, 40);
    lv_obj_set_pos(bottom_hud, 0, SCREEN_HEIGHT - 40);
    lv_obj_set_layout(bottom_hud, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bottom_hud, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_hud, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_style(bottom_hud, getStyleStatusBar(), 0);
    lv_obj_clear_flag(bottom_hud, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* decoded_title = lv_label_create(bottom_hud);
    lv_label_set_text(decoded_title, "Typing: ");
    lv_obj_set_style_text_color(decoded_title, LV_COLOR_TEXT_SECONDARY, 0);

    shooter_decoded_label = lv_label_create(bottom_hud);
    lv_label_set_text(shooter_decoded_label, "_");
    lv_obj_set_style_text_color(shooter_decoded_label, LV_COLOR_ACCENT_GREEN, 0);
    lv_obj_set_style_text_font(shooter_decoded_label, getThemeFonts()->font_subtitle, 0);

    // Invisible focus container for keyboard input (ESC to exit)
    lv_obj_t* focus_container = lv_obj_create(screen);
    lv_obj_set_size(focus_container, 1, 1);
    lv_obj_set_pos(focus_container, -10, -10);
    lv_obj_set_style_bg_opa(focus_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus_container, 0, 0);
    lv_obj_set_style_outline_width(focus_container, 0, 0);
    lv_obj_set_style_outline_width(focus_container, 0, LV_STATE_FOCUSED);
    lv_obj_clear_flag(focus_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(focus_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus_container, shooter_key_event_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus_container);
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }
    lv_group_focus_obj(focus_container);

    shooter_screen = screen;
    return screen;
}

// Update score display
void updateShooterScore(int score) {
    if (shooter_score_label != NULL) {
        lv_label_set_text_fmt(shooter_score_label, "%d", score);
    }
}

// Update lives display (supports 1-5 lives)
void updateShooterLives(int lives) {
    if (shooter_lives_container != NULL) {
        uint32_t child_count = lv_obj_get_child_cnt(shooter_lives_container);
        for (uint32_t i = 0; i < child_count && i < 5; i++) {
            lv_obj_t* heart = lv_obj_get_child(shooter_lives_container, i);
            if ((int)i < lives) {
                // Active heart - red
                lv_obj_set_style_text_color(heart, LV_COLOR_ERROR, 0);
                lv_obj_clear_flag(heart, LV_OBJ_FLAG_HIDDEN);
            } else {
                // Lost heart - gray (but still visible for context)
                lv_obj_set_style_text_color(heart, LV_COLOR_TEXT_DISABLED, 0);
                // Hide hearts beyond max lives for this game
                // Show gray hearts for lost lives
            }
        }
    }
}

// Update combo display
void updateShooterCombo(int combo, int multiplier) {
    if (shooter_combo_label != NULL) {
        if (combo >= 3 && multiplier > 1) {
            // Show combo with multiplier
            lv_label_set_text_fmt(shooter_combo_label, "x%d!", multiplier);

            // Color based on multiplier level
            if (multiplier >= 10) {
                lv_obj_set_style_text_color(shooter_combo_label, lv_color_hex(0xFF00FF), 0);  // Magenta for 10x
            } else if (multiplier >= 5) {
                lv_obj_set_style_text_color(shooter_combo_label, lv_color_hex(0xFF4400), 0);  // Orange for 5x
            } else if (multiplier >= 3) {
                lv_obj_set_style_text_color(shooter_combo_label, LV_COLOR_WARNING, 0);  // Yellow for 3x
            } else {
                lv_obj_set_style_text_color(shooter_combo_label, LV_COLOR_ACCENT_GREEN, 0);  // Green for 2x
            }
        } else {
            // Hide combo display when not active
            lv_label_set_text(shooter_combo_label, "");
        }
    }
}

// Update decoded text
void updateShooterDecoded(const char* text) {
    if (shooter_decoded_label != NULL) {
        if (text == NULL || strlen(text) == 0) {
            lv_label_set_text(shooter_decoded_label, "_");
        } else {
            lv_label_set_text(shooter_decoded_label, text);
        }
    }
}

// Show/hide/position a falling letter (supports up to 8 letters)
void updateShooterLetter(int index, char letter, int x, int y, bool visible) {
    if (index >= 0 && index < 8 && shooter_letter_labels[index] != NULL) {
        if (visible) {
            char buf[2] = {letter, '\0'};
            lv_label_set_text(shooter_letter_labels[index], buf);
            lv_obj_set_pos(shooter_letter_labels[index], x, y);
            lv_obj_clear_flag(shooter_letter_labels[index], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(shooter_letter_labels[index], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Show/hide/position a falling word (for Word and Callsign modes)
void updateShooterWord(int index, const char* word, int lettersTyped, int x, int y, bool visible) {
    if (index >= 0 && index < 8 && shooter_letter_labels[index] != NULL) {
        if (visible && word != NULL) {
            // Build display string with typed letters highlighted
            // Format: completed letters shown normally, remaining with dots
            String display = "";
            int len = strlen(word);
            for (int i = 0; i < len; i++) {
                display += word[i];
                if (i < len - 1) {
                    display += (i < lettersTyped) ? " " : ".";
                }
            }
            lv_label_set_text(shooter_letter_labels[index], display.c_str());
            lv_obj_set_pos(shooter_letter_labels[index], x, y);
            lv_obj_clear_flag(shooter_letter_labels[index], LV_OBJ_FLAG_HIDDEN);

            // Color: partially completed words show progress
            if (lettersTyped > 0 && lettersTyped < len) {
                lv_obj_set_style_text_color(shooter_letter_labels[index], LV_COLOR_ACCENT_GREEN, 0);
            } else {
                lv_obj_set_style_text_color(shooter_letter_labels[index], LV_COLOR_WARNING, 0);
            }
        } else {
            lv_obj_add_flag(shooter_letter_labels[index], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Helper to draw a small colorful house with roof on canvas
static void drawHouse(int x, int baseY, int width, int height, lv_color_t wallColor, lv_color_t roofColor, lv_color_t doorColor) {
    if (shooter_canvas == NULL) return;

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_COVER;

    // House body
    rect_dsc.bg_color = wallColor;
    lv_canvas_draw_rect(shooter_canvas, x, baseY - height, width, height, &rect_dsc);

    // Roof (filled triangle)
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = roofColor;
    line_dsc.width = 1;

    int roofHeight = height / 2 + 2;
    int peakX = x + width / 2;
    int peakY = baseY - height - roofHeight;

    for (int dy = 0; dy <= roofHeight; dy++) {
        int lineY = peakY + dy;
        float ratio = (float)dy / roofHeight;
        int halfWidth = (int)(ratio * (width / 2 + 2));
        lv_point_t points[2] = {{(lv_coord_t)(peakX - halfWidth), (lv_coord_t)lineY},
                                {(lv_coord_t)(peakX + halfWidth), (lv_coord_t)lineY}};
        lv_canvas_draw_line(shooter_canvas, points, 2, &line_dsc);
    }

    // Window (yellow glow)
    rect_dsc.bg_color = lv_color_hex(0xFFFF88);  // Warm yellow
    int winW = width / 3;
    int winH = height / 3;
    lv_canvas_draw_rect(shooter_canvas, x + width/2 - winW/2, baseY - height + height/4, winW, winH, &rect_dsc);

    // Door
    rect_dsc.bg_color = doorColor;
    int doorW = width / 4;
    int doorH = height / 2;
    lv_canvas_draw_rect(shooter_canvas, x + width/2 - doorW/2, baseY - doorH, doorW, doorH, &rect_dsc);
}

// Helper to draw a small evergreen tree (triangular)
static void drawPineTree(int x, int baseY, int height, lv_color_t color) {
    if (shooter_canvas == NULL) return;

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_COVER;

    // Trunk
    rect_dsc.bg_color = lv_color_hex(0x8B4513);
    int trunkW = height / 6;
    int trunkH = height / 4;
    lv_canvas_draw_rect(shooter_canvas, x - trunkW/2, baseY - trunkH, trunkW, trunkH, &rect_dsc);

    // Tree body (filled triangle - 3 layers for pine look)
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = 1;

    int treeTop = baseY - height;
    int treeHeight = height - trunkH;
    for (int dy = 0; dy <= treeHeight; dy++) {
        int lineY = treeTop + dy;
        float ratio = (float)dy / treeHeight;
        int halfWidth = (int)(ratio * (height / 3));
        lv_point_t points[2] = {{(lv_coord_t)(x - halfWidth), (lv_coord_t)lineY},
                                {(lv_coord_t)(x + halfWidth), (lv_coord_t)lineY}};
        lv_canvas_draw_line(shooter_canvas, points, 2, &line_dsc);
    }
}

// Helper to draw a round deciduous tree
static void drawRoundTree(int x, int baseY, int height, lv_color_t leafColor) {
    if (shooter_canvas == NULL) return;

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_COVER;

    // Trunk
    rect_dsc.bg_color = lv_color_hex(0x654321);
    int trunkW = height / 5;
    int trunkH = height / 3;
    lv_canvas_draw_rect(shooter_canvas, x - trunkW/2, baseY - trunkH, trunkW, trunkH, &rect_dsc);

    // Canopy (filled circle)
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = leafColor;
    line_dsc.width = 1;

    int radius = height / 3;
    int centerY = baseY - trunkH - radius;
    for (int dy = -radius; dy <= radius; dy++) {
        int halfWidth = (int)sqrt(radius * radius - dy * dy);
        if (halfWidth > 0) {
            lv_point_t points[2] = {{(lv_coord_t)(x - halfWidth), (lv_coord_t)(centerY + dy)},
                                    {(lv_coord_t)(x + halfWidth), (lv_coord_t)(centerY + dy)}};
            lv_canvas_draw_line(shooter_canvas, points, 2, &line_dsc);
        }
    }
}

// Helper to draw a bush/shrub
static void drawBush(int x, int baseY, int size, lv_color_t color) {
    if (shooter_canvas == NULL) return;

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = 1;

    // Simple half-circle bush
    int radius = size / 2;
    for (int dy = 0; dy <= radius; dy++) {
        int halfWidth = (int)sqrt(radius * radius - dy * dy);
        if (halfWidth > 0) {
            lv_point_t points[2] = {{(lv_coord_t)(x - halfWidth), (lv_coord_t)(baseY - dy)},
                                    {(lv_coord_t)(x + halfWidth), (lv_coord_t)(baseY - dy)}};
            lv_canvas_draw_line(shooter_canvas, points, 2, &line_dsc);
        }
    }
}

// Draw scenery on canvas (called once at game start)
void drawShooterScenery() {
    if (shooter_canvas == NULL || shooter_canvas_buf == NULL) return;

    // Clear canvas with night sky gradient (dark blue)
    lv_canvas_fill_bg(shooter_canvas, lv_color_hex(0x0a0a20), LV_OPA_COVER);

    // Canvas coordinates: 0,0 is top-left, canvas height is SCREEN_HEIGHT - 80
    int canvasHeight = SCREEN_HEIGHT - 80;
    int groundY = canvasHeight - 30;  // Ground line (lower for more scenery)

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_COVER;

    // Draw ground (grass gradient - darker at bottom)
    rect_dsc.bg_color = lv_color_hex(0x228B22);  // Forest green
    lv_canvas_draw_rect(shooter_canvas, 0, groundY, SCREEN_WIDTH, 30, &rect_dsc);
    rect_dsc.bg_color = lv_color_hex(0x1a6b1a);  // Darker green bottom
    lv_canvas_draw_rect(shooter_canvas, 0, groundY + 15, SCREEN_WIDTH, 15, &rect_dsc);

    // Small colorful houses spread across - various styles
    // Left side
    drawHouse(8, groundY, 28, 22, lv_color_hex(0xCD5C5C), lv_color_hex(0x8B0000), lv_color_hex(0x4a2511));    // Indian red, dark red roof
    drawHouse(50, groundY, 24, 18, lv_color_hex(0x87CEEB), lv_color_hex(0x4682B4), lv_color_hex(0x2F4F4F));   // Sky blue, steel blue roof
    drawHouse(90, groundY, 30, 24, lv_color_hex(0xFFE4B5), lv_color_hex(0xD2691E), lv_color_hex(0x8B4513));   // Moccasin, chocolate roof

    // Center-left
    drawHouse(140, groundY, 26, 20, lv_color_hex(0x98FB98), lv_color_hex(0x2E8B57), lv_color_hex(0x654321));  // Pale green, sea green roof
    drawHouse(180, groundY, 22, 16, lv_color_hex(0xFFB6C1), lv_color_hex(0xC71585), lv_color_hex(0x8B4513));  // Light pink, medium violet roof

    // Center-right (leave gap for turret)
    drawHouse(290, groundY, 24, 18, lv_color_hex(0xDDA0DD), lv_color_hex(0x9932CC), lv_color_hex(0x4a2511));  // Plum, dark orchid roof
    drawHouse(330, groundY, 28, 22, lv_color_hex(0xF0E68C), lv_color_hex(0xDAA520), lv_color_hex(0x8B4513));  // Khaki, goldenrod roof

    // Right side
    drawHouse(375, groundY, 26, 20, lv_color_hex(0xADD8E6), lv_color_hex(0x4169E1), lv_color_hex(0x2F4F4F));  // Light blue, royal blue roof
    drawHouse(415, groundY, 22, 16, lv_color_hex(0xFFA07A), lv_color_hex(0xFF4500), lv_color_hex(0x654321));  // Light salmon, orange red roof
    drawHouse(450, groundY, 24, 18, lv_color_hex(0xE6E6FA), lv_color_hex(0x6A5ACD), lv_color_hex(0x483D8B));  // Lavender, slate blue roof

    // Trees - varied types and colors
    drawPineTree(38, groundY, 28, lv_color_hex(0x006400));   // Dark green pine
    drawRoundTree(78, groundY, 24, lv_color_hex(0x32CD32));  // Lime green round
    drawPineTree(125, groundY, 22, lv_color_hex(0x228B22));  // Forest green pine
    drawRoundTree(168, groundY, 20, lv_color_hex(0x3CB371)); // Medium sea green
    drawPineTree(205, groundY, 26, lv_color_hex(0x2E8B57)); // Sea green pine

    drawPineTree(275, groundY, 24, lv_color_hex(0x006400));  // Dark green pine
    drawRoundTree(318, groundY, 22, lv_color_hex(0x228B22)); // Forest green
    drawPineTree(358, groundY, 20, lv_color_hex(0x32CD32));  // Lime pine
    drawRoundTree(402, groundY, 26, lv_color_hex(0x3CB371)); // Sea green round
    drawPineTree(438, groundY, 18, lv_color_hex(0x006400));  // Small dark pine

    // Bushes for extra detail
    drawBush(20, groundY, 10, lv_color_hex(0x228B22));
    drawBush(65, groundY, 8, lv_color_hex(0x32CD32));
    drawBush(110, groundY, 12, lv_color_hex(0x2E8B57));
    drawBush(155, groundY, 9, lv_color_hex(0x3CB371));
    drawBush(195, groundY, 11, lv_color_hex(0x228B22));
    drawBush(305, groundY, 10, lv_color_hex(0x32CD32));
    drawBush(345, groundY, 8, lv_color_hex(0x2E8B57));
    drawBush(390, groundY, 12, lv_color_hex(0x228B22));
    drawBush(425, groundY, 9, lv_color_hex(0x3CB371));
    drawBush(468, groundY, 10, lv_color_hex(0x006400));

    // === TURRET (center of screen) ===
    int turretCenterX = SCREEN_WIDTH / 2;

    // Turret base platform (metallic gray)
    rect_dsc.bg_color = lv_color_hex(0x708090);  // Slate gray
    lv_canvas_draw_rect(shooter_canvas, turretCenterX - 20, groundY - 8, 40, 8, &rect_dsc);

    // Turret body (darker metal)
    rect_dsc.bg_color = lv_color_hex(0x4a5568);
    lv_canvas_draw_rect(shooter_canvas, turretCenterX - 12, groundY - 20, 24, 12, &rect_dsc);

    // Turret dome (cyan highlight)
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = LV_COLOR_ACCENT_CYAN;
    line_dsc.width = 1;

    // Draw dome as half circle
    int domeRadius = 10;
    int domeCenterY = groundY - 20;
    for (int dy = -domeRadius; dy <= 0; dy++) {
        int halfWidth = (int)sqrt(domeRadius * domeRadius - dy * dy);
        if (halfWidth > 0) {
            lv_point_t points[2] = {{(lv_coord_t)(turretCenterX - halfWidth), (lv_coord_t)(domeCenterY + dy)},
                                    {(lv_coord_t)(turretCenterX + halfWidth), (lv_coord_t)(domeCenterY + dy)}};
            lv_canvas_draw_line(shooter_canvas, points, 2, &line_dsc);
        }
    }

    // Turret barrel (pointing up)
    line_dsc.color = lv_color_hex(0x00CED1);  // Dark turquoise
    line_dsc.width = 4;
    lv_point_t barrel[2] = {{(lv_coord_t)turretCenterX, (lv_coord_t)(groundY - 30)},
                            {(lv_coord_t)turretCenterX, (lv_coord_t)(groundY - 50)}};
    lv_canvas_draw_line(shooter_canvas, barrel, 2, &line_dsc);

    // Barrel tip glow
    line_dsc.color = LV_COLOR_ACCENT_CYAN;
    line_dsc.width = 6;
    lv_point_t tip[2] = {{(lv_coord_t)turretCenterX, (lv_coord_t)(groundY - 48)},
                         {(lv_coord_t)turretCenterX, (lv_coord_t)(groundY - 52)}};
    lv_canvas_draw_line(shooter_canvas, tip, 2, &line_dsc);
}

// ============================================
// Visual Effects and Game Over
// ============================================

// Laser beam points (line object declared via forward declaration above)
static lv_point_t shooter_laser_points[2];

// Animation callback for opacity
static void shooter_effect_fade_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t*)obj, v, 0);
}

// Animation complete callback - hide the object
static void shooter_effect_fade_ready(lv_anim_t* a) {
    lv_obj_add_flag((lv_obj_t*)a->var, LV_OBJ_FLAG_HIDDEN);
}

// Animation callback for explosion scale (simulated by moving circles outward)
static void shooter_explosion_scale_cb(void* obj, int32_t v) {
    // v goes from 0 to 100, we use it to expand circles
    if (shooter_explosion_container == NULL) return;

    int scale = v;  // 0 to 100
    for (int i = 0; i < 4; i++) {
        if (shooter_explosion_circles[i] != NULL) {
            // Position circles in 4 directions, expanding outward
            int offset = (scale * 15) / 100;  // Max 15 pixels outward
            int dx = 0, dy = 0;
            switch(i) {
                case 0: dx = -offset; dy = -offset; break;  // Top-left
                case 1: dx = offset; dy = -offset; break;   // Top-right
                case 2: dx = -offset; dy = offset; break;   // Bottom-left
                case 3: dx = offset; dy = offset; break;    // Bottom-right
            }
            lv_obj_set_pos(shooter_explosion_circles[i], 10 + dx, 10 + dy);
        }
    }
}

// Show laser beam from turret to target position
static void showLaserBeam(int targetX, int targetY) {
    if (shooter_screen == NULL) return;

    // Turret position (center of screen, near bottom of canvas area)
    int turretX = SCREEN_WIDTH / 2;
    int turretY = SCREEN_HEIGHT - 80 - 10;  // Near bottom of canvas, accounting for HUD

    // Create laser line if needed
    if (shooter_laser_line == NULL) {
        shooter_laser_line = lv_line_create(shooter_screen);
        lv_obj_set_style_line_width(shooter_laser_line, 3, 0);
        lv_obj_set_style_line_rounded(shooter_laser_line, true, 0);
    }

    // Set line points
    shooter_laser_points[0].x = turretX;
    shooter_laser_points[0].y = turretY;
    shooter_laser_points[1].x = targetX + 10;  // Center of letter
    shooter_laser_points[1].y = targetY + 10;

    lv_line_set_points(shooter_laser_line, shooter_laser_points, 2);
    lv_obj_set_style_line_color(shooter_laser_line, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_set_style_opa(shooter_laser_line, LV_OPA_COVER, 0);
    lv_obj_clear_flag(shooter_laser_line, LV_OBJ_FLAG_HIDDEN);

    // Fade out animation for laser
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, shooter_laser_line);
    lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&anim, 150);  // Quick fade
    lv_anim_set_exec_cb(&anim, shooter_effect_fade_cb);
    lv_anim_set_ready_cb(&anim, shooter_effect_fade_ready);
    lv_anim_start(&anim);
}

// Show explosion effect at position
static void showExplosion(int x, int y) {
    if (shooter_screen == NULL) return;

    // Create explosion container if needed
    if (shooter_explosion_container == NULL) {
        shooter_explosion_container = lv_obj_create(shooter_screen);
        lv_obj_set_size(shooter_explosion_container, 40, 40);
        lv_obj_set_style_bg_opa(shooter_explosion_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(shooter_explosion_container, 0, 0);
        lv_obj_clear_flag(shooter_explosion_container, LV_OBJ_FLAG_SCROLLABLE);

        // Create 4 explosion circles (yellow, orange, red, white center)
        lv_color_t colors[4] = {
            lv_color_hex(0xFFFF00),  // Yellow
            lv_color_hex(0xFF8800),  // Orange
            lv_color_hex(0xFF0000),  // Red
            lv_color_hex(0xFFFFFF)   // White
        };
        int sizes[4] = {16, 12, 8, 4};

        for (int i = 0; i < 4; i++) {
            shooter_explosion_circles[i] = lv_obj_create(shooter_explosion_container);
            lv_obj_set_size(shooter_explosion_circles[i], sizes[i], sizes[i]);
            lv_obj_set_style_radius(shooter_explosion_circles[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(shooter_explosion_circles[i], colors[i], 0);
            lv_obj_set_style_bg_opa(shooter_explosion_circles[i], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(shooter_explosion_circles[i], 0, 0);
            lv_obj_set_pos(shooter_explosion_circles[i], 10, 10);  // Center initially
        }
    }

    // Position container at explosion location
    lv_obj_set_pos(shooter_explosion_container, x - 10, y - 10);
    lv_obj_set_style_opa(shooter_explosion_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(shooter_explosion_container, LV_OBJ_FLAG_HIDDEN);

    // Reset circle positions
    for (int i = 0; i < 4; i++) {
        if (shooter_explosion_circles[i] != NULL) {
            lv_obj_set_pos(shooter_explosion_circles[i], 10, 10);
        }
    }

    // Expansion animation
    lv_anim_t expand_anim;
    lv_anim_init(&expand_anim);
    lv_anim_set_var(&expand_anim, shooter_explosion_container);
    lv_anim_set_values(&expand_anim, 0, 100);
    lv_anim_set_time(&expand_anim, 200);
    lv_anim_set_exec_cb(&expand_anim, shooter_explosion_scale_cb);
    lv_anim_start(&expand_anim);

    // Fade out animation (starts after expansion)
    lv_anim_t fade_anim;
    lv_anim_init(&fade_anim);
    lv_anim_set_var(&fade_anim, shooter_explosion_container);
    lv_anim_set_values(&fade_anim, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&fade_anim, 200);
    lv_anim_set_delay(&fade_anim, 150);  // Start fading after expansion begins
    lv_anim_set_exec_cb(&fade_anim, shooter_effect_fade_cb);
    lv_anim_set_ready_cb(&fade_anim, shooter_effect_fade_ready);
    lv_anim_start(&fade_anim);
}

// Show hit effect at position (combines laser + explosion)
void showShooterHitEffect(int x, int y) {
    if (shooter_screen == NULL) return;

    // Show laser beam from turret to target
    showLaserBeam(x, y);

    // Show explosion at target
    showExplosion(x, y);
}

// (shooter_game_over_overlay declared via forward declaration above)

// External game state
extern int gameScore;
extern bool gameOver;

// Forward declaration for game restart
extern void resetGame();

// Key callback for game over screen
static void shooter_gameover_key_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ENTER) {
        // Restart game
        if (shooter_game_over_overlay != NULL) {
            lv_obj_del(shooter_game_over_overlay);
            shooter_game_over_overlay = NULL;
        }
        extern LGFX tft;
        resetGame();
        drawShooterScenery();
        beep(TONE_SELECT, BEEP_MEDIUM);
    } else if (key == LV_KEY_ESC) {
        // Exit to games menu
        if (shooter_game_over_overlay != NULL) {
            lv_obj_del(shooter_game_over_overlay);
            shooter_game_over_overlay = NULL;
        }
        onLVGLBackNavigation();
        lv_event_stop_processing(e);
    }
}

// Show game over overlay
void showShooterGameOver() {
    if (shooter_screen == NULL) return;

    extern int getCurrentModeHighScore();
    bool isHighScore = (gameScore >= getCurrentModeHighScore() && gameScore > 0);

    // Create overlay using the existing helper
    shooter_game_over_overlay = createGameOverOverlay(shooter_screen, gameScore, isHighScore);

    // Make overlay focusable for keyboard input
    lv_obj_add_flag(shooter_game_over_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(shooter_game_over_overlay, shooter_gameover_key_cb, LV_EVENT_KEY, NULL);

    // Add to navigation group and focus
    clearNavigationGroup();
    addNavigableWidget(shooter_game_over_overlay);
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }
    lv_group_focus_obj(shooter_game_over_overlay);
}

// ============================================
// Morse Shooter Settings Screen
// ============================================

// External state from game_morse_shooter.h
// Note: cwSpeed, cwTone, cwKeyType are from settings_cw.h (now included above)
extern ShooterDifficulty shooterDifficulty;
extern int shooterHighScores[3];
extern void loadShooterPrefs();
extern void saveShooterPrefs();
extern void startMorseShooter(LGFX& tft);

// Screen state
enum ShooterScreenState {
    SHOOTER_STATE_SETTINGS,
    SHOOTER_STATE_PLAYING,
    SHOOTER_STATE_GAME_OVER
};
static ShooterScreenState shooterScreenState = SHOOTER_STATE_SETTINGS;

// External settings from game_morse_shooter.h
extern ShooterSettings shooterSettings;
extern int shooterHighScoreClassic;
extern int shooterHighScoreProgressive;
extern int shooterHighScoreWord;
extern int shooterHighScoreCallsign;
extern void applyShooterPreset(ShooterPreset preset);

// Key type names
static const char* shooter_key_names[] = {"Straight", "Iambic A", "Iambic B"};

// Forward declarations
static void shooter_settings_update_all();
void startShooterFromSettings();

// Row indices for settings screen
// 0=Mode, 1=Preset, 2=Speed, 3=Tone, 4=Key Type, 5=Lives, 6=START
#define SHOOTER_ROW_MODE    0
#define SHOOTER_ROW_PRESET  1
#define SHOOTER_ROW_SPEED   2
#define SHOOTER_ROW_TONE    3
#define SHOOTER_ROW_KEY     4
#define SHOOTER_ROW_LIVES   5
#define SHOOTER_ROW_START   6

// Get high score for current mode
static int getShooterHighScoreForMode() {
    switch (shooterSettings.gameMode) {
        case 0: return shooterHighScoreClassic;
        case 1: return shooterHighScoreProgressive;
        case 2: return shooterHighScoreWord;
        case 3: return shooterHighScoreCallsign;
        default: return 0;
    }
}

// Adjust a setting value by direction (-1 or +1)
static void shooterAdjustValue(int row, int direction) {
    extern const PresetConfig PRESET_CONFIGS[];

    switch (row) {
        case SHOOTER_ROW_MODE: {
            int newMode = (int)shooterSettings.gameMode + direction;
            if (newMode >= 0 && newMode <= 3) {
                shooterSettings.gameMode = newMode;
            }
            break;
        }
        case SHOOTER_ROW_PRESET: {
            int newPreset = (int)shooterSettings.preset + direction;
            if (newPreset >= 0 && newPreset <= 6) {
                shooterSettings.preset = newPreset;
                applyShooterPreset((ShooterPreset)shooterSettings.preset);
            }
            break;
        }
        case SHOOTER_ROW_SPEED:
            // Only editable in Custom preset
            if (shooterSettings.preset != 0) return;
            if (direction < 0 && cwSpeed > 5) cwSpeed--;
            else if (direction > 0 && cwSpeed < 40) cwSpeed++;
            else return;
            break;
        case SHOOTER_ROW_TONE:
            if (direction < 0 && cwTone > 400) cwTone -= 50;
            else if (direction > 0 && cwTone < 1200) cwTone += 50;
            else return;
            break;
        case SHOOTER_ROW_KEY:
            if (direction > 0) {
                if (cwKeyType == KEY_STRAIGHT) cwKeyType = KEY_IAMBIC_A;
                else if (cwKeyType == KEY_IAMBIC_A) cwKeyType = KEY_IAMBIC_B;
                else return;
            } else {
                if (cwKeyType == KEY_IAMBIC_B) cwKeyType = KEY_IAMBIC_A;
                else if (cwKeyType == KEY_IAMBIC_A) cwKeyType = KEY_STRAIGHT;
                else return;
            }
            break;
        case SHOOTER_ROW_LIVES:
            // Only editable in Custom preset
            if (shooterSettings.preset != 0) return;
            if (direction < 0 && shooterSettings.startLives > 1) shooterSettings.startLives--;
            else if (direction > 0 && shooterSettings.startLives < 5) shooterSettings.startLives++;
            else return;
            break;
        default:
            return;
    }
    beep(TONE_MENU_NAV, BEEP_SHORT);
    shooter_settings_update_all();
}

// Value handler: processes LEFT/RIGHT for value adjustment on settings rows.
// Registered BEFORE linear_nav_handler so it runs first.
static void shooter_value_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    // Only handle LEFT/RIGHT - let linear_nav_handler handle UP/DOWN/TAB
    if (key != LV_KEY_LEFT && key != LV_KEY_RIGHT) return;

    int row = (int)(intptr_t)lv_event_get_user_data(e);
    int direction = (key == LV_KEY_RIGHT) ? 1 : -1;
    shooterAdjustValue(row, direction);
    lv_event_stop_processing(e);
}

// Click handler for settings rows: ENTER cycles value forward
static void shooter_row_click_handler(lv_event_t* e) {
    int row = (int)(intptr_t)lv_event_get_user_data(e);
    if (row == SHOOTER_ROW_START) {
        // Start game
        saveShooterPrefs();
        saveCWSettings();
        beep(TONE_SELECT, BEEP_MEDIUM);
        startShooterFromSettings();
    } else {
        // Cycle value forward on ENTER
        shooterAdjustValue(row, 1);
    }
}

// Update all value labels to reflect current settings
static void shooter_settings_update_all() {
    extern const PresetConfig PRESET_CONFIGS[];
    bool isCustom = (shooterSettings.preset == 0);

    // Mode
    if (shooter_mode_value != NULL) {
        const char* modeNames[] = {"Classic", "Progressive", "Word", "Callsign"};
        lv_label_set_text(shooter_mode_value, modeNames[shooterSettings.gameMode]);
    }

    // Preset
    if (shooter_preset_value != NULL) {
        const char* presetNames[] = {"Custom", "Beginner", "Easy", "Medium", "Hard", "Expert", "Insane"};
        lv_label_set_text(shooter_preset_value, presetNames[shooterSettings.preset]);
    }

    // Speed (WPM) - dimmed when locked by preset
    if (shooter_speed_value != NULL) {
        lv_label_set_text_fmt(shooter_speed_value, "%d WPM", cwSpeed);
        lv_obj_set_style_text_color(shooter_speed_value,
            isCustom ? LV_COLOR_ACCENT_CYAN : LV_COLOR_TEXT_DISABLED, 0);
    }

    // Tone
    if (shooter_tone_value != NULL) {
        lv_label_set_text_fmt(shooter_tone_value, "%d Hz", cwTone);
    }

    // Key type
    if (shooter_key_value != NULL) {
        lv_label_set_text(shooter_key_value, shooter_key_names[cwKeyType]);
    }

    // Lives - dimmed when locked by preset
    if (shooter_lives_value != NULL) {
        int lives = shooterSettings.startLives;
        if (!isCustom) {
            lives = PRESET_CONFIGS[shooterSettings.preset].startLives;
        }
        lv_label_set_text_fmt(shooter_lives_value, "%d", lives);
        lv_obj_set_style_text_color(shooter_lives_value,
            isCustom ? LV_COLOR_ACCENT_CYAN : LV_COLOR_TEXT_DISABLED, 0);
    }

    // High score for current mode
    if (shooter_highscore_value != NULL) {
        lv_label_set_text_fmt(shooter_highscore_value, "%d", getShooterHighScoreForMode());
    }
}

// Create settings screen for Morse Shooter
lv_obj_t* createMorseShooterSettingsScreen() {
    // Clean up any stale pointers from previous screens
    cleanupShooterScreenPointers();
    cleanupShooterSettingsPointers();

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
    lv_label_set_text(title, "MORSE SHOOTER");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery)
    createCompactStatusBar(screen);

    // High score display (top right of content area)
    lv_obj_t* hs_container = lv_obj_create(screen);
    lv_obj_set_size(hs_container, 120, 50);
    lv_obj_set_pos(hs_container, SCREEN_WIDTH - 140, HEADER_HEIGHT + 10);
    lv_obj_set_style_bg_opa(hs_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hs_container, 0, 0);
    lv_obj_clear_flag(hs_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hs_label = lv_label_create(hs_container);
    lv_label_set_text(hs_label, "High Score");
    lv_obj_set_style_text_color(hs_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(hs_label, getThemeFonts()->font_small, 0);
    lv_obj_align(hs_label, LV_ALIGN_TOP_MID, 0, 0);

    shooter_highscore_value = lv_label_create(hs_container);
    lv_label_set_text_fmt(shooter_highscore_value, "%d", getShooterHighScoreForMode());
    lv_obj_set_style_text_color(shooter_highscore_value, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(shooter_highscore_value, getThemeFonts()->font_title, 0);
    lv_obj_align(shooter_highscore_value, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Settings container
    lv_obj_t* settings_card = lv_obj_create(screen);
    lv_obj_set_size(settings_card, SCREEN_WIDTH - 40, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 80);
    lv_obj_set_pos(settings_card, 20, HEADER_HEIGHT + 10);
    lv_obj_set_layout(settings_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(settings_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(settings_card, 2, 0);
    lv_obj_set_style_pad_all(settings_card, 6, 0);
    applyCardStyle(settings_card);
    lv_obj_set_scroll_dir(settings_card, LV_DIR_VER);

    // Helper: create a navigable settings row button
    auto createSettingsRow = [&](const char* label_text, int row_index, lv_obj_t** row_out, lv_obj_t** value_out) {
        lv_obj_t* btn = lv_btn_create(settings_card);
        lv_obj_set_size(btn, SCREEN_WIDTH - 80, 26);
        lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_color(btn, LV_COLOR_BG_LAYER2, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, LV_COLOR_BORDER_SUBTLE, 0);
        lv_obj_set_style_pad_hor(btn, 10, 0);
        lv_obj_set_style_pad_ver(btn, 2, 0);
        // Focus style: cyan border when selected
        lv_obj_set_style_border_color(btn, LV_COLOR_ACCENT_CYAN, LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(btn, 2, LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(btn, LV_COLOR_BG_LAYER2, LV_STATE_FOCUSED);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label_text);
        lv_obj_set_style_text_color(lbl, LV_COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(lbl, getThemeFonts()->font_small, 0);

        lv_obj_t* val = lv_label_create(btn);
        lv_obj_set_style_text_color(val, LV_COLOR_ACCENT_CYAN, 0);
        lv_obj_set_style_text_font(val, getThemeFonts()->font_small, 0);

        // Value handler FIRST (handles LEFT/RIGHT), then linear_nav_handler (UP/DOWN)
        lv_obj_add_event_cb(btn, shooter_value_handler, LV_EVENT_KEY, (void*)(intptr_t)row_index);
        lv_obj_add_event_cb(btn, shooter_row_click_handler, LV_EVENT_CLICKED, (void*)(intptr_t)row_index);
        lv_obj_add_event_cb(btn, linear_nav_handler, LV_EVENT_KEY, NULL);
        addNavigableWidget(btn);

        *row_out = btn;
        *value_out = val;
    };

    // Create settings rows (no legacy Difficulty row)
    createSettingsRow("Mode", SHOOTER_ROW_MODE, &shooter_mode_row, &shooter_mode_value);
    createSettingsRow("Preset", SHOOTER_ROW_PRESET, &shooter_preset_row, &shooter_preset_value);
    createSettingsRow("Speed", SHOOTER_ROW_SPEED, &shooter_speed_row, &shooter_speed_value);
    createSettingsRow("Tone", SHOOTER_ROW_TONE, &shooter_tone_row, &shooter_tone_value);
    createSettingsRow("Key Type", SHOOTER_ROW_KEY, &shooter_key_row, &shooter_key_value);
    createSettingsRow("Lives", SHOOTER_ROW_LIVES, &shooter_lives_row, &shooter_lives_value);

    // Start button
    shooter_start_btn = lv_btn_create(screen);
    lv_obj_set_size(shooter_start_btn, 200, 50);
    lv_obj_set_pos(shooter_start_btn, (SCREEN_WIDTH - 200) / 2, SCREEN_HEIGHT - FOOTER_HEIGHT - 70);
    lv_obj_set_style_bg_color(shooter_start_btn, LV_COLOR_SUCCESS, 0);
    lv_obj_set_style_bg_color(shooter_start_btn, lv_color_hex(0x00CC44), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(shooter_start_btn, 8, 0);
    lv_obj_set_style_border_width(shooter_start_btn, 1, 0);
    lv_obj_set_style_border_color(shooter_start_btn, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_border_color(shooter_start_btn, LV_COLOR_ACCENT_CYAN, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(shooter_start_btn, 2, LV_STATE_FOCUSED);

    lv_obj_t* btn_label = lv_label_create(shooter_start_btn);
    lv_label_set_text(btn_label, "START GAME");
    lv_obj_set_style_text_font(btn_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_center(btn_label);

    // Standard navigation for START button
    lv_obj_add_event_cb(shooter_start_btn, shooter_row_click_handler, LV_EVENT_CLICKED, (void*)(intptr_t)SHOOTER_ROW_START);
    lv_obj_add_event_cb(shooter_start_btn, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(shooter_start_btn);

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate   " LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " Adjust   ENTER Start   ESC Back");
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    // Focus on START button by default (quick-start for returning users)
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }
    lv_group_focus_obj(shooter_start_btn);

    // Initialize values
    shooter_settings_update_all();

    shooterScreenState = SHOOTER_STATE_SETTINGS;
    shooter_settings_screen = screen;
    return screen;
}

// Transition from settings to game
void startShooterFromSettings() {
    extern LGFX tft;
    shooterScreenState = SHOOTER_STATE_PLAYING;
    clearNavigationGroup();
    lv_obj_t* game_screen = createMorseShooterScreen();
    loadScreen(game_screen, SCREEN_ANIM_FADE);
    startMorseShooter(tft);
    drawShooterScenery();
}

// ============================================
// Memory Chain Game Screen - NEW IMPLEMENTATION
// ============================================

// Forward declaration - game implementation is in game_memory_chain.h
lv_obj_t* createMemoryChainScreen();  // Implemented in game_memory_chain.h

// Forward declarations - game implementation is in game_cw_speeder.h
lv_obj_t* createCWSpeedSelectScreen();  // Implemented in game_cw_speeder.h
lv_obj_t* createCWSpeedGameScreen();    // Implemented in game_cw_speeder.h


// ============================================
// Game Over / Pause Overlays
// ============================================

lv_obj_t* createGameOverOverlay(lv_obj_t* parent, int final_score, bool is_high_score) {
    // Semi-transparent overlay
    lv_obj_t* overlay = lv_obj_create(parent);
    lv_obj_set_size(overlay, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Game over card
    lv_obj_t* card = lv_obj_create(overlay);
    lv_obj_set_size(card, 300, 180);
    lv_obj_center(card);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 15, 0);
    applyCardStyle(card);

    lv_obj_t* game_over_label = lv_label_create(card);
    lv_label_set_text(game_over_label, "GAME OVER");
    lv_obj_set_style_text_font(game_over_label, getThemeFonts()->font_title, 0);
    lv_obj_set_style_text_color(game_over_label, LV_COLOR_ERROR, 0);

    lv_obj_t* score_label = lv_label_create(card);
    lv_label_set_text_fmt(score_label, "Final Score: %d", final_score);
    lv_obj_set_style_text_font(score_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(score_label, LV_COLOR_TEXT_PRIMARY, 0);

    if (is_high_score) {
        lv_obj_t* high_score_label = lv_label_create(card);
        lv_label_set_text(high_score_label, "NEW HIGH SCORE!");
        lv_obj_set_style_text_font(high_score_label, getThemeFonts()->font_input, 0);
        lv_obj_set_style_text_color(high_score_label, LV_COLOR_WARNING, 0);
    }

    lv_obj_t* restart_hint = lv_label_create(card);
    lv_label_set_text(restart_hint, "Press ENTER to restart");
    lv_obj_add_style(restart_hint, getStyleLabelBody(), 0);

    return overlay;
}

// ============================================
// Screen Selector
// Mode values MUST match MenuMode enum in menu_ui.h
// ============================================

lv_obj_t* createGameScreenForMode(int mode) {
    switch (mode) {
        case MODE_MORSE_SHOOTER:
            // Show settings screen first, game starts when user presses START
            return createMorseShooterSettingsScreen();
        case MODE_MORSE_MEMORY:
            return createMemoryChainScreen();

        // Spark Watch game modes
        case MODE_SPARK_WATCH:
        case MODE_SPARK_WATCH_DIFFICULTY:
        case MODE_SPARK_WATCH_CAMPAIGN:
        case MODE_SPARK_WATCH_MISSION:
        case MODE_SPARK_WATCH_CHALLENGE:
        case MODE_SPARK_WATCH_BRIEFING:
        case MODE_SPARK_WATCH_GAMEPLAY:
        case MODE_SPARK_WATCH_RESULTS:
        case MODE_SPARK_WATCH_DEBRIEFING:
        case MODE_SPARK_WATCH_SETTINGS:
        case MODE_SPARK_WATCH_STATS:
            return createSparkWatchScreenForMode(mode);

        // CW Speeder game modes
        case MODE_CW_SPEEDER_SELECT:
            return createCWSpeedSelectScreen();
        case MODE_CW_SPEEDER:
            return createCWSpeedGameScreen();

        default:
            Serial.printf("[GameScreens] Unknown game mode: %d\n", mode);
            return NULL;
    }
}

#endif // LV_GAME_SCREENS_H
