# Installation Guide

## Hardware Requirements

| Item | Spec |
|---|---|
| Microcontroller | ESP32 WROOM 32U (any standard ESP32 DevKit carrier board) |
| USB cable | Micro-USB (for flash / monitor / UART command channel) |
| LED + resistor | Any LED + 330Ω resistor on GPIO 2 (default) |
| WiFi router | **2.4 GHz WPA2** — the ESP32 hardware does **not** support 5 GHz |

---

## Software Requirements

| Tool | Version | Notes |
|---|---|---|
| ESP-IDF | v5.5+ | Xtensa toolchain included |
| EPICS Base | 7.0.8.1+ | Must be compiled for `linux-x86_64` |
| asyn | 4-45+ | EPICS support module |
| StreamDevice | 2.8+ | EPICS support module |
| Python | 3.10+ | For test scripts and `pyepics` |
| pyepics | 3.5+ | `pip install pyepics` |
| requests | 2.28+ | `pip install requests` (for `test_webui.py`) |

---

## 1 — Install ESP-IDF

> Official guide: **https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/**

```bash
mkdir -p ~/.espressif
git clone --depth 1 --branch v5.5 \
    https://github.com/espressif/esp-idf.git \
    ~/.espressif/v5.5.3/esp-idf

~/.espressif/v5.5.3/esp-idf/install.sh esp32
```

Update `project.conf`:
```ini
IDF_PATH = /home/<user>/.espressif/v5.5.3/esp-idf
IDF_VENV = /home/<user>/.espressif/python_env/idf5.5_py3.12_env
```

---

## 2 — Build EPICS Base

> Official guide: **https://epics-controls.org/resources-and-support/base/**

```bash
mkdir -p ~/EPICS && cd ~/EPICS
wget https://epics.anl.gov/download/base/base-7.0.8.1.tar.gz
tar -xzf base-7.0.8.1.tar.gz && cd base-7.0.8.1
make
```

Update `project.conf`:
```ini
EPICS_BASE = /home/<user>/EPICS/base-7.0.8.1
```

---

## 3 — Build asyn

> Repository: **https://github.com/epics-modules/asyn**

```bash
git clone https://github.com/epics-modules/asyn.git ~/EPICS/support/asyn
cd ~/EPICS/support/asyn
echo "EPICS_BASE=~/EPICS/base-7.0.8.1" >> configure/RELEASE
make
```

Update `project.conf`:
```ini
ASYN_PATH = /home/<user>/EPICS/support/asyn
```

---

## 4 — Build StreamDevice

> Documentation: **https://paulscherrerinstitute.github.io/StreamDevice/**

```bash
git clone https://github.com/paulscherrerinstitute/StreamDevice.git \
    ~/EPICS/support/stream/StreamDevice
cd ~/EPICS/support/stream/StreamDevice
# Add EPICS_BASE and ASYN to configure/RELEASE
make
```

Update `project.conf`:
```ini
STREAM_PATH = /home/<user>/EPICS/support/stream/StreamDevice
```

---

## 5 — Configure and Build the Project

```bash
cd epics_esp32_project/

# 1. Fill in project.conf (WiFi SSID, paths, port, etc.)
# 2. Generate derived config files
make gen-config

# 3. Build everything
make all

# 4. Flash the ESP32
make fw-flash
```

> **WiFi note**: After every change to `WIFI_SSID` or `WIFI_PASSWORD` you must
> run `make gen-config && make fw-flash` to bake the new credentials into the firmware.

---

## 6 — Discover the ESP32 IP Address

Open the serial monitor immediately after flashing:
```bash
make fw-monitor
```
Watch for the line:
```
I (2634) COMMS_WIFI: Got IP: 192.168.1.xxx
```
Then update `project.conf`:
```ini
ESP32_IP = 192.168.1.xxx
```
And regenerate:
```bash
make gen-config
```

---

## 7 — Start the EPICS IOC and Verify

```bash
make ioc-run     # start IOC (Ctrl+C to stop)

# In another terminal:
make verify      # runs automated PV checks
# — or test directly:
python3 scripts/test_tcp.py   <ESP32_IP>
python3 scripts/test_webui.py <ESP32_IP>
```

Open **`http://<ESP32_IP>/`** for the live dashboard.
