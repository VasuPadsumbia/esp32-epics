# User Guide

## Quick Start

1. **Edit `project.conf`** — set your WiFi credentials, serial port, and EPICS paths.
2. **`make gen-config`** — generates `sdkconfig.defaults` and EPICS `RELEASE` from `project.conf`.
3. **`make all`** — builds firmware and EPICS IOC.
4. **`make fw-flash`** — flashes the ESP32.
5. Watch the serial monitor (`make fw-monitor`) for the assigned IP address.
6. Update `ESP32_IP` in `project.conf` with that address, then run `make gen-config` again.
7. **`make verify`** — confirms all EPICS PVs respond correctly over WiFi.
8. Open **`http://<ESP32_IP>/`** for the live dashboard.

---

## Configuring the Project

All settings live in **`project.conf`** at the project root.
Run **`make gen-config`** after every change to propagate them.

| Key | Description | Example |
|---|---|---|
| `FIRMWARE_PORT` | USB serial port for flash/monitor | `/dev/ttyACM0` |
| `FIRMWARE_BAUD` | UART baud rate | `115200` |
| `WIFI_SSID` | 2.4 GHz WiFi SSID (**ESP32 does not support 5 GHz**) | `MyNetwork` |
| `WIFI_PASSWORD` | WiFi WPA2 password | `secret` |
| `TCP_PORT` | ESP32 TCP command port | `7070` |
| `WEBUI_PORT` | ESP32 Web UI HTTP port | `80` |
| `ESP32_LED_GPIO` | GPIO pin for the indicator LED | `2` |
| `EPICS_BASE` | Path to compiled EPICS Base | `/home/.../base-7.0.8.1` |
| `ASYN_PATH` | Path to compiled asyn module | `/home/.../asyn` |
| `STREAM_PATH` | Path to compiled StreamDevice | `/home/.../StreamDevice` |
| `SERIAL_PORT` | Serial port used in EPICS `st.cmd` | `/dev/ttyACM0` |
| `ESP32_IP` | ESP32 WiFi IP (from router/serial monitor, empty = serial only) | `192.168.1.110` |

> **Important**: WiFi credentials are baked into the firmware at build time via the ESP-IDF Kconfig system.
> After changing `WIFI_SSID` or `WIFI_PASSWORD` you **must** run `make gen-config && make fw-flash`.

---

## Build Targets

| Command | Action |
|---|---|
| `make gen-config` | Generate EPICS RELEASE + firmware sdkconfig.defaults from `project.conf` |
| `make all` | Build firmware + IOC |
| `make fw-build` | Build firmware only |
| `make fw-flash` | Flash firmware to ESP32 |
| `make fw-monitor` | Open serial monitor (`Ctrl+]` to quit) |
| `make ioc-build` | Build EPICS IOC |
| `make ioc-run` | Start EPICS IOC |
| `make verify` | Run comprehensive system verification and generate reports |
| `make test-fw` | Flash + run Unity tests (log saved to `logs/unity/`) |
| `make clean` | Clean all build artifacts |

---

## Using EPICS PVs

### LED Control
```bash
caput ESP32:LED 1          # turn LED on
caput ESP32:LED 0          # turn LED off
caget ESP32:LED:RBV        # read current hardware state (shadow register)
```

### System Status
```bash
caget ESP32:SYS:UPTIME     # uptime in seconds  (EGU: s)
caget ESP32:SYS:HEAP       # free heap in bytes  (EGU: bytes)
caget ESP32:SYS:VERSION    # firmware version string
caget ESP32:SYS:STATUS     # uptime in milliseconds (EGU: ms)
caget ESP32:SYS:CYCLE      # FreeRTOS app_task cycle count
caput ESP32:SYS:RESET 1    # trigger firmware soft reset
```

### Task Cycle Times
Monitoring is available for `APP`, `UART`, and `TCP` tasks. All values in **µs**.
```bash
caget ESP32:TASK:APP:MIN   # min cycle time
caget ESP32:TASK:APP:MAX   # max cycle time
caget ESP32:TASK:APP:AVG   # rolling average
caget ESP32:TASK:COUNT     # number of tracked tasks
```

---

## Web UI

Open `http://<ESP32_IP>/` in a browser while the ESP32 is on WiFi.

The dashboard auto-refreshes every second and shows:
- **Live Status** — uptime, free heap, firmware version
- **Task Monitor** — min / avg / max cycle times for APP, UART, and TCP tasks
- **LED Control** — ON / OFF buttons (calls `POST /api/gpio`)

### REST API summary

| Endpoint | Method | Description |
|---|---|---|
| `/api/status` | GET | `{uptime_ms, free_heap, version}` |
| `/api/tasks` | GET | Array of task timing objects `{name, min_us, max_us, avg_us}` |
| `/api/gpio` | GET | `{pin, value}` — current GPIO state |
| `/api/gpio` | POST | `{"pin":<n>, "value":<0|1>}` — set GPIO |
| `/api/config` | GET | `{ssid, tcp_port, webui_port}` — read-only configuration |

---

## Logging and Reports

| Log file | Contents |
|---|---|
| `logs/verification_test.log` | Channel Access status for all PVs |
| `logs/ioc_startup.log` | Live EPICS IOC console output |
| `logs/test_tcp_final.log` | Results of `scripts/test_tcp.py` |
| `logs/test_webui_final.log` | Results of `scripts/test_webui.py` |
| `logs/build_final.log` | Full firmware build trace |

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| ESP32 won't connect to WiFi | SSID is a **5 GHz** network, or `make gen-config` was not run after editing `project.conf` | Use a **2.4 GHz** SSID; run `make gen-config && make fw-flash` |
| `No reply within 1000 ms` in EPICS | Device disconnected, wrong port, or `ESP32_IP` not set | Check `project.conf` and run `nc -zv <ip> 7070` to test TCP |
| `Cannot format value with '%s'` | Protocol syntax error in `esp32.proto` | Ensure the proto file uses `$1` for parameters, not `%s` |
| `LED:GET` always returns 0 | Old firmware without GPIO shadow register | Flash the latest firmware (`make fw-flash`) |
| `TIMEOUT/FAIL` in verify | Hardware not responding | Ensure firmware is flashed and ESP32 is on the correct WiFi network |
| Serial port permission error | User not in `dialout` group | Run `sudo chmod a+rw /dev/ttyACM0` or `sudo adduser $USER dialout` |
