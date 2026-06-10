# VAIL SUMMIT

A pocket-sized Morse code (CW) trainer and ham radio tool. Learn code from scratch, keep your
skills sharp, and use it on the air. It has a color screen, a built-in paddle and speaker, and
runs on its own battery, so you don't need a computer.

![Version](https://img.shields.io/badge/version-0.71-blue)
![For](https://img.shields.io/badge/for-CW%20%26%20ham%20radio-green)
![License](https://img.shields.io/badge/license-PolyForm%20Noncommercial-orange)

> **Now in testing. Official release is planned for July 2026.** You can get one early during the
> testing phase at [shop.ke9bos.com](https://shop.ke9bos.com), or email me (Brett, KE9BOS) at
> [ke9bos@pigletradio.org](mailto:ke9bos@pigletradio.org).

## What is it?

VAIL SUMMIT is a standalone device for learning and practicing Morse code. New to CW? It will take
you from your first character to real copy. Been at it a while? Use it to warm up, log contacts,
and get on the air.

Turn it on and you start at a home screen with your training progress and the day's band
conditions. From there you can take a lesson, run a quick drill, play a game, key a real radio, or
hop on the internet Morse repeater and ragchew with other ops in code.

It does two jobs:

- It teaches CW with a structured course that starts at full character speed.
- It works as a portable ham tool for logging contacts, POTA, propagation, keying your rig, and
  storing CW messages.

## What you can do with it

### Learn CW

One course that takes you from your first character to solid copy. It uses the Koch method: you
hear letters at full speed from the start, so you learn the sound of each character instead of
counting dits and dahs. Each lesson moves from a quick intro to solo practice, then mixed copy,
then real words and callsigns. It saves your place, so you pick up where you left off.

Three ways to practice once you're going:

- **Daily practice:** a short warm-up that mixes in everything you've learned.
- **Copy practice:** open-ended listen-and-type, for as long as you want.
- **Send practice:** key the paddle and the device decodes your sending as you go.

### Practice and decode

There's a practice oscillator with a live decoder built in. Key on the paddle (or a straight key)
and the device shows your Morse as text on screen, along with your speed in words per minute. It's
a handy way to check your own fist.

### Games

A few games to make practice less of a chore: **Morse Shooter** (arcade-style falling characters),
**Memory Chain**, the maritime-themed **Spark Watch**, a speed challenge, and more. Each one keeps
score.

### On the air

- **Key your radio:** a 3.5 mm jack keys an external transceiver, either as a keyer or a
  straight-through key.
- **CW memories:** store your common calls and exchanges and send them with one button.
- **QSO logger:** log contacts on the device or in your browser, view them on a map, and export to
  ADIF or CSV for your main logging program.
- **POTA:** Parks On The Air lookups and activation tracking for portable operating.
- **Band conditions and band plans:** live solar and propagation numbers, plus frequency
  privileges by license class, so you know where and when to call CQ.

### Vail internet repeater

Connect over WiFi to the Vail Morse repeater (vailmorse.com) and send and receive real, hand-keyed
Morse with other operators around the world. It's an easy way to get on the "air" when you don't
have a radio in front of you.

### Connect it

- **WiFi web interface:** open the device in any browser on your network to practice, log contacts,
  manage files, and change settings on a bigger screen. It's password-protected, and your password
  shows on the device under System Info.
- **Bluetooth:** works as a wireless keyer for software like MorseRunner, speaks standard BLE MIDI,
  and can connect to a Bluetooth keyboard.

## Getting started

### 1. Put firmware on it

No software to install:

1. Open the [web flasher](https://update.vailadapter.com) in Chrome or Edge.
2. Plug the device into your computer with a USB cable.
3. Pick **VAIL SUMMIT**, click **Flash**, and follow the prompts.

### 2. First steps

1. Power on. You'll land on the home screen.
2. Move with the **arrow keys**, select with **Enter**, go back with **Esc**.
3. Pick **Learn CW** to start your first lesson, or look around the menus.
4. For WiFi (needed for the repeater, logging, and the web interface), go to
   **More > Settings > WiFi** and join your network.

### 3. Handy to know

- **Quick settings:** press **V** from a menu to adjust volume, brightness, CW speed, and tone
  without leaving what you're doing.
- **Web access:** once you're on WiFi, open `http://vail-summit.local/` from a browser on the same
  network.
- **Start over:** there's a factory reset under **Settings > Device Settings**, or hold both paddle
  levers while you power on.

## What's in it

A small, battery-powered computer set up for CW:

- **4-inch color LCD** with a card-style menu
- **Built-in speaker**, plus a real **iambic paddle and capacitive touch pads** (straight key works too)
- **Rechargeable battery** with a charge readout on screen
- **WiFi and Bluetooth** built in
- **microSD card** slot for logs and files
- **3.5 mm jack** for keying an external radio

## Help and community

- **Buy a Summit (testing phase):** [shop.ke9bos.com](https://shop.ke9bos.com) or [ke9bos@pigletradio.org](mailto:ke9bos@pigletradio.org)
- **Questions or ideas:** [GitHub Discussions](https://github.com/Vail-CW/vail-summit/discussions)
- **Found a bug?** [GitHub Issues](https://github.com/Vail-CW/vail-summit/issues)

## For tinkerers and developers

VAIL SUMMIT runs on an ESP32-S3, and the firmware source is open for noncommercial use. If you want
to build it, change it, or just look around:

- [Building from source](docs/BUILDING.md): toolchain setup and compiling
- [Hardware reference](docs/HARDWARE.md): board, pins, and interfaces
- [Architecture](docs/ARCHITECTURE.md) and [development notes](docs/DEVELOPMENT.md)

## License

Copyright (c) 2025 Brett Hollifield (KE9BOS) / Vail-CW.

The firmware is licensed under the **PolyForm Noncommercial License 1.0.0** (see
[LICENSE.md](LICENSE.md)). You're free to use, modify, and share it for noncommercial purposes.
Selling devices that run this firmware needs a license from me. It's built with several
open-source libraries; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for credits.

## Thanks

- The [Vail Morse Repeater](https://vailmorse.com) community for the internet CW connection
- [morse-pro](https://github.com/scp93ch/morse-pro) by Stephen C Phillips for the decoding foundation
- The CW clubs and Elmers who keep new hams getting on the air

*73 de KE9BOS, Brett Hollifield, [ke9bos@pigletradio.org](mailto:ke9bos@pigletradio.org)*
