# Device Self Check: Plan

A self check for the Summit that runs from the **Get Help page on the updater
site** (`online-updater/troubleshoot.html` in the vail-adapter repo). The user
plugs into a computer, the page runs the checks over USB and walks them through
the guided ones. Then they **Copy** or **Save** the report and email it the
way they already do with troubleshooting logs.

No cloud service, no keys in firmware, no SD needed. Serial still works when
the SD card, WiFi or even the screen is broken.

## What exists today

- **Get Help page** (`troubleshoot.js`): pick device, Connect over Web Serial
  at 115200, live log view, **Restart Device** for the Summit (captures boot
  messages), Save `.txt`, Copy, Clear. It only reads; it never writes to the
  device.
- **Summit firmware:** prints logs to `Serial` (USB CDC) but never reads it.
  There are no `Serial.read`/`Serial.available` calls anywhere. `setTxTimeoutMs(0)`
  (`vail-summit.ino:322`) means output is dropped rather than blocking when the
  host isn't reading.

## User flow

```
Get Help page
  1. Which device?  -> Vail Summit
  2. Connect        (existing)
  3. NEW: "Run Self Check" button
       - automatic checks run (~10 s), checklist fills in on the page
       - guided checks, big buttons on the computer:
           Sound    "Play test tone" -> "Did you hear it?  Yes / No"
           Screen   Summit shows color bars -> "Do they look right?  Yes / No"
           Keyboard "Press A, 1, an arrow, ENTER on the Summit" (ticks off live)
           Paddle   "Press DIT ... now DAH" (ticks off live)
       - results: PASS / WARN / FAIL with plain-English advice
  4. Copy Report / Save Report / "Email support" (mailto, subject prefilled)
       report = results + answers + event history + the full log
```

The advice text lives on the website, so it can be improved without a firmware
release.

The on-device Settings > Self Check screen becomes optional (phase 4). It can
run the same checks and show results, but the computer is how reports get to
you.

## Firmware: serial command console

New `src/core/serial_console.h`, polled from `loop()`:

- Non-blocking line reader: up to 64 bytes per loop into a `char[64]`, act on
  `\n`. No `String`.
- Commands are prefixed so stray typing can't trigger them:

| Command | Does |
|---|---|
| `@vail hello` | replies `@vail {"type":"hello","fw":"0.93","proto":1}`. The page uses this to detect support and shows "update your firmware" if there's no reply in 3 s. |
| `@vail diag` | runs the automatic checks as a state machine and emits one result line per check |
| `@vail events` | dumps the crash-surviving event history |
| `@vail tone` | plays the test tone (600 Hz, 1 s, current volume) |
| `@vail tone loud` | same at higher volume, to tell "volume at 0" apart from "dead speaker" |
| `@vail screen` | full-screen color bars until `@vail screen off` or ESC |
| `@vail keys` / `@vail paddle` | for 20 s, reports each key / paddle press as an event line |

- Machine-readable lines are JSON with an `@vail ` prefix, for example
  `@vail {"type":"check","id":"sd","status":"fail","detail":"CMD0 no response"}`.
  The normal log keeps flowing around them; the page parses the `@vail` lines
  and keeps everything for the copied report.
- **TX buffer gotcha:** with `setTxTimeoutMs(0)`, a 3 KB burst overflows the
  CDC buffer and lines vanish. Emit one line per loop tick from the state
  machine, or raise the TX timeout while a diag session is active
  (`Serial.isConnected()`), then restore it.
- While a diag session runs, the Summit shows "Self check running from
  computer". During `tone`, no display updates (audio rule).

## Checks

### Automatic (`@vail diag`)

| Check | How | Building block |
|---|---|---|
| Firmware / web files version | constants | `config.h:12-23` |
| Last reset reason, crash count | `esp_reset_reason()`, RTC breadcrumbs | `vail-summit.ino:325`, pattern in `ble_keyboard_host.h:68,1102` |
| Heap / PSRAM / fragmentation | memory snapshot, lowest max block | `memory_monitor.h:31,19,85-102` |
| NVS usage | existing stats | `vail-summit.ino:511` |
| **SD card** | wiring test + card test, see below; `/www/` present, `version.txt` vs `WEB_FILES_VERSION` | `sd_card.h:25-111` |
| I2C bus | probe CardKB `0x5F`, battery gauge `0x36`/`0x0B` | `runI2CScan()` `hardware_init.h:84` |
| Battery | gauge found, voltage, percent | `hardware_init.h:98` |
| Audio driver | I2S init succeeded | `i2s_audio.h:316` (needs a getter for `i2s_initialized`) |
| WiFi | saved network count, connected, RSSI, internet reachable | `internet_check.h:104,291` |
| BLE keyboard | paired, connected, crash count | `ble_keyboard_host.h:72,1043` |

