# Heap diet notes (October 2026)

Branch: `brett/elegant-leavitt-e8a73f`. Built with the core 3.3.12 toolchain
(`C:\acli3`). All numbers below are compiler "Global variables use" (static
.data + .bss); heap effects are described per change and need the on-device
`[HEAP ...]` log lines to be confirmed.

| Build | Static RAM | Notes |
|-------|-----------:|-------|
| main (0d3c5c1), core 2.0.14 | 81,428 B | baseline |
| core 3 port alone (gracious-liskov) | 85,452 B | from core3-port-notes.md |
| this branch, core 3, after all changes | 79,332 B | includes the core 3 port and the BT pairing branch |

Flash is at 93% of the huge_app partition on core 3 (about 200 KB headroom).

## Changes, in commit order

### 1. Heap reporting (`src/core/memory_monitor.h`)
`logHeapCaps()` prints internal free / largest block (and the lowest largest
block seen since boot), PSRAM free / largest when fitted, and the current mode.
Printed once after setup (`[HEAP boot]`) and every 30 s (`[HEAP 30s]`).
No RAM cost. This is the before/after instrument for everything below.

### 2. On-demand web server (`src/web/server/web_server.h`)
The AsyncWebServer no longer starts when WiFi gets an IP. It starts when a
screen that shows the web URL is current (System Info, WiFi Setup, Web
Interface, Web Files), when a `MODE_WEB_*` mode runs, or always when the new
**Web Server: ALWAYS ON** toggle (Settings > WiFi > Web Interface) is set.
It stops after 10 minutes with no HTTP request, no WebSocket client and no
such screen (`updateWebServerLifecycle()` in the main loop). AP mode / SETUP
MODE keeps it up for as long as the AP exists.

- Expected gain while the server has never been started: the AsyncTCP task
  stack (16 KB, `CONFIG_ASYNC_TCP_STACK_SIZE`) plus the route handler
  objects, mDNS, and the lwIP listener, roughly 20-25 KB internal on both
  board types. AsyncTCP never deletes its task once created, so after the
  first start/stop cycle only the handler objects and listener come back.
- The two 4 KB `StaticJsonDocument` stack buffers in the web files
  downloader are heap documents on the PSRAM allocator now.
- Define to flip: none; the behaviour is the Preferences key
  `webpw/srvAlways` (the toggle). `WEB_SERVER_IDLE_TIMEOUT_MS` sets the idle
  stop.
- Not done: lowering `CONFIG_ASYNC_TCP_STACK_SIZE` to 8192. It needs a build
  flag in the CI workflow and `build.ps1` (the library compiles as its own
  translation unit) and a stack audit of the handlers; the downloader and the
  QSO export handlers serialise whole JSON documents into `String`s.

### 3. LVGL simple-layer buffer (`lv_conf.h`)
`LV_LAYER_SIMPLE_BUF_SIZE` 24 KB -> 8 KB, fallback 3 KB -> 2 KB. The only
widgets with `style_opa < 255` are the Morse Shooter laser and explosion
fades; both are far below the 4096 px single-chunk limit. Screen loads use
`LV_SCR_LOAD_ANIM_NONE`, so no screen transition is affected. The buffer is
a transient allocation while a translucent widget is drawn, so this lowers
the peak heap spike (both board types), not static RAM. Trade-off: a future
translucent widget larger than ~64x64 px renders in more chunks (slower,
still correct).

### 4. Lazy mode-scoped tables (`src/core/lazy_alloc.h`) - 6.5 KB static
Allocated on first use (PSRAM when fitted, internal otherwise), freed in the
owning mode's `cleanupTable` entry:

| Table | Size | Allocated by | Freed by |
|-------|-----:|--------------|----------|
| mailbox `inboxCache` (20 records, now `char[]`) | 2.5 KB | `fetchMailboxInbox()` | mailbox hub exit (`cleanupMailboxLinkScreen`) |
| `cwMemories` (10 presets) | 1.2 KB | CW Memories / Radio Output screen create, web API handlers | those screens' cleanup, skipped while the web server runs (its handlers read the table from the async task) |
| `radioMessageQueue` | 1.0 KB | first `queueRadioMessage()` | Radio Output cleanup when the queue is empty |
| `satSearch` + SGP4 propagator | ~1.5 KB | first use via `satS()` / `satP()` | satellite hub exit (`freeSatelliteData`) |
| WiFi scan `networks` (20 x 40 B) | 0.8 KB | `scanNetworks()` | `cleanupWiFiScreen()` |

Also moved the license downloader's 1 KB chunk buffer off the loop task
stack. On PSRAM boards all of this leaves internal RAM; on non-PSRAM boards
it is internal heap that comes back when the mode exits instead of a
permanent .bss reservation.

