/*
 * VAIL SUMMIT - LVGL Screen Manager
 * Handles screen transitions, input groups, and global ESC navigation
 */

#ifndef LV_SCREEN_MANAGER_H
#define LV_SCREEN_MANAGER_H

#include <lvgl.h>
#include "lv_init.h"
#include "lv_theme_manager.h"

// Current active screen
static lv_obj_t* current_screen = NULL;

// Callback type for back button action
typedef void (*BackActionCallback)(void);
static BackActionCallback global_back_callback = NULL;

// ============================================
// Screen Transition Animations
// ============================================

typedef enum {
    SCREEN_ANIM_NONE = 0,
    SCREEN_ANIM_FADE,
    SCREEN_ANIM_SLIDE_LEFT,
    SCREEN_ANIM_SLIDE_RIGHT,
    SCREEN_ANIM_SLIDE_UP,
    SCREEN_ANIM_SLIDE_DOWN
} ScreenAnimType;

// Default transition duration in ms
#define DEFAULT_TRANSITION_MS 150

// ============================================
// Global ESC Handler
// ============================================

/*
 * Global ESC key event handler
 * Attached to all navigable widgets
 */
static void global_esc_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY) {
        // Check if event was already handled by a screen-specific handler
        // In LVGL v8.3, check the stop_bubbling flag directly
        if (e->stop_bubbling) {
            return;
        }

        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ESC) {
            Serial.println("[ScreenManager] ESC pressed");

            // Call the global back callback if set
            if (global_back_callback != NULL) {
                global_back_callback();
            }
        }
    }
}

/*
 * Set the global back navigation callback
 */
void setBackCallback(BackActionCallback callback) {
    global_back_callback = callback;
}

// ============================================
// Input Group Management
// ============================================

/*
 * Add a widget to the navigation group with ESC handler
 * This is the primary way to make widgets navigable
 */
void addNavigableWidget(lv_obj_t* widget) {
    lv_group_t* group = getLVGLInputGroup();
    if (group == NULL) {
        Serial.println("[ScreenManager] WARNING: No input group available!");
        return;
    }

    // Add to input group for keyboard navigation
    lv_group_add_obj(group, widget);

    // Add ESC handler for back navigation
    lv_obj_add_event_cb(widget, global_esc_handler, LV_EVENT_KEY, NULL);

    Serial.printf("[ScreenManager] Added widget to nav group, now has %d objects\n",
                  lv_group_get_obj_count(group));
}

/*
 * Remove a widget from the navigation group
 */
void removeNavigableWidget(lv_obj_t* widget) {
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_remove_obj(widget);
    }
}

/*
 * Clear all widgets from the navigation group
 */
void clearNavigationGroup() {
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, false);  // Reset editing mode to prevent arrow key bugs
        lv_group_remove_all_objs(group);
    }
}

/*
 * Focus a specific widget
 */
void focusWidget(lv_obj_t* widget) {
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_focus_obj(widget);
    }
}

// ============================================
// Grid Navigation Handler (unified)
// ============================================

/*
 * Navigation context for grid/list navigation.
 * Passed as user_data to grid_nav_handler via lv_obj_add_event_cb().
 */
struct NavGridContext {
    lv_obj_t** buttons;     // Pointer to button array
    int* button_count;      // Pointer to button count
    int columns;            // Number of columns (1 = linear list)
};

/*
 * Unified 2D grid navigation handler
 * Supports any column count via NavGridContext passed as user_data.
 * - Blocks TAB (LV_KEY_NEXT) from navigating
 * - UP/DOWN: Move to same column in previous/next row
 * - LEFT/RIGHT: Move within the row (blocked if columns == 1)
 * - At any edge: does nothing (no wrap)
 * - Scrolls focused item into view
 *
 * Usage:
 *   static NavGridContext ctx = { button_array, &button_count, 2 };
 *   lv_obj_add_event_cb(btn, grid_nav_handler, LV_EVENT_KEY, &ctx);
 *   addNavigableWidget(btn);
 */
static void grid_nav_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    // Block TAB key (LV_KEY_NEXT = '\t' = 9) from navigating
    if (key == '\t' || key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }

    // Only handle arrow keys
    if (key != LV_KEY_LEFT && key != LV_KEY_RIGHT &&
        key != LV_KEY_PREV &&
        key != LV_KEY_UP && key != LV_KEY_DOWN) return;

    // Always stop propagation to prevent LVGL's default linear navigation
    lv_event_stop_processing(e);

    NavGridContext* ctx = (NavGridContext*)lv_event_get_user_data(e);
    if (!ctx || !ctx->buttons || !ctx->button_count) return;

    int count = *(ctx->button_count);
    int cols = ctx->columns;
    if (count <= 1) return;

    // Block LEFT/RIGHT for single-column lists
    if (cols <= 1 && (key == LV_KEY_LEFT || key == LV_KEY_RIGHT)) {
        return;
    }

    // Get current focused object
    lv_obj_t* focused = lv_event_get_target(e);
    if (!focused) return;

    // Find index of focused object in the button array
    int focused_idx = -1;
    for (int i = 0; i < count; i++) {
        if (ctx->buttons[i] == focused) {
            focused_idx = i;
            break;
        }
    }
    if (focused_idx < 0) return;

    // Calculate grid position
    int row = focused_idx / cols;
    int col = focused_idx % cols;
    int total_rows = (count + cols - 1) / cols;

    int target_idx = -1;

    if (key == LV_KEY_RIGHT) {
        if (col < cols - 1) {
            int potential = focused_idx + 1;
            if (potential < count) {
                target_idx = potential;
            }
        }
    } else if (key == LV_KEY_LEFT) {
        if (col > 0) {
            target_idx = focused_idx - 1;
        }
    } else if (key == LV_KEY_DOWN) {
        if (row < total_rows - 1) {
            int potential = focused_idx + cols;
            if (potential < count) {
                target_idx = potential;
            } else {
                target_idx = count - 1;
            }
        }
    } else if (key == LV_KEY_PREV || key == LV_KEY_UP) {
        if (row > 0) {
            target_idx = focused_idx - cols;
        }
    }

    // Focus target if valid and scroll into view
    if (target_idx >= 0 && target_idx < count) {
        lv_obj_t* target = ctx->buttons[target_idx];
        if (target) {
            lv_group_focus_obj(target);
            lv_obj_scroll_to_view(target, LV_ANIM_ON);
        }
    }
}

