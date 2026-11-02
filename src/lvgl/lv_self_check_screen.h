/*
 * Self Check Screen (Settings > Device Settings > Self Check, MODE_SELF_CHECK)
 *
 * Two ways in:
 *  - On the device: intro -> automatic checks -> guided checks (screen colors,
 *    sound, keyboard, paddle) -> results list.
 *  - From a computer: the Get Help page on the updater site sends "@vail ..."
 *    commands over USB (src/diagnostics/serial_console.h). The screen then
 *    shows "Connected to a computer" and runs each test on request, with the
 *    yes/no questions asked on the computer instead.
 *
 * One 50 ms LVGL timer drives everything; cleanupSelfCheckScreen() (cleanup
 * table) deletes it along with the color-bar overlay.
 */

#ifndef LV_SELF_CHECK_SCREEN_H
#define LV_SELF_CHECK_SCREEN_H

#include <lvgl.h>
#include "lv_theme_summit.h"
#include "lv_widgets_summit.h"
#include "lv_screen_manager.h"
#include "../diagnostics/diag_checks.h"

#define SC_KEYS_TIMEOUT_MS    20000
#define SC_PADDLE_TIMEOUT_MS  15000
#define SC_TONE_HZ            600
#define SC_TONE_MS            1000
#define SC_LOUD_VOLUME        80

enum SCState {
  SC_INTRO,
  SC_AUTO,
  SC_SCREEN_TEST,
  SC_SOUND_PLAY,       // tone starts on a later tick (so the prompt draws first)
  SC_SOUND_ASK,
  SC_SOUND_LOUD_PLAY,
  SC_SOUND_LOUD_ASK,
  SC_SOUND_WAIT,       // tone playing on the audio task; no display updates
  SC_KEYS,
  SC_PADDLE_DIT,
  SC_PADDLE_DAH,
  SC_RESULTS,
  SC_REMOTE            // idle, waiting for the computer's next command
};

static lv_obj_t* sc_screen = NULL;
static lv_obj_t* sc_prompt = NULL;
static lv_obj_t* sc_list = NULL;
static lv_obj_t* sc_footer = NULL;
static lv_obj_t* sc_bars = NULL;          // full-screen color bars on lv_layer_top()
static lv_obj_t* sc_bars_hint = NULL;
static lv_obj_t* sc_key_boxes[4] = { NULL };
static lv_obj_t* sc_key_row = NULL;
static lv_timer_t* sc_timer = NULL;

static SCState sc_state = SC_INTRO;
static bool sc_remote = false;
static unsigned long sc_state_ms = 0;
static int sc_rows_built = 0;
static bool sc_remote_loud = false;       // remote tone: loud variant requested
static uint16_t sc_rows_replaced = 0;     // diagResultsReplaced when rows were built
// Tone in progress
static SCState sc_tone_next = SC_SOUND_ASK;
static bool sc_tone_loud = false;
// Guided tests waiting for a yes/no, so a late or repeated answer from the
// computer can't overwrite a result: screen, sound, sound_loud
static bool sc_answer_pending[3] = { false, false, false };
static int sc_tone_saved_volume = -1;     // >= 0 while the loud tone has volume raised

// Keyboard test
static const char* const sc_key_labels[4] = { "A", "1", LV_SYMBOL_UP " " LV_SYMBOL_DOWN, "ENTER" };
static bool sc_keys_hit[4];
static char sc_keys_typed[24];

// Paddle test
static bool sc_dit_seen, sc_dah_seen, sc_swap_seen;
static bool sc_dit_stuck, sc_dah_stuck;
static bool sc_last_dit, sc_last_dah;

static void sc_set_state(SCState s);

static void sc_set_prompt(const char* text) {
  if (sc_prompt) lvLabelSetIfChanged(sc_prompt, text);
}

static void sc_set_footer(const char* text) {
  if (sc_footer) lvLabelSetIfChanged(sc_footer, text);
}

static lv_color_t sc_status_color(DiagStatus st) {
  switch (st) {
    case DIAG_PASS: return LV_COLOR_SUCCESS;
    case DIAG_WARN: return LV_COLOR_WARNING;
    case DIAG_FAIL: return LV_COLOR_ERROR;
    default:        return LV_COLOR_TEXT_SECONDARY;
  }
}

static const char* sc_status_symbol(DiagStatus st) {
  switch (st) {
    case DIAG_PASS: return LV_SYMBOL_OK;
    case DIAG_WARN: return LV_SYMBOL_WARNING;
    case DIAG_FAIL: return LV_SYMBOL_CLOSE;
    default:        return LV_SYMBOL_BULLET;
  }
}

