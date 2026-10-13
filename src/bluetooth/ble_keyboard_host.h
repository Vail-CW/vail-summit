/*
 * BLE Keyboard Host Module
 * Implements BLE Central (Client) mode to receive input from external Bluetooth keyboards
 * Uses NimBLE-Arduino library for BLE Central functionality
 */

#ifndef BLE_KEYBOARD_HOST_H
#define BLE_KEYBOARD_HOST_H

#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include "../core/config.h"
#include "hid_keycodes.h"

// Fixed passkey shown to the user when a keyboard demands PIN entry during
// pairing (we are the "display" side; the user types this on the keyboard)
#define BLE_KB_PASSKEY 123456

// BLE HID Service and Characteristic UUIDs
#define HID_SERVICE_UUID        "1812"
#define HID_REPORT_CHAR_UUID    "2A4D"
#define HID_BOOT_INPUT_UUID     "2A22"  // Boot Keyboard Input Report
#define HID_REPORT_MAP_UUID     "2A4B"
#define HID_INFO_UUID           "2A4A"
#define HID_PROTO_MODE_UUID     "2A4E"  // Protocol Mode (0=Boot, 1=Report)
#define DIS_SERVICE_UUID        "180A"  // Device Information Service
#define DIS_PNP_ID_UUID         "2A50"

// Key buffer size (circular buffer for incoming keys)
#define BLE_KEY_BUFFER_SIZE     16

// Scan and connection timeouts
#define BLE_SCAN_DURATION_SEC   10
#define BLE_RECONNECT_INTERVAL  30000  // 30 seconds between reconnect attempts
#define BLE_MAX_FOUND_DEVICES   10

// The connect/pair sequence runs on its own FreeRTOS task (see
// connectToBLEKeyboardByAddress). Worst case per attempt is the 10s connect
// timeout plus NimBLE's fixed 30s Security Manager timeout plus discovery,
// and there are up to two attempts - so the sequence can legitimately take
// over a minute. The main loop force-drops the link past this deadline.
#define BLE_CONNECT_DEADLINE_MS   90000
#define BLE_CONNECT_TASK_STACK    8192
// NimBLE needs ~50KB of internal RAM to bring the stack up; refuse to init
// below this rather than letting the allocation fail somewhere inside the host
#define BLE_MIN_FREE_INTERNAL_HEAP 60000

// Boot crash guard: how many consecutive abnormal resets with the keyboard
// host active before auto-reconnect is switched off so the device boots clean
#define BLE_CRASH_TRIP_COUNT      2
// A connection that stays up this long proves the pairing is healthy and
// clears the crash counter
#define BLE_CRASH_CLEAR_UPTIME_MS 60000

// What the keyboard host was doing, stored in RTC memory so it survives a
// panic/watchdog reset. Read back at boot to attribute the crash.
enum BLEKBPhase : uint8_t {
  BLEKB_PHASE_NONE = 0,     // host not running
  BLEKB_PHASE_INIT,         // NimBLE stack coming up
  BLEKB_PHASE_ACTIVE,       // stack up (idle, connected, or between scans)
  BLEKB_PHASE_SCANNING,     // scan in progress
  BLEKB_PHASE_CONNECTING    // connect/pair sequence in progress
};
#define BLEKB_BOOT_MAGIC 0xB1E0CB01UL
RTC_NOINIT_ATTR uint32_t bleKBBootMagic;
RTC_NOINIT_ATTR uint8_t  bleKBBootPhase;

// BLE Keyboard Host States
enum BLEKBHostState {
  BLEKB_STATE_IDLE,           // Not active, BLE not initialized
  BLEKB_STATE_READY,          // BLE initialized, waiting for action
  BLEKB_STATE_SCANNING,       // Actively scanning for keyboards
  BLEKB_STATE_SCAN_COMPLETE,  // Scan finished, results available
  BLEKB_STATE_CONNECTING,     // Attempting to connect
  BLEKB_STATE_CONNECTED,      // Connected and receiving input
  BLEKB_STATE_DISCONNECTED,   // Was connected, now disconnected
  BLEKB_STATE_ERROR           // Error occurred
};

// Paired device info (persisted to Preferences)
struct PairedKeyboard {
  char name[32];
  char address[18];  // "XX:XX:XX:XX:XX:XX"
  uint8_t addrType;  // BLE address type (public vs random) - a reconnect to a
                     // random-static address with type "public" never succeeds,
                     // and nearly all BLE keyboards use random-static addresses
  bool valid;
};

// Scan result entry
struct ScanResult {
  char name[32];
  char address[18];
  uint8_t addrType;
  int rssi;
};

// BLE Keyboard Host context
struct BLEKBHostContext {
  BLEKBHostState state;
  NimBLEClient* pClient;
  NimBLERemoteCharacteristic* pReportChar;
  PairedKeyboard pairedDevice;

  // Circular key buffer (thread-safe via volatile)
  volatile char keyBuffer[BLE_KEY_BUFFER_SIZE];
  volatile uint8_t keyHead;
  volatile uint8_t keyTail;

  // Track previous HID report to detect key changes
  uint8_t prevKeys[6];

  // Scan results
  ScanResult foundDevices[BLE_MAX_FOUND_DEVICES];
  int foundCount;
  int selectedDevice;

  // Connection management
  bool autoReconnect;
  unsigned long lastReconnectAttempt;
  volatile bool reconnectDeviceSeen;  // set from scan callback when the paired keyboard advertises
  char reconnectAddr[18];             // currently advertised address of the paired keyboard
  uint8_t reconnectAddrType;          // (keyboards rotate their private address, so the
                                      //  saved identity address may not be what's on air)
  char lastError[40];

  // Activity tracking
  unsigned long lastKeyTime;
  unsigned long connectedSince;       // millis() when the current link came up

  // Boot crash guard (see bleKBCheckPreviousBoot)
  uint8_t crashCount;                 // consecutive abnormal resets with the host active
  uint8_t lastCrashPhase;             // BLEKBPhase the host was in when it happened
  bool autoReconnectTripped;          // auto-reconnect was switched off by the guard
};

// Global context
BLEKBHostContext bleKBHost;

// ---- Connect worker -------------------------------------------------------
// NimBLEClient::connect() and secureConnection() block the calling task until
// the stack answers - and secureConnection() waits with NO timeout of its own
// (NimBLE's Security Manager gives up after a fixed 30s). Run from the main
// loop, a keyboard that sits in pairing mode without answering stalled loop()
// past the 30s task watchdog and the device panic-rebooted; with auto-reconnect
// that repeated on every boot. The sequence therefore runs on this dedicated
// task, which is not watched by the loop WDT, and the main loop only polls
// bleKBHost.state.
struct BLEKBConnectRequest {
  char address[18];
  uint8_t addrType;
  bool freshPair;
  char name[32];       // advertised name, copied at launch (scan results may change)
};
static BLEKBConnectRequest bleKBConnectReq;
static volatile bool bleKBConnectBusy = false;      // worker task alive
// Bumped by forgetBLEKeyboardPairing(). A connect worker that started before
// a forget sees a different value at the end and must not re-save the pairing.
static volatile uint32_t bleKBPairEpoch = 0;
static volatile bool bleKBConnectAborted = false;   // main loop gave up on it
static unsigned long bleKBConnectStart = 0;


