import requests
import socket
import json
import time
import pytest

# Configuration (Assume project.conf defaults or environment variables)
IP = "192.168.1.110"
TCP_PORT = 7070
HTTP_PORT = 80
TIMEOUT = 5

def send_tcp_cmd(cmd):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(TIMEOUT)
        s.connect((IP, TCP_PORT))
        s.sendall((cmd + "\n").encode())
        full_data = b""
        while True:
            try:
                chunk = s.recv(4096)
                if not chunk: break
                full_data += chunk
                # Small wait for trailing data in multi-line responses
                if b"\n" in chunk:
                    s.settimeout(0.5) 
            except socket.timeout:
                break
        return full_data.decode().strip()

def get_rest(path):
    r = requests.get(f"http://{IP}:{HTTP_PORT}{path}", timeout=TIMEOUT)
    return r.json()

def post_rest(path, data):
    r = requests.post(f"http://{IP}:{HTTP_PORT}{path}", json=data, timeout=TIMEOUT)
    return r.json()

# --- System Tests ---
def test_sys_ping():
    assert send_tcp_cmd("SYS:PING") == "PONG"

def test_sys_version():
    assert send_tcp_cmd("SYS:VERSION").startswith("1.")

def test_sys_status():
    res = send_tcp_cmd("SYS:STATUS")
    assert res.startswith("OK")
    uptime = int(res.split()[1])
    assert uptime > 0

def test_sys_cycle():
    assert send_tcp_cmd("SYS:CYCLE 50") == "OK"
    # Verify via REST
    status = get_rest("/api/status")
    assert status['cycle_ms'] == 50
    # Revert
    assert send_tcp_cmd("SYS:CYCLE 100") == "OK"

# --- LED/GPIO Tests ---
def test_led_control():
    assert send_tcp_cmd("LED:SET 1") == "OK"
    assert send_tcp_cmd("LED:GET") == "1"
    assert send_tcp_cmd("LED:SET 0") == "OK"
    assert send_tcp_cmd("LED:GET") == "0"

def test_gpio_raw():
    pin = 4 # Use a safe pin
    assert send_tcp_cmd(f"GPIO:DIR {pin} 0") == "OK" # OUTPUT
    assert send_tcp_cmd(f"GPIO:SET {pin} 1") == "OK"
    assert send_tcp_cmd(f"GPIO:GET {pin}") == "1"
    assert send_tcp_cmd(f"GPIO:SET {pin} 0") == "OK"
    assert send_tcp_cmd(f"GPIO:GET {pin}") == "0"

# --- Peripherals ---
def test_pwm():
    pin = 2 # Usually LED
    # Configure via PIN:CFG if available or just use PWM:SET if PIN:CFG handles it
    # The firmware has PIN:CFG (DEVICE_PIN)
    assert send_tcp_cmd(f"PIN:CFG {pin} 3") == "OK" # 3 = PWM
    assert send_tcp_cmd(f"PWM:SET {pin} 512") == "OK"
    # Verify via REST schema if possible
    schema = get_rest("/api/gpio/schema")
    p_info = next(p for p in schema if p['pin'] == pin)
    assert p_info['role'] == 3
    assert p_info['val'] == 512

def test_dac():
    pin = 25 # DAC1
    assert send_tcp_cmd(f"PIN:CFG {pin} 5") == "OK" # 5 = DAC
    assert send_tcp_cmd(f"DAC:SET {pin} 128") == "OK"
    schema = get_rest("/api/gpio/schema")
    p_info = next(p for p in schema if p['pin'] == pin)
    assert p_info['role'] == 5
    assert p_info['val'] == 128

# --- Task Monitoring ---
def test_tasks():
    res = send_tcp_cmd("TASK:LIST")
    count = int(res)
    assert count > 0
    all_tasks = send_tcp_cmd("TASK:GETALL")
    lines = [L for L in all_tasks.split('\n') if L.strip()]
    assert len(lines) >= count

# --- WebUI API ---
def test_webui_status():
    data = get_rest("/api/status")
    assert 'uptime_ms' in data
    assert 'free_heap' in data
    assert 'version' in data

def test_webui_cycle_post():
    res = post_rest("/api/cycle", {"ms": 200})
    assert res['status'] == 'ok'
    status = get_rest("/api/status")
    assert status['cycle_ms'] == 200
    # Reset
    post_rest("/api/cycle", {"ms": 100})

if __name__ == "__main__":
    pytest.main([__file__])
