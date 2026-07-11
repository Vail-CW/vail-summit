/*
 * VAIL SUMMIT - Satellite Tracker Screens
 * Ham Tools > Satellites: searchable catalog, pass predictions (start/end
 * azimuth + max elevation), pass detail with countdown, live az/el view,
 * and tracker settings.
 */

#ifndef LV_SATELLITE_SCREENS_H
#define LV_SATELLITE_SCREENS_H

#include <lvgl.h>
#include <WiFi.h>
#include "lv_theme_summit.h"
#include "lv_widgets_summit.h"
#include "lv_screen_manager.h"
#include "../core/config.h"
#include "../core/modes.h"
#include "../core/deferred_save.h"
#include "../satellites/sat_data.h"
#include "../satellites/sat_predict.h"
#include "../satellites/sat_freqs.h"
#include "../settings/settings_satellites.h"

extern void onLVGLMenuSelect(int target_mode);
extern void onLVGLBackNavigation();
static void playAlertChirp();  // defined in lv_mode_integration.h

// ============================================
// Screen State
// ============================================

// Satellite list
static lv_obj_t* sat_list_table = NULL;
static lv_obj_t* sat_list_count_label = NULL;
static lv_obj_t* sat_list_info_label = NULL;      // search text (left)
static lv_obj_t* sat_list_tle_label = NULL;       // TLE age (right)
static lv_obj_t* sat_list_empty_label = NULL;     // "no TLE data" message
static int sat_list_selected_row = 0;
static bool sat_search_entry = false;             // typing into the search filter
static char sat_search_filter[13] = "";

// Selection carried between screens
static int sat_selected_catalog_idx = -1;
static SatPass satSelectedPass;
static bool satSelectedPassValid = false;

// Passes
static lv_obj_t* sat_passes_table = NULL;
static lv_obj_t* sat_passes_status_label = NULL;
static lv_timer_t* sat_passes_timer = NULL;
static int sat_passes_selected_row = 0;

// Detail
static lv_obj_t* sat_detail_countdown_label = NULL;
static lv_timer_t* sat_detail_timer = NULL;

// Live view
static lv_obj_t* sat_live_az_label = NULL;
static lv_obj_t* sat_live_el_label = NULL;
static lv_obj_t* sat_live_dist_label = NULL;
static lv_obj_t* sat_live_status_label = NULL;
static lv_timer_t* sat_live_timer = NULL;

// Settings
static lv_obj_t* sat_set_value_labels[5] = { NULL };

// ============================================
// Cleanup (registered for all satellite modes)
// ============================================

void cleanupSatelliteScreens() {
    if (sat_passes_timer) { lv_timer_del(sat_passes_timer); sat_passes_timer = NULL; }
    if (sat_detail_timer) { lv_timer_del(sat_detail_timer); sat_detail_timer = NULL; }
    if (sat_live_timer)   { lv_timer_del(sat_live_timer);   sat_live_timer = NULL; }
    satSearch.active = false;

    sat_list_table = NULL;
    sat_list_count_label = NULL;
    sat_list_info_label = NULL;
    sat_list_tle_label = NULL;
    sat_list_empty_label = NULL;
    sat_passes_table = NULL;
    sat_passes_status_label = NULL;
    sat_detail_countdown_label = NULL;
    sat_live_az_label = NULL;
    sat_live_el_label = NULL;
    sat_live_dist_label = NULL;
    sat_live_status_label = NULL;
    for (int i = 0; i < 5; i++) sat_set_value_labels[i] = NULL;
}

// ============================================
// Shared Helpers
// ============================================

// Row highlight for keyboard-navigated tables
static void sat_table_highlight_cb(lv_event_t* e) {
    lv_obj_t* table = lv_event_get_target(e);
    lv_obj_draw_part_dsc_t* dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_ITEMS) return;
    int* selected = (int*)lv_event_get_user_data(e);
    uint16_t colCnt = lv_table_get_col_cnt(table);
    if (colCnt == 0 || !selected) return;
    if ((int)(dsc->id / colCnt) == *selected) {
        dsc->rect_dsc->bg_color = LV_COLOR_ACCENT_PRIMARY;
        dsc->rect_dsc->bg_opa = LV_OPA_COVER;
        if (dsc->label_dsc) dsc->label_dsc->color = getThemeColors()->text_on_accent;
    }
}

static lv_obj_t* satCreateHeader(lv_obj_t* screen, const char* title) {
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_add_style(header, getStyleStatusBar(), 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title_lbl = lv_label_create(header);
    lv_label_set_text(title_lbl, title);
    lv_obj_add_style(title_lbl, getStyleLabelTitle(), 0);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 15, 0);

    createCompactStatusBar(screen);
    return header;
}

