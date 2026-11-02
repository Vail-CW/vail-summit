// Which display states hold the shared MISO line low, and does the SD
// library mount in each? Uses the Summit's exact LovyanGFX config.
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

static void report(const char* state) {
  // MISO with nothing selected, then with the card selected
  SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
  digitalWrite(SD_CS, HIGH);
  uint8_t h = SPI.transfer(0xFF);
  digitalWrite(SD_CS, LOW);
  uint8_t l = SPI.transfer(0xFF);
  digitalWrite(SD_CS, HIGH);
  SPI.transfer(0xFF);
  SPI.endTransaction();
  SD.end();
  bool ok = SD.begin(SD_CS, SPI, 4000000, "/sd", 5, false);
  SD.end();
  Serial.printf("%-26s idle %02X  selected %02X  library %s\n", state, h, l, ok ? "MOUNT OK" : "FAIL");
}

static void displayCmd(uint8_t c) {
  tft.startWrite();
  tft.writeCommand(c);
  tft.endWrite();
  delay(150);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  ledcAttach(TFT_BL, 5000, 8); ledcWrite(TFT_BL, 255);
  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH);
  pinMode(TFT_RST, OUTPUT);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, -1);
  Serial.println("\n=== sd_disp ===");

  digitalWrite(TFT_RST, LOW); delay(20);
  report("display held in reset");
  digitalWrite(TFT_RST, HIGH); delay(150);
  report("out of reset, no init");

  tft.init(); tft.setRotation(3); tft.fillScreen(TFT_DARKGREEN);
  report("after tft.init");
  uint32_t id = tft.readPixel(0, 0);
  Serial.printf("display ID 0x%08lX\n", (unsigned long)id);
  report("after ID read");
  displayCmd(0x28);  report("display off (0x28)");
  displayCmd(0x29);  report("display on (0x29)");
  displayCmd(0x10);  report("sleep in (0x10)");
  displayCmd(0x11);  report("sleep out (0x11)");
  displayCmd(0x00);  report("nop (0x00)");
  tft.fillScreen(TFT_NAVY);
  report("after redraw");
  Serial.println("=== done ===");
}

void loop() { delay(1000); }
