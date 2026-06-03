# VAIL SUMMIT

**Learn Morse code from zero — then take it on the air.** A pocket-sized CW trainer and
ham radio companion with a bright color screen, a built-in paddle and speaker, and a
friendly course that meets you wherever you are.

![Version](https://img.shields.io/badge/version-0.7-blue)
![For](https://img.shields.io/badge/for-CW%20%26%20ham%20radio-green)
![License](https://img.shields.io/badge/license-PolyForm%20Noncommercial-orange)

> **Hardware release planned for spring 2026.** Beta units are available now for **$75** —
> contact Brett (KE9BOS) at [ke9bos@pigletradio.org](mailto:ke9bos@pigletradio.org).

---

## What is it?

VAIL SUMMIT is a standalone device for learning and enjoying Morse code (CW). Whether you've
never sent a dit in your life or you're a seasoned op looking to sharpen your fist, it's built
to be your everyday CW practice partner — no computer required.

Turn it on and you land on a simple home screen that shows your progress and the day's band
conditions. From there you can take a lesson, run a quick drill, play a game, key a real radio,
or jump on the internet Morse repeater to ragchew with other operators in code.

It's two things at once, on purpose:

- **A patient CW teacher** — a proven, step-by-step course that introduces characters at full
  speed and grows with you.
- **A pocket ham-shack tool** — log your contacts, look up POTA parks, check propagation, key
  your rig, and store your favorite CW messages.

---

## What you can do with it

### 📚 Learn CW
One clear path from your very first character to confident copy. Built on the time-tested
**Koch method** — you hear letters at full speed from day one and build real instincts, not
a counting habit. Each lesson walks you from introduction to solo practice to mixed copy to
real words and callsigns, and remembers exactly where you left off.

Plus three ways to keep your skills sharp:
- **Daily practice** — a short, adaptive warm-up over everything you've learned.
- **Copy practice** — open-ended listen-and-type, as long as you like.
- **Send practice** — key the paddle and watch the device decode your sending in real time.

### 🎯 Practice & decode
A built-in **practice oscillator** with a live decoder. Key away on the paddle (or a straight
key) and see your Morse turned into text on screen, with your speed in words-per-minute — a
great way to hear and check your own fist.

### 🕹️ Play
Learning sticks when it's fun. Arcade-style **Morse Shooter**, **Memory Chain**, the maritime-
themed **Spark Watch**, a speed challenge, and more — all with scoring to chase.

### 📡 On the air
- **Key your radio** — a 3.5 mm jack lets the Summit key an external transceiver, as a keyer or
  a straight-through key.
- **CW message memories** — store your go-to calls and exchanges and send them at the touch of a button.
- **QSO logger** — log contacts on the device or in your browser, see them on a map, and export
  to **ADIF/CSV** for your favorite logging program.
- **POTA** — Parks On The Air lookups and activation tracking for portable operating.
- **Band conditions & band plans** — live solar/propagation data and frequency privileges by
  license class, so you know where (and when) to call CQ.

### 🌐 Vail internet repeater
Connect over WiFi to the **Vail Morse repeater** (vailmorse.com) and send and receive real,
hand-keyed Morse with other operators around the world — a fun, low-pressure way to get on the
"air" without a radio.

### 🔗 Connect it
- **WiFi web interface** — open the device in any browser on your network to practice, log
  contacts, manage files, and tweak settings from a big screen. (Protected by a password — yours
  is shown on the device under *System Info*.)
- **Bluetooth** — acts as a wireless keyer for contest/training software like MorseRunner, speaks
  standard BLE MIDI, and can host an external Bluetooth keyboard.

---

## Getting started

### 1. Put firmware on it
The easiest way — no software to install:

1. Open the [web flasher](https://update.vailadapter.com) in Chrome or Edge.
2. Plug the device into your computer with a USB cable.
3. Choose **VAIL SUMMIT** and click **Flash**, then follow the prompts.

### 2. First steps
1. Power on — you'll arrive at the home screen.
2. Use the **arrow keys** to move, **Enter** to select, **Esc** to go back.
3. Pick **Learn CW** and start your first lesson, or explore the menus.
4. Want WiFi (for the repeater, logging, and the web interface)? Go to **More → Settings →
   WiFi** and join your network.

### 3. Day-to-day handiness
- **Quick settings:** press **V** from any menu to pop up volume, brightness, CW speed, and
  tone — adjust on the fly without digging through menus.
- **Web access:** once on WiFi, visit `http://vail-summit.local/` from a browser on the same network.
- **Start fresh:** a factory reset lives in **Settings → Device Settings**, or hold both paddle
  levers while powering on.

---

## What's in the device

A capable little computer tuned for CW, all battery-powered and portable:

- **4-inch color LCD** with a clean, card-based menu
- **Built-in speaker** and a real **iambic paddle / capacitive touch** input (straight key supported too)
- **Rechargeable battery** with on-screen charge level
- **WiFi and Bluetooth** built in
- **microSD card** slot for logs and files
- **3.5 mm jack** to key an external radio

---

## Help & community

- **Questions or ideas:** [GitHub Discussions](https://github.com/Vail-CW/vail-summit/discussions)
- **Found a bug?** [GitHub Issues](https://github.com/Vail-CW/vail-summit/issues)
- **Get a beta unit:** [ke9bos@pigletradio.org](mailto:ke9bos@pigletradio.org)

---

## For tinkerers & developers

VAIL SUMMIT runs on an ESP32-S3 and the firmware is open for noncommercial use. If you'd like to
build it yourself, modify it, or peek under the hood:

- [Building from source](docs/BUILDING.md) — toolchain setup and compiling
- [Hardware reference](docs/HARDWARE.md) — board, pins, and interfaces
- [Architecture](docs/ARCHITECTURE.md) and [development notes](docs/DEVELOPMENT.md)

## License

Copyright © 2025 Brett Hollifield (KE9BOS) / Vail-CW.

Licensed under the **PolyForm Noncommercial License 1.0.0** ([LICENSE.md](LICENSE.md)) — free to
use, modify, and share for **noncommercial** purposes; selling devices that run this firmware
requires a license from the copyright holder. Built with several open-source libraries; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for credits.

## Thanks

- The [Vail Morse Repeater](https://vailmorse.com) community for the internet CW connection
- [morse-pro](https://github.com/scp93ch/morse-pro) by Stephen C Phillips for the decoding foundation
- The CW clubs and Elmers who keep the code alive

---

*73 de KE9BOS — Brett Hollifield · [ke9bos@pigletradio.org](mailto:ke9bos@pigletradio.org)*