### SD wiring test

The SD slot is on the back of the display board, and its 4 pins (CS 38,
MOSI 35, SCK 36, MISO 37) reach the Feather through their own header joints. A
working screen proves the display header, not the SD header. There is no
card-detect pin.

**Empty slot (limited):**

- **Bridges and shorts.** Detach the 4 pins from SPI briefly (display
  deselected, no flush running). Drive one pin at a time high, then low, with
  the other three as inputs with pulls. A neighbor that follows means a bridge.
  A pin that reads low with its pull-up on means a short to GND; high with its
  pull-down on means a short to 3V3. Then re-attach SPI.
- **Open joints, only if the display board has pull-ups on the SD lines.** Set
  the pin as an input with the internal pull-down (~45k). A 10k board pull-up
  makes it read HIGH if the joint is good and LOW if it's open. Confirm on the
  bench first: measure GPIO38/37 to 3V3 on an unpowered known-good board, then
  lift a pin and check the test flags it. With no pull-ups, report "can't
  check without a card".

**Card inserted (the real test).** Talk to the card directly so the result
says where it failed:

| Step | Result | Meaning |
|---|---|---|
| CMD0 at 400 kHz | only 0xFF back | no response: CS/SCK/MOSI/MISO joint, or no card |
| CMD0 | garbage / 0x00 | MISO bridged or stuck |
| CMD8, ACMD41, CMD58 | OK | wiring is good |
| Read a block at 400 kHz, 1, 4, 10 MHz | fails only at higher speeds | marginal joint |
| Wiring good, `SD.begin()` fails | | format: not FAT32 / exFAT card |
| Write/read/verify a 4 KB file | fails | read-only or worn-out card |

Bypass the 30 s `SD_INIT_RETRY_MS` backoff so the test is real. A failed
`SD.begin()` blocks about 1 s, which is fine as one state-machine step.

## Event history (what happened before)

RTC_NOINIT ring of ~2 KB, which survives panics and watchdog resets (magic
word + CRC to detect power-on garbage). `diagEvent("SD", "mount fail")` at the
places that matter: SD mount result, I2S init failure, heap low, WiFi connect
fail, BLE state changes, last ~10 mode changes. `@vail events` dumps it, and
`@vail diag` includes it.

The page's existing **Restart Device** already records boot messages, so a
boot loop or panic backtrace lands in the log too.

## Website changes (vail-adapter repo, `online-updater/`)

- `troubleshoot.js`: open a writer on `state.port.writable`, add
  `sendCommand()`, and parse `@vail ` lines out of the existing read loop.
- "Run Self Check" panel (Summit only): send `hello`, run `diag`, render the
  checklist, then the guided steps.
- Report = header (date, fw, results, guided answers) + `@vail events` output +
  full raw log. Reuse the existing Copy and Save buttons. Add an "Email
  support" `mailto:` with subject `Summit self check <fw> <callsign>` and a
  short body telling them to paste (mailto bodies over ~2 KB get cut off, so
  don't try to put the whole report in it).
- Privacy note on the page: callsign is included; no passwords, no WiFi names.

## Phases

1. **Firmware:** serial console + `hello` + `diag` (automatic checks, including
   the SD card test) + event ring.
   **Site:** command writer, Run Self Check, checklist, report copy/save.
   This alone covers the SD case.
2. Guided checks: tone, screen, keys, paddle (firmware commands + site
   buttons).
3. SD empty-slot wiring test, after the bench check of the pull-ups.
4. Optional on-device Self Check screen in Settings (mode registration,
   cleanup table, `linear_nav_handler` results list). Also fill the missing SD
   and battery rows on System Info (`lv_settings_screens.h:1767`).

## Open questions

- Does the Summit's display board have pull-ups on the SD lines? (Decides
  whether phase 3 can find open joints.)
- Include the callsign in the report by default?
