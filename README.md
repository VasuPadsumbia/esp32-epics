# ESP32 EPICS Integration

A professional-grade integration between an **ESP32 WROOM 32U** microcontroller and an **EPICS IOC**, communicating over both UART and **WiFi TCP** using a custom ASCII protocol.

[![Firmware Build](https://github.com/actions/workflows/badge.svg)](.github/workflows/firmware.yml)
[![IOC Build](https://github.com/actions/workflows/badge.svg)](.github/workflows/epics.yml)

---

## Features

- 🔌 **Dual transport** — UART (USB) and WiFi TCP on the same protocol
- 🎛️ **Universal Pin Configuration** — assign roles (ADC, PWM, DAC, I2C, UART, GPIO) to any pin live
- 💾 **Persistent Settings** — pin roles and configurations are saved to NVS and restored on boot
- 📊 **Task timing telemetry** — real-time FreeRTOS cycle stats for APP / UART / TCP tasks
- 🚀 **Advanced Peripherals** — 28 PWM pins, all 15 ADC channels, and 2 DAC outputs supported
- 🌐 **Embedded Web UI** — glassmorphism dashboard at `http://<ESP32_IP>/` with dynamic pin config
- ⚙️ **Single config file** — `project.conf` drives firmware, IOC, and test scripts

---

## Quick Start

```bash
# 1. Fill in your settings (WiFi, serial port, EPICS paths)
vim project.conf

# 2. Generate derived configs
make gen-config

# 3. Build everything
make all

# 4. Flash the ESP32
make fw-flash

# 5. Watch serial monitor for "Got IP: 192.168.x.x", then update project.conf
make fw-monitor

# 6. Run the EPICS IOC
make ioc-run

# 7. Verify everything
make verify
```

> ⚠️ **WiFi note**: The ESP32 only supports **2.4 GHz** WPA2 networks. 5 GHz networks will silently fail.

---

## Project Layout

```
epics_esp32_project/
├── project.conf                    # ← Edit this first
├── Makefile                        # fw-build, fw-flash, ioc-run, verify, ...
├── scripts/
│   ├── gen_config.py               # generates RELEASE + sdkconfig.defaults
│   ├── test_tcp.py                 # TCP socket test suite (10 tests)
│   ├── test_webui.py               # HTTP endpoint test suite (6 tests)
│   └── verify_system.py            # end-to-end IOC + PV verification
├── firmware/
│   ├── main/
│   │   ├── main.c                  # app entry point, task dispatch
│   │   └── Kconfig.projbuild       # exposes CONFIG_PROJECT_* symbols to ESP-IDF
│   ├── components/
│   │   ├── hw_hal/                 # GPIO abstraction + output shadow register
│   │   ├── comms/                  # UART + WiFi TCP transports
│   │   ├── protocol/               # ASCII command parser → FreeRTOS queue
│   │   ├── monitor/                # µs-resolution task timing
│   │   ├── webui/                  # HTTP server + REST API + SPA dashboard
│   │   └── utils/                  # logging macros
│   └── docs/
│       └── components.md           # per-component API reference
├── epics_ioc/
│   ├── esp32App/Db/
│   │   ├── esp32.db                # all EPICS records
│   │   └── esp32.proto             # StreamDevice ASCII protocol mapping
│   ├── iocBoot/iocesp32/
│   │   ├── st.cmd                  # IOC startup (WiFi TCP transport)
│   │   └── env_project.cmd         # auto-generated env vars
│   └── docs/
│       └── pvs.md                  # full PV table with units
├── docs/
│   ├── architecture.md             # system diagram, component roles, data flow
│   ├── user_guide.md               # build targets, PV usage, Web UI, troubleshooting
│   ├── installation.md             # step-by-step ESP-IDF + EPICS setup
│   └── requirements.md             # hardware/software version matrix
└── logs/                           # test output (gitignored)
    ├── test_tcp_final.log
    ├── test_webui_final.log
    └── verification_test.log
```

---

## Key EPICS PVs

| PV | Description |
|---|---|
| `ESP32:LED` | Set LED on/off |
| `ESP32:LED:RBV` | Read current LED state |
| `ESP32:DAC:PIN25` | Set DAC output voltage (0-255) |
| `ESP32:PIN13:ROLE` | Set Role for Pin 13 (ADC, PWM, etc.) |
| `ESP32:AI:PIN34` | Read Analog Input (mV) |
| `ESP32:SYS:UPTIME` | Uptime in seconds |
| `ESP32:SYS:HEAP` | Free heap bytes |
| `ESP32:SYS:VERSION` | Firmware version string |
| `ESP32:TASK:APP:AVG` | `app_task` average cycle time (µs) |

```bash
caput ESP32:LED 1
caget ESP32:SYS:UPTIME
caget ESP32:TASK:APP:MAX
```

See [`epics_ioc/docs/pvs.md`](epics_ioc/docs/pvs.md) for the full PV table.

---

## Web UI & REST API

Open `http://<ESP32_IP>/` in a browser. Auto-refreshes every second.

| Endpoint | Method | Description |
|---|---|---|
| `/api/status` | GET | `{uptime_ms, free_heap, version}` |
| `/api/tasks` | GET | Task timing array |
| `/api/gpio/schema`| GET | Pin capability & current role map for all 40 GPIOs |
| `/api/pin/cfg` | POST | Set pin role & persist to NVS |
| `/api/pwm` | POST | Set PWM duty cycle |
| `/api/dac` | POST | Set DAC output voltage |
| `/api/config` | GET | WiFi SSID, port config |

---

## Make Targets

| Command | Action |
|---|---|
| `make gen-config` | Regenerate EPICS RELEASE + firmware sdkconfig.defaults |
| `make all` | Build firmware + IOC |
| `make fw-flash` | Flash ESP32 |
| `make fw-monitor` | Serial monitor (find IP address here) |
| `make ioc-run` | Start EPICS IOC |
| `make verify` | End-to-end PV verification |
| `make clean` | Clean all build artifacts |

---

## Test Results

| Suite | Result |
|---|---|
| `scripts/test_tcp.py` (10 tests) | ✅ **10 / 10 PASS** |
| `scripts/test_webui.py` (6 tests) | ✅ **6 / 6 PASS** |

---

## Documentation

- [Architecture](docs/architecture.md) — system diagram, component overview, protocol
- [User Guide](docs/user_guide.md) — PV usage, Web UI, troubleshooting
- [Installation](docs/installation.md) — step-by-step setup
- [Requirements](docs/requirements.md) — hardware/software versions
- [Firmware Components](firmware/docs/components.md) — per-component API reference
- [EPICS PVs](epics_ioc/docs/pvs.md) — full PV table with units
