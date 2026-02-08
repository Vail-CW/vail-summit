/*
 * VAIL SUMMIT - Morse Code Training Device
 * Main program file (refactored for modularity)
 */

// Note: PSRAM is enabled via Arduino IDE board settings (PSRAM: "OPI PSRAM")
// No explicit PSRAM initialization code needed for Arduino ESP32 core 2.0.14

// LovyanGFX for ST7796S 4.0" display (requires Arduino ESP32 core 2.0.14)
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// LVGL Graphics Library
// Uses lv_conf.h from Arduino/libraries folder (next to lvgl folder)
#include <lvgl.h>

// Use LovyanGFX built-in fonts (in lgfx::v1::fonts namespace)
// Available: FreeSansBold9pt7b, FreeSansBold12pt7b, FreeSansBold18pt7b, etc.
using namespace lgfx::v1::fonts;

// Standard libraries
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <WiFi.h>

// Configuration and battery monitoring
#include "src/core/config.h"
#include "src/core/memory_monitor.h"
#include <Adafruit_LC709203F.h>
#include <Adafruit_MAX1704X.h>

// Core modules
#include "src/audio/i2s_audio.h"
#include "src/core/morse_code.h"
#include "src/core/task_manager.h"

// Hardware initialization (defines LGFX class, must come before LVGL modules)
#include "src/core/hardware_init.h"

// LVGL modules (must come after hardware_init.h for LGFX type)
#include "src/lvgl/lv_init.h"
#include "src/lvgl/lv_screen_manager.h"
#include "src/lvgl/lv_theme_summit.h"
#include "src/lvgl/lv_widgets_summit.h"
#include "src/lvgl/lv_menu_screens.h"

// LVGL is the only UI system - no legacy rendering

// Boot splash screen (LVGL version)
#include "src/lvgl/lv_splash_screen.h"

// Status bar
#include "src/ui/status_bar.h"

// Menu system
#include "src/ui/menu_ui.h"

// Training modes
#include "src/training/training_hear_it_type_it.h"
#include "src/training/training_practice.h"
#include "src/training/training_cwa.h"
#include "src/training/training_license_ui.h"
#include "src/training/training_license_input.h"
#include "src/training/training_vail_master.h"

// Games
#include "src/games/game_morse_shooter.h"
#include "src/games/game_memory_chain.h"
#include "src/games/game_story_time.h"
#include "src/games/game_cw_speeder.h"

// Settings modes
#include "src/settings/settings_wifi.h"
#include "src/settings/settings_cw.h"
#include "src/settings/settings_volume.h"
#include "src/settings/settings_brightness.h"
#include "src/settings/settings_general.h"
#include "src/settings/settings_web_password.h"

// Connectivity
#include "src/network/vail_repeater.h"
#include "src/network/morse_mailbox.h"
#include "src/network/cwschool_link.h"

// Practice time tracking (for CW School sync)
#include "src/settings/settings_practice_time.h"

// Progress sync (CW School cloud sync)
#include "src/network/progress_sync.h"

// Vail Course training mode
#include "src/training/training_vail_course_core.h"

// Radio modes (CW memories must be included before radio_output)
#include "src/radio/radio_cw_memories.h"
#include "src/radio/radio_output.h"

// NTP Time
#include "src/network/ntp_time.h"

// QSO Logger (order matters - validation and API must come first)
#include "src/qso/qso_logger_validation.h"
#include "src/network/pota_api.h"
#include "src/qso/qso_logger.h"
#include "src/qso/qso_logger_storage.h"
#include "src/qso/qso_logger_input.h"
#include "src/qso/qso_logger_ui.h"
#include "src/qso/qso_logger_view.h"
#include "src/qso/qso_logger_statistics.h"
#include "src/qso/qso_logger_settings.h"

// POTA Recorder (must come after QSO Logger for storage access)
#include "src/pota/pota_recorder.h"

// Web Server (must come after QSO Logger to access storage)
#include "src/web/server/web_server.h"

// Web Practice Mode
#include "src/web/modes/web_practice_mode.h"

// Web Memory Chain Mode
#include "src/web/modes/web_memory_chain_mode.h"

// Web Hear It Type It Mode
#include "src/web/modes/web_hear_it_mode.h"

// SD Card Storage
#include "src/storage/sd_card.h"

// Web File Downloader (needed early for boot-time download check)
#include "src/web/server/web_file_downloader.h"

