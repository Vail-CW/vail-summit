/*
 * VAIL SUMMIT - display throughput benchmark
 *
 * Standalone bench sketch. Arduino only recurses into src/, so this does not
 * affect the main firmware build.
 *
 * Question it answers: is the display flush limited by the SPI clock, or by
 * reading pixels back out of PSRAM? If PSRAM is the real ceiling, then the
 * 80MHz -> 40MHz change costs far less than the arithmetic suggests, and
 * moving the LVGL draw buffers into internal SRAM would matter more than the
 * clock rate ever did.
 *
 * It measures the matrix that decides that:
 *     memory {PSRAM, internal SRAM} x clock {40, 80 MHz} x swap565 {on, off}
 * plus raw read bandwidth for each memory type, to separate the memory cost
 * from the bus cost.
 *
 * Buffer geometry matches the firmware exactly: 480 x 40 px = 38400 bytes,
 * which is one LVGL flush. A full 480x320 screen is 8 of them.
 *
 * NOTE: on a board that cannot do 80MHz the screen will show garbage during
 * the 80MHz passes. That is expected and does not affect the timings, which
 * measure the ESP32 side of the transfer.
 */

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <esp_heap_caps.h>

// Must match src/core/config.h
#define TFT_CS      10
#define TFT_RST     11
#define TFT_DC      12
#define TFT_MOSI    35
#define TFT_SCK     36
#define TFT_MISO    37
#define SD_CS       38
#define TFT_BL      39

// Matches LV_BUF_SIZE in src/lvgl/lv_init.h (SCREEN_WIDTH * LV_BUF_LINES)
#define BUF_W        480
#define BUF_LINES    40
#define BUF_PX       (BUF_W * BUF_LINES)
#define BUF_BYTES    (BUF_PX * 2)
#define FLUSHES_PER_SCREEN 8      // 480x320 / (480x40)

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 _panel_instance;
  lgfx::Bus_SPI      _bus_instance;
public:
  LGFX(void) { configure(40000000); }

  void configure(uint32_t write_hz) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = write_hz;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = TFT_SCK;
      cfg.pin_mosi    = TFT_MOSI;
      cfg.pin_miso    = TFT_MISO;
      cfg.pin_dc      = TFT_DC;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = TFT_CS;
      cfg.pin_rst          = TFT_RST;
      cfg.pin_busy         = -1;
      cfg.memory_width     = 320;
      cfg.memory_height    = 480;
      cfg.panel_width      = 320;
      cfg.panel_height     = 480;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = false;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;

static uint16_t* buf_psram = nullptr;
static uint16_t* buf_sram  = nullptr;

static void fill_pattern(uint16_t* b) {
  // Varied data so nothing can be optimized into a constant fill.
  for (int i = 0; i < BUF_PX; i++) {
    b[i] = (uint16_t)((i * 2654435761u) >> 16);
  }
}

// Raw sequential read bandwidth, isolated from SPI.
static float read_bandwidth_mbps(const uint16_t* b) {
  const int ITER = 20;
  volatile uint32_t sink = 0;
  uint32_t t0 = micros();
  for (int it = 0; it < ITER; it++) {
    for (int i = 0; i < BUF_PX; i++) sink += b[i];
  }
  uint32_t dt = micros() - t0;
  (void)sink;
  return (float)((uint64_t)BUF_BYTES * ITER) / (float)dt;   // bytes/us == MB/s
}

struct Row {
  const char* mem;
  uint32_t    hz;
  bool        swap;
  float       mbps;
  float       full_screen_ms;
};
static Row rows[8];
static int  row_count = 0;

static void bench_push(const char* memName, uint16_t* b, uint32_t hz, bool swap) {
  if (b == nullptr) return;

  tft.configure(hz);
  tft.init();
  tft.setRotation(0);

  const int ITER = 30;

  // Warm up once so the first-transaction setup is not counted.
  tft.startWrite();
  tft.setAddrWindow(0, 0, BUF_W, BUF_LINES);
  tft.pushPixels(b, BUF_PX, swap);
  tft.endWrite();

  uint32_t t0 = micros();
  for (int i = 0; i < ITER; i++) {
    tft.startWrite();
    tft.setAddrWindow(0, 0, BUF_W, BUF_LINES);
    tft.pushPixels(b, BUF_PX, swap);
    tft.endWrite();
  }
  uint32_t dt = micros() - t0;

  float mbps = (float)((uint64_t)BUF_BYTES * ITER) / (float)dt;
  float per_flush_us = (float)dt / ITER;
  float full_ms = (per_flush_us * FLUSHES_PER_SCREEN) / 1000.0f;

  Serial.printf("  %-6s  %2lu MHz  swap=%-3s  %6.2f MB/s   flush %6.2f ms   full screen %6.2f ms\n",
                memName, (unsigned long)(hz / 1000000), swap ? "on" : "off",
                mbps, per_flush_us / 1000.0f, full_ms);

  if (row_count < 8) {
    rows[row_count].mem = memName;
    rows[row_count].hz = hz;
    rows[row_count].swap = swap;
    rows[row_count].mbps = mbps;
    rows[row_count].full_screen_ms = full_ms;
    row_count++;
  }
}

