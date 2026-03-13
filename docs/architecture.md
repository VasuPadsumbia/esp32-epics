# System Architecture

## Overview

This project integrates an **ESP32 WROOM 32U** microcontroller with an **EPICS IOC** running on a Linux host.
Communication uses a custom open ASCII protocol over both UART (USB cable) and WiFi (TCP socket).

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ESP32 WROOM 32U  (192.168.1.110)             │
│                                                                     │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐        │
│  │  comms   │   │ protocol │   │ monitor  │   │  webui   │        │
│  │UART+WiFi │──►│  parser  │──►│ timings  │   │ HTTP/SPA │        │
│  └──────────┘   └────┬─────┘   └──────────┘   └──────────┘        │
│                      │ FreeRTOS cmd queue                           │
│                      ▼                                              │
│              ┌───────────────┐                                      │
│              │   app_task    │──► hw_hal (GPIO + shadow register)  │
│              └───────────────┘                                      │
└─────────────────────────────────────────────────────────────────────┘
         │ /dev/ttyACM0 (UART)             │ WiFi TCP :7070
         ▼                                 ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         EPICS IOC (Linux host)                      │
│   drvAsynIPPortConfigure → ESP32_WIFI (192.168.1.110:7070)         │
│   StreamDevice esp32.proto ── ASCII command ↔ EPICS record          │
│   esp32.db    ─────────── all PVs (LED, SYS, TASK timing, GPIO)    │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Firmware Components

| Component | Responsibility |
|---|---|
| `hw_hal` | GPIO abstraction — shadow register for reliable output readback |
| `utils` | `UTILS_LOG*` macros wrapping ESP_LOG |
| `protocol` | ASCII parser → `protocol_cmd_t` → FreeRTOS queue |
| `comms` | UART listener + WiFi TCP server; credentials baked in at build time |
| `monitor` | µs-resolution task cycle-time tracking via `esp_timer`; mutex-protected |
| `webui` | `esp_http_server` serving binary-embedded `index.html` dashboard + REST JSON API |

---

## Communication Protocol

Format: **`<DEVICE>:<ACTION> [VALUE]\n`**

All commands and responses are ASCII, human-readable, and `\n`-terminated.

| Category | Commands |
|---|---|
| System | `SYS:PING`, `SYS:STATUS`, `SYS:CYCLE`, `SYS:VERSION`, `SYS:HEAP`, `SYS:RESET` |
| LED | `LED:SET <0|1>`, `LED:GET` |
| GPIO | `GPIO:SET <pin> <0|1>`, `GPIO:GET <pin>`, `GPIO:DIR <pin> <IN|OUT>` |
| Task | `TASK:LIST`, `TASK:GET <name>`, `TASK:GETALL`, `TASK:RESET <name>` |

The `response_fd` field in `protocol_cmd_t` routes replies back to the correct socket (UART = −1, WiFi = client fd).

---

## EPICS Integration

| Layer | Detail |
|---|---|
| **Transport** | `asyn` `drvAsynIPPortConfigure` connects to `ESP32_IP:TCP_PORT` over WiFi |
| **Protocol** | `StreamDevice` `.proto` file maps ASCII commands ↔ EPICS record operations |
| **Records** | `bo/bi` for LED/GPIO, `ai/stringin` for status and timing channels |
| **Scaling** | Firmware reports uptime in ms; EPICS `ai` record converts to seconds with `ASLO=0.001` |

---

## Web UI REST API

The embedded HTTP server on port 80 exposes:

| Endpoint | Method | Response |
|---|---|---|
| `/` | GET | `index.html` (embedded binary dashboard, 1s auto-refresh) |
| `/api/status` | GET | `{uptime_ms, free_heap, version}` |
| `/api/tasks` | GET | `[{name, min_us, max_us, avg_us}, ...]` |
| `/api/gpio` | GET | `{pin, value}` |
| `/api/gpio` | POST | `{"pin":<n>,"value":<0|1>}` |
| `/api/config` | GET | `{ssid, tcp_port, webui_port}` |

---

## Configuration Flow

A single file — **`project.conf`** — drives the entire project:

```
project.conf
    │
    ├─► make gen-config
    │       │
    │       ├─► firmware/sdkconfig.defaults   (WiFi SSID/PW, ports, GPIO)
    │       ├─► epics_ioc/configure/RELEASE    (EPICS module paths)
    │       └─► epics_ioc/iocBoot/iocesp32/env_project.cmd  (runtime env vars)
    │
    └─► ESP32 firmware: Kconfig.projbuild defines CONFIG_PROJECT_* symbols
        so sdkconfig.defaults values are compiled into the binary.
```

---

## Verified Test Results (Phase 6)

| Test Suite | Result |
|---|---|
| TCP socket (`scripts/test_tcp.py`) | **10 / 10 PASS** |
| Web UI HTTP (`scripts/test_webui.py`) | **6 / 6 PASS** |