// Web First Boot (must come after sd_card.h)
#include "src/web/server/web_first_boot.h"

// LVGL Web Download Screen (must come after web_first_boot.h)
#include "src/lvgl/lv_web_download_screen.h"

// Bluetooth modes
#include "src/bluetooth/ble_hid.h"
#include "src/bluetooth/ble_midi.h"

// Bluetooth keyboard host (external keyboard input)
#include "src/settings/settings_bt_keyboard.h"

// Menu navigation (must come after all mode headers that define input handlers)
#include "src/ui/menu_navigation.h"

// Additional LVGL screen modules (must come after settings modules for type access)
#include "src/lvgl/lv_settings_screens.h"
#include "src/lvgl/lv_training_screens.h"
#include "src/lvgl/lv_game_screens.h"
#include "src/lvgl/lv_mode_screens.h"
#include "src/lvgl/lv_pota_recorder.h"
#include "src/lvgl/lv_mode_integration.h"

// ============================================
// cwKeyType accessor functions for LVGL (needed because cwKeyType is KeyType enum)
// ============================================
int getCwKeyTypeAsInt() { return (int)cwKeyType; }
void setCwKeyTypeFromInt(int keyType) { cwKeyType = (KeyType)keyType; }

// ============================================
// Global Hardware Objects
// ============================================

// Battery monitor (one of these will be present)
Adafruit_LC709203F lc;
Adafruit_MAX17048 maxlipo;
bool hasLC709203 = false;
bool hasMAX17048 = false;
bool hasBatteryMonitor = false;

// Create display object (LovyanGFX configured in hardware_init.h)
LGFX tft;

// ============================================
// Menu System State
// ============================================

MenuMode currentMode = MODE_MAIN_MENU;
int currentSelection = 0;
bool menuActive = true;

// Getter/setter for currentMode to allow LVGL integration without circular includes
int getCurrentModeAsInt() { return (int)currentMode; }
void setCurrentModeFromInt(int mode) { currentMode = (MenuMode)mode; }

// ============================================
// Early Boot Download Screen (direct TFT, no LVGL)
// ============================================
// Used during web file downloads that happen before LVGL init.
// Draws directly to the display using LovyanGFX.

#define DL_BAR_X      60
#define DL_BAR_Y      160
#define DL_BAR_W      360
#define DL_BAR_H      22
#define DL_STATUS_Y   210
#define DL_FILE_Y     245

void drawEarlyBootDownloadScreen() {
  tft.fillScreen(COLOR_BACKGROUND);

  // Title
  tft.setFont(&FreeSansBold18pt7b);
  tft.setTextColor(COLOR_TITLE);
  tft.setTextDatum(lgfx::top_center);
  tft.drawString("VAIL SUMMIT", SCREEN_WIDTH / 2, 30);

  // Subtitle
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(COLOR_TEXT_PRIMARY);
  tft.drawString("Downloading Web Files", SCREEN_WIDTH / 2, 90);

  // "Please wait" note
  tft.setFont(&FreeSansBold9pt7b);
  tft.setTextColor(0x8C71);  // dim gray
  tft.drawString("Device will reboot when complete", SCREEN_WIDTH / 2, 125);

  // Progress bar outline
  tft.drawRect(DL_BAR_X, DL_BAR_Y, DL_BAR_W, DL_BAR_H, COLOR_TEXT_PRIMARY);

  // Initial status
  tft.setFont(nullptr);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_ACCENT_CYAN);
  tft.setTextDatum(lgfx::top_center);
  tft.drawString("Initializing...", SCREEN_WIDTH / 2, DL_STATUS_Y);
  tft.setTextSize(1);
}

void earlyBootProgressCallback(const char* status, int currentFile, int totalFiles) {
  // Update progress bar fill
  if (totalFiles > 0) {
    int fillW = ((DL_BAR_W - 4) * currentFile) / totalFiles;
    tft.fillRect(DL_BAR_X + 2, DL_BAR_Y + 2, fillW, DL_BAR_H - 4, COLOR_ACCENT_CYAN);
  }

  // Clear text areas
  tft.fillRect(0, DL_STATUS_Y, SCREEN_WIDTH, 80, COLOR_BACKGROUND);

  // File counter
  tft.setFont(nullptr);
  tft.setTextSize(2);
  tft.setTextDatum(lgfx::top_center);

  if (totalFiles > 0) {
    char countBuf[32];
    snprintf(countBuf, sizeof(countBuf), "File %d / %d", currentFile, totalFiles);
    tft.setTextColor(COLOR_TEXT_PRIMARY);
    tft.drawString(countBuf, SCREEN_WIDTH / 2, DL_STATUS_Y);
  }

  // Status / filename
  tft.setTextColor(COLOR_ACCENT_CYAN);
  tft.drawString(status, SCREEN_WIDTH / 2, DL_FILE_Y);
  tft.setTextSize(1);
}

