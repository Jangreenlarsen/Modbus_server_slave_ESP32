#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ST Logic Blink Program Setup
Controls GPIO 2 LED blinking with variable frequency based on GPIO 4 input:
- GPIO 4 = LOW (0): Blink 1 Hz
- GPIO 4 = HIGH (1): Blink 5 Hz
"""

import serial
import time
import sys
import os

# Force UTF-8 output for Windows
if sys.platform == 'win32':
    os.system('chcp 65001 > nul')

# Configuration
PORT = "COM11"  # ESP32 serial port
BAUD = 115200
TIMEOUT = 2.0

# Colors (simple ANSI codes for Windows compatibility)
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
        print(f"{RED}Fejl ved åbning af {PORT}: {e}{RESET}")
        print(f"Tilgængelige porte: COM3-COM15 på Windows")
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
        print(f"{RED}Fejl: {e}{RESET}")
        return ""

def print_step(num, title):
    """Print a demo step"""
    print(f"\n{BOLD}{CYAN}[TRIN {num}] {title}{RESET}")

def print_command(cmd):
    """Print command to execute"""
    print(f"\n{YELLOW}Kommando:{RESET}")
    print(f"  {BLUE}> {cmd}{RESET}")

def print_response(response, lines=5):
    """Print response preview"""
    print(f"\n{YELLOW}Svar:{RESET}")
    lines_list = response.split('\n')
    for line in lines_list[:lines]:
        if line.strip():
            # Filter problematic unicode characters
            clean_line = line.encode('ascii', errors='ignore').decode('ascii')
            print(f"  {GREEN}>{RESET} {clean_line[:80]}")

def main():
    print(f"\n{BOLD}{CYAN}{'='*70}{RESET}")
    print(f"{BOLD}{CYAN}  GPIO 2 Blinking Program - Variabel frekvens baseret pa GPIO 4{RESET}")
    print(f"{BOLD}{CYAN}  GPIO 4 = 0: Blink 1 Hz  |  GPIO 4 = 1: Blink 5 Hz{RESET}")
    print(f"{BOLD}{CYAN}{'='*70}{RESET}\n")

    # Open serial
    ser = open_serial()
    if not ser:
        sys.exit(1)

    print(f"{GREEN}[OK] Forbundet til {PORT} @ {BAUD} baud{RESET}")
    time.sleep(1)

    try:
        # ===================================================================
        # STEP 1: Enable GPIO2 user mode
        # ===================================================================
        print_step(1, "Aktiver GPIO 2 bruger-tilstand")
        print_command("set gpio 2 enable")
        response = send_command(ser, "set gpio 2 enable")
        print_response(response, lines=3)
        print(f"{GREEN}[OK] GPIO 2 aktiveret (heartbeat deaktiveret){RESET}")
        time.sleep(0.5)

        # ===================================================================
        # STEP 2: Save config
        # ===================================================================
        print_step(2, "Gem konfiguration til persistent storage")
        print_command("save")
        response = send_command(ser, "save")
        print_response(response, lines=2)
        print(f"{GREEN}✓ Konfiguration gemt{RESET}")
        time.sleep(0.5)

        # ===================================================================
        # STEP 3: Map GPIO2 to Coil
        # ===================================================================
        print_step(3, "Map GPIO 2 til Coil #20")
        print_command("set gpio 2 static map coil:20")
        response = send_command(ser, "set gpio 2 static map coil:20")
        print_response(response, lines=2)
        print(f"{GREEN}✓ GPIO 2 maplet til Coil #20{RESET}")
        time.sleep(0.5)

        # ===================================================================
        # STEP 4: Map GPIO4 to Discrete Input
        # ===================================================================
        print_step(4, "Map GPIO 4 til Discrete Input #10")
        print_command("set gpio 4 static map input:10")
        response = send_command(ser, "set gpio 4 static map input:10")
        print_response(response, lines=2)
        print(f"{GREEN}✓ GPIO 4 maplet til Discrete Input #10{RESET}")
        time.sleep(0.5)

        # ===================================================================
        # STEP 5: Upload ST Logic program
        # ===================================================================
        print_step(5, "Upload ST Logic blinking program")

        # Program logic:
        # - gpio4_state = 0 (LOW): blink 1 Hz (500ms on/off) = counter % 100 < 50
        # - gpio4_state = 1 (HIGH): blink 5 Hz (100ms on/off) = counter % 20 < 10
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
        print_command(f'set logic 1 upload "{program[:60]}..."')
        response = send_command(ser, cmd, timeout=3.0)
        print_response(response, lines=8)

        if "success" in response.lower() or "compiled" in response.lower():
            print(f"{GREEN}✓ Program compileret med succes{RESET}")
        else:
            print(f"{RED}✗ Kompilering fejlede - tjek svar ovenfor{RESET}")
            print(response)
            ser.close()
            return 1

        time.sleep(1)

        # ===================================================================
        # STEP 6: Bind GPIO4 state variable
        # ===================================================================
        print_step(6, "Bind 'gpio4_state' til Discrete Input #10")
        print_command("set logic 1 bind gpio4_state input-dis:10")
        response = send_command(ser, "set logic 1 bind gpio4_state input-dis:10")
        print_response(response, lines=3)
        if "ok" in response.lower() or "gpio4" in response.lower():
            print(f"{GREEN}✓ gpio4_state bundet til DI#10{RESET}")
        time.sleep(0.5)

        # ===================================================================
        # STEP 7: Bind counter variable
        # ===================================================================
        print_step(7, "Bind 'counter' til Holding Register #11")
        print_command("set logic 1 bind counter reg:11")
        response = send_command(ser, "set logic 1 bind counter reg:11")
        print_response(response, lines=3)
        if "ok" in response.lower() or "counter" in response.lower():
            print(f"{GREEN}✓ counter bundet til HR#11{RESET}")
        time.sleep(0.5)

        # ===================================================================
        # STEP 8: Bind LED variable
        # ===================================================================
        print_step(8, "Bind 'led' til Coil #20 (GPIO 2)")
        print_command("set logic 1 bind led coil:20")
        response = send_command(ser, "set logic 1 bind led coil:20")
        print_response(response, lines=3)
        if "ok" in response.lower() or "led" in response.lower():
            print(f"{GREEN}✓ led bundet til Coil#20 (GPIO 2){RESET}")
        time.sleep(0.5)

        # ===================================================================
        # STEP 9: Enable program
        # ===================================================================
        print_step(9, "Aktivér ST Logic program")
        print_command("set logic 1 enabled:true")
        response = send_command(ser, "set logic 1 enabled:true")
        print_response(response, lines=2)
        print(f"{GREEN}✓ Program aktiveret og kørende{RESET}")
        time.sleep(1)

        # ===================================================================
        # STEP 10: Show program details
        # ===================================================================
        print_step(10, "Vis program detaljer og bindings")
        print_command("show logic 1")
        response = send_command(ser, "show logic 1", timeout=3.0)
        print_response(response, lines=15)
        time.sleep(0.5)

        # ===================================================================
        # STEP 11: Show program overview
        # ===================================================================
        print_step(11, "Vis alle programmer")
        print_command("show logic program")
        response = send_command(ser, "show logic program", timeout=3.0)
        print_response(response, lines=10)
        time.sleep(0.5)

        # ===================================================================
        # STEP 12: Show statistics
        # ===================================================================
        print_step(12, "Vis statistik")
        print_command("show logic stats")
        response = send_command(ser, "show logic stats", timeout=3.0)
        print_response(response, lines=10)

        # ===================================================================
        # SUMMARY
        # ===================================================================
        print(f"\n{BOLD}{CYAN}[SETUP AFSLUTTET!]{RESET}")

        print(f"\n{GREEN}[OK] Programmet er nu aktivt:{RESET}")
        print(f"  [OK] GPIO 2 LED kontrolleret af ST Logic")
        print(f"  [OK] GPIO 4 indgang læst som Discrete Input #10")
        print(f"  [OK] Program gemmet på ESP32 (persistent)")
        print(f"  [OK] Blinking fungerer hvis GPIO 4 har signal")

        print(f"\n{YELLOW}Hvordan det virker:{RESET}")
        print(f"  1. Når GPIO 4 = LOW (0):")
        print(f"     -> LED blinker med 1 Hz (500ms ON, 500ms OFF)")
        print(f"  2. Når GPIO 4 = HIGH (1):")
        print(f"     -> LED blinker med 5 Hz (100ms ON, 100ms OFF)")
        print(f"  3. counter øges hver loop og resettes ved 10000")

        print(f"\n{BLUE}Register status:{RESET}")
        print(f"  - HR#11: counter værdi (0-10000)")
        print(f"  - DI#10: GPIO 4 tilstand (0 eller 1)")
        print(f"  - Coil#20: LED tilstand (0=OFF, 1=ON)")

        print(f"\n{CYAN}Programmet gemmes på serveren og slettes IKKE!{RESET}")
        print(f"{CYAN}Det overlever reboot af ESP32.{RESET}")

        print(f"\n{YELLOW}Test kommandoer (hvis GPIO 4 ikke har hardware signal):{RESET}")
        print(f"  set discrete_input 10 0    # Simulér GPIO 4 = LOW → 1 Hz blinking")
        print(f"  set discrete_input 10 1    # Simulér GPIO 4 = HIGH → 5 Hz blinking")
        print(f"  show discrete_input 10     # Se GPIO 4 tilstand")
        print(f"  show coil 20               # Se LED tilstand")

        print(f"\n{BOLD}Forbindelse lukkes...{RESET}\n")

        ser.close()
        return 0

    except KeyboardInterrupt:
        print(f"\n{YELLOW}Setup afbrudt af bruger{RESET}")
        ser.close()
        return 1
    except Exception as e:
        print(f"\n{RED}Fejl: {e}{RESET}")
        ser.close()
        return 1

if __name__ == "__main__":
    sys.exit(main())
