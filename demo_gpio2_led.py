#!/usr/bin/env python3
"""
GPIO2 LED Demo for ST Logic Mode
Automatically demonstrates LED control via ST Logic
"""

import serial
import time
import sys

# Configuration
PORT = "COM10"  # CHANGE THIS to your ESP32 port
BAUD = 115200
TIMEOUT = 2.0

# Colors
GREEN = '\033[92m'
RED = '\033[91m'
YELLOW = '\033[93m'
BLUE = '\033[94m'
CYAN = '\033[96m'
RESET = '\033[0m'
BOLD = '\033[1m'

def open_serial():
    """Open serial connection"""
    try:
        ser = serial.Serial(PORT, BAUD, timeout=TIMEOUT)
        time.sleep(0.5)
        return ser
    except Exception as e:
        print(f"{RED}Failed to open {PORT}: {e}{RESET}")
        print(f"Available ports: Try COM3-COM15 on Windows, /dev/ttyUSB0 on Linux")
        return None

def send_command(ser, cmd, timeout=2.0):
    """Send command and read response"""
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
        print(f"{RED}Error: {e}{RESET}")
        return ""

def print_step(num, title):
    """Print a demo step"""
    print(f"\n{BOLD}{CYAN}{'='*60}{RESET}")
    print(f"{BOLD}{CYAN}STEP {num}: {title}{RESET}")
    print(f"{BOLD}{CYAN}{'='*60}{RESET}")

def print_command(cmd):
    """Print command to execute"""
    print(f"\n{YELLOW}Command:{RESET}")
    print(f"  {BLUE}→ {cmd}{RESET}")

def print_response(response, lines=5):
    """Print response preview"""
    print(f"\n{YELLOW}Response:{RESET}")
    lines_list = response.split('\n')
    for line in lines_list[:lines]:
        if line.strip():
            print(f"  {GREEN}>{RESET} {line[:70]}")

def check_success(response, keywords):
    """Check if response contains success keywords"""
    for kw in keywords:
        if kw.lower() not in response.lower():
            return False
    return True

