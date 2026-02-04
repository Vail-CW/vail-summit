#ifndef LV_MORSE_NOTES_SCREENS_H
#define LV_MORSE_NOTES_SCREENS_H

#include "lv_screen_manager.h"
#include "../core/config.h"
#include "../morse_notes/morse_notes_types.h"
#include "../morse_notes/morse_notes_storage.h"
#include "../morse_notes/morse_notes_recorder.h"
#include "../morse_notes/morse_notes_playback.h"
#include "../keyer/keyer.h"

// Forward declarations
void onLVGLMenuSelect(int menuItem);
void getPaddleState(bool* dit, bool* dah);  // From task_manager.h

// ===================================
// MORSE NOTES - LVGL UI SCREENS
// ===================================

// Screen state variables
static lv_obj_t* mnLibraryScreen = nullptr;
static lv_obj_t* mnRecordScreen = nullptr;
static lv_obj_t* mnPlaybackScreen = nullptr;
static lv_obj_t* mnSettingsScreen = nullptr;

// Library screen widgets
static lv_obj_t* mnLibraryHeaderBtns[2] = {nullptr, nullptr};
static int mnLibraryHeaderBtnCount = 2;
static lv_obj_t* mnLibraryItems[MN_MAX_RECORDINGS];
static int mnLibraryItemCount = 0;
static lv_obj_t* mnLibraryList = nullptr;
static unsigned long mnSelectedRecordingId = 0;

// Record screen widgets
static lv_obj_t* mnRecordBtn = nullptr;
static lv_obj_t* mnRecordStopBtn = nullptr;
static lv_obj_t* mnRecordDurationLabel = nullptr;
static lv_obj_t* mnRecordStatsLabel = nullptr;
static lv_obj_t* mnRecordActivityBar = nullptr;
static lv_obj_t* mnRecordControlRow = nullptr;
static lv_timer_t* mnRecordTimer = nullptr;

// Keyer state for recording
static StraightKeyer* mnRecordKeyer = nullptr;
static bool mnRecordDitPressed = false;
static bool mnRecordDahPressed = false;

// External CW settings
extern int cwTone;
extern int cwSpeed;
extern int getCwKeyTypeAsInt();  // From vail-summit.ino

// Save dialog widgets
static lv_obj_t* mnSaveDialog = nullptr;
static lv_obj_t* mnSaveTitleInput = nullptr;
static lv_obj_t* mnSavePreviewBtn = nullptr;
static lv_obj_t* mnSaveSaveBtn = nullptr;
static lv_obj_t* mnSaveDiscardBtn = nullptr;
static lv_timer_t* mnSavePreviewTimer = nullptr;
static bool mnSavePreviewPlaying = false;

// Playback screen widgets
static lv_obj_t* mnPlaybackBtns[3] = {nullptr, nullptr, nullptr};
static int mnPlaybackBtnCount = 3;
static lv_obj_t* mnPlaybackPlayBtn = nullptr;
static lv_obj_t* mnPlaybackSpeedBtn = nullptr;
static lv_obj_t* mnPlaybackDeleteBtn = nullptr;
static lv_obj_t* mnPlaybackProgressBar = nullptr;
static lv_obj_t* mnPlaybackTimeLabel = nullptr;
static lv_obj_t* mnPlaybackSpeedLabel = nullptr;
static lv_timer_t* mnPlaybackTimer = nullptr;

// External keyer callback registration
extern void (*morseNotesKeyCallback)(bool keyDown, unsigned long timestamp);

// ===================================
// LIBRARY SCREEN - NAVIGATION HANDLERS
// ===================================

/**
 * Header navigation (Settings / +New buttons)
 */
static void mnLibraryHeaderNavHandler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* target = lv_event_get_target(e);

    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    // Block TAB
    if (key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }

    // Find current button index
    int currentIndex = -1;
    for (int i = 0; i < mnLibraryHeaderBtnCount; i++) {
        if (mnLibraryHeaderBtns[i] == target) {
            currentIndex = i;
            break;
        }
    }

    if (currentIndex < 0) return;

    // LEFT/RIGHT navigation
    if (key == LV_KEY_LEFT && currentIndex > 0) {
        lv_group_focus_obj(mnLibraryHeaderBtns[currentIndex - 1]);
        lv_event_stop_processing(e);
    }
    else if (key == LV_KEY_RIGHT && currentIndex < mnLibraryHeaderBtnCount - 1) {
        lv_group_focus_obj(mnLibraryHeaderBtns[currentIndex + 1]);
        lv_event_stop_processing(e);
    }
    // DOWN to first list item
    else if (key == LV_KEY_DOWN && mnLibraryItemCount > 0) {
        lv_group_focus_obj(mnLibraryItems[0]);
        lv_obj_scroll_to_view(mnLibraryItems[0], LV_ANIM_ON);
        lv_event_stop_processing(e);
    }
}

/**
 * List navigation
 */
