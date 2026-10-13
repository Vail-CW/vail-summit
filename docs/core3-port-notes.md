# Migration to Arduino ESP32 core 3.3.12

Status: **compiles, hardware-tested only in part.** The firmware builds
exclusively on core 3.3.12 (ESP-IDF 5.5.5) from this branch on; core 2.0.14
is no longer supported and no compatibility code for it remains. Run the
checklist at the bottom before shipping a release from this core.

## Why

`docs/BUILDING.md` claimed cores newer than 2.0.14 boot-loop with the ST7796S
display. A standalone spike (`C:\spike3`, 2026-10-12) ran core 3.3.12 with
LovyanGFX 1.2.32 on a bench unit: the unchanged LGFX class from
`src/core/hardware_init.h` drove the display, the unchanged legacy I2S config
played a clean tone, and `touchRead()` worked. The full firmware was then
ported; WiFi, menus and sending practice have been confirmed on the bench
unit, BLE after the NimBLE change has not yet.

Reasons to be on core 3.x: current toolchain (GCC 14, IDF 5.5),
`CONFIG_LWIP_IPV4_NAPT=y` compiled in (the captive-portal/tethering idea
needs it; on 2.0.14 it was compiled out), IRAM-safe I2S ISR, and a core that
still receives fixes.

## Versions

| Component | Before (2.0.14 era) | Now |
|---|---|---|
| esp32:esp32 core | 2.0.14 (IDF 4.4) | **3.3.12** (IDF 5.5.5) |
| LovyanGFX | 1.1.16 | **1.2.32** |
| WebSockets | 2.4.1 | **2.6.1** |
| NimBLE-Arduino | 1.4.2 | **2.5.1** |
| lvgl | 8.3.11 | 8.3.11 |
| ArduinoJson | 7.0.4 | 7.0.4 |
| ESP Async WebServer | 3.6.0 | 3.6.0 |
| Async TCP | 3.3.2 | 3.3.2 |
| Adafruit MAX1704X | 1.0.3 | 1.0.3 |
| Adafruit LC709203F | 1.3.4 | 1.3.4 |
| Adafruit BusIO | 1.17.4 | 1.17.4 |

FQBN and options are unchanged:
`esp32:esp32:adafruit_feather_esp32s3:CDCOnBoot=cdc,PartitionScheme=huge_app,PSRAM=enabled,FlashSize=4M,USBMode=hwcdc`

`lv_conf.h` still sits next to the `lvgl` library folder. The release
workflow (`build_and_release.yml`) installs these versions; the local
`C:\acli` environment has to be upgraded by hand (commands in
`docs/BUILDING.md`).

Build cache gotcha found during the port: arduino-cli keys its default
intermediate folder on the sketch path only. Objects compiled with the old
core in `%TEMP%\arduino\sketches\...` get mixed into a new-core build and the
first symptom is a binary carrying the wrong bootloader. Clean that cache or
pass `--build-path` once after upgrading the core.

## Code changes

### 1. LEDC backlight API (`src/settings/settings_brightness.h`, `config.h`)

`ledcSetup(channel, freq, res)` and `ledcAttachPin(pin, channel)` were
removed in core 3.0. Replacement: `ledcAttach(pin, freq, res)` allocates a
channel internally, and `ledcWrite(pin, duty)` is addressed by **pin**.
`BRIGHTNESS_PWM_CHANNEL` was removed from `config.h`.

### 2. Legacy I2S deprecation warning (`src/audio/i2s_audio.h`)

`driver/i2s.h` still ships in IDF 5.5 (under `driver/deprecated/`) and every
call the project makes (`i2s_driver_install`, `i2s_set_pin`, `i2s_write`,
`i2s_zero_dma_buffer`, `i2s_driver_uninstall`) compiles and links unchanged.
The header emits `#warning` on include; the official off-switch
(`CONFIG_I2S_SUPPRESS_DEPRECATE_WARN`) lives in the prebuilt sdkconfig, so the
include is wrapped in `#pragma GCC diagnostic ignored "-Wcpp"`. The
`i2s_config_t` initialiser is unchanged: IDF 5 adds `.mclk_multiple` and
`.bits_per_chan` after `.fixed_mclk`, and leaving them out zero-initialises
them to the "default" enumerators.

