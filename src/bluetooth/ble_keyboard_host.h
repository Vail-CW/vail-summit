/*
 * BLE Keyboard Host Module
 * Implements BLE Central (Client) mode to receive input from external Bluetooth keyboards
 * Uses NimBLE-Arduino library for BLE Central functionality
 */

#ifndef BLE_KEYBOARD_HOST_H
#define BLE_KEYBOARD_HOST_H

#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
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

// Key buffer size (circular buffer for incoming keys)
#define BLE_KEY_BUFFER_SIZE     16

// Scan and connection timeouts
#define BLE_SCAN_DURATION_SEC   10
#define BLE_RECONNECT_INTERVAL  30000  // 30 seconds between reconnect attempts
#define BLE_MAX_FOUND_DEVICES   10

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
  String name;
  String address;
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
  String lastError;

  // Activity tracking
  unsigned long lastKeyTime;
};

// Global context
BLEKBHostContext bleKBHost;

// Forward declarations
void initBLEKeyboardHost();
void deinitBLEKeyboardHost();
void startBLEKeyboardScan();
void stopBLEKeyboardScan();
bool connectToBLEKeyboard(int deviceIndex);
// freshPair: user-initiated pairing from the scan list. Deletes any stale
// bond first - a bond we remember but the keyboard forgot (it's back in
// pairing mode) makes encryption fail until both sides are power-cycled.
bool connectToBLEKeyboardByAddress(const char* address, uint8_t addrType, bool freshPair = false);
void disconnectBLEKeyboard();
void updateBLEKeyboardHost();
bool isBLEKeyboardConnected();
bool hasBLEKeyboardInput();
char getBLEKeyboardKey();
void saveBLEKeyboardSettings();
void loadBLEKeyboardSettings();
void forgetBLEKeyboardPairing();
const char* getBLEKBStateString();

// NimBLE Client callbacks for connection state
class BLEKBClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) override {
    Serial.println("BLEKB: Connected to keyboard");
    bleKBHost.state = BLEKB_STATE_CONNECTED;
    bleKBHost.lastKeyTime = millis();
  }

  void onDisconnect(NimBLEClient* pClient) override {
    Serial.println("BLEKB: Disconnected");
    bleKBHost.state = BLEKB_STATE_DISCONNECTED;
    bleKBHost.pReportChar = nullptr;

    // Schedule a reconnect scan ~2s out. Keyboards commonly drop the link
    // right after initial bonding; a fast rescan reconnects them promptly
    // instead of waiting the full reconnect interval.
    if (bleKBHost.autoReconnect && bleKBHost.pairedDevice.valid) {
      bleKBHost.lastReconnectAttempt = millis() - BLE_RECONNECT_INTERVAL + 2000;
    }
  }

  void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
    Serial.printf("BLEKB: Auth complete - encrypted=%d bonded=%d\n",
                  desc->sec_state.encrypted, desc->sec_state.bonded);
  }

  // Keyboard demands PIN entry: we are the display side, the user types
  // this passkey on the keyboard. The settings screen shows the number
  // before the connect starts.
  uint32_t onPassKeyRequest() override {
    Serial.printf("BLEKB: Passkey requested, displaying %d\n", BLE_KB_PASSKEY);
    return BLE_KB_PASSKEY;
  }

  // Numeric comparison pairing: no way to show/confirm mid-connect, accept
  bool onConfirmPIN(uint32_t pin) override {
    Serial.printf("BLEKB: Confirm PIN %lu - accepting\n", (unsigned long)pin);
    return true;
  }
};

// Scan callbacks for device discovery
class BLEKBScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
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

    String addr = advertisedDevice->getAddress().toString().c_str();

    // If this is the paired keyboard, flag it so the main loop can reconnect
    // (connecting from the scan callback context is not allowed)
    if (bleKBHost.pairedDevice.valid &&
        addr.equalsIgnoreCase(bleKBHost.pairedDevice.address)) {
      bleKBHost.reconnectDeviceSeen = true;
    }

    if (bleKBHost.foundCount < BLE_MAX_FOUND_DEVICES) {
      String name = advertisedDevice->getName().c_str();
      if (name.length() == 0) {
        name = "Unknown Device";
      }

      // Check for duplicates
      for (int i = 0; i < bleKBHost.foundCount; i++) {
        if (bleKBHost.foundDevices[i].address == addr) {
          return;  // Already found this device
        }
      }

      bleKBHost.foundDevices[bleKBHost.foundCount].name = name;
      bleKBHost.foundDevices[bleKBHost.foundCount].address = addr;
      bleKBHost.foundDevices[bleKBHost.foundCount].addrType = advertisedDevice->getAddress().getType();
      bleKBHost.foundDevices[bleKBHost.foundCount].rssi = advertisedDevice->getRSSI();
      bleKBHost.foundCount++;

      Serial.print("BLEKB: Found HID device: ");
      Serial.print(name);
      Serial.print(" [");
      Serial.print(addr);
      Serial.print("] RSSI: ");
      Serial.println(advertisedDevice->getRSSI());
    }
  }
};