// ============================================
// Setup - Hardware Initialization
// ============================================

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500); // Brief wait for serial monitor
  Serial.println("\n\n=== VAIL SUMMIT STARTING ===");
  Serial.printf("Firmware: %s (Build: %s)\n", FIRMWARE_VERSION, FIRMWARE_DATE);
  Serial.println("Starting setup...");

  // ============================================
  // PSRAM Diagnostic - Run First
  // ============================================
  Serial.println("\n--- PSRAM Diagnostic ---");
  Serial.printf("ESP32 Chip Model: %s\n", ESP.getChipModel());
  Serial.printf("Chip Revision: %d\n", ESP.getChipRevision());
  Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash Size: %d bytes\n", ESP.getFlashChipSize());

  Serial.println("\nChecking PSRAM (before manual init)...");
  Serial.printf("psramFound(): %s\n", psramFound() ? "true" : "false");
  Serial.printf("ESP.getPsramSize(): %d bytes\n", ESP.getPsramSize());

  // PSRAM is auto-initialized when "OPI PSRAM" is selected in Arduino IDE
  // Manual initialization (esp_psram_init) is not available in Arduino ESP32 core 2.0.14
  if (psramFound() && ESP.getPsramSize() == 0) {
    Serial.println("\nWARNING: PSRAM detected but reports 0 bytes size.");
    Serial.println("This may indicate a board configuration issue.");
  }

  Serial.println("\nRechecking PSRAM (after manual init)...");
  Serial.printf("psramFound(): %s\n", psramFound() ? "true" : "false");
  Serial.printf("ESP.getPsramSize(): %d bytes\n", ESP.getPsramSize());
  Serial.printf("ESP.getFreePsram(): %d bytes\n", ESP.getFreePsram());
  Serial.printf("ESP.getMinFreePsram(): %d bytes\n", ESP.getMinFreePsram());

  // Try a test allocation
  if (psramFound()) {
    Serial.println("\nPSRAM found! Testing allocation...");
    void* testPtr = ps_malloc(1024);
    if (testPtr) {
      Serial.println("PSRAM test allocation: SUCCESS");
      free(testPtr);
    } else {
      Serial.println("PSRAM test allocation: FAILED");
    }
  } else {
    Serial.println("\nWARNING: PSRAM NOT DETECTED!");
    Serial.println("Possible causes:");
    Serial.println("  1. PSRAM not enabled in Arduino IDE (Tools > PSRAM)");
    Serial.println("  2. Wrong board selected (should be ESP32-S3 variant)");
    Serial.println("  3. Hardware doesn't have PSRAM");
    Serial.println("  4. ESP32 Arduino core version issue");
  }
  Serial.println("--- End PSRAM Diagnostic ---\n");

  // ============================================
  // Early Boot Web Download Check
  // ============================================
  // Check if a web files download was requested (via reboot)
  // This MUST happen before LVGL initialization when RAM is plentiful
  Serial.println("Checking for pending web download...");
  bool downloadPending = isWebDownloadPending();
  Serial.printf("Download pending flag: %s\n", downloadPending ? "TRUE" : "FALSE");
  if (downloadPending) {
    Serial.println("\n========================================");
    Serial.println("WEB DOWNLOAD PENDING - EARLY BOOT MODE");
    Serial.println("========================================");
    Serial.printf("Free heap: %d bytes, max block: %d bytes\n",
      ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    // Initialize display + backlight early so user sees progress
    setupBrightnessPWM();
    loadBrightnessSettings();
    applyBrightness(brightnessValue);
    initDisplay();
    drawEarlyBootDownloadScreen();

    // Load saved WiFi credentials from Preferences
    // WiFi credentials are stored as ssid1/pass1, ssid2/pass2, ssid3/pass3
    Preferences wifiPrefs;
    wifiPrefs.begin("wifi", true);  // read-only
    String savedSSID = wifiPrefs.getString("ssid1", "");
    String savedPassword = wifiPrefs.getString("pass1", "");
    wifiPrefs.end();

    Serial.printf("Loaded WiFi credentials - SSID: '%s' (length: %d)\n",
      savedSSID.c_str(), savedSSID.length());

    if (savedSSID.length() > 0) {
      Serial.printf("Using saved WiFi: %s\n", savedSSID.c_str());

      // Perform the download with progress callback for display updates
      bool success = performEarlyBootWebDownload(savedSSID.c_str(), savedPassword.c_str(),
                                                  earlyBootProgressCallback);

      // Clear the pending flag
      clearWebDownloadPending();

      // Show result on screen before rebooting
      tft.fillRect(0, DL_STATUS_Y, SCREEN_WIDTH, 80, COLOR_BACKGROUND);
      tft.setFont(nullptr);
      tft.setTextSize(2);
      tft.setTextDatum(lgfx::top_center);

      if (success) {
        Serial.println("\n========================================");
        Serial.println("DOWNLOAD COMPLETE - REBOOTING TO NORMAL");
        Serial.println("========================================\n");
        // Fill progress bar completely
        tft.fillRect(DL_BAR_X + 2, DL_BAR_Y + 2, DL_BAR_W - 4, DL_BAR_H - 4, COLOR_ACCENT_CYAN);
        tft.setTextColor(0x07E0);  // green
        tft.drawString("Download complete!", SCREEN_WIDTH / 2, DL_STATUS_Y);
        tft.setTextColor(COLOR_TEXT_PRIMARY);
        tft.drawString("Rebooting...", SCREEN_WIDTH / 2, DL_FILE_Y);
      } else {
        Serial.println("\n========================================");
        Serial.println("DOWNLOAD FAILED - REBOOTING TO NORMAL");
        Serial.println("========================================\n");
        tft.setTextColor(0xF800);  // red
        tft.drawString("Download failed!", SCREEN_WIDTH / 2, DL_STATUS_Y);
        tft.setTextColor(COLOR_TEXT_PRIMARY);
        tft.drawString("Rebooting...", SCREEN_WIDTH / 2, DL_FILE_Y);
      }
      tft.setTextSize(1);

      // Clean up before reboot
      Serial.println("Disconnecting WiFi before reboot...");
      WiFi.disconnect(true);  // Disconnect and clear saved state
      WiFi.mode(WIFI_OFF);
      delay(100);

      delay(1500);  // Show result message briefly
      ESP.restart();
    } else {
      Serial.println("No saved WiFi credentials - clearing flag and continuing");
      clearWebDownloadPending();
      // Show error on screen
      earlyBootProgressCallback("No WiFi credentials saved!", 0, 0);
      delay(2000);
      ESP.restart();
    }
  }

  // Initialize backlight control with PWM (GPIO 39)
  Serial.println("Initializing backlight PWM control on GPIO 39...");
  setupBrightnessPWM();
  loadBrightnessSettings();
  applyBrightness(brightnessValue);
  Serial.print("Backlight enabled at ");
  Serial.print(brightnessValue);
  Serial.println("% brightness");

  // Initialize I2C first (before display to avoid conflicts)
  Serial.println("Initializing I2C...");
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);

  // Initialize I2S Audio BEFORE display (critical for DMA priority)
  Serial.println("\nInitializing I2S audio...");
  initI2SAudio();
  delay(100);

  // Initialize LCD (after I2S to avoid DMA conflicts)
  initDisplay();

  // Initialize LVGL library with display and input drivers
  Serial.println("Initializing LVGL...");
  if (!initLVGL(tft)) {
    Serial.println("CRITICAL ERROR: LVGL initialization failed!");
    // Can't continue without LVGL - display error and halt
    tft.fillScreen(0xF800);  // Red background
    tft.setTextColor(0xFFFF);
    tft.setCursor(10, 100);
    tft.println("LVGL INIT FAILED");
    while (1) delay(1000);
  }
  Serial.println("LVGL initialized successfully");
  // Initialize theme manager (must be before initSummitTheme)
  initThemeManager();
  // Load saved theme preference
  loadThemeSettings();
  // Initialize VAIL SUMMIT theme styles (uses active theme colors)
  initSummitTheme();
  // Initialize mode integration (menu callbacks, back navigation)
  initLVGLModeIntegration();

  // Set up FreeRTOS task manager (audio on Core 0, UI on Core 1)
  Serial.println("Setting up task manager...");
  setupTaskManager();

  // Show LVGL boot splash screen immediately for visual feedback
  showSplashScreen();
  setSplashStage(1);  // "Initializing I2C..."

  // Initialize GPIO pins
  initPins();
  setSplashStage(2);  // "Starting audio..."

  // SD card initialization moved to on-demand (when storage page is accessed)
  // This avoids SPI bus conflicts with display during boot
  Serial.println("SD card will be initialized on first access");

  // Initialize battery monitoring (I2C chip)
  initBatteryMonitor();
  setSplashStage(3);  // "Loading settings..."

  // Initialize WiFi and attempt auto-connect
  Serial.println("Initializing WiFi...");

  // Set up WiFi event handler to auto-start web server
  // Note: Web files check is now triggered by internet_check.h when INET_CONNECTED is verified
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      Serial.println("WiFi connected! Starting web server...");
      setupWebServer();
      // Web files check moved to after internet verification (see internet_check.h)
    } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      Serial.println("WiFi disconnected. Stopping web server...");
      stopWebServer();
    }
  });

  // Enable auto-reconnect to saved networks
  WiFi.setAutoReconnect(true);

  autoConnectWiFi();

  // Initialize internet check with optimistic display (shows cyan immediately if WiFi connected)
  initInternetCheck();
  Serial.println("WiFi initialized");
  Serial.printf("WiFi status: %s, Internet status: %s\n",
      WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected",
      getInternetStatusString());

  setSplashStage(4);  // "Configuring WiFi..."
  // NOTE: OTA server starts on-demand when entering firmware update menu

  // Load CW settings from preferences
  Serial.println("Loading CW settings...");
  loadCWSettings();

  // Load radio settings from preferences
  Serial.println("Loading radio settings...");
  loadRadioSettings();

  // Load CW memories from preferences
  Serial.println("Loading CW memories...");
  loadCWMemories();

  // Load saved callsign
  Serial.println("Loading callsign...");
  loadSavedCallsign();

  // Load saved web password
  Serial.println("Loading web password...");
  loadSavedWebPassword();

  // Load Vail Repeater settings (saved room)
  Serial.println("Loading Vail settings...");
  loadVailSettings();

  // Initialize Morse Mailbox (load settings, start polling if linked)
  Serial.println("Initializing Morse Mailbox...");
  initMailboxPolling();

  // Initialize CW School (load settings)
  Serial.println("Initializing CW School...");
  initCWSchool();

  // Initialize practice time tracking
  Serial.println("Loading practice time data...");
  loadPracticeTimeData();

  // Initialize progress sync
  Serial.println("Initializing progress sync...");
  initProgressSync();

  // Initialize Vail Course
  Serial.println("Initializing Vail Course...");
  initVailCourse();

  setSplashStage(5);  // "Starting web server..."

  // Initialize NTP time (if WiFi connected)
  Serial.println("Initializing NTP time...");
  initNTPTime();

  // Initialize SPIFFS for QSO Logger
  Serial.println("Initializing storage...");
  if (!initStorage()) {
    Serial.println("WARNING: Storage initialization failed!");
  }

  // Load QSO Logger operator settings
  Serial.println("Loading QSO Logger settings...");
  loadOperatorSettings();
  setSplashStage(6);  // "Initializing UI..."

  // Load BLE keyboard settings (for auto-reconnect on boot)
  Serial.println("Loading BLE keyboard settings...");
  loadBLEKeyboardSettings();

  // Initial status update
  Serial.println("Updating status...");
  updateStatus();
  setSplashStage(7);  // "Almost ready..."

  // Brief pause to show 100% complete
  setSplashStage(8);  // "Ready!"
  delay(300);  // Brief pause to show completion

  // Clean up splash screen and show initial LVGL menu
  Serial.println("Drawing LVGL menu...");
  cleanupSplashScreen();  // Free splash screen resources
  showInitialLVGLScreen();

  Serial.println("Setup complete!");

  // Log memory status after full initialization
  Serial.println("\n=== MEMORY STATUS AFTER BOOT ===");
  logMemoryStatus("Boot");
  Serial.println("================================\n");

  // Startup beep (test I2S audio)
  Serial.println("\nTesting I2S audio output...");
  Serial.println("You should hear a 1000 Hz beep now");
  beep(TONE_STARTUP, BEEP_MEDIUM);
  delay(200);

  // Second test beep
  Serial.println("Second test beep at 700 Hz");
  beep(700, 300);
  Serial.println("Audio test complete\n");

  // Optionally test QSO storage (uncomment to test)
  // testSaveDummyQSO();
}

