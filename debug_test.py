#!/usr/bin/env python3
"""
Debug test for counter reads
"""
import serial
import time
import re

def main():
    try:
        ser = serial.Serial('COM11', 115200, timeout=0.5)
        time.sleep(2)  # Wait for ESP32 boot

        print("=" * 70)
        print("DEBUG: COUNTER READ TEST")
        print("=" * 70)

        # Test 1: Simple register read
        print("\n[TEST 1] Simple read reg 10 1")
        ser.write(b"read reg 10 1\r\n")
        time.sleep(0.5)
        output = ser.read(2000).decode('utf-8', errors='ignore')
        print("RAW OUTPUT:")
        print(repr(output))
        print("FORMATTED:")
        print(output)

        # Test 2: Read 2 registers
        print("\n[TEST 2] Read 2 registers (read reg 10 2)")
        ser.write(b"read reg 10 2\r\n")
        time.sleep(0.5)
        output = ser.read(2000).decode('utf-8', errors='ignore')
        print("RAW OUTPUT:")
        print(repr(output))
        print("FORMATTED:")
        print(output)

        # Test 3: Configure counter, then read
        print("\n[TEST 3] Configure counter, then read")
        cmd = 'set counter 1 mode 1 parameter hw-mode:hw edge:rising prescaler:1 hw-gpio:13 index-reg:10 raw-reg:11 ctrl-reg:14'
        ser.write(f"{cmd}\r\n".encode())
        time.sleep(2)
        output = ser.read(2000).decode('utf-8', errors='ignore')
        print("CONFIG OUTPUT:")
        print(output[:500])

        # Now read register
        print("\n[TEST 4] Read register 10 after config")
        ser.write(b"read reg 10 1\r\n")
        time.sleep(0.5)
        output = ser.read(2000).decode('utf-8', errors='ignore')
        print("RAW OUTPUT:")
        print(repr(output))
        print("FORMATTED:")
        print(output)

        # Try to extract using same regex as test
        print("\n[EXTRACTION TEST]")
        for line in output.split('\n'):
            match = re.search(r'Reg\[10\]:\s*(\d+)', line)
            if match:
                print(f"✓ Found: Reg[10] = {match.group(1)}")
            else:
                if line.strip():
                    print(f"  Line: {repr(line)}")

        ser.close()

    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()

if __name__ == '__main__':
    main()
