#ifndef LV_MORSE_NOTES_SCREENS_H
#define LV_MORSE_NOTES_SCREENS_H

#include "lv_screen_manager.h"
#include "../core/config.h"
#include "../core/modes.h"
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
    onLVGLMenuSelect(MODE_MORSE_NOTES_PLAYBACK);
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

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    mnLibraryScreen = screen;

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_add_style(header, getStyleStatusBar(), 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Status bar (WiFi + battery)
    createCompactStatusBar(screen);

    // Title
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Morse Notes Library");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Settings button
    lv_obj_t* settings_btn = lv_btn_create(header);
    lv_obj_set_size(settings_btn, 45, 35);
    lv_obj_align(settings_btn, LV_ALIGN_RIGHT_MID, -100, 0);
    lv_obj_t* settings_lbl = lv_label_create(settings_btn);
    lv_label_set_text(settings_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_center(settings_lbl);
    lv_obj_add_event_cb(settings_btn, [](lv_event_t* e) {
        onLVGLMenuSelect(MODE_MORSE_NOTES_SETTINGS);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(settings_btn, mnLibraryHeaderNavHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(settings_btn);
    mnLibraryHeaderBtns[0] = settings_btn;

    // New Recording button
    lv_obj_t* new_btn = lv_btn_create(header);
    lv_obj_set_size(new_btn, 100, 35);
    lv_obj_align(new_btn, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(new_btn, LV_COLOR_SUCCESS, 0);
    lv_obj_t* new_lbl = lv_label_create(new_btn);
    lv_label_set_text(new_lbl, LV_SYMBOL_PLUS " New");
    lv_obj_center(new_lbl);
    lv_obj_add_event_cb(new_btn, [](lv_event_t* e) {
        onLVGLMenuSelect(MODE_MORSE_NOTES_RECORD);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(new_btn, mnLibraryHeaderNavHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(new_btn);
    mnLibraryHeaderBtns[1] = new_btn;

    // List container
    lv_obj_t* list = lv_obj_create(screen);
    lv_obj_set_size(list, 460, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 20);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 5);
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
            lv_obj_set_style_bg_color(item, LV_COLOR_BG_LAYER2, 0);
            lv_obj_set_style_bg_color(item, LV_COLOR_CARD_CYAN, LV_STATE_FOCUSED);
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
            lv_obj_set_style_text_font(icon, getThemeFonts()->font_large, 0);
            lv_obj_set_style_text_color(icon, LV_COLOR_ACCENT_CYAN, 0);

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
            lv_obj_set_style_text_color(info_lbl, LV_COLOR_TEXT_SECONDARY, 0);
            lv_obj_set_style_text_font(info_lbl, getThemeFonts()->font_small, 0);

            // Click handler
            lv_obj_add_event_cb(item, mnLibraryItemClick, LV_EVENT_CLICKED, nullptr);
            lv_obj_add_event_cb(item, mnLibraryListNavHandler, LV_EVENT_KEY, nullptr);
            addNavigableWidget(item);
            mnLibraryItems[mnLibraryItemCount++] = item;
        }
    }

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hint = lv_label_create(footer);
    lv_label_set_text(hint, FOOTER_NAV_ENTER_ESC);
    lv_obj_set_style_text_color(hint, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(hint, getThemeFonts()->font_small, 0);
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
        onLVGLMenuSelect(MODE_MORSE_NOTES_LIBRARY);
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
    onLVGLMenuSelect(MODE_MORSE_NOTES_LIBRARY);
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
    lv_obj_set_style_bg_color(mnSaveDialog, LV_COLOR_BG_LAYER2, 0);
    lv_obj_set_style_border_width(mnSaveDialog, 2, 0);
    lv_obj_set_style_border_color(mnSaveDialog, LV_COLOR_ACCENT_CYAN, 0);
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
    lv_obj_set_style_bg_color(mnSavePreviewBtn, LV_COLOR_ACCENT_MAGENTA, 0);
    lv_obj_t* preview_lbl = lv_label_create(mnSavePreviewBtn);
    lv_label_set_text(preview_lbl, LV_SYMBOL_PLAY " Preview");
    lv_obj_center(preview_lbl);
    lv_obj_add_event_cb(mnSavePreviewBtn, mnPreviewBtnClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(mnSavePreviewBtn, mnSaveDialogBtnNavHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(mnSavePreviewBtn);

    // Save button (green)
    mnSaveSaveBtn = lv_btn_create(btn_row);
    lv_obj_set_size(mnSaveSaveBtn, 110, 40);
    lv_obj_set_style_bg_color(mnSaveSaveBtn, LV_COLOR_SUCCESS, 0);
    lv_obj_t* save_lbl = lv_label_create(mnSaveSaveBtn);
    lv_label_set_text(save_lbl, LV_SYMBOL_SAVE " Save");
    lv_obj_center(save_lbl);
    lv_obj_add_event_cb(mnSaveSaveBtn, mnSaveBtnClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(mnSaveSaveBtn, mnSaveDialogBtnNavHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(mnSaveSaveBtn);

    // Discard button (red)
    mnSaveDiscardBtn = lv_btn_create(btn_row);
    lv_obj_set_size(mnSaveDiscardBtn, 110, 40);
    lv_obj_set_style_bg_color(mnSaveDiscardBtn, LV_COLOR_ERROR, 0);
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
    onLVGLMenuSelect(MODE_MORSE_NOTES_LIBRARY);
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

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    mnRecordScreen = screen;

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_add_style(header, getStyleStatusBar(), 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Status bar (WiFi + battery)
    createCompactStatusBar(screen);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_LEFT " New Recording");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Main content
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, 440, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 10);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 5);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 12, 0);

    // Current settings info
    const char* mnRecKeyTypeNames[] = {"Straight", "Iambic A", "Iambic B", "Ultimatic"};
    char settingsInfo[64];
    snprintf(settingsInfo, sizeof(settingsInfo), "%d WPM  •  %d Hz  •  %s",
             cwSpeed, cwTone, mnRecKeyTypeNames[getCwKeyTypeAsInt()]);
    lv_obj_t* settings_lbl = lv_label_create(content);
    lv_label_set_text(settings_lbl, settingsInfo);
    lv_obj_set_style_text_color(settings_lbl, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_set_style_text_font(settings_lbl, getThemeFonts()->font_small, 0);

    // Instructions
    lv_obj_t* instructions = lv_label_create(content);
    lv_label_set_text(instructions, "Press REC to start recording");
    lv_obj_set_style_text_color(instructions, LV_COLOR_TEXT_SECONDARY, 0);

    // REC button (big, red, round)
    mnRecordBtn = lv_btn_create(content);
    lv_obj_set_size(mnRecordBtn, 120, 60);
    lv_obj_set_style_bg_color(mnRecordBtn, LV_COLOR_ERROR, 0);
    lv_obj_set_style_bg_color(mnRecordBtn, LV_COLOR_ERROR, LV_STATE_FOCUSED);
    lv_obj_set_style_radius(mnRecordBtn, 30, 0);

    lv_obj_t* rec_lbl = lv_label_create(mnRecordBtn);
    lv_label_set_text(rec_lbl, LV_SYMBOL_STOP " REC");
    lv_obj_set_style_text_font(rec_lbl, getThemeFonts()->font_subtitle, 0);
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
    lv_obj_set_style_bg_color(mnRecordStopBtn, LV_COLOR_ERROR, 0);
    lv_obj_t* stop_lbl = lv_label_create(mnRecordStopBtn);
    lv_label_set_text(stop_lbl, LV_SYMBOL_STOP " STOP");
    lv_obj_center(stop_lbl);
    lv_obj_add_event_cb(mnRecordStopBtn, mnStopBtnClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(mnRecordStopBtn, mnRecordKeyHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(mnRecordStopBtn);

    // Duration label
    mnRecordDurationLabel = lv_label_create(content);
    lv_label_set_text(mnRecordDurationLabel, "00:00 / 05:00");
    lv_obj_set_style_text_font(mnRecordDurationLabel, getThemeFonts()->font_large, 0);

    // Activity bar
    mnRecordActivityBar = lv_bar_create(content);
    lv_obj_set_size(mnRecordActivityBar, 350, 20);
    lv_bar_set_range(mnRecordActivityBar, 0, 100);
    lv_bar_set_value(mnRecordActivityBar, 0, LV_ANIM_OFF);
    applyBarStyle(mnRecordActivityBar);

    // Stats label
    mnRecordStatsLabel = lv_label_create(content);
    lv_label_set_text(mnRecordStatsLabel, "0 events  •  0 WPM avg");
    lv_obj_set_style_text_color(mnRecordStatsLabel, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(mnRecordStatsLabel, getThemeFonts()->font_small, 0);

    // Footer
    lv_obj_t* rec_footer = lv_obj_create(screen);
    lv_obj_set_size(rec_footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(rec_footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(rec_footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rec_footer, 0, 0);
    lv_obj_clear_flag(rec_footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* rec_hint = lv_label_create(rec_footer);
    lv_label_set_text(rec_hint, "ENTER Stop   ESC Back");
    lv_obj_set_style_text_color(rec_hint, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(rec_hint, getThemeFonts()->font_small, 0);
    lv_obj_center(rec_hint);

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
static void mnPlaybackDeleteConfirm(lv_event_t* e) {
    if (mnDeleteRecording(mnSelectedRecordingId)) {
        Serial.println("[MorseNotes] Recording deleted");
        onLVGLMenuSelect(MODE_MORSE_NOTES_LIBRARY);
    } else {
        Serial.println("[MorseNotes] ERROR: Failed to delete recording");
    }
}

static void mnPlaybackDeleteBtnClick(lv_event_t* e) {
    // Stop playback
    if (mnIsPlaying()) {
        mnStopPlayback();
    }

    // Show confirmation dialog
    createConfirmDialog(
        "Delete Recording",
        "Are you sure you want to delete\nthis recording?",
        mnPlaybackDeleteConfirm,
        [](lv_event_t* e) {
            // Cancel: do nothing (dialog closes automatically)
        }
    );
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

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    mnPlaybackScreen = screen;

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_add_style(header, getStyleStatusBar(), 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Status bar (WiFi + battery)
    createCompactStatusBar(screen);

    char headerTitle[80];
    snprintf(headerTitle, sizeof(headerTitle), "%s %.50s", LV_SYMBOL_LEFT, meta->title);
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, headerTitle);
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Info card
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, 440, 80);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 5);
    applyCardStyle(card);
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
    lv_obj_set_style_text_font(info_lbl, getThemeFonts()->font_body, 0);
    lv_obj_set_style_text_color(info_lbl, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_center(info_lbl);

    // Progress bar
    int pbY = HEADER_HEIGHT + 90;
    mnPlaybackProgressBar = lv_bar_create(screen);
    lv_obj_set_size(mnPlaybackProgressBar, 440, 20);
    lv_obj_align(mnPlaybackProgressBar, LV_ALIGN_TOP_MID, 0, pbY);
    lv_bar_set_range(mnPlaybackProgressBar, 0, 100);
    lv_bar_set_value(mnPlaybackProgressBar, 0, LV_ANIM_OFF);
    applyBarStyle(mnPlaybackProgressBar);

    // Time label
    mnPlaybackTimeLabel = lv_label_create(screen);
    char timeBuf[32];
    mnGetPlaybackTimeString(timeBuf, sizeof(timeBuf));
    lv_label_set_text(mnPlaybackTimeLabel, timeBuf);
    lv_obj_set_style_text_font(mnPlaybackTimeLabel, getThemeFonts()->font_body, 0);
    lv_obj_align(mnPlaybackTimeLabel, LV_ALIGN_TOP_MID, 0, pbY + 25);

    // Control buttons
    lv_obj_t* controls = lv_obj_create(screen);
    lv_obj_set_size(controls, 440, 60);
    lv_obj_align(controls, LV_ALIGN_TOP_MID, 0, pbY + 50);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Play button
    mnPlaybackPlayBtn = lv_btn_create(controls);
    lv_obj_set_size(mnPlaybackPlayBtn, 120, 50);
    lv_obj_set_style_bg_color(mnPlaybackPlayBtn, LV_COLOR_SUCCESS, 0);
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
    lv_obj_set_style_bg_color(mnPlaybackSpeedBtn, LV_COLOR_ACCENT_MAGENTA, 0);
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
    lv_obj_set_style_bg_color(mnPlaybackDeleteBtn, LV_COLOR_ERROR, 0);
    lv_obj_t* del_lbl = lv_label_create(mnPlaybackDeleteBtn);
    lv_label_set_text(del_lbl, LV_SYMBOL_TRASH " Delete");
    lv_obj_center(del_lbl);
    lv_obj_add_event_cb(mnPlaybackDeleteBtn, mnPlaybackDeleteBtnClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(mnPlaybackDeleteBtn, mnPlaybackNavHandler, LV_EVENT_KEY, nullptr);
    addNavigableWidget(mnPlaybackDeleteBtn);
    mnPlaybackBtns[2] = mnPlaybackDeleteBtn;

    mnPlaybackBtnCount = 3;

    // Footer
    lv_obj_t* pb_footer = lv_obj_create(screen);
    lv_obj_set_size(pb_footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(pb_footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(pb_footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pb_footer, 0, 0);
    lv_obj_clear_flag(pb_footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* pb_hint = lv_label_create(pb_footer);
    lv_label_set_text(pb_hint, "L/R Navigate   UP/DN Speed   ESC Back");
    lv_obj_set_style_text_color(pb_hint, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(pb_hint, getThemeFonts()->font_small, 0);
    lv_obj_center(pb_hint);

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

// Extern declarations for settings functions
extern void saveCWSettings();
extern int getKeyAccelerationStep();
extern void onLVGLBackNavigation();
extern void setCwKeyTypeFromInt(int keyType);
extern void beep(int frequency, int duration);

// Static variables for Morse Notes settings screen
static lv_obj_t* mnSettingsFocusContainer = nullptr;
static lv_obj_t* mnSettingsSpeedRow = nullptr;
static lv_obj_t* mnSettingsToneRow = nullptr;
static lv_obj_t* mnSettingsKeyTypeRow = nullptr;
static lv_obj_t* mnSettingsSpeedSlider = nullptr;
static lv_obj_t* mnSettingsToneSlider = nullptr;
static lv_obj_t* mnSettingsSpeedValue = nullptr;
static lv_obj_t* mnSettingsToneValue = nullptr;
static lv_obj_t* mnSettingsKeyTypeValue = nullptr;
static int mnSettingsFocus = 0;  // 0=Speed, 1=Tone, 2=KeyType

// Musical note frequencies for tone snapping (chromatic scale 400-1175 Hz)
static const int mnSettingsNoteFreqs[] = {
    400, 415, 440, 466, 494, 523, 554, 587, 622, 659,
    698, 740, 784, 831, 880, 932, 988, 1047, 1109, 1175
};
static const int mnSettingsNoteCount = 20;

// Key type names for selector display
static const char* mnSettingsKeyTypeNames[] = {"Straight", "Iambic A", "Iambic B", "Ultimatic"};
static const int mnSettingsKeyTypeCount = 4;

// Update visual focus indicator for settings rows
static void mnSettingsUpdateFocus() {
    if (mnSettingsSpeedRow) {
        if (mnSettingsFocus == 0) {
            lv_obj_set_style_bg_color(mnSettingsSpeedRow, LV_COLOR_CARD_TEAL, 0);
            lv_obj_set_style_bg_opa(mnSettingsSpeedRow, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(mnSettingsSpeedRow, LV_COLOR_ACCENT_CYAN, 0);
            lv_obj_set_style_border_width(mnSettingsSpeedRow, 2, 0);
        } else {
            lv_obj_set_style_bg_opa(mnSettingsSpeedRow, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(mnSettingsSpeedRow, 0, 0);
        }
    }
    if (mnSettingsSpeedSlider) {
        if (mnSettingsFocus == 0) {
            lv_obj_add_state(mnSettingsSpeedSlider, LV_STATE_FOCUSED);
        } else {
            lv_obj_clear_state(mnSettingsSpeedSlider, LV_STATE_FOCUSED);
        }
    }
    if (mnSettingsToneRow) {
        if (mnSettingsFocus == 1) {
            lv_obj_set_style_bg_color(mnSettingsToneRow, LV_COLOR_CARD_TEAL, 0);
            lv_obj_set_style_bg_opa(mnSettingsToneRow, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(mnSettingsToneRow, LV_COLOR_ACCENT_CYAN, 0);
            lv_obj_set_style_border_width(mnSettingsToneRow, 2, 0);
        } else {
            lv_obj_set_style_bg_opa(mnSettingsToneRow, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(mnSettingsToneRow, 0, 0);
        }
    }
    if (mnSettingsToneSlider) {
        if (mnSettingsFocus == 1) {
            lv_obj_add_state(mnSettingsToneSlider, LV_STATE_FOCUSED);
        } else {
            lv_obj_clear_state(mnSettingsToneSlider, LV_STATE_FOCUSED);
        }
    }
    if (mnSettingsKeyTypeRow) {
        if (mnSettingsFocus == 2) {
            lv_obj_set_style_bg_color(mnSettingsKeyTypeRow, LV_COLOR_CARD_TEAL, 0);
            lv_obj_set_style_bg_opa(mnSettingsKeyTypeRow, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(mnSettingsKeyTypeRow, LV_COLOR_ACCENT_CYAN, 0);
            lv_obj_set_style_border_width(mnSettingsKeyTypeRow, 2, 0);
        } else {
            lv_obj_set_style_bg_opa(mnSettingsKeyTypeRow, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(mnSettingsKeyTypeRow, 0, 0);
        }
    }
    if (mnSettingsKeyTypeValue) {
        lv_obj_set_style_text_color(mnSettingsKeyTypeValue,
            mnSettingsFocus == 2 ? LV_COLOR_ACCENT_CYAN : LV_COLOR_TEXT_SECONDARY, 0);
    }
}

// Unified key handler for Morse Notes settings
static void mnSettingsKeyHandler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_NEXT || key == LV_KEY_PREV) {
        lv_event_stop_bubbling(e);
        return;
    }

    if (key == LV_KEY_ESC) {
        lv_event_stop_bubbling(e);
        onLVGLBackNavigation();
        return;
    }

    if (key == LV_KEY_UP) {
        lv_event_stop_bubbling(e);
        if (mnSettingsFocus > 0) {
            mnSettingsFocus--;
            mnSettingsUpdateFocus();
        }
        return;
    }
    if (key == LV_KEY_DOWN) {
        lv_event_stop_bubbling(e);
        if (mnSettingsFocus < 2) {
            mnSettingsFocus++;
            mnSettingsUpdateFocus();
        }
        return;
    }

    if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        lv_event_stop_bubbling(e);
        if (mnSettingsFocus == 0 && mnSettingsSpeedSlider) {
            int step = getKeyAccelerationStep();
            int delta = (key == LV_KEY_RIGHT) ? step : -step;
            int current = lv_slider_get_value(mnSettingsSpeedSlider);
            int new_val = current + delta;
            if (new_val < WPM_MIN) new_val = WPM_MIN;
            if (new_val > WPM_MAX) new_val = WPM_MAX;
            lv_slider_set_value(mnSettingsSpeedSlider, new_val, LV_ANIM_OFF);
            lv_event_send(mnSettingsSpeedSlider, LV_EVENT_VALUE_CHANGED, NULL);
        }
        else if (mnSettingsFocus == 1 && mnSettingsToneSlider) {
            int current = lv_slider_get_value(mnSettingsToneSlider);
            int new_val = current;
            int current_idx = 0;
            int minDiff = abs(current - mnSettingsNoteFreqs[0]);
            for (int i = 1; i < mnSettingsNoteCount; i++) {
                int diff = abs(current - mnSettingsNoteFreqs[i]);
                if (diff < minDiff) {
                    minDiff = diff;
                    current_idx = i;
                }
            }
            if (key == LV_KEY_RIGHT && current_idx < mnSettingsNoteCount - 1) {
                new_val = mnSettingsNoteFreqs[current_idx + 1];
            } else if (key == LV_KEY_LEFT && current_idx > 0) {
                new_val = mnSettingsNoteFreqs[current_idx - 1];
            }
            if (new_val != current) {
                lv_slider_set_value(mnSettingsToneSlider, new_val, LV_ANIM_OFF);
                lv_event_send(mnSettingsToneSlider, LV_EVENT_VALUE_CHANGED, NULL);
            }
        }
        else if (mnSettingsFocus == 2 && mnSettingsKeyTypeValue) {
            int current = getCwKeyTypeAsInt();
            if (key == LV_KEY_RIGHT) {
                current = (current + 1) % mnSettingsKeyTypeCount;
            } else {
                current = (current - 1 + mnSettingsKeyTypeCount) % mnSettingsKeyTypeCount;
            }
            lv_label_set_text_fmt(mnSettingsKeyTypeValue, "< %s >", mnSettingsKeyTypeNames[current]);
            setCwKeyTypeFromInt(current);
            saveCWSettings();
        }
        return;
    }

    if (key == LV_KEY_ENTER) {
        lv_event_stop_bubbling(e);
        return;
    }
}

static void mnSettingsSpeedEventCb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    cwSpeed = lv_slider_get_value(slider);
    if (mnSettingsSpeedValue != NULL) {
        lv_label_set_text_fmt(mnSettingsSpeedValue, "%d WPM", cwSpeed);
    }
    saveCWSettings();
    beep(cwTone, 100);
}

static void mnSettingsToneEventCb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    cwTone = lv_slider_get_value(slider);
    if (mnSettingsToneValue != NULL) {
        lv_label_set_text_fmt(mnSettingsToneValue, "%d Hz", cwTone);
    }
    saveCWSettings();
    beep(cwTone, 100);
}

/**
 * Create Morse Notes settings screen
 */
lv_obj_t* createMorseNotesSettingsScreen() {
    clearNavigationGroup();

    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    mnSettingsScreen = screen;

    // Title bar
    lv_obj_t* title_bar = lv_obj_create(screen);
    lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, "MORSE NOTES SETTINGS");
    lv_obj_add_style(title, getStyleLabelTitle(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    // Status bar (WiFi + battery) on the right side
    createCompactStatusBar(screen);

    // Content container
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_WIDTH - 40, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 20);
    lv_obj_set_pos(content, 20, HEADER_HEIGHT + 10);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 8, 0);
    lv_obj_set_style_pad_all(content, 10, 0);
    applyCardStyle(content);
    lv_obj_add_flag(content, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // Invisible focus container to receive all key events
    mnSettingsFocusContainer = lv_obj_create(content);
    lv_obj_set_size(mnSettingsFocusContainer, 0, 0);
    lv_obj_set_style_bg_opa(mnSettingsFocusContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mnSettingsFocusContainer, 0, 0);
    lv_obj_clear_flag(mnSettingsFocusContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mnSettingsFocusContainer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(mnSettingsFocusContainer, mnSettingsKeyHandler, LV_EVENT_KEY, NULL);
    addNavigableWidget(mnSettingsFocusContainer);

    // Put group in edit mode so UP/DOWN go to widget instead of LVGL group nav
    lv_group_t* group = getLVGLInputGroup();
    if (group != NULL) {
        lv_group_set_editing(group, true);
    }

    // Ensure focus is on our container
    lv_group_focus_obj(mnSettingsFocusContainer);

    // Reset focus state
    mnSettingsFocus = 0;

    // --- Speed setting row ---
    mnSettingsSpeedRow = lv_obj_create(content);
    lv_obj_set_size(mnSettingsSpeedRow, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(mnSettingsSpeedRow, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(mnSettingsSpeedRow, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(mnSettingsSpeedRow, 5, 0);
    lv_obj_set_style_bg_opa(mnSettingsSpeedRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mnSettingsSpeedRow, 0, 0);
    lv_obj_set_style_pad_all(mnSettingsSpeedRow, 8, 0);
    lv_obj_set_style_radius(mnSettingsSpeedRow, 6, 0);
    lv_obj_clear_flag(mnSettingsSpeedRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mnSettingsSpeedRow, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t* speed_header = lv_obj_create(mnSettingsSpeedRow);
    lv_obj_set_size(speed_header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(speed_header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(speed_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(speed_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(speed_header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(speed_header, 0, 0);
    lv_obj_set_style_pad_all(speed_header, 0, 0);

    lv_obj_t* speed_label = lv_label_create(speed_header);
    lv_label_set_text(speed_label, "Speed");
    lv_obj_add_style(speed_label, getStyleLabelSubtitle(), 0);

    mnSettingsSpeedValue = lv_label_create(speed_header);
    lv_label_set_text_fmt(mnSettingsSpeedValue, "%d WPM", cwSpeed);
    lv_obj_set_style_text_color(mnSettingsSpeedValue, LV_COLOR_ACCENT_CYAN, 0);

    mnSettingsSpeedSlider = lv_slider_create(mnSettingsSpeedRow);
    lv_obj_set_width(mnSettingsSpeedSlider, lv_pct(100));
    lv_slider_set_range(mnSettingsSpeedSlider, WPM_MIN, WPM_MAX);
    lv_slider_set_value(mnSettingsSpeedSlider, cwSpeed, LV_ANIM_OFF);
    applySliderStyle(mnSettingsSpeedSlider);
    lv_obj_add_event_cb(mnSettingsSpeedSlider, mnSettingsSpeedEventCb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- Tone setting row ---
    mnSettingsToneRow = lv_obj_create(content);
    lv_obj_set_size(mnSettingsToneRow, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(mnSettingsToneRow, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(mnSettingsToneRow, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(mnSettingsToneRow, 5, 0);
    lv_obj_set_style_bg_opa(mnSettingsToneRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mnSettingsToneRow, 0, 0);
    lv_obj_set_style_pad_all(mnSettingsToneRow, 8, 0);
    lv_obj_set_style_radius(mnSettingsToneRow, 6, 0);
    lv_obj_clear_flag(mnSettingsToneRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mnSettingsToneRow, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t* tone_header = lv_obj_create(mnSettingsToneRow);
    lv_obj_set_size(tone_header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(tone_header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tone_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tone_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(tone_header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tone_header, 0, 0);
    lv_obj_set_style_pad_all(tone_header, 0, 0);

    lv_obj_t* tone_label = lv_label_create(tone_header);
    lv_label_set_text(tone_label, "Tone");
    lv_obj_add_style(tone_label, getStyleLabelSubtitle(), 0);

    mnSettingsToneValue = lv_label_create(tone_header);
    lv_label_set_text_fmt(mnSettingsToneValue, "%d Hz", cwTone);
    lv_obj_set_style_text_color(mnSettingsToneValue, LV_COLOR_ACCENT_CYAN, 0);

    mnSettingsToneSlider = lv_slider_create(mnSettingsToneRow);
    lv_obj_set_width(mnSettingsToneSlider, lv_pct(100));
    lv_slider_set_range(mnSettingsToneSlider, 400, 1200);
    lv_slider_set_value(mnSettingsToneSlider, cwTone, LV_ANIM_OFF);
    applySliderStyle(mnSettingsToneSlider);
    lv_obj_add_event_cb(mnSettingsToneSlider, mnSettingsToneEventCb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- Key type setting row ---
    mnSettingsKeyTypeRow = lv_obj_create(content);
    lv_obj_set_size(mnSettingsKeyTypeRow, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(mnSettingsKeyTypeRow, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(mnSettingsKeyTypeRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mnSettingsKeyTypeRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(mnSettingsKeyTypeRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mnSettingsKeyTypeRow, 0, 0);
    lv_obj_set_style_pad_all(mnSettingsKeyTypeRow, 8, 0);
    lv_obj_set_style_radius(mnSettingsKeyTypeRow, 6, 0);
    lv_obj_clear_flag(mnSettingsKeyTypeRow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* keytype_label = lv_label_create(mnSettingsKeyTypeRow);
    lv_label_set_text(keytype_label, "Key Type");
    lv_obj_add_style(keytype_label, getStyleLabelSubtitle(), 0);

    mnSettingsKeyTypeValue = lv_label_create(mnSettingsKeyTypeRow);
    lv_label_set_text_fmt(mnSettingsKeyTypeValue, "< %s >", mnSettingsKeyTypeNames[getCwKeyTypeAsInt()]);
    lv_obj_set_style_text_color(mnSettingsKeyTypeValue, LV_COLOR_ACCENT_CYAN, 0);
    lv_obj_set_style_text_font(mnSettingsKeyTypeValue, getThemeFonts()->font_subtitle, 0);

    // Set initial focus styling
    mnSettingsUpdateFocus();

    // Footer
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* help = lv_label_create(footer);
    lv_label_set_text(help, FOOTER_NAV_ADJUST_ESC);
    lv_obj_set_style_text_color(help, LV_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(help, getThemeFonts()->font_small, 0);
    lv_obj_center(help);

    return screen;
}

#endif // LV_MORSE_NOTES_SCREENS_H