static void satCreateFooter(lv_obj_t* screen, const char* text) {
    lv_obj_t* footer = lv_obj_create(screen);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_color(footer, getThemeColors()->bg_deep, 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* footer_text = lv_label_create(footer);
    lv_label_set_text(footer_text, text);
    lv_obj_set_style_text_font(footer_text, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(footer_text, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(footer_text, LV_ALIGN_CENTER, 0, 0);
}

// Standard data-table styling shared by the list and passes screens
static void satStyleTable(lv_obj_t* table) {
    lv_obj_set_style_bg_color(table, getThemeColors()->bg_deep, 0);
    lv_obj_set_style_bg_opa(table, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(table, 0, 0);
    lv_obj_set_style_pad_all(table, 0, 0);
    lv_obj_set_style_text_font(table, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_text_color(table, LV_COLOR_TEXT_PRIMARY, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(table, getThemeColors()->bg_deep, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(table, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(table, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(table, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(table, 4, LV_PART_ITEMS);
}

// Blocking TLE download behind a modal overlay (POTA refresh pattern).
// Returns true on success.
static bool satRunTLEUpdate() {
    if (WiFi.status() != WL_CONNECTED) {
        // May be invoked from a held ENTER (settings row) - discard the press
        // so its release cannot instantly dismiss the alert.
        lv_indev_t* indev = getLVGLKeypad();
        if (indev) lv_indev_wait_release(indev);
        playAlertChirp();
        createAlertDialog("WiFi Required", "Connect to WiFi first to\ndownload satellite TLEs.");
        return false;
    }
    lv_obj_t* overlay = createLoadingOverlay("Downloading TLEs...");
    bool ok = satFetchTLEs();
    lv_obj_del(overlay);
    if (ok) {
        beep(1000, 100);
        showToast("TLEs updated");
    } else {
        beep(400, 200);
        createAlertDialog("Download Failed", "Could not fetch TLEs from\nCelestrak. Try again later.");
    }
    return ok;
}

// ============================================
// Satellite List Screen
// ============================================

static void satUpdateListHeaderLabels() {
    if (sat_list_count_label && lv_obj_is_valid(sat_list_count_label)) {
        char buf[24];
        snprintf(buf, sizeof(buf), "SATELLITES (%d)", satDisplayCount);
        lv_label_set_text(sat_list_count_label, buf);
    }
    if (sat_list_info_label && lv_obj_is_valid(sat_list_info_label)) {
        char buf[32];
        if (sat_search_entry) {
            snprintf(buf, sizeof(buf), "Search: %s_", sat_search_filter);
        } else if (sat_search_filter[0] != '\0') {
            snprintf(buf, sizeof(buf), "Filter: %s", sat_search_filter);
        } else {
            strlcpy(buf, "", sizeof(buf));
        }
        lv_label_set_text(sat_list_info_label, buf);
    }
    if (sat_list_tle_label && lv_obj_is_valid(sat_list_tle_label)) {
        int age = satTLEAgeDays();
        char buf[36];
        if (!satCatalog.valid) {
            strlcpy(buf, "No TLE data", sizeof(buf));
            lv_obj_set_style_text_color(sat_list_tle_label, LV_COLOR_ERROR, 0);
        } else if (age < 0) {
            strlcpy(buf, "TLE age: unknown", sizeof(buf));
            lv_obj_set_style_text_color(sat_list_tle_label, LV_COLOR_WARNING, 0);
        } else {
            snprintf(buf, sizeof(buf), "TLE age: %dd", age);
            if (age >= SAT_TLE_STALE_DAYS) {
                lv_obj_set_style_text_color(sat_list_tle_label, LV_COLOR_ERROR, 0);
            } else if (age >= SAT_TLE_WARN_DAYS) {
                lv_obj_set_style_text_color(sat_list_tle_label, LV_COLOR_WARNING, 0);
            } else {
                lv_obj_set_style_text_color(sat_list_tle_label, LV_COLOR_TEXT_TERTIARY, 0);
            }
        }
        lv_label_set_text(sat_list_tle_label, buf);
    }
}

static void satRefreshListTable() {
    if (!sat_list_table || !lv_obj_is_valid(sat_list_table)) return;

    buildSatDisplayList(sat_search_filter);
    satUpdateListHeaderLabels();

    bool empty = (satDisplayCount == 0);
    if (sat_list_empty_label && lv_obj_is_valid(sat_list_empty_label)) {
        if (empty) lv_obj_clear_flag(sat_list_empty_label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(sat_list_empty_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(sat_list_empty_label,
            satCatalog.valid ? "No satellites match the filter"
                             : "No TLE data on device.\nPress R to download from Celestrak.");
    }

    if (empty) {
        lv_table_set_row_cnt(sat_list_table, 1);
        lv_table_set_cell_value(sat_list_table, 0, 0, "");
        lv_table_set_cell_value(sat_list_table, 0, 1, "");
        lv_table_set_cell_value(sat_list_table, 0, 2, "");
        return;
    }

    lv_table_set_row_cnt(sat_list_table, satDisplayCount);
    for (int i = 0; i < satDisplayCount; i++) {
        SatEntry& e = satCatalog.sats[satDisplayIdx[i]];
        lv_table_set_cell_value(sat_list_table, i, 0, isSatFavorite(e.norad) ? "*" : "");
        lv_table_set_cell_value(sat_list_table, i, 1, e.name);
        char id[12];
        snprintf(id, sizeof(id), "%lu", (unsigned long)e.norad);
        lv_table_set_cell_value(sat_list_table, i, 2, id);
    }

    if (sat_list_selected_row >= satDisplayCount) {
        sat_list_selected_row = satDisplayCount - 1;
    }
    if (sat_list_selected_row < 0) sat_list_selected_row = 0;
    lv_obj_invalidate(sat_list_table);
}

static void sat_list_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    // --- Search entry mode: printable keys build the filter ---
    if (sat_search_entry) {
        if (key == LV_KEY_ESC) {
            sat_search_entry = false;
            sat_search_filter[0] = '\0';
            satRefreshListTable();
            lv_event_stop_processing(e);
            return;
        }
        if (key == LV_KEY_ENTER) {
            sat_search_entry = false;
            satUpdateListHeaderLabels();
            lv_event_stop_processing(e);
            return;
        }
        if (key == LV_KEY_BACKSPACE) {
            int len = strlen(sat_search_filter);
            if (len > 0) sat_search_filter[len - 1] = '\0';
            satRefreshListTable();
            lv_event_stop_processing(e);
            return;
        }
        if (key >= 32 && key < 127) {
            int len = strlen(sat_search_filter);
            if (len < (int)sizeof(sat_search_filter) - 1) {
                sat_search_filter[len] = (char)toupper((int)key);
                sat_search_filter[len + 1] = '\0';
                sat_list_selected_row = 0;
                satRefreshListTable();
            }
            lv_event_stop_processing(e);
            return;
        }
        // fall through for arrows so navigation still works while searching
    }

    if (key == '/') {
        sat_search_entry = true;
        satUpdateListHeaderLabels();
        lv_event_stop_processing(e);
        return;
    }

    if (!sat_search_entry && (key == 'R' || key == 'r')) {
        if (satRunTLEUpdate()) {
            sat_list_selected_row = 0;
            satRefreshListTable();
        }
        lv_event_stop_processing(e);
        return;
    }

    if (!sat_search_entry && (key == 'S' || key == 's')) {
        onLVGLMenuSelect(MODE_SAT_SETTINGS);
        lv_event_stop_processing(e);
        return;
    }

    if (!sat_search_entry && (key == 'F' || key == 'f')) {
        if (satDisplayCount > 0 && sat_list_selected_row < satDisplayCount) {
            toggleSatFavorite(satCatalog.sats[satDisplayIdx[sat_list_selected_row]].norad);
            beep(800, 60);
            satRefreshListTable();
        }
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        if (sat_list_selected_row > 0) {
            sat_list_selected_row--;
            lv_obj_scroll_to_y(sat_list_table, sat_list_selected_row * 30, LV_ANIM_ON);
            lv_obj_invalidate(sat_list_table);
        }
        lv_event_stop_processing(e);
        return;
    }
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        if (sat_list_selected_row < satDisplayCount - 1) {
            sat_list_selected_row++;
            lv_obj_scroll_to_y(sat_list_table, sat_list_selected_row * 30, LV_ANIM_ON);
            lv_obj_invalidate(sat_list_table);
        }
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_ENTER) {
        if (satDisplayCount > 0 && sat_list_selected_row < satDisplayCount) {
            double lat, lon;
            if (!gridToLatLon(satEffectiveGrid(), &lat, &lon)) {
                // Discard the in-flight ENTER press: its release would land on
                // the alert's OK button and dismiss the dialog instantly.
                lv_indev_t* indev = getLVGLKeypad();
                if (indev) lv_indev_wait_release(indev);
                playAlertChirp();
                createAlertDialog("Grid Square Needed",
                    "Set your Maidenhead grid in\nSetup (press S) so passes can\nbe computed for your location.");
            } else if (!ntpSynced) {
                lv_indev_t* indev = getLVGLKeypad();
                if (indev) lv_indev_wait_release(indev);
                playAlertChirp();
                createAlertDialog("Clock Not Set",
                    "Time is not synced yet.\nConnect to WiFi so NTP can\nset the clock, then retry.");
            } else {
                sat_selected_catalog_idx = satDisplayIdx[sat_list_selected_row];
                onLVGLMenuSelect(MODE_SAT_PASSES);
            }
        }
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_ESC) {
        onLVGLBackNavigation();
        lv_event_stop_processing(e);
        return;
    }

    // Swallow stray printable keys outside search mode so they don't reach LVGL defaults
    if (key >= 32 && key < 127) {
        lv_event_stop_processing(e);
    }
}

lv_obj_t* createSatListScreen() {
    clearNavigationGroup();
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    initSatCatalog();
    loadSatSettings();
    loadSatFavorites();
    if (!satCatalog.valid) {
        satLoadTLEsFromSD();
    }

    sat_search_entry = false;

    // Header
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_add_style(header, getStyleStatusBar(), 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    sat_list_count_label = lv_label_create(header);
    lv_label_set_text(sat_list_count_label, "SATELLITES");
    lv_obj_add_style(sat_list_count_label, getStyleLabelTitle(), 0);
    lv_obj_align(sat_list_count_label, LV_ALIGN_LEFT_MID, 15, 0);

    createCompactStatusBar(screen);

    // Info bar: search text left, TLE age right
    lv_obj_t* info_bar = lv_obj_create(screen);
    lv_obj_set_size(info_bar, SCREEN_WIDTH - 20, 18);
    lv_obj_set_pos(info_bar, 10, HEADER_HEIGHT + 3);
    lv_obj_set_style_bg_opa(info_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(info_bar, 0, 0);
    lv_obj_set_style_pad_all(info_bar, 0, 0);
    lv_obj_clear_flag(info_bar, LV_OBJ_FLAG_SCROLLABLE);

    sat_list_info_label = lv_label_create(info_bar);
    lv_label_set_text(sat_list_info_label, "");
    lv_obj_set_style_text_font(sat_list_info_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sat_list_info_label, LV_COLOR_WARNING, 0);
    lv_obj_align(sat_list_info_label, LV_ALIGN_LEFT_MID, 5, 0);

    sat_list_tle_label = lv_label_create(info_bar);
    lv_label_set_text(sat_list_tle_label, "");
    lv_obj_set_style_text_font(sat_list_tle_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sat_list_tle_label, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(sat_list_tle_label, LV_ALIGN_RIGHT_MID, -5, 0);

    // Column header bar
    int header_row_y = HEADER_HEIGHT + 24;
    lv_obj_t* header_bar = lv_obj_create(screen);
    lv_obj_set_size(header_bar, SCREEN_WIDTH - 20, 26);
    lv_obj_set_pos(header_bar, 10, header_row_y);
    lv_obj_set_style_bg_color(header_bar, getThemeColors()->bg_layer2, 0);
    lv_obj_set_style_bg_opa(header_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header_bar, 0, 0);
    lv_obj_set_style_pad_all(header_bar, 0, 0);
    lv_obj_clear_flag(header_bar, LV_OBJ_FLAG_SCROLLABLE);

    const char* colNames[3] = { "FAV", "NAME", "NORAD" };
    int colX[3] = { 4, 48, 352 };
    for (int i = 0; i < 3; i++) {
        lv_obj_t* h = lv_label_create(header_bar);
        lv_label_set_text(h, colNames[i]);
        lv_obj_set_style_text_font(h, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(h, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_set_pos(h, colX[i], 4);
    }

    // Table
    int table_y = header_row_y + 28;
    int table_height = SCREEN_HEIGHT - table_y - FOOTER_HEIGHT - 5;

    sat_list_table = lv_table_create(screen);
    lv_obj_set_size(sat_list_table, SCREEN_WIDTH - 20, table_height);
    lv_obj_set_pos(sat_list_table, 10, table_y);
    satStyleTable(sat_list_table);
    lv_table_set_col_cnt(sat_list_table, 3);
    lv_table_set_col_width(sat_list_table, 0, 44);
    lv_table_set_col_width(sat_list_table, 1, 304);
    lv_table_set_col_width(sat_list_table, 2, 110);

    lv_obj_add_event_cb(sat_list_table, sat_table_highlight_cb, LV_EVENT_DRAW_PART_BEGIN, &sat_list_selected_row);
    lv_obj_add_flag(sat_list_table, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sat_list_table, sat_list_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(sat_list_table);

    // Empty-state message (over table)
    sat_list_empty_label = lv_label_create(screen);
    lv_label_set_text(sat_list_empty_label, "");
    lv_obj_set_style_text_font(sat_list_empty_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(sat_list_empty_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_align(sat_list_empty_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sat_list_empty_label, LV_ALIGN_CENTER, 0, 10);
    lv_obj_add_flag(sat_list_empty_label, LV_OBJ_FLAG_HIDDEN);

    satCreateFooter(screen, "ENTER Passes   / Search   F Fav   R Update TLEs   S Setup   ESC Back");

    satRefreshListTable();
    return screen;
}

// ============================================
// Passes Screen
// ============================================

static void satRefreshPassesTable() {
    if (!sat_passes_table || !lv_obj_is_valid(sat_passes_table)) return;

    if (satSearch.count == 0) {
        lv_table_set_row_cnt(sat_passes_table, 1);
        char msg[64];
        snprintf(msg, sizeof(msg), "No passes above %d deg in next %dh",
                 satSettings.minElevation, satSettings.lookaheadHours);
        lv_table_set_cell_value(sat_passes_table, 0, 0, satSearch.done ? msg : "");
        for (int c = 1; c < 5; c++) lv_table_set_cell_value(sat_passes_table, 0, c, "");
        return;
    }

    lv_table_set_row_cnt(sat_passes_table, satSearch.count);
    for (int i = 0; i < satSearch.count; i++) {
        SatPass& p = satSearch.passes[i];
        char buf[40];

        satFmtLocalDate(p.aos, buf, sizeof(buf));
        lv_table_set_cell_value(sat_passes_table, i, 0, buf);

        char lt[12], zt[12];
        satFmtLocalTime(p.aos, lt, sizeof(lt));
        satFmtZuluTime(p.aos, zt, sizeof(zt));
        snprintf(buf, sizeof(buf), "%s (%s)", lt, zt);
        lv_table_set_cell_value(sat_passes_table, i, 1, buf);

        long durMin = (long)(p.los - p.aos + 30) / 60;
        snprintf(buf, sizeof(buf), "%ldm", durMin);
        lv_table_set_cell_value(sat_passes_table, i, 2, buf);

        snprintf(buf, sizeof(buf), "%.0f", p.maxEl);
        lv_table_set_cell_value(sat_passes_table, i, 3, buf);

        snprintf(buf, sizeof(buf), "%s>%s", azToCompass(p.aosAz), azToCompass(p.losAz));
        lv_table_set_cell_value(sat_passes_table, i, 4, buf);
    }

    if (sat_passes_selected_row >= satSearch.count) {
        sat_passes_selected_row = satSearch.count - 1;
    }
    if (sat_passes_selected_row < 0) sat_passes_selected_row = 0;
    lv_obj_invalidate(sat_passes_table);
}

static void sat_passes_search_timer_cb(lv_timer_t* timer) {
    if (!sat_passes_status_label || !lv_obj_is_valid(sat_passes_status_label)) return;

    bool finished = satPassSearchStep(35);

    if (!finished) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Searching... %d%% (%d found)",
                 satPassSearchProgress(), satSearch.count);
        lv_label_set_text(sat_passes_status_label, buf);
        return;
    }

    // Done
    if (sat_passes_timer) { lv_timer_del(sat_passes_timer); sat_passes_timer = NULL; }
    char buf[64];
    snprintf(buf, sizeof(buf), "%d passes in next %dh (min el %d deg)",
             satSearch.count, satSettings.lookaheadHours, satSettings.minElevation);
    lv_label_set_text(sat_passes_status_label, buf);
    satRefreshPassesTable();
}

static void satStartPassesSearchUI() {
    if (sat_passes_timer) { lv_timer_del(sat_passes_timer); sat_passes_timer = NULL; }
    sat_passes_selected_row = 0;

    if (!satStartPassSearch(sat_selected_catalog_idx)) {
        if (sat_passes_status_label && lv_obj_is_valid(sat_passes_status_label)) {
            lv_label_set_text(sat_passes_status_label, "Cannot search: check grid + clock");
            lv_obj_set_style_text_color(sat_passes_status_label, LV_COLOR_ERROR, 0);
        }
        return;
    }
    sat_passes_timer = lv_timer_create(sat_passes_search_timer_cb, 60, NULL);
}

static void sat_passes_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        if (sat_passes_selected_row > 0) {
            sat_passes_selected_row--;
            lv_obj_scroll_to_y(sat_passes_table, sat_passes_selected_row * 30, LV_ANIM_ON);
            lv_obj_invalidate(sat_passes_table);
        }
        lv_event_stop_processing(e);
        return;
    }
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        if (sat_passes_selected_row < satSearch.count - 1) {
            sat_passes_selected_row++;
            lv_obj_scroll_to_y(sat_passes_table, sat_passes_selected_row * 30, LV_ANIM_ON);
            lv_obj_invalidate(sat_passes_table);
        }
        lv_event_stop_processing(e);
        return;
    }

    if (key == 'R' || key == 'r') {
        if (!satSearch.active) {
            satRefreshPassesTable();
            satStartPassesSearchUI();
        }
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_ENTER) {
        if (satSearch.done && satSearch.count > 0 && sat_passes_selected_row < satSearch.count) {
            satSelectedPass = satSearch.passes[sat_passes_selected_row];
            satSelectedPassValid = true;
            onLVGLMenuSelect(MODE_SAT_PASS_DETAIL);
        }
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_ESC) {
        onLVGLBackNavigation();
        lv_event_stop_processing(e);
        return;
    }
}

lv_obj_t* createSatPassesScreen() {
    clearNavigationGroup();
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    const char* satName = (sat_selected_catalog_idx >= 0 && sat_selected_catalog_idx < satCatalog.count)
        ? satCatalog.sats[sat_selected_catalog_idx].name : "?";

    // Header with satellite name
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_add_style(header, getStyleStatusBar(), 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    createSplitTitleLabel(header, "PASSES", satName);
    createCompactStatusBar(screen);

    // Status line
    sat_passes_status_label = lv_label_create(screen);
    lv_label_set_text(sat_passes_status_label, "Searching...");
    lv_obj_set_style_text_font(sat_passes_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sat_passes_status_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_pos(sat_passes_status_label, 15, HEADER_HEIGHT + 5);

    // Column headers
    int header_row_y = HEADER_HEIGHT + 24;
    lv_obj_t* header_bar = lv_obj_create(screen);
    lv_obj_set_size(header_bar, SCREEN_WIDTH - 20, 26);
    lv_obj_set_pos(header_bar, 10, header_row_y);
    lv_obj_set_style_bg_color(header_bar, getThemeColors()->bg_layer2, 0);
    lv_obj_set_style_bg_opa(header_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header_bar, 0, 0);
    lv_obj_set_style_pad_all(header_bar, 0, 0);
    lv_obj_clear_flag(header_bar, LV_OBJ_FLAG_SCROLLABLE);

    const char* colNames[5] = { "DATE", "AOS", "LEN", "EL", "PATH" };
    int colX[5] = { 8, 112, 248, 306, 366 };
    for (int i = 0; i < 5; i++) {
        lv_obj_t* h = lv_label_create(header_bar);
        lv_label_set_text(h, colNames[i]);
        lv_obj_set_style_text_font(h, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(h, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_set_pos(h, colX[i], 4);
    }

    // Table
    int table_y = header_row_y + 28;
    int table_height = SCREEN_HEIGHT - table_y - FOOTER_HEIGHT - 5;

    sat_passes_table = lv_table_create(screen);
    lv_obj_set_size(sat_passes_table, SCREEN_WIDTH - 20, table_height);
    lv_obj_set_pos(sat_passes_table, 10, table_y);
    satStyleTable(sat_passes_table);
    lv_table_set_col_cnt(sat_passes_table, 5);
    lv_table_set_col_width(sat_passes_table, 0, 104);
    lv_table_set_col_width(sat_passes_table, 1, 136);
    lv_table_set_col_width(sat_passes_table, 2, 58);
    lv_table_set_col_width(sat_passes_table, 3, 60);
    lv_table_set_col_width(sat_passes_table, 4, 100);

    lv_table_set_row_cnt(sat_passes_table, 1);
    for (int c = 0; c < 5; c++) lv_table_set_cell_value(sat_passes_table, 0, c, "");

    lv_obj_add_event_cb(sat_passes_table, sat_table_highlight_cb, LV_EVENT_DRAW_PART_BEGIN, &sat_passes_selected_row);
    lv_obj_add_flag(sat_passes_table, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sat_passes_table, sat_passes_key_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(sat_passes_table);

    satCreateFooter(screen, "UP/DN Scroll   ENTER Detail   R Re-search   ESC Back");

    satStartPassesSearchUI();
    return screen;
}

// ============================================
// Pass Detail Screen
// ============================================

static void sat_detail_countdown_cb(lv_timer_t* timer) {
    if (!sat_detail_countdown_label || !lv_obj_is_valid(sat_detail_countdown_label)) return;
    if (!satSelectedPassValid || !ntpSynced) return;

    time_t now = time(nullptr);
    char cd[24], buf[64];
    if (now < satSelectedPass.aos) {
        satFmtCountdown((long)(satSelectedPass.aos - now), cd, sizeof(cd));
        snprintf(buf, sizeof(buf), "AOS in %s", cd);
        lv_obj_set_style_text_color(sat_detail_countdown_label, LV_COLOR_ACCENT_PRIMARY, 0);
    } else if (now <= satSelectedPass.los) {
        satFmtCountdown((long)(satSelectedPass.los - now), cd, sizeof(cd));
        snprintf(buf, sizeof(buf), "PASS IN PROGRESS - LOS in %s", cd);
        lv_obj_set_style_text_color(sat_detail_countdown_label, LV_COLOR_SUCCESS, 0);
    } else {
        strlcpy(buf, "Pass complete", sizeof(buf));
        lv_obj_set_style_text_color(sat_detail_countdown_label, LV_COLOR_TEXT_TERTIARY, 0);
    }
    lv_label_set_text(sat_detail_countdown_label, buf);
}

static void sat_detail_live_btn_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        onLVGLMenuSelect(MODE_SAT_LIVE);
    }
}

lv_obj_t* createSatPassDetailScreen() {
    clearNavigationGroup();
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    const char* satName = (sat_selected_catalog_idx >= 0 && sat_selected_catalog_idx < satCatalog.count)
        ? satCatalog.sats[sat_selected_catalog_idx].name : "?";
    uint32_t norad = (sat_selected_catalog_idx >= 0 && sat_selected_catalog_idx < satCatalog.count)
        ? satCatalog.sats[sat_selected_catalog_idx].norad : 0;

    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_add_style(header, getStyleStatusBar(), 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    createSplitTitleLabel(header, "PASS", satName);
    createCompactStatusBar(screen);

    // Content column
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_set_size(content, SCREEN_WIDTH - 20, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 10);
    lv_obj_set_pos(content, 10, HEADER_HEIGHT + 5);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_style_pad_row(content, 4, 0);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    char buf[96];

    // Countdown (updated by 1s timer)
    sat_detail_countdown_label = lv_label_create(content);
    lv_label_set_text(sat_detail_countdown_label, "");
    lv_obj_set_style_text_font(sat_detail_countdown_label, getThemeFonts()->font_subtitle, 0);

    if (satSelectedPassValid) {
        char date[16], lt[12], zt[12];
        satFmtLocalDate(satSelectedPass.aos, date, sizeof(date));

        // AOS / MAX / LOS rows
        satFmtLocalTime(satSelectedPass.aos, lt, sizeof(lt));
        satFmtZuluTime(satSelectedPass.aos, zt, sizeof(zt));
        snprintf(buf, sizeof(buf), "AOS  %s %s (%s)   az %.0f %s",
                 date, lt, zt, satSelectedPass.aosAz, azToCompass(satSelectedPass.aosAz));
        lv_obj_t* aos_lbl = lv_label_create(content);
        lv_label_set_text(aos_lbl, buf);
        lv_obj_set_style_text_font(aos_lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(aos_lbl, LV_COLOR_TEXT_PRIMARY, 0);

        satFmtLocalTime(satSelectedPass.tmax, lt, sizeof(lt));
        satFmtZuluTime(satSelectedPass.tmax, zt, sizeof(zt));
        snprintf(buf, sizeof(buf), "MAX  %s (%s)   el %.0f   az %.0f %s",
                 lt, zt, satSelectedPass.maxEl, satSelectedPass.maxAz, azToCompass(satSelectedPass.maxAz));
        lv_obj_t* max_lbl = lv_label_create(content);
        lv_label_set_text(max_lbl, buf);
        lv_obj_set_style_text_font(max_lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(max_lbl, LV_COLOR_ACCENT_PRIMARY, 0);

        satFmtLocalTime(satSelectedPass.los, lt, sizeof(lt));
        satFmtZuluTime(satSelectedPass.los, zt, sizeof(zt));
        snprintf(buf, sizeof(buf), "LOS  %s (%s)   az %.0f %s",
                 lt, zt, satSelectedPass.losAz, azToCompass(satSelectedPass.losAz));
        lv_obj_t* los_lbl = lv_label_create(content);
        lv_label_set_text(los_lbl, buf);
        lv_obj_set_style_text_font(los_lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(los_lbl, LV_COLOR_TEXT_PRIMARY, 0);

        long durS = (long)(satSelectedPass.los - satSelectedPass.aos);
        snprintf(buf, sizeof(buf), "Duration %ldm %02lds", durS / 60, durS % 60);
        lv_obj_t* dur_lbl = lv_label_create(content);
        lv_label_set_text(dur_lbl, buf);
        lv_obj_set_style_text_font(dur_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(dur_lbl, LV_COLOR_TEXT_SECONDARY, 0);
    }

    // Frequency reference card (curated table)
    const SatFreqInfo* fi = lookupSatFreqs(norad);
    if (fi) {
        lv_obj_t* fcard = lv_obj_create(content);
        lv_obj_set_size(fcard, lv_pct(100), LV_SIZE_CONTENT);
        applyCardStyle(fcard);
        lv_obj_set_style_pad_all(fcard, 8, 0);
        lv_obj_set_style_pad_row(fcard, 2, 0);
        lv_obj_set_layout(fcard, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(fcard, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(fcard, LV_OBJ_FLAG_SCROLLABLE);

        if (fi->uplink[0] != '\0') {
            snprintf(buf, sizeof(buf), "UP  %s MHz", fi->uplink);
            lv_obj_t* l = lv_label_create(fcard);
            lv_label_set_text(l, buf);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(l, LV_COLOR_TEXT_PRIMARY, 0);
        }
        snprintf(buf, sizeof(buf), "DN  %s MHz   (%s)", fi->downlink, fi->mode);
        lv_obj_t* dn = lv_label_create(fcard);
        lv_label_set_text(dn, buf);
        lv_obj_set_style_text_font(dn, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(dn, LV_COLOR_SUCCESS, 0);

        lv_obj_t* note = lv_label_create(fcard);
        lv_label_set_text(note, fi->note);
        lv_obj_set_style_text_font(note, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(note, LV_COLOR_WARNING, 0);
        lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(note, SCREEN_WIDTH - 60);
    }

    // LIVE VIEW button (also the ESC anchor)
    lv_obj_t* live_btn = lv_obj_create(content);
    lv_obj_set_size(live_btn, 160, 34);
    lv_obj_add_flag(live_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(live_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(live_btn, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_radius(live_btn, 8, 0);
    lv_obj_set_style_border_width(live_btn, 1, 0);
    lv_obj_set_style_border_color(live_btn, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_border_width(live_btn, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(live_btn, LV_COLOR_ACCENT_PRIMARY, LV_STATE_FOCUSED);
    lv_obj_t* live_lbl = lv_label_create(live_btn);
    lv_label_set_text(live_lbl, LV_SYMBOL_GPS " LIVE VIEW");
    lv_obj_set_style_text_font(live_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(live_lbl, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(live_lbl);

    lv_obj_add_event_cb(live_btn, sat_detail_live_btn_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(live_btn, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(live_btn);

    satCreateFooter(screen, "ENTER Live View   ESC Back");

    sat_detail_countdown_cb(NULL);
    sat_detail_timer = lv_timer_create(sat_detail_countdown_cb, 1000, NULL);
    return screen;
}

// ============================================
// Live View Screen
// ============================================

static void sat_live_timer_cb(lv_timer_t* timer) {
    if (!sat_live_az_label || !lv_obj_is_valid(sat_live_az_label)) return;

    SatLiveState st;
    if (!satLiveUpdate(sat_selected_catalog_idx, &st)) {
        lv_label_set_text(sat_live_status_label, "Tracking unavailable");
        return;
    }

    char buf[48];
    snprintf(buf, sizeof(buf), "%.1f  %s", st.az, azToCompass(st.az));
    lv_label_set_text(sat_live_az_label, buf);

    snprintf(buf, sizeof(buf), "%+.1f", st.el);
    lv_label_set_text(sat_live_el_label, buf);
    lv_obj_set_style_text_color(sat_live_el_label,
        st.el > 0 ? LV_COLOR_SUCCESS : LV_COLOR_TEXT_SECONDARY, 0);

    snprintf(buf, sizeof(buf), "%.0f km", st.distKm);
    lv_label_set_text(sat_live_dist_label, buf);

    // Status: above/below horizon + countdown vs the selected pass
    if (sat_live_status_label && lv_obj_is_valid(sat_live_status_label)) {
        time_t now = time(nullptr);
        char cd[24];
        if (st.el > 0) {
            if (satSelectedPassValid && now <= satSelectedPass.los && now >= satSelectedPass.aos) {
                satFmtCountdown((long)(satSelectedPass.los - now), cd, sizeof(cd));
                snprintf(buf, sizeof(buf), "ABOVE HORIZON - LOS in %s", cd);
            } else {
                strlcpy(buf, "ABOVE HORIZON", sizeof(buf));
            }
            lv_obj_set_style_text_color(sat_live_status_label, LV_COLOR_SUCCESS, 0);
        } else {
            if (satSelectedPassValid && now < satSelectedPass.aos) {
                satFmtCountdown((long)(satSelectedPass.aos - now), cd, sizeof(cd));
                snprintf(buf, sizeof(buf), "BELOW HORIZON - AOS in %s", cd);
            } else {
                strlcpy(buf, "BELOW HORIZON", sizeof(buf));
            }
            lv_obj_set_style_text_color(sat_live_status_label, LV_COLOR_TEXT_SECONDARY, 0);
        }
        lv_label_set_text(sat_live_status_label, buf);
    }
}

lv_obj_t* createSatLiveScreen() {
    clearNavigationGroup();
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    const char* satName = (sat_selected_catalog_idx >= 0 && sat_selected_catalog_idx < satCatalog.count)
        ? satCatalog.sats[sat_selected_catalog_idx].name : "?";

    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_add_style(header, getStyleStatusBar(), 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    createSplitTitleLabel(header, "LIVE", satName);
    createCompactStatusBar(screen);

    // Status line
    sat_live_status_label = lv_label_create(screen);
    lv_label_set_text(sat_live_status_label, "");
    lv_obj_set_style_text_font(sat_live_status_label, &lv_font_montserrat_16, 0);
    lv_obj_align(sat_live_status_label, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 12);

    // Big AZ/EL/DIST readouts in a 3-column row
    struct { const char* caption; lv_obj_t** value; } readouts[3] = {
        { "AZIMUTH", &sat_live_az_label },
        { "ELEVATION", &sat_live_el_label },
        { "RANGE", &sat_live_dist_label },
    };
    int cardW = 145, cardH = 110;
    int startX = (SCREEN_WIDTH - (cardW * 3 + 20)) / 2;
    for (int i = 0; i < 3; i++) {
        lv_obj_t* card = lv_obj_create(screen);
        lv_obj_set_size(card, cardW, cardH);
        lv_obj_set_pos(card, startX + i * (cardW + 10), HEADER_HEIGHT + 45);
        applyCardStyle(card);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* cap = lv_label_create(card);
        lv_label_set_text(cap, readouts[i].caption);
        lv_obj_set_style_text_font(cap, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(cap, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_align(cap, LV_ALIGN_TOP_MID, 0, 4);

        lv_obj_t* val = lv_label_create(card);
        lv_label_set_text(val, "--");
        lv_obj_set_style_text_font(val, getThemeFonts()->font_title, 0);
        lv_obj_set_style_text_color(val, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_align(val, LV_ALIGN_CENTER, 0, 10);
        *(readouts[i].value) = val;
    }

    // Hint
    lv_obj_t* hint = lv_label_create(screen);
    lv_label_set_text(hint, "Point the antenna: azimuth is compass heading, elevation is tilt");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 170);

    // Invisible focus anchor so ESC back-navigation works
    lv_obj_t* focus_anchor = lv_obj_create(screen);
    lv_obj_set_size(focus_anchor, 1, 1);
    lv_obj_set_pos(focus_anchor, 0, SCREEN_HEIGHT - 2);
    lv_obj_set_style_bg_opa(focus_anchor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(focus_anchor, 0, 0);
    lv_obj_add_flag(focus_anchor, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(focus_anchor, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(focus_anchor);

    satCreateFooter(screen, "Updates every second   ESC Back");

    sat_live_timer_cb(NULL);
    sat_live_timer = lv_timer_create(sat_live_timer_cb, 1000, NULL);
    return screen;
}

// ============================================
// Settings Screen
// ============================================
// Rows: 0 grid, 1 min elevation, 2 lookahead, 3 UTC offset, 4 update TLEs

static void satUpdateSettingsValueLabels() {
    char buf[40];

    if (sat_set_value_labels[0] && lv_obj_is_valid(sat_set_value_labels[0])) {
        if (satSettings.grid[0] != '\0') {
            strlcpy(buf, satSettings.grid, sizeof(buf));
        } else if (operatorGrid[0] != '\0') {
            snprintf(buf, sizeof(buf), "%s (logger)", operatorGrid);
        } else {
            strlcpy(buf, "not set", sizeof(buf));
        }
        lv_label_set_text(sat_set_value_labels[0], buf);
    }
    if (sat_set_value_labels[1] && lv_obj_is_valid(sat_set_value_labels[1])) {
        snprintf(buf, sizeof(buf), "%d deg", satSettings.minElevation);
        lv_label_set_text(sat_set_value_labels[1], buf);
    }
    if (sat_set_value_labels[2] && lv_obj_is_valid(sat_set_value_labels[2])) {
        snprintf(buf, sizeof(buf), "%dh", satSettings.lookaheadHours);
        lv_label_set_text(sat_set_value_labels[2], buf);
    }
    if (sat_set_value_labels[3] && lv_obj_is_valid(sat_set_value_labels[3])) {
        formatUtcOffset(buf, sizeof(buf));
        lv_label_set_text(sat_set_value_labels[3], buf);
    }
    if (sat_set_value_labels[4] && lv_obj_is_valid(sat_set_value_labels[4])) {
        int age = satTLEAgeDays();
        if (!satCatalog.valid) strlcpy(buf, "no data", sizeof(buf));
        else if (age < 0) strlcpy(buf, "age unknown", sizeof(buf));
        else snprintf(buf, sizeof(buf), "%dd old", age);
        lv_label_set_text(sat_set_value_labels[4], buf);
    }
}

static void sat_settings_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);
    int row = (int)(intptr_t)lv_event_get_user_data(e);

    // Grid row: type letters/digits, backspace to erase (empty = logger grid)
    if (row == 0) {
        if (key == LV_KEY_BACKSPACE) {
            int len = strlen(satSettings.grid);
            if (len > 0) {
                satSettings.grid[len - 1] = '\0';
                markDeferredSave(saveSatSettings);
                satUpdateSettingsValueLabels();
            }
            lv_event_stop_processing(e);
            return;
        }
        if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) {
            int len = strlen(satSettings.grid);
            if (len < 8) {
                satSettings.grid[len] = (char)toupper((int)key);
                satSettings.grid[len + 1] = '\0';
                markDeferredSave(saveSatSettings);
                satUpdateSettingsValueLabels();
            }
            lv_event_stop_processing(e);
            return;
        }
    }

    if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        int dir = (key == LV_KEY_RIGHT) ? 1 : -1;
        switch (row) {
            case 1:
                satSettings.minElevation += dir * 5;
                if (satSettings.minElevation < 0) satSettings.minElevation = 0;
                if (satSettings.minElevation > 45) satSettings.minElevation = 45;
                markDeferredSave(saveSatSettings);
                break;
            case 2: {
                static const int opts[4] = { 12, 24, 48, 72 };
                int cur = 2;
                for (int i = 0; i < 4; i++) if (opts[i] == satSettings.lookaheadHours) cur = i;
                cur = (cur + dir + 4) % 4;
                satSettings.lookaheadHours = opts[cur];
                markDeferredSave(saveSatSettings);
                break;
            }
            case 3:
                utcOffsetMinutes += dir * 30;
                if (utcOffsetMinutes < -720) utcOffsetMinutes = -720;
                if (utcOffsetMinutes > 840) utcOffsetMinutes = 840;
                markDeferredSave(saveClockSettings);
                break;
            default:
                break;
        }
        satUpdateSettingsValueLabels();
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_ENTER && row == 4) {
        satRunTLEUpdate();
        satUpdateSettingsValueLabels();
        lv_event_stop_processing(e);
        return;
    }
}

lv_obj_t* createSatSettingsScreen() {
    clearNavigationGroup();
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);
    loadSatSettings();

    satCreateHeader(screen, "SATELLITE SETUP");

    lv_obj_t* list = lv_obj_create(screen);
    lv_obj_set_size(list, SCREEN_WIDTH - 20, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT - 10);
    lv_obj_set_pos(list, 10, HEADER_HEIGHT + 5);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 2, 0);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_layout(list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    static const char* rowNames[5] = {
        "Grid Square (type to set)",
        "Min Elevation",
        "Lookahead Window",
        "UTC Offset (local time)",
        "Update TLEs Now"
    };

    for (int i = 0; i < 5; i++) {
        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(row, 10, 0);
        applyCardStyle(row);
        lv_obj_set_style_border_color(row, LV_COLOR_ACCENT_PRIMARY, LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(row, 2, LV_STATE_FOCUSED);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, rowNames[i]);
        lv_obj_add_style(lbl, getStyleLabelBody(), 0);

        lv_obj_t* val = lv_label_create(row);
        lv_label_set_text(val, "");
        lv_obj_set_style_text_color(val, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_set_style_text_font(val, getThemeFonts()->font_input, 0);
        sat_set_value_labels[i] = val;

        lv_obj_add_event_cb(row, sat_settings_key_handler, LV_EVENT_KEY, (void*)(intptr_t)i);
        lv_obj_add_event_cb(row, linear_nav_handler, LV_EVENT_KEY, NULL);
        addNavigableWidget(row);
    }

    satUpdateSettingsValueLabels();
    satCreateFooter(screen, "UP/DN Navigate   L/R Adjust   Type grid on Grid row   ESC Back (auto-saves)");
    return screen;
}

// ============================================
// Mode Dispatcher
// ============================================

lv_obj_t* createSatelliteScreenForMode(int mode) {
    switch (mode) {
        case MODE_SAT_LIST:        return createSatListScreen();
        case MODE_SAT_PASSES:      return createSatPassesScreen();
        case MODE_SAT_PASS_DETAIL: return createSatPassDetailScreen();
        case MODE_SAT_LIVE:        return createSatLiveScreen();
        case MODE_SAT_SETTINGS:    return createSatSettingsScreen();
        default:                   return NULL;
    }
}

#endif // LV_SATELLITE_SCREENS_H
