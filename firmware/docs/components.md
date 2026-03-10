# Firmware Components

The ESP32 firmware is organized as modular ESP-IDF components.
Each component exposes a clean header-only API and has no circular dependencies.

---

## `hw_hal` — Hardware Abstraction Layer

**Responsibility**: Wraps ESP-IDF GPIO driver calls into a stable, project-specific API.
Decouples application logic from the underlying SDK, making the code portable and unit-testable.

**Key implementation note**: `hw_hal.c` maintains a 64-bit **shadow register** for all GPIO outputs.
`hw_hal_gpio_get()` returns the last value written via `hw_hal_gpio_set()` rather than sampling the physical pin.
This is required because `gpio_get_level()` does not reliably read the output latch on output-only pins.

| Function | Description |
|---|---|
| `hw_hal_gpio_init_output(pin)`| Configure a pin as push-pull output (default LOW) |
| `hw_hal_gpio_set(pin, level)` | Set pin HIGH or LOW + update shadow register |
| `hw_hal_gpio_get(pin)` | Read last-written state from shadow register |
| `hw_hal_pin_cfg(pin, role)` | **Dynamic Configuration**: assign pin as ADC/PWM/DAC/GPIO |
| `hw_hal_dac_set_voltage(p,v)` | Set true analog voltage on DAC-capable pins (25, 26) |
| `hw_hal_init_all()` | Restore all pin roles from NVS on boot |

---

## `utils` — Logging Utilities

**Responsibility**: Standardized logging macros wrapping `ESP_LOG*` with consistent component tags.

| Macro | Equivalent |
|---|---|
| `UTILS_LOGI(TAG, fmt, ...)` | `ESP_LOGI(TAG, fmt, ...)` |
| `UTILS_LOGW(TAG, fmt, ...)` | `ESP_LOGW(TAG, fmt, ...)` |
| `UTILS_LOGE(TAG, fmt, ...)` | `ESP_LOGE(TAG, fmt, ...)` |

---

## `protocol` — ASCII Command Parser

**Responsibility**: Parses raw ASCII lines into typed `protocol_cmd_t` structures and enqueues them on a FreeRTOS queue shared by all transports.

| Function | Description |
|---|---|
| `protocol_parse_and_enqueue(line, fd)` | Parse `"PIN:CFG 13 4"` → enqueue `{DEVICE_PIN, ACTION_CFG, 13, 4, fd}` |
| `protocol_format_response(fd, ...)` | Send a formatted ASCII response back on the correct transport |

**Protocol format**: `<DEVICE>:<ACTION> [VALUE]\n`

---

## `comms` — Communication Transports

**Responsibility**: Lifecycle management for both transport layers.

| Transport | Details |
|---|---|
| **UART** | Always active on UART0 at 115200 baud |
| **WiFi TCP** | Connects using credentials from `sdkconfig` (generated from `project.conf`). Spawns a per-client task on connection. |

Both transports enqueue commands to the **same** `protocol` FreeRTOS queue — the `response_fd` field routes replies back to the correct socket.

**WiFi config flow**: `project.conf` → `make gen-config` → `firmware/sdkconfig.defaults` → `Kconfig.projbuild` symbols → compiled into firmware.

---

## `monitor` — Task Instrumentation

**Responsibility**: Tracks FreeRTOS task execution cycle times at µs resolution using `esp_timer_get_time()`.

| Function | Description |
|---|---|
| `monitor_task_begin(name)` | Record cycle start timestamp |
| `monitor_task_end(name)` | Record end, update min/max/EMA stats |
| `monitor_get_stats(name, min, max, avg)` | Read statistics for a named task |
| `monitor_get_uptime_ms()` | Total uptime in milliseconds |
| `monitor_get_free_heap()` | Current free heap bytes |

Supports up to 8 named tasks. All accesses are mutex-protected.

---

## `webui` — HTTP Dashboard

**Responsibility**: Embedded HTTP server (`esp_http_server`) serving a live dashboard SPA and a REST JSON API.

| Endpoint | Method | Response |
|---|---|---|
| `/api/status` | GET | `{uptime_ms, free_heap, version}` |
| `/api/tasks` | GET | Array of `{name, min_us, max_us, avg_us}` for each task |
| `/api/gpio/schema` | GET | List of all 40 GPIOs with roles, caps, and current levels |
| `/api/pin/cfg` | POST | `{"pin":<n>, "role":<r>}` — persists role to NVS |
| `/api/dac` | POST | `{"pin":<n>, "value":<0-255>}` — set DAC voltage |
| `/api/pwm` | POST | `{"pin":<n>, "duty":<0-1023>}` — set PWM duty |
| `/api/config` | GET | `{ssid, tcp_port}` — core project settings |

The dashboard serves real-time task timings and includes live LED on/off controls.
