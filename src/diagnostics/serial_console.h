/*
 * USB Serial Command Console (self check)
 *
 * Reads "@vail <command>" lines from USB serial, sent by the Get Help page on
 * the updater site. Replies are "@vail {json}" lines (see diag_checks.h).
 * Anything that doesn't start with "@vail" is ignored, so stray typing in a
 * serial monitor does nothing.
 *
 *   @vail hello              protocol/firmware info
 *   @vail diag               run the automatic checks (opens the Self Check screen)
 *   @vail screen on|off      color bars
 *   @vail tone [loud]        test tone
 *   @vail keys               keyboard test
 *   @vail paddle             paddle test
 *   @vail answer <test> yes|no   record a yes/no answer given on the computer
 *   @vail stop               abort the running test
 *   @vail events             crash-surviving event history
 *   @vail report             re-send the last results + event history
 *
 * Polled from loop() (never from an LVGL callback), so it can safely change
 * screens. Non-blocking: reads at most 64 bytes per call.
 */

#ifndef SERIAL_CONSOLE_H
#define SERIAL_CONSOLE_H

#include "diag_checks.h"
#include "../lvgl/lv_self_check_screen.h"

#define CONSOLE_LINE_MAX 64
static char consoleLine[CONSOLE_LINE_MAX];
static uint8_t consoleLen = 0;
static bool consoleOverflow = false;

// Open the Self Check screen from wherever the user is, running the current
// mode's cleanup first (menu selection alone doesn't, and leaving a mode
// without it leaks timers/audio/network).
//
// Only done from a menu with no pop-up open. Feature screens can do their exit
// work in their own ESC handlers (Practice saves settings and stops its
// decoder there) and pop-ups live on the top layer, which a screen change
// leaves behind, so anywhere else the user is asked to go back to a menu.
static void consoleError(const char* msg);

static bool consoleOpenSelfCheck() {
  if (!selfCheckScreenActive()) {
    if (!isModeMenu((int)currentMode) || lv_obj_get_child_cnt(lv_layer_top()) > 0) {
      consoleError("not_on_menu");
      return false;
    }
    dispatchModeCallback(cleanupTable, cleanupTableSize, (int)currentMode);
    onLVGLMenuSelect(MODE_SELF_CHECK);
  }
  selfCheckRemoteBegin();
  return true;
}

static void consoleAck(const char* cmd) {
  diagEmit("{\"t\":\"ack\",\"cmd\":\"%s\"}", cmd);
}

static void consoleError(const char* msg) {
  diagEmit("{\"t\":\"err\",\"msg\":\"%s\"}", msg);
}

static void consoleHandle(char* line) {
  // Expect "@vail <cmd> [arg] [arg2]"
  char* save = nullptr;
  char* tok = strtok_r(line, " \t", &save);
  if (!tok || strcmp(tok, "@vail") != 0) return;
  char* cmd = strtok_r(nullptr, " \t", &save);
  char* arg = strtok_r(nullptr, " \t", &save);
  char* arg2 = strtok_r(nullptr, " \t", &save);
  if (!cmd) return;

  diagSessionTouch();

  if (strcmp(cmd, "hello") == 0) { diagEmitHello(); return; }
  if (strcmp(cmd, "events") == 0) { diagEmitEvents(); return; }
  if (strcmp(cmd, "report") == 0) { diagEmitLastResults(); diagEmitEvents(); return; }

  bool needsScreen = strcmp(cmd, "diag") == 0 || strcmp(cmd, "screen") == 0 ||
                     strcmp(cmd, "tone") == 0 || strcmp(cmd, "keys") == 0 ||
                     strcmp(cmd, "paddle") == 0 || strcmp(cmd, "answer") == 0 ||
                     strcmp(cmd, "stop") == 0;
  if (!needsScreen) { consoleError("unknown command"); return; }

  if (!consoleOpenSelfCheck()) return;

  if (strcmp(cmd, "stop") == 0) {
    if (selfCheckRemoteStop()) consoleAck(cmd);
    else consoleError("busy");
    return;
  }
  if (strcmp(cmd, "answer") == 0) {
    bool yes = arg2 && strcasecmp(arg2, "yes") == 0;
    bool no = arg2 && strcasecmp(arg2, "no") == 0;
    if (!arg || !(yes || no)) { consoleError("usage: answer <test> yes|no"); return; }
    int r = selfCheckRemoteAnswer(arg, yes);
    if (r < 0) { consoleError("unknown test"); return; }
    if (r == 0) { consoleError("not waiting for that answer"); return; }
    consoleAck(cmd);
    return;
  }
  if (strcmp(cmd, "screen") == 0 && arg && strcmp(arg, "off") == 0) {
    selfCheckRemoteScreen(false);
    consoleAck(cmd);
    return;
  }

  if (!selfCheckRemoteIdle()) { consoleError("busy"); return; }

  if (strcmp(cmd, "diag") == 0) selfCheckRemoteDiag();
  else if (strcmp(cmd, "screen") == 0) selfCheckRemoteScreen(true);
  else if (strcmp(cmd, "tone") == 0) selfCheckRemoteTone(arg && strcmp(arg, "loud") == 0);
  else if (strcmp(cmd, "keys") == 0) selfCheckRemoteKeys();
  else if (strcmp(cmd, "paddle") == 0) selfCheckRemotePaddle();
  consoleAck(cmd);
}

void serialConsolePoll() {
  for (int budget = 64; budget > 0 && Serial.available() > 0; budget--) {
    int c = Serial.read();
    if (c < 0) break;
    if (c == '\r') continue;
    if (c == '\n') {
      if (!consoleOverflow && consoleLen > 0) {
        consoleLine[consoleLen] = '\0';
        consoleHandle(consoleLine);
      }
      consoleLen = 0;
      consoleOverflow = false;
      continue;
    }
    if (consoleLen < CONSOLE_LINE_MAX - 1) consoleLine[consoleLen++] = (char)c;
    else consoleOverflow = true;
  }
}

#endif // SERIAL_CONSOLE_H