static void mnLibraryListNavHandler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* target = lv_event_get_target(e);

    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    // Block TAB
    if (key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }

    // Block LEFT/RIGHT (vertical list only)
    if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        lv_event_stop_processing(e);
        return;
    }

    // Find current item index
    int currentIndex = -1;
    for (int i = 0; i < mnLibraryItemCount; i++) {
        if (mnLibraryItems[i] == target) {
            currentIndex = i;
            break;
        }
    }

    if (currentIndex < 0) return;

    // UP from first item goes to last header button
    if (key == LV_KEY_UP && currentIndex == 0) {
        lv_group_focus_obj(mnLibraryHeaderBtns[mnLibraryHeaderBtnCount - 1]);
        lv_event_stop_processing(e);
    }
}

/**
 * List item click handler
 */
static void mnLibraryItemClick(lv_event_t* e) {
    lv_obj_t* item = lv_event_get_target(e);
    unsigned long id = (unsigned long)(intptr_t)lv_obj_get_user_data(item);

    mnSelectedRecordingId = id;
    Serial.printf("[MorseNotes] Selected recording: %lu\n", id);

    // Navigate to playback screen
    onLVGLMenuSelect(167);  // LVGL_MODE_MORSE_NOTES_PLAYBACK
}

// ===================================
// LIBRARY SCREEN - CREATION
// ===================================

/**
 * Format timestamp as readable date
 */
static String formatTimestamp(unsigned long timestamp) {
    struct tm timeinfo;
    time_t ts = (time_t)timestamp;
    localtime_r(&ts, &timeinfo);

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%b %d, %Y %I:%M %p", &timeinfo);
    return String(buffer);
}

/**
 * Create library screen
 */
