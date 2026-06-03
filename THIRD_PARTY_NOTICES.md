# Third-Party Notices

VAIL SUMMIT firmware incorporates the following third-party software. Each component
is the property of its respective copyright holders and is used under the license
noted. Full license texts are in each library's source (bundled under
`arduino-cli/user/libraries/` and the ESP32 Arduino core).

| Component | Version | License | Copyright |
|-----------|---------|---------|-----------|
| ESP32 Arduino core | 2.0.14 | LGPL-2.1 / Apache-2.0 | Espressif Systems |
| LVGL | 8.3.11 | MIT | 2021 LVGL Kft |
| LovyanGFX | 1.1.16 | FreeBSD (BSD-2-Clause) | lovyan03 (Bonsai-Bonsai) |
| NimBLE-Arduino | 1.4.2 | Apache-2.0 | h2zero & contributors |
| ArduinoJson | 7.0.4 | MIT | Benoit Blanchon |
| ESP Async WebServer | 3.6.0 | **LGPL-3.0** | Hristo Gochkov / ESP32Async |
| AsyncTCP | 3.3.2 | **LGPL-3.0** | Hristo Gochkov / ESP32Async |
| WebSockets (arduinoWebSockets) | 2.4.1 | **LGPL-2.1** | 2015 Markus Sattler |
| Adafruit LC709203F | 1.3.4 | BSD-3-Clause | 2019 Adafruit Industries |
| Adafruit MAX1704X | 1.0.3 | BSD-3-Clause | 2022 Adafruit Industries |
| Adafruit BusIO | 1.17.4 | MIT | 2017 Adafruit Industries |

## Morse decoding (derived work)

`src/audio/morse_decoder.h`, `src/audio/morse_decoder_adaptive.h`, and
`src/audio/morse_wpm.h` are ported/derived from **morse-pro** by Stephen C Phillips
(<https://github.com/scp93ch/morse-pro>).

- Original work: Copyright (c) 2024 Stephen C Phillips
- Modifications: Copyright (c) 2025 VAIL SUMMIT Contributors
- License: **European Union Public Licence (EUPL) v1.2** — <https://opensource.org/licenses/EUPL-1.2>

The EUPL is a copyleft license. The source of these modules is made available under the
EUPL; their attribution and license headers must be preserved in distributions.

## Copyleft obligations (important for distribution)

The firmware statically links the **LGPL** components above (ESP Async WebServer,
AsyncTCP, WebSockets) and **LGPL** parts of the ESP32 core. LGPL permits use in a
commercial/closed product, but requires that recipients be able to replace the LGPL
library with a modified version — in practice for embedded firmware this means
distributing either the firmware source or the relinkable object files for the product.
Keeping this repository's source available satisfies that. The **EUPL** modules
similarly require their source to remain available under the EUPL.

> This file is informational, not legal advice. Confirm the obligations above with
> counsel before commercial distribution, especially if any part of the firmware is
> closed-sourced.