// ============================================
// Linear Navigation Handler
// ============================================

/*
 * Linear navigation handler for vertical button lists
 * - Blocks TAB to prevent unwanted focus cycling
 * - Blocks LEFT/RIGHT (not meaningful for vertical lists)
 * - Explicitly handles UP/DOWN navigation and scrolls into view
 *
 * Usage: Add to buttons before addNavigableWidget():
 *   lv_obj_add_event_cb(btn, linear_nav_handler, LV_EVENT_KEY, NULL);
 *   addNavigableWidget(btn);
 */
static void linear_nav_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);
    lv_group_t* group = getLVGLInputGroup();

    // Block TAB key - only arrow keys should navigate
    if (key == '\t' || key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }

    // Block LEFT/RIGHT for pure vertical lists
    if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        lv_event_stop_processing(e);
        return;
    }

    // Explicitly handle UP - move to previous item
    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        if (group) {
            lv_group_focus_prev(group);
            lv_obj_t* focused = lv_group_get_focused(group);
            if (focused) {
                lv_obj_scroll_to_view(focused, LV_ANIM_ON);
            }
        }
        lv_event_stop_processing(e);
        return;
    }

    // Explicitly handle DOWN - move to next item
    if (key == LV_KEY_DOWN) {
        if (group) {
            lv_group_focus_next(group);
            lv_obj_t* focused = lv_group_get_focused(group);
            if (focused) {
                lv_obj_scroll_to_view(focused, LV_ANIM_ON);
            }
        }
        lv_event_stop_processing(e);
        return;
    }
}

// ============================================
// Screen Management
// ============================================

/*
 * Create a new screen with default styling
 */
lv_obj_t* createScreen() {
    lv_obj_t* screen = lv_obj_create(NULL);

    // Apply default background color from active theme
    lv_obj_set_style_bg_color(screen, getThemeColors()->bg_deep, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    return screen;
}

/*
 * Load a screen directly (replacing current)
 * NOTE: Do NOT clear the navigation group here - widgets are added during
 * screen creation and would be lost. The group is managed by screen creators.
 */
void loadScreen(lv_obj_t* new_screen, ScreenAnimType anim = SCREEN_ANIM_FADE) {
    // Determine animation type
    lv_scr_load_anim_t lv_anim = LV_SCR_LOAD_ANIM_FADE_ON;
    switch (anim) {
        case SCREEN_ANIM_NONE:
            lv_anim = LV_SCR_LOAD_ANIM_NONE;
            break;
        case SCREEN_ANIM_FADE:
            lv_anim = LV_SCR_LOAD_ANIM_FADE_ON;
            break;
        case SCREEN_ANIM_SLIDE_LEFT:
            lv_anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;
            break;
        case SCREEN_ANIM_SLIDE_RIGHT:
            lv_anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
            break;
        case SCREEN_ANIM_SLIDE_UP:
            lv_anim = LV_SCR_LOAD_ANIM_MOVE_TOP;
            break;
        case SCREEN_ANIM_SLIDE_DOWN:
            lv_anim = LV_SCR_LOAD_ANIM_MOVE_BOTTOM;
            break;
    }

    // Delete old screen after transition
    lv_scr_load_anim(new_screen, lv_anim, DEFAULT_TRANSITION_MS, 0, true);
    current_screen = new_screen;

    lv_group_t* group = getLVGLInputGroup();
    Serial.printf("[ScreenManager] Loaded screen (direct), nav group has %d objects\n",
                  group ? lv_group_get_obj_count(group) : -1);
}

/*
 * Get the current active screen
 */
lv_obj_t* getCurrentScreen() {
    return current_screen;
}

// ============================================
// Modal Dialogs
// ============================================

/*
 * Show a modal message box
 */
lv_obj_t* showMessageBox(const char* title, const char* message, const char* buttons[], int num_buttons) {
    lv_obj_t* mbox = lv_msgbox_create(NULL, title, message, buttons, false);
    lv_obj_center(mbox);

    // Add the buttons to navigation group
    lv_obj_t* btns = lv_msgbox_get_btns(mbox);
    if (btns != NULL) {
        addNavigableWidget(btns);
    }

    return mbox;
}

/*
 * Close a message box
 */
void closeMessageBox(lv_obj_t* mbox) {
    lv_msgbox_close(mbox);
}

// ============================================
// Utility Functions
// ============================================

/*
 * Add all children of a container to the navigation group
 */
void addContainerChildrenToNav(lv_obj_t* container) {
    uint32_t count = lv_obj_get_child_cnt(container);
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t* child = lv_obj_get_child(container, i);
        // Only add focusable widgets
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_CLICKABLE)) {
            addNavigableWidget(child);
        }
    }
}

/*
 * Refresh screen (force redraw)
 */
void refreshScreen() {
    lv_obj_invalidate(lv_scr_act());
}

#endif // LV_SCREEN_MANAGER_H
