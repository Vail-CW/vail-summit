/*
 * Web Password Settings Module
 * Allows user to set/change password for web interface access
 * Password is stored and can be changed without knowing current password
 */

#ifndef SETTINGS_WEB_PASSWORD_H
#define SETTINGS_WEB_PASSWORD_H

#include <Preferences.h>
#include "../core/config.h"

// Buffer sizes
#define WEB_PASSWORD_MAX_LEN 17  // 16 chars + null terminator

// Password settings state
enum PasswordState {
  PASSWORD_STATE_NORMAL,
  PASSWORD_STATE_CONFIRM_DISABLE
};

// Password settings globals
char webPasswordInput[WEB_PASSWORD_MAX_LEN] = "";
unsigned long passwordLastBlink = 0;
bool passwordCursorVisible = true;
Preferences webPasswordPrefs;
PasswordState passwordState = PASSWORD_STATE_NORMAL;

// Global password state (used by web_server.h)
char webPassword[WEB_PASSWORD_MAX_LEN] = "";
bool webAuthEnabled = false;

// LVGL mode flag - when true, skip legacy draw functions (LVGL handles display)
bool webPasswordUseLVGL = true;  // Default to LVGL mode

// Forward declarations
void startWebPasswordSettings(LGFX &display);
void drawWebPasswordUI(LGFX &display);
int handleWebPasswordInput(char key, LGFX &display);
void saveWebPassword(const char* password);
bool loadWebPassword(char* password, size_t len);
void clearWebPassword();

// Start web password settings mode
void startWebPasswordSettings(LGFX &display) {
  // Start with empty input (no need to show current password)
  webPasswordInput[0] = '\0';
  passwordState = PASSWORD_STATE_NORMAL;

  passwordCursorVisible = true;
  passwordLastBlink = millis();

  drawWebPasswordUI(display);
}

// Draw password input UI
void drawWebPasswordUI(LGFX &display) {
  if (webPasswordUseLVGL) return;  // LVGL handles display
  // Clear screen (preserve header)
  display.fillRect(0, 42, SCREEN_WIDTH, SCREEN_HEIGHT - 42, COLOR_BACKGROUND);

  // Title
  display.setFont(nullptr);  // Use default font
  display.setTextColor(COLOR_TITLE);
  display.setTextSize(1);

  int16_t x1, y1;
  uint16_t w, h;
  const char* title = "Web Password";
  getTextBounds_compat(display, title, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 65);
  display.print(title);
  display.setFont(nullptr); // Reset font

  // Current status
  display.setTextSize(1);
  if (webAuthEnabled && strlen(webPassword) > 0) {
    display.setTextColor(ST77XX_GREEN);
    const char* status = "Status: ENABLED";
    display.setCursor((SCREEN_WIDTH - strlen(status) * 6) / 2, 85);
    display.print(status);
  } else {
    display.setTextColor(COLOR_WARNING);
    const char* status = "Status: DISABLED";
    display.setCursor((SCREEN_WIDTH - strlen(status) * 6) / 2, 85);
    display.print(status);
  }

  // Instructions
  display.setTextSize(1);
  display.setTextColor(0x7BEF); // Light gray
  const char* prompt = "Enter new password (8-16 chars)";
  display.setCursor((SCREEN_WIDTH - strlen(prompt) * 6) / 2, 105);
  display.print(prompt);

  // Input box
  int boxX = 30;
  int boxY = 120;
  int boxW = SCREEN_WIDTH - 60;
  int boxH = 50;

  display.fillRoundRect(boxX, boxY, boxW, boxH, 8, 0x1082); // Dark blue fill
  display.drawRoundRect(boxX, boxY, boxW, boxH, 8, 0x34BF); // Light blue outline

  // Display password input as asterisks
  display.setFont(nullptr);  // Use default font
  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(1);

  size_t inputLen = strlen(webPasswordInput);
  char maskedPassword[WEB_PASSWORD_MAX_LEN];
  for (size_t i = 0; i < inputLen && i < sizeof(maskedPassword) - 1; i++) {
    maskedPassword[i] = '*';
  }
  maskedPassword[inputLen < sizeof(maskedPassword) - 1 ? inputLen : sizeof(maskedPassword) - 1] = '\0';

  getTextBounds_compat(display, maskedPassword, 0, 0, &x1, &y1, &w, &h);
  int textX = boxX + 15;
  int textY = boxY + (boxH / 2) + (h / 2) + 5;
  display.setCursor(textX, textY);
  display.print(maskedPassword);

  // Blinking cursor
  if (passwordCursorVisible) {
    int cursorX = textX + w + 5;
    if (cursorX < boxX + boxW - 10) {
      display.fillRect(cursorX, textY - h, 3, h + 5, COLOR_WARNING);
    }
  }

  display.setFont(nullptr); // Reset font

  // Username hint
  display.setTextSize(1);
  display.setTextColor(0x7BEF); // Light gray
  const char* usernameHint = "Username: admin";
  display.setCursor((SCREEN_WIDTH - strlen(usernameHint) * 6) / 2, 195);
  display.print(usernameHint);

  // Footer with controls
  display.setTextSize(1);
  display.setTextColor(COLOR_WARNING);
  const char* footerText = "ENTER Save";
  display.setCursor(40, SCREEN_HEIGHT - 15);
  display.print(footerText);

  display.setTextColor(ST77XX_RED);
  const char* clearText = "DEL Disable";
  display.setCursor(130, SCREEN_HEIGHT - 15);
  display.print(clearText);

  display.setTextColor(0x7BEF);
  const char* escText = "ESC Cancel";
  display.setCursor(230, SCREEN_HEIGHT - 15);
  display.print(escText);

  // Password strength indicator
  if (inputLen > 0) {
    int strength = 0;
    if (inputLen >= 8) strength++;
    if (inputLen >= 12) strength++;

    bool hasUpper = false, hasLower = false, hasDigit = false;
    for (size_t i = 0; i < inputLen; i++) {
      if (isupper(webPasswordInput[i])) hasUpper = true;
      if (islower(webPasswordInput[i])) hasLower = true;
      if (isdigit(webPasswordInput[i])) hasDigit = true;
    }
    if (hasUpper && hasLower) strength++;
    if (hasDigit) strength++;

    uint16_t strengthColor;
    const char* strengthText;
    if (strength <= 1) {
      strengthColor = ST77XX_RED;
      strengthText = "Weak";
    } else if (strength <= 2) {
      strengthColor = COLOR_WARNING;
      strengthText = "Fair";
    } else if (strength <= 3) {
      strengthColor = ST77XX_YELLOW;
      strengthText = "Good";
    } else {
      strengthColor = ST77XX_GREEN;
      strengthText = "Strong";
    }

    display.setTextSize(1);
    display.setTextColor(strengthColor);
    display.setCursor((SCREEN_WIDTH - strlen(strengthText) * 6) / 2, 180);
    display.print(strengthText);
  }
}

