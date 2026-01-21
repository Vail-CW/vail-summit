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
static lv_obj_t* shooter_letter_labels[5] = {NULL, NULL, NULL, NULL, NULL};  // Pool of falling letter objects

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
static lv_obj_t* shooter_diff_value = NULL;
static lv_obj_t* shooter_speed_value = NULL;
static lv_obj_t* shooter_tone_value = NULL;
static lv_obj_t* shooter_key_value = NULL;
static lv_obj_t* shooter_highscore_value = NULL;
static lv_obj_t* shooter_start_btn = NULL;
static lv_obj_t* shooter_diff_row = NULL;
static lv_obj_t* shooter_speed_row = NULL;
static lv_obj_t* shooter_tone_row = NULL;
static lv_obj_t* shooter_key_row = NULL;

// Reset all shooter screen static pointers (called before creating new screen)
// This prevents crashes from stale pointers after screen deletion
static void cleanupShooterScreenPointers() {
    // Game screen pointers
    shooter_screen = NULL;
    shooter_canvas = NULL;
    shooter_score_label = NULL;
    shooter_lives_container = NULL;
    shooter_decoded_label = NULL;
    for (int i = 0; i < 5; i++) {
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
    shooter_diff_value = NULL;
    shooter_speed_value = NULL;
    shooter_tone_value = NULL;
    shooter_key_value = NULL;
    shooter_highscore_value = NULL;
    shooter_start_btn = NULL;
    shooter_diff_row = NULL;
    shooter_speed_row = NULL;
    shooter_tone_row = NULL;
    shooter_key_row = NULL;
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

    // Lives (hearts)
    shooter_lives_container = lv_obj_create(hud);
    lv_obj_set_size(shooter_lives_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(shooter_lives_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(shooter_lives_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(shooter_lives_container, 5, 0);
    lv_obj_set_style_bg_opa(shooter_lives_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(shooter_lives_container, 0, 0);
    lv_obj_set_style_pad_all(shooter_lives_container, 0, 0);

    // Initialize 3 heart icons
    for (int i = 0; i < 3; i++) {
        lv_obj_t* heart = lv_label_create(shooter_lives_container);
        lv_label_set_text(heart, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(heart, LV_COLOR_ERROR, 0);
        lv_obj_set_style_text_font(heart, getThemeFonts()->font_subtitle, 0);
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

    // Create falling letter labels (object pool)
    for (int i = 0; i < 5; i++) {
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

// Update lives display
void updateShooterLives(int lives) {
    if (shooter_lives_container != NULL) {
        uint32_t child_count = lv_obj_get_child_cnt(shooter_lives_container);
        for (uint32_t i = 0; i < child_count && i < 3; i++) {
            lv_obj_t* heart = lv_obj_get_child(shooter_lives_container, i);
            if ((int)i < lives) {
                lv_obj_set_style_text_color(heart, LV_COLOR_ERROR, 0);
            } else {
                lv_obj_set_style_text_color(heart, LV_COLOR_TEXT_DISABLED, 0);
            }
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

// Show/hide/position a falling letter
void updateShooterLetter(int index, char letter, int x, int y, bool visible) {
    if (index >= 0 && index < 5 && shooter_letter_labels[index] != NULL) {
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

    extern int shooterHighScores[3];
    extern ShooterDifficulty shooterDifficulty;
    bool isHighScore = (gameScore == shooterHighScores[shooterDifficulty] && gameScore > 0);

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

// (Settings screen widget pointers declared via forward declarations above)

// Track which row is focused (0=difficulty, 1=speed, 2=tone, 3=key, 4=start)
static int shooter_settings_focus = 0;

// Difficulty names for display
static const char* shooter_diff_names[] = {"Easy", "Medium", "Hard"};

// Key type names
static const char* shooter_key_names[] = {"Straight", "Iambic A", "Iambic B"};

// Forward declarations
static void shooter_settings_update_focus();
static void shooter_settings_update_values();
void startShooterFromSettings();

// Update focus styling on settings rows
static void shooter_settings_update_focus() {
    lv_obj_t* rows[] = {shooter_diff_row, shooter_speed_row, shooter_tone_row, shooter_key_row, shooter_start_btn};
    const int num_rows = 5;

    for (int i = 0; i < num_rows; i++) {
        if (rows[i] == NULL) continue;

        if (i == shooter_settings_focus) {
            // Focused row - highlight border
            lv_obj_set_style_border_color(rows[i], LV_COLOR_ACCENT_CYAN, 0);
            lv_obj_set_style_border_width(rows[i], 2, 0);
        } else {
            // Not focused - subtle border
            lv_obj_set_style_border_color(rows[i], LV_COLOR_BORDER_SUBTLE, 0);
            lv_obj_set_style_border_width(rows[i], 1, 0);
        }
    }
}

// Update value labels to reflect current settings
static void shooter_settings_update_values() {
    if (shooter_diff_value != NULL) {
        lv_label_set_text(shooter_diff_value, shooter_diff_names[shooterDifficulty]);
    }
    if (shooter_speed_value != NULL) {
        lv_label_set_text_fmt(shooter_speed_value, "%d WPM", cwSpeed);
    }
    if (shooter_tone_value != NULL) {
        lv_label_set_text_fmt(shooter_tone_value, "%d Hz", cwTone);
    }
    if (shooter_key_value != NULL) {
        lv_label_set_text(shooter_key_value, shooter_key_names[cwKeyType]);
    }
    if (shooter_highscore_value != NULL) {
        lv_label_set_text_fmt(shooter_highscore_value, "%d", shooterHighScores[shooterDifficulty]);
    }
}

// Key event callback for settings screen
static void shooter_settings_key_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);
    Serial.printf("[Shooter Settings] Key: %lu (0x%02lX), focus=%d\n", key, key, shooter_settings_focus);

    switch(key) {
        case LV_KEY_UP:
            shooter_settings_focus--;
            if (shooter_settings_focus < 0) shooter_settings_focus = 4;
            shooter_settings_update_focus();
            beep(TONE_MENU_NAV, BEEP_SHORT);
            break;

        case LV_KEY_DOWN:
            shooter_settings_focus++;
            if (shooter_settings_focus > 4) shooter_settings_focus = 0;
            shooter_settings_update_focus();
            beep(TONE_MENU_NAV, BEEP_SHORT);
            break;

        case LV_KEY_LEFT:
            switch (shooter_settings_focus) {
                case 0:  // Difficulty
                    if (shooterDifficulty > SHOOTER_EASY) {
                        shooterDifficulty = (ShooterDifficulty)(shooterDifficulty - 1);
                        shooter_settings_update_values();
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                    break;
                case 1:  // Speed
                    if (cwSpeed > 5) {
                        cwSpeed--;
                        shooter_settings_update_values();
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                    break;
                case 2:  // Tone
                    if (cwTone > 400) {
                        cwTone -= 50;
                        shooter_settings_update_values();
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                    break;
                case 3:  // Key Type (cycle: Straight -> Iambic A -> Iambic B)
                    if (cwKeyType == KEY_IAMBIC_B) {
                        cwKeyType = KEY_IAMBIC_A;
                    } else if (cwKeyType == KEY_IAMBIC_A) {
                        cwKeyType = KEY_STRAIGHT;
                    }
                    // If already KEY_STRAIGHT, stay there (no wrap on left)
                    shooter_settings_update_values();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    break;
            }
            break;

        case LV_KEY_RIGHT:
            switch (shooter_settings_focus) {
                case 0:  // Difficulty
                    if (shooterDifficulty < SHOOTER_HARD) {
                        shooterDifficulty = (ShooterDifficulty)(shooterDifficulty + 1);
                        shooter_settings_update_values();
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                    break;
                case 1:  // Speed
                    if (cwSpeed < 40) {
                        cwSpeed++;
                        shooter_settings_update_values();
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                    break;
                case 2:  // Tone
                    if (cwTone < 1200) {
                        cwTone += 50;
                        shooter_settings_update_values();
                        beep(TONE_MENU_NAV, BEEP_SHORT);
                    }
                    break;
                case 3:  // Key Type (cycle: Straight -> Iambic A -> Iambic B)
                    if (cwKeyType == KEY_STRAIGHT) {
                        cwKeyType = KEY_IAMBIC_A;
                    } else if (cwKeyType == KEY_IAMBIC_A) {
                        cwKeyType = KEY_IAMBIC_B;
                    }
                    // If already KEY_IAMBIC_B, stay there (no wrap on right)
                    shooter_settings_update_values();
                    beep(TONE_MENU_NAV, BEEP_SHORT);
                    break;
            }
            break;

        case LV_KEY_ENTER:
            if (shooter_settings_focus == 4) {  // Start button
                // Save settings and start game
                saveShooterPrefs();
                saveCWSettings();
                beep(TONE_SELECT, BEEP_MEDIUM);
                startShooterFromSettings();
            }
            break;

        case LV_KEY_ESC:
            // Save and exit to games menu
            saveShooterPrefs();
            saveCWSettings();
            onLVGLBackNavigation();
            lv_event_stop_processing(e);
            break;
    }
}

// Create settings screen for Morse Shooter
lv_obj_t* createMorseShooterSettingsScreen() {
    // Clean up any stale pointers from previous screens
    cleanupShooterScreenPointers();
    cleanupShooterSettingsPointers();

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
    lv_label_set_text_fmt(shooter_highscore_value, "%d", shooterHighScores[shooterDifficulty]);
    lv_obj_set_style_text_color(shooter_highscore_value, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(shooter_highscore_value, getThemeFonts()->font_title, 0);
    lv_obj_align(shooter_highscore_value, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Settings container - reduced height to avoid overlap with START button
    lv_obj_t* settings_card = lv_obj_create(screen);
    lv_obj_set_size(settings_card, SCREEN_WIDTH - 40, 150);
    lv_obj_set_pos(settings_card, 20, HEADER_HEIGHT + 10);
    lv_obj_set_layout(settings_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(settings_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(settings_card, 4, 0);
    lv_obj_set_style_pad_all(settings_card, 8, 0);
    applyCardStyle(settings_card);
    lv_obj_clear_flag(settings_card, LV_OBJ_FLAG_SCROLLABLE);

    // Helper to create a settings row
    auto createSettingsRow = [&](const char* label_text, lv_obj_t** row_out, lv_obj_t** value_out) {
        lv_obj_t* row = lv_obj_create(settings_card);
        lv_obj_set_size(row, SCREEN_WIDTH - 80, 28);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_color(row, LV_COLOR_BG_LAYER2, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, LV_COLOR_BORDER_SUBTLE, 0);
        lv_obj_set_style_pad_hor(row, 15, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, label_text);
        lv_obj_set_style_text_color(lbl, LV_COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(lbl, getThemeFonts()->font_body, 0);

        lv_obj_t* val = lv_label_create(row);
        lv_obj_set_style_text_color(val, LV_COLOR_ACCENT_CYAN, 0);
        lv_obj_set_style_text_font(val, getThemeFonts()->font_body, 0);

        *row_out = row;
        *value_out = val;
    };

    // Create settings rows
    createSettingsRow("Difficulty", &shooter_diff_row, &shooter_diff_value);
    createSettingsRow("Speed", &shooter_speed_row, &shooter_speed_value);
    createSettingsRow("Tone", &shooter_tone_row, &shooter_tone_value);
    createSettingsRow("Key Type", &shooter_key_row, &shooter_key_value);

    // Start button
    shooter_start_btn = lv_btn_create(screen);
    lv_obj_set_size(shooter_start_btn, 200, 50);
    lv_obj_set_pos(shooter_start_btn, (SCREEN_WIDTH - 200) / 2, SCREEN_HEIGHT - FOOTER_HEIGHT - 70);
    lv_obj_set_style_bg_color(shooter_start_btn, LV_COLOR_SUCCESS, 0);
    lv_obj_set_style_radius(shooter_start_btn, 8, 0);
    lv_obj_set_style_border_width(shooter_start_btn, 1, 0);
    lv_obj_set_style_border_color(shooter_start_btn, LV_COLOR_BORDER_SUBTLE, 0);

    lv_obj_t* btn_label = lv_label_create(shooter_start_btn);
    lv_label_set_text(btn_label, "START GAME");
    lv_obj_set_style_text_font(btn_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_center(btn_label);

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

    // Invisible focus container for keyboard input
    lv_obj_t* focus_container = lv_obj_create(screen);
    lv_obj_set_size(focus_container, 1, 1);
    lv_obj_set_pos(focus_container, -10, -10);
    lv_obj_set_style_bg_opa(focus_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus_container, 0, 0);
    lv_obj_set_style_outline_width(focus_container, 0, 0);
    lv_obj_set_style_outline_width(focus_container, 0, LV_STATE_FOCUSED);
    lv_obj_clear_flag(focus_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(focus_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus_container, shooter_settings_key_cb, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus_container);

    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }
    lv_group_focus_obj(focus_container);

    // Initialize values and focus
    shooter_settings_focus = 4;  // Start on START button
    shooter_settings_update_values();
    shooter_settings_update_focus();

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
// CW DOOM Game Screens
// ============================================

#include "../games/game_cw_doom.h"

// Screen pointers
static lv_obj_t* doom_canvas = NULL;
static lv_color_t* doom_canvas_buf = NULL;
static lv_obj_t* doom_health_label = NULL;
static lv_obj_t* doom_ammo_label = NULL;
static lv_obj_t* doom_score_label = NULL;
static lv_obj_t* doom_screen = NULL;
static lv_obj_t* doom_hint_label = NULL;

// Settings screen pointers
static lv_obj_t* doom_diff_value = NULL;
static lv_obj_t* doom_level_value = NULL;
static lv_obj_t* doom_highscore_value = NULL;

// Settings row pointers for focus styling
static lv_obj_t* doom_diff_row = NULL;
static lv_obj_t* doom_level_row = NULL;
static lv_obj_t* doom_start_btn = NULL;

// Settings state
static int doom_selected_difficulty = 0;
static int doom_selected_level = 1;
static int doom_settings_focus = 0;  // 0=diff, 1=level, 2=start

// Update focus styling on settings rows
static void doom_settings_update_focus() {
    lv_obj_t* rows[] = {doom_diff_row, doom_level_row, doom_start_btn};
    const int num_rows = 3;

    for (int i = 0; i < num_rows; i++) {
        if (rows[i] == NULL) continue;

        if (i == doom_settings_focus) {
            // Focused row - highlight border
            lv_obj_set_style_border_color(rows[i], LV_COLOR_ACCENT_CYAN, 0);
            lv_obj_set_style_border_width(rows[i], 2, 0);
        } else {
            // Not focused - subtle border
            lv_obj_set_style_border_color(rows[i], LV_COLOR_BORDER_SUBTLE, 0);
            lv_obj_set_style_border_width(rows[i], 1, 0);
        }
    }
}

static void cleanupDoomScreenPointers() {
    // Free the canvas buffer to prevent memory leak and crash on re-entry
    if (doom_canvas_buf != NULL) {
        free(doom_canvas_buf);
        doom_canvas_buf = NULL;
    }
    doom_canvas = NULL;
    doom_health_label = NULL;
    doom_ammo_label = NULL;
    doom_score_label = NULL;
    doom_screen = NULL;
    doom_hint_label = NULL;
}

static void cleanupDoomSettingsPointers() {
    doom_diff_value = NULL;
    doom_level_value = NULL;
    doom_highscore_value = NULL;
    doom_diff_row = NULL;
    doom_level_row = NULL;
    doom_start_btn = NULL;
}

// Render the raycasted view to a buffer, then blit scaled to canvas
static void renderDoomToCanvas() {
    if (!doom_canvas || !doom_canvas_buf || !doomActive) return;
    if (doomGame.state != DOOM_STATE_PLAYING) return;

    // Render at low resolution (120x80)
    static uint16_t render_buf[DOOM_RENDER_WIDTH * DOOM_RENDER_HEIGHT];

    // Clear to ceiling/floor colors
    for (int y = 0; y < DOOM_RENDER_HEIGHT / 2; y++) {
        for (int x = 0; x < DOOM_RENDER_WIDTH; x++) {
            render_buf[y * DOOM_RENDER_WIDTH + x] = 0x2104;  // Dark ceiling
        }
    }
    for (int y = DOOM_RENDER_HEIGHT / 2; y < DOOM_RENDER_HEIGHT; y++) {
        for (int x = 0; x < DOOM_RENDER_WIDTH; x++) {
            render_buf[y * DOOM_RENDER_WIDTH + x] = 0x4208;  // Dark floor
        }
    }

    // Cast rays and draw walls
    int halfHeight = DOOM_RENDER_HEIGHT / 2;
    for (int x = 0; x < DOOM_RENDER_WIDTH; x++) {
        int rayAngle = doomGame.player.angle - DOOM_HALF_FOV + (x * DOOM_FOV / DOOM_RENDER_WIDTH);
        DoomRayHit hit = castDoomRay(doomGame.player.x, doomGame.player.y, rayAngle);

        int wallHeight = doomGetWallHeight(hit.distance);
        int wallTop = halfHeight - wallHeight / 2;
        int wallBottom = halfHeight + wallHeight / 2;

        if (wallTop < 0) wallTop = 0;
        if (wallBottom >= DOOM_RENDER_HEIGHT) wallBottom = DOOM_RENDER_HEIGHT - 1;

        uint16_t wallColor = doomGetWallColor(hit.wallType, hit.isVertical);

        // Apply distance shading
        float distF = FP_TO_FLOAT(hit.distance);
        if (distF > 1.0f) {
            int shade = (int)(255.0f / distF);
            if (shade < 32) shade = 32;
            if (shade > 255) shade = 255;
            // Extract RGB565 and scale
            int r = ((wallColor >> 11) & 0x1F) * shade / 255;
            int g = ((wallColor >> 5) & 0x3F) * shade / 255;
            int b = (wallColor & 0x1F) * shade / 255;
            wallColor = (r << 11) | (g << 5) | b;
        }

        for (int y = wallTop; y <= wallBottom; y++) {
            render_buf[y * DOOM_RENDER_WIDTH + x] = wallColor;
        }
    }

    // Draw enemies as simple sprites
    for (int e = 0; e < doomGame.enemyCount; e++) {
        DoomEnemy& enemy = doomGame.enemies[e];
        if (!enemy.active) continue;

        // Calculate angle to enemy
        float dx = FP_TO_FLOAT(enemy.x - doomGame.player.x);
        float dy = FP_TO_FLOAT(enemy.y - doomGame.player.y);
        float dist = sqrt(dx * dx + dy * dy);
        if (dist < 0.5f) continue;

        float angleToEnemy = atan2(dy, dx) * 180.0f / PI;
        float relAngle = angleToEnemy - doomGame.player.angle;
        while (relAngle > 180) relAngle -= 360;
        while (relAngle < -180) relAngle += 360;

        if (abs(relAngle) > DOOM_HALF_FOV + 10) continue;

        int screenX = (int)((relAngle + DOOM_HALF_FOV) * DOOM_RENDER_WIDTH / DOOM_FOV);
        int spriteHeight = (int)(20.0f / dist);
        if (spriteHeight > DOOM_RENDER_HEIGHT) spriteHeight = DOOM_RENDER_HEIGHT;
        int spriteWidth = spriteHeight / 2;

        int spriteTop = halfHeight - spriteHeight / 2;
        int spriteLeft = screenX - spriteWidth / 2;

        uint16_t enemyColor = (enemy.hitTimer > 0) ? 0xFFFF : 0xF800;  // White flash or red

        for (int sy = 0; sy < spriteHeight; sy++) {
            for (int sx = 0; sx < spriteWidth; sx++) {
                int px = spriteLeft + sx;
                int py = spriteTop + sy;
                if (px >= 0 && px < DOOM_RENDER_WIDTH && py >= 0 && py < DOOM_RENDER_HEIGHT) {
                    render_buf[py * DOOM_RENDER_WIDTH + px] = enemyColor;
                }
            }
        }
    }

    // Draw crosshair
    int cx = DOOM_RENDER_WIDTH / 2;
    int cy = DOOM_RENDER_HEIGHT / 2;
    if (cy > 0 && cy < DOOM_RENDER_HEIGHT - 1) {
        render_buf[(cy - 1) * DOOM_RENDER_WIDTH + cx] = 0x07E0;  // Green
        render_buf[(cy + 1) * DOOM_RENDER_WIDTH + cx] = 0x07E0;
    }
    if (cx > 0 && cx < DOOM_RENDER_WIDTH - 1) {
        render_buf[cy * DOOM_RENDER_WIDTH + cx - 1] = 0x07E0;
        render_buf[cy * DOOM_RENDER_WIDTH + cx + 1] = 0x07E0;
    }

    // Scale 4x to canvas (120x80 -> 480x320)
    lv_color_t* dest = doom_canvas_buf;
    for (int y = 0; y < DOOM_RENDER_HEIGHT; y++) {
        for (int sy = 0; sy < DOOM_SCALE; sy++) {
            for (int x = 0; x < DOOM_RENDER_WIDTH; x++) {
                uint16_t color = render_buf[y * DOOM_RENDER_WIDTH + x];
                lv_color_t lvColor = lv_color_hex(
                    (((color >> 11) & 0x1F) << 3) << 16 |
                    (((color >> 5) & 0x3F) << 2) << 8 |
                    ((color & 0x1F) << 3)
                );
                for (int sx = 0; sx < DOOM_SCALE; sx++) {
                    *dest++ = lvColor;
                }
            }
        }
    }

    lv_obj_invalidate(doom_canvas);
}

static void updateDoomHUD() {
    if (doom_health_label) {
        lv_label_set_text_fmt(doom_health_label, "HP: %d", doomGame.player.health);
        lv_color_t hpColor = (doomGame.player.health > 50) ? LV_COLOR_SUCCESS :
                            (doomGame.player.health > 25) ? LV_COLOR_WARNING : LV_COLOR_ERROR;
        lv_obj_set_style_text_color(doom_health_label, hpColor, 0);
    }
    if (doom_ammo_label) {
        // Show enemies remaining as the main objective counter
        int remaining = doomEnemiesRemaining();
        if (remaining > 0) {
            lv_label_set_text_fmt(doom_ammo_label, "Enemies: %d", remaining);
            lv_obj_set_style_text_color(doom_ammo_label, LV_COLOR_ERROR, 0);
        } else {
            lv_label_set_text(doom_ammo_label, "EXIT OPEN!");
            lv_obj_set_style_text_color(doom_ammo_label, LV_COLOR_SUCCESS, 0);
        }
    }
    if (doom_score_label) {
        lv_label_set_text_fmt(doom_score_label, "Score: %d", doomGame.player.score);
    }

    // Update hint label with target character when enemy is in view
    if (doom_hint_label) {
        if (doomGame.enemyInView && doomGame.targetChar != 0) {
            // Show big target character - key this to shoot!
            lv_label_set_text_fmt(doom_hint_label, "TYPE: %c  to SHOOT!", doomGame.targetChar);
            lv_obj_set_style_text_color(doom_hint_label, LV_COLOR_ERROR, 0);
            lv_obj_set_style_text_font(doom_hint_label, getThemeFonts()->font_title, 0);
        } else {
            // Show movement controls with clearer objective
            int remaining = doomEnemiesRemaining();
            if (remaining > 0) {
                lv_label_set_text(doom_hint_label, "Find enemies! Dit=Left  Dah=Right  Both=Forward");
            } else {
                lv_label_set_text(doom_hint_label, "All enemies killed! Find the EXIT (green)");
            }
            lv_obj_set_style_text_color(doom_hint_label, LV_COLOR_WARNING, 0);
            lv_obj_set_style_text_font(doom_hint_label, getThemeFonts()->font_small, 0);
        }
    }
}

// Called from main loop to update display
void updateCWDoomDisplay() {
    if (!doomActive || !doom_screen) return;

    if (doomGame.state == DOOM_STATE_PLAYING) {
        if (doomGame.needsRender) {
            renderDoomToCanvas();
            updateDoomHUD();
            doomGame.needsRender = false;
        }
    } else if (doomGame.state == DOOM_STATE_GAME_OVER) {
        saveDoomHighScore();
        if (doom_hint_label) {
            lv_label_set_text(doom_hint_label, "GAME OVER - Press ESC");
        }
    } else if (doomGame.state == DOOM_STATE_VICTORY) {
        saveDoomHighScore();
        if (doom_hint_label) {
            lv_label_set_text_fmt(doom_hint_label, "LEVEL %d COMPLETE! Score: %d",
                                  doomGame.currentLevel, doomGame.player.score);
        }
    }
}

// Key handler for game screen
static void doom_game_key_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ESC) {
        stopDoomGame();
        cleanupDoomScreenPointers();
        onLVGLBackNavigation();
    }
}

// Forward declaration
lv_obj_t* createCWDoomScreen();

// Settings screen value update
static void doom_settings_update_values() {
    const char* diffNames[] = {"Easy", "Medium", "Hard"};

    if (doom_diff_value) {
        lv_label_set_text(doom_diff_value, diffNames[doom_selected_difficulty]);
    }
    if (doom_level_value) {
        lv_label_set_text_fmt(doom_level_value, "Level %d", doom_selected_level);
    }
    if (doom_highscore_value) {
        lv_label_set_text_fmt(doom_highscore_value, "%d", doomGame.highScores[doom_selected_difficulty]);
    }
}

// Settings screen key handler
// Focus indices: 0=difficulty, 1=level, 2=start button
static void doom_settings_key_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_ESC) {
        cleanupDoomSettingsPointers();
        onLVGLBackNavigation();
        return;
    }

    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        doom_settings_focus--;
        if (doom_settings_focus < 0) doom_settings_focus = 2;
        doom_settings_update_focus();
        beep(TONE_MENU_NAV, BEEP_SHORT);
    } else if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        doom_settings_focus++;
        if (doom_settings_focus > 2) doom_settings_focus = 0;
        doom_settings_update_focus();
        beep(TONE_MENU_NAV, BEEP_SHORT);
    } else if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        int dir = (key == LV_KEY_RIGHT) ? 1 : -1;
        switch (doom_settings_focus) {
            case 0:  // Difficulty
                doom_selected_difficulty += dir;
                if (doom_selected_difficulty < 0) doom_selected_difficulty = 2;
                if (doom_selected_difficulty > 2) doom_selected_difficulty = 0;
                break;
            case 1:  // Level
                doom_selected_level += dir;
                if (doom_selected_level < 1) doom_selected_level = 3;
                if (doom_selected_level > 3) doom_selected_level = 1;
                break;
        }
        doom_settings_update_values();
        beep(TONE_MENU_NAV, BEEP_SHORT);
    } else if (key == LV_KEY_ENTER && doom_settings_focus == 2) {
        beep(TONE_SELECT, BEEP_LONG);
        cleanupDoomSettingsPointers();
        extern void setCurrentModeFromInt(int mode);
        setCurrentModeFromInt(139);  // LVGL_MODE_CW_DOOM
        initDoomGame(doom_selected_level, (DoomDifficulty)doom_selected_difficulty);
        initDoomKeyer();
        clearNavigationGroup();
        lv_obj_t* game_screen = createCWDoomScreen();
        loadScreen(game_screen, SCREEN_ANIM_FADE);
    }
}

lv_obj_t* createCWDoomSettingsScreen() {
    cleanupDoomSettingsPointers();
    loadDoomHighScores();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "CW DOOM");
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

    doom_highscore_value = lv_label_create(hs_container);
    lv_label_set_text_fmt(doom_highscore_value, "%d", doomGame.highScores[doom_selected_difficulty]);
    lv_obj_set_style_text_color(doom_highscore_value, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(doom_highscore_value, getThemeFonts()->font_title, 0);
    lv_obj_align(doom_highscore_value, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Settings container
    lv_obj_t* settings_card = lv_obj_create(screen);
    lv_obj_set_size(settings_card, SCREEN_WIDTH - 40, 150);
    lv_obj_set_pos(settings_card, 20, HEADER_HEIGHT + 10);
    lv_obj_set_layout(settings_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(settings_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(settings_card, 4, 0);
    lv_obj_set_style_pad_all(settings_card, 8, 0);
    applyCardStyle(settings_card);
    lv_obj_clear_flag(settings_card, LV_OBJ_FLAG_SCROLLABLE);

    // Helper to create a settings row
    auto createSettingsRow = [&](const char* label_text, lv_obj_t** row_out, lv_obj_t** value_out) {
        lv_obj_t* row = lv_obj_create(settings_card);
        lv_obj_set_size(row, SCREEN_WIDTH - 80, 28);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_color(row, LV_COLOR_BG_LAYER2, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, LV_COLOR_BORDER_SUBTLE, 0);
        lv_obj_set_style_pad_hor(row, 15, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, label_text);
        lv_obj_set_style_text_color(lbl, LV_COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(lbl, getThemeFonts()->font_body, 0);

        lv_obj_t* val = lv_label_create(row);
        lv_obj_set_style_text_color(val, LV_COLOR_ACCENT_CYAN, 0);
        lv_obj_set_style_text_font(val, getThemeFonts()->font_body, 0);

        *row_out = row;
        *value_out = val;
    };

    // Create settings rows (removed Controls - now always type-to-shoot)
    createSettingsRow("Difficulty", &doom_diff_row, &doom_diff_value);
    createSettingsRow("Level", &doom_level_row, &doom_level_value);

    // Start button - positioned absolutely like Morse Shooter
    doom_start_btn = lv_btn_create(screen);
    lv_obj_set_size(doom_start_btn, 200, 50);
    lv_obj_set_pos(doom_start_btn, (SCREEN_WIDTH - 200) / 2, SCREEN_HEIGHT - FOOTER_HEIGHT - 70);
    lv_obj_set_style_bg_color(doom_start_btn, LV_COLOR_SUCCESS, 0);
    lv_obj_set_style_radius(doom_start_btn, 8, 0);
    lv_obj_set_style_border_width(doom_start_btn, 1, 0);
    lv_obj_set_style_border_color(doom_start_btn, LV_COLOR_BORDER_SUBTLE, 0);

    lv_obj_t* btn_label = lv_label_create(doom_start_btn);
    lv_label_set_text(btn_label, "START GAME");
    lv_obj_set_style_text_font(btn_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_center(btn_label);

    // Footer with help text
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

    // Update values and initial focus
    doom_settings_focus = 0;  // Start on difficulty
    doom_settings_update_values();
    doom_settings_update_focus();

    // Invisible focus container for keyboard input
    lv_obj_t* focus_obj = lv_obj_create(screen);
    lv_obj_set_size(focus_obj, 1, 1);
    lv_obj_set_pos(focus_obj, -10, -10);
    lv_obj_set_style_bg_opa(focus_obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus_obj, 0, 0);
    lv_obj_set_style_outline_width(focus_obj, 0, 0);
    lv_obj_set_style_outline_width(focus_obj, 0, LV_STATE_FOCUSED);
    lv_obj_add_flag(focus_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(focus_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(focus_obj, doom_settings_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus_obj);

    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }
    lv_group_focus_obj(focus_obj);

    return screen;
}

lv_obj_t* createCWDoomScreen() {
    cleanupDoomScreenPointers();

    doom_screen = createScreen();
    applyScreenStyle(doom_screen);

    // Allocate canvas buffer in PSRAM (480x320 RGB565)
    if (doom_canvas_buf == NULL) {
        size_t buf_size = DOOM_SCREEN_WIDTH * DOOM_SCREEN_HEIGHT * sizeof(lv_color_t);
        if (psramFound()) {
            doom_canvas_buf = (lv_color_t*)ps_malloc(buf_size);
        } else {
            doom_canvas_buf = (lv_color_t*)malloc(buf_size);
        }
    }

    // Create canvas for game rendering - full screen
    doom_canvas = lv_canvas_create(doom_screen);
    lv_obj_set_pos(doom_canvas, 0, 0);

    if (doom_canvas_buf != NULL) {
        lv_canvas_set_buffer(doom_canvas, doom_canvas_buf, DOOM_SCREEN_WIDTH, DOOM_SCREEN_HEIGHT, LV_IMG_CF_TRUE_COLOR);
        // Clear to black initially
        memset(doom_canvas_buf, 0, DOOM_SCREEN_WIDTH * DOOM_SCREEN_HEIGHT * sizeof(lv_color_t));
    }

    // HUD overlay at bottom
    lv_obj_t* hud_bar = lv_obj_create(doom_screen);
    lv_obj_set_size(hud_bar, SCREEN_WIDTH, 40);
    lv_obj_set_pos(hud_bar, 0, SCREEN_HEIGHT - 40);
    lv_obj_set_style_bg_color(hud_bar, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(hud_bar, LV_OPA_80, 0);
    lv_obj_set_style_border_width(hud_bar, 0, 0);
    lv_obj_clear_flag(hud_bar, LV_OBJ_FLAG_SCROLLABLE);

    doom_health_label = lv_label_create(hud_bar);
    lv_label_set_text(doom_health_label, "HP: 100");
    lv_obj_set_style_text_font(doom_health_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(doom_health_label, LV_COLOR_SUCCESS, 0);
    lv_obj_align(doom_health_label, LV_ALIGN_LEFT_MID, 10, 0);

    doom_ammo_label = lv_label_create(hud_bar);
    lv_label_set_text(doom_ammo_label, "Ammo: 50");
    lv_obj_set_style_text_font(doom_ammo_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(doom_ammo_label, LV_COLOR_WARNING, 0);
    lv_obj_align(doom_ammo_label, LV_ALIGN_CENTER, 0, 0);

    doom_score_label = lv_label_create(hud_bar);
    lv_label_set_text(doom_score_label, "Score: 0");
    lv_obj_set_style_text_font(doom_score_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(doom_score_label, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(doom_score_label, LV_ALIGN_RIGHT_MID, -10, 0);

    // Hint label at top - shows target character when enemy in view
    // Or movement controls when no target
    doom_hint_label = lv_label_create(doom_screen);
    lv_label_set_text(doom_hint_label, "Dit=L  Dah=R  Squeeze=Fwd  TapDah=Door");
    lv_obj_set_style_text_font(doom_hint_label, getThemeFonts()->font_subtitle, 0);
    lv_obj_set_style_text_color(doom_hint_label, LV_COLOR_WARNING, 0);
    lv_obj_align(doom_hint_label, LV_ALIGN_TOP_MID, 0, 5);

    // Focus container for key handling
    lv_obj_t* focus_obj = lv_obj_create(doom_screen);
    lv_obj_set_size(focus_obj, 1, 1);
    lv_obj_set_pos(focus_obj, -10, -10);
    lv_obj_set_style_bg_opa(focus_obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus_obj, 0, 0);
    lv_obj_add_flag(focus_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(focus_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(focus_obj, doom_game_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus_obj);
    lv_group_focus_obj(focus_obj);

    // Activate game
    doomActive = true;
    doomGame.needsRender = true;

    return doom_screen;
}

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
        case 16: // MODE_MORSE_SHOOTER
            // Show settings screen first, game starts when user presses START
            return createMorseShooterSettingsScreen();
        case 17: // MODE_MORSE_MEMORY
            return createMemoryChainScreen();

        // Spark Watch game modes (78-88)
        case 78: // LVGL_MODE_SPARK_WATCH
        case 79: // LVGL_MODE_SPARK_WATCH_DIFFICULTY
        case 80: // LVGL_MODE_SPARK_WATCH_CAMPAIGN
        case 81: // LVGL_MODE_SPARK_WATCH_MISSION
        case 82: // LVGL_MODE_SPARK_WATCH_CHALLENGE
        case 83: // LVGL_MODE_SPARK_WATCH_BRIEFING
        case 84: // LVGL_MODE_SPARK_WATCH_GAMEPLAY
        case 85: // LVGL_MODE_SPARK_WATCH_RESULTS
        case 86: // LVGL_MODE_SPARK_WATCH_DEBRIEFING
        case 87: // LVGL_MODE_SPARK_WATCH_SETTINGS
        case 88: // LVGL_MODE_SPARK_WATCH_STATS
            return createSparkWatchScreenForMode(mode);

        // CW Speeder game modes (134-135)
        case 134: // LVGL_MODE_CW_SPEEDER_SELECT
            return createCWSpeedSelectScreen();
        case 135: // LVGL_MODE_CW_SPEEDER
            return createCWSpeedGameScreen();

        // CW DOOM game modes (138-139)
        case 138: // LVGL_MODE_CW_DOOM_SETTINGS
            return createCWDoomSettingsScreen();
        case 139: // LVGL_MODE_CW_DOOM
            return createCWDoomScreen();

        default:
            Serial.printf("[GameScreens] Unknown game mode: %d\n", mode);
            return NULL;
    }
}

#endif // LV_GAME_SCREENS_H