Migrating to `driver/i2s_std.h` is **deferred** on purpose. The audio path
(DRAM sine LUT, IRAM tone functions, core 0 task placement, 16 DMA buffers,
LEVEL3 interrupt) was tuned by trial against flash-cache stalls; rewriting the
driver layer at the same time as the core would make a regression impossible
to attribute.

### 3. Task watchdog (`vail-summit.ino`)

`esp_task_wdt_init()` takes an `esp_task_wdt_config_t*`. The first bench boot
logged `E task_wdt: esp_task_wdt_init(517): TWDT already initialized` because
the core's sdkconfig has `CONFIG_ESP_TASK_WDT_INIT` (5 s default) and the
startup code already created the watchdog, so our 30 s loop timeout was never
applied. The code now calls `esp_task_wdt_init()` and, on
`ESP_ERR_INVALID_STATE`, `esp_task_wdt_reconfigure(&cfg)`.
`esp_task_wdt_add()` / `esp_task_wdt_reset()` are unchanged.

### 4. NVS iterator API (`src/settings/settings_sd_backup.h`)

IDF 5 changed the iterator API used by the SD settings backup:

| IDF 4.4 | IDF 5.5 |
|---|---|
| `nvs_iterator_t it = nvs_entry_find(part, ns, type);` | `esp_err_t r = nvs_entry_find(part, ns, type, &it);` |
| `it = nvs_entry_next(it);` (NULL when done, auto-released) | `r = nvs_entry_next(&it);` (`ESP_ERR_NVS_NOT_FOUND` when done) |
| nothing | `nvs_release_iterator(it);` required |

The loop runs on `r == ESP_OK` and releases the iterator after the loop.
Behaviour (which entries are written, in which order) is the same.

### 5. `esp_read_mac()` include (`src/bluetooth/ble_core.h`)

`esp_read_mac` and `ESP_MAC_BT` are declared in `esp_mac.h` on IDF 5 and no
longer arrive transitively. Added `#include <esp_mac.h>`.

### 6. Duplicate `IRAM_ATTR` on forward declarations (`src/audio/i2s_audio.h`, `src/core/task_manager.h`)

On core 3 each `IRAM_ATTR` expands to a uniquely numbered
`section(".iram1.N")`, so an `IRAM_ATTR` on a forward declaration and another
on the definition produce two different section names and GCC warns
"ignoring attribute section ... conflicts with previous". The first one wins,
which is still an `.iram1.*` section, so the functions landed in IRAM either
way. Removed the attribute from the four forward declarations
(`continueTone`, `startToneInternal`, `continueToneInternal`,
`stopToneInternal`); the definitions keep it.

### 7. NimBLE-Arduino 2.5.1 (`src/bluetooth/*.h`)

**Why the upgrade was forced.** NimBLE-Arduino 1.4.2 compiles and links on
3.3.12 with about 1,100 `"CONFIG_BT_NIMBLE_..." redefined` warnings: core 3's
prebuilt sdkconfig enables IDF's own NimBLE host (`CONFIG_BT_NIMBLE_ENABLED`,
roles, `MAX_CONNECTIONS 3`, `MAX_BONDS 3`, transport buffer sizes) whereas
2.0.14's enabled Bluedroid, and 1.4.2 vendors its own host that re-`#define`s
those macros in `nimconfig.h`. On the bench unit that mismatch was fatal:
selecting the external keyboard option gave stuttering audio, then a panic
and reboot. Symbolised backtrace: `loop() -> onLVGLMenuSelect ->
initializeModeInt -> startBTKeyboardSettings -> initBLEKeyboardHost ->
NimBLEDevice::init -> esp_bt_controller_init -> btdm_controller_init ->
r_llm_env_init -> malloc_internal_wrapper -> heap_caps_malloc -> tlsf
block_locate_free` with a garbage free-list pointer (`0x39cb39cb`); core 0
died with a cache/MMU fault while idle; the controller logged `MAGIC fadebead
VERSION 0001000b` then `... VERSION 00010001`. The controller was being
configured with values the prebuilt binary was not built for and scribbled
over the heap. NimBLE-Arduino 2.x is the line upstream maintains for core 3;
2.5.1 emits none of those warnings.

