/*
 * Status Bar Module
 * Handles battery and WiFi status monitoring and display
 */

#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <Adafruit_LC709203F.h>
#include <Adafruit_MAX1704X.h>
#include <WiFi.h>
#include "../core/config.h"
#include "../network/internet_check.h"

// Forward declarations from main file
extern LGFX tft;
extern Adafruit_LC709203F lc;
extern Adafruit_MAX17048 maxlipo;
extern bool hasLC709203;
extern bool hasMAX17048;
extern bool hasBatteryMonitor;

// Status tracking
bool wifiConnected = false;
int batteryPercent = 100;
bool isCharging = false;

/*
 * Update WiFi and battery status from hardware
 */
void updateStatus() {
  // Update internet connectivity status (handles timing internally)
  updateInternetStatus();

  // Update WiFi status based on internet check result
  wifiConnected = (getInternetStatus() == INET_CONNECTED);

  // Read battery voltage and percentage from I2C battery monitor
  float voltage = 3.7; // Default
  batteryPercent = 50;

  if (hasLC709203) {
    voltage = lc.cellVoltage();
    batteryPercent = (int)lc.cellPercent();
  }
  else if (hasMAX17048) {
    voltage = maxlipo.cellVoltage();
    batteryPercent = (int)maxlipo.cellPercent();
  }
  else {
    // No battery monitor - show placeholder values
    voltage = 3.7;
    batteryPercent = 50;
  }

  // Validate readings
  if (voltage < 2.5 || voltage > 5.0) {
    voltage = 3.7;
  }

  // Constrain to 0-100%
  if (batteryPercent > 100) batteryPercent = 100;
  if (batteryPercent < 0) batteryPercent = 0;

  // USB detection DISABLED - A3 conflicts with I2S_LCK_PIN (GPIO 15)
  // Cannot use analogRead on GPIO 15 or it breaks I2S audio!
  // Assume not charging for now (could use battery voltage trend instead)
  isCharging = false;

  // Debug output
  if (DEBUG_ENABLED) {
    Serial.print("Battery: ");
    Serial.print(voltage);
    Serial.print("V (");
    Serial.print(batteryPercent);
    Serial.print("%) ");
    Serial.print(isCharging ? "CHARGING" : "");
    Serial.print(" | WiFi: ");
    Serial.println(wifiConnected ? "Connected" : "Disconnected");
  }
}

#endif // STATUS_BAR_H