// Append rows for any results added since the last call
static void sc_sync_rows() {
  if (!sc_list) return;
  if (sc_rows_replaced != diagResultsReplaced) {
    // A result was overwritten in place (a repeated test): rebuild the list
    sc_rows_replaced = diagResultsReplaced;
    lv_obj_clean(sc_list);
    sc_rows_built = 0;
  }
  int n = diagResultCount();
  for (; sc_rows_built < n; sc_rows_built++) {
    const DiagResult* r = diagResultAt(sc_rows_built);
    if (!r) break;

    lv_obj_t* row = lv_obj_create(sc_list);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_style(row, getStyleTransparentContainer(), 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_ver(row, 2, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* line = lv_label_create(row);
    lv_label_set_long_mode(line, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(line, lv_pct(100));
    lv_label_set_text_fmt(line, "%s %s: %s", sc_status_symbol(r->st), r->name, r->detail);
    lv_obj_set_style_text_color(line, sc_status_color(r->st), 0);
    lv_obj_set_style_text_font(line, getThemeFonts()->font_small, 0);

    if (r->advice && (r->st == DIAG_WARN || r->st == DIAG_FAIL)) {
      lv_obj_t* adv = lv_label_create(row);
      lv_label_set_long_mode(adv, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(adv, lv_pct(100));
      lv_label_set_text(adv, r->advice);
      lv_obj_set_style_text_color(adv, LV_COLOR_TEXT_SECONDARY, 0);
      lv_obj_set_style_text_font(adv, getThemeFonts()->font_small, 0);
      lv_obj_set_style_pad_left(adv, 18, 0);
    }
    lv_obj_scroll_to_view(row, LV_ANIM_OFF);
  }
}

static void sc_clear_rows() {
  if (sc_list) lv_obj_clean(sc_list);
  sc_rows_built = 0;
}

// ---- color bars ---------------------------------------------------------------

static void sc_show_bars(bool on) {
  if (!on) {
    if (sc_bars) lv_obj_del(sc_bars);
    sc_bars = NULL;
    sc_bars_hint = NULL;
    return;
  }
  if (sc_bars) return;

  static const uint32_t colors[8] = { 0xFFFFFF, 0xFFFF00, 0x00FFFF, 0x00FF00,
                                      0xFF00FF, 0xFF0000, 0x0000FF, 0x000000 };
  sc_bars = lv_obj_create(lv_layer_top());
  lv_obj_set_size(sc_bars, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(sc_bars, 0, 0);
  lv_obj_set_style_radius(sc_bars, 0, 0);
  lv_obj_set_style_pad_all(sc_bars, 0, 0);
  lv_obj_set_style_border_width(sc_bars, 2, 0);
  lv_obj_set_style_border_color(sc_bars, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_color(sc_bars, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(sc_bars, LV_OPA_COVER, 0);
  lv_obj_clear_flag(sc_bars, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  const int barW = SCREEN_WIDTH / 8;
  const int barH = SCREEN_HEIGHT * 2 / 3;
  for (int i = 0; i < 8; i++) {
    lv_obj_t* bar = lv_obj_create(sc_bars);
    lv_obj_set_size(bar, barW, barH);
    lv_obj_set_pos(bar, i * barW, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(colors[i]), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  }

  // Black-to-white ramp shows banding, dead color channels and contrast
  lv_obj_t* ramp = lv_obj_create(sc_bars);
  lv_obj_set_size(ramp, SCREEN_WIDTH, 40);
  lv_obj_set_pos(ramp, 0, barH);
  lv_obj_set_style_radius(ramp, 0, 0);
  lv_obj_set_style_border_width(ramp, 0, 0);
  lv_obj_set_style_bg_color(ramp, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_grad_color(ramp, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_grad_dir(ramp, LV_GRAD_DIR_HOR, 0);
  lv_obj_set_style_bg_opa(ramp, LV_OPA_COVER, 0);
  lv_obj_clear_flag(ramp, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  sc_bars_hint = lv_label_create(sc_bars);
  lv_label_set_text(sc_bars_hint, sc_remote ? "Answer on the computer"
                                            : "Do the colors look right?   Y = yes   N = no");
  lv_obj_set_style_text_color(sc_bars_hint, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(sc_bars_hint, getThemeFonts()->font_body, 0);
  lv_obj_align(sc_bars_hint, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// ---- keyboard test boxes --------------------------------------------------------

static void sc_show_key_boxes(bool on) {
  if (sc_list) {
    if (on) lv_obj_add_flag(sc_list, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(sc_list, LV_OBJ_FLAG_HIDDEN);
  }
  if (!on) {
    if (sc_key_row) lv_obj_del(sc_key_row);
    sc_key_row = NULL;
    for (int i = 0; i < 4; i++) sc_key_boxes[i] = NULL;
    return;
  }
  if (sc_key_row || !sc_screen) return;
  sc_key_row = lv_obj_create(sc_screen);
  lv_obj_set_size(sc_key_row, SCREEN_WIDTH - 40, 60);
  lv_obj_align(sc_key_row, LV_ALIGN_CENTER, 0, 30);
  lv_obj_add_style(sc_key_row, getStyleTransparentContainer(), 0);
  lv_obj_set_flex_flow(sc_key_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(sc_key_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(sc_key_row, LV_OBJ_FLAG_SCROLLABLE);
  for (int i = 0; i < 4; i++) {
    lv_obj_t* box = lv_obj_create(sc_key_row);
    lv_obj_set_size(box, 90, 46);
    applyCardStyle(box);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_border_color(box, LV_COLOR_TEXT_TERTIARY, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* lbl = lv_label_create(box);
    lv_label_set_text(lbl, sc_key_labels[i]);
    lv_obj_set_style_text_font(lbl, getThemeFonts()->font_body, 0);
    lv_obj_center(lbl);
    sc_key_boxes[i] = box;
  }
}

static void sc_mark_key(int idx) {
  if (idx < 0 || idx > 3 || sc_keys_hit[idx]) return;
  sc_keys_hit[idx] = true;
  if (sc_key_boxes[idx]) {
    lv_obj_set_style_border_color(sc_key_boxes[idx], LV_COLOR_SUCCESS, 0);
    lv_obj_set_style_bg_color(sc_key_boxes[idx], LV_COLOR_SUCCESS, 0);
  }
  diagEmit("{\"t\":\"key_target\",\"index\":%d}", idx);
}

// ---- guided results -------------------------------------------------------------

static void sc_finish_keys(bool skipped) {
  sc_show_key_boxes(false);
  int got = 0;
  for (int i = 0; i < 4; i++) got += sc_keys_hit[i];
  char missing[40] = "";
  for (int i = 0; i < 4; i++) {
    if (sc_keys_hit[i]) continue;
    if (missing[0]) strlcat(missing, ", ", sizeof(missing));
    strlcat(missing, i == 2 ? "arrow" : sc_key_labels[i], sizeof(missing));
  }
  if (skipped && got == 0) {
    diagAdd("keys_test", "Keyboard test", DIAG_INFO, nullptr, "skipped");
  } else if (sc_keys_typed[0] == '\0' && !sc_remote) {
    // On the device the user just used this keyboard to get here, so a
    // timeout with no presses means they stepped away, not a dead keyboard
    diagAdd("keys_test", "Keyboard test", DIAG_INFO, nullptr, "timed out with no keys pressed");
  } else if (got == 4) {
    diagAdd("keys_test", "Keyboard test", DIAG_PASS, nullptr, "all 4 keys worked");
  } else if (sc_keys_typed[0] == '\0') {
    diagAdd("keys_test", "Keyboard test", DIAG_FAIL,
            "No key presses arrived. Check the keyboard cable is pushed in firmly at both ends.",
            "no keys received");
  } else {
    diagAdd("keys_test", "Keyboard test", DIAG_WARN,
            "Some keys didn't register. Try them again; if a key never works, the keyboard may be faulty.",
            "missing: %s (typed: %s)", missing, sc_keys_typed);
  }
}

static void sc_finish_paddle(bool skipped) {
  if (skipped && !sc_dit_seen && !sc_dah_seen) {
    diagAdd("paddle_test", "Paddle test", DIAG_INFO, nullptr, "skipped");
  } else if (sc_dit_stuck || sc_dah_stuck) {
    diagAdd("paddle_test", "Paddle test", DIAG_FAIL,
            "A paddle side is stuck closed. Unplug the paddle: if it clears, the paddle or cable is shorted; if not, check the jack.",
            "%s%s%s stuck closed", sc_dit_stuck ? "DIT" : "", sc_dit_stuck && sc_dah_stuck ? " and " : "",
            sc_dah_stuck ? "DAH" : "");
  } else if (sc_swap_seen && sc_dit_seen && sc_dah_seen) {
    diagAdd("paddle_test", "Paddle test", DIAG_WARN,
            "DIT and DAH look reversed. Flip the paddle (or its wiring): tip is DIT, ring is DAH.",
            "DIT and DAH reversed");
  } else if (sc_dit_seen && sc_dah_seen) {
    diagAdd("paddle_test", "Paddle test", DIAG_PASS, nullptr, "DIT and DAH both work");
  } else if (sc_dit_seen) {
    diagAdd("paddle_test", "Paddle test", DIAG_WARN,
            "DIT works but DAH never closed. That's normal for a straight key. With a paddle, check the cable is plugged all the way in.",
            "DIT works, DAH never closed");
  } else {
    diagAdd("paddle_test", "Paddle test", DIAG_FAIL,
            "Check the paddle cable is plugged all the way into the jack. A straight key only has a DIT side.",
            "%s never closed", !sc_dit_seen && !sc_dah_seen ? "DIT and DAH" : (!sc_dit_seen ? "DIT" : "DAH"));
  }
}

static int sc_answer_index(const char* test) {
  if (strcmp(test, "screen") == 0) return 0;
  if (strcmp(test, "sound") == 0) return 1;
  if (strcmp(test, "sound_loud") == 0) return 2;
  return -1;
}

static bool sc_record_answer(const char* test, bool yes) {
  int idx = sc_answer_index(test);
  if (idx >= 0) sc_answer_pending[idx] = false;
  if (strcmp(test, "screen") == 0) {
    diagAdd("screen_test", "Screen test", yes ? DIAG_PASS : DIAG_FAIL,
            yes ? nullptr : "Colors looked wrong. Note which bars were off; a missing color often means a loose display connection.",
            yes ? "colors look right" : "colors looked wrong");
  } else if (strcmp(test, "sound") == 0) {
    diagAdd("sound_test", "Sound test", yes ? DIAG_PASS : DIAG_WARN,
            yes ? nullptr : "No tone at normal volume. Turn the volume up (press V on a menu screen).",
            yes ? "tone heard" : "not heard at normal volume");
  } else if (strcmp(test, "sound_loud") == 0) {
    diagAdd("sound_loud", "Sound test (loud)", yes ? DIAG_WARN : DIAG_FAIL,
            yes ? "The tone was only heard at high volume. Turn the volume up."
                : "No tone even at high volume. Check headphones are unplugged, then the speaker and amplifier connections.",
            yes ? "heard only at %d%% volume" : "not heard at %d%% volume", SC_LOUD_VOLUME);
  } else {
    return false;
  }
  return true;
}

static void sc_after_guided(SCState nextLocal);

// Start the test tone on the audio task (Core 0) so loop() keeps running;
// SC_SOUND_WAIT finishes it. Falls back to the blocking beep if the audio task
// isn't up.
static void sc_start_tone(bool loud, SCState next) {
  sc_tone_loud = loud;
  sc_tone_next = next;
  sc_answer_pending[loud ? 2 : 1] = true;
  if (loud && audio_volume < SC_LOUD_VOLUME) {
    sc_tone_saved_volume = audio_volume;
    audio_volume = SC_LOUD_VOLUME;   // temporary, never saved
  }
  if (isAudioTaskRunning()) requestPlayTone(SC_TONE_HZ, SC_TONE_MS);
  else beep(SC_TONE_HZ, SC_TONE_MS);
  sc_set_state(SC_SOUND_WAIT);
}

static void sc_finish_tone() {
  int played = audio_volume;
  if (sc_tone_saved_volume >= 0) {
    audio_volume = sc_tone_saved_volume;
    sc_tone_saved_volume = -1;
  }
  diagEmit("{\"t\":\"tone_done\",\"loud\":%s,\"volume\":%d}", sc_tone_loud ? "true" : "false", played);
  sc_after_guided(sc_tone_next);
}

// ---- state machine --------------------------------------------------------------

static const char* sc_remote_footer = "Follow the steps on the computer   UP/DN Scroll   ESC Exit";

static void sc_show_results_summary() {
  int counts[4] = { 0, 0, 0, 0 };
  for (int i = 0; i < diagResultCount(); i++) counts[diagResultAt(i)->st]++;
  char buf[160];
  if (counts[DIAG_FAIL] + counts[DIAG_WARN] == 0) {
    snprintf(buf, sizeof(buf), "All good! %d checks passed.", counts[DIAG_PASS]);
  } else {
    snprintf(buf, sizeof(buf), "%d problem(s), %d warning(s). Need help? Plug into a computer and open update.vailadapter.com > Get Help.",
             counts[DIAG_FAIL], counts[DIAG_WARN]);
  }
  sc_set_prompt(buf);
}

static void sc_set_state(SCState s) {
  sc_state = s;
  sc_state_ms = millis();
  switch (s) {
    case SC_INTRO:
      sc_set_prompt("Checks the SD card, keyboard, sound, screen and paddle. Takes about a minute.");
      // Rows from an earlier run show under the intro and can be scrolled
      sc_set_footer(diagResultCount() > 0 ? "UP/DN Scroll   ENTER Start   ESC Back" : "ENTER Start   ESC Back");
      break;
    case SC_AUTO:
      sc_set_prompt("Running checks...");
      sc_set_footer(sc_remote ? sc_remote_footer : "Please wait");
      break;
    case SC_SCREEN_TEST:
      sc_answer_pending[0] = true;
      sc_show_bars(true);
      diagEmit("{\"t\":\"screen\",\"on\":true}");
      break;
    case SC_SOUND_PLAY:
      sc_set_prompt("Listen for a tone...");
      sc_set_footer("");
      break;
    case SC_SOUND_ASK:
      sc_set_prompt("Did you hear a tone?");
      sc_set_footer("Y Yes   N No   R Replay   ESC Skip");
      break;
    case SC_SOUND_LOUD_PLAY:
      sc_set_prompt("Trying again, louder...");
      sc_set_footer("");
      break;
    case SC_SOUND_LOUD_ASK:
      sc_set_prompt("Did you hear it this time?");
      sc_set_footer("Y Yes   N No   ESC Skip");
      break;
    case SC_SOUND_WAIT:
      break;
    case SC_KEYS:
      memset(sc_keys_hit, 0, sizeof(sc_keys_hit));
      sc_keys_typed[0] = '\0';
      sc_set_prompt("Press each key shown below on the Summit keyboard.");
      sc_set_footer("ESC Skip");
      sc_show_key_boxes(true);
      diagEmit("{\"t\":\"keys_start\",\"timeout\":%d}", SC_KEYS_TIMEOUT_MS / 1000);
      break;
    case SC_PADDLE_DIT:
      sc_dit_seen = sc_dah_seen = sc_swap_seen = false;
      sc_last_dit = digitalRead(DIT_PIN) == PADDLE_ACTIVE;
      sc_last_dah = digitalRead(DAH_PIN) == PADDLE_ACTIVE;
      sc_dit_stuck = sc_last_dit;
      sc_dah_stuck = sc_last_dah;
      sc_set_prompt("Press the DIT side of your paddle (or your straight key).");
      sc_set_footer("ESC Skip");
      diagEmit("{\"t\":\"paddle_step\",\"want\":\"dit\"}");
      break;
    case SC_PADDLE_DAH:
      sc_set_prompt("Now press the DAH side.");
      sc_set_footer("ESC Skip");
      diagEmit("{\"t\":\"paddle_step\",\"want\":\"dah\"}");
      break;
    case SC_RESULTS:
      sc_show_results_summary();
      sc_set_footer("UP/DN Scroll   ENTER Run again   ESC Back");
      break;
    case SC_REMOTE:
      sc_set_prompt("Connected to a computer. Follow the steps on the computer screen.");
      sc_set_footer(sc_remote_footer);
      break;
  }
}

// Where a guided step goes when it finishes
static void sc_after_guided(SCState nextLocal) {
  sc_set_state(sc_remote ? SC_REMOTE : nextLocal);
}

static void sc_end_screen_test(const char* answer) {
  sc_show_bars(false);
  diagEmit("{\"t\":\"screen\",\"on\":false}");
  if (answer) sc_record_answer("screen", answer[0] == 'y');
  sc_after_guided(SC_SOUND_PLAY);
}

void cleanupSelfCheckScreen();

static void sc_timer_cb(lv_timer_t* t) {
  (void)t;
  // Left without the cleanup table running (a path that loads another screen
  // directly): the screen and its children are gone, so tidy up and stop.
  if (currentMode != MODE_SELF_CHECK) {
    sc_key_row = NULL;
    cleanupSelfCheckScreen();
    return;
  }
  unsigned long elapsed = millis() - sc_state_ms;

  switch (sc_state) {
    case SC_AUTO: {
      bool done = diagRunPoll();
      sc_sync_rows();
      if (done) {
        diagEmitEvents();
        if (sc_remote) sc_set_state(SC_REMOTE);
        else sc_set_state(SC_SCREEN_TEST);
      } else {
        char buf[48];
        snprintf(buf, sizeof(buf), "Running checks... %d of %d", diagRunStep() + 1, diagRunTotal());
        sc_set_prompt(buf);
      }
      break;
    }
    case SC_SOUND_PLAY:
      // Give the prompt one tick to draw: no display updates during the tone
      if (elapsed > 150) sc_start_tone(sc_remote && sc_remote_loud, SC_SOUND_ASK);
      break;
    case SC_SOUND_LOUD_PLAY:
      if (elapsed > 150) sc_start_tone(true, SC_SOUND_LOUD_ASK);
      break;
    case SC_SOUND_WAIT:
      if (elapsed > SC_TONE_MS + 100) sc_finish_tone();
      break;
    case SC_KEYS:
      if (elapsed > SC_KEYS_TIMEOUT_MS) {
        sc_finish_keys(false);
        diagEmit("{\"t\":\"keys_end\"}");
        sc_after_guided(SC_PADDLE_DIT);
      }
      break;
    case SC_PADDLE_DIT:
    case SC_PADDLE_DAH: {
      bool dit = digitalRead(DIT_PIN) == PADDLE_ACTIVE;
      bool dah = digitalRead(DAH_PIN) == PADDLE_ACTIVE;
      if (!dit) sc_dit_stuck = false;   // released at least once: not stuck
      if (!dah) sc_dah_stuck = false;
      bool ditEdge = dit && !sc_last_dit;
      bool dahEdge = dah && !sc_last_dah;
      sc_last_dit = dit;
      sc_last_dah = dah;
      if (ditEdge) diagEmit("{\"t\":\"paddle\",\"side\":\"dit\"}");
      if (dahEdge) diagEmit("{\"t\":\"paddle\",\"side\":\"dah\"}");

      if (ditEdge) sc_dit_seen = true;
      if (dahEdge) sc_dah_seen = true;

      // The first side to close is taken as the "DIT" press. If that was the
      // DAH pin, the paddle is reversed, and the DAH step then waits for the
      // DIT pin instead.
      if (sc_state == SC_PADDLE_DIT) {
        if (ditEdge || dahEdge) {
          sc_swap_seen = dahEdge && !ditEdge;
          sc_set_state(SC_PADDLE_DAH);
        }
      } else if (sc_swap_seen ? ditEdge : dahEdge) {
        sc_finish_paddle(false);
        diagEmit("{\"t\":\"paddle_end\"}");
        sc_after_guided(SC_RESULTS);
        break;
      }
      if (elapsed > SC_PADDLE_TIMEOUT_MS) {
        sc_finish_paddle(false);
        diagEmit("{\"t\":\"paddle_end\"}");
        sc_after_guided(SC_RESULTS);
      }
      break;
    }
    default:
      break;
  }
  if (sc_state == SC_RESULTS || sc_state == SC_REMOTE) sc_sync_rows();
}

static void sc_start_auto() {
  sc_clear_rows();
  if (!diagRunStart()) {
    sc_set_prompt("Couldn't start the check (out of memory). Restart and try again.");
    return;
  }
  sc_set_state(SC_AUTO);
}

static void self_check_key_cb(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_KEY) return;
  uint32_t key = lv_event_get_key(e);
  bool yes = (key == 'y' || key == 'Y');
  bool no = (key == 'n' || key == 'N');

  // Keyboard test eats every key, ESC included only as "skip"
  if (sc_state == SC_KEYS) {
    if (key == LV_KEY_ESC) {
      sc_finish_keys(true);
      diagEmit("{\"t\":\"keys_end\"}");
      sc_after_guided(SC_PADDLE_DIT);
    } else {
      char name[8];
      if (key == LV_KEY_UP || key == LV_KEY_DOWN || key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        strlcpy(name, "^", sizeof(name)); sc_mark_key(2);
      } else if (key == LV_KEY_ENTER) {
        strlcpy(name, "<CR>", sizeof(name)); sc_mark_key(3);
      } else if (key >= 32 && key < 127) {
        snprintf(name, sizeof(name), "%c", (char)key);
        if (key == 'a' || key == 'A') sc_mark_key(0);
        if (key == '1') sc_mark_key(1);
      } else {
        snprintf(name, sizeof(name), "#%u", (unsigned)key);
      }
      if (strlen(sc_keys_typed) + strlen(name) + 1 < sizeof(sc_keys_typed)) {
        if (sc_keys_typed[0]) strlcat(sc_keys_typed, " ", sizeof(sc_keys_typed));
        strlcat(sc_keys_typed, name, sizeof(sc_keys_typed));
      }
      char esc[16];
      diagJsonEscape(esc, sizeof(esc), name);
      diagEmit("{\"t\":\"key\",\"key\":\"%s\"}", esc);
      bool all = sc_keys_hit[0] && sc_keys_hit[1] && sc_keys_hit[2] && sc_keys_hit[3];
      if (all) {
        sc_finish_keys(false);
        diagEmit("{\"t\":\"keys_end\"}");
        sc_after_guided(SC_PADDLE_DIT);
      }
    }
    lv_event_stop_processing(e);
    return;
  }

  // Between tests the results list is on screen: the arrows scroll it
  bool idle = sc_state == SC_INTRO || sc_state == SC_RESULTS || sc_state == SC_REMOTE;
  if (idle && sc_list && (key == LV_KEY_UP || key == LV_KEY_DOWN)) {
    lv_obj_scroll_by(sc_list, 0, key == LV_KEY_UP ? 40 : -40, LV_ANIM_ON);
    lv_event_stop_processing(e);
    return;
  }

  switch (sc_state) {
    case SC_INTRO:
    case SC_RESULTS:
      if (key == LV_KEY_ENTER) { sc_start_auto(); lv_event_stop_processing(e); return; }
      break;
    case SC_SCREEN_TEST:
      if (!sc_remote && (yes || no)) { sc_end_screen_test(yes ? "y" : "n"); lv_event_stop_processing(e); return; }
      if (key == LV_KEY_ESC) { sc_end_screen_test(nullptr); lv_event_stop_processing(e); return; }
      lv_event_stop_processing(e);
      return;
    case SC_SOUND_ASK:
      if (yes) { sc_record_answer("sound", true); sc_set_state(SC_KEYS); }
      else if (no) { sc_record_answer("sound", false); sc_set_state(SC_SOUND_LOUD_PLAY); }
      else if (key == 'r' || key == 'R') sc_set_state(SC_SOUND_PLAY);
      else if (key == LV_KEY_ESC) sc_set_state(SC_KEYS);
      lv_event_stop_processing(e);
      return;
    case SC_SOUND_LOUD_ASK:
      if (yes || no) { sc_record_answer("sound_loud", yes); sc_set_state(SC_KEYS); }
      else if (key == LV_KEY_ESC) sc_set_state(SC_KEYS);
      lv_event_stop_processing(e);
      return;
    case SC_PADDLE_DIT:
    case SC_PADDLE_DAH:
      if (key == LV_KEY_ESC) {
        sc_finish_paddle(true);
        diagEmit("{\"t\":\"paddle_end\"}");
        sc_after_guided(SC_RESULTS);
      }
      lv_event_stop_processing(e);
      return;
    case SC_AUTO:
    case SC_SOUND_PLAY:
    case SC_SOUND_LOUD_PLAY:
    case SC_SOUND_WAIT:
      // Busy: don't leave mid-test (the SD tests have the bus released)
      lv_event_stop_processing(e);
      return;
    default:
      break;
  }

  if (key == LV_KEY_ESC) {
    onLVGLBackNavigation();
    lv_event_stop_processing(e);
  }
}

lv_obj_t* createSelfCheckScreen() {
  lv_obj_t* screen = createScreen();
  applyScreenStyle(screen);
  sc_screen = screen;

  lv_obj_t* title_bar = lv_obj_create(screen);
  lv_obj_set_size(title_bar, SCREEN_WIDTH, HEADER_HEIGHT);
  lv_obj_set_pos(title_bar, 0, 0);
  lv_obj_add_style(title_bar, getStyleStatusBar(), 0);
  lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* title = lv_label_create(title_bar);
  lv_label_set_text(title, "SELF CHECK");
  lv_obj_add_style(title, getStyleLabelTitle(), 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

  sc_prompt = lv_label_create(screen);
  lv_label_set_long_mode(sc_prompt, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(sc_prompt, SCREEN_WIDTH - 40);
  lv_obj_set_pos(sc_prompt, 20, HEADER_HEIGHT + 8);
  lv_obj_set_style_text_font(sc_prompt, getThemeFonts()->font_body, 0);
  lv_obj_set_style_text_color(sc_prompt, LV_COLOR_TEXT_PRIMARY, 0);
  lv_label_set_text(sc_prompt, "");

  const int listTop = HEADER_HEIGHT + 70;
  sc_list = lv_obj_create(screen);
  lv_obj_set_size(sc_list, SCREEN_WIDTH - 30, SCREEN_HEIGHT - listTop - FOOTER_HEIGHT - 6);
  lv_obj_set_pos(sc_list, 15, listTop);
  applyCardStyle(sc_list);
  lv_obj_set_style_pad_all(sc_list, 8, 0);
  lv_obj_set_flex_flow(sc_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(sc_list, 2, 0);
  lv_obj_set_scrollbar_mode(sc_list, LV_SCROLLBAR_MODE_AUTO);

  lv_obj_t* footer = lv_obj_create(screen);
  lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
  lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
  lv_obj_add_style(footer, getStyleTransparent(), 0);
  lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
  sc_footer = lv_label_create(footer);
  lv_obj_set_style_text_color(sc_footer, LV_COLOR_WARNING, 0);
  lv_obj_set_style_text_font(sc_footer, getThemeFonts()->font_small, 0);
  lv_obj_center(sc_footer);
  lv_obj_set_style_text_align(sc_footer, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(sc_footer, "");

  createKeyCatcher(screen, self_check_key_cb, true);

  sc_rows_built = 0;
  sc_remote = false;
  sc_remote_loud = false;
  sc_set_state(SC_INTRO);
  // Show the previous run's results (if any) under the intro
  sc_sync_rows();

  if (sc_timer) lv_timer_del(sc_timer);   // left over from an exit that skipped cleanup
  sc_timer = lv_timer_create(sc_timer_cb, 50, NULL);
  return screen;
}

void cleanupSelfCheckScreen() {
  if (sc_timer) {
    lv_timer_del(sc_timer);
    sc_timer = NULL;
  }
  sc_show_bars(false);
  if (sc_tone_saved_volume >= 0) {
    audio_volume = sc_tone_saved_volume;
    sc_tone_saved_volume = -1;
  }
  sc_key_row = NULL;            // child of the screen, deleted with it
  for (int i = 0; i < 4; i++) sc_key_boxes[i] = NULL;
  if (sc_remote) diagEmit("{\"t\":\"closed\"}");
  sc_screen = sc_prompt = sc_list = sc_footer = NULL;
  sc_remote = false;
  sc_state = SC_INTRO;
  diagStepIndex = -1;           // abandon a run in progress
}

// ============================================
// Remote control (serial console)
// ============================================

bool selfCheckScreenActive() { return sc_screen != NULL && currentMode == MODE_SELF_CHECK; }

// True when the screen is between tests and can take a new command
bool selfCheckRemoteIdle() {
  return sc_state == SC_REMOTE || sc_state == SC_INTRO || sc_state == SC_RESULTS;
}

void selfCheckRemoteBegin() {
  sc_remote = true;
  if (sc_bars_hint) lv_label_set_text(sc_bars_hint, "Answer on the computer");
  if (selfCheckRemoteIdle()) sc_set_state(SC_REMOTE);
}

void selfCheckRemoteDiag() { sc_start_auto(); }

void selfCheckRemoteScreen(bool on) {
  if (on) sc_set_state(SC_SCREEN_TEST);
  else if (sc_state == SC_SCREEN_TEST) sc_end_screen_test(nullptr);
}

void selfCheckRemoteTone(bool loud) {
  sc_remote_loud = loud;
  sc_set_state(SC_SOUND_PLAY);
}

void selfCheckRemoteKeys() { sc_set_state(SC_KEYS); }
void selfCheckRemotePaddle() { sc_set_state(SC_PADDLE_DIT); }

// 1 recorded, 0 that test isn't waiting for an answer, -1 unknown test
int selfCheckRemoteAnswer(const char* test, bool yes) {
  int idx = sc_answer_index(test);
  if (idx < 0) return -1;
  if (!sc_answer_pending[idx]) return 0;
  sc_record_answer(test, yes);
  return 1;
}

// Abort whatever test is running and go back to waiting. The automatic
// checks can't be interrupted (the SD steps own the bus), and a tone already
// playing runs out its ~1 s: returns false for both.
bool selfCheckRemoteStop() {
  if (sc_state == SC_AUTO || sc_state == SC_SOUND_WAIT) return false;
  switch (sc_state) {
    case SC_SCREEN_TEST: sc_end_screen_test(nullptr); break;
    case SC_KEYS: sc_finish_keys(true); diagEmit("{\"t\":\"keys_end\"}"); break;
    case SC_PADDLE_DIT:
    case SC_PADDLE_DAH: sc_finish_paddle(true); diagEmit("{\"t\":\"paddle_end\"}"); break;
    default: break;
  }
  sc_set_state(SC_REMOTE);
  return true;
}

#endif // LV_SELF_CHECK_SCREEN_H
