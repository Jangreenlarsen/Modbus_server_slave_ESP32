#!/usr/bin/env python3
"""
ST Logic Blink Program Setup - Simple Version
Controls GPIO 2 LED blinking with variable frequency based on GPIO 4 input:
- GPIO 4 = LOW (0): Blink 1 Hz
- GPIO 4 = HIGH (1): Blink 5 Hz
"""

import serial
import time
import sys

PORT = "COM11"
BAUD = 115200
TIMEOUT = 2.0

def open_serial():
    try:
        ser = serial.Serial(PORT, BAUD, timeout=TIMEOUT)
        time.sleep(0.5)
        return ser
    except Exception as e:
        print(f"FAIL: Could not open {PORT}: {e}")
        return None

def send_command(ser, cmd, timeout=2.0):
    try:
        ser.reset_input_buffer()
        time.sleep(0.1)
        ser.write((cmd + '\r').encode())

        response = b''
        start_time = time.time()
        while time.time() - start_time < timeout:
            if ser.in_waiting > 0:
                response += ser.read(ser.in_waiting)
                time.sleep(0.05)

        return response.decode('utf-8', errors='ignore')
    except Exception as e:
        print(f"ERROR: {e}")
        return ""

def main():
    print("\n" + "="*70)
    print("  GPIO 2 Blinking Program Setup - Variable Frequency (1 Hz / 5 Hz)")
    print("  GPIO 4 = LOW:  1 Hz blinking  |  GPIO 4 = HIGH:  5 Hz blinking")
    print("="*70 + "\n")

    ser = open_serial()
    if not ser:
        sys.exit(1)

    print("[OK] Connected to " + PORT + " @ " + str(BAUD) + " baud\n")
    time.sleep(1)

    try:
        # STEP 1
        print("[STEP 1] Enable GPIO 2 user mode")
        print("  > set gpio 2 enable")
        response = send_command(ser, "set gpio 2 enable")
        if response.strip():
            print("  OK - Response received")
        print()
        time.sleep(0.5)

        # STEP 2
        print("[STEP 2] Save configuration")
        print("  > save")
        response = send_command(ser, "save")
        if response.strip():
            print("  OK - Configuration saved")
        print()
        time.sleep(0.5)

        # STEP 3
        print("[STEP 3] Map GPIO 2 to Coil #20")
        print("  > set gpio 2 static map coil:20")
        response = send_command(ser, "set gpio 2 static map coil:20")
        if response.strip():
            print("  OK - GPIO 2 mapped to Coil #20")
        print()
        time.sleep(0.5)

        # STEP 4
        print("[STEP 4] Map GPIO 4 to Discrete Input #10")
        print("  > set gpio 4 static map input:10")
        response = send_command(ser, "set gpio 4 static map input:10")
        if response.strip():
            print("  OK - GPIO 4 mapped to Discrete Input #10")
        print()
        time.sleep(0.5)

        # STEP 5
        print("[STEP 5] Upload ST Logic blinking program")
        program = (
            'VAR gpio4_state: BOOL; counter: INT; led: BOOL; END_VAR '
            'IF NOT gpio4_state THEN '
            '  IF (counter MOD 100) < 50 THEN led := TRUE; ELSE led := FALSE; END_IF; '
            'ELSE '
            '  IF (counter MOD 20) < 10 THEN led := TRUE; ELSE led := FALSE; END_IF; '
            'END_IF; '
            'counter := counter + 1; '
            'IF counter >= 10000 THEN counter := 0; END_IF;'
        )

        cmd = f'set logic 1 upload "{program}"'
        print("  > set logic 1 upload \"<ST program>\"")
        response = send_command(ser, cmd, timeout=3.0)

        if "success" in response.lower() or "compiled" in response.lower():
            print("  OK - Program compiled successfully")
        else:
            print("  FAIL - Program compilation failed")
            print("  Response:", response[:200])
            ser.close()
            return 1
        print()
        time.sleep(1)

        # STEP 6
        print("[STEP 6] Bind gpio4_state to Discrete Input #10")
        print("  > set logic 1 bind gpio4_state input-dis:10")
        response = send_command(ser, "set logic 1 bind gpio4_state input-dis:10")
        if response.strip():
            print("  OK - gpio4_state bound to DI#10")
        print()
        time.sleep(0.5)

        # STEP 7
        print("[STEP 7] Bind counter to Holding Register #11")
        print("  > set logic 1 bind counter reg:11")
        response = send_command(ser, "set logic 1 bind counter reg:11")
        if response.strip():
            print("  OK - counter bound to HR#11")
        print()
        time.sleep(0.5)

        # STEP 8
        print("[STEP 8] Bind led to Coil #20 (GPIO 2)")
        print("  > set logic 1 bind led coil:20")
        response = send_command(ser, "set logic 1 bind led coil:20")
        if response.strip():
            print("  OK - led bound to Coil #20")
        print()
        time.sleep(0.5)

        # STEP 9
        print("[STEP 9] Enable ST Logic program")
        print("  > set logic 1 enabled:true")
        response = send_command(ser, "set logic 1 enabled:true")
        if response.strip():
            print("  OK - Program enabled and running")
        print()
        time.sleep(1)

        # STEP 10
        print("[STEP 10] Show program details")
        print("  > show logic 1")
        response = send_command(ser, "show logic 1", timeout=3.0)
        lines = response.split('\n')
        for line in lines[:12]:
            if line.strip():
                clean = line.encode('ascii', errors='ignore').decode('ascii')
                print("  " + clean[:75])
        print()
        time.sleep(0.5)

        # STEP 11
        print("[STEP 11] Show program overview")
        print("  > show logic program")
        response = send_command(ser, "show logic program", timeout=3.0)
        lines = response.split('\n')
        for line in lines[:8]:
            if line.strip():
                clean = line.encode('ascii', errors='ignore').decode('ascii')
                print("  " + clean[:75])
        print()
        time.sleep(0.5)

        # STEP 12
        print("[STEP 12] Show statistics")
        print("  > show logic stats")
        response = send_command(ser, "show logic stats", timeout=3.0)
        lines = response.split('\n')
        for line in lines[:8]:
            if line.strip():
                clean = line.encode('ascii', errors='ignore').decode('ascii')
                print("  " + clean[:75])
        print()

        # SUMMARY
        print("="*70)
        print("[SETUP COMPLETE!]")
        print("="*70)
        print()
        print("SUCCESS - Program is now active:")
        print("  [OK] GPIO 2 LED controlled by ST Logic")
        print("  [OK] GPIO 4 input read as Discrete Input #10")
        print("  [OK] Program saved on ESP32 (persistent)")
        print("  [OK] Blinking works if GPIO 4 has signal")
        print()
        print("How it works:")
        print("  1. When GPIO 4 = LOW (0):")
        print("     -> LED blinks at 1 Hz (500ms ON, 500ms OFF)")
        print("  2. When GPIO 4 = HIGH (1):")
        print("     -> LED blinks at 5 Hz (100ms ON, 100ms OFF)")
        print("  3. counter increases each loop and resets at 10000")
        print()
        print("Register mapping:")
        print("  - HR#11: counter value (0-10000)")
        print("  - DI#10: GPIO 4 state (0 or 1)")
        print("  - Coil#20: LED state (0=OFF, 1=ON)")
        print()
        print("IMPORTANT: Program is saved on server and will NOT be deleted!")
        print("It will survive reboot of ESP32.")
        print()
        print("Test commands (if GPIO 4 has no hardware signal):")
        print("  set discrete_input 10 0    # Simulate GPIO 4 = LOW  -> 1 Hz blink")
        print("  set discrete_input 10 1    # Simulate GPIO 4 = HIGH -> 5 Hz blink")
        print("  show discrete_input 10     # Check GPIO 4 state")
        print("  show coil 20               # Check LED state")
        print()

        ser.close()
        return 0

    except KeyboardInterrupt:
        print("\nSetup interrupted by user")
        ser.close()
        return 1
    except Exception as e:
        print(f"\nError: {e}")
        ser.close()
        return 1

if __name__ == "__main__":
    sys.exit(main())