// Handle web password input
int handleWebPasswordInput(char key, LGFX &display) {
  // Handle confirmation state first
  if (passwordState == PASSWORD_STATE_CONFIRM_DISABLE) {
    if (key == KEY_ENTER || key == KEY_ENTER_ALT) {
      // Confirm disable
      clearWebPassword();
      webPassword[0] = '\0';
      webAuthEnabled = false;
      beep(TONE_SELECT, BEEP_MEDIUM);

      display.fillRect(0, 42, SCREEN_WIDTH, SCREEN_HEIGHT - 42, COLOR_BACKGROUND);
      display.setTextSize(2);
      display.setTextColor(ST77XX_YELLOW);
      display.setCursor(45, 110);
      display.print("Password Disabled");
      display.setTextSize(1);
      display.setTextColor(0x7BEF);
      display.setCursor(55, 135);
      display.print("Web access now open");
      delay(2000);

      return -1;  // Exit settings
    } else if (key == KEY_ESC) {
      // Cancel disable, return to normal state
      beep(TONE_MENU_NAV, BEEP_SHORT);
      passwordState = PASSWORD_STATE_NORMAL;
      drawWebPasswordUI(display);
      return 0;
    }
    return 0;  // Ignore other keys in confirmation state
  }

  // Normal state handling
  // Update cursor blink
  if (millis() - passwordLastBlink > 500) {
    passwordCursorVisible = !passwordCursorVisible;
    passwordLastBlink = millis();
    drawWebPasswordUI(display);
  }

  size_t inputLen = strlen(webPasswordInput);

  if (key == KEY_BACKSPACE || key == 0x7F) {
    // Backspace or DEL key - remove last character
    if (inputLen > 0) {
      webPasswordInput[inputLen - 1] = '\0';
      passwordCursorVisible = true;
      passwordLastBlink = millis();
      drawWebPasswordUI(display);
    }
    return 1;
  }
  else if (key == KEY_ENTER || key == KEY_ENTER_ALT) {
    // Save password or show disable confirmation if empty
    if (inputLen == 0) {
      // Empty password - show disable confirmation
      passwordState = PASSWORD_STATE_CONFIRM_DISABLE;
      beep(TONE_MENU_NAV, BEEP_SHORT);

      display.fillRect(0, 42, SCREEN_WIDTH, SCREEN_HEIGHT - 42, COLOR_BACKGROUND);
      display.setTextSize(2);
      display.setTextColor(COLOR_WARNING);
      display.setCursor(65, 100);
      display.print("Disable Web");
      display.setCursor(75, 125);
      display.print("Password?");

      display.setTextSize(1);
      display.setTextColor(ST77XX_WHITE);
      display.setCursor(70, 155);
      display.print("ENTER Confirm  ESC Cancel");

      return 0;
    }
    else if (inputLen >= 8 && inputLen <= 16) {
      // Valid password length - save it
      saveWebPassword(webPasswordInput);
      strncpy(webPassword, webPasswordInput, sizeof(webPassword) - 1);
      webPassword[sizeof(webPassword) - 1] = '\0';
      webAuthEnabled = true;
      beep(TONE_SELECT, BEEP_MEDIUM);

      // Show success message
      display.fillRect(0, 42, SCREEN_WIDTH, SCREEN_HEIGHT - 42, COLOR_BACKGROUND);
      display.setTextSize(2);
      display.setTextColor(ST77XX_GREEN);
      display.setCursor(55, 100);
      display.print("Password Set!");
      display.setTextSize(1);
      display.setTextColor(ST77XX_WHITE);
      display.setCursor(45, 125);
      display.print("Web access now protected");

      display.setTextColor(ST77XX_YELLOW);
      display.setCursor(65, 150);
      display.print("Login credentials:");
      display.setTextColor(0x7BEF); // Light gray
      display.setCursor(75, 170);
      display.print("Username: admin");
      display.setCursor(75, 185);
      display.print("Password: (your password)");

      delay(4000);

      return -1;  // Exit settings
    } else {
      // Too short or too long (1-7 characters)
      beep(TONE_ERROR, BEEP_MEDIUM);
      display.fillRect(0, 185, SCREEN_WIDTH, 20, COLOR_BACKGROUND);
      display.setTextSize(1);
      display.setTextColor(ST77XX_RED);
      const char* errorMsg = "Must be 8-16 characters";
      display.setCursor((SCREEN_WIDTH - strlen(errorMsg) * 6) / 2, 195);
      display.print(errorMsg);
      delay(1500);
      drawWebPasswordUI(display);
    }
    return 0;
  }
  else if (key == KEY_ESC) {
    // Cancel without saving
    beep(TONE_MENU_NAV, BEEP_SHORT);
    return -1;  // Exit settings
  }
  else if (key >= 32 && key <= 126 && inputLen < 16) {
    // Add printable character (max 16 chars for password)
    // Accept alphanumeric and special characters
    webPasswordInput[inputLen] = key;
    webPasswordInput[inputLen + 1] = '\0';
    passwordCursorVisible = true;
    passwordLastBlink = millis();
    beep(TONE_MENU_NAV, BEEP_SHORT);
    drawWebPasswordUI(display);
    return 1;
  }

  return 0;
}

