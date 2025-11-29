#!/usr/bin/env python3
"""
Simple SYNCHRONOUS timer test (no threading)
"""
import serial
import time

def send_cmd(ser, cmd, wait=0.5):
    """Send command and read response"""
    ser.write(f"{cmd}\r\n".encode())
    time.sleep(wait)

    # Read all available data
    response = []
    while ser.in_waiting:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                response.append(line)
        except:
            pass
    return response

def extract_coil(output, coil_addr):
    """Extract coil value"""
    for line in output:
        if f'Coil[{coil_addr}]:' in line:
            if '1' in line:
                return 1
            elif '0' in line:
                return 0
    return None

def main():
    ser = serial.Serial('COM11', 115200, timeout=0.5)
    time.sleep(2)

    print("=" * 70)
    print("SYNC TIMER TEST (no threading)")
    print("=" * 70)

    # Configure
    print("\n[CONFIG] set timer 1 mode 3 on:1000 off:1000 output-coil:200")
    output = send_cmd(ser, 'set timer 1 mode 3 on:1000 off:1000 output-coil:200', wait=2.0)
    print(f"  Lines: {len(output)}")

    # Read 5 times with 1s interval
    print("\n[MONITOR] Reading coil 200")
    states = []
    for i in range(5):
        output = send_cmd(ser, 'read coil 200 1', wait=0.5)
        state = extract_coil(output, 200)
        if state is not None:
            states.append(state)
            print(f"  [{i}] State={state}")
        else:
            print(f"  [{i}] State=None")
            # Debug
            for line in output:
                if 'Coil' in line:
                    print(f"      Found: {line}")
        time.sleep(1)

    print(f"\nResult: {states}")
    print(f"Expected: Toggle pattern like [1, 0, 1, 0, 1] or [0, 1, 0, 1, 0]")

    ser.close()

if __name__ == '__main__':
    main()
