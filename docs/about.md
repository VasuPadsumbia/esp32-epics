# Project Purpose and Limitations

## Purpose
The **ESP32 EPICS Integration** project is designed to provide a robust, scalable, and professional-grade bridge between the **ESP32 WROOM 32U** microcontroller and an **EPICS (Experimental Physics and Industrial Control System) IOC**.

Unlike simple Arduino-style serial integrations, this project leverages:
- **FreeRTOS** for concurrent task management on the ESP32.
- **ESP-IDF** for low-level hardware control and networking.
- **Modular Firmware Architecture** to allow independent development and testing of components (HAL, Protocol, Comms, Monitor).
- **Dual Transport Support**: Communication via both USB (UART) and WiFi (TCP/IP).
- **Embedded Web UI** for live monitoring and hardware field configuration.
- **In-depth Telemetry**: Real-time task cycle-time monitoring to ensure real-time performance and system health.

This project is intended for laboratory automation, industrial control, and experimental setups where ESP32's versatility and low cost can be combined with EPICS' powerful distributed control capabilities.

## Limitations

### Hardware Limitations
- **Processing Power**: While the ESP32 is a dual-core 240MHz MCU, it is not a 1GHz+ Linux host. Excessive logging or very high-frequency polling from EPICS (100Hz+) might cause task cycle-time jitter.
- **Memory**: The 520KB SRAM is limited. Large Web UI resources or heavy TCP client connections can exhaust memory.
- **WiFi Reliability**: WiFi and Bluetooth share the same 2.4GHz antenna. High network congestion can affect TCP communication latency.

### Software Limitations
- **Security**: Current implementation uses plain HTTP and TCP without SSL/TLS. It should only be deployed on trusted laboratory/local networks.
- **Protocol**: The ASCII protocol is designed for simplicity. For extremely high-throughput data streaming (e.g., fast waveform capture), a custom binary protocol or `asynPortDriver` with NDArray support would be preferred.
- **EPICS Version**: Tested primarily against EPICS Base 7.0.8.1.

## Installation Guidelines

### ESP-IDF Installation
Official guidelines: [ESP-IDF Get Started](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/)
1. Prerequisites: `sudo apt install git wget flex bison gperf python3 python3-pip python3-setuptools cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0`
2. Download and run `install.sh`.
3. Source `export.sh` before building.

### EPICS Installation
Official guidelines: [EPICS Installation](https://epics-controls.org/resources-and-support/base/)
1. Download Base.
2. Build with `make`.
3. Install support modules (`asyn`, `StreamDevice`) following their respective docs.
