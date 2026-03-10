#!/usr/bin/env python3
import socket
import sys
import time
import argparse

def test_tcp_command(ip, port, cmd, expected_prefix=None, timeout=2.0):
    print(f"Sending: {cmd.strip()}")
    try:
        with socket.create_connection((ip, port), timeout=timeout) as s:
            s.sendall(cmd.encode("ascii"))
            response = s.recv(1024).decode("ascii").strip()
            print(f"    Received: {response}")
            
            if expected_prefix:
                if response.startswith(expected_prefix):
                    return True, response
                else:
                    print(f"    [FAIL] Expected prefix '{expected_prefix}', got '{response}'")
                    return False, response
            return True, response
    except Exception as e:
        print(f"    [FAIL] Connection error: {e}")
        return False, None

def main():
    parser = argparse.ArgumentParser(description="Test ESP32 Advanced Peripherals (Phase 7)")
    parser.add_argument("ip", help="IP address of the ESP32")
    parser.add_argument("--port", type=int, default=7070, help="TCP port (default: 7070)")
    args = parser.parse_args()

    ip = args.ip
    port = args.port

    print(f"=== Starting Peripheral Tests on {ip}:{port} ===")
    
    # 1. PWM Test
    print("\n--- Testing PWM ---")
    # Config PWM on Pin 2, Channel 0, 5000Hz
    test_tcp_command(ip, port, "PWM:CFG 2 0 5000\n", "OK")
    # Set Duty to 512 (50%)
    test_tcp_command(ip, port, "PWM:SET 0 512\n", "OK")
    
    # 2. ADC Test
    print("\n--- Testing ADC ---")
    # Read ADC value directly (no config needed)
    test_tcp_command(ip, port, "AI:GET 34\n", "")

    # 3. I2C Test
    print("\n--- Testing I2C ---")
    # Config I2C SDA=21, SCL=22, Speed=100000
    test_tcp_command(ip, port, "I2C:CFG 21 22 100000\n", "OK")

    # 4. UART2 Test
    print("\n--- Testing UART2 ---")
    # Config UART2 TX=17, RX=16, Baud=9600
    test_tcp_command(ip, port, "UART2:CFG 17 16 9600\n", "OK")

    print("\n=== Peripheral Tests Complete ===")


if __name__ == "__main__":
    main()
