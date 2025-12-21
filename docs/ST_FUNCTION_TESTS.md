# ST Logic - Test Eksempler for Nye Funktioner

**Version:** v4.4
**Build:** #672+
**Nye Funktioner:** LIMIT, SEL, SIN, COS, TAN

---

## 🧪 Test 1: LIMIT - Temperature Clamping

**Use Case:** Begræns temperatur setpoint til sikker range (0-100°C)

### ST Program:
```structured-text
PROGRAM
VAR
  raw_setpoint: INT;
  safe_setpoint: INT;
END_VAR
BEGIN
  (* LIMIT clamps value to 0-100 range *)
  safe_setpoint := LIMIT(0, raw_setpoint, 100);
END_PROGRAM
```

### CLI Upload:
```bash
set logic 1 upload "PROGRAM VAR raw_setpoint: INT; safe_setpoint: INT; END_VAR BEGIN safe_setpoint := LIMIT(0, raw_setpoint, 100); END_PROGRAM"
```

### Binding Setup:
```bash
# Bind raw_setpoint til HR100 (input/output)
set logic 1 bind raw_setpoint reg:100

# Bind safe_setpoint til HR101 (output)
set logic 1 bind safe_setpoint reg:101

# Enable program
set logic 1 enabled:true
```

### Test via Modbus:
```python
# Test 1: Værdi under minimum
client.write_register(100, -10)    # raw_setpoint = -10
time.sleep(0.1)
result = client.read_holding_registers(101, 1)
print(f"LIMIT(0, -10, 100) = {result.registers[0]}")  # Expect: 0

# Test 2: Værdi over maximum
client.write_register(100, 150)    # raw_setpoint = 150
time.sleep(0.1)
result = client.read_holding_registers(101, 1)
print(f"LIMIT(0, 150, 100) = {result.registers[0]}")  # Expect: 100

# Test 3: Værdi i range
client.write_register(100, 50)     # raw_setpoint = 50
time.sleep(0.1)
result = client.read_holding_registers(101, 1)
print(f"LIMIT(0, 50, 100) = {result.registers[0]}")   # Expect: 50
```

### Forventet Resultat:
```
✓ LIMIT(0, -10, 100) = 0    (clamped til minimum)
✓ LIMIT(0, 150, 100) = 100  (clamped til maximum)
✓ LIMIT(0, 50, 100) = 50    (ingen clamping)
```

---

## 🧪 Test 2: SEL - Mode Selection

**Use Case:** Vælg mellem manuel og automatisk mode

### ST Program:
```structured-text
PROGRAM
VAR
  manual_mode: BOOL;
  manual_value: INT;
  auto_value: INT;
  output: INT;
END_VAR
BEGIN
  (* SEL selects based on boolean *)
  (* manual_mode=false -> auto_value *)
  (* manual_mode=true  -> manual_value *)
  output := SEL(manual_mode, auto_value, manual_value);
END_PROGRAM
```

### CLI Upload:
```bash
set logic 1 upload "PROGRAM VAR manual_mode: BOOL; manual_value: INT; auto_value: INT; output: INT; END_VAR BEGIN output := SEL(manual_mode, auto_value, manual_value); END_PROGRAM"
```

### Binding Setup:
```bash
# Bind inputs
set logic 1 bind manual_mode coil:0        # Coil 0 = mode selector
set logic 1 bind manual_value reg:100      # HR100 = manual setpoint
set logic 1 bind auto_value reg:101        # HR101 = auto setpoint

# Bind output
set logic 1 bind output reg:102            # HR102 = active setpoint

# Enable
set logic 1 enabled:true
```

### Test via Modbus:
```python
# Setup values
client.write_register(100, 50)   # manual_value = 50
client.write_register(101, 75)   # auto_value = 75

# Test 1: Auto mode (manual_mode=false)
client.write_coil(0, False)
time.sleep(0.1)
result = client.read_holding_registers(102, 1)
print(f"SEL(false, 75, 50) = {result.registers[0]}")  # Expect: 75

# Test 2: Manual mode (manual_mode=true)
client.write_coil(0, True)
time.sleep(0.1)
result = client.read_holding_registers(102, 1)
print(f"SEL(true, 75, 50) = {result.registers[0]}")   # Expect: 50
```

### Forventet Resultat:
```
✓ SEL(false, 75, 50) = 75  (auto mode valgt)
✓ SEL(true, 75, 50) = 50   (manual mode valgt)
```

