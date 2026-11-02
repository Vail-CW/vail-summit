// What does MISO look like with the card selected, before and after a raw
// start-up, and does the SD library mount after it?
#include <SPI.h>
#include <SD.h>

#define SD_CS 38
#define SD_MOSI 35
#define SD_SCK 36
#define SD_MISO 37
#define TFT_CS 10

static void idle(const char* when) {
  SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
  digitalWrite(SD_CS, HIGH);
  uint8_t h[4]; for (int i = 0; i < 4; i++) h[i] = SPI.transfer(0xFF);
  digitalWrite(SD_CS, LOW);
  uint8_t l[8]; for (int i = 0; i < 8; i++) l[i] = SPI.transfer(0xFF);
  digitalWrite(SD_CS, HIGH);
  SPI.transfer(0xFF);
  SPI.endTransaction();
  Serial.printf("%-14s CS high: %02X%02X%02X%02X   CS low: %02X%02X%02X%02X%02X%02X%02X%02X\n", when,
                h[0], h[1], h[2], h[3], l[0], l[1], l[2], l[3], l[4], l[5], l[6], l[7]);
}

static uint8_t cmd(uint8_t c, uint32_t a, uint8_t crc) {
  SPI.transfer(0xFF);
  SPI.transfer(0x40 | c);
  SPI.transfer(a >> 24); SPI.transfer(a >> 16); SPI.transfer(a >> 8); SPI.transfer(a);
  SPI.transfer(crc);
  uint8_t r = 0xFF;
  for (int i = 0; i < 16; i++) { r = SPI.transfer(0xFF); if (!(r & 0x80)) break; }
  return r;
}

static void wake() {
  SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
  digitalWrite(SD_CS, HIGH);
  for (int i = 0; i < 20; i++) SPI.transfer(0xFF);
  uint8_t r0 = 0; int t0 = 0;
  for (; t0 < 10 && r0 != 1; t0++) { digitalWrite(SD_CS, LOW); r0 = cmd(0, 0, 0x95); digitalWrite(SD_CS, HIGH); SPI.transfer(0xFF); }
  digitalWrite(SD_CS, LOW); uint8_t r8 = cmd(8, 0x1AA, 0x87); for (int i = 0; i < 4; i++) SPI.transfer(0xFF); digitalWrite(SD_CS, HIGH); SPI.transfer(0xFF);
  uint8_t r41; int n = 0; unsigned long st = millis();
  do { digitalWrite(SD_CS, LOW); cmd(55, 0, 0x65); r41 = cmd(41, 0x40000000UL, 0x77); digitalWrite(SD_CS, HIGH); SPI.transfer(0xFF); n++; }
  while (r41 == 1 && millis() - st < 1500);
  SPI.endTransaction();
  Serial.printf("wake: cmd0 %02X after %d, cmd8 %02X, acmd41 %02X after %d\n", r0, t0, r8, r41, n);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, -1);
  Serial.println("\n=== sd_wake ===");
  idle("start");
  bool ok = SD.begin(SD_CS, SPI, 4000000, "/sd", 5, false);
  Serial.printf("library first try: %s\n", ok ? "MOUNT OK" : "FAIL");
  SD.end();
  idle("after lib");
  wake();
  idle("after wake");
  ok = SD.begin(SD_CS, SPI, 4000000, "/sd", 5, false);
  Serial.printf("library after wake: %s\n", ok ? "MOUNT OK" : "FAIL");
  SD.end();
  idle("end");
  Serial.println("=== done ===");
}

void loop() { delay(1000); }
