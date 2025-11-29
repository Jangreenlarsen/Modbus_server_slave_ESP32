#!/usr/bin/env python3
"""
Debug Timer Mode 3
"""
import serial
import time
import re

def main():
    try:
        ser = serial.Serial('COM11', 115200, timeout=0.5)
        time.sleep(2)

        print("=" * 70)
        print("DEBUG: TIMER MODE 3 TEST")
        print("=" * 70)

        # Configure timer 1 mode 3 with 500ms on/off
        print("\n[STEP 1] Configure timer 1 mode 3 (astable)")
        cmd = 'set timer 1 mode 3 on:500 off:500 output-coil:200'
        ser.write(f"{cmd}\r\n".encode())
        time.sleep(1)
        output = ser.read(2000).decode('utf-8', errors='ignore')
        print("OUTPUT:", output[:500])

        # Read coil multiple times
        print("\n[STEP 2] Read coil 200 repeatedly (every 100ms)")
        for i in range(10):
            ser.write(b"read coil 200 1\r\n")
            time.sleep(0.1)
            output = ser.read(1000).decode('utf-8', errors='ignore')

            # Extract coil value
            for line in output.split('\n'):
                if 'Coil[200]' in line or '200:' in line:
                    print(f"  {i*100}ms: {line.strip()}")

        # Try reading multiple coils
        print("\n[STEP 3] Read 2 coils")
        ser.write(b"read coil 200 2\r\n")
        time.sleep(0.5)
        output = ser.read(2000).decode('utf-8', errors='ignore')
        print("OUTPUT:", output)

        ser.close()

    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()

if __name__ == '__main__':
    main()