// Save web password to flash memory
void saveWebPassword(const char* password) {
  Serial.printf("[WebPW] Saving password: '%s' (len=%d)\n", password, strlen(password));
  webPasswordPrefs.begin("webpw", false);
  webPasswordPrefs.putString("pw", password);
  webPasswordPrefs.putBool("enabled", true);
  webPasswordPrefs.end();

  Serial.println("[WebPW] Password saved and enabled");
}

// Clear/disable web password
void clearWebPassword() {
  webPasswordPrefs.begin("webpw", false);
  webPasswordPrefs.remove("pw");
  webPasswordPrefs.putBool("enabled", false);
  webPasswordPrefs.end();

  Serial.println("Web password disabled");
}

// Load web password from flash memory
bool loadWebPassword(char* password, size_t len) {
  webPasswordPrefs.begin("webpw", true);
  bool enabled = webPasswordPrefs.getBool("enabled", false);
  String val = webPasswordPrefs.getString("pw", "");
  webPasswordPrefs.end();

  strncpy(password, val.c_str(), len - 1);
  password[len - 1] = '\0';

  return (enabled && strlen(password) > 0);
}

// Load web password on startup (call from setup())
void loadSavedWebPassword() {
  char savedPassword[WEB_PASSWORD_MAX_LEN];
  if (loadWebPassword(savedPassword, sizeof(savedPassword))) {
    strncpy(webPassword, savedPassword, sizeof(webPassword) - 1);
    webPassword[sizeof(webPassword) - 1] = '\0';
    webAuthEnabled = true;
    Serial.printf("[WebPW] Loaded password: '%s' (len=%d)\n", savedPassword, strlen(savedPassword));
    Serial.println("[WebPW] Web password protection enabled");
  } else {
    webPassword[0] = '\0';
    webAuthEnabled = false;
    Serial.println("[WebPW] Web password protection disabled (no saved password)");
  }
}

#endif // SETTINGS_WEB_PASSWORD_H
