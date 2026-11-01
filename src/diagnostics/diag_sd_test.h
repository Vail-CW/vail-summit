/*
 * SD Card Wiring and Card Tests (self check)
 *
 * The SD slot sits on the back of the display board and reaches the Feather
 * through its own 4 header joints (CS 38, MOSI 35, SCK 36, MISO 37). A working
 * screen proves the display header, not these. Two tests:
 *
 *  diagSDWiringTest() - works with an EMPTY slot. Briefly takes the 4 pins off
 *    the SPI bus and checks for solder bridges between them, shorts to GND,
 *    and which pins see a pull-up on the display board (a pin that should see
 *    one and doesn't points at an open joint).
 *
 *  diagSDCardProbe() - needs a card. Talks the SD protocol directly so the
 *    result says WHERE it failed: no response at all (wiring / no card), a
 *    garbled response (MISO fault), init OK but reads failing at higher clock
 *    speeds (marginal joint), or all good (any mount failure is then the card's
 *    format, not the hardware).
 *
 * Both run synchronously between LVGL frames (no flush can be in flight) and
 * take well under a second each.
 */

#ifndef DIAG_SD_TEST_H
#define DIAG_SD_TEST_H

#include <SPI.h>
#include <SD.h>
#include "../core/config.h"
#include "../storage/sd_card.h"

extern LGFX tft;

#define DIAG_SD_PIN_COUNT 4
static const uint8_t diagSdPins[DIAG_SD_PIN_COUNT] = { SD_CS, SD_MOSI, SD_SCK, SD_MISO };
static const char* const diagSdPinNames[DIAG_SD_PIN_COUNT] = { "CS", "MOSI", "SCK", "MISO" };

struct DiagSDWiring {
  uint8_t pullupMask;     // bit i: pin i read HIGH against the internal pull-down
  uint8_t shortGndMask;   // bit i: pin i read LOW against the internal pull-up
  uint8_t bridgeMask;     // bit i: pin i follows another SD pin
  char bridges[48];       // e.g. "CS-MOSI"
};

struct DiagSDProbe {
  bool responded;         // CMD0 got anything but 0xFF
  bool garbled;           // CMD0 response was not a valid idle reply
  uint8_t cmd0;
  bool v2;                // answered CMD8 (SD v2 / SDHC capable)
  bool initOk;            // ACMD41 completed
  bool sdhc;
  uint8_t speedOkMask;    // bit per diagSdSpeeds[] entry: block 0 read with good CRC
  uint8_t speedTriedMask;
};

static const uint32_t diagSdSpeeds[] = { 400000, 1000000, 4000000, 10000000 };
#define DIAG_SD_SPEED_COUNT (sizeof(diagSdSpeeds) / sizeof(diagSdSpeeds[0]))

// Release the shared bus: unmount SD, drop the Arduino SPI pins and the
// display's bus so the pins can be driven as plain GPIO.
static void diagSDReleaseBus() {
  if (sdCardAvailable) SD.end();
  sdCardAvailable = false;
  SPI.end();
  tft.releaseBus();
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);   // keep the display deselected throughout
}

static void diagSDRestoreBus() {
  for (int i = 0; i < DIAG_SD_PIN_COUNT; i++) pinMode(diagSdPins[i], INPUT);
  tft.initBus();
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
}

static inline bool diagReadPin(uint8_t pin) {
  delayMicroseconds(50);
  return digitalRead(pin) == HIGH;
}

void diagSDWiringTest(DiagSDWiring* out) {
  memset(out, 0, sizeof(*out));
  diagSDReleaseBus();

  for (int i = 0; i < DIAG_SD_PIN_COUNT; i++) {
    uint8_t p = diagSdPins[i];
    pinMode(p, INPUT_PULLUP);
    if (!diagReadPin(p)) out->shortGndMask |= (1 << i);
    pinMode(p, INPUT_PULLDOWN);
    if (diagReadPin(p)) out->pullupMask |= (1 << i);
    pinMode(p, INPUT);
  }

  // Bridge test: drive one pin low then high; a neighbor that follows it is
  // connected to it. Works whether or not the neighbor has a board pull-up.
  for (int i = 0; i < DIAG_SD_PIN_COUNT; i++) {
    for (int j = 0; j < DIAG_SD_PIN_COUNT; j++) {
      if (j != i) pinMode(diagSdPins[j], INPUT_PULLDOWN);
    }
    pinMode(diagSdPins[i], OUTPUT);
    digitalWrite(diagSdPins[i], LOW);
    bool low[DIAG_SD_PIN_COUNT];
    for (int j = 0; j < DIAG_SD_PIN_COUNT; j++) low[j] = (j != i) && diagReadPin(diagSdPins[j]);
    digitalWrite(diagSdPins[i], HIGH);
    for (int j = i + 1; j < DIAG_SD_PIN_COUNT; j++) {
      bool high = diagReadPin(diagSdPins[j]);
      if (!low[j] && high) {
        out->bridgeMask |= (1 << i) | (1 << j);
        size_t used = strlen(out->bridges);
        snprintf(out->bridges + used, sizeof(out->bridges) - used, "%s%s-%s",
                 used ? " " : "", diagSdPinNames[i], diagSdPinNames[j]);
      }
    }
    pinMode(diagSdPins[i], INPUT);
  }

  diagSDRestoreBus();
}

