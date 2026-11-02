// SD bisect for the Summit: add the firmware's startup pieces one at a time
// (same order as setup()) and test the SD card after each.
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <LovyanGFX.hpp>

#define SD_CS   38
#define SD_MOSI 35
#define SD_SCK  36
#define SD_MISO 37
#define TFT_CS  10
#define TFT_RST 11
#define TFT_DC  12
#define TFT_BL  39

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
I2SClass i2s;

bool sdTest(const char* stage) {
  SD.end();
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  bool ok = SD.begin(SD_CS, SPI, 4000000, "/sd", 5, false);
  bool rw = false;
  if (ok) {
    File f = SD.open("/bisect.txt", FILE_WRITE);
    if (f) { rw = f.print(stage) > 0; f.close(); }
  }
  Serial.printf("STAGE %-22s mount %s  write %s\n", stage, ok ? "OK  " : "FAIL", rw ? "OK" : "-");
  SD.end();
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(2500);
  Serial.println("\n=== SD bisect ===");
  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  pinMode(TFT_RST, OUTPUT); digitalWrite(TFT_RST, HIGH);
  pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH);

  sdTest("0 bare");

  ledcAttach(TFT_BL, 5000, 8);
  ledcWrite(TFT_BL, 255);
  delay(200);
  sdTest("1 backlight full");

  Wire.begin(3, 4);
  delay(100);
  sdTest("2 + I2C");

  i2s.setPins(14, 15, 16);
  bool i2sOk = i2s.begin(I2S_MODE_STD, 22050, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  Serial.printf("i2s begin %d\n", i2sOk);
  delay(100);
  sdTest("3 + I2S");

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLUE);
  sdTest("4 + display init");

  tft.fillScreen(TFT_RED);
  sdTest("5 + after a redraw");

  pinMode(6, INPUT_PULLUP); pinMode(9, INPUT_PULLUP);
  touchRead(8); touchRead(5);
  sdTest("6 + paddle/touch");
  Serial.println("=== done ===");
}

void loop() { delay(1000); }