**Call-site changes** (old 1.4.2 form on the left, what the code does now on
the right):

| File | 1.4.2 | 2.5.1 |
|---|---|---|
| `ble_core.h` | `NimBLEServerCallbacks::onConnect(NimBLEServer*)` | `onConnect(NimBLEServer*, NimBLEConnInfo&)` |
| `ble_core.h` | `onDisconnect(NimBLEServer*)` | `onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason)` |
| `ble_core.h` | services start themselves | `pServer->start()` before advertising (also done implicitly by `startAdvertising()`; explicit so a failure is logged) |
| `ble_midi.h` | `NimBLECharacteristicCallbacks::onWrite(NimBLECharacteristic*)` / `onRead(...)` | both take an extra `NimBLEConnInfo&` |
| `ble_midi.h` | `NimBLEService::start()` | removed (deprecated no-op; services start with the server) |
| `ble_hid.h` | `NimBLEHIDDevice::manufacturer/pnp/hidInfo/reportMap/inputReport/hidService` | `setManufacturer/setPnp/setHidInfo/setReportMap/getInputReport/getHidService` |
| `ble_hid.h` | `NimBLEHIDDevice::startServices()` | removed (deprecated) |
| `ble_hid.h` | `NimBLEAdvertising::setScanResponse(true)` | `enableScanResponse(true)` |
| `ble_keyboard_host.h` | `NimBLEClientCallbacks::onDisconnect(NimBLEClient*)` | `onDisconnect(NimBLEClient*, int reason)`; `onConnectFail(NimBLEClient*, int)` added for logging |
| `ble_keyboard_host.h` | `onAuthenticationComplete(ble_gap_conn_desc*)` | `onAuthenticationComplete(NimBLEConnInfo&)` (`isEncrypted()`, `isBonded()`) |
| `ble_keyboard_host.h` | `uint32_t onPassKeyRequest()` returned the key for every request | `uint32_t onPassKeyDisplay(NimBLEConnInfo&)` returns it (we are `BLE_HS_IO_DISPLAY_ONLY`); `onPassKeyEntry(NimBLEConnInfo&)` calls `NimBLEDevice::injectPassKey(info, BLE_KB_PASSKEY)` for the case where the peer asks us to enter |
| `ble_keyboard_host.h` | `bool onConfirmPIN(uint32_t)` returned true | `void onConfirmPasskey(NimBLEConnInfo&, uint32_t)` calls `NimBLEDevice::injectConfirmPasskey(info, true)` |
| `ble_keyboard_host.h` | `NimBLEAdvertisedDeviceCallbacks::onResult(NimBLEAdvertisedDevice*)` + a separate completion callback passed to `NimBLEScan::start(sec, cb, false)` to get the non-blocking form | `NimBLEScanCallbacks::onResult(const NimBLEAdvertisedDevice*)` + `onScanEnd(const NimBLEScanResults&, int)`; registered with `setScanCallbacks()`; `start(ms, false, true)` is always non-blocking and takes **milliseconds** |
| `ble_keyboard_host.h` | `NimBLEClient::setConnectTimeout(10)` seconds | `setConnectTimeout(10000)` milliseconds |
| `ble_keyboard_host.h` | `getCharacteristics(true)` returned `std::vector*` | returns `const std::vector&` |
| `ble_keyboard_host.h` | `NimBLEDevice::setPower(ESP_PWR_LVL_P9)` | `setPower(9)` (dBm) |

