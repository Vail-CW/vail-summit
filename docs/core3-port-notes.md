# Arduino ESP32 core 3.x port notes

Status: **compiles, not yet hardware-tested.** The release build stays on core
2.0.14 until the checklist at the bottom has been run on a bench unit.

The same source now builds on both cores. Every core-specific line is behind
`ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)` so nothing changes
for the 2.0.14 release build.

## Why

`docs/BUILDING.md` claimed cores newer than 2.0.14 boot-loop with the ST7796S
display. A standalone spike (`C:\spike3`, 2026-10-12) ran core 3.3.12 /
ESP-IDF 5.5.5 with LovyanGFX 1.2.32 on a bench unit: the unchanged LGFX class
from `src/core/hardware_init.h` drove the display, the unchanged legacy I2S
config played a clean tone, and `touchRead()` worked. The boot-loop claim was
wrong, or at least no longer true.

Reasons to want core 3.x eventually: current toolchain (GCC 14, IDF 5.5),
`CONFIG_LWIP_IPV4_NAPT=y` compiled in (the captive-portal/tethering idea needs
it; on 2.0.14 it was compiled out), IRAM-safe I2S ISR, and a core that still
receives fixes.

## Environments

| | Release (unchanged) | Core 3.x (new) |
|---|---|---|
| Local folder | `C:\acli` | `C:\acli3` |
| CI | `build_and_release.yml` | `build_core3.yml` (non-blocking, no release) |
| esp32 core | 2.0.14 (IDF 4.4) | **3.3.12** (IDF 5.5.5) |
| LovyanGFX | 1.1.16 | **1.2.32** |
| WebSockets | 2.4.1 | **2.6.1** (see below) |
| lvgl | 8.3.11 | 8.3.11 |
| NimBLE-Arduino | 1.4.2 | **2.5.1** (see below) |
| ArduinoJson | 7.0.4 | 7.0.4 |
| ESP Async WebServer | 3.6.0 | 3.6.0 |
| Async TCP | 3.3.2 | 3.3.2 |
| Adafruit MAX1704X | 1.0.3 | 1.0.3 |
| Adafruit LC709203F | 1.3.4 | 1.3.4 |
| Adafruit BusIO | 1.17.4 | 1.17.4 |

`lv_conf.h` sits at `C:\acli3\user\libraries\lv_conf.h`, next to the `lvgl`
folder, exactly as in `C:\acli`. Setup commands are in `docs/BUILDING.md`
under "Option 2b".

Both environments must compile with their own `--build-path` (`C:\b2` and
`C:\b3`). arduino-cli keys its default intermediate folder on the sketch path
only, so two toolchains building the same sketch share one object cache and
silently mix objects; the first symptom seen was a core 3 output carrying the
2.0.14 bootloader.

FQBN and options are identical for both:
`esp32:esp32:adafruit_feather_esp32s3:CDCOnBoot=cdc,PartitionScheme=huge_app,PSRAM=enabled,FlashSize=4M,USBMode=hwcdc`

## Code changes

Six source files. All diffs are additive and guarded.

### 1. LEDC backlight API (`src/settings/settings_brightness.h`)

`ledcSetup(channel, freq, res)` and `ledcAttachPin(pin, channel)` were removed
in core 3.0. Replacement: `ledcAttach(pin, freq, res)` allocates a channel
internally, and `ledcWrite(pin, duty)` is addressed by **pin** instead of
channel. Both `setupBrightnessPWM()` and `applyBrightness()` are guarded.
`BRIGHTNESS_PWM_CHANNEL` in `config.h` is still used by the 2.0.14 path.

### 2. Legacy I2S deprecation warning (`src/audio/i2s_audio.h`)

`driver/i2s.h` still ships in IDF 5.5 (under `driver/deprecated/`) and every
call the project makes (`i2s_driver_install`, `i2s_set_pin`, `i2s_write`,
`i2s_zero_dma_buffer`, `i2s_driver_uninstall`) compiles and links unchanged.
The header emits `#warning` on include; the official off-switch
(`CONFIG_I2S_SUPPRESS_DEPRECATE_WARN`) lives in the prebuilt sdkconfig, so the
include is wrapped in `#pragma GCC diagnostic ignored "-Wcpp"`, same as the
spike. The `i2s_config_t` initialiser is unchanged: IDF 5 adds
`.mclk_multiple` and `.bits_per_chan` after `.fixed_mclk`, and leaving them
out zero-initialises them to the "default" enumerators.

