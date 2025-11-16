#!/usr/bin/env python3
"""
Test script for ESP32 Modbus RTU Server CLI
Tests all show commands and set commands
"""

import serial
import time
import sys

# Configuration
PORT = "COM10"
BAUD = 115200
TIMEOUT = 1.0

# Color output
GREEN = '\033[92m'
RED = '\033[91m'
YELLOW = '\033[93m'
RESET = '\033[0m'

def open_serial():
    """Open serial connection"""
    try:
        ser = serial.Serial(PORT, BAUD, timeout=TIMEOUT)
        time.sleep(0.5)  # Wait for serial to initialize
        return ser
    except Exception as e:
        print(f"{RED}Failed to open {PORT}: {e}{RESET}")
        return None

def send_command(ser, cmd, timeout=2.0):
    """Send command and read response"""
    try:
        # Clear input buffer
        ser.reset_input_buffer()
        time.sleep(0.1)

        # Send command
        ser.write((cmd + '\r').encode())

        # Read response
        response = b''
        start_time = time.time()
        while time.time() - start_time < timeout:
            if ser.in_waiting > 0:
                response += ser.read(ser.in_waiting)
                time.sleep(0.05)

        return response.decode('utf-8', errors='ignore')
    except Exception as e:
        print(f"{RED}Error: {e}{RESET}")
        return ""

def test_command(ser, cmd, description, expected_keywords=None):
    """Test a CLI command"""
    print(f"\n{YELLOW}Testing: {description}{RESET}")
    print(f"  Command: {cmd}")

    response = send_command(ser, cmd)

    if not response:
        print(f"{RED}  [FAIL] No response{RESET}")
        return False

    # Check for expected keywords if provided
    if expected_keywords:
        found_all = all(kw.lower() in response.lower() for kw in expected_keywords)
        if not found_all:
            print(f"{RED}  [FAIL] Missing expected keywords{RESET}")
            print(f"    Expected: {expected_keywords}")
            print(f"    Got: {response[:200]}")
            return False

    # Check for error indicators
    if "error" in response.lower() or "unknown command" in response.lower():
        print(f"{RED}  [FAIL] Command returned error{RESET}")
        print(f"    Response: {response[:200]}")
        return False

    print(f"{GREEN}  [PASS] Success{RESET}")
    print(f"  Response preview: {response[:100]}...")
    return True

def main():
    print(f"{YELLOW}=== ESP32 Modbus RTU Server CLI Test ==={RESET}\n")

    # Open serial connection
    ser = open_serial()
    if not ser:
        sys.exit(1)

    print(f"{GREEN}Connected to {PORT} @ {BAUD} baud{RESET}")

    # Wait for initial setup
    time.sleep(2)

    # Clear any pending data
    ser.reset_input_buffer()
    time.sleep(0.2)

    results = {}

    # Test SHOW commands
    print(f"\n{YELLOW}=== SHOW COMMANDS ==={RESET}")

    tests = [
        ("show version", "Show firmware version", ["version", "esp32"]),
        ("show config", "Show system configuration", ["counter", "config"]),
        ("show counters", "Show counter status", ["counter", "status"]),
        ("show timers", "Show timer status", ["timer"]),
        ("show registers", "Show holding registers", ["register"]),
        ("show coils", "Show coil states", ["coil"]),
        ("show inputs", "Show discrete inputs", ["input"]),
        ("show gpio", "Show GPIO mapping", ["gpio"]),
        ("help", "Show help text", ["help", "command"]),
    ]

    for cmd, desc, keywords in tests:
        results[cmd] = test_command(ser, cmd, desc, keywords)
        time.sleep(0.5)

    # Test SET commands (if implemented)
    print(f"\n{YELLOW}=== SET COMMANDS (if implemented) ==={RESET}")

    set_tests = [
        ("set counter 1 mode 1 parameter", "Set counter 1 mode", None),
    ]

    for cmd, desc, keywords in set_tests:
        # Don't expect success for unimplemented commands
        test_command(ser, cmd, desc, keywords)
        time.sleep(0.5)

    # Summary
    print(f"\n{YELLOW}=== TEST SUMMARY ==={RESET}")

    passed = sum(1 for v in results.values() if v)
    total = len(results)

    print(f"\nPassed: {GREEN}{passed}{RESET}/{total}")

    for cmd, result in results.items():
        status = "[OK]" if result else "[FAIL]"
        color = GREEN if result else RED
        print(f"  {color}{status}{RESET} {cmd}")

    # Close connection
    ser.close()

    if passed == total:
        print(f"\n{GREEN}All tests passed!{RESET}")
        return 0
    else:
        print(f"\n{RED}{total - passed} test(s) failed{RESET}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