Unchanged: `NimBLEDevice::init/deinit/setSecurityAuth/setSecurityIOCap/deleteBond/isBonded/getScan/createClient/deleteClient`,
`NimBLEClient::connect/secureConnection/getConnInfo/getPeerAddress/setConnectionParams`,
`onConnParamsUpdateRequest`, `NimBLERemoteCharacteristic::subscribe(true, cb, true)`
(the mandatory confirmed-write CCCD subscribe keeps the same signature),
`readValue()`, `getDescriptor()`, `writeValue()`,
`NimBLEConnInfo::getIdAddress/getAddress/getMTU/getConnInterval/isEncrypted`,
`NimBLEAttValue`. The HOGP enumeration sequence (PnP ID, HID Info, Report
Map, Report Reference, Protocol Mode), bond hygiene on forget/fresh-pair, the
rotating-address reconnect matching and the connection-parameter acceptance
are untouched.

**Keyboard pairing worker:** the v0.91 connect/pair worker task
(`bleKBConnectTask`) and RTC boot-crash guard are merged in, with the
NimBLE 2.x renames above applied to them. Connecting no longer blocks
`loop()`. Pairing now requires an encrypted link; an unencrypted link
is dropped instead of accepted.

### 8. Pre-init heap check (`src/bluetooth/ble_heap_guard.h`, `ble_core.h`, `ble_keyboard_host.h`)

`bleHeapOkForInit(who)` logs `ESP.getFreeHeap()` and `ESP.getMaxAllocHeap()`
immediately before every `NimBLEDevice::init()` and refuses the init when
free heap is below `BLE_MIN_FREE_HEAP` (60,000 B) or the largest block below
`BLE_MIN_LARGEST_BLOCK` (20,000 B), both in `config.h`. On refusal
`bleLastErrorMsg` gets `Low memory (NNK free) - turn WiFi off`, and:

- HID / MIDI: `initBLECore()` leaves `bleCore.initialized` false with state
  `BLE_STATE_ERROR`; `startBTHID()` / `startBTMIDI()` check that before
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

Heap headroom is tight on this core: with WiFi connected the bench unit had
about 90 KB free with a 47 KB largest block (173 KB free without WiFi). The
thresholds pass that case, so they only catch a genuinely starved heap. Raise
them if the controller is seen failing above these numbers.

### Not changed

- LGFX class, `touchRead()`, `psramFound()`, `ESP.getFreeHeap()`: verbatim.
- `Preferences`, `SD`, `SPIFFS`, `WiFi`, `ESPmDNS`, `HTTPClient`,
  `WiFiClientSecure`: all compile unchanged. Core 3 aliases
  `WiFiClientSecure` to `NetworkClientSecure`.

## Library findings

### WebSockets 2.4.1 does not compile on core 3

`WebSocketsClient.cpp:250` calls `setCACertBundle(const uint8_t*)`; core 3.0.4+
changed `NetworkClientSecure::setCACertBundle` to `(bundle, size)`. 2.6.1
guards that call by core version and is otherwise the same client API the
project uses in `vail_repeater.h` (`beginSSL`, `onEvent`, `loop`, `sendTXT`,
`disconnect`, `setExtraHeaders`, `setReconnectInterval`, `enableHeartbeat`,
the `WStype_*` enum). Nothing in `src/` changed for it. Because the repeater
uses `beginSSL`, the TLS path now goes through core 3's `NetworkClientSecure`
with mbedTLS 3; that is part of why item 1 of the checklist runs with the
repeater connected.

### Deprecation warnings (harmless, pre-existing)

ArduinoJson 7.0.4 `StaticJsonDocument` / `DynamicJsonDocument` /
`createNestedArray` (GCC 14 now surfaces these), `AsyncWebServerRequest::send_P`,
`volatile ++` in `task_manager.h`, mixed-enum bitwise ops in two LVGL screen
files. None affect behaviour; cleanup is independent of the core change.