Migrating to `driver/i2s_std.h` is **deferred** on purpose. The audio path
(DRAM sine LUT, IRAM tone functions, core 0 task placement, 16 DMA buffers,
LEVEL3 interrupt) was tuned by trial against flash-cache stalls; rewriting the
driver layer at the same time as the core would make a regression impossible
to attribute.

### 3. Task watchdog (`vail-summit.ino`)

The signature guard was already in place before this port:
`esp_task_wdt_init()` takes an `esp_task_wdt_config_t*` on core 3 and
`(timeout_s, panic)` on 2.0.14. `esp_task_wdt_add()` / `esp_task_wdt_reset()`
are unchanged (grep across `src/` and the sketch).

First boot of the core 3 build on the bench unit logged
`E task_wdt: esp_task_wdt_init(517): TWDT already initialized` right before
`[WDT] Loop watchdog armed`. Core 3's sdkconfig has `CONFIG_ESP_TASK_WDT_INIT`
so the startup code already created the watchdog with the default 5 s timeout;
our init call was rejected and the 30 s loop timeout was never applied. The
core 3 branch now calls `esp_task_wdt_init()` and, on `ESP_ERR_INVALID_STATE`,
`esp_task_wdt_reconfigure(&cfg)` so the configured timeout and panic flag take
effect. 2.0.14 path unchanged.

### 4. NVS iterator API (`src/settings/settings_sd_backup.h`)

Compile error on core 3. IDF 5 changed the iterator API used by the SD
settings backup:

| IDF 4.4 (2.0.14) | IDF 5.5 (3.3.12) |
|---|---|
| `nvs_iterator_t it = nvs_entry_find(part, ns, type);` | `esp_err_t r = nvs_entry_find(part, ns, type, &it);` |
| `it = nvs_entry_next(it);` (NULL when done, auto-released) | `r = nvs_entry_next(&it);` (`ESP_ERR_NVS_NOT_FOUND` when done) |
| nothing | `nvs_release_iterator(it);` required |

The core 3 branch loops on `r == ESP_OK` and releases the iterator after the
loop. Behaviour (which entries are written, in which order) is the same.

### 5. `esp_read_mac()` include (`src/bluetooth/ble_core.h`)

Compile error on core 3: `esp_read_mac` and `ESP_MAC_BT` were no longer
declared. On IDF 4.4 they arrived transitively through `esp_system.h`; IDF 5
moved them to `esp_mac.h`, which also exists on 4.4. Added
`#include <esp_mac.h>` unconditionally.

### 6. Duplicate `IRAM_ATTR` on forward declarations (`src/audio/i2s_audio.h`, `src/core/task_manager.h`)