// Forward declarations
void initBLEKeyboardHost();
void deinitBLEKeyboardHost();
void startBLEKeyboardScan();
void stopBLEKeyboardScan();
bool connectToBLEKeyboard(int deviceIndex);
// freshPair: user-initiated pairing from the scan list. Deletes any stale
// bond first - a bond we remember but the keyboard forgot (it's back in
// pairing mode) makes encryption fail until both sides are power-cycled.
// Both connect functions are NON-BLOCKING: they launch the connect/pair
// sequence on a worker task and return true if it was started. Watch
// bleKBHost.state (CONNECTING -> CONNECTED / ERROR) for the outcome.
bool connectToBLEKeyboardByAddress(const char* address, uint8_t addrType, bool freshPair = false);
bool isBLEKeyboardConnectBusy();
void disconnectBLEKeyboard();
void updateBLEKeyboardHost();
bool isBLEKeyboardConnected();
bool hasBLEKeyboardInput();
char getBLEKeyboardKey();
void saveBLEKeyboardSettings();
void loadBLEKeyboardSettings();
void forgetBLEKeyboardPairing();
void bleKBClearCrashCount();
const char* getBLEKBStateString();
const char* getBLEKBPhaseString(uint8_t phase);
static void bleKBSetLastError(const char* msg);

// NimBLE Client callbacks for connection state
class BLEKBClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) override {
    Serial.println("BLEKB: Link up");
    // The link is up but pairing and enumeration are still ahead; the
    // connect worker publishes CONNECTED once the keyboard can actually type
    if (!bleKBConnectBusy) {
      bleKBHost.state = BLEKB_STATE_CONNECTED;
      bleKBHost.connectedSince = millis();
    }
    bleKBHost.lastKeyTime = millis();
  }

  void onDisconnect(NimBLEClient* pClient, int reason) override {
    Serial.printf("BLEKB: Disconnected (reason %d)\n", reason);
    bleKBHost.state = BLEKB_STATE_DISCONNECTED;
    bleKBHost.pReportChar = nullptr;

    // Schedule a reconnect scan ~2s out. Keyboards commonly drop the link
    // right after initial bonding; a fast rescan reconnects them promptly
    // instead of waiting the full reconnect interval.
    if (bleKBHost.autoReconnect && bleKBHost.pairedDevice.valid) {
      bleKBHost.lastReconnectAttempt = millis() - BLE_RECONNECT_INTERVAL + 2000;
    }
  }

  void onConnectFail(NimBLEClient* pClient, int reason) override {
    Serial.printf("BLEKB: Connect failed, reason %d\n", reason);
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
    Serial.printf("BLEKB: Auth complete - encrypted=%d bonded=%d\n",
                  connInfo.isEncrypted(), connInfo.isBonded());
  }

  // We are the display side (BLE_HS_IO_DISPLAY_ONLY): the stack asks which
  // passkey to show and the user types it on the keyboard. The settings
  // screen shows the number before the connect starts.
  uint32_t onPassKeyDisplay(NimBLEConnInfo& connInfo) override {
    Serial.printf("BLEKB: Passkey requested, displaying %d\n", BLE_KB_PASSKEY);
    return BLE_KB_PASSKEY;
  }

  // Should a peer ask US to enter a key, answer with the same fixed passkey.
  void onPassKeyEntry(NimBLEConnInfo& connInfo) override {
    Serial.printf("BLEKB: Passkey entry requested, injecting %d\n", BLE_KB_PASSKEY);
    NimBLEDevice::injectPassKey(connInfo, BLE_KB_PASSKEY);
  }

  // Numeric comparison pairing: no way to show/confirm mid-connect, accept
  void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t pin) override {
    Serial.printf("BLEKB: Confirm PIN %lu - accepting\n", (unsigned long)pin);
    NimBLEDevice::injectConfirmPasskey(connInfo, true);
  }

  // Accept the keyboard's preferred connection parameters. NimBLE's default
  // counter-proposes OUR stored params; some keyboard firmwares won't start
  // sending reports until their (battery-friendly) parameters are granted.
  bool onConnParamsUpdateRequest(NimBLEClient* pClient,
                                 const ble_gap_upd_params* params) override {
    Serial.printf("BLEKB: Peer conn params itvl %d-%d lat %d timeout %d - accepting\n",
                  params->itvl_min, params->itvl_max,
                  params->latency, params->supervision_timeout);
    pClient->setConnectionParams(params->itvl_min, params->itvl_max,
                                 params->latency, params->supervision_timeout);
    return true;
  }
};

// Scan callbacks: per-device results plus the scan-end notification. Both run
// on the NimBLE host task, so they must not touch LVGL or connect -
// updateBLEKeyboardHost() advances the state machine from the main loop.
class BLEKBScanCallbacks : public NimBLEScanCallbacks {
  void onScanEnd(const NimBLEScanResults& results, int reason) override {
    Serial.printf("BLEKB: Scan finished, %d HID device(s) found\n", bleKBHost.foundCount);
  }

  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    // Check if device advertises HID service
    bool isHid = advertisedDevice->haveServiceUUID() &&
                 advertisedDevice->isAdvertisingService(NimBLEUUID(HID_SERVICE_UUID));

    // Some keyboards omit the service UUID from the advertisement but set a
    // HID appearance (category 0x03C0-0x03FF: keyboard/mouse/etc.)
    if (!isHid && advertisedDevice->haveAppearance()) {
      uint16_t appearance = advertisedDevice->getAppearance();
      isHid = (appearance & 0xFFC0) == 0x03C0;
    }
    if (!isHid) return;

    char addr[18];
    char name[32];
    strncpy(addr, advertisedDevice->getAddress().toString().c_str(), sizeof(addr) - 1);
    addr[sizeof(addr) - 1] = '\0';
    strncpy(name, advertisedDevice->getName().c_str(), sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    if (name[0] == '\0') {
      strcpy(name, "Unknown Device");
    }

    // If this is the paired keyboard, flag it so the main loop can reconnect
    // (connecting from the scan callback context is not allowed). Match by
    // identity address or by name - keyboards advertise with a rotating
    // private address, so the address alone stops matching within minutes.
    if (bleKBHost.pairedDevice.valid &&
        (strcasecmp(addr, bleKBHost.pairedDevice.address) == 0 ||
         (bleKBHost.pairedDevice.name[0] != '\0' &&
          strcmp(name, bleKBHost.pairedDevice.name) == 0))) {
      // Record what's actually on the air right now - connecting to the
      // advertised address always works; the stored identity may not
      strncpy(bleKBHost.reconnectAddr, addr, sizeof(bleKBHost.reconnectAddr) - 1);
      bleKBHost.reconnectAddr[sizeof(bleKBHost.reconnectAddr) - 1] = '\0';
      bleKBHost.reconnectAddrType = advertisedDevice->getAddress().getType();
      bleKBHost.reconnectDeviceSeen = true;
    }

    if (bleKBHost.foundCount < BLE_MAX_FOUND_DEVICES) {
      // Check for duplicates
      for (int i = 0; i < bleKBHost.foundCount; i++) {
        if (strcasecmp(bleKBHost.foundDevices[i].address, addr) == 0) {
          return;  // Already found this device
        }
      }

      ScanResult* r = &bleKBHost.foundDevices[bleKBHost.foundCount];
      strcpy(r->name, name);
      strcpy(r->address, addr);
      r->addrType = advertisedDevice->getAddress().getType();
      r->rssi = advertisedDevice->getRSSI();
      bleKBHost.foundCount++;

      Serial.printf("BLEKB: Found HID device: %s [%s] RSSI: %d\n", name, addr, r->rssi);
    }
  }
};