## Flash and RAM

Same FQBN, `--export-binaries`, 2026-10-12. The 2.0.14 column is the last
build of the pre-port source on the old core, for reference.

| | core 2.0.14 (before) | core 3.3.12 (now) | delta |
|---|---|---|---|
| Program storage (of 3,145,728 B) | 2,655,113 B (84%) | 2,956,343 B (93%) | **+301,230 B** |
| Headroom in the 3 MB app partition | 490,615 B | 189,385 B | |
| Global RAM (of 327,680 B) | 81,428 B (24%) | 86,148 B (26%) | +4,720 B |
| `bootloader.bin` | 15,104 B | 19,968 B | |
| `partitions.bin` | 3,072 B | 3,072 B | |

Roughly 10 KB of the growth is NimBLE 2.5.1; the rest is IDF 5 itself
(newer lwIP, mbedTLS 3, NimBLE host in the prebuilt libs, Network library)
and is the single biggest cost of the move. The huge_app scheme still fits,
but a few more features would force a partition rethink (there is 1 MB SPIFFS
to trade).

The bootloader and partition table come from the core, so a build from this
branch must be flashed as a full set (all three files) over a 2.0.14
install. Flashing only `vail-summit.bin` over the old bootloader is untested.

## Deferred

- `driver/i2s_std.h` migration (see change 2).
- `ESP_INTR_FLAG_IRAM` on the I2S interrupt: the `i2s_audio.h` comment says
  the IDF 4.4 driver was not IRAM-safe; core 3's sdkconfig has
  `CONFIG_I2S_ISR_IRAM_SAFE=y`. Might help sidetone during flash writes; not
  tried.
- ArduinoJson 7.x deprecation cleanup.
- No-PSRAM (`PSRAM=disabled`) variant not built on core 3 yet.
- Upgrading the local `C:\acli` environment (commands in BUILDING.md).

## Hardware test checklist (ordered by risk)

Flash all three binaries from the build output. Boot screen should report
the firmware version and the serial banner should show the IDF v5.5.5
string. Already seen passing on the bench unit: boot, display, WiFi connect,
sending practice.

1. **Sidetone while keying with WiFi connected and the Vail repeater
   active.** This is the whole reason the audio path is the way it is. Key
   continuously for a minute on the Decoder room with another station
   sending; listen for clicks, buzz, dropouts or pitch wobble. Compare with
   a 2.0.14 build on the same unit if anything is audible. Also key while
   the web interface is loading pages.
2. **I2S beeps in menus.** Navigate menus, change volume, use the practice
   modes with no WiFi. Clean tone, correct pitch, no stuck note on ESC.
3. **BLE keyboard pairing and reconnect** (NimBLE-Arduino 2.5.1).
   First confirm the crash is gone: open Settings > Bluetooth > External
   Keyboard with WiFi connected and check the `[BLEKB] Heap before NimBLE
   init` line, no `MAGIC fadebead` double version print, no panic. Then
   fresh pair, power-cycle reconnect, forget-and-repair, and a keyboard that
   rotates its address. Watch for `Auth complete - encrypted=1 bonded=1`,
   the `Subscribed to report char` lines and raw `Report h=` traces; a
   keyboard that stays in pairing mode with no reports is the CCCD /
   enumeration regression the 1.x code was tuned against. Also exercise the
   passkey path on a keyboard that demands a PIN (`onPassKeyDisplay`). Then
   BLE HID output (pair with a PC, Ctrl keys arrive) and BLE MIDI (service
   visible, notes arrive, Program Change accepted). Finally force the heap
   check: connect WiFi, open the web interface, then start BLE and confirm
   either a clean start or the `Low memory` message, never a panic.
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
    with no `TWDT already initialized` error before it (change 3), and that
    the device does not spontaneously reboot during a long idle or a slow
    operation such as a WiFi scan or SD backup (the 5 s core default would
    have tripped where our 30 s timeout does not).
