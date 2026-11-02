// Does sleeping the display around SD.begin() give a mount that keeps
// working with the display awake? Starts from a known state every run:
// display held in reset and a raw card init, so earlier runs can't leak in.
#include <SPI.h>
#include <SD.h>
#include <LovyanGFX.hpp>

#define SD_CS 38
#define SD_MOSI 35
#define SD_SCK 36
#define SD_MISO 37
#define TFT_CS 10
#define TFT_RST 11
#define TFT_DC 12
#define TFT_BL 39

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 _panel;
  lgfx::Bus_SPI _bus;
public:
  LGFX() {
    auto b = _bus.config();
    b.spi_host = SPI2_HOST; b.spi_mode = 0;
    b.freq_write = 40000000; b.freq_read = 16000000;
    b.spi_3wire = false; b.use_lock = true; b.dma_channel = SPI_DMA_CH_AUTO;
    b.pin_sclk = SD_SCK; b.pin_mosi = SD_MOSI; b.pin_miso = SD_MISO; b.pin_dc = TFT_DC;
    _bus.config(b);
    _panel.setBus(&_bus);
    auto p = _panel.config();
    p.pin_cs = TFT_CS; p.pin_rst = TFT_RST; p.pin_busy = -1;
    p.memory_width = 320; p.memory_height = 480; p.panel_width = 320; p.panel_height = 480;
    p.dummy_read_pixel = 8; p.dummy_read_bits = 1; p.readable = true;
    p.invert = false; p.rgb_order = false; p.dlen_16bit = false; p.bus_shared = true;
    _panel.config(p);
    setPanel(&_panel);
  }
};
LGFX tft;