// Static callback instances (a new'd instance per scan/connect would leak)
static BLEKBClientCallbacks bleKBClientCallbacks;
static BLEKBScanCallbacks bleKBScanCallbacks;

static void bleKBSetLastError(const char* msg) {
  strncpy(bleKBHost.lastError, msg, sizeof(bleKBHost.lastError) - 1);
  bleKBHost.lastError[sizeof(bleKBHost.lastError) - 1] = '\0';
}

bool isBLEKeyboardConnectBusy() {
  return bleKBConnectBusy;
}

// HID Report notification callback
void hidReportNotifyCallback(NimBLERemoteCharacteristic* pChar,
                              uint8_t* pData, size_t length, bool isNotify) {
  // Raw report trace - proves whether the keyboard is sending anything at all
  Serial.printf("BLEKB: Report h=%d len=%d:", pChar->getHandle(), (int)length);
  for (size_t i = 0; i < length && i < 10; i++) {
    Serial.printf(" %02X", pData[i]);
  }
  Serial.println();

  // HID Boot Protocol keyboard report is 8 bytes:
  // [0] = Modifier keys (Ctrl, Shift, Alt, GUI)
  // [1] = Reserved (always 0)
  // [2-7] = Up to 6 key codes
  //
  // Anything shorter than modifier + reserved + one key is a media/mouse/
  // vendor report from a combo device: ignore it. Longer reports are parsed
  // as keyboard reports, capped at 6 keys.
  if (pData == nullptr || length < 3) {
    return;
  }

  uint8_t modifiers = pData[0];
  // pData[1] is reserved

  // Process each key code (bytes 2-7)
  int maxKeys = min((int)(length - 2), 6);
  for (int i = 0; i < maxKeys; i++) {
    uint8_t keyCode = pData[i + 2];

    if (keyCode == 0) continue;  // No key

    // Check if this is a new key (not in previous report)
    bool isNewKey = true;
    for (int j = 0; j < 6; j++) {
      if (keyCode == bleKBHost.prevKeys[j]) {
        isNewKey = false;
        break;
      }
    }

    if (isNewKey) {
      // Convert HID key code to ASCII/CardKB format
      char ascii = hidKeyCodeToChar(keyCode, modifiers);
      if (ascii != 0) {
        // Add to circular buffer
        uint8_t nextHead = (bleKBHost.keyHead + 1) % BLE_KEY_BUFFER_SIZE;
        if (nextHead != bleKBHost.keyTail) {
          bleKBHost.keyBuffer[bleKBHost.keyHead] = ascii;
          bleKBHost.keyHead = nextHead;
          bleKBHost.lastKeyTime = millis();

          Serial.print("BLEKB: Key 0x");
          Serial.print(keyCode, HEX);
          Serial.print(" -> '");
          if (ascii >= 32 && ascii < 127) {
            Serial.print(ascii);
          } else {
            Serial.print("0x");
            Serial.print((uint8_t)ascii, HEX);
          }
          Serial.println("'");
        }
      }
    }
  }

  // Save current keys for next comparison
  memset(bleKBHost.prevKeys, 0, sizeof(bleKBHost.prevKeys));
  for (int i = 0; i < maxKeys && i < 6; i++) {
    bleKBHost.prevKeys[i] = pData[i + 2];
  }
}

