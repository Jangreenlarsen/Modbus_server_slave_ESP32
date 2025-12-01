#!/usr/bin/env python3
"""
Comprehensive test script for ST Logic Mode
Tests all new features: bindings, errors, program overview, etc.
"""

import serial
import time
import sys

# Configuration
PORT = "COM10"
BAUD = 115200
TIMEOUT = 2.0

# Color output
GREEN = '\033[92m'
RED = '\033[91m'
YELLOW = '\033[93m'
BLUE = '\033[94m'
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

def test_command(ser, cmd, description, expected_keywords=None, should_fail=False):
    """Test a CLI command"""
    print(f"\n  {YELLOW}→ {description}{RESET}")
    print(f"    Command: {cmd}")

    response = send_command(ser, cmd)

    if not response:
        print(f"    {RED}[FAIL] No response{RESET}")
        return False

    # Check for expected keywords
    if expected_keywords:
        found_all = all(kw.lower() in response.lower() for kw in expected_keywords)
        if not found_all:
            if should_fail:
                print(f"    {GREEN}[PASS] Expected failure{RESET}")
                return True
            print(f"    {RED}[FAIL] Missing keywords{RESET}")
            print(f"    Expected: {expected_keywords}")
            print(f"    Got (first 200 chars): {response[:200]}")
            return False

    # Check for errors (unless expecting failure)
    if not should_fail and ("error" in response.lower() or "unknown" in response.lower()):
        if "logic program" not in response.lower():  # Allow "no logic programs"
            print(f"    {RED}[FAIL] Command error{RESET}")
            print(f"    Response: {response[:200]}")
            return False

    print(f"    {GREEN}[PASS]{RESET}")
    # Show response preview (first 150 chars)
    lines = response.split('\n')
    for line in lines[:3]:
        if line.strip():
            print(f"    {BLUE}>{RESET} {line[:80]}")

    return True