// ---- raw SD protocol over SPI -------------------------------------------------

static uint8_t diagSdCmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
  // Wait for the card to stop signalling busy (bounded)
  for (int i = 0; i < 200 && SPI.transfer(0xFF) != 0xFF; i++) {}
  SPI.transfer(0x40 | cmd);
  SPI.transfer((uint8_t)(arg >> 24));
  SPI.transfer((uint8_t)(arg >> 16));
  SPI.transfer((uint8_t)(arg >> 8));
  SPI.transfer((uint8_t)arg);
  SPI.transfer(crc);
  uint8_t r = 0xFF;
  for (int i = 0; i < 10; i++) {
    r = SPI.transfer(0xFF);
    if (!(r & 0x80)) break;
  }
  return r;
}

static inline void diagSdSelect(bool on) {
  digitalWrite(SD_CS, on ? LOW : HIGH);
  SPI.transfer(0xFF);
}

static uint16_t diagCrc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
  }
  return crc;
}

// Read block 0 and verify the card's data CRC
static bool diagSdReadBlock0(uint8_t* buf) {
  diagSdSelect(true);
  bool ok = false;
  if (diagSdCmd(17, 0, 0xFF) == 0x00) {
    unsigned long start = millis();
    uint8_t token = 0xFF;
    while (millis() - start < 150 && (token = SPI.transfer(0xFF)) == 0xFF) {}
    if (token == 0xFE) {
      for (int i = 0; i < 512; i++) buf[i] = SPI.transfer(0xFF);
      uint16_t crc = ((uint16_t)SPI.transfer(0xFF) << 8) | SPI.transfer(0xFF);
      ok = (crc == diagCrc16(buf, 512));
    }
  }
  diagSdSelect(false);
  return ok;
}

void diagSDCardProbe(DiagSDProbe* out) {
  memset(out, 0, sizeof(*out));
  if (sdCardAvailable) SD.end();
  sdCardAvailable = false;

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, -1);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));

  // 80+ clocks with CS high puts the card in SPI mode
  for (int i = 0; i < 10; i++) SPI.transfer(0xFF);

  uint8_t r = 0xFF;
  for (int attempt = 0; attempt < 5 && r != 0x01; attempt++) {
    diagSdSelect(true);
    r = diagSdCmd(0, 0, 0x95);
    diagSdSelect(false);
  }
  out->cmd0 = r;
  out->responded = (r != 0xFF);
  out->garbled = out->responded && r != 0x01;

  if (r == 0x01) {
    diagSdSelect(true);
    r = diagSdCmd(8, 0x1AA, 0x87);
    if (r == 0x01) {
      uint8_t echo[4];
      for (int i = 0; i < 4; i++) echo[i] = SPI.transfer(0xFF);
      out->v2 = (echo[3] == 0xAA);
    }
    diagSdSelect(false);

    unsigned long start = millis();
    do {
      diagSdSelect(true);
      diagSdCmd(55, 0, 0x65);
      r = diagSdCmd(41, out->v2 ? 0x40000000UL : 0, 0x77);
      diagSdSelect(false);
    } while (r == 0x01 && millis() - start < 1000);
    out->initOk = (r == 0x00);

    if (out->initOk && out->v2) {
      diagSdSelect(true);
      if (diagSdCmd(58, 0, 0xFD) == 0x00) {
        uint8_t ocr[4];
        for (int i = 0; i < 4; i++) ocr[i] = SPI.transfer(0xFF);
        out->sdhc = (ocr[0] & 0x40) != 0;
      }
      diagSdSelect(false);
    }
  }
  SPI.endTransaction();

  if (out->initOk) {
    uint8_t* buf = (uint8_t*)malloc(512);
    if (buf) {
      for (size_t s = 0; s < DIAG_SD_SPEED_COUNT; s++) {
        SPI.beginTransaction(SPISettings(diagSdSpeeds[s], MSBFIRST, SPI_MODE0));
        out->speedTriedMask |= (1 << s);
        if (diagSdReadBlock0(buf)) out->speedOkMask |= (1 << s);
        SPI.endTransaction();
      }
      free(buf);
    }
  }
}

#endif // DIAG_SD_TEST_H