// Initialize BLE keyboard host system
void initBLEKeyboardHost() {
  if (bleKBHost.state != BLEKB_STATE_IDLE) {
    Serial.println("BLEKB: Already initialized");
    return;
  }

  Serial.println("BLEKB: Initializing BLE Keyboard Host...");

  // NimBLE allocates its controller/host memory from internal RAM; starting
  // it on a starved heap fails deep inside the stack instead of cleanly here
  size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  if (freeInternal < BLE_MIN_FREE_INTERNAL_HEAP) {
    Serial.printf("BLEKB: Not enough internal heap to start BLE (%u free)\n",
                  (unsigned)freeInternal);
    bleKBSetLastError("Low memory, BLE off");
    return;
  }
  bleKBBootPhase = BLEKB_PHASE_INIT;

  // Initialize context
  bleKBHost.pClient = nullptr;
  bleKBHost.pReportChar = nullptr;
  bleKBHost.keyHead = 0;
  bleKBHost.keyTail = 0;
  bleKBHost.foundCount = 0;
  bleKBHost.selectedDevice = 0;
  // First auto-reconnect scan fires ~5s after init instead of waiting the
  // full interval, so a keyboard that's already on reconnects shortly after boot
  bleKBHost.lastReconnectAttempt = millis() - BLE_RECONNECT_INTERVAL + 5000;
  bleKBHost.reconnectDeviceSeen = false;
  bleKBHost.reconnectAddr[0] = '\0';
  bleKBHost.reconnectAddrType = 0;
  bleKBHost.lastKeyTime = 0;
  bleKBHost.connectedSince = 0;
  bleKBHost.lastError[0] = '\0';
  memset(bleKBHost.prevKeys, 0, sizeof(bleKBHost.prevKeys));

  // Initialize NimBLE in Central mode
  NimBLEDevice::init("VAIL-SUMMIT-KB");
  if (!NimBLEDevice::isInitialized()) {
    Serial.println("BLEKB: NimBLE init failed");
    bleKBSetLastError("BLE init failed");
    bleKBBootPhase = BLEKB_PHASE_NONE;
    return;
  }

  // Set power level for better range (+9 dBm)
  NimBLEDevice::setPower(9);

  // HID keyboards require bonding + link encryption before they will send
  // input reports. Just Works pairing (bond, no MITM, secure connections)
  // works with most keyboards; DISPLAY_ONLY additionally enables passkey
  // entry for keyboards that demand a PIN (user types BLE_KB_PASSKEY on the
  // keyboard - Just Works still applies when neither side requires MITM).
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

  bleKBHost.state = BLEKB_STATE_READY;
  bleKBBootPhase = BLEKB_PHASE_ACTIVE;
  Serial.printf("BLEKB: Initialized successfully (%u internal heap free)\n",
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

// Deinitialize BLE keyboard host
void deinitBLEKeyboardHost() {
  if (bleKBHost.state == BLEKB_STATE_IDLE) {
    return;
  }

  Serial.println("BLEKB: Deinitializing...");

  // A connect/pair sequence still running on the worker task would be left
  // holding a dangling client after NimBLEDevice::deinit. Drop the link so
  // the worker unblocks and give it a moment to finish.
  if (bleKBConnectBusy) {
    Serial.println("BLEKB: Connect in progress, aborting it first");
    if (bleKBHost.pClient != nullptr && bleKBHost.pClient->isConnected()) {
      bleKBHost.pClient->disconnect();
    }
    for (int i = 0; i < 300 && bleKBConnectBusy; i++) {
      delay(10);
    }
    if (bleKBConnectBusy) {
      Serial.println("BLEKB: Connect worker still busy, deinit postponed");
      return;
    }
  }

  // Disconnect if connected
  if (bleKBHost.pClient != nullptr && bleKBHost.pClient->isConnected()) {
    bleKBHost.pClient->disconnect();
    delay(100);
  }

  // Stop any ongoing scan
  NimBLEDevice::getScan()->stop();

  // Clean up client
  if (bleKBHost.pClient != nullptr) {
    NimBLEDevice::deleteClient(bleKBHost.pClient);
    bleKBHost.pClient = nullptr;
  }

  // Deinit NimBLE
  NimBLEDevice::deinit(true);
  delay(100);

  bleKBHost.state = BLEKB_STATE_IDLE;
  bleKBHost.pReportChar = nullptr;
  bleKBHost.reconnectDeviceSeen = false;
  bleKBBootPhase = BLEKB_PHASE_NONE;

  Serial.println("BLEKB: Deinitialized");
}

// Start scanning for BLE keyboards
void startBLEKeyboardScan() {
  if (bleKBHost.state == BLEKB_STATE_IDLE) {
    initBLEKeyboardHost();
  }

  if (bleKBHost.state == BLEKB_STATE_IDLE) {
    return;  // init refused (low memory) - nothing to scan with
  }

  if (bleKBHost.state == BLEKB_STATE_SCANNING) {
    Serial.println("BLEKB: Already scanning");
    return;
  }

  // NimBLE cannot scan and connect at the same time; the worker owns the
  // radio until the connect sequence finishes
  if (bleKBConnectBusy) {
    Serial.println("BLEKB: Connect in progress, scan skipped");
    return;
  }

  // Disconnect if connected
  if (bleKBHost.pClient != nullptr && bleKBHost.pClient->isConnected()) {
    bleKBHost.pClient->disconnect();
  }

  Serial.println("BLEKB: Starting scan for HID keyboards...");

  // Clear previous results
  bleKBHost.foundCount = 0;
  bleKBHost.selectedDevice = 0;

  // Configure and start scan
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(&bleKBScanCallbacks, false);
  pScan->setActiveScan(true);  // Active scan for device names
  pScan->setInterval(100);
  pScan->setWindow(99);

  bleKBHost.state = BLEKB_STATE_SCANNING;
  bleKBBootPhase = BLEKB_PHASE_SCANNING;
  // start() is non-blocking (the blocking form is getResults()); duration is
  // in milliseconds and scan end arrives via onScanEnd()
  if (!pScan->start(BLE_SCAN_DURATION_SEC * 1000, false, true)) {
    Serial.println("BLEKB: Scan failed to start");
    bleKBHost.state = BLEKB_STATE_ERROR;
    bleKBSetLastError("Scan failed to start");
    bleKBBootPhase = BLEKB_PHASE_ACTIVE;
  }
}

// Stop scanning
void stopBLEKeyboardScan() {
  if (bleKBHost.state == BLEKB_STATE_SCANNING) {
    NimBLEDevice::getScan()->stop();
    bleKBHost.state = BLEKB_STATE_SCAN_COMPLETE;
    bleKBBootPhase = BLEKB_PHASE_ACTIVE;
    Serial.println("BLEKB: Scan stopped");
  }
}

// Connect to a keyboard from scan results
bool connectToBLEKeyboard(int deviceIndex) {
  if (deviceIndex < 0 || deviceIndex >= bleKBHost.foundCount) {
    Serial.println("BLEKB: Invalid device index");
    return false;
  }

  return connectToBLEKeyboardByAddress(
    bleKBHost.foundDevices[deviceIndex].address,
    bleKBHost.foundDevices[deviceIndex].addrType,
    true /* user-initiated fresh pairing */);
}

// The actual connect/pair/enumerate sequence. Runs on the worker task; must
// not touch LVGL. Everything blocking in here is bounded by NimBLE's own
// timeouts (10s connect, 30s SM) and by the main loop's deadline disconnect.
static bool bleKBRunConnect(const BLEKBConnectRequest& req) {
  const uint32_t epochAtStart = bleKBPairEpoch;
  Serial.printf("BLEKB: Connecting to %s (%s)%s\n", req.address, req.name,
                req.freshPair ? " [fresh pairing]" : "");

  // Create client if needed
  if (bleKBHost.pClient == nullptr) {
    bleKBHost.pClient = NimBLEDevice::createClient();
    if (bleKBHost.pClient == nullptr) {
      bleKBSetLastError("No memory for BLE client");
      return false;
    }
    bleKBHost.pClient->setClientCallbacks(&bleKBClientCallbacks, false);
    // Bounds the connect() wait; a keyboard that stops advertising mid-connect
    // otherwise holds the worker for NimBLE's 30s default (milliseconds)
    bleKBHost.pClient->setConnectTimeout(10000);
  }

  // Standard host-like connection parameters (30-60ms interval, 5s timeout).
  // The previous aggressive 15ms/2s params are exactly what a battery
  // keyboard fights against; its own update request is accepted in
  // onConnParamsUpdateRequest anyway.
  bleKBHost.pClient->setConnectionParams(24, 48, 0, 500);

  // Up to two full attempts: on a first-time pairing the notification
  // subscription can land before link encryption is fully established, in
  // which case the keyboard silently never sends reports even though every
  // call returned success. If the link isn't encrypted after attempt 1,
  // drop the connection and redo it - by then the bond exists, so attempt 2
  // encrypts immediately (this replaces the manual disconnect/reconnect that
  // used to be needed after the first pairing).
  NimBLEAddress bleAddr(std::string(req.address), req.addrType);

  // Fresh pairing: the keyboard is in pairing mode and expects a full SMP
  // pairing exchange. A bond left on our side makes NimBLE silently RESUME
  // the old encrypted session instead of pairing - the link encrypts and
  // subscribes fine, but the keyboard's pairing slot never completes, so it
  // keeps blinking and never sends a report. Delete our bonds for both the
  // target address and the previously saved identity so a real pairing runs.
  if (req.freshPair) {
    NimBLEDevice::deleteBond(bleAddr);  // no-op if absent
    if (bleKBHost.pairedDevice.valid && bleKBHost.pairedDevice.address[0] != '\0') {
      NimBLEAddress oldAddr(std::string(bleKBHost.pairedDevice.address),
                            bleKBHost.pairedDevice.addrType);
      NimBLEDevice::deleteBond(oldAddr);
    }
    Serial.println("BLEKB: Cleared stored bonds for fresh pairing");
  }

  int subscribedCount = 0;
  for (int attempt = 0; attempt < 2; attempt++) {
    subscribedCount = 0;
    bleKBHost.pReportChar = nullptr;
    // onDisconnect (from attempt 0's intentional drop, or a keyboard that
    // bounced) may have moved the state; we are still connecting
    bleKBHost.state = BLEKB_STATE_CONNECTING;

    if (bleKBConnectAborted) {
      bleKBSetLastError("Connect timed out");
      return false;
    }

    // Connect to the device (address type matters: keyboards almost always use
    // random-static addresses, and a connect with the wrong type never succeeds)
    if (!bleKBHost.pClient->connect(bleAddr)) {
      Serial.println("BLEKB: Connection failed");
      bleKBSetLastError("Connection failed");
      return false;
    }

    Serial.println("BLEKB: Connected, requesting encryption...");

    // Pair/bond and encrypt the link. Most keyboards refuse to send input
    // reports (or reject the subscribe) on an unencrypted link. This is the
    // call that can wait up to 30s for a keyboard that never answers.
    if (!bleKBHost.pClient->secureConnection()) {
      Serial.println("BLEKB: Pairing failed (continuing, some keyboards allow it)");
    }
    if (!bleKBHost.pClient->isConnected()) {
      // Keyboard dropped the link during pairing (wrong PIN, pairing mode
      // timed out, or the main loop's deadline disconnect)
      Serial.println("BLEKB: Link dropped during pairing");
      bleKBSetLastError(bleKBConnectAborted ? "Connect timed out" : "Pairing rejected");
      return false;
    }

    Serial.println("BLEKB: Discovering services...");

    // Get HID service
    NimBLERemoteService* pService = bleKBHost.pClient->getService(NimBLEUUID(HID_SERVICE_UUID));
    if (pService == nullptr) {
      Serial.println("BLEKB: HID service not found");
      bleKBHost.pClient->disconnect();
      bleKBSetLastError("HID service not found");
      return false;
    }

    // Full HOGP host enumeration, mirroring what a real OS host does before
    // enabling input: read PnP ID, HID Information and the Report Map, then
    // select Report Protocol. Some keyboards treat the Report Map read as
    // "enumeration complete" and never send a single report (staying in
    // pairing mode) when the host skips it.
    NimBLERemoteService* pDis = bleKBHost.pClient->getService(NimBLEUUID(DIS_SERVICE_UUID));
    if (pDis != nullptr) {
      NimBLERemoteCharacteristic* pPnp = pDis->getCharacteristic(NimBLEUUID(DIS_PNP_ID_UUID));
      if (pPnp != nullptr && pPnp->canRead()) {
        NimBLEAttValue v = pPnp->readValue();
        Serial.printf("BLEKB: PnP ID read (%d bytes)\n", (int)v.size());
      }
    }

    NimBLERemoteCharacteristic* pEnum = pService->getCharacteristic(NimBLEUUID(HID_INFO_UUID));
    if (pEnum != nullptr && pEnum->canRead()) {
      NimBLEAttValue v = pEnum->readValue();
      Serial.printf("BLEKB: HID info read (%d bytes)\n", (int)v.size());
    }

    pEnum = pService->getCharacteristic(NimBLEUUID(HID_REPORT_MAP_UUID));
    if (pEnum != nullptr && pEnum->canRead()) {
      NimBLEAttValue v = pEnum->readValue();
      Serial.printf("BLEKB: Report map read (%d bytes)\n", (int)v.size());
    }

    pEnum = pService->getCharacteristic(NimBLEUUID(HID_PROTO_MODE_UUID));
    if (pEnum != nullptr && pEnum->canWriteNoResponse()) {
      uint8_t reportMode = 0x01;
      pEnum->writeValue(&reportMode, 1, false);
      Serial.println("BLEKB: Protocol mode set to Report");
    }

    // Subscribe to every notifiable input report. Combo devices expose several
    // (keyboard, media keys, mouse) and only one carries key presses - the
    // notify callback parses keyboard-format reports and ignores the rest.
    // Fall back to the Boot Keyboard Input Report (2A22) if no 2A4D notifies.
    const std::vector<NimBLERemoteCharacteristic*>& chars = pService->getCharacteristics(true);

    for (auto pChar : chars) {
      if (!bleKBHost.pClient->isConnected()) break;
      bool isInputReport = (pChar->getUUID() == NimBLEUUID(HID_REPORT_CHAR_UUID) ||
                            pChar->getUUID() == NimBLEUUID(HID_BOOT_INPUT_UUID)) &&
                           pChar->canNotify();
      if (!isInputReport) continue;

      // Read the Report Reference descriptor (report ID + type). Hosts read
      // these during enumeration; also logs which report ID is which handle.
      NimBLERemoteDescriptor* pDesc = pChar->getDescriptor(NimBLEUUID((uint16_t)0x2908));
      if (pDesc != nullptr) {
        NimBLEAttValue rr = pDesc->readValue();
        if (rr.size() >= 2) {
          Serial.printf("BLEKB: Report handle %d: id=%d type=%d\n",
                        pChar->getHandle(), rr[0], rr[1]);
        }
      }

      // response=true: enable notifications with a confirmed Write Request.
      // The default (write command, no response) is silently ignored by some
      // keyboard stacks AND reports success either way - the classic result
      // is "connected but never sends a single report".
      if (pChar->subscribe(true, hidReportNotifyCallback, true)) {
        if (bleKBHost.pReportChar == nullptr) {
          bleKBHost.pReportChar = pChar;
        }
        subscribedCount++;
        Serial.printf("BLEKB: Subscribed to report char %s (handle %d)\n",
                      pChar->getUUID().toString().c_str(), pChar->getHandle());
      } else {
        Serial.printf("BLEKB: Subscribe failed for handle %d\n", pChar->getHandle());
      }
    }

    if (subscribedCount == 0) {
      Serial.println("BLEKB: No input report characteristic could be subscribed");
      bleKBHost.pClient->disconnect();
      bleKBSetLastError("Report char not found");
      return false;
    }

    // Encrypted link with subscriptions in place: reports will flow
    if (bleKBHost.pClient->getConnInfo().isEncrypted()) {
      break;
    }

    if (attempt == 0) {
      Serial.println("BLEKB: Link not encrypted after pairing, redoing connection...");
      bleKBHost.pClient->disconnect();
      // A half-established bond from the failed pairing would poison the
      // retry the same way a stale bond poisons a fresh pairing
      if (NimBLEDevice::isBonded(bleAddr)) {
        NimBLEDevice::deleteBond(bleAddr);
      }
      delay(300);  // let the disconnect complete before reconnecting
    } else {
      Serial.println("BLEKB: Link still unencrypted, refusing it");
      bleKBHost.pClient->disconnect();
      bleKBSetLastError("Pairing failed (not encrypted)");
      return false;
    }
  }

  if (!bleKBHost.pClient->isConnected()) {
    bleKBSetLastError("Link dropped");
    return false;
  }

  Serial.println("BLEKB: Subscribed to keyboard input notifications");

  // Nudge quirky firmwares: read the first input report once - some only
  // start notifying after the host performs an initial read
  if (bleKBHost.pReportChar != nullptr && bleKBHost.pReportChar->canRead()) {
    NimBLEAttValue initial = bleKBHost.pReportChar->readValue();
    Serial.printf("BLEKB: Initial report read (%d bytes)\n", (int)initial.size());
  }

  // Save pairing info using the peer's IDENTITY address: the advertised
  // address is a rotating private address that goes stale within minutes,
  // which made reconnect-by-saved-address impossible.
  NimBLEConnInfo connInfo = bleKBHost.pClient->getConnInfo();
  NimBLEAddress idAddr = connInfo.getIdAddress();
  Serial.printf("BLEKB: OTA addr %s / identity addr %s (type %d), MTU %d, itvl %d\n",
                connInfo.getAddress().toString().c_str(),
                idAddr.toString().c_str(), idAddr.getType(),
                connInfo.getMTU(), connInfo.getConnInterval());
  strncpy(bleKBHost.pairedDevice.address, idAddr.toString().c_str(),
          sizeof(bleKBHost.pairedDevice.address) - 1);
  bleKBHost.pairedDevice.address[sizeof(bleKBHost.pairedDevice.address) - 1] = '\0';
  bleKBHost.pairedDevice.addrType = idAddr.getType();

  // Device name: the advertised name captured at launch, else the address
  if (req.name[0] != '\0') {
    strncpy(bleKBHost.pairedDevice.name, req.name, sizeof(bleKBHost.pairedDevice.name) - 1);
  } else {
    strncpy(bleKBHost.pairedDevice.name, req.address, sizeof(bleKBHost.pairedDevice.name) - 1);
  }
  bleKBHost.pairedDevice.name[sizeof(bleKBHost.pairedDevice.name) - 1] = '\0';

  if (bleKBPairEpoch != epochAtStart) {
    // Forgotten while we were connecting: drop the link and the new bond
    Serial.println("BLEKB: Pairing was forgotten during connect, discarding");
    bleKBHost.pClient->disconnect();
    NimBLEDevice::deleteBond(idAddr);
    memset(&bleKBHost.pairedDevice, 0, sizeof(bleKBHost.pairedDevice));
    bleKBSetLastError("Pairing forgotten");
    return false;
  }
  bleKBHost.pairedDevice.valid = true;

  // Save to preferences
  saveBLEKeyboardSettings();

  Serial.printf("BLEKB: Successfully connected to %s\n", bleKBHost.pairedDevice.name);
  return true;
}

// Worker task entry: run the sequence, publish the outcome, exit
static void bleKBConnectTask(void* arg) {
  bool ok = bleKBRunConnect(bleKBConnectReq);
  if (ok && (bleKBHost.pClient == nullptr || !bleKBHost.pClient->isConnected())) {
    bleKBSetLastError("Link dropped");
    ok = false;
  }
  if (ok) {
    bleKBHost.state = BLEKB_STATE_CONNECTED;
    bleKBHost.lastKeyTime = millis();
    bleKBHost.connectedSince = millis();
  } else {
    // ERROR rather than whatever onDisconnect left behind, so the settings
    // screen shows lastError; auto-reconnect scans resume from ERROR too
    bleKBHost.state = BLEKB_STATE_ERROR;
  }
  bleKBBootPhase = BLEKB_PHASE_ACTIVE;
  Serial.printf("BLEKB: Connect worker done: %s%s%s\n", ok ? "success" : "failed",
                ok ? "" : " - ", ok ? "" : bleKBHost.lastError);
  bleKBConnectBusy = false;
  vTaskDelete(NULL);
}

// Connect to a keyboard by address (non-blocking: launches the worker task)
bool connectToBLEKeyboardByAddress(const char* address, uint8_t addrType, bool freshPair) {
  if (bleKBHost.state == BLEKB_STATE_IDLE) {
    initBLEKeyboardHost();
    if (bleKBHost.state == BLEKB_STATE_IDLE) {
      return false;  // init refused (low memory)
    }
  }

  if (bleKBConnectBusy) {
    Serial.println("BLEKB: Connect already in progress");
    return false;
  }

  if (address == nullptr || address[0] == '\0') {
    bleKBSetLastError("No address");
    return false;
  }

  // NimBLE cannot initiate a connection while a scan is running
  if (NimBLEDevice::getScan()->isScanning()) {
    NimBLEDevice::getScan()->stop();
  }

  memset(&bleKBConnectReq, 0, sizeof(bleKBConnectReq));
  strncpy(bleKBConnectReq.address, address, sizeof(bleKBConnectReq.address) - 1);
  bleKBConnectReq.addrType = addrType;
  bleKBConnectReq.freshPair = freshPair;
  for (int i = 0; i < bleKBHost.foundCount; i++) {
    if (strcasecmp(bleKBHost.foundDevices[i].address, address) == 0) {
      strncpy(bleKBConnectReq.name, bleKBHost.foundDevices[i].name,
              sizeof(bleKBConnectReq.name) - 1);
      break;
    }
  }
  if (bleKBConnectReq.name[0] == '\0' && bleKBHost.pairedDevice.valid) {
    // Reconnect to the paired keyboard: keep the name we already know
    strncpy(bleKBConnectReq.name, bleKBHost.pairedDevice.name,
            sizeof(bleKBConnectReq.name) - 1);
  }

  bleKBHost.state = BLEKB_STATE_CONNECTING;
  bleKBHost.lastError[0] = '\0';
  bleKBConnectStart = millis();
  bleKBConnectAborted = false;
  bleKBConnectBusy = true;
  bleKBBootPhase = BLEKB_PHASE_CONNECTING;

  // Same core as loop() so the NimBLE host task (core 0) and audio keep
  // their core to themselves; priority matches loop()
  TaskHandle_t handle = NULL;
  BaseType_t rc = xTaskCreatePinnedToCore(bleKBConnectTask, "blekb_conn",
                                          BLE_CONNECT_TASK_STACK, NULL, 1, &handle, 1);
  if (rc != pdPASS) {
    Serial.println("BLEKB: Could not start connect worker (no memory)");
    bleKBConnectBusy = false;
    bleKBHost.state = BLEKB_STATE_ERROR;
    bleKBSetLastError("No memory for connect");
    bleKBBootPhase = BLEKB_PHASE_ACTIVE;
    return false;
  }
  return true;
}

// Disconnect from keyboard
void disconnectBLEKeyboard() {
  if (bleKBHost.pClient != nullptr && bleKBHost.pClient->isConnected()) {
    Serial.println("BLEKB: Disconnecting...");
    bleKBHost.pClient->disconnect();
  }
  if (!bleKBConnectBusy) {
    bleKBHost.state = BLEKB_STATE_READY;
  }
  bleKBHost.pReportChar = nullptr;
}

// Update function (call from main loop)
void updateBLEKeyboardHost() {
  // Only run if BLE keyboard host is active
  if (bleKBHost.state == BLEKB_STATE_IDLE) {
    return;
  }

  unsigned long now = millis();

  // Connect worker supervision. The worker is bounded by NimBLE's own
  // timeouts, but a keyboard that keeps the link up without ever finishing
  // pairing could hold it for a long time: past the deadline, drop the link
  // so every blocking call inside the worker returns.
  if (bleKBConnectBusy) {
    if (!bleKBConnectAborted && now - bleKBConnectStart > BLE_CONNECT_DEADLINE_MS) {
      Serial.println("BLEKB: Connect deadline passed, dropping link");
      bleKBConnectAborted = true;
      if (bleKBHost.pClient != nullptr && bleKBHost.pClient->isConnected()) {
        bleKBHost.pClient->disconnect();
      }
    }
    return;  // the worker owns the radio; no scans or reconnects meanwhile
  }

  // Detect scan completion (NimBLE stops on its own after BLE_SCAN_DURATION_SEC,
  // but nothing else advances our state machine out of SCANNING)
  if (bleKBHost.state == BLEKB_STATE_SCANNING && !NimBLEDevice::getScan()->isScanning()) {
    bleKBHost.state = BLEKB_STATE_SCAN_COMPLETE;
    bleKBBootPhase = BLEKB_PHASE_ACTIVE;
    Serial.printf("BLEKB: Scan complete, %d device(s) found\n", bleKBHost.foundCount);
  }

  // A link that has stayed up for a while proves this pairing is healthy:
  // clear the boot crash counter so an unrelated crash later can't trip it
  if (bleKBHost.state == BLEKB_STATE_CONNECTED && bleKBHost.crashCount > 0 &&
      now - bleKBHost.connectedSince > BLE_CRASH_CLEAR_UPTIME_MS) {
    bleKBClearCrashCount();
  }

  // The paired keyboard was spotted advertising: connect now. The device is
  // in range, so the connect resolves quickly instead of eating the full
  // 10s timeout.
  if (bleKBHost.reconnectDeviceSeen) {
    bleKBHost.reconnectDeviceSeen = false;
    if (bleKBHost.autoReconnect && bleKBHost.pairedDevice.valid &&
        bleKBHost.state != BLEKB_STATE_CONNECTED &&
        bleKBHost.state != BLEKB_STATE_CONNECTING) {
      Serial.println("BLEKB: Paired keyboard in range, reconnecting...");
      // Connect to the address it is advertising right now; the stored
      // identity address may differ from the rotating on-air address
      if (bleKBHost.reconnectAddr[0] != '\0') {
        connectToBLEKeyboardByAddress(bleKBHost.reconnectAddr,
                                      bleKBHost.reconnectAddrType);
      } else {
        connectToBLEKeyboardByAddress(bleKBHost.pairedDevice.address,
                                      bleKBHost.pairedDevice.addrType);
      }
      return;
    }
  }

  // Handle auto-reconnect by scanning for the paired keyboard rather than
  // blind-connecting: a connect attempt while the keyboard is off/asleep
  // would eat the full connect timeout, whereas a scan runs in the
  // background and only triggers a connect once the keyboard is seen.
  if (bleKBHost.state == BLEKB_STATE_DISCONNECTED ||
      bleKBHost.state == BLEKB_STATE_READY ||
      bleKBHost.state == BLEKB_STATE_SCAN_COMPLETE ||
      bleKBHost.state == BLEKB_STATE_ERROR) {
    if (bleKBHost.autoReconnect && bleKBHost.pairedDevice.valid) {
      if (now - bleKBHost.lastReconnectAttempt >= BLE_RECONNECT_INTERVAL) {
        bleKBHost.lastReconnectAttempt = now;
        Serial.println("BLEKB: Scanning for paired keyboard...");
        startBLEKeyboardScan();
      }
    }
  }
}

// Check if BLE keyboard is connected
bool isBLEKeyboardConnected() {
  return bleKBHost.state == BLEKB_STATE_CONNECTED &&
         bleKBHost.pClient != nullptr &&
         bleKBHost.pClient->isConnected();
}

// Check if there's keyboard input waiting
bool hasBLEKeyboardInput() {
  return bleKBHost.keyHead != bleKBHost.keyTail;
}

// Get next key from buffer (returns 0 if empty)
char getBLEKeyboardKey() {
  if (bleKBHost.keyHead == bleKBHost.keyTail) {
    return 0;  // Buffer empty
  }

  char key = bleKBHost.keyBuffer[bleKBHost.keyTail];
  bleKBHost.keyTail = (bleKBHost.keyTail + 1) % BLE_KEY_BUFFER_SIZE;
  return key;
}

// Save settings to Preferences
void saveBLEKeyboardSettings() {
  Preferences prefs;
  prefs.begin("btkeyboard", false);

  prefs.putBool("valid", bleKBHost.pairedDevice.valid);
  if (bleKBHost.pairedDevice.valid) {
    prefs.putString("name", bleKBHost.pairedDevice.name);
    prefs.putString("addr", bleKBHost.pairedDevice.address);
    prefs.putUChar("addrType", bleKBHost.pairedDevice.addrType);
  }
  prefs.putBool("autoRecon", bleKBHost.autoReconnect);

  prefs.end();
  Serial.println("BLEKB: Settings saved");
}

// Reset the boot crash counter (healthy connection, user re-enabled
// auto-reconnect, or pairing forgotten)
void bleKBClearCrashCount() {
  if (bleKBHost.crashCount == 0 && !bleKBHost.autoReconnectTripped) return;
  bleKBHost.crashCount = 0;
  bleKBHost.autoReconnectTripped = false;
  Preferences prefs;
  prefs.begin("btkeyboard", false);
  prefs.putUChar("crashCnt", 0);
  prefs.end();
  Serial.println("BLEKB: Crash counter cleared");
}

// Boot crash guard. The keyboard host records what it is doing in RTC memory
// (bleKBBootPhase survives panic and watchdog resets). If the previous boot
// ended abnormally while the host was active, count it; after
// BLE_CRASH_TRIP_COUNT consecutive such resets, switch auto-reconnect off and
// persist that, so the device comes up to a usable menu instead of crashing
// on every boot until the owner reflashes it. Runs once, from
// loadBLEKeyboardSettings() (before the host is started at boot).
static void bleKBCheckPreviousBoot() {
  static bool checked = false;
  if (checked) return;
  checked = true;

  esp_reset_reason_t reason = esp_reset_reason();
  bool abnormal = (reason == ESP_RST_PANIC || reason == ESP_RST_TASK_WDT ||
                   reason == ESP_RST_INT_WDT || reason == ESP_RST_WDT);
  uint8_t phase = (bleKBBootMagic == BLEKB_BOOT_MAGIC) ? bleKBBootPhase : BLEKB_PHASE_NONE;

  // Arm the marker for this boot
  bleKBBootMagic = BLEKB_BOOT_MAGIC;
  bleKBBootPhase = BLEKB_PHASE_NONE;

  if (!abnormal) {
    // Clean boot breaks the run, so only back-to-back crashes count
    Preferences prefs;
    prefs.begin("btkeyboard", true);
    uint8_t stale = prefs.getUChar("crashCnt", 0);
    prefs.end();
    if (stale != 0) {
      prefs.begin("btkeyboard", false);
      prefs.putUChar("crashCnt", 0);
      prefs.end();
    }
    return;
  }
  if (phase == BLEKB_PHASE_NONE) {
    Serial.println("BLEKB: Previous reset was abnormal but the keyboard host was off");
    return;
  }

  Preferences prefs;
  prefs.begin("btkeyboard", false);
  uint8_t count = prefs.getUChar("crashCnt", 0);
  if (count < 255) count++;
  prefs.putUChar("crashCnt", count);
  prefs.putUChar("crashPhase", phase);
  bleKBHost.crashCount = count;
  bleKBHost.lastCrashPhase = phase;
  Serial.printf("BLEKB: Previous boot crashed while %s (%d in a row)\n",
                getBLEKBPhaseString(phase), count);

  if (count >= BLE_CRASH_TRIP_COUNT && bleKBHost.autoReconnect) {
    bleKBHost.autoReconnect = false;
    bleKBHost.autoReconnectTripped = true;
    prefs.putBool("autoRecon", false);
    Serial.println("BLEKB: Auto-reconnect switched OFF after repeated crashes; "
                   "re-enable it in External Keyboard settings");
  }
  prefs.end();
}

// Load settings from Preferences
void loadBLEKeyboardSettings() {
  Preferences prefs;
  prefs.begin("btkeyboard", true);

  bleKBHost.pairedDevice.valid = prefs.getBool("valid", false);
  if (bleKBHost.pairedDevice.valid) {
    char name[32] = {0};
    char addr[18] = {0};
    prefs.getString("name", name, sizeof(name));
    prefs.getString("addr", addr, sizeof(addr));
    strncpy(bleKBHost.pairedDevice.name, name, sizeof(bleKBHost.pairedDevice.name) - 1);
    bleKBHost.pairedDevice.name[sizeof(bleKBHost.pairedDevice.name) - 1] = '\0';
    strncpy(bleKBHost.pairedDevice.address, addr, sizeof(bleKBHost.pairedDevice.address) - 1);
    bleKBHost.pairedDevice.address[sizeof(bleKBHost.pairedDevice.address) - 1] = '\0';
    // Default to random-static: what nearly all BLE keyboards use, and what
    // pairings saved before the type was persisted most likely need
    bleKBHost.pairedDevice.addrType = prefs.getUChar("addrType", BLE_ADDR_RANDOM);
    // A saved pairing with no address is unusable; drop it rather than
    // letting the reconnect path chew on an empty string every 30s
    if (bleKBHost.pairedDevice.address[0] == '\0') {
      Serial.println("BLEKB: Saved pairing has no address, ignoring it");
      bleKBHost.pairedDevice.valid = false;
    }
  }
  bleKBHost.autoReconnect = prefs.getBool("autoRecon", true);
  bleKBHost.crashCount = prefs.getUChar("crashCnt", 0);
  bleKBHost.lastCrashPhase = prefs.getUChar("crashPhase", BLEKB_PHASE_NONE);
  bleKBHost.autoReconnectTripped = false;

  prefs.end();

  Serial.print("BLEKB: Settings loaded, paired device: ");
  if (bleKBHost.pairedDevice.valid) {
    Serial.println(bleKBHost.pairedDevice.name);
  } else {
    Serial.println("None");
  }

  bleKBCheckPreviousBoot();
}

// Forget paired keyboard
void forgetBLEKeyboardPairing() {
  bleKBPairEpoch++;  // invalidates any connect worker still running
  // Disconnect if connected
  if (bleKBHost.pClient != nullptr && bleKBHost.pClient->isConnected()) {
    bleKBHost.pClient->disconnect();
  }

  // Delete the NimBLE bond too - keeping it makes the next "pairing" to the
  // same keyboard silently resume the old session instead of freshly pairing
  if (bleKBHost.state != BLEKB_STATE_IDLE &&
      bleKBHost.pairedDevice.valid && bleKBHost.pairedDevice.address[0] != '\0') {
    NimBLEAddress oldAddr(std::string(bleKBHost.pairedDevice.address),
                          bleKBHost.pairedDevice.addrType);
    NimBLEDevice::deleteBond(oldAddr);
    Serial.println("BLEKB: Bond deleted");
  }

  // Clear pairing info
  bleKBHost.pairedDevice.valid = false;
  memset(bleKBHost.pairedDevice.name, 0, sizeof(bleKBHost.pairedDevice.name));
  memset(bleKBHost.pairedDevice.address, 0, sizeof(bleKBHost.pairedDevice.address));
  bleKBHost.reconnectAddr[0] = '\0';
  bleKBHost.reconnectDeviceSeen = false;

  // Save to preferences
  saveBLEKeyboardSettings();
  bleKBClearCrashCount();

  if (!bleKBConnectBusy && bleKBHost.state != BLEKB_STATE_IDLE) {
    bleKBHost.state = BLEKB_STATE_READY;
  }
  Serial.println("BLEKB: Pairing forgotten");
}

// Get human-readable state string
const char* getBLEKBStateString() {
  switch (bleKBHost.state) {
    case BLEKB_STATE_IDLE:
      return "Idle";
    case BLEKB_STATE_READY:
      return "Ready";
    case BLEKB_STATE_SCANNING:
      return "Scanning...";
    case BLEKB_STATE_SCAN_COMPLETE:
      return "Scan Complete";
    case BLEKB_STATE_CONNECTING:
      return "Connecting...";
    case BLEKB_STATE_CONNECTED:
      return "Connected";
    case BLEKB_STATE_DISCONNECTED:
      return "Disconnected";
    case BLEKB_STATE_ERROR:
      return "Error";
    default:
      return "Unknown";
  }
}

// Human-readable boot phase (for crash attribution logs and the settings screen)
const char* getBLEKBPhaseString(uint8_t phase) {
  switch (phase) {
    case BLEKB_PHASE_INIT:       return "starting BLE";
    case BLEKB_PHASE_ACTIVE:     return "BLE active";
    case BLEKB_PHASE_SCANNING:   return "scanning";
    case BLEKB_PHASE_CONNECTING: return "connecting/pairing";
    default:                     return "off";
  }
}

const char* getBLEKBLastError() {
  return bleKBHost.lastError;
}

#endif // BLE_KEYBOARD_HOST_H