// Static callback instances (a new'd instance per scan/connect would leak)
static BLEKBClientCallbacks bleKBClientCallbacks;
static BLEKBScanCallbacks bleKBScanCallbacks;

// Scan-complete callback. Runs on the NimBLE host task, so it must not touch
// LVGL or connect - updateBLEKeyboardHost() advances the state machine from
// the main loop. Its real purpose is selecting the NON-BLOCKING overload of
// NimBLEScan::start(); the (duration, bool) overload blocks the caller for
// the entire scan, which froze the UI for 10s per scan.
static void bleKBScanCompleteCB(NimBLEScanResults results) {
  Serial.printf("BLEKB: Scan finished, %d HID device(s) found\n", bleKBHost.foundCount);
}

// HID Report notification callback
void hidReportNotifyCallback(NimBLERemoteCharacteristic* pChar,
                              uint8_t* pData, size_t length, bool isNotify) {
  // HID Boot Protocol keyboard report is 8 bytes:
  // [0] = Modifier keys (Ctrl, Shift, Alt, GUI)
  // [1] = Reserved (always 0)
  // [2-7] = Up to 6 key codes

  if (length < 8) {
    // Non-standard report, try to handle anyway
    if (length >= 3) {
      // At least modifier + reserved + 1 key
    } else {
      return;
    }
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
  bleKBHost.lastKeyTime = 0;
  memset(bleKBHost.prevKeys, 0, sizeof(bleKBHost.prevKeys));

  // Initialize NimBLE in Central mode
  NimBLEDevice::init("VAIL-SUMMIT-KB");

  // Set power level for better range
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  // HID keyboards require bonding + link encryption before they will send
  // input reports. Just Works pairing (bond, no MITM, secure connections)
  // works with most keyboards; DISPLAY_ONLY additionally enables passkey
  // entry for keyboards that demand a PIN (user types BLE_KB_PASSKEY on the
  // keyboard - Just Works still applies when neither side requires MITM).
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

  bleKBHost.state = BLEKB_STATE_READY;
  Serial.println("BLEKB: Initialized successfully");
}

// Deinitialize BLE keyboard host
void deinitBLEKeyboardHost() {
  if (bleKBHost.state == BLEKB_STATE_IDLE) {
    return;
  }

  Serial.println("BLEKB: Deinitializing...");

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

  Serial.println("BLEKB: Deinitialized");
}

// Start scanning for BLE keyboards
void startBLEKeyboardScan() {
  if (bleKBHost.state == BLEKB_STATE_IDLE) {
    initBLEKeyboardHost();
  }

  if (bleKBHost.state == BLEKB_STATE_SCANNING) {
    Serial.println("BLEKB: Already scanning");
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
  pScan->setAdvertisedDeviceCallbacks(&bleKBScanCallbacks, false);
  pScan->setActiveScan(true);  // Active scan for device names
  pScan->setInterval(100);
  pScan->setWindow(99);

  bleKBHost.state = BLEKB_STATE_SCANNING;
  // The callback overload is the non-blocking one; start(duration, bool)
  // blocks the calling task until the scan completes
  pScan->start(BLE_SCAN_DURATION_SEC, bleKBScanCompleteCB, false);
}

// Stop scanning
void stopBLEKeyboardScan() {
  if (bleKBHost.state == BLEKB_STATE_SCANNING) {
    NimBLEDevice::getScan()->stop();
    bleKBHost.state = BLEKB_STATE_SCAN_COMPLETE;
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
    bleKBHost.foundDevices[deviceIndex].address.c_str(),
    bleKBHost.foundDevices[deviceIndex].addrType,
    true /* user-initiated fresh pairing */);
}

// Connect to a keyboard by address
bool connectToBLEKeyboardByAddress(const char* address, uint8_t addrType, bool freshPair) {
  if (bleKBHost.state == BLEKB_STATE_IDLE) {
    initBLEKeyboardHost();
  }

  // NimBLE cannot initiate a connection while a scan is running
  if (NimBLEDevice::getScan()->isScanning()) {
    NimBLEDevice::getScan()->stop();
  }

  Serial.print("BLEKB: Connecting to ");
  Serial.println(address);

  bleKBHost.state = BLEKB_STATE_CONNECTING;
  bleKBHost.lastError = "";

  // Create client if needed
  if (bleKBHost.pClient == nullptr) {
    bleKBHost.pClient = NimBLEDevice::createClient();
    bleKBHost.pClient->setClientCallbacks(&bleKBClientCallbacks, false);
    // Connect blocks the main loop; keep the timeout well under the 30s loop
    // watchdog or a failed attempt would panic-reboot the device
    bleKBHost.pClient->setConnectTimeout(10);
  }

  // Set connection parameters for low latency
  bleKBHost.pClient->setConnectionParams(12, 12, 0, 200);

  // Up to two full attempts: on a first-time pairing the notification
  // subscription can land before link encryption is fully established, in
  // which case the keyboard silently never sends reports even though every
  // call returned success. If the link isn't encrypted after attempt 1,
  // drop the connection and redo it - by then the bond exists, so attempt 2
  // encrypts immediately (this replaces the manual disconnect/reconnect that
  // used to be needed after the first pairing).
  NimBLEAddress bleAddr(std::string(address), addrType);

  // Fresh pairing: the keyboard is in pairing mode, so it has forgotten any
  // previous bond. A stale bond on our side then makes encryption fail with
  // "missing key" until both devices are power-cycled - delete it up front.
  if (freshPair && NimBLEDevice::isBonded(bleAddr)) {
    Serial.println("BLEKB: Removing stale bond before fresh pairing");
    NimBLEDevice::deleteBond(bleAddr);
  }

  int subscribedCount = 0;
  for (int attempt = 0; attempt < 2; attempt++) {
    subscribedCount = 0;
    bleKBHost.pReportChar = nullptr;

    // The connect/pair sequence can block for tens of seconds worst-case
    // (10s connect timeout + SM procedure); feed the loop watchdog between
    // attempts so a slow pairing doesn't panic-reboot the device
    esp_task_wdt_reset();

    // Connect to the device (address type matters: keyboards almost always use
    // random-static addresses, and a connect with the wrong type never succeeds)
    if (!bleKBHost.pClient->connect(bleAddr)) {
      Serial.println("BLEKB: Connection failed");
      bleKBHost.state = BLEKB_STATE_ERROR;
      bleKBHost.lastError = "Connection failed";
      return false;
    }

    Serial.println("BLEKB: Connected, requesting encryption...");

    // Pair/bond and encrypt the link. Most keyboards refuse to send input
    // reports (or reject the subscribe) on an unencrypted link.
    if (!bleKBHost.pClient->secureConnection()) {
      Serial.println("BLEKB: Pairing failed (continuing, some keyboards allow it)");
    }

    Serial.println("BLEKB: Discovering services...");

    // Get HID service
    NimBLERemoteService* pService = bleKBHost.pClient->getService(NimBLEUUID(HID_SERVICE_UUID));
    if (pService == nullptr) {
      Serial.println("BLEKB: HID service not found");
      bleKBHost.pClient->disconnect();
      bleKBHost.state = BLEKB_STATE_ERROR;
      bleKBHost.lastError = "HID service not found";
      return false;
    }

    // Subscribe to every notifiable input report. Combo devices expose several
    // (keyboard, media keys, mouse) and only one carries key presses - the
    // notify callback parses keyboard-format reports and ignores the rest.
    // Fall back to the Boot Keyboard Input Report (2A22) if no 2A4D notifies.
    std::vector<NimBLERemoteCharacteristic*>* chars = pService->getCharacteristics(true);

    for (auto pChar : *chars) {
      bool isInputReport = (pChar->getUUID() == NimBLEUUID(HID_REPORT_CHAR_UUID) ||
                            pChar->getUUID() == NimBLEUUID(HID_BOOT_INPUT_UUID)) &&
                           pChar->canNotify();
      if (!isInputReport) continue;

      if (pChar->subscribe(true, hidReportNotifyCallback)) {
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
      bleKBHost.state = BLEKB_STATE_ERROR;
      bleKBHost.lastError = "Report char not found";
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
      Serial.println("BLEKB: Link still unencrypted, continuing anyway");
    }
  }

  Serial.println("BLEKB: Subscribed to keyboard input notifications");

  // Save pairing info
  strncpy(bleKBHost.pairedDevice.address, address, sizeof(bleKBHost.pairedDevice.address) - 1);
  bleKBHost.pairedDevice.addrType = addrType;

  // Get device name
  String deviceName = bleKBHost.pClient->getPeerAddress().toString().c_str();
  for (int i = 0; i < bleKBHost.foundCount; i++) {
    if (bleKBHost.foundDevices[i].address == address) {
      deviceName = bleKBHost.foundDevices[i].name;
      break;
    }
  }
  strncpy(bleKBHost.pairedDevice.name, deviceName.c_str(),
          sizeof(bleKBHost.pairedDevice.name) - 1);
  bleKBHost.pairedDevice.valid = true;

  // Save to preferences
  saveBLEKeyboardSettings();

  bleKBHost.state = BLEKB_STATE_CONNECTED;
  bleKBHost.lastKeyTime = millis();

  Serial.print("BLEKB: Successfully connected to ");
  Serial.println(bleKBHost.pairedDevice.name);

  return true;
}

// Disconnect from keyboard
void disconnectBLEKeyboard() {
  if (bleKBHost.pClient != nullptr && bleKBHost.pClient->isConnected()) {
    Serial.println("BLEKB: Disconnecting...");
    bleKBHost.pClient->disconnect();
  }
  bleKBHost.state = BLEKB_STATE_READY;
  bleKBHost.pReportChar = nullptr;
}

// Update function (call from main loop)
void updateBLEKeyboardHost() {
  // Only run if BLE keyboard host is active
  if (bleKBHost.state == BLEKB_STATE_IDLE) {
    return;
  }

  // Detect scan completion (NimBLE stops on its own after BLE_SCAN_DURATION_SEC,
  // but nothing else advances our state machine out of SCANNING)
  if (bleKBHost.state == BLEKB_STATE_SCANNING && !NimBLEDevice::getScan()->isScanning()) {
    bleKBHost.state = BLEKB_STATE_SCAN_COMPLETE;
    Serial.printf("BLEKB: Scan complete, %d device(s) found\n", bleKBHost.foundCount);
  }

  // The paired keyboard was spotted advertising: connect now. The device is
  // in range, so the blocking connect resolves quickly instead of eating the
  // full 10s timeout.
  if (bleKBHost.reconnectDeviceSeen) {
    bleKBHost.reconnectDeviceSeen = false;
    if (bleKBHost.autoReconnect && bleKBHost.pairedDevice.valid &&
        bleKBHost.state != BLEKB_STATE_CONNECTED &&
        bleKBHost.state != BLEKB_STATE_CONNECTING) {
      Serial.println("BLEKB: Paired keyboard in range, reconnecting...");
      connectToBLEKeyboardByAddress(bleKBHost.pairedDevice.address,
                                    bleKBHost.pairedDevice.addrType);
    }
  }

  // Handle auto-reconnect by scanning for the paired keyboard rather than
  // blind-connecting: a connect attempt while the keyboard is off/asleep
  // blocks the main loop for the full connect timeout, whereas a scan runs
  // in the background and only triggers a connect once the keyboard is seen.
  if (bleKBHost.state == BLEKB_STATE_DISCONNECTED ||
      bleKBHost.state == BLEKB_STATE_READY ||
      bleKBHost.state == BLEKB_STATE_SCAN_COMPLETE ||
      bleKBHost.state == BLEKB_STATE_ERROR) {
    if (bleKBHost.autoReconnect && bleKBHost.pairedDevice.valid) {
      unsigned long now = millis();
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

// Load settings from Preferences
void loadBLEKeyboardSettings() {
  Preferences prefs;
  prefs.begin("btkeyboard", true);

  bleKBHost.pairedDevice.valid = prefs.getBool("valid", false);
  if (bleKBHost.pairedDevice.valid) {
    String name = prefs.getString("name", "");
    String addr = prefs.getString("addr", "");
    strncpy(bleKBHost.pairedDevice.name, name.c_str(),
            sizeof(bleKBHost.pairedDevice.name) - 1);
    strncpy(bleKBHost.pairedDevice.address, addr.c_str(),
            sizeof(bleKBHost.pairedDevice.address) - 1);
    // Default to random-static: what nearly all BLE keyboards use, and what
    // pairings saved before the type was persisted most likely need
    bleKBHost.pairedDevice.addrType = prefs.getUChar("addrType", BLE_ADDR_RANDOM);
  }
  bleKBHost.autoReconnect = prefs.getBool("autoRecon", true);

  prefs.end();

  Serial.print("BLEKB: Settings loaded, paired device: ");
  if (bleKBHost.pairedDevice.valid) {
    Serial.println(bleKBHost.pairedDevice.name);
  } else {
    Serial.println("None");
  }
}

// Forget paired keyboard
void forgetBLEKeyboardPairing() {
  // Disconnect if connected
  if (bleKBHost.pClient != nullptr && bleKBHost.pClient->isConnected()) {
    bleKBHost.pClient->disconnect();
  }

  // Clear pairing info
  bleKBHost.pairedDevice.valid = false;
  memset(bleKBHost.pairedDevice.name, 0, sizeof(bleKBHost.pairedDevice.name));
  memset(bleKBHost.pairedDevice.address, 0, sizeof(bleKBHost.pairedDevice.address));

  // Save to preferences
  saveBLEKeyboardSettings();

  bleKBHost.state = BLEKB_STATE_READY;
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

#endif // BLE_KEYBOARD_HOST_H