def main():
    print(f"\n{BOLD}{CYAN}╔{'='*58}╗{RESET}")
    print(f"{BOLD}{CYAN}║{' '*58}║{RESET}")
    print(f"{BOLD}{CYAN}║  {' '*54}  ║{RESET}")
    print(f"{BOLD}{CYAN}║     ST Logic Mode - GPIO2 LED Demonstration     ║{RESET}")
    print(f"{BOLD}{CYAN}║  {' '*54}  ║{RESET}")
    print(f"{BOLD}{CYAN}║{' '*58}║{RESET}")
    print(f"{BOLD}{CYAN}╚{'='*58}╝{RESET}\n")

    # Open serial
    ser = open_serial()
    if not ser:
        sys.exit(1)

    print(f"{GREEN}✓ Connected to {PORT} @ {BAUD} baud{RESET}")
    print(f"{YELLOW}Please open serial monitor to see LED changes (pio device monitor){RESET}")
    time.sleep(2)

    try:
        # ===================================================================
        # STEP 1: Enable GPIO2 user mode
        # ===================================================================
        print_step(1, "Enable GPIO2 User Mode (disable heartbeat)")
        print_command("set gpio2 user_mode:true")

        response = send_command(ser, "set gpio2 user_mode:true")
        print_response(response)

        if check_success(response, ["ok", "user"]):
            print(f"{GREEN}✓ GPIO2 user mode enabled{RESET}")
        else:
            print(f"{YELLOW}⚠ Check serial monitor for GPIO2 status{RESET}")

        time.sleep(1)

        # ===================================================================
        # STEP 2: Upload ST Logic program
        # ===================================================================
        print_step(2, "Upload ST Logic Program")
        print(f"{YELLOW}Program logic:{RESET}")
        print(f"  {BLUE}IF counter > 50 THEN led := TRUE")
        print(f"         ELSE led := FALSE")
        print(f"  END_IF{RESET}")

        program = 'VAR led: BOOL; counter: INT; END_VAR IF counter > 50 THEN led := TRUE; ELSE led := FALSE; END_IF;'
        cmd = f'set logic 1 upload "{program}"'
        print_command(f'set logic 1 upload "{program[:50]}..."')

        response = send_command(ser, cmd)
        print_response(response, lines=6)

        if check_success(response, ["compiled", "success"]):
            print(f"{GREEN}✓ Program compiled successfully{RESET}")
        else:
            print(f"{RED}✗ Compilation failed{RESET}")
            print(response)
            ser.close()
            return 1

        time.sleep(1)

        # ===================================================================
        # STEP 3: Bind counter variable to register
        # ===================================================================
        print_step(3, "Bind 'counter' variable to Holding Register #10")
        print_command("set logic 1 bind counter reg:10")

        response = send_command(ser, "set logic 1 bind counter reg:10")
        print_response(response)

        if check_success(response, ["ok", "counter"]):
            print(f"{GREEN}✓ Counter bound to HR#10{RESET}")
        else:
            print(f"{RED}✗ Binding failed{RESET}")

        time.sleep(1)

        # ===================================================================
        # STEP 4: Bind LED variable to GPIO2 coil
        # ===================================================================
        print_step(4, "Bind 'led' variable to Coil #0 (GPIO2)")
        print_command("set logic 1 bind led coil:0")

        response = send_command(ser, "set logic 1 bind led coil:0")
        print_response(response)

        if check_success(response, ["ok", "led"]):
            print(f"{GREEN}✓ LED bound to Coil#0 (GPIO2){RESET}")
        else:
            print(f"{RED}✗ Binding failed{RESET}")

        time.sleep(1)

        # ===================================================================
        # STEP 5: Enable program
        # ===================================================================
        print_step(5, "Enable Logic Program")
        print_command("set logic 1 enabled:true")

        response = send_command(ser, "set logic 1 enabled:true")
        print_response(response)

        if check_success(response, ["enabled", "ok"]):
            print(f"{GREEN}✓ Program enabled (running){RESET}")
        else:
            print(f"{YELLOW}⚠ Check status{RESET}")

        time.sleep(1)

        # ===================================================================
        # STEP 6: Show program details
        # ===================================================================
        print_step(6, "Show Program Details & Bindings")
        print_command("show logic 1")

        response = send_command(ser, "show logic 1", timeout=3.0)
        print_response(response, lines=10)

        if check_success(response, ["binding", "logic1"]):
            print(f"{GREEN}✓ Program details displayed{RESET}")

        time.sleep(1)

        # ===================================================================
        # STEP 7: Test - LED ON (counter > 50)
        # ===================================================================
        print_step(7, "TEST A: Turn LED ON (set counter = 75)")
        print(f"\n{YELLOW}Since 75 > 50, the condition is TRUE → LED should turn ON 🔵{RESET}")
        print_command("set holding_register 10 75")

        response = send_command(ser, "set holding_register 10 75")
        print_response(response)

        print(f"\n{BOLD}{GREEN}🔵 WATCH the serial monitor - LED should be ON!{RESET}")
        time.sleep(3)

        # ===================================================================
        # STEP 8: Test - LED OFF (counter < 50)
        # ===================================================================
        print_step(8, "TEST B: Turn LED OFF (set counter = 25)")
        print(f"\n{YELLOW}Since 25 <= 50, the condition is FALSE → LED should turn OFF ⚫{RESET}")
        print_command("set holding_register 10 25")

        response = send_command(ser, "set holding_register 10 25")
        print_response(response)

        print(f"\n{BOLD}{RED}⚫ WATCH the serial monitor - LED should be OFF!{RESET}")
        time.sleep(3)

        # ===================================================================
        # STEP 9: Test - LED ON again
        # ===================================================================
        print_step(9, "TEST C: Turn LED ON again (set counter = 100)")
        print(f"\n{YELLOW}Since 100 > 50, the condition is TRUE → LED should turn ON 🔵{RESET}")
        print_command("set holding_register 10 100")

        response = send_command(ser, "set holding_register 10 100")
        print_response(response)

        print(f"\n{BOLD}{GREEN}🔵 WATCH the serial monitor - LED should be ON again!{RESET}")
        time.sleep(3)

        # ===================================================================
        # STEP 10: Show final program overview
        # ===================================================================
        print_step(10, "Show Program Overview")
        print_command("show logic program")

        response = send_command(ser, "show logic program", timeout=3.0)
        print_response(response, lines=8)

        # ===================================================================
        # STEP 11: Show statistics
        # ===================================================================
        print_step(11, "Show Statistics")
        print_command("show logic stats")

        response = send_command(ser, "show logic stats", timeout=3.0)
        print_response(response, lines=8)

        # ===================================================================
        # SUMMARY
        # ===================================================================
        print(f"\n{BOLD}{CYAN}{'='*60}{RESET}")
        print(f"{BOLD}{CYAN}DEMO COMPLETE!{RESET}")
        print(f"{BOLD}{CYAN}{'='*60}{RESET}")

        print(f"\n{GREEN}✓ Successfully demonstrated:{RESET}")
        print(f"  ✓ GPIO2 user mode activation")
        print(f"  ✓ ST Logic program upload and compilation")
        print(f"  ✓ Variable binding (register + coil)")
        print(f"  ✓ Program enabling and execution")
        print(f"  ✓ LED control via ST Logic (ON/OFF)")

        print(f"\n{YELLOW}What happened:{RESET}")
        print(f"  1. GPIO2 LED controlled by ST Logic program")
        print(f"  2. Program reads 'counter' from Holding Register #10")
        print(f"  3. IF counter > 50 → sets 'led' = TRUE → Coil#0 → GPIO2 ON")
        print(f"  4. ELSE → sets 'led' = FALSE → Coil#0 → GPIO2 OFF")
        print(f"  5. Changes in HR#10 instantly affect LED state")

        print(f"\n{BLUE}Try these next:{RESET}")
        print(f"  • Upload a different ST program to Logic2, Logic3, Logic4")
        print(f"  • Use 'show logic errors' to practice error diagnostics")
        print(f"  • Bind multiple variables to different registers")
        print(f"  • Check 'show logic program' for program overview")

        print(f"\n{BOLD}Demo finished! Connection closing...{RESET}\n")

        ser.close()
        return 0

    except KeyboardInterrupt:
        print(f"\n{YELLOW}Demo interrupted by user{RESET}")
        ser.close()
        return 1
    except Exception as e:
        print(f"\n{RED}Error: {e}{RESET}")
        ser.close()
        return 1

if __name__ == "__main__":
    sys.exit(main())