---

## 🧪 Test 3: SIN/COS - Cirkulær Bevægelse

**Use Case:** Generer cirkulær bevægelse (X/Y positioner)

### ST Program:
```structured-text
PROGRAM
VAR
  angle: REAL;
  x_pos: REAL;
  y_pos: REAL;
  radius: REAL;
END_VAR
BEGIN
  (* Circular motion *)
  radius := 100.0;
  x_pos := radius * COS(angle);
  y_pos := radius * SIN(angle);

  (* Increment angle (wrap at 2*PI ≈ 6.28) *)
  angle := angle + 0.1;
  IF angle > 6.28 THEN
    angle := 0.0;
  END_IF;
END_PROGRAM
```

### CLI Upload:
```bash
set logic 1 upload "PROGRAM VAR angle: REAL; x_pos: REAL; y_pos: REAL; radius: REAL; END_VAR BEGIN radius := 100.0; x_pos := radius * COS(angle); y_pos := radius * SIN(angle); angle := angle + 0.1; IF angle > 6.28 THEN angle := 0.0; END_IF; END_PROGRAM"
```

### Binding Setup:
```bash
# Bind variables to registers (REAL = 2 words hver)
set logic 1 bind angle reg:100      # HR100-101 (REAL)
set logic 1 bind x_pos reg:102      # HR102-103 (REAL)
set logic 1 bind y_pos reg:104      # HR104-105 (REAL)
set logic 1 bind radius reg:106     # HR106-107 (REAL)

# Enable
set logic 1 enabled:true
```

### Test via CLI:
```bash
# Observe angle, x_pos, y_pos increment hver 10ms cycle
show logic 1

# Du burde se:
# angle: 0.0 -> 0.1 -> 0.2 -> ... -> 6.28 -> 0.0 (wrap)
# x_pos: 100.0 -> 99.5 -> 98.0 -> ... (cosine curve)
# y_pos: 0.0 -> 10.0 -> 19.9 -> ... (sine curve)
```

### Forventet Resultat:
```
✓ angle increments by 0.1 hver cycle
✓ x_pos = 100 * cos(angle)  (peaks at ±100)
✓ y_pos = 100 * sin(angle)  (peaks at ±100)
✓ Circular motion verified: x² + y² ≈ 10000
```

---

## 🧪 Test 4: TAN - Vinkel Beregning

**Use Case:** Beregn vinkel fra X/Y position

### ST Program:
```structured-text
PROGRAM
VAR
  x_pos: REAL;
  y_pos: REAL;
  angle_rad: REAL;
  slope: REAL;
END_VAR
BEGIN
  (* Calculate slope (tan = y/x) *)
  IF x_pos <> 0.0 THEN
    slope := y_pos / x_pos;
    angle_rad := TAN(slope);  (* Note: Should use ATAN, but TAN for demo *)
  ELSE
    angle_rad := 0.0;
  END_IF;
END_PROGRAM
```

### CLI Upload:
```bash
set logic 1 upload "PROGRAM VAR x_pos: REAL; y_pos: REAL; angle_rad: REAL; slope: REAL; END_VAR BEGIN IF x_pos <> 0.0 THEN slope := y_pos / x_pos; angle_rad := TAN(slope); ELSE angle_rad := 0.0; END_IF; END_PROGRAM"
```

### Binding Setup:
```bash
set logic 1 bind x_pos reg:100       # HR100-101 (REAL)
set logic 1 bind y_pos reg:102       # HR102-103 (REAL)
set logic 1 bind slope reg:104       # HR104-105 (REAL)
set logic 1 bind angle_rad reg:106   # HR106-107 (REAL)

set logic 1 enabled:true
```

### Test via Modbus:
```python
import struct

# Helper: Write REAL value (IEEE 754 float)
def write_real(address, value):
    bytes_val = struct.pack('!f', value)
    words = struct.unpack('!HH', bytes_val)
    client.write_registers(address, list(words))

# Test TAN at different angles
test_values = [
    (1.0, 0.0),      # tan(0) ≈ 0
    (1.0, 1.0),      # tan(π/4) ≈ 1
    (1.0, 1.732),    # tan(π/3) ≈ √3
]

for x, y in test_values:
    write_real(100, x)  # x_pos
    write_real(102, y)  # y_pos
    time.sleep(0.1)

    # Read slope and angle
    slope_regs = client.read_holding_registers(104, 2)
    slope_bytes = struct.pack('!HH', *slope_regs.registers)
    slope = struct.unpack('!f', slope_bytes)[0]

    print(f"TAN({slope:.3f}) calculated from x={x}, y={y}")
```