Warning on core 3, not an error, but worth fixing because it concerns the
audio hot path. On core 3 each `IRAM_ATTR` expands to a uniquely numbered
`section(".iram1.N")`, so an `IRAM_ATTR` on a forward declaration and another
on the definition produce two different section names and GCC warns
"ignoring attribute section ... conflicts with previous". The first one (the
declaration's) wins, which is still an `.iram1.*` section, so the functions
did land in IRAM either way. Removed the attribute from the four forward
declarations (`continueTone`, `startToneInternal`, `continueToneInternal`,
`stopToneInternal`); the definitions keep it. Identical code on both cores.

### 7. NimBLE-Arduino 2.x port (`src/bluetooth/*.h`)

Why: see "NimBLE-Arduino 1.4.2 on core 3.3.12" below. The 2.0.14 build keeps
1.4.2, so every call site that differs is behind `SUMMIT_NIMBLE_V2`, defined
in the new `src/bluetooth/nimble_compat.h` from `NIMBLE_CPP_VERSION_MAJOR`
(2.x defines it, 1.4.2 has no version macro). Where a callback body is shared
it was moved into a static helper and the two `#if` branches are thin
overrides that call it. Fourteen guarded sites:

| File | 1.4.2 | 2.5.1 |
|---|---|---|
| `ble_core.h` | `NimBLEServerCallbacks::onConnect(NimBLEServer*)` | `onConnect(NimBLEServer*, NimBLEConnInfo&)` |
| `ble_core.h` | `onDisconnect(NimBLEServer*)` | `onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason)` |
| `ble_core.h` | services start themselves | `pServer->start()` before advertising (also done implicitly by `startAdvertising()`; explicit so a failure is logged) |
| `ble_midi.h` | `NimBLECharacteristicCallbacks::onWrite(NimBLECharacteristic*)` / `onRead(...)` | both take an extra `NimBLEConnInfo&` |
| `ble_midi.h` | `NimBLEService::start()` | deprecated no-op, skipped |
| `ble_hid.h` | `NimBLEHIDDevice::manufacturer/pnp/hidInfo/reportMap/inputReport/hidService` | `setManufacturer/setPnp/setHidInfo/setReportMap/getInputReport/getHidService` |
| `ble_hid.h` | `NimBLEHIDDevice::startServices()` | deprecated, skipped |
| `ble_hid.h` | `NimBLEAdvertising::setScanResponse(true)` | `enableScanResponse(true)` |
| `ble_keyboard_host.h` | `NimBLEClientCallbacks::onDisconnect(NimBLEClient*)` | `onDisconnect(NimBLEClient*, int reason)`; `onConnectFail(NimBLEClient*, int)` added for logging |
| `ble_keyboard_host.h` | `onAuthenticationComplete(ble_gap_conn_desc*)` | `onAuthenticationComplete(NimBLEConnInfo&)` (`isEncrypted()`, `isBonded()`) |
| `ble_keyboard_host.h` | `uint32_t onPassKeyRequest()` returns the key | `uint32_t onPassKeyDisplay(NimBLEConnInfo&)` returns it (we are `BLE_HS_IO_DISPLAY_ONLY`); `onPassKeyEntry(NimBLEConnInfo&)` calls `NimBLEDevice::injectPassKey(info, BLE_KB_PASSKEY)` for the case where the peer asks us to enter |
| `ble_keyboard_host.h` | `bool onConfirmPIN(uint32_t)` returns true | `void onConfirmPasskey(NimBLEConnInfo&, uint32_t)` calls `NimBLEDevice::injectConfirmPasskey(info, true)` |
| `ble_keyboard_host.h` | `NimBLEAdvertisedDeviceCallbacks::onResult(NimBLEAdvertisedDevice*)` + separate `NimBLEScan::start(sec, completeCB, false)` for the non-blocking form | `NimBLEScanCallbacks::onResult(const NimBLEAdvertisedDevice*)` + `onScanEnd(const NimBLEScanResults&, int)`; registered with `setScanCallbacks()`; `start(ms, false, true)` is always non-blocking and takes **milliseconds** |
| `ble_keyboard_host.h` | `NimBLEClient::setConnectTimeout(10)` seconds | `setConnectTimeout(10000)` milliseconds |
| `ble_keyboard_host.h` | `getCharacteristics(true)` returns `std::vector*` | returns `const std::vector&` |
| `ble_keyboard_host.h` | `NimBLEDevice::setPower(ESP_PWR_LVL_P9)` | `setPower(9)` (dBm) |

Unchanged on both: `NimBLEDevice::init/deinit/setSecurityAuth/setSecurityIOCap/deleteBond/isBonded/getScan/createClient/deleteClient`, `NimBLEClient::connect/secureConnection/getConnInfo/getPeerAddress/setConnectionParams`, `onConnParamsUpdateRequest`, `NimBLERemoteCharacteristic::subscribe(true, cb, true)` (the mandatory confirmed-write CCCD subscribe keeps the same signature), `readValue()`, `getDescriptor()`, `writeValue()`, `NimBLEConnInfo::getIdAddress/getAddress/getMTU/getConnInterval/isEncrypted`, `NimBLEAttValue`. The HOGP enumeration sequence (PnP ID, HID Info, Report Map, Report Reference, Protocol Mode), bond hygiene on forget/fresh-pair, the rotating-address reconnect matching and the connection-parameter acceptance are untouched.

**Not in this branch:** the connect/pair worker task (`bleKBConnectTask`)
and RTC boot-crash guard from the v0.91 pre-release branch
`brett/bt-keyboard-pairing-hardening-87e4f2` are not on `main`, so they are
not here either. That branch rewrites ~440 lines of `ble_keyboard_host.h`;
merging it onto this one will conflict in the callback classes and the
connect function, and its NimBLE calls will need the same `SUMMIT_NIMBLE_V2`
treatment (same table as above, plus whatever `NimBLEClient` calls run on the
worker task). Do that merge before any core 3 BLE field test: on this branch
`connectToBLEKeyboardByAddress()` still blocks `loop()` for the connect plus
pairing sequence exactly as 0.90 did.

### 8. Pre-init heap check (`src/bluetooth/nimble_compat.h`, `ble_core.h`, `ble_keyboard_host.h`)

`bleHeapOkForInit(who)` logs `ESP.getFreeHeap()` and `ESP.getMaxAllocHeap()`
immediately before every `NimBLEDevice::init()` and refuses the init when
free heap is below `BLE_MIN_FREE_HEAP` (60,000 B) or the largest block below
`BLE_MIN_LARGEST_BLOCK` (20,000 B), both in `config.h`. On refusal
`bleLastErrorMsg` gets `Low memory (NNK free) - turn WiFi off`, and:

- HID / MIDI: `initBLECore()` leaves `bleCore.initialized` false with state
  `BLE_STATE_ERROR`; `startBTHID()` / `startBTMIDI()` now check that before
  creating the HID device or MIDI service (previously they would have
  dereferenced a null server) and show the message on the mode screen. The
  `BLE_STATE_ERROR` case in both update loops shows the message instead of
  a bare "Error".
- Keyboard host: `initBLEKeyboardHost()` leaves the state `IDLE` (so nothing
  touches the uninitialised stack), stores the message in
  `bleKBHost.lastError`, and `getBLEKBStateString()` returns it instead of
  "Idle" so the settings screen shows it. `startBLEKeyboardScan()` and
  `connectToBLEKeyboardByAddress()` bail out if the init they triggered was
  refused.

The thresholds are deliberately conservative: the core 3 bench measurement
with WiFi connected (about 90 KB free, 47 KB largest block) passes them, so
they only catch a genuinely starved heap. Raise them if the controller is
seen failing above these numbers.

### Not changed

- LGFX class, `touchRead()`, `psramFound()`, `ESP.getFreeHeap()`: verbatim.
- `Preferences`, `SD`, `SPIFFS`, `WiFi`, `ESPmDNS`, `HTTPClient`,
  `WiFiClientSecure`: all compile unchanged. Core 3 aliases
  `WiFiClientSecure` to `NetworkClientSecure`.

## Library findings

### WebSockets 2.4.1 does not compile on core 3

`WebSocketsClient.cpp:250` calls `setCACertBundle(const uint8_t*)`; core 3.0.4+
changed `NetworkClientSecure::setCACertBundle` to `(bundle, size)`. 2.6.1 is
the version used here; it guards that call by core version and is otherwise
the same client API the project uses in `vail_repeater.h` (`beginSSL`,
`onEvent`, `loop`, `sendTXT`, `disconnect`, `setExtraHeaders`,
`setReconnectInterval`, `enableHeartbeat`, the `WStype_*` enum). Because the
repeater uses `beginSSL`, the TLS path now goes through core 3's
`NetworkClientSecure` with mbedTLS 3; that is part of why item 1 of the
checklist runs with the repeater connected. Nothing in `src/` changed for it. The minimum version that
compiles was not bisected; 2.6.1 is the oldest release known to carry the
guard. **The 2.0.14 environment stays on 2.4.1.**

### NimBLE-Arduino 1.4.2 on core 3.3.12: compiles, then corrupts the heap

1.4.2 compiles and links on 3.3.12 with no call-site change, but with about
1,100 `"CONFIG_BT_NIMBLE_..." redefined` warnings: core 3's prebuilt
sdkconfig enables IDF's own NimBLE host (`CONFIG_BT_NIMBLE_ENABLED=1`, roles,
`MAX_CONNECTIONS 3`, `MAX_BONDS 3`, transport buffer sizes) whereas 2.0.14's
enabled Bluedroid, and 1.4.2 vendors its own host that re-`#define`s those
macros in `nimconfig.h` (ACL buffer count 12, HCI event buffers 30/8, ...).

