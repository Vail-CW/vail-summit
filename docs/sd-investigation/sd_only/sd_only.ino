// Minimal SD test for the Summit hardware: no display driver, no LVGL, no
// Summit firmware. Display is held deselected. Repeats every 5 s.
#include <SPI.h>
#include <SD.h>

#define SD_CS   38
#define SD_MOSI 35
#define SD_SCK  36
#define SD_MISO 37
#define TFT_CS  10
#define TFT_RST 11

void setup() {
  Serial.begin(115200);
  delay(1500);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, -1);
}

void tryCard(uint32_t hz) {
  Serial.printf("\n--- SD.begin at %lu Hz ---\n", (unsigned long)hz);
  SD.end();
  if (!SD.begin(SD_CS, SPI, hz, "/sd", 5, false)) {
    Serial.println("RESULT: mount FAILED");
    return;
  }
  Serial.printf("type %d, size %llu MB\n", SD.cardType(), SD.cardSize() / (1024 * 1024));
  File f = SD.open("/sdtest.txt", FILE_WRITE);
  bool wrote = f && f.print("hello summit") > 0;
  if (f) f.close();
  f = SD.open("/sdtest.txt");
  String back = f ? f.readString() : "";
  if (f) f.close();
  Serial.printf("RESULT: mount OK, write %s, read back \"%s\"\n", wrote ? "OK" : "FAILED", back.c_str());
}

void loop() {
  tryCard(400000);
  tryCard(4000000);
  delay(5000);
}
