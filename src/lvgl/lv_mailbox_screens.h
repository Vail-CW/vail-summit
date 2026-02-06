/*
 * VAIL SUMMIT - Morse Mailbox LVGL Screens
 * Device linking, inbox, playback, and compose screens
 */

#ifndef LV_MAILBOX_SCREENS_H
#define LV_MAILBOX_SCREENS_H

#include <lvgl.h>
#include "lv_theme_summit.h"
#include "lv_widgets_summit.h"
#include "lv_screen_manager.h"
#include "../core/config.h"
#include "../core/modes.h"
#include "../network/morse_mailbox.h"
#include "../network/internet_check.h"

// Forward declarations for mode switching
extern void onLVGLMenuSelect(int target_mode);  // Proper navigation with screen loading

// ============================================
// Screen State
// ============================================

// Link screen state
static lv_obj_t* mailbox_link_screen = NULL;
static lv_obj_t* mailbox_code_label = NULL;
static lv_obj_t* mailbox_url_label = NULL;
static lv_obj_t* mailbox_status_label = NULL;
static lv_obj_t* mailbox_timer_label = NULL;
static lv_timer_t* mailbox_link_timer = NULL;

// Inbox screen state
static lv_obj_t* mailbox_inbox_screen = NULL;
static lv_obj_t* mailbox_inbox_list = NULL;
static int mailbox_inbox_selection = 0;

// Playback screen state
static lv_obj_t* mailbox_playback_screen = NULL;
static lv_obj_t* mailbox_play_btn = NULL;
static lv_obj_t* mailbox_speed_label = NULL;
static float mailbox_playback_speed = 1.0f;
static String currentPlaybackMessageId = "";
static String mailbox_reply_recipient = "";  // For pre-filling compose screen when replying

// Playback timing state
static int mailbox_playback_event_index = 0;
static unsigned long mailbox_playback_start_time = 0;
static bool mailbox_is_playing = false;
static lv_timer_t* mailbox_playback_timer = NULL;  // Track timer for cleanup

// Account screen state
static lv_obj_t* mailbox_account_screen = NULL;

// Inbox screen navigation tracking
static lv_obj_t* mailbox_inbox_header_btns[2] = {NULL};  // Compose, Account
static int mailbox_inbox_header_btn_count = 0;
static lv_obj_t* mailbox_inbox_message_items[MAILBOX_INBOX_CACHE_SIZE] = {NULL};
static int mailbox_inbox_message_count = 0;
static String mailbox_inbox_msgIds[MAILBOX_INBOX_CACHE_SIZE];  // Message IDs for click handler user_data

// Playback screen button tracking
static lv_obj_t* mailbox_playback_btns[3] = {NULL};  // play_btn, speed_btn, reply_btn
static int mailbox_playback_btn_count = 0;

// Compose screen button tracking
static lv_obj_t* mailbox_compose_focusable[3] = {NULL};  // recipient, record, send
static int mailbox_compose_focusable_count = 0;

// Forward declaration for playback control
static void mailbox_stop_playback();

// Loading overlay state
static lv_obj_t* mailbox_loading_overlay = NULL;
static lv_obj_t* mailbox_loading_label = NULL;
static lv_obj_t* mailbox_loading_spinner = NULL;

/*
 * Show/hide loading overlay on current screen
 */