lv_obj_t* createMorseNotesLibraryScreen() {
    clearNavigationGroup();

    // Load library
    if (!mnLoadLibrary()) {
        Serial.println("[MorseNotes] ERROR: Failed to load library");
    }

    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    mnLibraryScreen = screen;

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, 480, 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Morse Notes Library");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Settings button
    lv_obj_t* settings_btn = lv_btn_create(header);
    lv_obj_set_size(settings_btn, 45, 35);
    lv_obj_align(settings_btn, LV_ALIGN_RIGHT_MID, -100, 0);
    lv_obj_t* settings_lbl = lv_label_create(settings_btn);
    lv_label_set_text(settings_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_center(settings_lbl);
    lv_obj_add_event_cb(settings_btn, [](lv_event_t* e) {
        onLVGLMenuSelect(168);  // LVGL_MODE_MORSE_NOTES_SETTINGS
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(settings_btn, mnLibraryHeaderNavHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(settings_btn);
    mnLibraryHeaderBtns[0] = settings_btn;

    // New Recording button
    lv_obj_t* new_btn = lv_btn_create(header);
    lv_obj_set_size(new_btn, 100, 35);
    lv_obj_align(new_btn, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(new_btn, lv_color_hex(0x00aa00), 0);
    lv_obj_t* new_lbl = lv_label_create(new_btn);
    lv_label_set_text(new_lbl, LV_SYMBOL_PLUS " New");
    lv_obj_center(new_lbl);
    lv_obj_add_event_cb(new_btn, [](lv_event_t* e) {
        onLVGLMenuSelect(166);  // LVGL_MODE_MORSE_NOTES_RECORD
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(new_btn, mnLibraryHeaderNavHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(new_btn);
    mnLibraryHeaderBtns[1] = new_btn;

    // List container
    lv_obj_t* list = lv_obj_create(screen);
    lv_obj_set_size(list, 460, 160);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 8, 0);
    mnLibraryList = list;

    int count = mnGetLibraryCount();
    mnLibraryItemCount = 0;

    if (count == 0) {
        lv_obj_t* empty = lv_label_create(list);
        lv_label_set_text(empty, "No recordings yet.\nPress +New to start.");
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(empty);
    } else {
        // Create recording items
        for (int i = 0; i < count && i < MN_MAX_RECORDINGS; i++) {
            MorseNoteMetadata* meta = mnGetMetadataByIndex(i);
            if (!meta) continue;

            lv_obj_t* item = lv_btn_create(list);
            lv_obj_set_size(item, 440, 70);
            lv_obj_set_style_bg_color(item, lv_color_hex(0x2a2a2a), 0);
            lv_obj_set_style_bg_color(item, lv_color_hex(0x3a5a7a), LV_STATE_FOCUSED);
            lv_obj_set_style_radius(item, 8, 0);

            // Store metadata ID
            lv_obj_set_user_data(item, (void*)(intptr_t)meta->id);

            // Layout
            lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_hor(item, 15, 0);

            // Icon
            lv_obj_t* icon = lv_label_create(item);
            lv_label_set_text(icon, LV_SYMBOL_AUDIO);
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_22, 0);
            lv_obj_set_style_text_color(icon, lv_color_hex(0x00aaff), 0);

            // Text column
            lv_obj_t* col = lv_obj_create(item);
            lv_obj_set_size(col, 300, LV_SIZE_CONTENT);
            lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(col, 0, 0);
            lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_all(col, 0, 0);

            // Title
            lv_obj_t* title_lbl = lv_label_create(col);
            lv_label_set_text(title_lbl, meta->title);

            // Info line
            char info[80];
            int mins = meta->durationMs / 60000;
            int secs = (meta->durationMs / 1000) % 60;
            String dateStr = formatTimestamp(meta->timestamp);
            snprintf(info, sizeof(info), "%s  •  %dm %02ds  •  %.0f WPM",
                     dateStr.c_str(), mins, secs, meta->avgWPM);
            lv_obj_t* info_lbl = lv_label_create(col);
            lv_label_set_text(info_lbl, info);
            lv_obj_set_style_text_color(info_lbl, lv_color_hex(0x888888), 0);

            // Click handler
            lv_obj_add_event_cb(item, mnLibraryItemClick, LV_EVENT_CLICKED, nullptr);
            lv_obj_add_event_cb(item, mnLibraryListNavHandler, LV_EVENT_KEY, nullptr);
            addNavigableWidget(item);
            mnLibraryItems[mnLibraryItemCount++] = item;
        }
    }

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, 480, 30);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(footer, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hint = lv_label_create(footer);
    lv_label_set_text(hint, "ESC Back  |  ENTER Select  |  " LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate");
    lv_obj_center(hint);

    return screen;
}

// ===================================
// RECORD SCREEN - KEYER CALLBACK
// ===================================

/**
 * Keyer callback for morse notes recording
 * Called by the keyer when tx state changes
 */
static void mnRecordKeyerCallback(bool txOn, int element) {
    unsigned long currentTime = millis();

    // Forward to morse notes recorder
    mnKeyerCallback(txOn, currentTime);
}

// ===================================
// RECORD SCREEN - TIMER CALLBACK
// ===================================

/**
 * Recording timer callback (20ms for responsive keying)
 */
static void mnRecordTimerCb(lv_timer_t* timer) {
    if (!mnRecordScreen || !lv_obj_is_valid(mnRecordScreen)) {
        return;
    }

    // Always tick the keyer when recording is active
    if (mnIsRecording() && mnRecordKeyer) {
        // Get paddle state
        bool newDitPressed = false;
        bool newDahPressed = false;
        getPaddleState(&newDitPressed, &newDahPressed);

        // Feed paddle state to keyer
        if (newDitPressed != mnRecordDitPressed) {
            mnRecordKeyer->key(PADDLE_DIT, newDitPressed);
            mnRecordDitPressed = newDitPressed;
        }
        if (newDahPressed != mnRecordDahPressed) {
            mnRecordKeyer->key(PADDLE_DAH, newDahPressed);
            mnRecordDahPressed = newDahPressed;
        }

        // Tick the keyer state machine
        mnRecordKeyer->tick(millis());
    }

    // Update UI every ~100ms (every 5th call at 20ms interval)
    static int uiUpdateCounter = 0;
    if (++uiUpdateCounter < 5) return;
    uiUpdateCounter = 0;

    if (!mnIsRecording()) return;

    // Update duration
    char durBuf[32];
    mnGetRecordingDurationString(durBuf, sizeof(durBuf));
    lv_label_set_text(mnRecordDurationLabel, durBuf);

    // Update stats
    char statsBuf[64];
    mnGetRecordingStats(statsBuf, sizeof(statsBuf));
    lv_label_set_text(mnRecordStatsLabel, statsBuf);

    // Activity bar: pulse when key is down
    int activity = mnIsKeyDown() ? 100 : 0;
    lv_bar_set_value(mnRecordActivityBar, activity, LV_ANIM_ON);

    // Check for warning
    if (mnShouldShowRecordingWarning()) {
        // Could show toast here
    }
}

// ===================================
// RECORD SCREEN - SAVE DIALOG HANDLERS
// ===================================

// Forward declaration
static void mnStopPreview();

/**
 * Save dialog preview timer callback
 */
static void mnSavePreviewTimerCb(lv_timer_t* timer) {
    if (!mnSavePreviewPlaying) {
        return;
    }

    // Update playback
    mnUpdatePlayback();

    // Check if complete
    if (mnIsPlaybackComplete()) {
        mnStopPreview();
    }
}

/**
 * Stop preview playback
 */
static void mnStopPreview() {
    mnSavePreviewPlaying = false;
    mnStopPlayback();

    // Update button label
    if (mnSavePreviewBtn) {
        lv_obj_t* lbl = lv_obj_get_child(mnSavePreviewBtn, 0);
        if (lbl) lv_label_set_text(lbl, LV_SYMBOL_PLAY " Preview");
    }

    // Delete timer
    if (mnSavePreviewTimer) {
        lv_timer_del(mnSavePreviewTimer);
        mnSavePreviewTimer = nullptr;
    }
}

/**
 * Save dialog - preview button click
 */
static void mnPreviewBtnClick(lv_event_t* e) {
    if (mnSavePreviewPlaying) {
        // Stop preview
        mnStopPreview();
    } else {
        // Start preview
        float* buffer = mnGetRecordingTimingBuffer();
        int eventCount = mnGetRecordingEventCount();

        if (mnInitPreviewPlayback(buffer, eventCount, cwTone)) {
            if (mnStartPlayback()) {
                mnSavePreviewPlaying = true;

                // Update button label
                lv_obj_t* lbl = lv_obj_get_child(mnSavePreviewBtn, 0);
                if (lbl) lv_label_set_text(lbl, LV_SYMBOL_STOP " Stop");

                // Create timer for playback updates
                if (!mnSavePreviewTimer) {
                    mnSavePreviewTimer = lv_timer_create(mnSavePreviewTimerCb, 50, nullptr);
                }
            }
        }
    }
}

/**
 * Save dialog - save button click
 */
static void mnSaveBtnClick(lv_event_t* e) {
    // Stop preview if playing
    if (mnSavePreviewPlaying) {
        mnStopPreview();
    }

    const char* title = lv_textarea_get_text(mnSaveTitleInput);

    if (mnSaveRecording(title)) {
        // Delete save dialog
        if (mnSaveDialog) {
            lv_obj_del(mnSaveDialog);
            mnSaveDialog = nullptr;
        }

        // Return to library
        onLVGLMenuSelect(165);  // LVGL_MODE_MORSE_NOTES_LIBRARY
    } else {
        Serial.println("[MorseNotes] ERROR: Failed to save recording");
    }
}

/**
 * Save dialog - discard button click
 */
static void mnDiscardBtnClick(lv_event_t* e) {
    // Stop preview if playing
    if (mnSavePreviewPlaying) {
        mnStopPreview();
    }

    mnDiscardRecording();

    // Delete save dialog
    if (mnSaveDialog) {
        lv_obj_del(mnSaveDialog);
        mnSaveDialog = nullptr;
    }

    // Return to library
    onLVGLMenuSelect(165);  // LVGL_MODE_MORSE_NOTES_LIBRARY
}

/**
 * Save dialog text input key handler - DOWN moves to buttons
 */
static void mnSaveInputKeyHandler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    // DOWN arrow moves to preview button
    if (key == LV_KEY_DOWN) {
        lv_group_focus_obj(mnSavePreviewBtn);
        lv_event_stop_processing(e);
        return;
    }

    // Block TAB
    if (key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }
}

/**
 * Save dialog button navigation handler
 */
static void mnSaveDialogBtnNavHandler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);
    lv_obj_t* target = lv_event_get_target(e);

    // Block TAB
    if (key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }

    // UP arrow moves to text input
    if (key == LV_KEY_UP) {
        lv_group_focus_obj(mnSaveTitleInput);
        lv_event_stop_processing(e);
        return;
    }

    // LEFT/RIGHT navigation between buttons
    if (key == LV_KEY_LEFT) {
        if (target == mnSaveSaveBtn) {
            lv_group_focus_obj(mnSavePreviewBtn);
        } else if (target == mnSaveDiscardBtn) {
            lv_group_focus_obj(mnSaveSaveBtn);
        }
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_RIGHT) {
        if (target == mnSavePreviewBtn) {
            lv_group_focus_obj(mnSaveSaveBtn);
        } else if (target == mnSaveSaveBtn) {
            lv_group_focus_obj(mnSaveDiscardBtn);
        }
        lv_event_stop_processing(e);
        return;
    }
}

/**
 * Show save dialog
 */
static void mnShowSaveDialog() {
    if (mnSaveDialog) return;  // Already showing

    mnSavePreviewPlaying = false;

    // Create modal dialog (taller to accommodate 3 buttons)
    mnSaveDialog = lv_obj_create(mnRecordScreen);
    lv_obj_set_size(mnSaveDialog, 420, 180);
    lv_obj_center(mnSaveDialog);
    lv_obj_set_style_bg_color(mnSaveDialog, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(mnSaveDialog, 2, 0);
    lv_obj_set_style_border_color(mnSaveDialog, lv_color_hex(0x00aaff), 0);
    lv_obj_clear_flag(mnSaveDialog, LV_OBJ_FLAG_SCROLLABLE);

    // Prompt
    lv_obj_t* prompt = lv_label_create(mnSaveDialog);
    lv_label_set_text(prompt, "Enter recording title:");
    lv_obj_align(prompt, LV_ALIGN_TOP_MID, 0, 15);

    // Title input
    mnSaveTitleInput = lv_textarea_create(mnSaveDialog);
    lv_textarea_set_one_line(mnSaveTitleInput, true);
    lv_textarea_set_max_length(mnSaveTitleInput, 60);
    lv_obj_set_size(mnSaveTitleInput, 380, 40);
    lv_obj_align(mnSaveTitleInput, LV_ALIGN_TOP_MID, 0, 45);
    lv_obj_add_event_cb(mnSaveTitleInput, mnSaveInputKeyHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(mnSaveTitleInput);

    // Generate default title
    char defaultTitle[64];
    time_t now = time(nullptr);
    mnGenerateDefaultTitle((unsigned long)now, defaultTitle, sizeof(defaultTitle));
    lv_textarea_set_text(mnSaveTitleInput, defaultTitle);

    // Button row container
    lv_obj_t* btn_row = lv_obj_create(mnSaveDialog);
    lv_obj_set_size(btn_row, 400, 50);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    // Preview button (blue/purple)
    mnSavePreviewBtn = lv_btn_create(btn_row);
    lv_obj_set_size(mnSavePreviewBtn, 110, 40);
    lv_obj_set_style_bg_color(mnSavePreviewBtn, lv_color_hex(0x6644aa), 0);
    lv_obj_t* preview_lbl = lv_label_create(mnSavePreviewBtn);
    lv_label_set_text(preview_lbl, LV_SYMBOL_PLAY " Preview");
    lv_obj_center(preview_lbl);
    lv_obj_add_event_cb(mnSavePreviewBtn, mnPreviewBtnClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(mnSavePreviewBtn, mnSaveDialogBtnNavHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(mnSavePreviewBtn);

    // Save button (green)
    mnSaveSaveBtn = lv_btn_create(btn_row);
    lv_obj_set_size(mnSaveSaveBtn, 110, 40);
    lv_obj_set_style_bg_color(mnSaveSaveBtn, lv_color_hex(0x00aa00), 0);
    lv_obj_t* save_lbl = lv_label_create(mnSaveSaveBtn);
    lv_label_set_text(save_lbl, LV_SYMBOL_SAVE " Save");
    lv_obj_center(save_lbl);
    lv_obj_add_event_cb(mnSaveSaveBtn, mnSaveBtnClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(mnSaveSaveBtn, mnSaveDialogBtnNavHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(mnSaveSaveBtn);

    // Discard button (red)
    mnSaveDiscardBtn = lv_btn_create(btn_row);
    lv_obj_set_size(mnSaveDiscardBtn, 110, 40);
    lv_obj_set_style_bg_color(mnSaveDiscardBtn, lv_color_hex(0xaa0000), 0);
    lv_obj_t* discard_lbl = lv_label_create(mnSaveDiscardBtn);
    lv_label_set_text(discard_lbl, LV_SYMBOL_TRASH " Discard");
    lv_obj_center(discard_lbl);
    lv_obj_add_event_cb(mnSaveDiscardBtn, mnDiscardBtnClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(mnSaveDiscardBtn, mnSaveDialogBtnNavHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(mnSaveDiscardBtn);

    // Focus on input
    lv_group_focus_obj(mnSaveTitleInput);
}

/**
 * REC button click
 */
static void mnRecBtnClick(lv_event_t* e) {
    if (mnStartRecording()) {
        // Initialize keyer for recording
        int ditDuration = DIT_DURATION(cwSpeed);
        mnRecordKeyer = getKeyer(getCwKeyTypeAsInt());
        mnRecordKeyer->reset();
        mnRecordKeyer->setDitDuration(ditDuration);
        mnRecordKeyer->setTxCallback(mnRecordKeyerCallback);
        mnRecordDitPressed = false;
        mnRecordDahPressed = false;

        // Hide REC button, show controls
        lv_obj_add_flag(mnRecordBtn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(mnRecordControlRow, LV_OBJ_FLAG_HIDDEN);

        // Focus on stop button
        lv_group_focus_obj(mnRecordStopBtn);

        Serial.println("[MorseNotes] Recording started with keyer");
    }
}

/**
 * Stop recording helper (shared between button click and key handler)
 */
static void mnDoStopRecording() {
    if (mnStopRecording()) {
        // Clean up keyer
        mnRecordKeyer = nullptr;

        // Show save dialog
        mnShowSaveDialog();
    }
}

/**
 * STOP button click
 */
static void mnStopBtnClick(lv_event_t* e) {
    mnDoStopRecording();
}

/**
 * Discard recording and exit (for ESC)
 */
static void mnDoDiscardAndExit() {
    // Stop recording if active
    if (mnIsRecording()) {
        mnStopRecording();
    }
    mnDiscardRecording();

    // Clean up keyer
    mnRecordKeyer = nullptr;

    // Return to library
    onLVGLMenuSelect(165);  // LVGL_MODE_MORSE_NOTES_LIBRARY
}

/**
 * Record screen key handler for ENTER and ESC
 */
static void mnRecordKeyHandler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    // ENTER stops recording (when recording is active)
    if (key == LV_KEY_ENTER && mnIsRecording()) {
        mnDoStopRecording();
        lv_event_stop_processing(e);
        return;
    }

    // ESC discards and exits (when recording is active and no save dialog)
    if (key == LV_KEY_ESC && mnIsRecording() && !mnSaveDialog) {
        mnDoDiscardAndExit();
        lv_event_stop_processing(e);
        return;
    }

    // Block TAB
    if (key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }
}

// ===================================
// RECORD SCREEN - CREATION
// ===================================

/**
 * Create record screen
 */
lv_obj_t* createMorseNotesRecordScreen() {
    clearNavigationGroup();

    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    mnRecordScreen = screen;

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, 480, 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_LEFT " New Recording");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Main content
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, 440, 190);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 12, 0);

    // Instructions
    lv_obj_t* instructions = lv_label_create(content);
    lv_label_set_text(instructions, "Press REC to start recording");
    lv_obj_set_style_text_color(instructions, lv_color_hex(0x888888), 0);

    // REC button (big, red, round)
    mnRecordBtn = lv_btn_create(content);
    lv_obj_set_size(mnRecordBtn, 120, 60);
    lv_obj_set_style_bg_color(mnRecordBtn, lv_color_hex(0xcc0000), 0);
    lv_obj_set_style_bg_color(mnRecordBtn, lv_color_hex(0xff3333), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(mnRecordBtn, 30, 0);

    lv_obj_t* rec_lbl = lv_label_create(mnRecordBtn);
    lv_label_set_text(rec_lbl, LV_SYMBOL_STOP " REC");
    lv_obj_set_style_text_font(rec_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(rec_lbl);

    lv_obj_add_event_cb(mnRecordBtn, mnRecBtnClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(mnRecordBtn, mnRecordKeyHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(mnRecordBtn);

    // Control row (hidden initially)
    mnRecordControlRow = lv_obj_create(content);
    lv_obj_set_size(mnRecordControlRow, 300, 50);
    lv_obj_set_style_bg_opa(mnRecordControlRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mnRecordControlRow, 0, 0);
    lv_obj_set_flex_flow(mnRecordControlRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mnRecordControlRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(mnRecordControlRow, LV_OBJ_FLAG_HIDDEN);

    // STOP button
    mnRecordStopBtn = lv_btn_create(mnRecordControlRow);
    lv_obj_set_size(mnRecordStopBtn, 120, 45);
    lv_obj_set_style_bg_color(mnRecordStopBtn, lv_color_hex(0xcc0000), 0);
    lv_obj_t* stop_lbl = lv_label_create(mnRecordStopBtn);
    lv_label_set_text(stop_lbl, LV_SYMBOL_STOP " STOP");
    lv_obj_center(stop_lbl);
    lv_obj_add_event_cb(mnRecordStopBtn, mnStopBtnClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(mnRecordStopBtn, mnRecordKeyHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(mnRecordStopBtn);

    // Duration label
    mnRecordDurationLabel = lv_label_create(content);
    lv_label_set_text(mnRecordDurationLabel, "00:00 / 05:00");
    lv_obj_set_style_text_font(mnRecordDurationLabel, &lv_font_montserrat_22, 0);

    // Activity bar
    mnRecordActivityBar = lv_bar_create(content);
    lv_obj_set_size(mnRecordActivityBar, 350, 20);
    lv_bar_set_range(mnRecordActivityBar, 0, 100);
    lv_bar_set_value(mnRecordActivityBar, 0, LV_ANIM_OFF);

    // Stats label
    mnRecordStatsLabel = lv_label_create(content);
    lv_label_set_text(mnRecordStatsLabel, "0 events  •  0 WPM avg");
    lv_obj_set_style_text_color(mnRecordStatsLabel, lv_color_hex(0x888888), 0);

    // Create timer (20ms interval for responsive keying)
    mnRecordTimer = lv_timer_create(mnRecordTimerCb, 20, nullptr);

    return screen;
}

/**
 * Cleanup record screen
 */
void cleanupMorseNotesRecordScreen() {
    // Stop preview if playing
    if (mnSavePreviewPlaying) {
        mnStopPreview();
    }

    // Stop recording if active and discard
    if (mnIsRecording()) {
        mnStopRecording();
    }
    mnDiscardRecording();

    // Clean up keyer
    mnRecordKeyer = nullptr;

    // Delete record timer
    if (mnRecordTimer) {
        lv_timer_del(mnRecordTimer);
        mnRecordTimer = nullptr;
    }

    // Delete preview timer
    if (mnSavePreviewTimer) {
        lv_timer_del(mnSavePreviewTimer);
        mnSavePreviewTimer = nullptr;
    }

    // Delete save dialog if open
    if (mnSaveDialog) {
        lv_obj_del(mnSaveDialog);
        mnSaveDialog = nullptr;
    }

    mnRecordScreen = nullptr;
}

// ===================================
// PLAYBACK SCREEN - TIMER CALLBACK
// ===================================

/**
 * Playback timer callback (50ms updates)
 */
static void mnPlaybackTimerCb(lv_timer_t* timer) {
    if (!mnIsPlaying() || !mnPlaybackScreen || !lv_obj_is_valid(mnPlaybackScreen)) {
        if (timer) lv_timer_del(timer);
        mnPlaybackTimer = nullptr;
        return;
    }

    // Update playback
    mnUpdatePlayback();

    // Update progress bar
    float progress = mnGetPlaybackProgress();
    lv_bar_set_value(mnPlaybackProgressBar, (int)(progress * 100), LV_ANIM_OFF);

    // Update time label
    char timeBuf[32];
    mnGetPlaybackTimeString(timeBuf, sizeof(timeBuf));
    lv_label_set_text(mnPlaybackTimeLabel, timeBuf);

    // Check if complete
    if (mnIsPlaybackComplete()) {
        lv_label_set_text(lv_obj_get_child(mnPlaybackPlayBtn, 0), LV_SYMBOL_REFRESH " Replay");
        lv_timer_del(timer);
        mnPlaybackTimer = nullptr;
    }
}

// ===================================
// PLAYBACK SCREEN - HANDLERS
// ===================================

/**
 * Play button click
 */
static void mnPlaybackPlayBtnClick(lv_event_t* e) {
    if (mnIsPlaying()) {
        mnStopPlayback();
        lv_label_set_text(lv_obj_get_child(mnPlaybackPlayBtn, 0), LV_SYMBOL_PLAY " Play");

        if (mnPlaybackTimer) {
            lv_timer_del(mnPlaybackTimer);
            mnPlaybackTimer = nullptr;
        }
    } else {
        if (mnStartPlayback()) {
            lv_label_set_text(lv_obj_get_child(mnPlaybackPlayBtn, 0), LV_SYMBOL_PAUSE " Pause");

            // Start playback timer
            if (!mnPlaybackTimer) {
                mnPlaybackTimer = lv_timer_create(mnPlaybackTimerCb, 50, nullptr);
            }
        }
    }
}

/**
 * Speed button key handler (UP/DOWN adjusts speed)
 */
static void mnPlaybackSpeedAdjust(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_UP || key == LV_KEY_DOWN) {
        mnCyclePlaybackSpeed(key == LV_KEY_UP);

        // Update label
        char speedBuf[16];
        mnFormatSpeed(mnGetPlaybackSpeed(), speedBuf, sizeof(speedBuf));
        char labelBuf[32];
        snprintf(labelBuf, sizeof(labelBuf), LV_SYMBOL_UP LV_SYMBOL_DOWN " %s", speedBuf);
        lv_label_set_text(mnPlaybackSpeedLabel, labelBuf);

        lv_event_stop_processing(e);
    }
}

/**
 * Delete button click
 */
static void mnPlaybackDeleteBtnClick(lv_event_t* e) {
    // Stop playback
    if (mnIsPlaying()) {
        mnStopPlayback();
    }

    // Delete recording
    if (mnDeleteRecording(mnSelectedRecordingId)) {
        Serial.println("[MorseNotes] Recording deleted");
        onLVGLMenuSelect(165);  // Return to library
    } else {
        Serial.println("[MorseNotes] ERROR: Failed to delete recording");
    }
}

/**
 * Playback navigation handler (LEFT/RIGHT between buttons)
 */
static void mnPlaybackNavHandler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    // Block TAB
    if (key == LV_KEY_NEXT) {
        lv_event_stop_processing(e);
        return;
    }

    // Find current button
    lv_obj_t* target = lv_event_get_target(e);
    int currentIndex = -1;
    for (int i = 0; i < mnPlaybackBtnCount; i++) {
        if (mnPlaybackBtns[i] == target) {
            currentIndex = i;
            break;
        }
    }

    if (currentIndex < 0) return;

    // LEFT/RIGHT navigation
    if (key == LV_KEY_LEFT && currentIndex > 0) {
        lv_group_focus_obj(mnPlaybackBtns[currentIndex - 1]);
        lv_event_stop_processing(e);
    }
    else if (key == LV_KEY_RIGHT && currentIndex < mnPlaybackBtnCount - 1) {
        lv_group_focus_obj(mnPlaybackBtns[currentIndex + 1]);
        lv_event_stop_processing(e);
    }
}

// ===================================
// PLAYBACK SCREEN - CREATION
// ===================================

/**
 * Create playback screen
 */
lv_obj_t* createMorseNotesPlaybackScreen() {
    clearNavigationGroup();

    // Load recording
    if (!mnLoadForPlayback(mnSelectedRecordingId)) {
        Serial.println("[MorseNotes] ERROR: Failed to load recording");
        return nullptr;
    }

    MorseNoteMetadata* meta = mnGetCurrentMetadata();
    if (!meta) {
        Serial.println("[MorseNotes] ERROR: No metadata");
        return nullptr;
    }

    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    mnPlaybackScreen = screen;

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, 480, 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    char headerTitle[80];
    snprintf(headerTitle, sizeof(headerTitle), "%s %.50s", LV_SYMBOL_LEFT, meta->title);
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, headerTitle);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Info card
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, 440, 80);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x2a2a2a), 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Info text
    char infoText[128];
    int mins = meta->durationMs / 60000;
    int secs = (meta->durationMs / 1000) % 60;
    String dateStr = formatTimestamp(meta->timestamp);
    snprintf(infoText, sizeof(infoText),
             "Date: %s\nDuration: %dm %02ds  •  WPM: %.1f",
             dateStr.c_str(), mins, secs, meta->avgWPM);
    lv_obj_t* info_lbl = lv_label_create(card);
    lv_label_set_text(info_lbl, infoText);
    lv_obj_center(info_lbl);

    // Progress bar
    mnPlaybackProgressBar = lv_bar_create(screen);
    lv_obj_set_size(mnPlaybackProgressBar, 440, 20);
    lv_obj_align(mnPlaybackProgressBar, LV_ALIGN_TOP_MID, 0, 145);
    lv_bar_set_range(mnPlaybackProgressBar, 0, 100);
    lv_bar_set_value(mnPlaybackProgressBar, 0, LV_ANIM_OFF);

    // Time label
    mnPlaybackTimeLabel = lv_label_create(screen);
    char timeBuf[32];
    mnGetPlaybackTimeString(timeBuf, sizeof(timeBuf));
    lv_label_set_text(mnPlaybackTimeLabel, timeBuf);
    lv_obj_align(mnPlaybackTimeLabel, LV_ALIGN_TOP_MID, 0, 170);

    // Control buttons
    lv_obj_t* controls = lv_obj_create(screen);
    lv_obj_set_size(controls, 440, 60);
    lv_obj_align(controls, LV_ALIGN_TOP_MID, 0, 195);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Play button
    mnPlaybackPlayBtn = lv_btn_create(controls);
    lv_obj_set_size(mnPlaybackPlayBtn, 120, 50);
    lv_obj_set_style_bg_color(mnPlaybackPlayBtn, lv_color_hex(0x00aa00), 0);
    lv_obj_t* play_lbl = lv_label_create(mnPlaybackPlayBtn);
    lv_label_set_text(play_lbl, LV_SYMBOL_PLAY " Play");
    lv_obj_center(play_lbl);
    lv_obj_add_event_cb(mnPlaybackPlayBtn, mnPlaybackPlayBtnClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(mnPlaybackPlayBtn, mnPlaybackNavHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(mnPlaybackPlayBtn);
    mnPlaybackBtns[0] = mnPlaybackPlayBtn;

    // Speed button
    mnPlaybackSpeedBtn = lv_btn_create(controls);
    lv_obj_set_size(mnPlaybackSpeedBtn, 120, 50);
    lv_obj_set_style_bg_color(mnPlaybackSpeedBtn, lv_color_hex(0x8800ff), 0);
    mnPlaybackSpeedLabel = lv_label_create(mnPlaybackSpeedBtn);
    lv_label_set_text(mnPlaybackSpeedLabel, LV_SYMBOL_UP LV_SYMBOL_DOWN " 1.00x");
    lv_obj_center(mnPlaybackSpeedLabel);
    lv_obj_add_event_cb(mnPlaybackSpeedBtn, mnPlaybackSpeedAdjust, LV_EVENT_KEY, nullptr);
    lv_obj_add_event_cb(mnPlaybackSpeedBtn, mnPlaybackNavHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(mnPlaybackSpeedBtn);
    mnPlaybackBtns[1] = mnPlaybackSpeedBtn;

    // Delete button
    mnPlaybackDeleteBtn = lv_btn_create(controls);
    lv_obj_set_size(mnPlaybackDeleteBtn, 120, 50);
    lv_obj_set_style_bg_color(mnPlaybackDeleteBtn, lv_color_hex(0xcc0000), 0);
    lv_obj_t* del_lbl = lv_label_create(mnPlaybackDeleteBtn);
    lv_label_set_text(del_lbl, LV_SYMBOL_TRASH " Delete");
    lv_obj_center(del_lbl);
    lv_obj_add_event_cb(mnPlaybackDeleteBtn, mnPlaybackDeleteBtnClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(mnPlaybackDeleteBtn, mnPlaybackNavHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(mnPlaybackDeleteBtn);
    mnPlaybackBtns[2] = mnPlaybackDeleteBtn;

    mnPlaybackBtnCount = 3;

    return screen;
}

/**
 * Cleanup playback screen
 */
void cleanupMorseNotesPlaybackScreen() {
    // Stop playback
    if (mnIsPlaying()) {
        mnStopPlayback();
    }

    // Delete timer
    if (mnPlaybackTimer) {
        lv_timer_del(mnPlaybackTimer);
        mnPlaybackTimer = nullptr;
    }

    mnPlaybackScreen = nullptr;
}

// ===================================
// SETTINGS SCREEN - CREATION
// ===================================

/**
 * Create settings screen (placeholder for v1)
 */
lv_obj_t* createMorseNotesSettingsScreen() {
    clearNavigationGroup();

    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), 0);
    mnSettingsScreen = screen;

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, 480, 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_LEFT " Morse Notes Settings");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Placeholder content
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, 440, 190);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);

    lv_obj_t* placeholder = lv_label_create(content);
    lv_label_set_text(placeholder, "Settings screen\n(To be implemented in Phase 2)");
    lv_obj_set_style_text_align(placeholder, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(placeholder);

    return screen;
}

#endif // LV_MORSE_NOTES_SCREENS_H

