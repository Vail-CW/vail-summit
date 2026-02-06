/*
 * Menu Navigation Module
 * Legacy input routing - most functionality replaced by LVGL mode integration.
 * Retained: enterDeepSleep() for power management.
 */

#ifndef MENU_NAVIGATION_H
#define MENU_NAVIGATION_H

#include <WiFi.h>
#include <esp_sleep.h>
#include "../core/config.h"
#include "../audio/i2s_audio.h"
#include "menu_ui.h"

// Forward declarations from main file
extern LGFX tft;

/*
 * Enter deep sleep mode with wake on DIT paddle
 */
void enterDeepSleep() {
  Serial.println("Entering deep sleep...");

  // Disconnect WiFi if connected
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }

  // Show sleep message
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);

  tft.setCursor(40, 110);
  tft.print("Going to");
  tft.setCursor(50, 140);
  tft.print("Sleep...");

  tft.setFont(nullptr);
  tft.setTextSize(1);
  tft.setTextColor(0x7BEF);
  tft.setCursor(30, 180);
  tft.print("Press DIT paddle to wake");

  delay(2000);

  // Turn off display
  tft.fillScreen(ST77XX_BLACK);

  // Turn off backlight (GPIO 39)
  digitalWrite(TFT_BL, LOW);  // LOW = backlight OFF (active HIGH logic)
  Serial.println("Backlight turned off for deep sleep");

  // Configure wake on DIT paddle press (active LOW)
  esp_sleep_enable_ext0_wakeup((gpio_num_t)DIT_PIN, LOW);

  // Enter deep sleep
  esp_deep_sleep_start();
  // Device will wake here and restart from setup()
}

#endif // MENU_NAVIGATION_H