On the bench unit that mismatch was fatal. Selecting the external keyboard
option gave stuttering audio, then a panic and reboot. Symbolised backtrace:
`loop() -> onLVGLMenuSelect -> initializeModeInt -> startBTKeyboardSettings
-> initBLEKeyboardHost -> NimBLEDevice::init -> esp_bt_controller_init ->
btdm_controller_init -> r_llm_env_init -> malloc_internal_wrapper ->
heap_caps_malloc -> tlsf block_locate_free` with a garbage free-list pointer
(`0x39cb39cb`) in the allocator; core 0 died with a cache/MMU fault while
idle; the controller logged `MAGIC fadebead VERSION 0001000b` then
`... VERSION 00010001`, two different version values. The controller was
being configured with values the prebuilt binary was not built for and
scribbled over the heap during init.

NimBLE-Arduino 2.x is the line upstream maintains for core 3.x. 2.5.1 is now
in `C:\acli3` and the CI core 3 job; the 2.0.14 environment keeps 1.4.2. The
2.x build emits none of the `redefined` warnings. Every API difference the
project hit is recorded under code change 7.

**Shared source assessment.** Keeping both majors in one source cost
fourteen `#if SUMMIT_NIMBLE_V2` sites across four files, with callback bodies
factored into helpers so the branches are only signatures. That is
manageable, but every future BLE change has to be written twice and only one
side gets hardware-tested at a time. Recommendation: once the core 3 build
has passed the hardware checklist, move the 2.0.14 build to NimBLE-Arduino
2.5.1 as well (2.x supports core 2.0.x), rebuild and re-run the keyboard
pairing tests on 2.0.14, then delete the `#else` branches. Until then the
`#if` keeps the release build byte-identical in its BLE stack. Do not flip
the release workflow's `LIB_NIMBLE_ARDUINO` before that test: the pairing
behaviour on 2.0.14 is the hard-won part.