### 5. Legacy strip (user request)
Removed the pre-LVGL TFT code paths: `drawXxxUI(LGFX&)` /
`handleXxxInput(char, LGFX&)` and the `*UseLVGL` flags in the settings,
radio, repeater, Bluetooth and license modules; the legacy QSO logger UI and
input files; the TFT splash, header and `menu_navigation`; and the
never-included `settings_callsign.h`. About 60 functions and 7 files. Init
functions that still do real work lost only their display parameter.

### 6. Strings (`src/network/vail_repeater.h`, `src/audio/morse_decoder.h`, `src/web`)
Repeater room/callsign/status/room-input and the ChatMessage / RoomInfo /
UserInfo records are fixed `char[]` sized to the protocol, so chat and
user-list updates no longer churn the heap. `morseToText()` is a static
table lookup returning `const char*` (it used to build several `String`s per
decoded character). The 34 JSON documents under `src/web` use
`psramJsonAllocator()`. Expected gain: fragmentation, not static RAM.

Still `String`-based (follow-ups): mailbox and CW School link code/url/error
globals, `decodedText` / `decodedMorse` in practice mode, the WiFi screen
error/failed-SSID strings, `cachedRemoteVersion`, the web download progress
record, `bleCore.connectedDeviceName`.

### 7. Shared LVGL styles (`src/lvgl/lv_theme_summit.h`, `lv_widgets_summit.h`)
Four shared styles (transparent, transparent container, secondary small
label, key catcher) replace the same 2-4 inline property sets on about 100
objects in the mode and settings screens, plus `createKeyCatcher()` for the
ten focus-container blocks and `lvLabelSetIfChanged()` for polled labels.
Each inline `lv_obj_set_style_*` costs a local style entry on the object, so
this saves heap while a screen is open (both board types). Navigation and
layout are unchanged: every focusable widget keeps its key handler and
`addNavigableWidget()` call.

### 8. LVGL heap in PSRAM (`lv_conf.h`, `src/lvgl/lv_mem_psram.h`) - PSRAM boards only
`LVGL_MEM_IN_PSRAM` (default 1) routes `lv_mem_alloc` / `free` / `realloc`
to `heap_caps_malloc(MALLOC_CAP_SPIRAM)` with an internal-heap fallback.
Boards without PSRAM fall straight through to the internal heap (no-op). The
display draw buffer is allocated separately in `lv_init.h` and stays
internal. First thing to flip to 0 if menu navigation feels slower on a
PSRAM board.

## Defines to flip

| Define | Where | Default | Effect |
|--------|-------|---------|--------|
| `LVGL_MEM_IN_PSRAM` | `lv_conf.h` | 1 | 0 keeps all LVGL allocations internal |
| `LV_LAYER_SIMPLE_BUF_SIZE` | `lv_conf.h` | 8 KB | raise back to 24 KB if translucent effects stutter |
| `WEB_SERVER_IDLE_TIMEOUT_MS` | `web_server.h` | 10 min | idle stop for the on-demand server |
| `MAILBOX_INBOX_CACHE_SIZE` | `morse_mailbox.h` | 20 | inbox rows (x ~124 B) |

## Hardware test list

Nothing in this branch has been flashed. Test on a PSRAM unit and a
non-PSRAM unit:

1. Boot heap lines: note `[HEAP boot]` internal free / largest on both boards,
   then the `[HEAP 30s]` line after WiFi connects. Compare with the 90 KB /
   47 KB numbers from the core 3 port.
2. Keying over the Vail repeater with WiFi connected: sidetone clean, chat
   send/receive, room list and user list render, custom room entry.
3. Menu navigation speed with `LVGL_MEM_IN_PSRAM=1` on the PSRAM board.
4. Web interface: System Info shows the URL and the server answers; leave to
   a menu and confirm it stops after 10 min idle (serial log); ALWAYS ON
   toggle starts it with WiFi; web practice mode from the browser.
5. Captive portal / AP SETUP MODE still serves the setup page.
6. Mailbox inbox: list, open a message, back, list again (cache reused),
   leave hub, re-enter (re-fetch).
7. CW memories: create / edit / delete / preview on device and from the web
   API; Radio Output memory selector; web `send` while on another screen.
8. Satellite tracker: pass list, pass detail, live view, leave hub, re-enter.
9. WiFi settings: scan list, connect, failed-password retry, AP mode.
10. Bluetooth keyboard pairing (the merged pairing-hardening branch plus the
    NimBLE 2.x callback port): scan, pair with passkey, reconnect after
    power cycle, forget.
11. Morse Shooter laser/explosion fades (8 KB layer buffer).
