#!/usr/bin/env python3
import socket
import sys
import time
import argparse

def test_tcp_command(ip, port, cmd, expected_response=None, timeout=2.0):
    print(f"Sending: {cmd.strip()}")
    try:
        with socket.create_connection((ip, port), timeout=timeout) as s:
            s.sendall(cmd.encode("ascii"))
            response = s.recv(1024).decode("ascii").strip()
            print(f"    Received: {response}")
            
            if expected_response:
                if response == expected_response or (expected_response == "OK" and response.startswith("OK")):
                    return True
                else:
                    print(f"    [FAIL] Expected '{expected_response}', got '{response}'")
                    return False
            return True
    except Exception as e:
        print(f"    [FAIL] Connection error: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description="Test ESP32 TCP Socket Communications")
    parser.add_argument("ip", help="IP address of the ESP32")
    parser.add_argument("--port", type=int, default=7070, help="TCP port (default: 7070)")
    args = parser.parse_args()

    ip = args.ip
    port = args.port

    print(f"=== Starting TCP Socket Tests on {ip}:{port} ===")
    
    # Simple ping test
    if not test_tcp_command(ip, port, "SYS:PING\n", "PONG"):
        print("\nFailed to ping device. Make sure it's running and the port is correct.")
        sys.exit(1)

    tests = [
        ("SYS:VERSION\n", None),
        ("SYS:STATUS\n", "OK"),
        ("LED:SET 1\n", "OK"),
        ("SLEEP", 0.3),           # small delay for firmware to process
        ("LED:GET\n", "1"),
        ("LED:SET 0\n", "OK"),
        ("LED:GET\n", "0"),
        ("TASK:LIST\n", None),
        ("TASK:GET APP\n", None),
        ("TASK:GETALL\n", None)
    ]

    passed = 1 # count the ping
    # count only real command tests (not SLEEP entries)
    total = sum(1 for c, _ in tests if not c.startswith("SLEEP")) + 1

    for cmd, expected in tests:
        if cmd == "SLEEP":
            time.sleep(expected)
            continue
        time.sleep(0.1) # brief pause between commands
        if test_tcp_command(ip, port, cmd, expected):
            passed += 1
            print("    [PASS]\n")
        else:
            print("\n")

    print(f"=== TCP Socket Tests Complete ===")
    print(f"Passed {passed} / {total} tests.")

    if passed == total:
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
