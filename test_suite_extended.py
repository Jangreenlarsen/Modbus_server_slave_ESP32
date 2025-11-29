#!/usr/bin/env python3
"""
ESP32 Modbus Server - Komplet Udvidet Test Suite
Tester ALLE funktioner: Counter modes, Timer modes, Dynamic config, System commands
"""

import serial
import time
import threading
import re
from datetime import datetime

class ESP32ExtendedTestSuite:
    def __init__(self, port='COM11', baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.running = False
        self.read_buffer = []
        self.results = []

    def connect(self):
        """Open serial connection"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
            time.sleep(1)
            self.running = True
            self.reader = threading.Thread(target=self._read_serial, daemon=True)
            self.reader.start()
            print(f"[OK] Tilsluttet til {self.port} @ {self.baudrate}")
            return True
        except Exception as e:
            print(f"[ERROR] Kunne ikke tilslutte: {e}")
            return False

    def disconnect(self):
        self.running = False
        time.sleep(0.5)
        if self.ser:
            self.ser.close()

    def _read_serial(self):
        while self.running:
            if self.ser and self.ser.in_waiting:
                try:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self.read_buffer.append(line)
                except:
                    pass
            time.sleep(0.01)

    def send_command(self, cmd, wait_time=0.5):
        if not self.ser:
            return []
        self.read_buffer = []
        self.ser.write(f"{cmd}\r\n".encode('utf-8'))
        time.sleep(wait_time)
        return self.read_buffer.copy()

    def check_output(self, output, expected_patterns):
        output_text = "\n".join(output)
        for pattern in expected_patterns:
            if isinstance(pattern, str):
                if pattern not in output_text:
                    return False, f"Missing: '{pattern}'"
            elif hasattr(pattern, 'search'):
                if not pattern.search(output_text):
                    return False, f"Pattern not matched: {pattern.pattern}"
        return True, "OK"

    def log_result(self, test_id, description, status, comment=""):
        result = {
            'id': test_id,
            'desc': description,
            'status': status,
            'comment': comment,
            'time': datetime.now().strftime("%H:%M:%S")
        }
        self.results.append(result)
        status_icon = "[PASS]" if status == "PASS" else "[FAIL]" if status == "FAIL" else "[WARN]"
        print(f"{status_icon} [{test_id}] {description}: {comment}")

    def extract_register_value(self, output, reg_addr):
        """Extract register value from output"""
        for line in output:
            match = re.search(rf'Reg\[{reg_addr}\]:\s*(\d+)', line)
            if match:
                return int(match.group(1))
        return None

    def extract_coil_value(self, output, coil_addr):
        """Extract coil value from output"""
        for line in output:
            if f'Coil[{coil_addr}]:' in line or f'{coil_addr}:' in line:
                if '1' in line or 'ON' in line:
                    return 1
                elif '0' in line or 'OFF' in line:
                    return 0
        return None

    # ========================================================================
    # COUNTER TESTS - PRESCALER, SCALE, DIRECTION
    # ========================================================================

    def test_counter_prescaler(self):
        """Test counter prescaler functionality"""
        print("\n" + "="*70)
        print("TESTING: COUNTER PRESCALER")
        print("="*70)

        # Test prescaler 4
        cmd = ('set counter 1 mode 1 hw-mode:hw edge:rising prescaler:4 '
               'hw-gpio:13 index-reg:10 raw-reg:11 freq-reg:12 ctrl-reg:14')
        self.send_command(cmd, wait_time=2.0)
        self.send_command('write reg 14 value 1', wait_time=0.5)  # Reset
        time.sleep(5)

        output = self.send_command('read reg 10 2', wait_time=0.5)
        count_scaled = self.extract_register_value(output, 10)
        count_raw = self.extract_register_value(output, 11)

        if count_raw and count_scaled:
            expected_ratio = count_scaled / count_raw if count_raw > 0 else 0
            # Prescaler 4 means raw = count/4, so scaled should be ~4x raw
            correct = abs(expected_ratio - 4) < 0.5
            self.log_result('CNT-HW-05', 'Prescaler 4 test',
                           "PASS" if correct else "FAIL",
                           f"Scaled={count_scaled}, Raw={count_raw}, Ratio={expected_ratio:.2f}")
        else:
            self.log_result('CNT-HW-05', 'Prescaler 4 test', "FAIL", "No count data")

    def test_counter_scale(self):
        """Test counter scale factor"""
        print("\n" + "="*70)
        print("TESTING: COUNTER SCALE FACTOR")
        print("="*70)

        # Test scale 2.5
        cmd = ('set counter 1 mode 1 hw-mode:hw edge:rising prescaler:1 '
               'scale:2.5 hw-gpio:13 index-reg:10 raw-reg:11 ctrl-reg:14')
        self.send_command(cmd, wait_time=2.0)
        self.send_command('write reg 14 value 1', wait_time=0.5)
        time.sleep(5)

        output = self.send_command('read reg 10 2', wait_time=0.5)
        count_scaled = self.extract_register_value(output, 10)
        count_raw = self.extract_register_value(output, 11)

        if count_raw and count_scaled:
            actual_scale = count_scaled / count_raw if count_raw > 0 else 0
            correct = abs(actual_scale - 2.5) < 0.2
            self.log_result('CNT-HW-06', 'Scale 2.5 test',
                           "PASS" if correct else "FAIL",
                           f"Scaled={count_scaled}, Raw={count_raw}, Scale={actual_scale:.2f}")
        else:
            self.log_result('CNT-HW-06', 'Scale 2.5 test', "FAIL", "No count data")

    def test_counter_direction(self):
        """Test counter direction (DOWN)"""
        print("\n" + "="*70)
        print("TESTING: COUNTER DIRECTION")
        print("="*70)

        # Set high start value and count down
        cmd = ('set counter 1 mode 1 hw-mode:hw edge:rising prescaler:1 '
               'direction:down start-value:10000 hw-gpio:13 index-reg:10 ctrl-reg:14')
        self.send_command(cmd, wait_time=2.0)

        # Read initial value
        output1 = self.send_command('read reg 10 1', wait_time=0.5)
        initial = self.extract_register_value(output1, 10)

        time.sleep(3)

        # Read after 3 seconds
        output2 = self.send_command('read reg 10 1', wait_time=0.5)
        final = self.extract_register_value(output2, 10)

        if initial and final:
            decreased = final < initial
            self.log_result('CNT-HW-07', 'Direction DOWN test',
                           "PASS" if decreased else "FAIL",
                           f"Initial={initial}, Final={final}, Decreased={decreased}")
        else:
            self.log_result('CNT-HW-07', 'Direction DOWN test', "FAIL", "No count data")

    def test_counter_start_value(self):
        """Test counter start value"""
        print("\n" + "="*70)
        print("TESTING: COUNTER START VALUE")
        print("="*70)

        cmd = ('set counter 1 mode 1 hw-mode:hw edge:rising prescaler:1 '
               'start-value:5000 hw-gpio:13 index-reg:10 ctrl-reg:14')
        self.send_command(cmd, wait_time=2.0)
        self.send_command('write reg 14 value 1', wait_time=0.5)  # Reset to start value
        time.sleep(0.5)

        output = self.send_command('read reg 10 1', wait_time=0.5)
        value = self.extract_register_value(output, 10)

        if value:
            correct = abs(value - 5000) < 100  # Allow small count during reset
            self.log_result('CNT-HW-08', 'Start value 5000',
                           "PASS" if correct else "FAIL",
                           f"Value={value} (expected ~5000)")
        else:
            self.log_result('CNT-HW-08', 'Start value 5000', "FAIL", "No count data")

    # ========================================================================
    # COUNTER CONTROL TESTS
    # ========================================================================

    def test_counter_control(self):
        """Test counter control register (reset, start, stop)"""
        print("\n" + "="*70)
        print("TESTING: COUNTER CONTROL REGISTER")
        print("="*70)

        # Configure counter
        cmd = ('set counter 1 mode 1 hw-mode:hw edge:rising prescaler:1 '
               'hw-gpio:13 index-reg:10 ctrl-reg:14')
        self.send_command(cmd, wait_time=2.0)

        # Test RESET (bit 0)
        time.sleep(2)
        output1 = self.send_command('read reg 10 1', wait_time=0.5)
        before_reset = self.extract_register_value(output1, 10)

        self.send_command('write reg 14 value 1', wait_time=0.5)  # Reset
        time.sleep(0.5)

        output2 = self.send_command('read reg 10 1', wait_time=0.5)
        after_reset = self.extract_register_value(output2, 10)

        if before_reset and after_reset is not None:
            reset_worked = after_reset < before_reset
            self.log_result('CTL-01', 'Counter reset via ctrl-reg',
                           "PASS" if reset_worked else "FAIL",
                           f"Before={before_reset}, After={after_reset}")
        else:
            self.log_result('CTL-01', 'Counter reset', "FAIL", "No data")

        # Test START (bit 1) - counter should already be running
        # Test STOP (bit 2)
        self.send_command('write reg 14 value 4', wait_time=0.5)  # Stop (bit 2)
        time.sleep(0.5)

        output3 = self.send_command('read reg 10 1', wait_time=0.5)
        count_at_stop = self.extract_register_value(output3, 10)

        time.sleep(2)

        output4 = self.send_command('read reg 10 1', wait_time=0.5)
        count_after_stop = self.extract_register_value(output4, 10)

        if count_at_stop is not None and count_after_stop is not None:
            stopped = abs(count_after_stop - count_at_stop) < 100
            self.log_result('CTL-06', 'Counter stop via ctrl-reg',
                           "PASS" if stopped else "FAIL",
                           f"At stop={count_at_stop}, After 2s={count_after_stop}")
        else:
            self.log_result('CTL-06', 'Counter stop', "FAIL", "No data")

    # ========================================================================
    # TIMER MODE TESTS
    # ========================================================================

    def test_timer_mode_3_astable(self):
        """Test Timer Mode 3: Astable (blink/toggle)"""
        print("\n" + "="*70)
        print("TESTING: TIMER MODE 3 (ASTABLE/BLINK)")
        print("="*70)

        # Configure timer 1: 1s ON, 1s OFF, output to coil 200
        cmd = ('set timer 1 mode 3 on:1000 off:1000 output-coil:200')
        self.send_command(cmd, wait_time=2.0)

        # Monitor coil 200 for toggling
        states = []
        for i in range(5):
            output = self.send_command('read coil 200 1', wait_time=0.5)
            state = self.extract_coil_value(output, 200)
            if state is not None:
                states.append(state)
            time.sleep(1)

        # Check if state toggles
        if len(states) >= 3:
            toggled = states[0] != states[2]  # Should toggle at least once
            self.log_result('TIM-03-01', 'Timer Mode 3 astable toggle',
                           "PASS" if toggled else "FAIL",
                           f"States={states}")
        else:
            self.log_result('TIM-03-01', 'Timer Mode 3', "FAIL", "Insufficient data")

    # ========================================================================
    # DYNAMIC CONFIGURATION TESTS
    # ========================================================================

    def test_static_register_value(self):
        """Test STATIC register value persistence"""
        print("\n" + "="*70)
        print("TESTING: STATIC REGISTER VALUE")
        print("="*70)

        # Set a STATIC register value (should persist)
        self.send_command('set reg STATIC 100 Value 12345', wait_time=0.5)
        time.sleep(0.5)

        # Read it back
        output = self.send_command('read reg 100 1', wait_time=0.5)
        value = self.extract_register_value(output, 100)

        if value:
            correct = value == 12345
            self.log_result('REG-ST-01', 'STATIC register set value',
                           "PASS" if correct else "FAIL",
                           f"Value={value} (expected 12345)")
        else:
            self.log_result('REG-ST-01', 'STATIC register', "FAIL", "No data")


    # ========================================================================
    # SYSTEM COMMANDS TESTS
    # ========================================================================

    def test_system_save_load(self):
        """Test save and load configuration"""
        print("\n" + "="*70)
        print("TESTING: SYSTEM SAVE/LOAD")
        print("="*70)

        # Set a STATIC register value (this will be persisted in NVS)
        self.send_command('set reg STATIC 100 Value 54321', wait_time=0.5)

        # Save config
        output1 = self.send_command('save', wait_time=2.0)
        saved = any('SAVED' in line or 'saved' in line for line in output1)
        self.log_result('SYS-06', 'Save config to NVS',
                       "PASS" if saved else "FAIL",
                       "Config saved" if saved else "No confirmation")

        # Change the value directly (without updating STATIC config)
        self.send_command('write reg 100 value 99999', wait_time=0.5)

        # Load config (should restore STATIC value from NVS)
        output2 = self.send_command('load', wait_time=2.0)
        loaded = any('LOADED' in line or 'loaded' in line for line in output2)

        # Check if original STATIC value restored
        output3 = self.send_command('read reg 100 1', wait_time=0.5)
        value = self.extract_register_value(output3, 100)

        if loaded and value:
            restored = value == 54321
            self.log_result('SYS-07', 'Load config from NVS',
                           "PASS" if restored else "FAIL",
                           f"Value after load={value} (expected 54321)")
        else:
            self.log_result('SYS-07', 'Load config', "FAIL", "Load failed")

    def test_system_set_id(self):
        """Test set slave ID"""
        print("\n" + "="*70)
        print("TESTING: SYSTEM SET ID")
        print("="*70)

        # Get current ID
        output1 = self.send_command('show config', wait_time=1.0)

        # Set new ID
        self.send_command('set id 42', wait_time=0.5)

        # Verify
        output2 = self.send_command('show config', wait_time=1.0)
        id_set = any('42' in line and ('Unit-ID' in line or 'Slave' in line)
                     for line in output2)

        self.log_result('SYS-03', 'Set slave ID to 42',
                       "PASS" if id_set else "FAIL",
                       "ID changed" if id_set else "ID not changed")

        # Restore original ID (20)
        self.send_command('set id 20', wait_time=0.5)

    # ========================================================================
    # MAIN TEST RUNNER
    # ========================================================================

    def run_all_tests(self):
        """Run all extended tests"""
        print("\n" + "="*70)
        print("ESP32 MODBUS SERVER - UDVIDET TEST SUITE")
        print("Kører ALLE tests inkl. Counter variants, Timers, Dynamic config")
        print("="*70)

        print("\n[INFO] Antager 1 kHz signal er tilsluttet GPIO 13")

        # Counter advanced tests
        self.test_counter_prescaler()
        self.test_counter_scale()
        self.test_counter_direction()
        self.test_counter_start_value()
        self.test_counter_control()

        # Timer tests
        self.test_timer_mode_3_astable()

        # STATIC register test
        self.test_static_register_value()

        # System commands
        self.test_system_save_load()
        self.test_system_set_id()

    def generate_report(self):
        """Generate comprehensive report"""
        print("\n" + "="*70)
        print("UDVIDET TEST RAPPORT")
        print("="*70)

        total = len(self.results)
        passed = sum(1 for r in self.results if r['status'] == 'PASS')
        failed = sum(1 for r in self.results if r['status'] == 'FAIL')
        warned = sum(1 for r in self.results if r['status'] == 'WARN')

        print(f"\nTotal tests: {total}")
        print(f"  Passed: {passed} ({100*passed/total:.1f}%)")
        print(f"  Failed: {failed} ({100*failed/total:.1f}%)")
        print(f"  Warned: {warned}")

        if failed > 0:
            print("\nFailed tests:")
            for r in self.results:
                if r['status'] == 'FAIL':
                    print(f"  - [{r['id']}] {r['desc']}: {r['comment']}")

        # Save detailed report
        with open('TEST_RAPPORT_EXTENDED.txt', 'w', encoding='utf-8') as f:
            f.write("="*70 + "\n")
            f.write("ESP32 MODBUS SERVER - UDVIDET TEST RAPPORT\n")
            f.write("="*70 + "\n\n")
            f.write(f"Dato: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            # Read build number from file
            try:
                with open('build_number.txt', 'r') as bf:
                    build_num = bf.read().strip()
            except:
                build_num = "unknown"
            f.write(f"Firmware: v1.0.0 Build #{build_num}\n\n")
            f.write(f"STATISTIK:\n")
            f.write(f"  Total: {total}, Passed: {passed}, Failed: {failed}, Warned: {warned}\n\n")
            f.write("DETALJEREDE RESULTATER:\n")
            f.write("-"*70 + "\n")
            for r in self.results:
                f.write(f"[{r['id']}] {r['desc']}\n")
                f.write(f"  Status: {r['status']}\n")
                if r['comment']:
                    f.write(f"  Note: {r['comment']}\n")
                f.write(f"  Time: {r['time']}\n\n")

        print(f"\n[OK] Rapport gemt til: TEST_RAPPORT_EXTENDED.txt")


def main():
    suite = ESP32ExtendedTestSuite(port='COM11', baudrate=115200)

    if not suite.connect():
        return

    try:
        suite.run_all_tests()
        suite.generate_report()
    except KeyboardInterrupt:
        print("\n[!] Test afbrudt")
    except Exception as e:
        print(f"\n[ERROR] {e}")
        import traceback
        traceback.print_exc()
    finally:
        suite.disconnect()

if __name__ == '__main__':
    main()
