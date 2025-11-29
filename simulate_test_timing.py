#!/usr/bin/env python3
"""
Simulate EXACT test timing to reproduce [0,0,0,0,0]
"""
import serial
import time
import threading
import re

class ExactTestSimulation:
    def __init__(self, port='COM11', baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.running = False
        self.read_buffer = []

    def connect(self):
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
            time.sleep(1)
            self.running = True
            self.reader = threading.Thread(target=self._read_serial, daemon=True)
            self.reader.start()
            print(f"[OK] Connected to {self.port}")
            return True
        except Exception as e:
            print(f"[ERROR] {e}")
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

    def extract_coil_value(self, output, coil_addr):
        """EXACT extract from test"""
        for line in output:
            if f'Coil[{coil_addr}]:' in line or f'{coil_addr}:' in line:
                if '1' in line or 'ON' in line:
                    return 1
                elif '0' in line or 'OFF' in line:
                    return 0
        return None

    def test_timer_mode_3(self):
        """EXACT test from test_suite"""
        print("\n" + "=" * 70)
        print("EXACT TEST SIMULATION: TIMER MODE 3")
        print("=" * 70)

        # Configure timer 1: 1s ON, 1s OFF, output to coil 200
        cmd = ('set timer 1 mode 3 on:1000 off:1000 output-coil:200')
        print(f"\n[CONFIG] {cmd}")
        output = self.send_command(cmd, wait_time=2.0)
        print(f"  Output lines: {len(output)}")
        for line in output[:3]:
            print(f"    {line}")

        # Monitor coil 200 for toggling (EXACT timing from test)
        print("\n[MONITOR] Reading coil 200 five times with 1s intervals")
        states = []
        for i in range(5):
            # EXACT: send command with 0.5s wait
            output = self.send_command('read coil 200 1', wait_time=0.5)
            state = self.extract_coil_value(output, 200)
            if state is not None:
                states.append(state)
                print(f"  [{i}] State={state}")
            else:
                print(f"  [{i}] State=None (extract failed!)")
                # Debug: print raw output
                print(f"      Raw output: {output[:5]}")
            # EXACT: sleep 1 second
            time.sleep(1)

        print(f"\nResult: states = {states}")
        print(f"Expected (approx): [1, 0, 1, 0, 1] or similar toggle")
        print(f"Test failure gets: [0, 0, 0, 0, 0]")

def main():
    test = ExactTestSimulation()
    if not test.connect():
        return

    try:
        test.test_timer_mode_3()
    except KeyboardInterrupt:
        print("\n[!] Interrupted")
    finally:
        test.disconnect()

if __name__ == '__main__':
    main()
