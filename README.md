# VAIL SUMMIT

A portable Morse code training device for ham radio operators, built on the ESP32-S3 platform with a modern UI.

![Version](https://img.shields.io/badge/version-0.33-blue)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-green)

> **Hardware Release:** Planned for March/April 2026. Beta test units available now for $75 — contact Brett at [ke9bos@pigletradio.org](mailto:ke9bos@pigletradio.org)

## Overview

VAIL SUMMIT is a standalone Morse code trainer designed for learning and practicing CW (Continuous Wave) communication. It features structured training curricula, interactive games, radio keying output, and a web-based interface for remote control and logging with many other Ham Radio Tools being added very regularly.

**Key Highlights:**
- 4.0" color LCD with intuitive card-based navigation
- Multiple input methods: I2C keyboard, iambic paddle, capacitive touch
- Training curricula (CW Academy, Koch Method, LICW)
- Games for fun practice (Morse Shooter, Memory Chain)
- Radio output for keying external transceivers
- QSO logging
- Connect to the Vail internet Morse repeater

## Features

### Training Modes

| Mode | Description |
|------|-------------|
| **CW Academy** | Official 4-track, 16-session curriculum with copy practice, sending practice, and daily drills |
| **Koch Method** | Scientifically-proven method teaching characters at full speed with progress tracking |
| **LICW Training** | Long Island CW Club 9-carousel curriculum with Time-To-Recognize (TTR) methodology |
| **Vail Master** | CW sending trainer with Sprint/Sweepstakes formats, scoring, and problem character analytics |
| **Hear It Type It** | Configurable receive training with 5 modes: callsigns, letters, numbers, mixed, or custom |
| **Practice Oscillator** | Free-form keying with real-time adaptive decoder, WPM display, and prosign support |
| **License Study** | Ham radio exam prep for Technician, General, and Extra with question database and progress tracking |

### Games

| Game | Description |
|------|-------------|
| **Morse Shooter** | Arcade-style game with falling characters, adaptive decoder, and high score tracking |
| **Memory Chain** | Progressive memory training with 3 game modes and 3 difficulty levels |
| **Spark Watch** | Maritime morse training with historical challenges (Titanic era) and campaign missions |

### Ham Radio Tools

| Tool | Description |
|------|-------------|
| **Band Conditions** | Real-time solar/propagation data from hamqsl.com (SFI, K-index, MUF, Aurora) |
| **Band Plans** | Frequency allocations filtered by license class (Tech/General/Extra) and mode |
| **POTA Integration** | Parks On The Air park lookup, validation, and activation tracking |
| **QSO Logger** | Contact logging with ADIF/CSV export, map visualization, and statistics |
| **Antennas** | Reference information for antenna designs and specifications |

### Radio Integration

| Feature | Description |
|---------|-------------|
| **Radio Output** | Key external radios via 3.5mm jack with Summit Keyer or Radio Keyer modes |
| **CW Memories** | 10 programmable message slots with prosign support and transmission queuing |
| **Vail Repeater** | Internet morse repeater connection via WebSocket to vailmorse.com |

### Bluetooth Connectivity

| Mode | Description |
|------|-------------|
| **BLE HID** | Keyboard emulation for MorseRunner and CW training software compatibility |
| **BLE MIDI** | Standard BLE MIDI protocol for Vail repeater MIDI tools |
| **BLE Keyboard Host** | Connect external Bluetooth keyboards for input |

### Web Interface

| Page | Description |
|------|-------------|
| **Dashboard** | Device status, battery, WiFi info, quick actions |
| **QSO Logger** | Contact logging with interactive map and export options |
| **Practice Mode** | Remote morse practice via WebSocket |
| **Memory Chain** | Browser-based game with device synchronization |
| **Hear It Type It** | Web-based training with Web Audio API |
| **Radio Control** | Remote morse transmission |
| **Settings** | CW speed, tone, volume, callsign configuration |
| **Storage** | SD card file browser with upload/download |
| **System** | Diagnostics, firmware info, memory stats |

### System Features

- **4.0" Color LCD** - ST7796S 480×320 with LVGL card-based UI
- **Multiple Inputs** - CardKB keyboard, iambic paddle, capacitive touch, straight key
- **Battery Monitoring** - MAX17048/LC709203F auto-detection with percentage display
- **SD Card Storage** - FAT32 with web-based file management
- **WiFi** - Station mode or Access Point mode for field use
- **NTP Time Sync** - Automatic time synchronization for QSO timestamps
- **Dual-Core Audio** - FreeRTOS task management for glitch-free audio
- **Deep Sleep** - Triple-ESC from main menu for power saving

## Hardware Requirements

### Core Components

| Component | Description |
|-----------|-------------|
| **MCU** | Adafruit Feather ESP32-S3 (2MB PSRAM) |
| **Display** | ST7796S 4.0" 480×320 LCD (SPI) |
| **Audio** | MAX98357A I2S amplifier |
| **Keyboard** | CardKB I2C keyboard (address 0x5F) |
| **Battery Monitor** | MAX17048 or LC709203F (auto-detected) |

### Input Options

- **Iambic Paddle** - GPIO 6 (DIT), GPIO 9 (DAH)
- **Capacitive Touch** - GPIO 8 (DIT), GPIO 5 (DAH)
- **Straight Key** - Supported in games and practice modes

### Optional

- **SD Card** - FAT32 formatted, for storage and logging
- **Radio Jack** - 3.5mm TRS for keying external radios

For complete pin assignments, see [docs/HARDWARE.md](docs/HARDWARE.md).

## Getting Started

### Pre-built Firmware

The easiest way to get started is with pre-built firmware:

1. Visit the [web flasher](https://update.vailadapter.com)
2. Connect your ESP32-S3 via USB
3. Select "VAIL SUMMIT" and click Flash
4. Follow the on-screen instructions

### First Boot

1. Power on the device
2. Use arrow keys to navigate, Enter to select, ESC to go back
3. Go to **Settings → WiFi Setup** to connect to your network
4. Access the web interface at `http://vail-summit.local/`

### Basic Navigation

| Key | Action |
|-----|--------|
| ↑/↓ | Navigate menu items |
| ←/→ | Adjust values (sliders) |
| Enter | Select / Confirm |
| ESC | Back / Exit mode |
| ESC×3 | Deep sleep (from main menu) |

## Web Interface

Access the device from any browser on your network:

- **mDNS:** `http://vail-summit.local/`
- **AP Mode:** `http://192.168.4.1/` (when in Access Point mode)

### Available Pages

- **Dashboard** - Device status, quick actions
- **QSO Logger** - Contact logging with map visualization
- **Settings** - CW speed, tone, volume, callsign
- **Radio Control** - Remote Morse transmission
- **Storage** - SD card file management
- **System** - Diagnostics and firmware info

### Access Point Mode

If WiFi connection fails or for field use:
1. Go to **Settings → WiFi Setup**
2. Press **'A'** key to enable AP mode
3. Connect to `VAIL-SUMMIT-XXXXXX` (password: `vailsummit`)

## Building from Source

### Prerequisites

- Windows PC (build scripts are Windows-specific)
- USB cable for ESP32-S3

### Quick Build (Windows)

Due to Windows path length limitations, use the short-path method:

**One-Time Setup:**
```powershell
# Create junction for project
New-Item -ItemType Junction -Path 'C:\vs' -Target 'C:\Users\YOUR_USERNAME\path\to\vail-summit'

# Copy arduino-cli to short path
Copy-Item -Path 'C:\vs\arduino-cli\*' -Destination 'C:\acli' -Recurse -Force
```

**Compile:**
```powershell
cd C:\acli
.\arduino-cli.exe compile --config-file arduino-cli.yaml --fqbn "esp32:esp32:adafruit_feather_esp32s3:CDCOnBoot=cdc,PartitionScheme=huge_app,PSRAM=enabled,FlashSize=4M,USBMode=hwcdc" --output-dir C:\vs\build --export-binaries C:\vs
```

**Upload (replace COM31 with your port):**
```powershell
cd C:\acli
.\arduino-cli.exe upload --config-file arduino-cli.yaml --fqbn "esp32:esp32:adafruit_feather_esp32s3:CDCOnBoot=cdc,PartitionScheme=huge_app,PSRAM=enabled,FlashSize=4M,USBMode=hwcdc" --port COM31 --input-dir C:\vs\build C:\vs
```

### Alternative: build.ps1 (may fail on long paths)

| Command | Description |
|---------|-------------|
| `.\build.ps1` | Compile firmware |
| `.\build.ps1 upload COMx` | Upload to device |
| `.\build.ps1 monitor COMx` | Serial monitor |
| `.\build.ps1 list` | List available ports |
| `.\build.ps1 clean` | Clean build directory |

For detailed build instructions, Arduino IDE setup, and library information, see [docs/BUILDING.md](docs/BUILDING.md).

## Documentation

| Document | Description |
|----------|-------------|
| [BUILDING.md](docs/BUILDING.md) | Build setup, compilation, firmware updates |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | System design, state machine, audio system |
| [HARDWARE.md](docs/HARDWARE.md) | Pin assignments, I2C devices, hardware interfaces |
| [FEATURES.md](docs/FEATURES.md) | CW Academy, games, radio mode details |
| [WEB_INTERFACE.md](docs/WEB_INTERFACE.md) | Web server, REST API, QSO logger |
| [DEVELOPMENT.md](docs/DEVELOPMENT.md) | Development patterns, troubleshooting |

## Project Structure

```
vail-summit/
├── vail-summit.ino     # Main sketch
├── src/
│   ├── core/           # Config, morse code, hardware init
│   ├── audio/          # I2S audio, morse decoder
│   ├── lvgl/           # UI screens and theme
│   ├── training/       # Training modes (CW Academy, Koch, etc.)
│   ├── games/          # Morse Shooter, Memory Chain
│   ├── radio/          # Radio output, CW memories
│   ├── settings/       # WiFi, CW, volume settings
│   ├── qso/            # QSO logging
│   ├── web/            # Web server and API
│   ├── network/        # Vail repeater, NTP
│   └── storage/        # SD card management
├── docs/               # Documentation
├── build.bat           # Build script (Windows)
└── build.ps1           # Build script (PowerShell)
```

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Acknowledgments

- [CW Academy](https://cwops.org/cw-academy/) for the training curriculum
- [Vail Morse Repeater](https://vailmorse.com) for internet connectivity
- [LVGL](https://lvgl.io) for the UI framework
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) for the display driver

## Support

- **Issues:** [GitHub Issues](https://github.com/Vail-CW/vail-summit/issues)
- **Discussions:** [GitHub Discussions](https://github.com/Vail-CW/vail-summit/discussions)

---

*73 de KE9BOS Brett Hollifield ke9bos@pigletradio.org*
