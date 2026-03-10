# Requirements

## Hardware Requirements

| Component | Requirement |
|---|---|
| MCU | ESP32 WROOM 32U (or any ESP32-based DevKit) |
| Flash | Minimum 2 MB (4 MB recommended for Web UI SPIFFS) |
| RAM | 520 KB SRAM (standard ESP32 WROOM 32U) |
| Host PC | x86_64 Linux (tested on Ubuntu 22.04, 24.04) |
| USB | Micro-USB for Flash/UART; must show as `/dev/ttyACM0` or `/dev/ttyUSB0` |
| Network | WiFi 2.4 GHz WPA2 (for TCP and Web UI features) |

## Software Requirements

### ESP32 Firmware
| Tool | Minimum Version | Where to Get |
|---|---|---|
| ESP-IDF | v5.5 | https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/ |
| CMake | 3.16 | `sudo apt install cmake` |
| Ninja | 1.10 | `sudo apt install ninja-build` |
| Python | 3.10 | bundled with ESP-IDF |

### EPICS IOC
| Tool | Minimum Version | Where to Get |
|---|---|---|
| EPICS Base | 7.0.8.1 | https://epics-controls.org/resources-and-support/base/ |
| asyn | 4-45 | https://github.com/epics-modules/asyn |
| StreamDevice | 2.8 | https://github.com/paulscherrerinstitute/StreamDevice |
| gcc | 11+ | `sudo apt install build-essential` |
| perl | 5.30+ | `sudo apt install perl` |
| readline | — | `sudo apt install libreadline-dev` |

### Testing & CI
| Tool | Minimum Version |
|---|---|
| Python | 3.10+ |
| pyepics | 3.5+ (`pip install pyepics`) |
| pytest | 7+ (`pip install pytest`) |
| doxygen | 1.9 (optional, for `make docs`) |
| git | 2.30+ |

## Tested Configuration Matrix

| Component | Version | OS |
|---|---|---|
| ESP-IDF | v5.5.3 | Ubuntu 24.04 |
| EPICS Base | 7.0.8.1 | Ubuntu 24.04 |
| asyn | 4-45 | Ubuntu 24.04 |
| StreamDevice | 2.8 | Ubuntu 24.04 |
| Python | 3.12.3 | Ubuntu 24.04 |
