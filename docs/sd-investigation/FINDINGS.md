# SD card fails on some Summits: investigation notes (2026-11-01)

Unit 1 (COM460) won't mount SD on v0.92, v0.91 or v0.85. Unit 2 mounts the
same card on the same firmware. The card is a known-good 31 GB card,
reformatted FAT32 (MBR) on the PC.

## Settled

- **Not solder, not the card, not core 3.** The joints measure normally (MI,
  MO and SCK all about 2.4k to GND unpowered). Raw SD commands read block 0
  correctly (MBR data plus 55AA) under every condition tried, including
  bit-banged SPI.
- **The display on unit 1 holds MISO (GPIO37) low whenever it is out of
  reset.** With TFT_RST low, MISO idles FF. Out of reset, it idles 00 or C0.
  Unit 2's display releases it.
- **The SD library is what fails.** With core debug logs on
  (`DebugLevel=verbose`), `sd_diskio.cpp` shows `sdWait(): Wait Failed`, then
  `Select Failed`, then `CRC_ON_OFF failed: 255`. `sdWait` treats 0x00 on MISO
  as "card busy" and gives up after 500 ms. A card only drives MISO while it's
  answering, so a low idle line looks permanently busy.
- **State survives ESP32 resets.** The display and the card stay powered
  through a flash, so one run's outcome depends on what the previous run left
  behind. Earlier "it works" and "it fails" results contradicted each other
  for this reason. Judge only with a known starting state, or after a full
  power cycle.

## Display state vs. mount (sd_disp sketch, unit 1)

| State | MISO idle (SD CS high) | With SD selected | Library |
|---|---|---|---|
| TFT_RST held low | FF | FF | mount OK |
| Out of reset, no init | C0 | 3F | mount OK |
| After tft.init | 00 | 3F | FAIL |
| After a readPixel | FF | FF | FAIL |
| Display off 0x28 / on 0x29 | 00 | 00 | FAIL |
| **Sleep in 0x10** | 00 | 00 | **mount OK** |
| Sleep out 0x11 | 00 | 3F | FAIL |
| NOP / redraw | 00 | 00 | FAIL |

A selected reading of `3F` means the line starts low and rises after about
2 bits, with the card fighting the display. A byte like that also has bit 7
clear, so the library can take it as a reply.

## Day 2 results (2026-11-02), each run from a known state

`sd_fix` and `sd_initbisect` hold TFT_RST low and run a raw card init first.

- **Display sleep is not a fix.** The same sleep-mount sequence passed in one
  run and failed in the next. When it did mount, reads and writes with the
  display awake passed 0 to 3 times out of 3. The line is marginal: the card
  barely wins against the display.
- **Slower SD clock (400 kHz, 1 MHz, 2 MHz):** no help.
- **0xB0 SDA_EN (3-wire mode, "DOUT not used"):** no help.
- **No init command is to blame.** With the display just out of reset and no
  commands sent, MISO idled 00 and every mount failed in one pass. In the
  other pass it idled C0 and mounted. After SWRESET it is always 00 and
  always fails. Held in reset, every mount passes, every time.

Conclusion: whenever this display's controller is out of reset, it loads the
shared MISO line. No command or timing makes it let go reliably, so firmware
can't fix this on its own. The fix is to disconnect the display's SDO (MISO)
pin. The only thing in the firmware that reads the display is the web
screenshot (`tft.readRect` in `web_api_screenshot.h`). It could copy from
LVGL instead.

What firmware can do is detect it. The self check reads MISO with SD CS high
and the pull-up on. If the line is low, it reports that the display is the
usual cause. Holding TFT_RST low would separate a display from a short to
GND, but re-initializing the display mid-session (tft.init()) corrupted every
later SD read/write in testing, so the check skips that step. The web
screenshot now copies LVGL's flushes instead of calling `tft.readRect`, so it
keeps working on boards with the pin cut.

**Confirmed by cutting the display's SDO pin on unit 1.** `sd_fix` then
showed idle FF with the display awake, every mount passing, and every I/O
round passing at every speed. Stock v0.92 self check: SD card, SD
read/write and SD wiring all pass.

## Candidate fixes (day 1, superseded by the results above)

1. **Sleep the display around SD start-up** (0x10, mount, 0x11). Sleep in made
   the mount work. The screen keeps its contents, with a short blank at boot
   and when a card goes in. Still to check: whether reads and writes then work
   with the display awake, or whether every SD access needs it.
2. **A vendored SD diskio that tolerates a low idle line.** It would not trust
   0x00 as busy right after select, and would confirm busy with CMD13. More
   work, but it fixes every display type.
3. **Hardware (future boards):** leave the display's SDO unconnected (the
   Summit never reads the display), or give SD its own MISO.

Tried and not enough on their own: a raw wake handshake before the mount,
`SPI.begin()` before `tft.init()`, and mounting before the display init.
Mounting before the display works at boot, but any remount after the display
is up fails.

## Next steps

- Start each experiment from a known state: TFT_RST low, then a full raw card
  init, before the step under test. Otherwise do a full power cycle.
- Test fix 1 inside the Summit. Mount in sleep, then read and write with the
  display awake, and check whether `sdWait` passes during normal I/O.
- Check unit 2's idle MISO with the same sketch, to set the self-check
  threshold.

## Sketches here

- `sd_only`: bare SD mount loop.
- `sd_bisect`: Summit start-up stages one at a time.
- `sd_wake`: idle bytes, plus a raw wake before the library.
- `sd_disp`: display state vs. MISO vs. mount. This one gave the table above.