---

## 🧪 Test 5: Kombineret Test - Alt-i-En

**Use Case:** Test alle funktioner sammen

### ST Program:
```structured-text
PROGRAM
VAR
  (* Inputs *)
  raw_temp: INT;
  mode_auto: BOOL;
  angle: REAL;

  (* Outputs *)
  safe_temp: INT;
  x_motion: REAL;
  y_motion: REAL;
  selected_output: INT;

  (* Internal *)
  auto_value: INT;
  manual_value: INT;
END_VAR
BEGIN
  (* 1. LIMIT: Clamp temperature *)
  safe_temp := LIMIT(0, raw_temp, 100);

  (* 2. SIN/COS: Circular motion *)
  x_motion := 50.0 * COS(angle);
  y_motion := 50.0 * SIN(angle);

  (* 3. SEL: Mode selection *)
  auto_value := 75;
  manual_value := 50;
  selected_output := SEL(mode_auto, manual_value, auto_value);

  (* Increment angle *)
  angle := angle + 0.1;
  IF angle > 6.28 THEN
    angle := 0.0;
  END_IF;
END_PROGRAM
```

### CLI Upload (one-liner):
```bash
set logic 1 upload "PROGRAM VAR raw_temp: INT; mode_auto: BOOL; angle: REAL; safe_temp: INT; x_motion: REAL; y_motion: REAL; selected_output: INT; auto_value: INT; manual_value: INT; END_VAR BEGIN safe_temp := LIMIT(0, raw_temp, 100); x_motion := 50.0 * COS(angle); y_motion := 50.0 * SIN(angle); auto_value := 75; manual_value := 50; selected_output := SEL(mode_auto, manual_value, auto_value); angle := angle + 0.1; IF angle > 6.28 THEN angle := 0.0; END_IF; END_PROGRAM"
```

### Binding Setup:
```bash
# Inputs
set logic 1 bind raw_temp reg:100       # HR100
set logic 1 bind mode_auto coil:0       # Coil 0
set logic 1 bind angle reg:101          # HR101-102 (REAL)

# Outputs
set logic 1 bind safe_temp reg:110      # HR110
set logic 1 bind x_motion reg:111       # HR111-112 (REAL)
set logic 1 bind y_motion reg:113      # HR113-114 (REAL)
set logic 1 bind selected_output reg:115  # HR115

# Enable
set logic 1 enabled:true
```

### Quick Verification:
```bash
# View program status
show logic 1

# Expected output:
# Variables:
#   [0] raw_temp (INT)
#   [1] mode_auto (BOOL)
#   [2] angle (REAL)
#   [3] safe_temp (INT)
#   [4] x_motion (REAL)
#   [5] y_motion (REAL)
#   [6] selected_output (INT)
# Status: RUNNING
# Cycles: increasing
```

---

## 📊 Python Test Script (Complete)

Gem som `test_st_functions.py`:

