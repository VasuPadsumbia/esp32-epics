#!/usr/bin/env python3
import requests
import sys
import time
import argparse
import socket

def wait_for_port(ip, port, timeout=30):
    print(f"Waiting for {ip}:{port} to become available...")
    start_time = time.time()
    while time.time() - start_time < timeout:
        try:
            with socket.create_connection((ip, port), timeout=1):
                print(f"Port {port} on {ip} is now open.\n")
                return True
        except (socket.timeout, ConnectionRefusedError):
            time.sleep(1)
    print(f"Timeout waiting for {ip}:{port}\n")
    return False

def test_endpoint(ip, method, endpoint, payload=None):
    url = f"http://{ip}{endpoint}"
    print(f"Testing {method} {url}...")
    try:
        start = time.time()
        if method == "GET":
            response = requests.get(url, timeout=5)
        elif method == "POST":
            response = requests.post(url, json=payload, timeout=5)
        else:
            print(f"Unsupported method: {method}")
            return False
            
        latency = (time.time() - start) * 1000
        print(f"    Status Code: {response.status_code} ({latency:.1f}ms)")
        
        try:
            data = response.json()
            print(f"    Response JSON: {data}")
            return response.status_code == 200
        except ValueError:
             print(f"    Response (raw): {response.text[:200]}...")
             return response.status_code == 200
             
    except requests.exceptions.RequestException as e:
        print(f"    Request failed: {e}")
        return False
    print("-" * 40)

def main():
    parser = argparse.ArgumentParser(description="Test ESP32 Web UI Endpoints")
    parser.add_argument("ip", help="IP address of the ESP32")
    args = parser.parse_args()
    ip = args.ip

    print(f"=== Starting Web UI Tests on {ip} ===")
    
    if not wait_for_port(ip, 80, timeout=10):
        print("Web UI server is not accessible. Make sure the ESP32 is on WiFi and the IP is correct.")
        sys.exit(1)

    tests_passed = 0
    total_tests = 0

    endpoints = [
        ("GET", "/"),
        ("GET", "/api/status"),
        ("GET", "/api/tasks"),
        ("GET", "/api/gpio/schema"),       # GET pin schema (not /api/gpio which is POST-only)
        ("POST", "/api/gpio", {"pin": 2, "value": 1}),
        ("GET", "/api/config"),
    ]

    for method, p, *rest in endpoints:
        total_tests += 1
        payload = rest[0] if rest else None
        if test_endpoint(ip, method, p, payload):
            tests_passed += 1
            print("    [PASS]")
        else:
            print("    [FAIL]")
        print("")

    print("=== Web UI Tests Complete ===")
    print(f"Passed {tests_passed} / {total_tests} tests.")
    
    if tests_passed == total_tests:
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