Heap headroom is tight regardless of library: on core 3 with WiFi connected
the bench unit had about 90 KB free with a 47 KB largest block (173 KB free
without WiFi). The pre-init heap check (change 8) exists because of this.

### Deprecation warnings (harmless, both pre-existing)

ArduinoJson 7.0.4 `StaticJsonDocument` / `DynamicJsonDocument` /
`createNestedArray` (GCC 14 now surfaces these), `AsyncWebServerRequest::send_P`,
`volatile ++` in `task_manager.h`, mixed-enum bitwise ops in two LVGL screen
files. None affect behaviour; cleanup is independent of the core port.

## Flash and RAM

Same source, same FQBN, `--export-binaries`, separate `--build-path` per
toolchain, 2026-10-12. Final state of this branch (NimBLE 1.4.2 on the left,
2.5.1 on the right):

| | core 2.0.14 | core 3.3.12 | delta |
|---|---|---|---|
| Program storage (of 3,145,728 B) | 2,655,453 B (84%) | 2,956,407 B (93%) | **+300,954 B** |
| Headroom in the 3 MB app partition | 490,275 B | 189,321 B | |
| Global RAM (of 327,680 B) | 81,476 B (24%) | 86,148 B (26%) | +4,672 B |
| `vail-summit.ino.bin` | 2,655,824 B | 2,956,560 B | |
| `bootloader.bin` | 15,104 B | 19,968 B | |
| `partitions.bin` | 3,072 B | 3,072 B | |

For reference, before the NimBLE 2.x port the core 3 build with 1.4.2 was
2,945,579 B / 85,452 B RAM, so NimBLE 2.5.1 costs about 10 KB of flash and
700 B of RAM. The remaining ~290 KB growth is IDF 5 itself (newer lwIP,
mbedTLS 3, NimBLE host in the prebuilt libs, Network library) and is the
single biggest cost of the port. With under 190 KB headroom the huge_app
scheme still fits, but a few more features would force a partition rethink
(there is 1 MB SPIFFS to trade). Binaries are in `build/core3/` of the build
mirror (`C:\vs3\build\core3` for the worktree build, `.elf` alongside for
symbolising backtraces) and the 2.0.14 comparison set in `build/core2/`.