def main():
    print(f"\n{BOLD}{YELLOW}╔══════════════════════════════════════════════╗{RESET}")
    print(f"{BOLD}{YELLOW}║   ST Logic Mode - Comprehensive Test Suite   ║{RESET}")
    print(f"{BOLD}{YELLOW}╚══════════════════════════════════════════════╝{RESET}\n")

    # Open serial connection
    ser = open_serial()
    if not ser:
        sys.exit(1)

    print(f"{GREEN}✓ Connected to {PORT} @ {BAUD} baud{RESET}")
    time.sleep(2)
    ser.reset_input_buffer()
    time.sleep(0.2)

    results = {}

    # ===================================================================
    # TEST 1: Show commands (overview and diagnostics)
    # ===================================================================
    print(f"\n{BOLD}{YELLOW}TEST 1: Show Commands (Program Overview & Diagnostics){RESET}")
    print("=" * 50)

    tests = [
        ("show logic program", "Program overview with status icons", ["logic", "program"]),
        ("show logic all", "All programs summary", ["logic", "program"]),
        ("show logic errors", "Programs with errors only", ["logic", "error"]),
        ("show logic stats", "Engine statistics", ["logic", "statistic"]),
    ]

    for cmd, desc, keywords in tests:
        results[cmd] = test_command(ser, cmd, desc, keywords)
        time.sleep(0.5)

    # ===================================================================
    # TEST 2: Upload valid ST program
    # ===================================================================
    print(f"\n{BOLD}{YELLOW}TEST 2: Upload and Compile Programs{RESET}")
    print("=" * 50)

    # Test 2a: Valid program
    print(f"\n  {YELLOW}2a. Valid ST program{RESET}")
    valid_program = 'VAR counter: INT; led: BOOL; END_VAR IF counter > 50 THEN led := TRUE; ELSE led := FALSE; END_IF;'
    cmd = f'set logic 1 upload "{valid_program}"'
    results["upload_valid"] = test_command(
        ser, cmd,
        "Upload valid program",
        ["compiled", "successfully", "instruction"],
        should_fail=False
    )
    time.sleep(0.5)

    # Test 2b: Show the uploaded program
    results["show_logic_1"] = test_command(
        ser, "show logic 1",
        "Show detailed program info",
        ["logic1", "enabled", "compiled"],
        should_fail=False
    )
    time.sleep(0.5)

    # ===================================================================
    # TEST 3: Variable Binding (New syntax)
    # ===================================================================
    print(f"\n{BOLD}{YELLOW}TEST 3: Variable Binding with New Syntax{RESET}")
    print("=" * 50)

    bind_tests = [
        ('set logic 1 bind counter reg:100', "Bind counter to holding register", ["counter", "HR#100", "OK"]),
        ('set logic 1 bind led coil:10', "Bind led to coil (output)", ["led", "Coil", "OK"]),
    ]

    for cmd, desc, keywords in bind_tests:
        results[f"bind_{desc.split()[0]}"] = test_command(ser, cmd, desc, keywords)
        time.sleep(0.5)

    # Test 3c: Show bindings in detail
    print(f"\n  {YELLOW}→ Verify bindings in program info{RESET}")
    response = send_command(ser, "show logic 1")
    if "counter" in response.lower() and "HR#100" in response:
        print(f"    {GREEN}[PASS] Bindings displayed in show logic{RESET}")
        results["bindings_display"] = True
    else:
        print(f"    {RED}[FAIL] Bindings not shown{RESET}")
        results["bindings_display"] = False
    time.sleep(0.5)

    # ===================================================================
    # TEST 4: Enable/Disable program
    # ===================================================================
    print(f"\n{BOLD}{YELLOW}TEST 4: Program Control (Enable/Disable){RESET}")
    print("=" * 50)

    control_tests = [
        ('set logic 1 enabled:true', "Enable program", ["enabled", "OK"]),
        ('set logic 1 enabled:false', "Disable program", ["disabled", "OK"]),
        ('set logic 1 enabled:true', "Re-enable program", ["enabled", "OK"]),
    ]

    for cmd, desc, keywords in control_tests:
        results[f"ctrl_{desc.split()[0]}"] = test_command(ser, cmd, desc, keywords)
        time.sleep(0.3)

    # ===================================================================
    # TEST 5: Invalid program (compilation error)
    # ===================================================================
    print(f"\n{BOLD}{YELLOW}TEST 5: Error Handling{RESET}")
    print("=" * 50)

    # Test 5a: Compilation error
    print(f"\n  {YELLOW}5a. Compilation error handling{RESET}")
    invalid_program = 'VAR x: INT END_VAR IF x > 10 THEN x := 1 END_IF'  # Missing semicolons
    cmd = f'set logic 2 upload "{invalid_program}"'
    response = send_command(ser, cmd, timeout=2.0)

    if "error" in response.lower() or "compile" in response.lower():
        print(f"    {YELLOW}→ Invalid program correctly rejected{RESET}")
        print(f"    {GREEN}[PASS]{RESET}")
        results["error_compile"] = True
        # Show error details
        if "parse error" in response.lower() or "error" in response.lower():
            lines = response.split('\n')
            for line in lines[:5]:
                if line.strip() and ("error" in line.lower() or "compile" in line.lower()):
                    print(f"    {BLUE}Error Message:{RESET} {line.strip()[:80]}")
    else:
        print(f"    {RED}[FAIL] Invalid program not rejected{RESET}")
        results["error_compile"] = False
    time.sleep(0.5)

    # Test 5b: Show errors
    results["show_errors"] = test_command(
        ser, "show logic errors",
        "5b. Show programs with errors",
        ["error"],
        should_fail=False
    )
    time.sleep(0.5)

    # ===================================================================
    # TEST 6: Program overview after changes
    # ===================================================================
    print(f"\n{BOLD}{YELLOW}TEST 6: Final Status Overview{RESET}")
    print("=" * 50)

    results["final_program_overview"] = test_command(
        ser, "show logic program",
        "Final program overview (should show Logic1 ACTIVE, Logic2/3/4 EMPTY/FAILED)",
        ["logic", "program"],
        should_fail=False
    )
    time.sleep(0.5)

    results["final_stats"] = test_command(
        ser, "show logic stats",
        "Final statistics",
        ["compiled", "enabled", "execution"],
        should_fail=False
    )
    time.sleep(0.5)

    # ===================================================================
    # SUMMARY
    # ===================================================================
    print(f"\n{BOLD}{YELLOW}╔══════════════════════════════════════════════╗{RESET}")
    print(f"{BOLD}{YELLOW}║              TEST SUMMARY REPORT             ║{RESET}")
    print(f"{BOLD}{YELLOW}╚══════════════════════════════════════════════╝{RESET}")

    passed = sum(1 for v in results.values() if v)
    total = len(results)
    percentage = (passed / total * 100) if total > 0 else 0

    print(f"\n{BOLD}Results: {GREEN}{passed}{RESET}/{total} tests passed ({percentage:.1f}%){RESET}\n")

    # Group results by category
    categories = {
        "Show Commands": ["show logic program", "show logic all", "show logic errors", "show logic stats"],
        "Upload/Compile": ["upload_valid", "show_logic_1"],
        "Binding": ["bind_Bind", "bindings_display"],
        "Control": ["ctrl_Enable", "ctrl_Disable", "ctrl_Re-enable"],
        "Error Handling": ["error_compile", "show_errors"],
        "Final Status": ["final_program_overview", "final_stats"],
    }

    for category, tests in categories.items():
        cat_results = {k: results.get(k, False) for k in tests if k in results}
        if cat_results:
            cat_passed = sum(1 for v in cat_results.values() if v)
            cat_total = len(cat_results)
            status = GREEN if cat_passed == cat_total else YELLOW
            print(f"{status}  {category}: {cat_passed}/{cat_total}{RESET}")
            for test_name, result in cat_results.items():
                symbol = GREEN + "✓" + RESET if result else RED + "✗" + RESET
                print(f"    {symbol} {test_name}")

    # Close connection
    ser.close()

    print(f"\n{BOLD}Connection closed.{RESET}\n")

    if passed == total:
        print(f"{GREEN}✓ All tests passed!{RESET}\n")
        return 0
    else:
        print(f"{RED}✗ {total - passed} test(s) failed{RESET}\n")
        return 1

if __name__ == "__main__":
    sys.exit(main())
