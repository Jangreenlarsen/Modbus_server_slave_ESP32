#!/usr/bin/env python3
"""
Trace TIM-03-01 problem - detailed debugging
"""
import serial
import time

def main():
    try:
        ser = serial.Serial('COM11', 115200, timeout=0.5)
        time.sleep(2)

        print("=" * 70)
        print("TRACE: TIMER MODE 3 ASTABLE")
        print("=" * 70)

        # STEP 1: Configure timer 1 mode 3 astable
        print("\n[STEP 1] Configure timer 1 mode 3 on:1000 off:1000 output-coil:200")
        cmd = 'set timer 1 mode 3 on:1000 off:1000 output-coil:200'
        ser.write(f"{cmd}\r\n".encode())
        time.sleep(1.5)
        output = ser.read(2000).decode('utf-8', errors='ignore')
        print("Config output:")
        for line in output.split('\n')[:10]:
            print(f"  {line}")

        # STEP 2: Check initial coil state
        print("\n[STEP 2] Read coil 200 immediately after config")
        for i in range(3):
            ser.write(b"read coil 200 1\r\n")
            time.sleep(0.3)
            output = ser.read(1000).decode('utf-8', errors='ignore')
            for line in output.split('\n'):
                if 'Coil[200]' in line:
                    print(f"  Read {i}: {line.strip()}")
                    break

        # STEP 3: Wait for phase transition (should happen at 1 second)
        print("\n[STEP 3] Wait 1.5s for phase transition and read again")
        time.sleep(1.5)
        for i in range(5):
            ser.write(b"read coil 200 1\r\n")
            time.sleep(0.2)
            output = ser.read(1000).decode('utf-8', errors='ignore')
            for line in output.split('\n'):
                if 'Coil[200]' in line:
                    elapsed = 1500 + (i * 200)
                    print(f"  T+{elapsed}ms: {line.strip()}")
                    break

        # STEP 4: Try to understand state via registers (if possible)
        print("\n[STEP 4] Try reading registers to check timer state")
        ser.write(b"read reg 0 5\r\n")
        time.sleep(0.5)
        output = ser.read(1500).decode('utf-8', errors='ignore')
        print("Register dump:")
        for line in output.split('\n'):
            if 'Reg[' in line:
                print(f"  {line.strip()}")

        # STEP 5: Check if coil 200 can be written manually
        print("\n[STEP 5] Try manually writing coil 200")
        ser.write(b"write coil 200 value 1\r\n")
        time.sleep(0.5)
        output = ser.read(1000).decode('utf-8', errors='ignore')
        print("Write output:", output[:200])

        ser.write(b"read coil 200 1\r\n")
        time.sleep(0.5)
        output = ser.read(1000).decode('utf-8', errors='ignore')
        for line in output.split('\n'):
            if 'Coil[200]' in line:
                print(f"  After manual write: {line.strip()}")
                break

        ser.close()

    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()

if __name__ == '__main__':
    main()