```python
#!/usr/bin/env python3
"""Test script for ST Logic new functions (v4.4+)"""

from pymodbus.client import ModbusSerialClient
import struct
import time

def write_real(client, address, value):
    """Write IEEE 754 float to 2 holding registers"""
    bytes_val = struct.pack('!f', value)
    words = struct.unpack('!HH', bytes_val)
    client.write_registers(address, list(words))

def read_real(client, address):
    """Read IEEE 754 float from 2 holding registers"""
    result = client.read_holding_registers(address, 2)
    bytes_val = struct.pack('!HH', *result.registers)
    return struct.unpack('!f', bytes_val)[0]

def main():
    # Connect to ESP32
    client = ModbusSerialClient(
        port='COM3',  # Adjust to your port
        baudrate=115200,
        timeout=1
    )

    if not client.connect():
        print("❌ Connection failed!")
        return

    print("✓ Connected to ESP32")
    print("\n" + "="*60)

    # Test 1: LIMIT
    print("TEST 1: LIMIT(0, value, 100)")
    print("-"*60)

    test_values = [-10, 0, 50, 100, 150]
    for val in test_values:
        client.write_register(100, val)  # raw_setpoint
        time.sleep(0.05)
        result = client.read_holding_registers(101, 1).registers[0]
        status = "✓" if (val < 0 and result == 0) or \
                       (val > 100 and result == 100) or \
                       (0 <= val <= 100 and result == val) else "❌"
        print(f"{status} LIMIT(0, {val:4d}, 100) = {result:3d}")

    print("\n" + "="*60)

    # Test 2: SEL
    print("TEST 2: SEL(mode, auto=75, manual=50)")
    print("-"*60)

    client.write_register(100, 50)   # manual_value
    client.write_register(101, 75)   # auto_value

    for mode in [False, True]:
        client.write_coil(0, mode)
        time.sleep(0.05)
        result = client.read_holding_registers(102, 1).registers[0]
        expected = 50 if mode else 75
        status = "✓" if result == expected else "❌"
        mode_str = "manual" if mode else "auto"
        print(f"{status} SEL({mode_str}) = {result} (expected {expected})")

    print("\n" + "="*60)

    # Test 3: SIN/COS (read values from running program)
    print("TEST 3: SIN/COS Circular Motion")
    print("-"*60)
    print("Reading 5 samples of angle, x_pos, y_pos...")

    for i in range(5):
        angle = read_real(client, 100)
        x_pos = read_real(client, 102)
        y_pos = read_real(client, 104)
        radius_calc = (x_pos**2 + y_pos**2)**0.5

        print(f"Sample {i+1}: angle={angle:.2f}, x={x_pos:6.1f}, y={y_pos:6.1f}, r={radius_calc:5.1f}")
        time.sleep(0.2)

    print("\n" + "="*60)
    print("✓ All tests completed!")

    client.close()

if __name__ == "__main__":
    main()
```

### Kør Test:
```bash
python test_st_functions.py
```

### Forventet Output:
```
✓ Connected to ESP32
============================================================
TEST 1: LIMIT(0, value, 100)
------------------------------------------------------------
✓ LIMIT(0,  -10, 100) =   0
✓ LIMIT(0,    0, 100) =   0
✓ LIMIT(0,   50, 100) =  50
✓ LIMIT(0,  100, 100) = 100
✓ LIMIT(0,  150, 100) = 100

============================================================
TEST 2: SEL(mode, auto=75, manual=50)
------------------------------------------------------------
✓ SEL(auto) = 75 (expected 75)
✓ SEL(manual) = 50 (expected 50)

============================================================
TEST 3: SIN/COS Circular Motion
------------------------------------------------------------
Sample 1: angle=0.10, x= 99.5, y= 10.0, r=100.0
Sample 2: angle=0.30, x= 95.5, y= 29.6, r=100.0
Sample 3: angle=0.50, x= 87.8, y= 47.9, r=100.0
Sample 4: angle=0.70, x= 76.5, y= 64.4, r=100.0
Sample 5: angle=0.90, x= 62.2, y= 78.3, r=100.0

============================================================
✓ All tests completed!
```

---

## ⚠️ Troubleshooting

### Problem: "Unknown function: LIMIT"
**Løsning:**
- Tjek at du kører Build #672 eller nyere
- Kør `show logic 1` for at se compilation errors

### Problem: LIMIT returnerer forkert værdi
**Løsning:**
- Tjek at argumenterne er i rigtig rækkefølge: `LIMIT(min, value, max)`
- Ikke: `LIMIT(value, min, max)` ❌

### Problem: SIN/COS returnerer 0
**Løsning:**
- Tjek at angle er REAL type (ikke INT)
- Husk: Input er i radianer, ikke grader!
- 90° = π/2 ≈ 1.57 radianer

### Problem: SEL vælger forkert værdi
**Løsning:**
- Husk argument rækkefølge: `SEL(selector, when_false, when_true)`
- selector=false → returnerer 2. argument
- selector=true → returnerer 3. argument

---

## 📝 Næste Tests

Når R_TRIG/F_TRIG er implementeret:
```structured-text
VAR
  button_count: INT;
END_VAR

IF R_TRIG(button_input) THEN
  button_count := button_count + 1;
END_IF;
```

Når TON timer er implementeret:
```structured-text
VAR
  motor_running: BOOL;
END_VAR

motor_running := TON(start_button, 5000);  (* 5 sec delay *)
```

---

**Succes med testene!** 🚀
