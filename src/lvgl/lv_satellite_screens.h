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
#include "../satellites/sat_xmtrs.h"
#include "../satellites/sat_update.h"
#include "../settings/settings_satellites.h"

extern void onLVGLMenuSelect(int target_mode);
extern void onLVGLBackNavigation();
static void playAlertChirp();  // defined in lv_mode_integration.h

// ============================================
// Screen State
// ============================================

// Satellite list - one screen, four variants selected from the hub menu
enum SatListVariant {
    SAT_LIST_ALL = 0,   // full catalog A-Z, type-to-search
    SAT_LIST_MY,        // favorites only, sorted by next pass
    SAT_LIST_POPULAR,   // curated freq-table birds, sorted by next pass
    SAT_LIST_BYPASS     // full catalog sorted by next pass, type-to-search
};
static SatListVariant sat_list_variant = SAT_LIST_ALL;
static lv_obj_t* sat_list_table = NULL;
static lv_obj_t* sat_list_count_label = NULL;
static lv_obj_t* sat_list_info_label = NULL;      // search text / progress (left)
static lv_obj_t* sat_list_tle_label = NULL;       // TLE age (right)
static lv_obj_t* sat_list_empty_label = NULL;     // "no TLE data" message
static int sat_list_selected_row = 0;
static char sat_search_filter[13] = "";

// Where ESC from the passes screen returns (whichever list/window launched it)
int satPassesReturnMode = MODE_SAT_LIST;

// Background next-pass computation (shown in the NEXT PASS column).
// aos/los per catalog index: 0 = not computed, -1 = no pass found within the
// lookahead window. Favorites + curated birds always compute; the "by next
// pass" variant sweeps the whole catalog (favorites first).
static time_t sat_np_aos[MAX_SATELLITES];
static time_t sat_np_los[MAX_SATELLITES];
static lv_timer_t* sat_list_np_timer = NULL;
static int sat_np_current = -1;         // catalog index in-flight, -1 = idle

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

// Frequencies (transmitter list)
static lv_obj_t* sat_freqs_table = NULL;
static int sat_freqs_selected_row = 0;

// Live view
static lv_obj_t* sat_live_az_label = NULL;
static lv_obj_t* sat_live_el_label = NULL;
static lv_obj_t* sat_live_dist_label = NULL;
static lv_obj_t* sat_live_status_label = NULL;
static lv_timer_t* sat_live_timer = NULL;

// Settings
static lv_obj_t* sat_set_value_labels[5] = { NULL };

// Sky window ("what's up in the hour after <date time>")
#define SAT_WIN_MAX_RESULTS 40
#define SAT_WIN_MINUTES 60
static struct { int catIdx; SatPass pass; } sat_win_results[SAT_WIN_MAX_RESULTS];
static int sat_win_count = 0;
static int sat_win_scan = 0;            // next catalog index to evaluate
static bool sat_win_inflight = false;
static time_t sat_win_start = 0;        // UTC start of the 60-min window
static int sat_win_selected_row = 0;
static lv_obj_t* sat_win_table = NULL;
static lv_obj_t* sat_win_status_label = NULL;
static lv_obj_t* sat_win_date_val = NULL;
static lv_obj_t* sat_win_time_val = NULL;
static lv_timer_t* sat_win_timer = NULL;
static bool sat_win_focus_table = false;   // true = "Next 60 Minutes" entry

// ============================================
// Cleanup (registered for all satellite modes)
// ============================================