static float lookup(const char* mem, uint32_t hz, bool swap) {
  for (int i = 0; i < row_count; i++) {
    if (rows[i].hz == hz && rows[i].swap == swap && strcmp(rows[i].mem, mem) == 0) {
      return rows[i].full_screen_ms;
    }
  }
  return -1.0f;
}

void setup() {
  Serial.begin(115200);
  delay(2500);

  Serial.println();
  Serial.println("##################################################");
  Serial.println("#  VAIL SUMMIT display throughput benchmark      #");
  Serial.println("##################################################");
  Serial.printf("buffer: %d x %d px = %d bytes (one LVGL flush)\n", BUF_W, BUF_LINES, BUF_BYTES);
  Serial.printf("full screen = %d flushes\n", FLUSHES_PER_SCREEN);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);    // keep SD off the shared bus

  Serial.println();
  Serial.printf("internal heap free before alloc: %lu bytes\n",
                (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

  buf_psram = (uint16_t*)ps_malloc(BUF_BYTES);
  buf_sram  = (uint16_t*)heap_caps_malloc(BUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  Serial.printf("PSRAM buffer: %s\n", buf_psram ? "allocated" : "FAILED");
  Serial.printf("SRAM  buffer: %s\n", buf_sram  ? "allocated" : "FAILED");
  Serial.printf("internal heap free after alloc:  %lu bytes\n",
                (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.println("  (the firmware would need 2 of these for double buffering)");

  if (buf_psram) fill_pattern(buf_psram);
  if (buf_sram)  fill_pattern(buf_sram);

  Serial.println();
  Serial.println("=== raw sequential read bandwidth (no SPI) ===");
  if (buf_psram) Serial.printf("  PSRAM: %6.2f MB/s\n", read_bandwidth_mbps(buf_psram));
  if (buf_sram)  Serial.printf("  SRAM : %6.2f MB/s\n", read_bandwidth_mbps(buf_sram));
  Serial.println("  Feeding SPI needs ~5 MB/s at 40MHz, ~10 MB/s at 80MHz.");

  Serial.println();
  Serial.println("=== SPI push throughput ===");
  Serial.println("  (80MHz rows will look garbled on a board that cannot do 80MHz;");
  Serial.println("   the timings are still valid, they measure the ESP32 side)");
  bench_push("PSRAM", buf_psram, 40000000, true);
  bench_push("PSRAM", buf_psram, 80000000, true);
  bench_push("SRAM",  buf_sram,  40000000, true);
  bench_push("SRAM",  buf_sram,  80000000, true);
  Serial.println();
  bench_push("PSRAM", buf_psram, 40000000, false);
  bench_push("PSRAM", buf_psram, 80000000, false);
  bench_push("SRAM",  buf_sram,  40000000, false);
  bench_push("SRAM",  buf_sram,  80000000, false);

  Serial.println();
  Serial.println("##################################################");
  Serial.println("#  WHAT THIS MEANS                               #");
  Serial.println("##################################################");

  float base   = lookup("PSRAM", 80000000, true);  // what the firmware used to do
  float now    = lookup("PSRAM", 40000000, true);  // what it does today
  float target = lookup("SRAM",  40000000, true);  // 40MHz with buffers moved to SRAM
  float noswap = lookup("SRAM",  40000000, false); // plus LV_COLOR_16_SWAP=1

  Serial.printf("  old firmware (80MHz, PSRAM, swap on) : %6.2f ms per full screen\n", base);
  Serial.printf("  current      (40MHz, PSRAM, swap on) : %6.2f ms\n", now);
  Serial.printf("  40MHz + SRAM buffers                 : %6.2f ms\n", target);
  Serial.printf("  40MHz + SRAM + no byte swap          : %6.2f ms\n", noswap);
  Serial.println();

  if (base > 0 && now > 0) {
    Serial.printf("  Cost of the clock drop alone: %.2f ms (%.0f%% slower)\n",
                  now - base, ((now / base) - 1.0f) * 100.0f);
    if (now < base * 1.6f) {
      Serial.println("  -> Well under the 2x the arithmetic predicts, so the bus was NOT");
      Serial.println("     the only limit. Memory read speed is a real factor.");
    } else {
      Serial.println("  -> Close to the full 2x, so the SPI clock really was the limit.");
    }
  }
  if (target > 0 && base > 0) {
    if (target <= base) {
      Serial.println("  -> 40MHz with SRAM buffers MATCHES OR BEATS the old 80MHz setup.");
      Serial.println("     Moving the LVGL draw buffers to internal SRAM buys back the");
      Serial.println("     clock drop entirely. Cost is ~77KB of internal RAM for two.");
    } else {
      Serial.printf("  -> 40MHz + SRAM is still %.2f ms behind the old 80MHz setup.\n", target - base);
      Serial.println("     Buffer placement helps but does not fully close the gap.");
    }
  }
  if (noswap > 0 && target > 0 && noswap < target) {
    Serial.printf("  -> Dropping the byte swap saves a further %.2f ms. That is what\n", target - noswap);
    Serial.println("     setting LV_COLOR_16_SWAP=1 in lv_conf.h would be worth.");
  }
  Serial.println();
  Serial.println("Benchmark complete. Results are static, nothing else will print.");
}

void loop() {
  delay(10000);
}