static void showMailboxLoading(lv_obj_t* screen, const char* message) {
    if (!screen || !lv_obj_is_valid(screen)) return;

    // Create overlay if needed
    if (!mailbox_loading_overlay || !lv_obj_is_valid(mailbox_loading_overlay)) {
        mailbox_loading_overlay = lv_obj_create(screen);
        lv_obj_set_size(mailbox_loading_overlay, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(mailbox_loading_overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(mailbox_loading_overlay, LV_OPA_70, 0);
        lv_obj_set_style_border_width(mailbox_loading_overlay, 0, 0);
        lv_obj_center(mailbox_loading_overlay);
        lv_obj_clear_flag(mailbox_loading_overlay, LV_OBJ_FLAG_SCROLLABLE);

        // Spinner
        mailbox_loading_spinner = lv_spinner_create(mailbox_loading_overlay, 1000, 60);
        lv_obj_set_size(mailbox_loading_spinner, 50, 50);
        lv_obj_align(mailbox_loading_spinner, LV_ALIGN_CENTER, 0, -20);

        // Label
        mailbox_loading_label = lv_label_create(mailbox_loading_overlay);
        lv_obj_set_style_text_color(mailbox_loading_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(mailbox_loading_label, getThemeFonts()->font_body, 0);
        lv_obj_align(mailbox_loading_label, LV_ALIGN_CENTER, 0, 30);
    }

    lv_label_set_text(mailbox_loading_label, message);
    lv_obj_clear_flag(mailbox_loading_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(mailbox_loading_overlay);

    // Force screen refresh to show loading immediately
    lv_refr_now(NULL);
}

static void hideMailboxLoading() {
    if (mailbox_loading_overlay && lv_obj_is_valid(mailbox_loading_overlay)) {
        lv_obj_add_flag(mailbox_loading_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

/*
 * Clean up mailbox playback state when leaving screen
 */
static void cleanupMailboxPlayback() {
    Serial.println("[Mailbox] Cleaning up playback state");

    // Stop playback and tone
    mailbox_is_playing = false;
    extern void requestStopTone();
    requestStopTone();

    // Delete timer
    if (mailbox_playback_timer) {
        lv_timer_del(mailbox_playback_timer);
        mailbox_playback_timer = NULL;
    }

    // Clear screen and button pointers
    mailbox_playback_screen = NULL;
    mailbox_play_btn = NULL;
    mailbox_speed_label = NULL;
    memset(mailbox_playback_btns, 0, sizeof(mailbox_playback_btns));
    mailbox_playback_btn_count = 0;

    // Clear loading overlay pointer (will be recreated on next screen)
    mailbox_loading_overlay = NULL;
    mailbox_loading_label = NULL;
    mailbox_loading_spinner = NULL;
}

// ============================================
// Linear Navigation Handler (for vertical lists)
// ============================================

static void mailbox_linear_nav_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    // Block TAB and horizontal navigation in vertical lists
    if (key == '\t' || key == LV_KEY_NEXT || key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        lv_event_stop_processing(e);
        return;
    }

    // Auto-scroll to focused item on vertical navigation
    if (key == LV_KEY_UP || key == LV_KEY_DOWN || key == LV_KEY_PREV) {
        lv_obj_t* target = lv_event_get_target(e);
        if (target) {
            lv_obj_scroll_to_view(target, LV_ANIM_ON);
        }
    }
}

/*
 * Header row navigation for horizontal buttons
 * LEFT/RIGHT between buttons, DOWN to message list
 */
static void mailbox_header_nav_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    if (key == '\t' || key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }

    lv_obj_t* target = lv_event_get_target(e);
    int current_idx = -1;
    for (int i = 0; i < mailbox_inbox_header_btn_count; i++) {
        if (mailbox_inbox_header_btns[i] == target) { current_idx = i; break; }
    }
    if (current_idx < 0) return;

    if (key == LV_KEY_LEFT && current_idx > 0) {
        lv_group_focus_obj(mailbox_inbox_header_btns[current_idx - 1]);
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_RIGHT && current_idx < mailbox_inbox_header_btn_count - 1) {
        lv_group_focus_obj(mailbox_inbox_header_btns[current_idx + 1]);
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_DOWN) {
        if (mailbox_inbox_message_count > 0 && mailbox_inbox_message_items[0]) {
            lv_group_focus_obj(mailbox_inbox_message_items[0]);
            lv_obj_scroll_to_view(mailbox_inbox_message_items[0], LV_ANIM_ON);
        }
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_UP || key == LV_KEY_PREV || key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        lv_event_stop_processing(e);  // Block at boundaries
    }
}

/*
 * Message list navigation - UP from first item goes to header
 * Also handles ENTER to trigger click (LVGL default doesn't always work)
 */
static void mailbox_list_nav_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    if (key == '\t' || key == LV_KEY_NEXT || key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        lv_event_stop_processing(e);
        return;
    }

    lv_obj_t* target = lv_event_get_target(e);

    // Handle ENTER key - manually send clicked event
    if (key == LV_KEY_ENTER) {
        Serial.println("[Mailbox] ENTER key detected, sending CLICKED event");
        lv_event_send(target, LV_EVENT_CLICKED, NULL);
        lv_event_stop_processing(e);
        return;
    }

    int current_idx = -1;
    for (int i = 0; i < mailbox_inbox_message_count; i++) {
        if (mailbox_inbox_message_items[i] == target) { current_idx = i; break; }
    }
    if (current_idx < 0) return;

    if ((key == LV_KEY_UP || key == LV_KEY_PREV) && current_idx == 0) {
        // At first message, go to last header button
        if (mailbox_inbox_header_btn_count > 0) {
            lv_group_focus_obj(mailbox_inbox_header_btns[mailbox_inbox_header_btn_count - 1]);
        }
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        lv_group_focus_obj(mailbox_inbox_message_items[current_idx - 1]);
        lv_obj_scroll_to_view(mailbox_inbox_message_items[current_idx - 1], LV_ANIM_ON);
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_DOWN && current_idx < mailbox_inbox_message_count - 1) {
        lv_group_focus_obj(mailbox_inbox_message_items[current_idx + 1]);
        lv_obj_scroll_to_view(mailbox_inbox_message_items[current_idx + 1], LV_ANIM_ON);
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_DOWN) {
        lv_event_stop_processing(e);  // At bottom
    }
}

// Inbox key handler for refresh
static void mailbox_inbox_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    if (key == 'r' || key == 'R') {
        invalidateMailboxInboxCache();
        onLVGLMenuSelect(MODE_MORSE_MAILBOX_INBOX);
        lv_event_stop_processing(e);
    }
}

/*
 * Compose screen - vertical between input/buttons, horizontal between buttons
 */
static void mailbox_compose_nav_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    if (key == '\t' || key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }

    lv_obj_t* target = lv_event_get_target(e);
    int current_idx = -1;
    for (int i = 0; i < mailbox_compose_focusable_count; i++) {
        if (mailbox_compose_focusable[i] == target) { current_idx = i; break; }
    }
    if (current_idx < 0) return;

    // idx 0 = input, 1 = record btn, 2 = send btn
    if (key == LV_KEY_DOWN && current_idx == 0) {
        lv_group_focus_obj(mailbox_compose_focusable[1]);
        lv_event_stop_processing(e);
    } else if ((key == LV_KEY_UP || key == LV_KEY_PREV) && current_idx > 0) {
        lv_group_focus_obj(mailbox_compose_focusable[0]);
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_LEFT && current_idx == 2) {
        lv_group_focus_obj(mailbox_compose_focusable[1]);
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_RIGHT && current_idx == 1) {
        lv_group_focus_obj(mailbox_compose_focusable[2]);
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_UP || key == LV_KEY_DOWN || key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        lv_event_stop_processing(e);
    }
}

/*
 * Playback screen navigation handler
 * - LEFT/RIGHT moves between buttons: [0]=Play, [1]=Speed, [2]=Reply
 * - UP/DOWN adjusts speed when on speed button (handled by mailbox_speed_adjust)
 * - TAB blocked
 * - ESC returns to inbox
 */
static void mailbox_playback_nav_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    // Block TAB
    if (key == '\t' || key == LV_KEY_NEXT || key == LV_KEY_PREV) {
        lv_event_stop_processing(e);
        return;
    }

    // ESC returns to inbox
    if (key == LV_KEY_ESC) {
        Serial.println("[Mailbox] Playback ESC - returning to inbox");
        cleanupMailboxPlayback();
        onLVGLMenuSelect(MODE_MORSE_MAILBOX_INBOX);
        lv_event_stop_processing(e);
        return;
    }

    lv_obj_t* target = lv_event_get_target(e);

    // Play button - LEFT/RIGHT navigation
    if (target == mailbox_playback_btns[0]) {
        if (key == LV_KEY_RIGHT && mailbox_playback_btn_count > 1) {
            lv_group_focus_obj(mailbox_playback_btns[1]);
            lv_event_stop_processing(e);
        } else if (key == LV_KEY_LEFT || key == LV_KEY_UP || key == LV_KEY_DOWN) {
            lv_event_stop_processing(e);  // Block, at edge
        }
    }
    // Speed button - LEFT/RIGHT navigates, UP/DOWN adjusts speed (handled by mailbox_speed_adjust)
    else if (target == mailbox_playback_btns[1]) {
        if (key == LV_KEY_LEFT && mailbox_playback_btn_count > 0) {
            lv_group_focus_obj(mailbox_playback_btns[0]);
            lv_event_stop_processing(e);
        } else if (key == LV_KEY_RIGHT && mailbox_playback_btn_count > 2) {
            lv_group_focus_obj(mailbox_playback_btns[2]);
            lv_event_stop_processing(e);
        }
        // UP/DOWN not stopped here - let mailbox_speed_adjust handle them
    }
    // Reply button - LEFT/RIGHT navigation
    else if (target == mailbox_playback_btns[2]) {
        if (key == LV_KEY_LEFT && mailbox_playback_btn_count > 1) {
            lv_group_focus_obj(mailbox_playback_btns[1]);
            lv_event_stop_processing(e);
        } else if (key == LV_KEY_RIGHT || key == LV_KEY_UP || key == LV_KEY_DOWN) {
            lv_event_stop_processing(e);  // Block, at edge
        }
    }
}

// ============================================
// Device Linking Screen
// ============================================

// Timer callback for link polling
static void mailbox_link_timer_cb(lv_timer_t* timer) {
    Serial.println("[Mailbox] Link timer callback fired");

    // Check link state
    int result = checkDeviceCode();
    Serial.printf("[Mailbox] checkDeviceCode returned: %d, state: %d\n", result, (int)getMailboxLinkState());

    // Update timer display
    int remaining = getMailboxLinkRemainingSeconds();
    if (remaining > 0 && mailbox_timer_label) {
        char buf[32];
        int mins = remaining / 60;
        int secs = remaining % 60;
        snprintf(buf, sizeof(buf), "Expires in %d:%02d", mins, secs);
        lv_label_set_text(mailbox_timer_label, buf);
    }

    // Update status based on result
    if (mailbox_status_label) {
        MailboxLinkState state = getMailboxLinkState();
        switch (state) {
            case MAILBOX_LINK_WAITING_FOR_USER:
                lv_label_set_text(mailbox_status_label, "Waiting for link...");
                lv_obj_set_style_text_color(mailbox_status_label, LV_COLOR_WARNING, 0);
                break;
            case MAILBOX_LINK_CHECKING:
                lv_label_set_text(mailbox_status_label, "Checking...");
                lv_obj_set_style_text_color(mailbox_status_label, LV_COLOR_ACCENT_CYAN, 0);
                break;
            case MAILBOX_LINK_EXCHANGING_TOKEN:
                lv_label_set_text(mailbox_status_label, "Linking account...");
                lv_obj_set_style_text_color(mailbox_status_label, LV_COLOR_ACCENT_CYAN, 0);
                break;
            case MAILBOX_LINK_SUCCESS: {
                Serial.println("[Mailbox] SUCCESS state - stopping timer and navigating");
                char successBuf[64];
                snprintf(successBuf, sizeof(successBuf), "Linked as %s!", getMailboxUserCallsign());
                lv_label_set_text(mailbox_status_label, successBuf);
                lv_obj_set_style_text_color(mailbox_status_label, LV_COLOR_SUCCESS, 0);
                // Stop timer
                if (mailbox_link_timer) {
                    Serial.println("[Mailbox] Deleting link timer (SUCCESS)");
                    lv_timer_del(mailbox_link_timer);
                    mailbox_link_timer = NULL;
                }
                // Navigate to inbox after 2 seconds
                lv_timer_create([](lv_timer_t* t) {
                    onLVGLMenuSelect(MODE_MORSE_MAILBOX_INBOX);
                    lv_timer_del(t);
                }, 2000, NULL);
                break;
            }
            case MAILBOX_LINK_EXPIRED:
                Serial.println("[Mailbox] EXPIRED state - stopping timer");
                lv_label_set_text(mailbox_status_label, "Code expired. Press ENTER to retry.");
                lv_obj_set_style_text_color(mailbox_status_label, LV_COLOR_ERROR, 0);
                if (mailbox_link_timer) {
                    Serial.println("[Mailbox] Deleting link timer (EXPIRED)");
                    lv_timer_del(mailbox_link_timer);
                    mailbox_link_timer = NULL;
                }
                break;
            case MAILBOX_LINK_ERROR: {
                Serial.printf("[Mailbox] ERROR state: %s\n", getMailboxLinkError().c_str());
                String errMsg = "Error: " + getMailboxLinkError();
                lv_label_set_text(mailbox_status_label, errMsg.c_str());
                lv_obj_set_style_text_color(mailbox_status_label, LV_COLOR_ERROR, 0);
                if (mailbox_link_timer) {
                    Serial.println("[Mailbox] Deleting link timer (ERROR)");
                    lv_timer_del(mailbox_link_timer);
                    mailbox_link_timer = NULL;
                }
                break;
            }
            default:
                break;
        }
    }
}

// Handle ENTER key to retry on error/expired
static void mailbox_link_key_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_ENTER) {
        MailboxLinkState state = getMailboxLinkState();
        if (state == MAILBOX_LINK_EXPIRED || state == MAILBOX_LINK_ERROR) {
            // Retry - request new code
            resetMailboxLinkState();
            if (requestDeviceCode()) {
                // Update display
                if (mailbox_code_label) {
                    lv_label_set_text(mailbox_code_label, getMailboxLinkCode().c_str());
                }
                if (mailbox_status_label) {
                    lv_label_set_text(mailbox_status_label, "Waiting for link...");
                    lv_obj_set_style_text_color(mailbox_status_label, LV_COLOR_WARNING, 0);
                }
                // Restart timer
                if (mailbox_link_timer) {
                    lv_timer_del(mailbox_link_timer);
                }
                mailbox_link_timer = lv_timer_create(mailbox_link_timer_cb, 5000, NULL);
            }
        }
    }
}