void cleanupSatelliteScreens() {
    if (sat_passes_timer) { lv_timer_del(sat_passes_timer); sat_passes_timer = NULL; }
    if (sat_detail_timer) { lv_timer_del(sat_detail_timer); sat_detail_timer = NULL; }
    if (sat_live_timer)   { lv_timer_del(sat_live_timer);   sat_live_timer = NULL; }
    if (sat_list_np_timer) { lv_timer_del(sat_list_np_timer); sat_list_np_timer = NULL; }
    if (sat_win_timer) { lv_timer_del(sat_win_timer); sat_win_timer = NULL; }
    sat_np_current = -1;
    sat_win_inflight = false;
    satSearch.active = false;

    sat_list_table = NULL;
    sat_list_count_label = NULL;
    sat_list_info_label = NULL;
    sat_list_tle_label = NULL;
    sat_list_empty_label = NULL;
    sat_passes_table = NULL;
    sat_passes_status_label = NULL;
    sat_detail_countdown_label = NULL;
    sat_freqs_table = NULL;
    sat_win_table = NULL;
    sat_win_status_label = NULL;
    sat_win_date_val = NULL;
    sat_win_time_val = NULL;
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

// Discard the in-flight ENTER press. Required before any screen navigation
// or dialog opened from an LV_EVENT_KEY handler: the KEY event fires on key
// DOWN, and without this the key UP is delivered as a CLICK to whatever
// widget is focused on the new screen/dialog (e.g. instantly activating the
// pass detail's LIVE VIEW button, or dismissing an alert unread).
static void satFlushEnterRelease() {
    lv_indev_t* indev = getLVGLKeypad();
    if (indev) lv_indev_wait_release(indev);
}

// Keep the highlighted row visible. Uses the exact per-row heights that
// lv_table maintains internally (lv_table_t.row_h) - any uniform-row-height
// estimate rounds or wraps wrong and the scroll position drifts away from
// the highlight a few pixels per row.
static void satScrollTableToRow(lv_obj_t* table, int row, int rowCount) {
    if (!table || rowCount <= 0) return;
    lv_table_t* tbl = (lv_table_t*)table;
    if (row >= (int)tbl->row_cnt) return;

    lv_coord_t rowTop = 0;
    for (int i = 0; i < row; i++) rowTop += tbl->row_h[i];
    lv_coord_t rowH = tbl->row_h[row];

    lv_coord_t viewH = lv_obj_get_height(table);
    lv_coord_t scrollY = lv_obj_get_scroll_y(table);
    if (rowTop < scrollY) {
        lv_obj_scroll_to_y(table, rowTop, LV_ANIM_OFF);
    } else if (rowTop + rowH > scrollY + viewH) {
        lv_obj_scroll_to_y(table, rowTop + rowH - viewH, LV_ANIM_OFF);
    }
    lv_obj_invalidate(table);
}

// ============================================
// Update Progress Overlay (non-blocking download UI)
// ============================================
// The download runs as a timer-driven job (sat_update.h), so the main loop
// keeps pumping LVGL: the spinner actually spins, progress text updates
// live, and the loop WDT stays fed. Blocking fetches froze all of that.

static lv_obj_t* sat_upd_overlay = NULL;
static lv_obj_t* sat_upd_stage_label = NULL;
static lv_obj_t* sat_upd_prog_label = NULL;
static lv_timer_t* sat_upd_timer = NULL;

static void satEnsureNextPassWorker();
static void satRefreshListTable();
static void satWinRestartScan();
static void sat_win_timer_cb(lv_timer_t* t);

static void satUpdateFinishUI(bool ok) {
    if (sat_upd_timer) { lv_timer_del(sat_upd_timer); sat_upd_timer = NULL; }
    if (sat_upd_overlay && lv_obj_is_valid(sat_upd_overlay)) lv_obj_del(sat_upd_overlay);
    sat_upd_overlay = NULL;
    sat_upd_stage_label = NULL;
    sat_upd_prog_label = NULL;

    if (ok) {
        // Catalog indexes changed - cached next-pass times are invalid
        memset(sat_np_aos, 0, sizeof(sat_np_aos));
        memset(sat_np_los, 0, sizeof(sat_np_los));
        beep(1000, 100);
        // Say where the cache landed - offline field use depends on it
        if (satTLEStorage == SAT_STORE_SD) showToast("Satellite data updated - saved to SD card");
        else if (satTLEStorage == SAT_STORE_FLASH) showToast("Satellite data updated - saved to internal flash");
        else showToast("Updated - NOT saved, lost at power-off!");
        // Refresh whichever satellite screen is currently visible
        if (sat_list_table && lv_obj_is_valid(sat_list_table)) {
            satRefreshListTable();
            satEnsureNextPassWorker();
        }
        if (sat_win_table && lv_obj_is_valid(sat_win_table)) {
            satWinRestartScan();
            if (!sat_win_timer) sat_win_timer = lv_timer_create(sat_win_timer_cb, 60, NULL);
        }
    } else {
        beep(400, 200);
        createAlertDialog("Update Failed", "Could not download satellite\ndata. Try again later.");
    }
}

static void sat_upd_timer_cb(lv_timer_t* t) {
    satUpdateJobTick(20);

    if (!satUpdateJobActive()) {
        satUpdateFinishUI(satUpd.finishedOk);
        return;
    }

    if (sat_upd_stage_label && lv_obj_is_valid(sat_upd_stage_label)) {
        const char* s = "";
        switch (satUpd.stage) {
            case SATUPD_TLE_AMATEUR: s = "Step 1 of 3: Orbits - amateur birds"; break;
            case SATUPD_TLE_ISS:     s = "Step 2 of 3: Orbits - ISS"; break;
            case SATUPD_XMTRS:       s = "Step 3 of 3: Frequencies (SatNOGS)"; break;
            default: break;
        }
        lv_label_set_text(sat_upd_stage_label, s);
    }
    if (sat_upd_prog_label && lv_obj_is_valid(sat_upd_prog_label)) {
        char buf[56];
        unsigned long kb = (unsigned long)(satUpd.stageBytes / 1024UL);
        if (satUpd.stage == SATUPD_XMTRS) {
            snprintf(buf, sizeof(buf), "%lu KB of ~2500 KB   %d freqs kept", kb, satXmtrCount);
        } else if (satUpd.stageTotal > 0) {
            snprintf(buf, sizeof(buf), "%lu of %d KB   %d satellites", kb, satUpd.stageTotal / 1024, satCatalog.count);
        } else {
            snprintf(buf, sizeof(buf), "%lu KB   %d satellites", kb, satCatalog.count);
        }
        lv_label_set_text(sat_upd_prog_label, buf);
    }
}

// Kick off the staged update with the progress overlay. Returns false only
// when it cannot start (no WiFi). Safe to call while already running.
static bool satStartUpdateUI() {
    if (satUpdateJobActive()) return true;
    if (WiFi.status() != WL_CONNECTED) {
        satFlushEnterRelease();
        playAlertChirp();
        createAlertDialog("WiFi Required", "Connect to WiFi first to\ndownload satellite data.");
        return false;
    }
    if (!satUpdateJobStart()) return false;

    // Full-screen modal on the top layer - survives screen navigation
    sat_upd_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(sat_upd_overlay, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(sat_upd_overlay, 0, 0);
    lv_obj_set_style_bg_color(sat_upd_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sat_upd_overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(sat_upd_overlay, 0, 0);
    lv_obj_set_style_radius(sat_upd_overlay, 0, 0);
    lv_obj_add_flag(sat_upd_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(sat_upd_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* card = lv_obj_create(sat_upd_overlay);
    lv_obj_set_size(card, 380, 210);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* spinner = lv_spinner_create(card, 1000, 60);
    lv_obj_set_size(spinner, 44, 44);
    lv_obj_align(spinner, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "UPDATING SATELLITE DATA");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 52);

    sat_upd_stage_label = lv_label_create(card);
    lv_label_set_text(sat_upd_stage_label, "Connecting...");
    lv_obj_set_style_text_font(sat_upd_stage_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sat_upd_stage_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(sat_upd_stage_label, LV_ALIGN_TOP_MID, 0, 82);

    sat_upd_prog_label = lv_label_create(card);
    lv_label_set_text(sat_upd_prog_label, "");
    lv_obj_set_style_text_font(sat_upd_prog_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sat_upd_prog_label, LV_COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(sat_upd_prog_label, LV_ALIGN_TOP_MID, 0, 106);

    lv_obj_t* note = lv_label_create(card);
    lv_label_set_text(note, "Downloading orbits + frequencies (~2.5 MB).\nThis can take a couple of minutes - hang tight.");
    lv_obj_set_style_text_font(note, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(note, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(note, LV_ALIGN_BOTTOM_MID, 0, -2);

    sat_upd_timer = lv_timer_create(sat_upd_timer_cb, 30, NULL);
    return true;
}

// Auto-download on screen entry when nothing has ever been grabbed and WiFi
// is available - a fresh device shouldn't need to find Update TLEs first.
// Retries at most once a minute so an outage doesn't nag on every entry.
static void satAutoFetchIfEmpty() {
    static uint32_t lastAttempt = 0;
    if (satCatalog.valid || satUpdateJobActive()) return;
    if (WiFi.status() != WL_CONNECTED) return;
    if (lastAttempt != 0 && (millis() - lastAttempt) < 60000UL) return;
    lastAttempt = millis();
    satStartUpdateUI();
}

// ============================================
// Favorite Next-Pass Column (background compute)
// ============================================

// "14:32 in 1h05m" / "NOW" / "none" for a favorite's cached next pass
static void satFmtNextPassCell(int catIdx, char* buf, size_t n) {
    buf[0] = '\0';
    if (!ntpSynced) return;
    time_t aos = sat_np_aos[catIdx];
    time_t los = sat_np_los[catIdx];
    time_t now = time(nullptr);
    if (aos == (time_t)-1) { strlcpy(buf, "none", n); return; }
    if (aos == 0 || now > los) { strlcpy(buf, "...", n); return; }  // computing / stale
    if (now >= aos) { strlcpy(buf, "NOW", n); return; }
    char lt[12];
    satFmtLocalTime(aos, lt, sizeof(lt));
    long mins = (long)(aos - now) / 60;
    if (mins < 60) snprintf(buf, n, "%s in %ldm", lt, mins);
    else snprintf(buf, n, "%s in %ldh%02ldm", lt, mins / 60, mins % 60);
}

// Does this catalog index want a next-pass value? Favorites and the curated
// birds always do (cheap, and their lists sort by it); the full-catalog
// "by next pass" variant wants everything.
static bool satNextPassWanted(int idx) {
    uint32_t norad = satCatalog.sats[idx].norad;
    if (isSatFavorite(norad) || lookupSatFreqs(norad) != NULL) return true;
    return sat_list_variant == SAT_LIST_BYPASS;
}

// Membership predicates for buildSatDisplayList
static bool satIncludeFavorite(uint32_t norad) { return isSatFavorite(norad); }
static bool satIncludePopular(uint32_t norad) { return lookupSatFreqs(norad) != NULL; }

// Next catalog index needing computation (favorites first), or -1 when
// everything wanted is current. -1 ("no pass in window") entries stay sticky
// until a TLE refresh - recomputing them is the most expensive case.
static int satNextComputeIndex() {
    if (!satCatalog.valid || !ntpSynced) return -1;
    double lat, lon;
    if (!gridToLatLon(satEffectiveGrid(), &lat, &lon)) return -1;

    time_t now = time(nullptr);
    for (int fav = 1; fav >= 0; fav--) {
        for (int i = 0; i < satCatalog.count; i++) {
            if ((int)isSatFavorite(satCatalog.sats[i].norad) != fav) continue;
            if (!satNextPassWanted(i)) continue;
            if (sat_np_aos[i] == (time_t)-1) continue;               // known: none in window
            if (sat_np_aos[i] > 0 && sat_np_los[i] >= now) continue; // still current
            return i;
        }
    }
    return -1;
}

// Compute progress for the header label: done/wanted
static void satNextPassProgress(int* done, int* wanted) {
    *done = 0;
    *wanted = 0;
    if (!satCatalog.valid) return;
    for (int i = 0; i < satCatalog.count; i++) {
        if (!satNextPassWanted(i)) continue;
        (*wanted)++;
        if (sat_np_aos[i] != 0) (*done)++;
    }
}

static void satRefreshListTable();  // forward
static void sat_list_np_timer_cb(lv_timer_t* t);  // forward

// Make sure the worker timer is running at full speed.
static void satEnsureNextPassWorker() {
    if (!sat_list_np_timer) {
        sat_list_np_timer = lv_timer_create(sat_list_np_timer_cb, 60, NULL);
    } else {
        lv_timer_set_period(sat_list_np_timer, 60);
    }
}

// Chunked worker: one satellite at a time through the shared pass-search
// state machine, stopping at the first pass found. When nothing needs
// computing, drop to a slow 30s tick that keeps the relative times fresh
// and picks up birds whose pass has ended.
static void sat_list_np_timer_cb(lv_timer_t* t) {
    if (!sat_list_table || !lv_obj_is_valid(sat_list_table)) return;

    if (sat_np_current < 0) {
        sat_np_current = satNextComputeIndex();
        if (sat_np_current < 0) {
            satRefreshListTable();  // keep relative times / NOW state fresh
            lv_timer_set_period(t, 30000);
            return;
        }
        lv_timer_set_period(t, 60);
        if (!satStartPassSearch(sat_np_current)) {
            sat_np_aos[sat_np_current] = (time_t)-1;
            sat_np_los[sat_np_current] = (time_t)-1;
            sat_np_current = -1;
            return;
        }
    }

    satPassSearchStep(25);
    if (satSearch.count > 0 || satSearch.done) {
        if (satSearch.count > 0) {
            sat_np_aos[sat_np_current] = satSearch.passes[0].aos;
            sat_np_los[sat_np_current] = satSearch.passes[0].los;
        } else {
            sat_np_aos[sat_np_current] = (time_t)-1;
            sat_np_los[sat_np_current] = (time_t)-1;
        }
        satSearch.active = false;
        satSearch.done = true;
        sat_np_current = -1;
        satRefreshListTable();
    }
}

// ============================================
// Satellite List Screen
// ============================================

static void satUpdateListHeaderLabels() {
    if (sat_list_count_label && lv_obj_is_valid(sat_list_count_label)) {
        char buf[24];
        static const char* variantTitles[4] = { "ALL SATELLITES", "MY SATS", "POPULAR BIRDS", "NEXT PASSES" };
        snprintf(buf, sizeof(buf), "%s (%d)", variantTitles[sat_list_variant], satDisplayCount);
        lv_label_set_text(sat_list_count_label, buf);
    }
    if (sat_list_info_label && lv_obj_is_valid(sat_list_info_label)) {
        char buf[48];
        buf[0] = '\0';
        if (sat_search_filter[0] != '\0') {
            snprintf(buf, sizeof(buf), "Search: %s_", sat_search_filter);
        } else if (sat_list_variant != SAT_LIST_ALL) {
            int done = 0, wanted = 0;
            satNextPassProgress(&done, &wanted);
            if (done < wanted) {
                snprintf(buf, sizeof(buf), "Computing passes %d/%d", done, wanted);
            }
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
            const char* store = (satTLEStorage == SAT_STORE_SD) ? "SD"
                              : (satTLEStorage == SAT_STORE_FLASH) ? "flash" : "RAM!";
            snprintf(buf, sizeof(buf), "TLE age: %dd (%s)", age, store);
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

    // Remember which satellite the cursor is on - resorting (next-pass mode
    // reorders as results land) must not teleport the selection.
    uint32_t selNorad = 0;
    if (satDisplayCount > 0 && sat_list_selected_row < satDisplayCount) {
        selNorad = satCatalog.sats[satDisplayIdx[sat_list_selected_row]].norad;
    }

    const time_t* sortKeys = (sat_list_variant == SAT_LIST_ALL) ? NULL : sat_np_aos;
    SatIncludeFn include = NULL;
    if (sat_list_variant == SAT_LIST_MY) include = satIncludeFavorite;
    else if (sat_list_variant == SAT_LIST_POPULAR) include = satIncludePopular;
    buildSatDisplayList(sat_search_filter, sortKeys, include);
    satUpdateListHeaderLabels();

    if (selNorad != 0) {
        for (int i = 0; i < satDisplayCount; i++) {
            if (satCatalog.sats[satDisplayIdx[i]].norad == selNorad) {
                sat_list_selected_row = i;
                break;
            }
        }
    }

    bool empty = (satDisplayCount == 0);
    if (sat_list_empty_label && lv_obj_is_valid(sat_list_empty_label)) {
        if (empty) lv_obj_clear_flag(sat_list_empty_label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(sat_list_empty_label, LV_OBJ_FLAG_HIDDEN);
        const char* emptyMsg;
        if (!satCatalog.valid) {
            emptyMsg = "No satellite data yet.\nConnect to WiFi and re-open this list -\nTLEs download automatically.";
        } else if (sat_list_variant == SAT_LIST_MY && sat_search_filter[0] == '\0') {
            emptyMsg = "No favorites yet.\nAdd them with F from any other list.";
        } else {
            emptyMsg = "No satellites match the search";
        }
        lv_label_set_text(sat_list_empty_label, emptyMsg);
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
        bool fav = isSatFavorite(e.norad);
        lv_table_set_cell_value(sat_list_table, i, 0, fav ? "*" : "");
        lv_table_set_cell_value(sat_list_table, i, 1, e.name);
        char next[24] = "";
        if (sat_list_variant != SAT_LIST_ALL || sat_np_aos[satDisplayIdx[i]] != 0) {
            satFmtNextPassCell(satDisplayIdx[i], next, sizeof(next));
        }
        lv_table_set_cell_value(sat_list_table, i, 2, next);
    }

    if (sat_list_selected_row >= satDisplayCount) {
        sat_list_selected_row = satDisplayCount - 1;
    }
    if (sat_list_selected_row < 0) sat_list_selected_row = 0;
    lv_obj_invalidate(sat_list_table);
}

static void sat_list_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    if (sat_upd_overlay) { lv_event_stop_processing(e); return; }  // modal update in progress
    uint32_t key = lv_event_get_key(e);
    bool searchable = (sat_list_variant == SAT_LIST_ALL || sat_list_variant == SAT_LIST_BYPASS);

    // Favorite toggle: F on the curated lists (they don't type), RIGHT on the
    // searchable lists (letters there go to the search filter).
    bool favKey = searchable ? (key == LV_KEY_RIGHT)
                             : (key == 'F' || key == 'f');
    if (favKey) {
        if (satDisplayCount > 0 && sat_list_selected_row < satDisplayCount) {
            toggleSatFavorite(satCatalog.sats[satDisplayIdx[sat_list_selected_row]].norad);
            beep(800, 60);
            satEnsureNextPassWorker();
            satRefreshListTable();
        }
        lv_event_stop_processing(e);
        return;
    }

    // Type-to-search on the full-catalog lists: letters/digits filter live
    if (searchable) {
        if (key == LV_KEY_BACKSPACE) {
            int len = strlen(sat_search_filter);
            if (len > 0) {
                sat_search_filter[len - 1] = '\0';
                satRefreshListTable();
            }
            lv_event_stop_processing(e);
            return;
        }
        if (key >= 32 && key < 127 && key != LV_KEY_ENTER) {
            int len = strlen(sat_search_filter);
            if (len < (int)sizeof(sat_search_filter) - 1) {
                sat_search_filter[len] = (char)toupper((int)key);
                sat_search_filter[len + 1] = '\0';
                sat_list_selected_row = 0;
                satRefreshListTable();
                satScrollTableToRow(sat_list_table, 0, satDisplayCount);
            }
            lv_event_stop_processing(e);
            return;
        }
    }

    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        if (sat_list_selected_row > 0) {
            sat_list_selected_row--;
            satScrollTableToRow(sat_list_table, sat_list_selected_row, satDisplayCount);
        }
        lv_event_stop_processing(e);
        return;
    }
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        if (sat_list_selected_row < satDisplayCount - 1) {
            sat_list_selected_row++;
            satScrollTableToRow(sat_list_table, sat_list_selected_row, satDisplayCount);
        }
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_ENTER) {
        if (satDisplayCount > 0 && sat_list_selected_row < satDisplayCount) {
            double lat, lon;
            if (!gridToLatLon(satEffectiveGrid(), &lat, &lon)) {
                satFlushEnterRelease();
                playAlertChirp();
                createAlertDialog("Grid Square Needed",
                    "Set your Maidenhead grid in\nSatellites > Settings so passes\ncan be computed for your location.");
            } else if (!ntpSynced) {
                satFlushEnterRelease();
                playAlertChirp();
                createAlertDialog("Clock Not Set",
                    "Time is not synced yet.\nConnect to WiFi so NTP can\nset the clock, then retry.");
            } else {
                sat_selected_catalog_idx = satDisplayIdx[sat_list_selected_row];
                // The passes screen owns the shared pass-search state machine;
                // stop the background next-pass worker before handing over.
                if (sat_list_np_timer) { lv_timer_del(sat_list_np_timer); sat_list_np_timer = NULL; }
                sat_np_current = -1;
                // ESC from the passes screen returns to this list variant
                static const int variantModes[4] = { MODE_SAT_LIST, MODE_SAT_MY, MODE_SAT_POPULAR, MODE_SAT_BYPASS };
                satPassesReturnMode = variantModes[sat_list_variant];
                satFlushEnterRelease();
                onLVGLMenuSelect(MODE_SAT_PASSES);
            }
        }
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_ESC) {
        if (sat_search_filter[0] != '\0') {
            // First ESC clears an active search, second one leaves the screen
            sat_search_filter[0] = '\0';
            satRefreshListTable();
        } else {
            onLVGLBackNavigation();
        }
        lv_event_stop_processing(e);
        return;
    }

    // Swallow stray printable keys so they don't reach LVGL defaults
    if (key >= 32 && key < 127) {
        lv_event_stop_processing(e);
    }
}

static lv_obj_t* createSatListScreenVariant(SatListVariant variant) {
    clearNavigationGroup();
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    initSatCatalog();
    loadSatSettings();
    loadSatFavorites();
    if (!satCatalog.valid) {
        satLoadTLEsFromStorage();
    }
    satLoadXmtrsFromStorage();
    satAutoFetchIfEmpty();

    // Entering a different list than last time starts clean
    if (variant != sat_list_variant) {
        sat_search_filter[0] = '\0';
        sat_list_selected_row = 0;
    }
    sat_list_variant = variant;

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

    const char* colNames[3] = { "FAV", "NAME", "NEXT PASS" };
    int colX[3] = { 4, 48, 292 };
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
    lv_table_set_col_width(sat_list_table, 0, 40);
    lv_table_set_col_width(sat_list_table, 1, 244);
    lv_table_set_col_width(sat_list_table, 2, 174);

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

    static const char* variantFooters[4] = {
        "Type to Search   ENTER Passes   RIGHT Fav   ESC Back",   // ALL
        "ENTER Passes   F Remove Favorite   ESC Back",            // MY
        "ENTER Passes   F Favorite   ESC Back",                   // POPULAR
        "Type to Search   ENTER Passes   RIGHT Fav   ESC Back",   // BYPASS
    };
    satCreateFooter(screen, variantFooters[variant]);

    satRefreshListTable();

    // Kick off background next-pass computation
    satEnsureNextPassWorker();
    return screen;
}

// ============================================
// Satellites Hub Menu
// ============================================

static const LVMenuItem satMenuItems[] = {
    MENU_ITEM_LV(LV_SYMBOL_HOME, "My Sats", MODE_SAT_MY),
    MENU_ITEM_FA(FA_EXTRA_SATELLITE_DISH, "Popular Birds", MODE_SAT_POPULAR),
    MENU_ITEM_LV(LV_SYMBOL_BELL, "Next 60 Minutes", MODE_SAT_WINDOW_NOW),
    MENU_ITEM_LV(LV_SYMBOL_EDIT, "Plan Date & Time", MODE_SAT_WINDOW),
    MENU_ITEM_LV(LV_SYMBOL_LIST, "All Satellites", MODE_SAT_LIST),
    MENU_ITEM_LV(LV_SYMBOL_LOOP, "By Next Pass", MODE_SAT_BYPASS),
    MENU_ITEM_LV(LV_SYMBOL_DOWNLOAD, "Update TLEs", MODE_SAT_TLE_UPDATE),
    MENU_ITEM_LV(LV_SYMBOL_SETTINGS, "Settings", MODE_SAT_SETTINGS),
};
#define SAT_MENU_COUNT 8

lv_obj_t* createSatMenuScreen() {
    return createMenuScreen("SATELLITES", satMenuItems, SAT_MENU_COUNT);
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
    if (sat_upd_overlay) { lv_event_stop_processing(e); return; }  // modal update in progress
    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        if (sat_passes_selected_row > 0) {
            sat_passes_selected_row--;
            satScrollTableToRow(sat_passes_table, sat_passes_selected_row, satSearch.count);
        }
        lv_event_stop_processing(e);
        return;
    }
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        if (sat_passes_selected_row < satSearch.count - 1) {
            sat_passes_selected_row++;
            satScrollTableToRow(sat_passes_table, sat_passes_selected_row, satSearch.count);
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
            satFlushEnterRelease();  // else the release clicks LIVE VIEW on the detail screen
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

static void sat_detail_freqs_btn_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    onLVGLMenuSelect(MODE_SAT_FREQS);
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

    // Frequency reference card: curated table first (hand-written operating
    // notes), otherwise the best SatNOGS transmitter for this bird
    const SatFreqInfo* fi = lookupSatFreqs(norad);
    satLoadXmtrsFromStorage();
    const SatTransmitter* bx = satBestXmtr(norad);
    if (fi || bx) {
        lv_obj_t* fcard = lv_obj_create(content);
        lv_obj_set_size(fcard, lv_pct(100), LV_SIZE_CONTENT);
        applyCardStyle(fcard);
        lv_obj_set_style_pad_all(fcard, 8, 0);
        lv_obj_set_style_pad_row(fcard, 2, 0);
        lv_obj_set_layout(fcard, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(fcard, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(fcard, LV_OBJ_FLAG_SCROLLABLE);

        if (fi) {
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
        } else {
            char up[14], dn2[14];
            satFmtMHz(bx->upHz, up, sizeof(up));
            satFmtMHz(bx->downHz, dn2, sizeof(dn2));
            if (bx->upHz) {
                snprintf(buf, sizeof(buf), "UP  %s MHz", up);
                lv_obj_t* l = lv_label_create(fcard);
                lv_label_set_text(l, buf);
                lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(l, LV_COLOR_TEXT_PRIMARY, 0);
            }
            snprintf(buf, sizeof(buf), "DN  %s MHz   (%s)", dn2, bx->mode[0] ? bx->mode : "?");
            lv_obj_t* dn = lv_label_create(fcard);
            lv_label_set_text(dn, buf);
            lv_obj_set_style_text_font(dn, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(dn, LV_COLOR_SUCCESS, 0);

            lv_obj_t* note = lv_label_create(fcard);
            lv_label_set_text(note, bx->desc);
            lv_obj_set_style_text_font(note, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(note, LV_COLOR_TEXT_SECONDARY, 0);
            lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(note, SCREEN_WIDTH - 60);
        }

        int nx = satXmtrCountFor(norad);
        if (nx > 1) {
            snprintf(buf, sizeof(buf), "%d transmitters known - see FREQS", nx);
            lv_obj_t* more = lv_label_create(fcard);
            lv_label_set_text(more, buf);
            lv_obj_set_style_text_font(more, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(more, LV_COLOR_TEXT_TERTIARY, 0);
        }
    }

    // Button row: LIVE VIEW + FREQS
    lv_obj_t* btn_row = lv_obj_create(content);
    lv_obj_set_size(btn_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_style_pad_column(btn_row, 10, 0);
    lv_obj_set_layout(btn_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    // LIVE VIEW button (also the ESC anchor)
    lv_obj_t* live_btn = lv_obj_create(btn_row);
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

    // The two buttons sit side by side: LEFT/RIGHT must move between them,
    // so they use grid_nav_handler (2 columns), not linear_nav_handler
    static lv_obj_t* detail_btns[2];
    static int detail_btn_count = 0;
    static NavGridContext detail_nav_ctx = { detail_btns, &detail_btn_count, 2 };
    detail_btn_count = 0;

    lv_obj_add_event_cb(live_btn, sat_detail_live_btn_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(live_btn, grid_nav_handler, LV_EVENT_KEY, &detail_nav_ctx);
    detail_btns[detail_btn_count++] = live_btn;
    addNavigableWidget(live_btn);

    // FREQS button - full transmitter list for this bird
    lv_obj_t* freq_btn = lv_obj_create(btn_row);
    lv_obj_set_size(freq_btn, 130, 34);
    lv_obj_add_flag(freq_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(freq_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(freq_btn, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_radius(freq_btn, 8, 0);
    lv_obj_set_style_border_width(freq_btn, 1, 0);
    lv_obj_set_style_border_color(freq_btn, LV_COLOR_BORDER_SUBTLE, 0);
    lv_obj_set_style_border_width(freq_btn, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(freq_btn, LV_COLOR_ACCENT_PRIMARY, LV_STATE_FOCUSED);
    lv_obj_t* freq_lbl = lv_label_create(freq_btn);
    lv_label_set_text(freq_lbl, LV_SYMBOL_AUDIO " FREQS");
    lv_obj_set_style_text_font(freq_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(freq_lbl, LV_COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(freq_lbl);

    lv_obj_add_event_cb(freq_btn, sat_detail_freqs_btn_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(freq_btn, grid_nav_handler, LV_EVENT_KEY, &detail_nav_ctx);
    detail_btns[detail_btn_count++] = freq_btn;
    addNavigableWidget(freq_btn);

    satCreateFooter(screen, "LEFT/RIGHT Select   ENTER Open   ESC Back");

    sat_detail_countdown_cb(NULL);
    sat_detail_timer = lv_timer_create(sat_detail_countdown_cb, 1000, NULL);
    return screen;
}

// ============================================
// Frequencies Screen (all transmitters for the selected bird)
// ============================================

static void sat_freqs_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    if (sat_upd_overlay) { lv_event_stop_processing(e); return; }  // modal update in progress
    uint32_t key = lv_event_get_key(e);
    uint32_t norad = (sat_selected_catalog_idx >= 0 && sat_selected_catalog_idx < satCatalog.count)
        ? satCatalog.sats[sat_selected_catalog_idx].norad : 0;
    int rows = satXmtrCountFor(norad);

    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        if (sat_freqs_selected_row > 0) {
            sat_freqs_selected_row--;
            satScrollTableToRow(sat_freqs_table, sat_freqs_selected_row, rows);
        }
        lv_event_stop_processing(e);
        return;
    }
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        if (sat_freqs_selected_row < rows - 1) {
            sat_freqs_selected_row++;
            satScrollTableToRow(sat_freqs_table, sat_freqs_selected_row, rows);
        }
        lv_event_stop_processing(e);
        return;
    }
    if (key == LV_KEY_ENTER) {
        lv_event_stop_processing(e);  // nothing to activate
        return;
    }
}

lv_obj_t* createSatFreqsScreen() {
    clearNavigationGroup();
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    satLoadXmtrsFromStorage();
    sat_freqs_selected_row = 0;

    uint32_t norad = 0;
    const char* satName = "FREQUENCIES";
    if (sat_selected_catalog_idx >= 0 && sat_selected_catalog_idx < satCatalog.count) {
        norad = satCatalog.sats[sat_selected_catalog_idx].norad;
        satName = satCatalog.sats[sat_selected_catalog_idx].name;
    }
    satCreateHeader(screen, satName);

    // Curated operating note (tones, schedules) when we have one
    const SatFreqInfo* fi = lookupSatFreqs(norad);
    int table_top = HEADER_HEIGHT + 4;
    if (fi) {
        lv_obj_t* note = lv_label_create(screen);
        char nbuf[96];
        snprintf(nbuf, sizeof(nbuf), "%s", fi->note);
        lv_label_set_text(note, nbuf);
        lv_obj_set_style_text_font(note, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(note, LV_COLOR_WARNING, 0);
        lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(note, SCREEN_WIDTH - 30);
        lv_obj_set_pos(note, 15, table_top);
        table_top += 34;
    }

    // Column headers
    lv_obj_t* header_bar = lv_obj_create(screen);
    lv_obj_set_size(header_bar, SCREEN_WIDTH - 20, 24);
    lv_obj_set_pos(header_bar, 10, table_top);
    lv_obj_set_style_bg_color(header_bar, getThemeColors()->bg_layer2, 0);
    lv_obj_set_style_bg_opa(header_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header_bar, 0, 0);
    lv_obj_set_style_pad_all(header_bar, 0, 0);
    lv_obj_clear_flag(header_bar, LV_OBJ_FLAG_SCROLLABLE);

    const char* colNames[4] = { "TRANSMITTER", "UP MHz", "DN MHz", "MODE" };
    int colX[4] = { 8, 214, 302, 390 };
    for (int i = 0; i < 4; i++) {
        lv_obj_t* h = lv_label_create(header_bar);
        lv_label_set_text(h, colNames[i]);
        lv_obj_set_style_text_font(h, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(h, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_set_pos(h, colX[i], 3);
    }

    int table_y = table_top + 26;
    int table_height = SCREEN_HEIGHT - table_y - FOOTER_HEIGHT - 5;

    sat_freqs_table = lv_table_create(screen);
    lv_obj_set_size(sat_freqs_table, SCREEN_WIDTH - 20, table_height);
    lv_obj_set_pos(sat_freqs_table, 10, table_y);
    satStyleTable(sat_freqs_table);
    lv_table_set_col_cnt(sat_freqs_table, 4);
    lv_table_set_col_width(sat_freqs_table, 0, 206);
    lv_table_set_col_width(sat_freqs_table, 1, 88);
    lv_table_set_col_width(sat_freqs_table, 2, 88);
    lv_table_set_col_width(sat_freqs_table, 3, 78);

    int rows = satXmtrCountFor(norad);
    if (rows == 0) {
        lv_table_set_row_cnt(sat_freqs_table, 1);
        lv_table_set_cell_value(sat_freqs_table, 0, 0,
            satXmtrCount == 0 ? "No frequency data downloaded yet.\nRun Update TLEs with WiFi."
                              : "No active transmitters known\nfor this satellite (SatNOGS)");
        for (int c = 1; c < 4; c++) lv_table_set_cell_value(sat_freqs_table, 0, c, "");
    } else {
        lv_table_set_row_cnt(sat_freqs_table, rows);
        for (int i = 0; i < rows; i++) {
            const SatTransmitter* x = satXmtrFor(norad, i);
            if (!x) break;
            char up[14], dn[14], mode[20];
            satFmtMHz(x->upHz, up, sizeof(up));
            satFmtMHz(x->downHz, dn, sizeof(dn));
            snprintf(mode, sizeof(mode), "%s%s", x->mode[0] ? x->mode : "?", x->invert ? " inv" : "");
            lv_table_set_cell_value(sat_freqs_table, i, 0, x->desc);
            lv_table_set_cell_value(sat_freqs_table, i, 1, up);
            lv_table_set_cell_value(sat_freqs_table, i, 2, dn);
            lv_table_set_cell_value(sat_freqs_table, i, 3, mode);
        }
    }

    lv_obj_add_event_cb(sat_freqs_table, sat_table_highlight_cb, LV_EVENT_DRAW_PART_BEGIN, &sat_freqs_selected_row);
    lv_obj_add_flag(sat_freqs_table, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sat_freqs_table, sat_freqs_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(sat_freqs_table, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(sat_freqs_table);

    satCreateFooter(screen, "UP/DN Scroll   ESC Back");
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

    // Frequency reference - operators need the downlink while pointing
    uint32_t liveNorad = (sat_selected_catalog_idx >= 0 && sat_selected_catalog_idx < satCatalog.count)
        ? satCatalog.sats[sat_selected_catalog_idx].norad : 0;
    const SatFreqInfo* lfi = lookupSatFreqs(liveNorad);
    const SatTransmitter* lbx = lfi ? NULL : satBestXmtr(liveNorad);
    if (lfi || lbx) {
        char fbuf[80];
        if (lfi) {
            if (lfi->uplink[0] != '\0') {
                snprintf(fbuf, sizeof(fbuf), "DN %s   UP %s", lfi->downlink, lfi->uplink);
            } else {
                snprintf(fbuf, sizeof(fbuf), "DN %s", lfi->downlink);
            }
        } else {
            char up[14], dn[14];
            satFmtMHz(lbx->upHz, up, sizeof(up));
            satFmtMHz(lbx->downHz, dn, sizeof(dn));
            if (lbx->upHz) snprintf(fbuf, sizeof(fbuf), "DN %s   UP %s", dn, up);
            else snprintf(fbuf, sizeof(fbuf), "DN %s", dn);
        }
        lv_obj_t* freq_lbl = lv_label_create(screen);
        lv_label_set_text(freq_lbl, fbuf);
        lv_obj_set_style_text_font(freq_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(freq_lbl, LV_COLOR_SUCCESS, 0);
        lv_obj_align(freq_lbl, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 163);
    }

    // Hint
    lv_obj_t* hint = lv_label_create(screen);
    lv_label_set_text(hint, "Point the antenna: azimuth is compass heading, elevation is tilt");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 188);

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
// Sky Window Screen (passes in the hour after a chosen time)
// ============================================

static void satWinUpdateChipLabels() {
    char buf[16];
    if (sat_win_date_val && lv_obj_is_valid(sat_win_date_val)) {
        satFmtLocalDate(sat_win_start, buf, sizeof(buf));
        lv_label_set_text(sat_win_date_val, buf);
    }
    if (sat_win_time_val && lv_obj_is_valid(sat_win_time_val)) {
        char zt[12];
        satFmtLocalTime(sat_win_start, buf, sizeof(buf));
        satFmtZuluTime(sat_win_start, zt, sizeof(zt));
        char both[24];
        snprintf(both, sizeof(both), "%s (%s)", buf, zt);
        lv_label_set_text(sat_win_time_val, both);
    }
}

static void satWinUpdateStatus() {
    if (!sat_win_status_label || !lv_obj_is_valid(sat_win_status_label)) return;
    char buf[64];
    if (!satCatalog.valid) {
        strlcpy(buf, "No TLE data - update from the satellite list", sizeof(buf));
    } else if (sat_win_scan < satCatalog.count) {
        snprintf(buf, sizeof(buf), "Scanning %d/%d... %d up", sat_win_scan, satCatalog.count, sat_win_count);
    } else {
        snprintf(buf, sizeof(buf), "%d satellites in the %d min after start (min el %d)",
                 sat_win_count, SAT_WIN_MINUTES, satSettings.minElevation);
    }
    lv_label_set_text(sat_win_status_label, buf);
}

static void satWinRefreshTable() {
    if (!sat_win_table || !lv_obj_is_valid(sat_win_table)) return;
    satWinUpdateStatus();

    if (sat_win_count == 0) {
        lv_table_set_row_cnt(sat_win_table, 1);
        lv_table_set_cell_value(sat_win_table, 0, 0,
            (sat_win_scan >= satCatalog.count) ? "Nothing above the horizon in this window" : "");
        for (int c = 1; c < 5; c++) lv_table_set_cell_value(sat_win_table, 0, c, "");
        return;
    }

    lv_table_set_row_cnt(sat_win_table, sat_win_count);
    for (int i = 0; i < sat_win_count; i++) {
        SatPass& p = sat_win_results[i].pass;
        char buf[32];

        lv_table_set_cell_value(sat_win_table, i, 0, satCatalog.sats[sat_win_results[i].catIdx].name);

        char lt[12];
        satFmtLocalTime(p.aos < sat_win_start ? sat_win_start : p.aos, lt, sizeof(lt));
        // A pass already in progress at window start shows the start time
        snprintf(buf, sizeof(buf), "%s%s", (p.aos < sat_win_start) ? "<" : "", lt);
        lv_table_set_cell_value(sat_win_table, i, 1, buf);

        long durMin = (long)(p.los - p.aos + 30) / 60;
        snprintf(buf, sizeof(buf), "%ldm", durMin);
        lv_table_set_cell_value(sat_win_table, i, 2, buf);

        snprintf(buf, sizeof(buf), "%.0f", p.maxEl);
        lv_table_set_cell_value(sat_win_table, i, 3, buf);

        snprintf(buf, sizeof(buf), "%s>%s", azToCompass(p.aosAz), azToCompass(p.losAz));
        lv_table_set_cell_value(sat_win_table, i, 4, buf);
    }

    if (sat_win_selected_row >= sat_win_count) sat_win_selected_row = sat_win_count - 1;
    if (sat_win_selected_row < 0) sat_win_selected_row = 0;
    lv_obj_invalidate(sat_win_table);
}

// Restart the catalog scan for the current window start
static void satWinRestartScan() {
    sat_win_count = 0;
    sat_win_scan = 0;
    sat_win_selected_row = 0;
    sat_win_inflight = false;
    satSearch.active = false;
    if (sat_win_timer) lv_timer_set_period(sat_win_timer, 60);
    satWinRefreshTable();
}

static void satWinInsertResult(int catIdx, const SatPass* p) {
    if (sat_win_count >= SAT_WIN_MAX_RESULTS) return;
    int pos = sat_win_count;
    while (pos > 0 && sat_win_results[pos - 1].pass.aos > p->aos) {
        sat_win_results[pos] = sat_win_results[pos - 1];
        pos--;
    }
    sat_win_results[pos].catIdx = catIdx;
    sat_win_results[pos].pass = *p;
    sat_win_count++;
}

// Chunked scan: one satellite at a time through the shared search machine
// over [start, start+60min] (with margin so a pass peaking just past the end
// is still found), keeping passes that overlap the window.
static void sat_win_timer_cb(lv_timer_t* t) {
    if (!sat_win_table || !lv_obj_is_valid(sat_win_table)) return;
    if (!satCatalog.valid || sat_win_scan >= satCatalog.count) {
        lv_timer_set_period(t, 60000);
        return;
    }

    if (!sat_win_inflight) {
        if (!satStartPassSearchAt(sat_win_scan, sat_win_start, SAT_WIN_MINUTES / 60.0 + 0.25)) {
            sat_win_scan++;
            return;
        }
        sat_win_inflight = true;
    }

    satPassSearchStep(25);
    if (satSearch.count > 0 || satSearch.done) {
        time_t winEnd = sat_win_start + SAT_WIN_MINUTES * 60;
        for (int i = 0; i < satSearch.count; i++) {
            SatPass& p = satSearch.passes[i];
            if (p.aos <= winEnd && p.los >= sat_win_start) {
                satWinInsertResult(sat_win_scan, &p);
                break;  // one entry per satellite
            }
        }
        satSearch.active = false;
        satSearch.done = true;
        sat_win_inflight = false;
        sat_win_scan++;
        // Refresh every few birds (and at the end) to keep redraw cost down
        if ((sat_win_scan % 4) == 0 || sat_win_scan >= satCatalog.count) {
            satWinRefreshTable();
        } else {
            satWinUpdateStatus();
        }
    }
}

// Round up to the next quarter hour
static time_t satWinRoundUp(time_t v) {
    return ((v + 899) / 900) * 900;
}

// Date/time chip adjust: row 0 = date (+/- 1 day), row 1 = time (+/- 15 min)
static void sat_win_chip_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    if (sat_upd_overlay) { lv_event_stop_processing(e); return; }  // modal update in progress
    uint32_t key = lv_event_get_key(e);
    if (key != LV_KEY_LEFT && key != LV_KEY_RIGHT) return;

    int row = (int)(intptr_t)lv_event_get_user_data(e);
    long delta = (row == 0) ? 86400L : 900L;
    if (key == LV_KEY_LEFT) delta = -delta;

    time_t now = time(nullptr);
    time_t v = sat_win_start + delta;
    if (v < now) v = satWinRoundUp(now);                    // no past windows
    if (v > now + 7L * 86400L) v = sat_win_start;           // TLEs go stale past ~a week
    if (v != sat_win_start) {
        sat_win_start = v;
        satWinUpdateChipLabels();
        satWinRestartScan();
    }
    lv_event_stop_processing(e);
}

static void sat_win_table_key_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    if (sat_upd_overlay) { lv_event_stop_processing(e); return; }  // modal update in progress
    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_UP || key == LV_KEY_PREV) {
        if (sat_win_selected_row > 0) {
            sat_win_selected_row--;
            satScrollTableToRow(sat_win_table, sat_win_selected_row, sat_win_count);
            lv_event_stop_processing(e);
        }
        // At the top row: fall through so linear_nav moves focus to the chips
        return;
    }
    if (key == LV_KEY_DOWN || key == LV_KEY_NEXT) {
        if (sat_win_selected_row < sat_win_count - 1) {
            sat_win_selected_row++;
            satScrollTableToRow(sat_win_table, sat_win_selected_row, sat_win_count);
        }
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_ENTER) {
        if (sat_win_count > 0 && sat_win_selected_row < sat_win_count) {
            sat_selected_catalog_idx = sat_win_results[sat_win_selected_row].catIdx;
            satSelectedPass = sat_win_results[sat_win_selected_row].pass;
            satSelectedPassValid = true;
            if (sat_win_timer) { lv_timer_del(sat_win_timer); sat_win_timer = NULL; }
            sat_win_inflight = false;
            // Detail -> ESC -> that bird's passes -> ESC -> back to this window
            satPassesReturnMode = sat_win_focus_table ? MODE_SAT_WINDOW_NOW : MODE_SAT_WINDOW;
            satFlushEnterRelease();  // else the release clicks LIVE VIEW on the detail screen
            onLVGLMenuSelect(MODE_SAT_PASS_DETAIL);
        }
        lv_event_stop_processing(e);
        return;
    }
}

static lv_obj_t* createSatWindowScreenVariant(bool focusTable) {
    clearNavigationGroup();
    lv_obj_t* screen = createScreen();
    applyScreenStyle(screen);

    // May be the first satellite screen visited this boot - load the cache
    initSatCatalog();
    loadSatSettings();
    loadSatFavorites();
    if (!satCatalog.valid) {
        satLoadTLEsFromStorage();
    }
    satLoadXmtrsFromStorage();
    satAutoFetchIfEmpty();

    sat_win_focus_table = focusTable;
    sat_win_start = satWinRoundUp(time(nullptr));

    satCreateHeader(screen, focusTable ? "NEXT 60 MINUTES" : "SKY WINDOW");

    // Date + time chips side by side
    struct { const char* caption; lv_obj_t** value; } chips[2] = {
        { "DATE", &sat_win_date_val },
        { "START", &sat_win_time_val },
    };
    for (int i = 0; i < 2; i++) {
        lv_obj_t* chip = lv_obj_create(screen);
        lv_obj_set_size(chip, 222, 34);
        lv_obj_set_pos(chip, 10 + i * 228, HEADER_HEIGHT + 5);
        applyCardStyle(chip);
        lv_obj_set_style_pad_all(chip, 6, 0);
        lv_obj_set_style_border_color(chip, LV_COLOR_ACCENT_PRIMARY, LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(chip, 2, LV_STATE_FOCUSED);
        lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* cap = lv_label_create(chip);
        lv_label_set_text(cap, chips[i].caption);
        lv_obj_set_style_text_font(cap, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(cap, LV_COLOR_TEXT_SECONDARY, 0);
        lv_obj_align(cap, LV_ALIGN_LEFT_MID, 2, 0);

        lv_obj_t* val = lv_label_create(chip);
        lv_label_set_text(val, "--");
        lv_obj_set_style_text_font(val, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(chip, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_set_style_text_color(chip, getThemeColors()->text_on_accent, LV_STATE_FOCUSED);
        lv_obj_align(val, LV_ALIGN_RIGHT_MID, -2, 0);
        *(chips[i].value) = val;

        lv_obj_add_event_cb(chip, sat_win_chip_key_handler, LV_EVENT_KEY, (void*)(intptr_t)i);
        lv_obj_add_event_cb(chip, linear_nav_handler, LV_EVENT_KEY, NULL);
        addNavigableWidget(chip);
    }

    // Status line
    sat_win_status_label = lv_label_create(screen);
    lv_label_set_text(sat_win_status_label, "Scanning...");
    lv_obj_set_style_text_font(sat_win_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sat_win_status_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_pos(sat_win_status_label, 15, HEADER_HEIGHT + 44);

    // Column headers
    int header_row_y = HEADER_HEIGHT + 62;
    lv_obj_t* header_bar = lv_obj_create(screen);
    lv_obj_set_size(header_bar, SCREEN_WIDTH - 20, 24);
    lv_obj_set_pos(header_bar, 10, header_row_y);
    lv_obj_set_style_bg_color(header_bar, getThemeColors()->bg_layer2, 0);
    lv_obj_set_style_bg_opa(header_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header_bar, 0, 0);
    lv_obj_set_style_pad_all(header_bar, 0, 0);
    lv_obj_clear_flag(header_bar, LV_OBJ_FLAG_SCROLLABLE);

    const char* colNames[5] = { "SAT", "AOS", "LEN", "EL", "PATH" };
    int colX[5] = { 8, 190, 268, 322, 378 };
    for (int i = 0; i < 5; i++) {
        lv_obj_t* h = lv_label_create(header_bar);
        lv_label_set_text(h, colNames[i]);
        lv_obj_set_style_text_font(h, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(h, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_set_pos(h, colX[i], 3);
    }

    // Table
    int table_y = header_row_y + 26;
    int table_height = SCREEN_HEIGHT - table_y - FOOTER_HEIGHT - 5;

    sat_win_table = lv_table_create(screen);
    lv_obj_set_size(sat_win_table, SCREEN_WIDTH - 20, table_height);
    lv_obj_set_pos(sat_win_table, 10, table_y);
    satStyleTable(sat_win_table);
    lv_table_set_col_cnt(sat_win_table, 5);
    lv_table_set_col_width(sat_win_table, 0, 182);
    lv_table_set_col_width(sat_win_table, 1, 78);
    lv_table_set_col_width(sat_win_table, 2, 54);
    lv_table_set_col_width(sat_win_table, 3, 56);
    lv_table_set_col_width(sat_win_table, 4, 88);

    lv_table_set_row_cnt(sat_win_table, 1);
    for (int c = 0; c < 5; c++) lv_table_set_cell_value(sat_win_table, 0, c, "");

    lv_obj_add_event_cb(sat_win_table, sat_table_highlight_cb, LV_EVENT_DRAW_PART_BEGIN, &sat_win_selected_row);
    lv_obj_add_flag(sat_win_table, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sat_win_table, sat_win_table_key_handler, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(sat_win_table, linear_nav_handler, LV_EVENT_KEY, NULL);
    addNavigableWidget(sat_win_table);

    satCreateFooter(screen, "UP/DN Navigate   L/R Adjust date+time   ENTER Detail   ESC Back");

    satWinUpdateChipLabels();
    satWinRestartScan();

    // Only scan when predictions are actually possible
    double lat, lon;
    if (!gridToLatLon(satEffectiveGrid(), &lat, &lon) || !ntpSynced || !satCatalog.valid) {
        if (sat_win_status_label) {
            lv_label_set_text(sat_win_status_label,
                !satCatalog.valid ? "No TLE data - use Update TLEs in the Satellites menu"
                                  : "Set your grid square in Settings and sync the clock first");
            lv_obj_set_style_text_color(sat_win_status_label, LV_COLOR_ERROR, 0);
        }
        sat_win_scan = satCatalog.count;  // nothing to do
    } else if (!sat_win_timer) {
        sat_win_timer = lv_timer_create(sat_win_timer_cb, 60, NULL);
    }

    if (focusTable && sat_win_table) {
        lv_group_focus_obj(sat_win_table);
    }
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
    if (sat_upd_overlay) { lv_event_stop_processing(e); return; }  // modal update in progress
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
        satStartUpdateUI();
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
        lv_obj_set_style_text_font(val, getThemeFonts()->font_input, 0);
        // Color comes from the row (inherited) so it can flip when the row is
        // focused - the focused card background is accent-family and accent
        // text disappears against it while typing the grid.
        lv_obj_set_style_text_color(row, LV_COLOR_ACCENT_PRIMARY, 0);
        lv_obj_set_style_text_color(row, getThemeColors()->text_on_accent, LV_STATE_FOCUSED);
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
        case MODE_SAT_MENU:        return createSatMenuScreen();
        case MODE_SAT_LIST:        return createSatListScreenVariant(SAT_LIST_ALL);
        case MODE_SAT_MY:          return createSatListScreenVariant(SAT_LIST_MY);
        case MODE_SAT_POPULAR:     return createSatListScreenVariant(SAT_LIST_POPULAR);
        case MODE_SAT_BYPASS:      return createSatListScreenVariant(SAT_LIST_BYPASS);
        case MODE_SAT_PASSES:      return createSatPassesScreen();
        case MODE_SAT_PASS_DETAIL: return createSatPassDetailScreen();
        case MODE_SAT_FREQS:       return createSatFreqsScreen();
        case MODE_SAT_LIVE:        return createSatLiveScreen();
        case MODE_SAT_SETTINGS:    return createSatSettingsScreen();
        case MODE_SAT_WINDOW:      return createSatWindowScreenVariant(false);
        case MODE_SAT_WINDOW_NOW:  return createSatWindowScreenVariant(true);
        default:                   return NULL;
    }
}

#endif // LV_SATELLITE_SCREENS_H