The bootloader and partition table come from the core, so a core 3 image must
be flashed as a full set (all three files). Flashing only `vail-summit.bin`
over a 2.0.14 bootloader is untested.

## Deferred

- `driver/i2s_std.h` migration (see change 2).
- NimBLE-Arduino 2.x on the 2.0.14 build, then removing the `#else`
  branches (see "Shared source assessment").
- Merge of the v0.91 keyboard-pairing hardening branch onto this one (see
  change 7).
- `ESP_INTR_FLAG_IRAM` on the I2S interrupt: the `i2s_audio.h` comment says
  the 2.0.14 driver was not IRAM-safe; core 3's sdkconfig has
  `CONFIG_I2S_ISR_IRAM_SAFE=y`. Might help sidetone during flash writes; not
  tried.
- ArduinoJson 7.x deprecation cleanup.
- No-PSRAM (`PSRAM=disabled`) variant not built on core 3 yet.
- Hardware test (below). Nothing has been flashed from this branch.

## Hardware test checklist (ordered by risk)

Flash all three binaries from `build/core3/`. Boot screen should report
firmware 0.90 and the serial banner should show the IDF v5.5.5 string.

1. **Sidetone while keying with WiFi connected and the Vail repeater
   active.** This is the whole reason the audio path is the way it is. Key
   continuously for a minute on the Decoder room with another station
   sending; listen for clicks, buzz, dropouts or pitch wobble. Compare with
   the 2.0.14 build on the same unit if anything is audible. Also key while
   the web interface is loading pages.
2. **I2S beeps in menus.** Navigate menus, change volume, use the practice
   modes with no WiFi. Clean tone, correct pitch, no stuck note on ESC.
3. **BLE keyboard pairing and reconnect** (now on NimBLE-Arduino 2.5.1).
   First confirm the crash is gone: open Settings > Bluetooth > External
   Keyboard with WiFi connected and check the `[BLEKB] Heap before NimBLE
   init` line, no `MAGIC fadebead` double version print, no panic. Then
   fresh pair, power-cycle reconnect, forget-and-repair, and a keyboard that
   rotates its address. Watch for `Auth complete - encrypted=1 bonded=1`,
   the `Subscribed to report char` lines and raw `Report h=` traces; a
   keyboard that stays in pairing mode with no reports is the CCCD /
   enumeration regression the 1.x code was tuned against. Also exercise the
   passkey path on a keyboard that demands a PIN (`onPassKeyDisplay` on
   2.x). Then BLE HID output (pair with a PC, Ctrl keys arrive) and BLE MIDI
   (service visible, notes arrive, Program Change accepted). Finally force
   the heap check: connect WiFi, open the web interface, then start BLE and
   confirm either a clean start or the `Low memory` message, never a panic.
4. **WiFi scan and connect.** Scan lists networks, saved network
   auto-connects, captive/open network, reconnect after AP drop. Confirm the
   mDNS name resolves.
5. **Web interface.** Dashboard loads, WebSocket modes (web practice) start
   and stop cleanly, settings save, file download/update of web files from
   the SD card works (`send_P` deprecation path).
6. **SD card QSO logging and settings backup.** Log a QSO, export ADIF,
   run the NVS settings backup to SD (change 4 is exactly this code) and
   check the backup file has the same entry count as a 2.0.14 backup.
7. **Touch paddles.** Dit/dah detection and threshold calibration behave as
   before; no phantom keying.
8. **Brightness slider.** Full range 0 to 100 percent, no flicker at low
   values, setting persists over reboot (change 1).
9. **Deep sleep and battery gauge.** Triple-tap ESC sleep, wake on dit paddle
   (GPIO 6), battery percentage from the MAX17048 / LC709203F reads sanely
   and USB-charging state updates.
10. **Watchdog.** Confirm the boot log shows `[WDT] Loop watchdog armed`
    with no `TWDT already initialized` error before it (first bench boot had
    that error; fixed with `esp_task_wdt_reconfigure`, see change 3), and
    that the device does not spontaneously reboot during a long idle or a
    slow operation such as a WiFi scan or SD backup (the 5 s core default
    would have tripped where our 30 s timeout does not).
