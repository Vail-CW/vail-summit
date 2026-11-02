// Which display init command makes the display load MISO? Sends the
// LovyanGFX ST7796 init list one command at a time over plain SPI and tests
// the SD mount (3 tries) after each step. Two passes, hardware reset between.
#include <SPI.h>
#include <SD.h>

#define SD_CS 38
#define SD_MOSI 35
#define SD_SCK 36
#define SD_MISO 37
#define TFT_CS 10
#define TFT_RST 11
#define TFT_DC 12
#define TFT_BL 39

struct Step { const char* name; uint8_t cmd; uint8_t len; uint8_t data[14]; uint16_t waitMs; };

static const Step steps[] = {
  { "SWRESET 01",        0x01, 0, {}, 150 },
  { "CSCON C3",          0xF0, 1, { 0xC3 }, 0 },
  { "CSCON 96",          0xF0, 1, { 0x96 }, 0 },
  { "INVCTR B4",         0xB4, 1, { 0x01 }, 0 },
  { "DFUNCTR B6",        0xB6, 3, { 0x80, 0x22, 0x3B }, 0 },
  { "DOCA E8",           0xE8, 8, { 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33 }, 0 },
  { "PWCTR2 C1",         0xC1, 1, { 0x06 }, 0 },
  { "PWCTR3 C2",         0xC2, 1, { 0xA7 }, 0 },
  { "VMCTR C5",          0xC5, 1, { 0x18 }, 120 },
  { "GMCTRP1 E0",        0xE0, 14, { 0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B }, 0 },
  { "GMCTRN1 E1",        0xE1, 14, { 0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B, 0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B }, 120 },
  { "CSCON 3C",          0xF0, 1, { 0x3C }, 0 },
  { "CSCON 69",          0xF0, 1, { 0x69 }, 0 },
  { "SLPOUT 11",         0x11, 0, {}, 130 },
  { "IDMOFF 38",         0x38, 0, {}, 0 },
  { "DISPON 29",         0x29, 0, {}, 20 },
  { "COLMOD 3A 55",      0x3A, 1, { 0x55 }, 0 },
  { "MADCTL 36",         0x36, 1, { 0xE8 }, 0 },
  { "SLPIN 10",          0x10, 0, {}, 150 },
  { "SLPOUT 11 again",   0x11, 0, {}, 130 },
};

static void tftCmd(const Step& s) {
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TFT_DC, LOW);
  digitalWrite(TFT_CS, LOW);
  SPI.transfer(s.cmd);
  digitalWrite(TFT_DC, HIGH);
  for (int i = 0; i < s.len; i++) SPI.transfer(s.data[i]);
  digitalWrite(TFT_CS, HIGH);
  SPI.endTransaction();
  if (s.waitMs) delay(s.waitMs);
}

static uint8_t idleByte() {
  SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
  digitalWrite(SD_CS, HIGH);
  uint8_t h = SPI.transfer(0xFF);
  SPI.endTransaction();
  return h;
}

static void test(const char* label) {
  int ok = 0;
  for (int i = 0; i < 3; i++) {
    SD.end();
    if (SD.begin(SD_CS, SPI, 4000000, "/sd", 5, false)) ok++;
  }
  SD.end();
  Serial.printf("  %-18s idle %02X  mount %d/3\n", label, idleByte(), ok);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  ledcAttach(TFT_BL, 5000, 8); ledcWrite(TFT_BL, 255);
  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  pinMode(TFT_DC, OUTPUT); digitalWrite(TFT_DC, HIGH);
  pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH);
  pinMode(TFT_RST, OUTPUT);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, -1);
  Serial.println("\n=== sd_initbisect ===");

  for (int pass = 1; pass <= 2; pass++) {
    Serial.printf("pass %d\n", pass);
    digitalWrite(TFT_RST, LOW); delay(20);
    test("in reset");
    digitalWrite(TFT_RST, HIGH); delay(150);
    test("out of reset");
    for (const Step& s : steps) { tftCmd(s); test(s.name); }
  }
  Serial.println("=== done ===");
}

void loop() { delay(1000); }