static uint8_t idleByte() {
  SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
  digitalWrite(SD_CS, HIGH);
  uint8_t h = SPI.transfer(0xFF);
  SPI.endTransaction();
  return h;
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

// Raw CMD0 / CMD8 / ACMD41 so the card starts every run in the same state
static void rawCardInit() {
  SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
  digitalWrite(SD_CS, HIGH);
  for (int i = 0; i < 20; i++) SPI.transfer(0xFF);
  uint8_t r0 = 0;
  for (int t = 0; t < 10 && r0 != 1; t++) { digitalWrite(SD_CS, LOW); r0 = cmd(0, 0, 0x95); digitalWrite(SD_CS, HIGH); SPI.transfer(0xFF); }
  digitalWrite(SD_CS, LOW); cmd(8, 0x1AA, 0x87); for (int i = 0; i < 4; i++) SPI.transfer(0xFF); digitalWrite(SD_CS, HIGH); SPI.transfer(0xFF);
  uint8_t r41; unsigned long st = millis();
  do { digitalWrite(SD_CS, LOW); cmd(55, 0, 0x65); r41 = cmd(41, 0x40000000UL, 0x77); digitalWrite(SD_CS, HIGH); SPI.transfer(0xFF); }
  while (r41 == 1 && millis() - st < 1500);
  SPI.endTransaction();
  Serial.printf("raw init: cmd0 %02X acmd41 %02X\n", r0, r41);
}

static void displayCmd(uint8_t c, int wait) {
  tft.startWrite();
  tft.writeCommand(c);
  tft.endWrite();
  delay(wait);
}

static bool tryMount(const char* label) {
  SD.end();
  bool ok = SD.begin(SD_CS, SPI, 4000000, "/sd", 5, false);
  Serial.printf("  %-34s idle %02X  mount %s\n", label, idleByte(), ok ? "OK" : "FAIL");
  return ok;
}

static bool sleepMount(const char* label) {
  SD.end();
  displayCmd(0x10, 10);
  bool ok = SD.begin(SD_CS, SPI, 4000000, "/sd", 5, false);
  displayCmd(0x11, 120);
  Serial.printf("  %-34s mount %s\n", label, ok ? "OK" : "FAIL");
  return ok;
}

// Write, read back, list, card info; draw on the display between rounds
static int ioRounds(const char* label, int rounds, bool sleepEach) {
  static uint8_t wbuf[2048], rbuf[2048];
  int good = 0;
  for (int r = 0; r < rounds; r++) {
    for (int i = 0; i < (int)sizeof(wbuf); i++) wbuf[i] = (uint8_t)(i * 7 + r);
    if (sleepEach) displayCmd(0x10, 5);
    bool ok = false;
    File f = SD.open("/sdfix.bin", FILE_WRITE);
    if (f) { ok = f.write(wbuf, sizeof(wbuf)) == sizeof(wbuf); f.close(); }
    if (ok) {
      f = SD.open("/sdfix.bin", FILE_READ);
      ok = f && f.read(rbuf, sizeof(rbuf)) == (int)sizeof(rbuf) && memcmp(wbuf, rbuf, sizeof(wbuf)) == 0;
      if (f) f.close();
    }
    if (ok) {
      File root = SD.open("/");
      int n = 0;
      if (root) { File e; while ((e = root.openNextFile())) { n++; e.close(); } root.close(); }
      ok = n > 0;
    }
    if (sleepEach) displayCmd(0x11, 120);
    if (ok) good++;
    tft.fillRect(0, 0, 480, 40, r & 1 ? TFT_NAVY : TFT_DARKGREEN);
    tft.setCursor(8, 12); tft.setTextColor(TFT_WHITE); tft.printf("%s %d/%d", label, good, r + 1);
  }
  Serial.printf("  %-34s %d/%d rounds OK\n", label, good, rounds);
  return good;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  ledcAttach(TFT_BL, 5000, 8); ledcWrite(TFT_BL, 255);
  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH);
  pinMode(TFT_RST, OUTPUT);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, -1);
  Serial.println("\n=== sd_fix ===");

  // Known state
  digitalWrite(TFT_RST, LOW); delay(20);
  rawCardInit();
  Serial.println("display in reset:");
  tryMount("baseline mount");
  SD.end();
  digitalWrite(TFT_RST, HIGH); delay(150);

  tft.init(); tft.setRotation(3); tft.fillScreen(TFT_BLACK);
  Serial.println("display awake:");
  tryMount("plain mount");
  pinMode(TFT_DC, OUTPUT);
  digitalWrite(TFT_DC, LOW);  tryMount("plain mount, DC low");
  digitalWrite(TFT_DC, HIGH); tryMount("plain mount, DC high");

  // Long sleep settle, in case 10 ms was too short
  SD.end();
  displayCmd(0x10, 150);
  Serial.printf("  %-34s idle %02X  mount %s\n", "sleep 150ms mount",
                idleByte(), SD.begin(SD_CS, SPI, 4000000, "/sd", 5, false) ? "OK" : "FAIL");
  displayCmd(0x11, 120);

  // Interface Mode Control: SDA_EN (bit 7) = 3-wire, DOUT not used
  Serial.println("0xB0 SDA_EN, display awake:");
  tft.startWrite(); tft.writeCommand(0xB0); tft.writeData(0x80); tft.endWrite();
  delay(10);
  tft.fillScreen(TFT_DARKGREY);
  if (tryMount("SDA_EN mount")) {
    ioRounds("SDA_EN I/O awake", 20, false);
    int ok = 0;
    for (int i = 0; i < 5; i++) { ok += tryMount("SDA_EN remount") && ioRounds("  then I/O", 3, false) == 3; }
    Serial.printf("SDA_EN remount + I/O passes: %d/5\n", ok);
  }
  tft.startWrite(); tft.writeCommand(0xB0); tft.writeData(0x00); tft.endWrite();
  delay(10);
  tryMount("SDA_EN cleared again");

  Serial.println("sleep around mount, I/O awake:");
  if (sleepMount("sleep mount")) {
    ioRounds("I/O awake", 20, false);
    ioRounds("I/O with sleep each", 5, true);
  }
  SD.end();

  // Does a slower clock let the card win the line? Mount awake and mount in
  // sleep, then I/O awake, at each speed.
  static const uint32_t speeds[] = { 400000, 1000000, 2000000 };
  for (uint32_t hz : speeds) {
    Serial.printf("at %lu Hz:\n", (unsigned long)hz);
    SD.end();
    bool ok = SD.begin(SD_CS, SPI, hz, "/sd", 5, false);
    Serial.printf("  %-34s mount %s\n", "awake mount", ok ? "OK" : "FAIL");
    if (!ok) {
      displayCmd(0x10, 150);
      ok = SD.begin(SD_CS, SPI, hz, "/sd", 5, false);
      displayCmd(0x11, 120);
      Serial.printf("  %-34s mount %s\n", "sleep mount", ok ? "OK" : "FAIL");
    }
    if (ok) ioRounds("I/O awake", 10, false);
  }
  SD.end();

  Serial.println("repeat remounts (card swap):");
  int ok = 0;
  for (int i = 0; i < 5; i++) {
    char l[24]; snprintf(l, sizeof(l), "sleep remount %d", i + 1);
    if (sleepMount(l)) ok += ioRounds("  then I/O awake", 3, false) == 3;
  }
  Serial.printf("remount + I/O passes: %d/5\n", ok);
  SD.end();
  Serial.println("=== done ===");
}

void loop() { delay(1000); }