// Helper function to read keyboard (used by first-boot prompt)
char readKeyboardNonBlocking() {
  Wire.requestFrom(CARDKB_ADDR, 1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0;
}

// Helper function to read paddle state (DIT/DAH via GPIO and touch)
void readPaddles(bool &dit, bool &dah) {
  dit = (digitalRead(DIT_PIN) == PADDLE_ACTIVE) || (touchRead(TOUCH_DIT_PIN) > TOUCH_THRESHOLD);
  dah = (digitalRead(DAH_PIN) == PADDLE_ACTIVE) || (touchRead(TOUCH_DAH_PIN) > TOUCH_THRESHOLD);
}

// ============================================
// Mode Poll Wrappers
// ============================================
// Each wrapper encapsulates the per-loop logic for a mode.
// Replaces scattered if/else blocks in the main loop.

void pollPracticeMode() {
    updatePracticeOscillator();
    if (needsUIUpdate && !isTonePlaying()) {
        updatePracticeDecoderDisplay(decodedText.c_str());
        needsUIUpdate = false;
    }
}

void pollHearIt() { updateHearItTypeIt(); }
void pollCWACopy() { updateCWACopyPractice(); }
void pollCWAQSO() { updateCWAQSOPractice(); }

void pollCWASending() {
    if (cwaUseLVGL) {
        updateCWASendingPracticeLVGL();
    } else {
        updateCWASendingPractice();
        if (cwaSendNeedsUIUpdate && !isTonePlaying()) {
            drawCWASendDecodedOnly(tft);
            cwaSendNeedsUIUpdate = false;
        }
    }
}

void pollVailRepeater() {
    updateVailRepeater(tft);
    updateVailScreenLVGL();
}

void pollMorseShooter() {
    updateMorseShooterInput(tft);
    updateMorseShooterVisuals(tft);
}

void pollMemoryChain() {
    memoryChainUpdate();
    bool ditPressed, dahPressed;
    readPaddles(ditPressed, dahPressed);
    memoryChainHandlePaddle(ditPressed, dahPressed);
}

void pollCWSpeeder() {
    cwSpeedUpdate();
    bool ditPressed, dahPressed;
    readPaddles(ditPressed, dahPressed);
    cwSpeedHandlePaddle(ditPressed, dahPressed);
}

void pollVailMasterPractice() { vmUpdateKeyer(); }
void pollRadioOutput() { updateRadioOutput(); }
void pollPOTARecorder() { updatePOTARecorder(); }

void pollWebPractice() {
    updateWebPracticeMode();
    extern AsyncWebSocket* practiceWebSocket;
    if (practiceWebSocket) practiceWebSocket->cleanupClients();
}

void pollWebMemoryChain() {
    updateWebMemoryChain();
    extern AsyncWebSocket* memoryChainWebSocket;
    if (memoryChainWebSocket) memoryChainWebSocket->cleanupClients();
}

void pollWebHearIt() {
    // Browser handles all game logic - just clean up WebSocket clients
    extern AsyncWebSocket* hearItWebSocket;
    if (hearItWebSocket) hearItWebSocket->cleanupClients();
}

void pollBTHID() { updateBTHID(); }
void pollBTMIDI() { updateBTMIDI(); }
void pollLICWSending() { updateLICWSendingPractice(); }

// ============================================
// Mode Poll Dispatch Table
// ============================================

static const ModeCallbackEntry pollTable[] = {
    { MODE_PRACTICE,                     pollPracticeMode },
    { MODE_HEAR_IT_TYPE_IT,              pollHearIt },
    { MODE_CW_ACADEMY_COPY_PRACTICE,     pollCWACopy },
    { MODE_CW_ACADEMY_QSO_PRACTICE,      pollCWAQSO },
    { MODE_CW_ACADEMY_SENDING_PRACTICE,  pollCWASending },
    { MODE_VAIL_REPEATER,                pollVailRepeater },
    { MODE_MORSE_SHOOTER,                pollMorseShooter },
    { MODE_MORSE_MEMORY,                 pollMemoryChain },
    { MODE_CW_SPEEDER,                   pollCWSpeeder },
    { MODE_VAIL_MASTER_PRACTICE,         pollVailMasterPractice },
    { MODE_RADIO_OUTPUT,                 pollRadioOutput },
    { MODE_POTA_RECORDER,                pollPOTARecorder },
    { MODE_WEB_PRACTICE,                 pollWebPractice },
    { MODE_WEB_MEMORY_CHAIN,             pollWebMemoryChain },
    { MODE_WEB_HEAR_IT,                  pollWebHearIt },
    { MODE_BT_HID,                       pollBTHID },
    { MODE_BT_MIDI,                      pollBTMIDI },
    { MODE_LICW_SEND_PRACTICE,           pollLICWSending },
};
static const int pollTableSize = sizeof(pollTable) / sizeof(pollTable[0]);

// ============================================
// Main Loop - Event Processing (LVGL-Only)
// ============================================

void loop() {

  // Process LVGL timer tasks (handles ALL UI, input, animations, redraws)
  // LVGL reads CardKB directly via its input driver
  lv_timer_handler();

  // Process any pending deferred screen operations
  // (screen creation is deferred from event callbacks to avoid stack overflow)
  processQSOViewLogsPending();

  // Poll non-blocking WiFi connection state machine
  // This must be called every loop to check connection progress
  if (updateWiFiConnection()) {
    // State changed - if we're in WiFi settings, update the UI
    if (currentMode == MODE_WIFI_SETTINGS) {
      updateWiFiScreen();
    }
  }

  // Update practice time activity accumulator (for inactivity detection)
  // This must run every loop to accurately track active practice time
  updateActivityAccumulator();

  // Check for pending web server restart (after file upload)
  if (isWebServerRestartPending()) {
    restartWebServer();
  }

  // Check for deferred web mode starts (safe to call LVGL from main loop)
  extern volatile bool webPracticeStartPending;
  extern volatile bool webHearItStartPending;
  extern volatile bool webMemoryChainStartPending;
  extern volatile bool webModeDisconnectPending;

  if (webPracticeStartPending) {
      webPracticeStartPending = false;
      onLVGLMenuSelect(MODE_WEB_PRACTICE);
  }
  if (webHearItStartPending) {
      webHearItStartPending = false;
      onLVGLMenuSelect(MODE_WEB_HEAR_IT);
  }
  if (webMemoryChainStartPending) {
      webMemoryChainStartPending = false;
      onLVGLMenuSelect(MODE_WEB_MEMORY_CHAIN);
  }
  if (webModeDisconnectPending) {
      webModeDisconnectPending = false;
      // Only trigger back-nav if we're actually in a web mode
      if (currentMode == MODE_WEB_PRACTICE || currentMode == MODE_WEB_HEAR_IT || currentMode == MODE_WEB_MEMORY_CHAIN) {
          onLVGLBackNavigation();
      }
  }

  // Note: Automatic web files version check disabled due to SSL RAM constraints
  // Users can manually download via Settings > WiFi > Web Files
  // The version check requires SSL which needs ~40KB internal RAM not available when LVGL runs
  // performWebFilesCheck();  // Disabled - SSL fails with low RAM
  // if (isWebFilesPromptPending() && !isWebDownloadScreenActive() && currentMode == MODE_MAIN_MENU) {
  //   showWebFilesUpdateNotification();
  //   clearWebFilesPromptPending();
  // }

  // Handle web download screen input and progress updates
  if (isWebDownloadScreenActive()) {
    updateWebDownloadProgressUI();

    // Get keyboard input for download screens
    char key = readKeyboardNonBlocking();
    if (key != 0) {
      handleWebDownloadInput(key);
    }
  }

  // Periodic memory health check (runs every 30 seconds internally)
  checkMemoryHealth();

  // Update status data periodically (skip during audio-critical/busy modes)
  static unsigned long lastStatusUpdate = 0;
  if (!isModeNoStatus((int)currentMode) && millis() - lastStatusUpdate > 5000) {
    updateStatus();
    lastStatusUpdate = millis();
  }

  // Dispatch mode-specific polling via registry table
  dispatchModeCallback(pollTable, pollTableSize, (int)currentMode);

  // Update Morse Mailbox polling (runs in background regardless of mode)
  updateMailboxPolling();

  // Update BLE Keyboard host (for auto-reconnect, skip during BT modes)
  if (currentMode != MODE_BT_HID && currentMode != MODE_BT_MIDI) {
    updateBLEKeyboardHost();
  }

  // Minimal delay - faster for audio-critical modes
  delay(isModeAudioCritical((int)currentMode) ? 1 : 10);
}