/*
 * Create device linking screen
 */
lv_obj_t* createMailboxLinkScreen() {
    // Check internet first
    if (getInternetStatus() != INET_CONNECTED) {
        // Show error screen
        lv_obj_t* screen = createScreen();
        applyScreenStyle(screen);

        lv_obj_t* content = lv_obj_create(screen);
        lv_obj_set_size(content, 400, 200);
        lv_obj_center(content);
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(content, 0, 0);
        lv_obj_set_style_pad_row(content, 15, 0);

        lv_obj_t* icon = lv_label_create(content);
        lv_label_set_text(icon, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(icon, LV_COLOR_WARNING, 0);

        lv_obj_t* msg = lv_label_create(content);
        lv_label_set_text(msg, "No Internet Connection");
        lv_obj_set_style_text_font(msg, getThemeFonts()->font_title, 0);
        lv_obj_set_style_text_color(msg, LV_COLOR_TEXT_PRIMARY, 0);

        lv_obj_t* hint = lv_label_create(content);
        lv_label_set_text(hint, "Connect to WiFi first, then try again");
        lv_obj_set_style_text_font(hint, getThemeFonts()->font_body, 0);
        lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_SECONDARY, 0);

        // Invisible focusable for ESC
        lv_obj_t* focus = lv_obj_create(screen);
        lv_obj_set_size(focus, 1, 1);
        lv_obj_set_style_bg_opa(focus, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(focus, 0, 0);
        lv_obj_add_flag(focus, LV_OBJ_FLAG_CLICKABLE);
        addNavigableWidget(focus);

        return screen;
    }

    // Request device code
    if (!requestDeviceCode()) {
        // Show error
        lv_obj_t* screen = createScreen();
        applyScreenStyle(screen);

        lv_obj_t* msg = lv_label_create(screen);
        lv_label_set_text(msg, "Failed to get device code");
        lv_obj_center(msg);
        lv_obj_set_style_text_color(msg, LV_COLOR_ERROR, 0);

        lv_obj_t* focus = lv_obj_create(screen);
        lv_obj_set_size(focus, 1, 1);
        lv_obj_set_style_bg_opa(focus, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(focus, 0, 0);
        addNavigableWidget(focus);

        return screen;
    }

    // Create link screen
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Link Morse Mailbox");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Main content
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, 440, 200);
    lv_obj_center(content);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(content, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(content, 1, 0);
    lv_obj_set_style_border_color(content, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_radius(content, 10, 0);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_style_pad_row(content, 12, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // Instructions
    lv_obj_t* instr = lv_label_create(content);
    lv_label_set_text(instr, "Visit morsemailbox.com/link-device");
    lv_obj_set_style_text_font(instr, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(instr, LV_COLOR_TEXT_SECONDARY, 0);

    lv_obj_t* instr2 = lv_label_create(content);
    lv_label_set_text(instr2, "and enter this code:");
    lv_obj_set_style_text_font(instr2, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(instr2, LV_COLOR_TEXT_SECONDARY, 0);

    // Code display (large, bold)
    mailbox_code_label = lv_label_create(content);
    lv_label_set_text(mailbox_code_label, getMailboxLinkCode().c_str());
    lv_obj_set_style_text_font(mailbox_code_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(mailbox_code_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_set_style_text_letter_space(mailbox_code_label, 8, 0);

    // Status
    mailbox_status_label = lv_label_create(content);
    lv_label_set_text(mailbox_status_label, "Waiting for link...");
    lv_obj_set_style_text_font(mailbox_status_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(mailbox_status_label, LV_COLOR_WARNING, 0);

    // Timer
    mailbox_timer_label = lv_label_create(content);
    int remaining = getMailboxLinkRemainingSeconds();
    char timerBuf[32];
    snprintf(timerBuf, sizeof(timerBuf), "Expires in %d:%02d", remaining / 60, remaining % 60);
    lv_label_set_text(mailbox_timer_label, timerBuf);
    lv_obj_set_style_text_font(mailbox_timer_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(mailbox_timer_label, LV_COLOR_TEXT_TERTIARY, 0);

    // Invisible focusable for keyboard input
    lv_obj_t* focus = lv_obj_create(screen);
    lv_obj_set_size(focus, 1, 1);
    lv_obj_set_style_bg_opa(focus, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus, 0, 0);
    lv_obj_add_flag(focus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus, mailbox_link_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus);

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "ESC Cancel");
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -10);

    // Start polling timer (every 5 seconds)
    if (mailbox_link_timer) {
        lv_timer_del(mailbox_link_timer);
    }
    mailbox_link_timer = lv_timer_create(mailbox_link_timer_cb, 5000, NULL);

    mailbox_link_screen = screen;
    return screen;
}

// ============================================
// Inbox Screen
// ============================================

// Message button click handler
static void mailbox_inbox_item_click(lv_event_t* e) {
    Serial.println("[Mailbox] Item click handler called!");
    lv_obj_t* btn = lv_event_get_target(e);
    String* msgId = (String*)lv_obj_get_user_data(btn);
    Serial.printf("[Mailbox] msgId ptr: %p\n", (void*)msgId);
    if (msgId) {
        Serial.printf("[Mailbox] msgId value: '%s', length: %d\n",
                      msgId->c_str(), msgId->length());
    }
    if (msgId && msgId->length() > 0) {
        currentPlaybackMessageId = *msgId;
        Serial.printf("[Mailbox] Switching to playback for message: %s\n",
                      currentPlaybackMessageId.c_str());
        onLVGLMenuSelect(MODE_MORSE_MAILBOX_PLAYBACK);
    } else {
        Serial.println("[Mailbox] ERROR: msgId is null or empty!");
    }
}

/*
 * Create inbox screen
 */
lv_obj_t* createMailboxInboxScreen() {
    // Reset navigation tracking
    mailbox_inbox_header_btn_count = 0;
    mailbox_inbox_message_count = 0;
    memset(mailbox_inbox_header_btns, 0, sizeof(mailbox_inbox_header_btns));
    memset(mailbox_inbox_message_items, 0, sizeof(mailbox_inbox_message_items));

    // Create screen first
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    mailbox_inbox_screen = screen;

    // Fetch inbox if needed (show loading during fetch)
    if (!isMailboxInboxCacheValid()) {
        showMailboxLoading(screen, "Loading inbox...");
        fetchMailboxInbox(20, "all");
        hideMailboxLoading();
    }

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Morse Mailbox Inbox");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Compose button in header
    lv_obj_t* compose_btn = lv_btn_create(header);
    lv_obj_set_size(compose_btn, 110, 35);
    lv_obj_align(compose_btn, LV_ALIGN_RIGHT_MID, -120, 0);
    lv_obj_set_style_bg_color(compose_btn, LV_COLOR_SUCCESS, 0);
    lv_obj_set_style_bg_color(compose_btn, LV_COLOR_ACCENT_GREEN, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(compose_btn, 5, 0);

    lv_obj_t* compose_lbl = lv_label_create(compose_btn);
    lv_label_set_text(compose_lbl, LV_SYMBOL_EDIT " New");
    lv_obj_set_style_text_font(compose_lbl, getThemeFonts()->font_body, 0);
    lv_obj_center(compose_lbl);

    lv_obj_add_event_cb(compose_btn, [](lv_event_t* e) {
        onLVGLMenuSelect(MODE_MORSE_MAILBOX_COMPOSE);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(compose_btn, mailbox_header_nav_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(compose_btn, mailbox_inbox_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(compose_btn);
    mailbox_inbox_header_btns[0] = compose_btn;

    // Account button in header
    lv_obj_t* account_btn = lv_btn_create(header);
    lv_obj_set_size(account_btn, 100, 35);
    lv_obj_align(account_btn, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(account_btn, LV_COLOR_CARD_TEAL, 0);
    lv_obj_set_style_bg_color(account_btn, LV_COLOR_CARD_CYAN, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(account_btn, 5, 0);

    lv_obj_t* account_lbl = lv_label_create(account_btn);
    lv_label_set_text(account_lbl, getMailboxUserCallsign());
    lv_obj_set_style_text_font(account_lbl, getThemeFonts()->font_body, 0);
    lv_obj_center(account_lbl);

    lv_obj_add_event_cb(account_btn, [](lv_event_t* e) {
        onLVGLMenuSelect(MODE_MORSE_MAILBOX_ACCOUNT);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(account_btn, mailbox_header_nav_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(account_btn, mailbox_inbox_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(account_btn);
    mailbox_inbox_header_btns[1] = account_btn;
    mailbox_inbox_header_btn_count = 2;

    // Message list
    lv_obj_t* list_container = lv_obj_create(screen);
    lv_obj_set_size(list_container, LV_PCT(95), SCREEN_HEIGHT - 100);
    lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_bg_opa(list_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_container, 0, 0);
    lv_obj_set_style_pad_all(list_container, 5, 0);
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list_container, 8, 0);
    lv_obj_add_flag(list_container, LV_OBJ_FLAG_SCROLLABLE);

    int msgCount = getMailboxInboxCount();
    MailboxMessage* msgs = getMailboxInboxCache();

    if (msgCount == 0) {
        lv_obj_t* empty = lv_label_create(list_container);
        lv_label_set_text(empty, "No messages");
        lv_obj_set_style_text_font(empty, getThemeFonts()->font_body, 0);
        lv_obj_set_style_text_color(empty, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_align(empty, LV_ALIGN_CENTER, 0, 0);

        // Need a focusable element
        lv_obj_t* focus = lv_obj_create(screen);
        lv_obj_set_size(focus, 1, 1);
        lv_obj_set_style_bg_opa(focus, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(focus, 0, 0);
        addNavigableWidget(focus);
    } else {
        // Create message items
        for (int i = 0; i < msgCount; i++) {
            MailboxMessage& msg = msgs[i];

            lv_obj_t* item = lv_btn_create(list_container);
            lv_obj_set_size(item, LV_PCT(100), 60);
            lv_obj_set_style_bg_color(item, LV_COLOR_BG_LAYER2, 0);
            lv_obj_set_style_bg_color(item, LV_COLOR_CARD_CYAN, LV_STATE_FOCUSED);
            lv_obj_set_style_radius(item, 8, 0);
            lv_obj_set_style_border_width(item, 1, 0);
            lv_obj_set_style_border_color(item, LV_COLOR_BORDER_SUBTLE, 0);
            lv_obj_set_style_border_color(item, LV_COLOR_ACCENT_CYAN, LV_STATE_FOCUSED);

            // Store message ID for click handler (use file-scope array)
            mailbox_inbox_msgIds[i] = msg.id;
            lv_obj_set_user_data(item, &mailbox_inbox_msgIds[i]);

            // Row layout
            lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_hor(item, 15, 0);

            // Status indicator (dot) - make non-clickable so parent button gets clicks
            lv_obj_t* dot = lv_obj_create(item);
            lv_obj_set_size(dot, 10, 10);
            lv_obj_set_style_radius(dot, 5, 0);
            lv_obj_set_style_bg_color(dot, msg.status == "unread" ? LV_COLOR_SUCCESS : LV_COLOR_TEXT_DISABLED, 0);
            lv_obj_set_style_border_width(dot, 0, 0);
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

            // Sender info column - use flex layout for proper stacking
            lv_obj_t* info_col = lv_obj_create(item);
            lv_obj_set_size(info_col, 280, 45);  // Fixed height for two lines
            lv_obj_set_style_bg_opa(info_col, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(info_col, 0, 0);
            lv_obj_set_style_pad_left(info_col, 10, 0);
            lv_obj_set_style_pad_ver(info_col, 0, 0);
            lv_obj_clear_flag(info_col, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_flex_flow(info_col, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(info_col, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

            lv_obj_t* sender = lv_label_create(info_col);
            lv_label_set_text(sender, msg.senderCallsign.c_str());
            lv_obj_set_style_text_font(sender, getThemeFonts()->font_input, 0);
            lv_obj_set_style_text_color(sender, LV_COLOR_TEXT_PRIMARY, 0);

            // Parse and format date (simplified - just show date portion)
            String dateStr = msg.sentAt.substring(0, 10);  // "2026-01-29"
            lv_obj_t* date = lv_label_create(info_col);
            lv_label_set_text(date, dateStr.c_str());
            lv_obj_set_style_text_font(date, getThemeFonts()->font_body, 0);
            lv_obj_set_style_text_color(date, LV_COLOR_TEXT_TERTIARY, 0);

            // Duration
            char durBuf[16];
            snprintf(durBuf, sizeof(durBuf), "%.1fs", msg.durationMs / 1000.0f);
            lv_obj_t* dur = lv_label_create(item);
            lv_label_set_text(dur, durBuf);
            lv_obj_set_style_text_font(dur, getThemeFonts()->font_body, 0);
            lv_obj_set_style_text_color(dur, LV_COLOR_TEXT_SECONDARY, 0);
            lv_obj_set_flex_grow(dur, 1);
            lv_obj_set_style_text_align(dur, LV_TEXT_ALIGN_RIGHT, 0);

            // Click handler
            lv_obj_add_event_cb(item, mailbox_inbox_item_click, LV_EVENT_CLICKED, NULL);
            lv_obj_add_event_cb(item, mailbox_list_nav_handler, LV_EVENT_KEY, NULL);
            lv_obj_add_event_cb(item, mailbox_inbox_key_handler, LV_EVENT_KEY, NULL);
            addNavigableWidget(item);
            mailbox_inbox_message_items[i] = item;
        }
        mailbox_inbox_message_count = msgCount;
    }

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "Arrows Navigate   ENTER Play   R Refresh   ESC Back");
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -5);

    mailbox_inbox_screen = screen;
    return screen;
}

// ============================================
// Message Playback Screen
// ============================================

// Playback timer callback
static void mailbox_playback_timer_cb(lv_timer_t* timer);

static void mailbox_start_playback() {
    if (!isMailboxMessageLoaded()) return;

    JsonDocument& doc = getCurrentMailboxMessage();
    JsonArray timing = doc["morse_timing"].as<JsonArray>();

    if (timing.size() == 0) {
        Serial.println("[Mailbox] No timing data in message");
        return;
    }

    mailbox_playback_event_index = 0;
    mailbox_playback_start_time = millis();
    mailbox_is_playing = true;
    setMailboxPlaybackState(MB_PLAYBACK_PLAYING);

    // Update button text
    if (mailbox_play_btn) {
        lv_obj_t* lbl = lv_obj_get_child(mailbox_play_btn, 0);
        if (lbl) lv_label_set_text(lbl, LV_SYMBOL_PAUSE " Pause");
    }

    // Create playback timer (run every 10ms for timing precision)
    // Delete any existing timer first
    if (mailbox_playback_timer) {
        lv_timer_del(mailbox_playback_timer);
    }
    mailbox_playback_timer = lv_timer_create(mailbox_playback_timer_cb, 10, NULL);
}

static void mailbox_stop_playback() {
    mailbox_is_playing = false;
    setMailboxPlaybackState(MB_PLAYBACK_READY);

    // Stop any playing tone
    extern void requestStopTone();
    requestStopTone();

    // Delete the playback timer
    if (mailbox_playback_timer) {
        lv_timer_del(mailbox_playback_timer);
        mailbox_playback_timer = NULL;
    }

    // Update button text (check screen still exists)
    if (mailbox_playback_screen && lv_obj_is_valid(mailbox_playback_screen) && mailbox_play_btn) {
        lv_obj_t* lbl = lv_obj_get_child(mailbox_play_btn, 0);
        if (lbl) lv_label_set_text(lbl, LV_SYMBOL_PLAY " Play");
    }
}

static void mailbox_playback_timer_cb(lv_timer_t* timer) {
    // Safety check - stop if not playing or screen destroyed
    if (!mailbox_is_playing || !mailbox_playback_screen || !lv_obj_is_valid(mailbox_playback_screen)) {
        mailbox_playback_timer = NULL;
        lv_timer_del(timer);
        return;
    }

    JsonDocument& doc = getCurrentMailboxMessage();
    JsonArray timing = doc["morse_timing"].as<JsonArray>();

    if (mailbox_playback_event_index >= timing.size()) {
        // Playback complete
        mailbox_is_playing = false;
        setMailboxPlaybackState(MB_PLAYBACK_COMPLETE);

        // Stop any playing tone
        extern void requestStopTone();
        requestStopTone();

        // Update button text to show "Replay" (check screen still valid)
        if (mailbox_playback_screen && lv_obj_is_valid(mailbox_playback_screen) && mailbox_play_btn) {
            lv_obj_t* lbl = lv_obj_get_child(mailbox_play_btn, 0);
            if (lbl) lv_label_set_text(lbl, LV_SYMBOL_REFRESH " Replay");
        }

        // Mark message as read
        markMailboxMessageRead(currentPlaybackMessageId);
        mailbox_playback_timer = NULL;
        lv_timer_del(timer);
        return;
    }

    // Calculate current playback time with speed adjustment
    unsigned long elapsed = (unsigned long)((millis() - mailbox_playback_start_time) * mailbox_playback_speed);

    // Process events up to current time
    while (mailbox_playback_event_index < timing.size()) {
        JsonObject event = timing[mailbox_playback_event_index];
        int timestamp = event["timestamp"].as<int>();

        if (timestamp > elapsed) {
            break;  // Not time for this event yet
        }

        String type = event["type"].as<String>();
        extern void requestStartTone(int freq);
        extern void requestStopTone();
        extern int cwTone;

        if (type == "keydown") {
            requestStartTone(cwTone);
        } else {
            requestStopTone();
        }

        mailbox_playback_event_index++;
    }
}

static void mailbox_play_btn_click(lv_event_t* e) {
    MailboxPlaybackState state = getMailboxPlaybackState();

    if (mailbox_is_playing) {
        mailbox_stop_playback();
    } else {
        // Reset to beginning if playback was complete
        if (state == MB_PLAYBACK_COMPLETE) {
            mailbox_playback_event_index = 0;
            setMailboxPlaybackState(MB_PLAYBACK_READY);
        }
        mailbox_start_playback();
    }
}

static void mailbox_speed_adjust(lv_event_t* e) {
    uint32_t key = lv_event_get_key(e);
    bool changed = false;

    // UP/DOWN adjusts speed when focused on speed button
    if (key == LV_KEY_UP) {
        if (mailbox_playback_speed < 2.0f) {
            mailbox_playback_speed += 0.25f;
            changed = true;
        }
        lv_event_stop_processing(e);
    } else if (key == LV_KEY_DOWN) {
        if (mailbox_playback_speed > 0.5f) {
            mailbox_playback_speed -= 0.25f;
            changed = true;
        }
        lv_event_stop_processing(e);
    }

    // Update both the header label and button label
    if (changed) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2fx", mailbox_playback_speed);
        if (mailbox_speed_label) {
            lv_label_set_text(mailbox_speed_label, buf);
        }
        // Also update button label
        if (mailbox_playback_btns[1]) {
            lv_obj_t* btn_lbl = lv_obj_get_child(mailbox_playback_btns[1], 0);
            if (btn_lbl) {
                char btnBuf[24];
                snprintf(btnBuf, sizeof(btnBuf), LV_SYMBOL_UP LV_SYMBOL_DOWN " %.2fx", mailbox_playback_speed);
                lv_label_set_text(btn_lbl, btnBuf);
            }
        }
    }
}

// Reply button click handler
static void mailbox_reply_btn_click(lv_event_t* e) {
    Serial.println("[Mailbox] Reply button clicked");
    // Stop playback if playing
    if (mailbox_is_playing) {
        mailbox_stop_playback();
    }
    // Get sender callsign from current message
    JsonDocument& doc = getCurrentMailboxMessage();
    mailbox_reply_recipient = doc["sender"]["callsign"].as<String>();
    Serial.printf("[Mailbox] Setting reply recipient: %s\n", mailbox_reply_recipient.c_str());
    onLVGLMenuSelect(MODE_MORSE_MAILBOX_COMPOSE);
}

/*
 * Create message playback screen
 */
lv_obj_t* createMailboxPlaybackScreen() {
    // Create screen first (for loading indicator)
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    mailbox_playback_screen = screen;

    // Show loading indicator while fetching
    showMailboxLoading(screen, "Loading message...");

    // Fetch the message (blocking network call)
    bool success = fetchMailboxMessage(currentPlaybackMessageId);

    // Hide loading
    hideMailboxLoading();

    if (!success) {
        // Error - show message and back button
        lv_obj_t* msg = lv_label_create(screen);
        lv_label_set_text(msg, "Failed to load message");
        lv_obj_center(msg);
        lv_obj_set_style_text_color(msg, LV_COLOR_ERROR, 0);

        lv_obj_t* focus = lv_obj_create(screen);
        lv_obj_set_size(focus, 1, 1);
        lv_obj_set_style_bg_opa(focus, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(focus, 0, 0);
        // Add ESC handler to go back
        lv_obj_add_event_cb(focus, [](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
                cleanupMailboxPlayback();
                onLVGLMenuSelect(MODE_MORSE_MAILBOX_INBOX);
            }
        }, LV_EVENT_KEY, NULL);
        addNavigableWidget(focus);

        return screen;
    }

    JsonDocument& doc = getCurrentMailboxMessage();

    // Continue building the screen (already created above)
    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Message Playback");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Message info card
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, 440, 180);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(card, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 20, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // From label
    String senderCallsign = doc["sender"]["callsign"].as<String>();
    String senderMmid = doc["sender"]["morse_mailbox_id"].as<String>();

    lv_obj_t* from_lbl = lv_label_create(card);
    char fromBuf[64];
    snprintf(fromBuf, sizeof(fromBuf), "From: %s (%s)", senderCallsign.c_str(), senderMmid.c_str());
    lv_label_set_text(from_lbl, fromBuf);
    lv_obj_set_style_text_font(from_lbl, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(from_lbl, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(from_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    // Date
    String sentAt = doc["sent_at"].as<String>();
    lv_obj_t* date_lbl = lv_label_create(card);
    lv_label_set_text(date_lbl, sentAt.substring(0, 10).c_str());
    lv_obj_set_style_text_font(date_lbl, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(date_lbl, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(date_lbl, LV_ALIGN_TOP_LEFT, 0, 30);

    // Play button
    mailbox_play_btn = lv_btn_create(card);
    lv_obj_set_size(mailbox_play_btn, 140, 50);
    lv_obj_align(mailbox_play_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(mailbox_play_btn, LV_COLOR_SUCCESS, 0);
    lv_obj_set_style_bg_color(mailbox_play_btn, LV_COLOR_ACCENT_GREEN, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(mailbox_play_btn, 8, 0);

    lv_obj_t* play_lbl = lv_label_create(mailbox_play_btn);
    lv_label_set_text(play_lbl, LV_SYMBOL_PLAY " Play");
    lv_obj_set_style_text_font(play_lbl, getThemeFonts()->font_input, 0);
    lv_obj_center(play_lbl);

    lv_obj_add_event_cb(mailbox_play_btn, mailbox_play_btn_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(mailbox_play_btn, mailbox_playback_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(mailbox_play_btn);
    mailbox_playback_btns[0] = mailbox_play_btn;

    // Speed control (displayed above the buttons)
    lv_obj_t* speed_container = lv_obj_create(card);
    lv_obj_set_size(speed_container, 150, 30);
    lv_obj_align(speed_container, LV_ALIGN_TOP_RIGHT, 0, 30);
    lv_obj_set_style_bg_opa(speed_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(speed_container, 0, 0);
    lv_obj_clear_flag(speed_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* speed_title = lv_label_create(speed_container);
    lv_label_set_text(speed_title, "Speed:");
    lv_obj_set_style_text_font(speed_title, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(speed_title, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(speed_title, LV_ALIGN_LEFT_MID, 0, 0);

    mailbox_speed_label = lv_label_create(speed_container);
    char speedBuf[16];
    snprintf(speedBuf, sizeof(speedBuf), "%.2fx", mailbox_playback_speed);
    lv_label_set_text(mailbox_speed_label, speedBuf);
    lv_obj_set_style_text_font(mailbox_speed_label, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(mailbox_speed_label, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(mailbox_speed_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // Speed adjustment button (UP/DOWN to adjust when focused)
    lv_obj_t* speed_btn = lv_btn_create(card);
    lv_obj_set_size(speed_btn, 100, 40);
    lv_obj_align(speed_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(speed_btn, LV_COLOR_CARD_TEAL, 0);
    lv_obj_set_style_bg_color(speed_btn, LV_COLOR_CARD_CYAN, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(speed_btn, 5, 0);

    lv_obj_t* adj_lbl = lv_label_create(speed_btn);
    char speedBtnText[24];
    snprintf(speedBtnText, sizeof(speedBtnText), LV_SYMBOL_UP LV_SYMBOL_DOWN " %.2fx", mailbox_playback_speed);
    lv_label_set_text(adj_lbl, speedBtnText);
    lv_obj_set_style_text_font(adj_lbl, getThemeFonts()->font_body, 0);
    lv_obj_center(adj_lbl);

    lv_obj_add_event_cb(speed_btn, mailbox_speed_adjust, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(speed_btn, mailbox_playback_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(speed_btn);
    mailbox_playback_btns[1] = speed_btn;

    // Reply button
    lv_obj_t* reply_btn = lv_btn_create(card);
    lv_obj_set_size(reply_btn, 100, 40);
    lv_obj_align(reply_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(reply_btn, LV_COLOR_SUCCESS, 0);
    lv_obj_set_style_bg_color(reply_btn, LV_COLOR_ACCENT_GREEN, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(reply_btn, 5, 0);

    lv_obj_t* reply_lbl = lv_label_create(reply_btn);
    lv_label_set_text(reply_lbl, LV_SYMBOL_EDIT " Reply");
    lv_obj_set_style_text_font(reply_lbl, getThemeFonts()->font_body, 0);
    lv_obj_center(reply_lbl);

    lv_obj_add_event_cb(reply_btn, mailbox_reply_btn_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(reply_btn, mailbox_playback_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(reply_btn);
    mailbox_playback_btns[2] = reply_btn;
    mailbox_playback_btn_count = 3;

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "ENTER Select  L/R Navigate  U/D Speed  ESC Back");
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -5);

    mailbox_playback_screen = screen;
    return screen;
}

// ============================================
// Account Screen
// ============================================

static void mailbox_unlink_confirm(lv_event_t* e) {
    clearMailboxCredentials();
    onLVGLMenuSelect(MODE_MORSE_MAILBOX);
}

/*
 * Create account info/unlink screen
 */
lv_obj_t* createMailboxAccountScreen() {
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Morse Mailbox Account");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Account info card
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, 400, 150);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_bg_color(card, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 20, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Callsign
    lv_obj_t* cs_row = lv_obj_create(card);
    lv_obj_set_size(cs_row, LV_PCT(100), 35);
    lv_obj_set_style_bg_opa(cs_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cs_row, 0, 0);
    lv_obj_clear_flag(cs_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(cs_row, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* cs_lbl = lv_label_create(cs_row);
    lv_label_set_text(cs_lbl, "Linked as:");
    lv_obj_set_style_text_font(cs_lbl, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(cs_lbl, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(cs_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* cs_val = lv_label_create(cs_row);
    lv_label_set_text(cs_val, getMailboxUserCallsign());
    lv_obj_set_style_text_font(cs_val, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(cs_val, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(cs_val, LV_ALIGN_RIGHT_MID, 0, 0);

    // MM ID
    lv_obj_t* mm_row = lv_obj_create(card);
    lv_obj_set_size(mm_row, LV_PCT(100), 35);
    lv_obj_set_style_bg_opa(mm_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mm_row, 0, 0);
    lv_obj_clear_flag(mm_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(mm_row, LV_ALIGN_TOP_LEFT, 0, 40);

    lv_obj_t* mm_lbl = lv_label_create(mm_row);
    lv_label_set_text(mm_lbl, "Mailbox ID:");
    lv_obj_set_style_text_font(mm_lbl, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(mm_lbl, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(mm_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* mm_val = lv_label_create(mm_row);
    lv_label_set_text(mm_val, getMailboxUserMmid());
    lv_obj_set_style_text_font(mm_val, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(mm_val, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_align(mm_val, LV_ALIGN_RIGHT_MID, 0, 0);

    // Device ID (truncated)
    lv_obj_t* dev_row = lv_obj_create(card);
    lv_obj_set_size(dev_row, LV_PCT(100), 35);
    lv_obj_set_style_bg_opa(dev_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dev_row, 0, 0);
    lv_obj_clear_flag(dev_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(dev_row, LV_ALIGN_TOP_LEFT, 0, 80);

    lv_obj_t* dev_lbl = lv_label_create(dev_row);
    lv_label_set_text(dev_lbl, "Device ID:");
    lv_obj_set_style_text_font(dev_lbl, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(dev_lbl, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(dev_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    String devId = getMailboxDeviceId();
    if (devId.length() > 15) {
        devId = devId.substring(0, 12) + "...";
    }
    lv_obj_t* dev_val = lv_label_create(dev_row);
    lv_label_set_text(dev_val, devId.c_str());
    lv_obj_set_style_text_font(dev_val, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(dev_val, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(dev_val, LV_ALIGN_RIGHT_MID, 0, 0);

    // Unlink button
    lv_obj_t* unlink_btn = lv_btn_create(screen);
    lv_obj_set_size(unlink_btn, 200, 50);
    lv_obj_align(unlink_btn, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_set_style_bg_color(unlink_btn, LV_COLOR_ERROR, 0);
    lv_obj_set_style_bg_color(unlink_btn, lv_color_hex(0xFACB), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(unlink_btn, 8, 0);

    lv_obj_t* unlink_lbl = lv_label_create(unlink_btn);
    lv_label_set_text(unlink_lbl, "Unlink Device");
    lv_obj_set_style_text_font(unlink_lbl, getThemeFonts()->font_input, 0);
    lv_obj_center(unlink_lbl);

    lv_obj_add_event_cb(unlink_btn, mailbox_unlink_confirm, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(unlink_btn, mailbox_linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(unlink_btn);

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "ENTER Unlink   ESC Back");
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -5);

    mailbox_account_screen = screen;
    return screen;
}

// ============================================
// Compose Screen
// ============================================

// Compose screen state
static lv_obj_t* mailbox_compose_screen = NULL;
static lv_obj_t* mailbox_recipient_input = NULL;
static lv_obj_t* mailbox_record_status_label = NULL;
static lv_obj_t* mailbox_record_duration_label = NULL;
static lv_obj_t* mailbox_record_btn = NULL;
static lv_obj_t* mailbox_send_btn = NULL;
static String compose_recipient = "";
static bool compose_input_focused = false;

// Keyer for recording
#include "../keyer/keyer.h"
#include "../core/task_manager.h"
#include "../settings/settings_cw.h"

static StraightKeyer* composeKeyer = nullptr;
static bool composeDitPressed = false;
static bool composeDahPressed = false;

// Keyer callback - handles both audio and timing recording
static void composeKeyerCallback(bool txOn, int element) {
    extern int cwTone;

    if (txOn) {
        // Start tone on Core 0
        extern void startToneInternal(int frequency);
        startToneInternal(cwTone);
        // Record keydown event
        recordMailboxKeyEvent(true);
    } else {
        // Stop tone on Core 0
        extern void stopToneInternal();
        stopToneInternal();
        // Record keyup event
        recordMailboxKeyEvent(false);
    }
}

// Paddle callback - runs on Core 0 for precise timing
static void composePaddleCallback(bool ditPressed, bool dahPressed, unsigned long now) {
    if (!composeKeyer) return;

    // Feed paddle state changes to keyer
    if (ditPressed != composeDitPressed) {
        composeKeyer->key(PADDLE_DIT, ditPressed);
        composeDitPressed = ditPressed;
    }
    if (dahPressed != composeDahPressed) {
        composeKeyer->key(PADDLE_DAH, dahPressed);
        composeDahPressed = dahPressed;
    }

    // Tick keyer state machine
    composeKeyer->tick(now);
}

// Update compose screen state (called from timer)
static void mailbox_compose_update_timer_cb(lv_timer_t* timer) {
    MailboxRecordState state = getMailboxRecordState();

    // Update status label
    if (mailbox_record_status_label) {
        switch (state) {
            case MB_RECORD_READY:
                lv_label_set_text(mailbox_record_status_label, "Press Start, then key your message");
                lv_obj_set_style_text_color(mailbox_record_status_label, LV_COLOR_TEXT_SECONDARY, 0);
                break;
            case MB_RECORD_RECORDING:
                lv_label_set_text(mailbox_record_status_label, "Recording... (use paddle)");
                lv_obj_set_style_text_color(mailbox_record_status_label, LV_COLOR_ERROR, 0);
                break;
            case MB_RECORD_STOPPED:
                lv_label_set_text(mailbox_record_status_label, "Recording complete");
                lv_obj_set_style_text_color(mailbox_record_status_label, LV_COLOR_SUCCESS, 0);
                break;
            case MB_RECORD_SENDING:
                lv_label_set_text(mailbox_record_status_label, "Sending...");
                lv_obj_set_style_text_color(mailbox_record_status_label, LV_COLOR_ACCENT_CYAN, 0);
                break;
            case MB_RECORD_SENT:
                lv_label_set_text(mailbox_record_status_label, "Message sent!");
                lv_obj_set_style_text_color(mailbox_record_status_label, LV_COLOR_SUCCESS, 0);
                break;
            case MB_RECORD_ERROR:
                lv_label_set_text(mailbox_record_status_label, "Failed to send");
                lv_obj_set_style_text_color(mailbox_record_status_label, LV_COLOR_ERROR, 0);
                break;
            default:
                break;
        }
    }

    // Update duration label
    if (mailbox_record_duration_label) {
        int events = getRecordedTimingCount();
        if (events > 0) {
            int durationMs = getRecordedDurationMs();
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f sec (%d events)", durationMs / 1000.0f, events);
            lv_label_set_text(mailbox_record_duration_label, buf);
        } else {
            lv_label_set_text(mailbox_record_duration_label, "No recording");
        }
    }

    // Update button text based on state
    if (mailbox_record_btn) {
        lv_obj_t* lbl = lv_obj_get_child(mailbox_record_btn, 0);
        if (lbl) {
            if (state == MB_RECORD_RECORDING) {
                lv_label_set_text(lbl, LV_SYMBOL_STOP " Stop");
            } else {
                lv_label_set_text(lbl, LV_SYMBOL_AUDIO " Start");
            }
        }
    }

    // Enable/disable send button based on state
    if (mailbox_send_btn) {
        if (state == MB_RECORD_STOPPED && getRecordedTimingCount() > 0) {
            lv_obj_clear_state(mailbox_send_btn, LV_STATE_DISABLED);
            lv_obj_set_style_bg_color(mailbox_send_btn, LV_COLOR_SUCCESS, 0);
        } else {
            lv_obj_add_state(mailbox_send_btn, LV_STATE_DISABLED);
            lv_obj_set_style_bg_color(mailbox_send_btn, LV_COLOR_TEXT_DISABLED, 0);
        }
    }

    // Auto-navigate away after successful send
    if (state == MB_RECORD_SENT) {
        static int sentCounter = 0;
        sentCounter++;
        if (sentCounter > 20) {  // ~2 seconds at 100ms interval
            sentCounter = 0;
            onLVGLMenuSelect(MODE_MORSE_MAILBOX_INBOX);
        }
    }
}

// Recipient input event handler
static void mailbox_recipient_input_event(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* ta = lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED) {
        compose_input_focused = true;
        // Unregister paddle callback while typing
        extern void registerPaddleCallback(void (*callback)(bool, bool, unsigned long));
        registerPaddleCallback(nullptr);
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        compose_input_focused = false;
        // Re-register paddle callback
        extern void registerPaddleCallback(void (*callback)(bool, bool, unsigned long));
        registerPaddleCallback(composePaddleCallback);
    }
    else if (code == LV_EVENT_VALUE_CHANGED) {
        compose_recipient = lv_textarea_get_text(ta);
    }
}

// Record button click
static void mailbox_record_btn_click(lv_event_t* e) {
    MailboxRecordState state = getMailboxRecordState();

    if (state == MB_RECORD_RECORDING) {
        // Stop recording
        stopMailboxRecording();
        // Stop any playing tone
        extern void requestStopTone();
        requestStopTone();
    } else {
        // Start recording (or re-record)
        clearMailboxRecording();
        startMailboxRecording();
    }
}

// Send button click
static void mailbox_send_btn_click(lv_event_t* e) {
    if (compose_recipient.length() < 2) {
        if (mailbox_record_status_label) {
            lv_label_set_text(mailbox_record_status_label, "Enter recipient callsign");
            lv_obj_set_style_text_color(mailbox_record_status_label, LV_COLOR_ERROR, 0);
        }
        return;
    }

    if (getRecordedTimingCount() == 0) {
        if (mailbox_record_status_label) {
            lv_label_set_text(mailbox_record_status_label, "Record a message first");
            lv_obj_set_style_text_color(mailbox_record_status_label, LV_COLOR_ERROR, 0);
        }
        return;
    }

    setMailboxRecordState(MB_RECORD_SENDING);

    // Get timing JSON and send
    String timingJson = getRecordedTimingJson();
    if (sendMailboxMessage(compose_recipient, timingJson)) {
        setMailboxRecordState(MB_RECORD_SENT);
        invalidateMailboxInboxCache();  // Force refresh on return to inbox
    } else {
        setMailboxRecordState(MB_RECORD_ERROR);
    }
}

// Cleanup compose keyer
static void cleanupComposeKeyer() {
    // Unregister paddle callback
    extern void registerPaddleCallback(void (*callback)(bool, bool, unsigned long));
    registerPaddleCallback(nullptr);

    // Stop any tone
    extern void requestStopTone();
    requestStopTone();

    // Reset keyer
    if (composeKeyer) {
        composeKeyer->reset();
    }
    composeDitPressed = false;
    composeDahPressed = false;
}

/*
 * Create compose/send message screen
 */
lv_obj_t* createMailboxComposeScreen() {
    // Reset navigation tracking
    mailbox_compose_focusable_count = 0;
    memset(mailbox_compose_focusable, 0, sizeof(mailbox_compose_focusable));

    // Initialize recording state
    clearMailboxRecording();
    setMailboxRecordState(MB_RECORD_READY);

    // Initialize keyer
    extern int cwSpeed;
    extern KeyType cwKeyType;
    composeKeyer = getKeyer(cwKeyType);
    composeKeyer->reset();
    composeKeyer->setDitDuration(DIT_DURATION(cwSpeed));
    composeKeyer->setTxCallback(composeKeyerCallback);
    composeDitPressed = false;
    composeDahPressed = false;

    // Register paddle callback for Core 0 timing
    extern void registerPaddleCallback(void (*callback)(bool, bool, unsigned long));
    registerPaddleCallback(composePaddleCallback);

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Compose Message");
    lv_obj_set_style_text_font(title, getThemeFonts()->font_input, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Main content card
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, 460, 200);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_bg_color(card, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 15, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Recipient row
    lv_obj_t* recip_row = lv_obj_create(card);
    lv_obj_set_size(recip_row, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(recip_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(recip_row, 0, 0);
    lv_obj_clear_flag(recip_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(recip_row, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* recip_lbl = lv_label_create(recip_row);
    lv_label_set_text(recip_lbl, "To:");
    lv_obj_set_style_text_font(recip_lbl, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(recip_lbl, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(recip_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    // Recipient input
    mailbox_recipient_input = lv_textarea_create(recip_row);
    lv_obj_set_size(mailbox_recipient_input, 350, 35);
    lv_obj_align(mailbox_recipient_input, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_textarea_set_one_line(mailbox_recipient_input, true);
    lv_textarea_set_placeholder_text(mailbox_recipient_input, "Callsign (e.g., W1ABC)");
    lv_textarea_set_max_length(mailbox_recipient_input, 15);
    lv_obj_set_style_bg_color(mailbox_recipient_input, LV_COLOR_BG_DEEP, 0);
    lv_obj_set_style_border_color(mailbox_recipient_input, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_border_color(mailbox_recipient_input, LV_COLOR_ACCENT_CYAN, LV_STATE_FOCUSED);
    lv_obj_set_style_text_font(mailbox_recipient_input, getThemeFonts()->font_input, 0);
    lv_obj_add_event_cb(mailbox_recipient_input, mailbox_recipient_input_event, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(mailbox_recipient_input, mailbox_compose_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(mailbox_recipient_input);
    mailbox_compose_focusable[0] = mailbox_recipient_input;

    // Pre-fill recipient if replying to a message
    if (mailbox_reply_recipient.length() > 0) {
        Serial.printf("[Mailbox] Pre-filling recipient: %s\n", mailbox_reply_recipient.c_str());
        lv_textarea_set_text(mailbox_recipient_input, mailbox_reply_recipient.c_str());
        compose_recipient = mailbox_reply_recipient;
        mailbox_reply_recipient = "";  // Clear after use
    }

    // Status label
    mailbox_record_status_label = lv_label_create(card);
    lv_label_set_text(mailbox_record_status_label, "Press Start, then key your message");
    lv_obj_set_style_text_font(mailbox_record_status_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(mailbox_record_status_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(mailbox_record_status_label, LV_ALIGN_TOP_MID, 0, 50);

    // Duration label
    mailbox_record_duration_label = lv_label_create(card);
    lv_label_set_text(mailbox_record_duration_label, "No recording");
    lv_obj_set_style_text_font(mailbox_record_duration_label, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(mailbox_record_duration_label, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(mailbox_record_duration_label, LV_ALIGN_TOP_MID, 0, 75);

    // Button row
    lv_obj_t* btn_row = lv_obj_create(card);
    lv_obj_set_size(btn_row, LV_PCT(100), 60);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Record/Stop button
    mailbox_record_btn = lv_btn_create(btn_row);
    lv_obj_set_size(mailbox_record_btn, 140, 45);
    lv_obj_set_style_bg_color(mailbox_record_btn, LV_COLOR_ERROR, 0);
    lv_obj_set_style_bg_color(mailbox_record_btn, lv_color_hex(0xFACB), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(mailbox_record_btn, 8, 0);

    lv_obj_t* record_lbl = lv_label_create(mailbox_record_btn);
    lv_label_set_text(record_lbl, LV_SYMBOL_AUDIO " Start");
    lv_obj_set_style_text_font(record_lbl, getThemeFonts()->font_input, 0);
    lv_obj_center(record_lbl);

    lv_obj_add_event_cb(mailbox_record_btn, mailbox_record_btn_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(mailbox_record_btn, mailbox_compose_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(mailbox_record_btn);
    mailbox_compose_focusable[1] = mailbox_record_btn;

    // Send button
    mailbox_send_btn = lv_btn_create(btn_row);
    lv_obj_set_size(mailbox_send_btn, 140, 45);
    lv_obj_set_style_bg_color(mailbox_send_btn, LV_COLOR_TEXT_DISABLED, 0);
    lv_obj_set_style_bg_color(mailbox_send_btn, LV_COLOR_ACCENT_GREEN, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(mailbox_send_btn, 8, 0);
    lv_obj_add_state(mailbox_send_btn, LV_STATE_DISABLED);

    lv_obj_t* send_lbl = lv_label_create(mailbox_send_btn);
    lv_label_set_text(send_lbl, LV_SYMBOL_OK " Send");
    lv_obj_set_style_text_font(send_lbl, getThemeFonts()->font_input, 0);
    lv_obj_center(send_lbl);

    lv_obj_add_event_cb(mailbox_send_btn, mailbox_send_btn_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(mailbox_send_btn, mailbox_compose_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(mailbox_send_btn);
    mailbox_compose_focusable[2] = mailbox_send_btn;
    mailbox_compose_focusable_count = 3;

    // Footer
    lv_obj_t* footer = lv_label_create(screen);
    lv_label_set_text(footer, "Arrows Navigate   ENTER Select   ESC Cancel");
    lv_obj_set_style_text_font(footer, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(footer, LV_COLOR_WARNING, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -5);

    // Create update timer (store reference for cleanup)
    lv_timer_t* compose_timer = lv_timer_create(mailbox_compose_update_timer_cb, 100, NULL);

    // Add screen delete event handler to cleanup keyer
    lv_obj_add_event_cb(screen, [](lv_event_t* e) {
        cleanupComposeKeyer();
        // Also delete the timer
        lv_timer_t* timer = (lv_timer_t*)lv_event_get_user_data(e);
        if (timer) {
            lv_timer_del(timer);
        }
    }, LV_EVENT_DELETE, compose_timer);

    mailbox_compose_screen = screen;
    return screen;
}

// Cleanup compose screen resources
void cleanupMailboxCompose() {
    cleanupComposeKeyer();
    clearMailboxRecording();
    mailbox_recipient_input = NULL;
    mailbox_record_status_label = NULL;
    mailbox_record_duration_label = NULL;
    mailbox_record_btn = NULL;
    mailbox_send_btn = NULL;
    mailbox_compose_screen = NULL;
    compose_recipient = "";
    compose_input_focused = false;
    mailbox_compose_focusable_count = 0;
    memset(mailbox_compose_focusable, 0, sizeof(mailbox_compose_focusable));
}

// ============================================
// Cleanup Functions
// ============================================

void cleanupMailboxLinkScreen() {
    Serial.println("[Mailbox] cleanupMailboxLinkScreen called");
    if (mailbox_link_timer) {
        Serial.println("[Mailbox] Deleting link timer (cleanup)");
        lv_timer_del(mailbox_link_timer);
        mailbox_link_timer = NULL;
    }
    mailbox_code_label = NULL;
    mailbox_url_label = NULL;
    mailbox_status_label = NULL;
    mailbox_timer_label = NULL;
    mailbox_link_screen = NULL;
}

// ============================================
// Mode Handler Integration
// ============================================

/*
 * Handle Morse Mailbox mode navigation
 * Called from main mode handler in vail-summit.ino
 */
bool handleMailboxMode(int mode) {
    lv_obj_t* screen = NULL;

    switch (mode) {
        case MODE_MORSE_MAILBOX:
            // Route to link or inbox based on account state
            if (isMailboxLinked()) {
                screen = createMailboxInboxScreen();
            } else {
                screen = createMailboxLinkScreen();
            }
            break;

        case MODE_MORSE_MAILBOX_LINK:
            cleanupMailboxLinkScreen();
            screen = createMailboxLinkScreen();
            break;

        case MODE_MORSE_MAILBOX_INBOX:
            screen = createMailboxInboxScreen();
            break;

        case MODE_MORSE_MAILBOX_PLAYBACK:
            cleanupMailboxPlayback();
            screen = createMailboxPlaybackScreen();
            break;

        case MODE_MORSE_MAILBOX_COMPOSE:
            cleanupMailboxCompose();
            screen = createMailboxComposeScreen();
            break;

        case MODE_MORSE_MAILBOX_ACCOUNT:
            screen = createMailboxAccountScreen();
            break;

        default:
            return false;  // Not a mailbox mode
    }

    if (screen) {
        loadScreen(screen, SCREEN_ANIM_FADE);
        return true;
    }

    return false;
}

#endif // LV_MAILBOX_SCREENS_H
