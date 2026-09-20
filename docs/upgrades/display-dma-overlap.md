# Display: overlap the SPI transfer with drawing

Not doing this yet. Writing it down so the numbers are not lost.

## The idea

The flush in `lv_init.h` is blocking. LVGL draws a buffer, then the CPU sits
and waits while that buffer goes out over SPI, then it draws the next one.
With DMA those two could happen at the same time. The draw buffers are already
double buffered, so the second buffer is sitting right there.

## Why it would be worth it

Measured on a Summit at 40MHz using the profiler in `lv_init.h`
(`DISPLAY_PERF_INSTRUMENT`):

| | draw | transfer | bus busy |
|---|---|---|---|
| sitting on home screen | 2.08 ms | 2.12 ms | 0.8% |
| entering practice | 5.59 ms | 7.50 ms | 6.7% |
| screen load, worst seen | 7.84 ms | 7.79 ms | 12.8% |

Draw time and transfer time come out almost identical. That is the best case
for overlapping, because neither side ends up waiting on the other.

Full screen is 8 flushes:

- old 80MHz blocking: about 94 ms
- 40MHz blocking, what it does now: about 125 ms
- 40MHz with overlap: about 70 ms

So overlapping at 40MHz would land faster than the old 80MHz build. Right now
the CPU throws away 7.8 ms per buffer doing nothing but waiting on the bus.

## Why I have not done it

Nothing is actually slow. Worst case the bus is busy 12.8% of the time and it
is 0.8% just sitting on a screen. The whole prize is about 30 ms on a screen
change.

And the comment in `lv_init.h` says overlapped DMA traffic fights the I2S DMA
and crunches the sidetone. That is the last part of this thing I want to break
for 30 ms.

## If I do it

Gate it on `MODE_FLAG_AUDIO_CRITICAL`. Blocking flush while keying, overlap
everywhere else. The flag is already in `mode_registry.h`, so the sidetone path
stays exactly as it is today.

## Dead ends, do not retry

Measured with `tools/display_bench`. All three of these sound reasonable and
all three are wrong:

- **Moving the LVGL draw buffers from PSRAM to internal SRAM.** No difference.
  62.00 ms vs 61.94 ms on a full screen, which is noise. PSRAM reads at
  28.5 MB/s and an 80MHz bus only ever asks for 10 MB/s. PSRAM was never the
  limit. Would have cost 77KB of internal RAM for nothing.
- **`LV_COLOR_16_SWAP=1` to skip the byte swap.** It is slower, not faster.
  Swap on is 4.96 MB/s, swap off is 4.47 MB/s. LovyanGFX's swapping path is
  the optimized one.
- **Bigger draw buffers.** The bus already runs at 98-99% of theoretical
  (4.96 of 5.00 MB/s at 40MHz, 9.84 of 10.00 at 80MHz). There is no per flush
  overhead left to win back.

## Related

Why the panel runs at 40MHz and not 80 is in the commit that changed it. Short
version: 80MHz is the ST7796S ceiling and one board out of a build of three
could not do it. There is nothing to fall back to in between either, ESP32-S3
divides the 80MHz APB by an integer, so asking for 60MHz quietly gives you 40.
