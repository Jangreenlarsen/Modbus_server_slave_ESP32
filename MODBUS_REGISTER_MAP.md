# Modbus Register Map - ESP32 Modbus RTU Server

**Version:** v4.1.0
**Dato:** 2025-12-12
**Hardware:** ESP32-WROOM-32

---

## 📋 Overview

Dette dokument beskriver **ALLE** Modbus registre, coils og discrete inputs som ESP32 Modbus RTU Serveren bruger.

**Register Typer:**
- **Holding Registers (HR):** Read-Write, 16-bit, FC03/FC06/FC10
- **Input Registers (IR):** Read-Only, 16-bit, FC04
- **Coils (C):** Read-Write, 1-bit, FC01/FC05/FC0F
- **Discrete Inputs (DI):** Read-Only, 1-bit, FC02

**Addressing:**
- Alle adresser er 0-baserede (Modbus standard)
- 32-bit værdier bruger 2 på hinanden følgende registre (HIGH word, LOW word)

---

## 🔧 Register Kategorier

### 1. Fixed Register Mappings
Disse registre har **faste adresser** og håndteres automatisk af systemet:
- **ST Logic Status & Control** (IR/HR 200-293)

### 2. Dynamic Register Mappings
Disse registre har **bruger-definerede adresser** konfigureret via CLI:
- **Counter Registers** (index, raw, freq, overflow, ctrl)
- **Timer Registers** (ctrl/output)
- **Persistent Registers** (bruger-defineret data storage)

### 3. Dynamic Coil/Discrete Input Mappings
Disse bits har **bruger-definerede adresser** konfigureret via CLI:
- **GPIO Mappings** (pins → coils/discrete inputs)
- **Timer Outputs** (timer state → coils)

---

## 📊 Fixed Register Map: ST Logic (IR/HR 200-293)

### Input Registers (Read-Only)

#### **IR 200-203: Program Status**
| Register | Program | Bits | Beskrivelse |
|----------|---------|------|-------------|
| **200** | Logic1 | [15:0] | Status flags |
| **201** | Logic2 | [15:0] | Status flags |
| **202** | Logic3 | [15:0] | Status flags |
| **203** | Logic4 | [15:0] | Status flags |

**Status Bits:**
- Bit 0: `ST_LOGIC_STATUS_ENABLED` (0x0001) - Program enabled
- Bit 1: `ST_LOGIC_STATUS_COMPILED` (0x0002) - Program compiled successfully
- Bit 2: `ST_LOGIC_STATUS_RUNNING` (0x0004) - Currently executing
- Bit 3: `ST_LOGIC_STATUS_ERROR` (0x0008) - Has error

**Eksempel:**
```
IR 200 = 0x0003  →  Logic1 is enabled + compiled (bits 0+1 set)
IR 201 = 0x0009  →  Logic2 has error + is enabled (bits 0+3 set)
```

---

#### **IR 204-207: Execution Count**
| Register | Program | Type | Beskrivelse |
|----------|---------|------|-------------|
| **204** | Logic1 | 16-bit | Number of times executed (wraps at 65535) |
| **205** | Logic2 | 16-bit | Number of times executed |
| **206** | Logic3 | 16-bit | Number of times executed |
| **207** | Logic4 | 16-bit | Number of times executed |

**Note:** Wraps at 65535 → 0. Se også IR 290-291 for total cycles.

---

#### **IR 208-211: Error Count**
| Register | Program | Type | Beskrivelse |
|----------|---------|------|-------------|
| **208** | Logic1 | 16-bit | Number of execution errors (wraps at 65535) |
| **209** | Logic2 | 16-bit | Number of execution errors |
| **210** | Logic3 | 16-bit | Number of execution errors |
| **211** | Logic4 | 16-bit | Number of execution errors |

**Note:** Nulstilles ved `set logic stats reset X` eller Modbus control bit 2.

---

#### **IR 212-215: Last Error Code**
| Register | Program | Type | Beskrivelse |
|----------|---------|------|-------------|
| **212** | Logic1 | 16-bit | 0 = no error, 1+ = error present |
| **213** | Logic2 | 16-bit | Error code |
| **214** | Logic3 | 16-bit | Error code |
| **215** | Logic4 | 16-bit | Error code |

**Error Codes:**
- `0` = No error
- `1` = Error present (se error string via CLI `show logic X`)

---

#### **IR 216-219: Variable Binding Count**
| Register | Program | Type | Beskrivelse |
|----------|---------|------|-------------|
| **216** | Logic1 | 16-bit | Number of variable bindings for this program |
| **217** | Logic2 | 16-bit | Variable binding count |
| **218** | Logic3 | 16-bit | Variable binding count |
| **219** | Logic4 | 16-bit | Variable binding count |

**Note:** Cached værdi, opdateres automatisk ved bind/unbind operationer.

---

#### **IR 220-251: Variable Values (8 variables × 4 programs)**
| Register Range | Program | Variables | Beskrivelse |
|----------------|---------|-----------|-------------|
| **220-227** | Logic1 | Var[0]-Var[7] | Variable values for Logic1 |
| **228-235** | Logic2 | Var[0]-Var[7] | Variable values for Logic2 |
| **236-243** | Logic3 | Var[0]-Var[7] | Variable values for Logic3 |
| **244-251** | Logic4 | Var[0]-Var[7] | Variable values for Logic4 |

**Format:**
```
IR 220 = Logic1.Var[0]  (første variable i Logic1)
IR 221 = Logic1.Var[1]  (anden variable i Logic1)
...
IR 227 = Logic1.Var[7]  (sidste variable i Logic1)
IR 228 = Logic2.Var[0]  (første variable i Logic2)
...
```

**Variable Mapping:**
- Kun variabler med bindings vises her
- Variable index matcher binding configuration
- Type conversion: BOOL → 0/1, INT → int16_t, REAL → (int16_t)float

---

### Performance Statistics (v4.1.0)

#### **IR 252-259: Min Execution Time (µs)**
| Register | Program | Type | Beskrivelse |
|----------|---------|------|-------------|
| **252-253** | Logic1 | 32-bit | Minimum execution time (microseconds) |
| **254-255** | Logic2 | 32-bit | Minimum execution time |
| **256-257** | Logic3 | 32-bit | Minimum execution time |
| **258-259** | Logic4 | 32-bit | Minimum execution time |

**32-bit Format:** `VALUE = (IR[N] << 16) | IR[N+1]`

**Eksempel:**
```python
# Læs Logic1 min time (IR 252-253)
high = read_input_register(252)  # High word (bits 31-16)
low = read_input_register(253)   # Low word (bits 15-0)
min_us = (high << 16) | low       # Combine to 32-bit
print(f"Logic1 min execution: {min_us} µs")
```

---

#### **IR 260-267: Max Execution Time (µs)**
| Register | Program | Type | Beskrivelse |
|----------|---------|------|-------------|
| **260-261** | Logic1 | 32-bit | Maximum execution time (microseconds) |
| **262-263** | Logic2 | 32-bit | Maximum execution time |
| **264-265** | Logic3 | 32-bit | Maximum execution time |
| **266-267** | Logic4 | 32-bit | Maximum execution time |

**Same 32-bit format as Min Execution Time.**

---

#### **IR 268-275: Avg Execution Time (µs, calculated)**
| Register | Program | Type | Beskrivelse |
|----------|---------|------|-------------|
| **268-269** | Logic1 | 32-bit | Average execution time (calculated: total_us / exec_count) |
| **270-271** | Logic2 | 32-bit | Average execution time |
| **272-273** | Logic3 | 32-bit | Average execution time |
| **274-275** | Logic4 | 32-bit | Average execution time |

**Calculation:** `avg = total_execution_us / execution_count`

**Note:** Returns 0 if execution_count = 0.

---

#### **IR 276-283: Overrun Count**
| Register | Program | Type | Beskrivelse |
|----------|---------|------|-------------|
| **276-277** | Logic1 | 32-bit | Number of times execution > target interval |
| **278-279** | Logic2 | 32-bit | Overrun count |
| **280-281** | Logic3 | 32-bit | Overrun count |
| **282-283** | Logic4 | 32-bit | Overrun count |

**Definition:** Overrun = execution time > `execution_interval_ms` (default 10ms)

**Eksempel:**
- Hvis Logic1 execution = 12ms og interval = 10ms → overrun count++
- Overrun rate % = (overrun_count / execution_count) × 100

---

### Global Cycle Statistics

#### **IR 284-285: Global Cycle Min Time (ms)**
| Register | Type | Beskrivelse |
|----------|------|-------------|
| **284-285** | 32-bit | Minimum total cycle time for all 4 programs (milliseconds) |

**Cycle Time Definition:** Tid for at køre Logic1 + Logic2 + Logic3 + Logic4 sequentially.

---

#### **IR 286-287: Global Cycle Max Time (ms)**
| Register | Type | Beskrivelse |
|----------|------|-------------|
| **286-287** | 32-bit | Maximum total cycle time for all 4 programs (milliseconds) |

---

#### **IR 288-289: Global Cycle Overrun Count**
| Register | Type | Beskrivelse |
|----------|------|-------------|
| **288-289** | 32-bit | Number of times total cycle > target interval |

**Definition:** Global overrun = (Logic1 + Logic2 + Logic3 + Logic4 execution time) > interval

---

#### **IR 290-291: Total Cycles Executed**
| Register | Type | Beskrivelse |
|----------|------|-------------|
| **290-291** | 32-bit | Total number of ST Logic cycles executed since boot/reset |

**Note:** Increments hver gang scheduler kører alle enabled programs.

---

#### **IR 292-293: Execution Interval (Read-Only Copy)**
| Register | Type | Beskrivelse |
|----------|------|-------------|
| **292-293** | 32-bit | Current execution interval in milliseconds (read-only) |

**Default:** 10ms

**Note:** For at ÆNDRE interval, brug HR 236-237 eller CLI `set logic interval:X`.

---

## 🎛️ Holding Registers (ST Logic Control)

### **HR 200-203: Program Control**
| Register | Program | Bits | Access | Beskrivelse |
|----------|---------|------|--------|-------------|
| **200** | Logic1 | [15:0] | R/W | Control flags |
| **201** | Logic2 | [15:0] | R/W | Control flags |
| **202** | Logic3 | [15:0] | R/W | Control flags |
| **203** | Logic4 | [15:0] | R/W | Control flags |

**Control Bits:**
- Bit 0: `ST_LOGIC_CONTROL_ENABLE` (0x0001) - Enable/disable program
  - Write 1 → enable program
  - Write 0 → disable program
- Bit 1: `ST_LOGIC_CONTROL_START` (0x0002) - Start/stop (future use)
- Bit 2: `ST_LOGIC_CONTROL_RESET_ERROR` (0x0004) - Reset error flag
  - Write 1 → clear error counter + error string
  - **Auto-clears after processing** (read-back = 0)

**Eksempel:**
```python
# Enable Logic1
write_register(200, 0x0001)  # Set bit 0

# Reset error for Logic2
write_register(201, 0x0004)  # Set bit 2 (auto-clears)

# Disable Logic3
write_register(202, 0x0000)  # Clear bit 0
```

---

### **HR 204-235: Variable Input - Direct Write to ST Logic Variables (v4.2.0)**
| Register Range | Program | Variables | Access | Beskrivelse |
|----------------|---------|-----------|--------|-------------|
| **204-211** | Logic1 | Var[0]-Var[7] | W | Direct write to Logic1 variables (bytecode) |
| **212-219** | Logic2 | Var[0]-Var[7] | W | Direct write to Logic2 variables (bytecode) |
| **220-227** | Logic3 | Var[0]-Var[7] | W | Direct write to Logic3 variables (bytecode) |
| **228-235** | Logic4 | Var[0]-Var[7] | W | Direct write to Logic4 variables (bytecode) |

**Status:** Implementeret i v4.2.0 - DIREKTE mapping uden variable binding lookup.

**Funktionalitet:**
- Modbus master skriver direkte til ST Logic variabel bytecode
- **Deterministisk mapping:** `prog_id = (addr - 204) / 8`, `var_index = (addr - 204) % 8`
- **Type-aware konvertering:**
  - `ST_TYPE_BOOL`: Value ≠ 0 konverteres til bool `true`/`false`
  - `ST_TYPE_REAL`: 16-bit integer castes til float
  - `ST_TYPE_INT/DWORD`: 16-bit signed integer
- **Bounds checking:** Writes ignoreres hvis program ikke compiled eller var_index >= var_count
- **Debug output:** `[ST_VAR_INPUT] Logic# var[#] = value (type=#)` hvis debug enabled

**Eksempler:**
```
# Skriv Logic1 var[0] (address 204) værdi 42
Write HR 204 = 42

# Skriv Logic2 var[3] (address 215) værdi 1 (BOOL)
Write HR 215 = 1 (true)

# Skriv Logic4 var[7] (address 235) værdi 255
Write HR 235 = 255
```

**Alternativ:** Variable bindings via CLI (`set logic X bind var_name reg:Y`) for permanent mapping med CLI kontrol.

---

### **HR 236-237: Execution Interval Control (v4.1.0)**
| Register | Type | Access | Beskrivelse |
|----------|------|--------|-------------|
| **236-237** | 32-bit | R/W | Global execution interval in milliseconds |

**Allowed Values:** 10, 20, 25, 50, 75, 100 (ms only)

**Validation:**
- Ugyldige værdier afvises automatisk
- Register nulstilles til nuværende værdi hvis invalid
- Debug output viser fejl

**Eksempel:**
```python
# Sæt interval til 50ms
write_register(236, 0)   # High word = 0
write_register(237, 50)  # Low word = 50

# Læs nuværende interval
high = read_holding_register(236)
low = read_holding_register(237)
interval = (high << 16) | low
print(f"Interval: {interval} ms")
```

**CLI Alternativ:**
```bash
set logic interval:50
save
```

---

## 🔢 Dynamic Register Mappings

Disse registre har **INGEN faste adresser**. Brugeren konfigurerer dem via CLI kommandoer.

### Counter Registers

Hver counter kan bruge op til **5 registre** (alle bruger-definerede adresser):

| Register Type | CLI Parameter | Type | Beskrivelse | Eksempel |
|---------------|---------------|------|-------------|----------|
| **index_reg** | `index-reg:X` | 16-bit | Scaled counter value = value × scale | `index-reg:100` |
| **raw_reg** | `raw-reg:Y` | 16-bit | Prescaled value = value / prescaler | `raw-reg:101` |
| **freq_reg** | `freq-reg:Z` | 16-bit | Measured frequency in Hz (0-20000 Hz) | `freq-reg:102` |
| **overload_reg** | `overload-reg:W` | 16-bit | Overflow flag (0 or 1) | `overload-reg:103` |
| **ctrl_reg** | `ctrl-reg:V` | 16-bit | Control/status register (bits) | `ctrl-reg:104` |

**Control Register Bits (ctrl_reg):**
- Bit 0: Counter enabled (read-only)
- Bit 1: Direction (0=up, 1=down, read-only)
- Bit 2: Overflow detected (read-only)
- Bit 3: Reset counter (write 1 to reset)
- Bit 4: Compare match (read-only, if compare_val configured)

**CLI Configuration Example:**
```bash
set counter 1 index-reg:100 raw-reg:101 freq-reg:102 overload-reg:103 ctrl-reg:104
set counter 1 enabled:true prescaler:16 scale:10
save
```

**Modbus Read Example:**
```python
# Læs Counter1 scaled value
scaled_value = read_holding_register(100)  # index_reg
print(f"Counter1 scaled: {scaled_value}")

# Læs Counter1 frequency
freq_hz = read_holding_register(102)  # freq_reg
print(f"Counter1 frequency: {freq_hz} Hz")

# Reset Counter1
write_register(104, 0x0008)  # ctrl_reg bit 3
```

**Register Count Per Counter:**
- Minimum: 1 register (index_reg kun)
- Maximum: 5 registers (alle funktioner)
- Total capacity: Op til 4 counters × 5 registers = 20 registre maksimalt

---

### Timer Registers

Hver timer bruger **1 register** (bruger-defineret adresse):

| Register Type | CLI Parameter | Type | Beskrivelse | Eksempel |
|---------------|---------------|------|-------------|----------|
| **ctrl_reg** | `ctrl-reg:X` | 16-bit | Timer control/status register | `ctrl-reg:50` |

**Control Register Bits (ctrl_reg):**
- Bit 0: Timer enabled (read/write)
- Bit 1: Timer mode (0-4, read-only)
- Bits 2-7: Timer state (mode-dependent, read-only)
- Bit 8: Manual trigger (write 1 to trigger mode 1/2)
- Bit 9: Reset timer (write 1 to reset)

**CLI Configuration Example:**
```bash
set timer 1 ctrl-reg:50
set timer 1 mode:3 enabled:true t1:1000 t2:500  # Astable blink
save
```

**Modbus Control Example:**
```python
# Enable Timer1
ctrl = read_holding_register(50)
ctrl |= 0x0001  # Set bit 0
write_register(50, ctrl)

# Trigger Timer1 (mode 1/2 only)
write_register(50, 0x0100)  # Set bit 8

# Read Timer1 state
ctrl = read_holding_register(50)
enabled = (ctrl & 0x0001) != 0
mode = (ctrl >> 1) & 0x07
print(f"Timer1: enabled={enabled}, mode={mode}")
```

---

### Persistent Registers

**Persistent registers** er bruger-defineret data storage som gemmes til NVS.

| CLI Parameter | Type | Beskrivelse | Eksempel |
|---------------|------|-------------|----------|
| `persist-reg:X=value` | 16-bit | Store arbitrary 16-bit value at address X | `persist-reg:150=4242` |

**Max Count:** 32 persistent registers (configurerbar)

**CLI Configuration:**
```bash
set persist enable:true
set persist group:0 persist-reg:150=4242
set persist group:1 persist-reg:151=1234
save
```

**Features:**
- Values overlever reboot (stored in NVS)
- Read/Write via Modbus FC03/FC06/FC10
- Kan bruges til user settings, calibration data, etc.

**Modbus Example:**
```python
# Læs persistent register 150
value = read_holding_register(150)
print(f"Persist reg 150: {value}")

# Skriv til persistent register 150
write_register(150, 9999)
# Note: Auto-saves til NVS (konfigureret via CLI)
```

---

## 🔘 Coils & Discrete Inputs

**Coils** og **Discrete Inputs** er dynamisk mappede til GPIO pins eller timer outputs.

### GPIO → Coil/Discrete Input Mapping

| CLI Parameter | Type | Direction | Beskrivelse | Eksempel |
|---------------|------|-----------|-------------|----------|
| `coil:X` | Coil | Output | Map GPIO output pin to coil address X | `coil:10` |
| `input-dis:Y` | Discrete Input | Input | Map GPIO input pin to discrete input Y | `input-dis:20` |

**CLI Configuration Example:**
```bash
# Map GPIO25 (output) to Coil #10
set gpio 25 coil:10

# Map GPIO26 (input) to Discrete Input #20
set gpio 26 input-dis:20

save
```

**Modbus Control Example:**
```python
# Set Coil #10 (GPIO25) HIGH
write_coil(10, True)

# Read Discrete Input #20 (GPIO26)
state = read_discrete_input(20)
print(f"GPIO26 state: {state}")
```

**Max Count:**
- Coils: 256 (0-255)
- Discrete Inputs: 256 (0-255)

---

### Timer Output → Coil Mapping

Timer outputs kan også mappes til coils:

```bash
set timer 1 ctrl-reg:50
set timer 1 output-coil:15  # Map timer output to Coil #15
save
```

**Modbus Read:**
```python
# Læs Timer1 output state via Coil #15
state = read_coil(15)
print(f"Timer1 output: {state}")
```

---

## 📋 Register Usage Summary

| Category | Type | Address Range | Count | Fixed/Dynamic |
|----------|------|---------------|-------|---------------|
| **ST Logic Status** | IR | 200-203 | 4 | Fixed |
| **ST Logic Exec Count** | IR | 204-207 | 4 | Fixed |
| **ST Logic Error Count** | IR | 208-211 | 4 | Fixed |
| **ST Logic Error Code** | IR | 212-215 | 4 | Fixed |
| **ST Logic Var Count** | IR | 216-219 | 4 | Fixed |
| **ST Logic Var Values** | IR | 220-251 | 32 | Fixed |
| **ST Logic Min Time** | IR | 252-259 | 8 (4×32-bit) | Fixed |
| **ST Logic Max Time** | IR | 260-267 | 8 (4×32-bit) | Fixed |
| **ST Logic Avg Time** | IR | 268-275 | 8 (4×32-bit) | Fixed |
| **ST Logic Overrun Count** | IR | 276-283 | 8 (4×32-bit) | Fixed |
| **Global Cycle Min** | IR | 284-285 | 2 (32-bit) | Fixed |
| **Global Cycle Max** | IR | 286-287 | 2 (32-bit) | Fixed |
| **Global Cycle Overrun** | IR | 288-289 | 2 (32-bit) | Fixed |
| **Total Cycles** | IR | 290-291 | 2 (32-bit) | Fixed |
| **Exec Interval (RO)** | IR | 292-293 | 2 (32-bit) | Fixed |
| **ST Logic Control** | HR | 200-203 | 4 | Fixed |
| **ST Logic Var Input** | HR | 204-235 | 32 (reserved) | Fixed |
| **Exec Interval (RW)** | HR | 236-237 | 2 (32-bit) | Fixed |
| **Counter Registers** | HR | 0-255 | 0-20 | Dynamic |
| **Timer Registers** | HR | 0-255 | 0-4 | Dynamic |
| **Persistent Registers** | HR | 0-255 | 0-32 | Dynamic |
| **GPIO Coils** | Coil | 0-255 | 0-256 | Dynamic |
| **GPIO Discrete Inputs** | DI | 0-255 | 0-256 | Dynamic |

**Total Fixed Register Usage:**
- Input Registers: 94 (IR 200-293)
- Holding Registers: 38 (HR 200-237)

**Available Dynamic Space:**
- HR 0-199, 238-255: 218 registers for counters/timers/persistent
- Coils/DI: 256 each (but overlap med ST Logic addresser skal undgås)

---

## 🚨 Address Collision Avoidance

**VIGTIGT:** Undgå at mappe dynamiske registre til ST Logic addresser!

### Reserved Address Ranges (DO NOT USE for dynamic mappings)

**Input Registers:**
- ❌ **IR 200-293** - Reserved for ST Logic (læs-kun)

**Holding Registers:**
- ❌ **HR 200-237** - Reserved for ST Logic Control

### Safe Address Ranges for Dynamic Mappings

**Holding Registers (Counter/Timer/Persist):**
- ✅ **HR 0-199** - Safe for dynamic mappings (200 registers)
- ✅ **HR 238-255** - Safe for dynamic mappings (18 registers)

**Coils & Discrete Inputs:**
- ✅ **Coil 0-255** - All available (avoid overlap med GPIO pins)
- ✅ **DI 0-255** - All available (avoid overlap med GPIO pins)

**Best Practice:**
```bash
# GODT: Counters i safe zone
set counter 1 index-reg:10 raw-reg:11 freq-reg:12

# DÅRLIGT: Collision med ST Logic!
set counter 1 index-reg:200  # ❌ Overlaps med ST Logic Status!
```

---

## 📖 CLI Commands Reference

### ST Logic Commands
```bash
# Status
show logic stats              # All programs performance stats
show logic 1 timing           # Detailed timing for Logic1
show logic 1                  # Show Logic1 config

# Control
set logic 1 enabled:true      # Enable Logic1
set logic interval:50         # Set execution interval to 50ms
set logic stats reset all     # Reset all statistics
set logic debug:true          # Enable debug output

# Upload program
set logic 1 upload "VAR x:INT; END_VAR x:=x+1;"

# Variable binding
set logic 1 bind x reg:100    # Bind variable x to HR 100
```

### Counter Commands
```bash
# Configuration
set counter 1 index-reg:10 raw-reg:11 freq-reg:12
set counter 1 enabled:true prescaler:16 scale:10
show counter 1

# Reset via CLI
set counter 1 reset           # Reset counter value
```

### Timer Commands
```bash
# Configuration
set timer 1 ctrl-reg:50
set timer 1 mode:3 t1:1000 t2:500  # Astable blink
set timer 1 enabled:true
show timer 1

# Control
set timer 1 trigger           # Manual trigger (mode 1/2)
```

### GPIO Commands
```bash
# Map GPIO pins
set gpio 25 coil:10           # GPIO25 output → Coil #10
set gpio 26 input-dis:20      # GPIO26 input → DI #20
show gpio
```

### Persistent Register Commands
```bash
# Enable persistence
set persist enable:true

# Define persistent registers
set persist group:0 persist-reg:150=4242
set persist group:1 persist-reg:151=1234

# Save to NVS
save
```

---

## 🔧 Python Modbus Client Examples

### Read ST Logic Statistics
```python
from pymodbus.client import ModbusTcpClient

client = ModbusTcpClient('192.168.1.100')

# Read Logic1 status (IR 200)
status = client.read_input_registers(200, 1).registers[0]
enabled = (status & 0x0001) != 0
compiled = (status & 0x0002) != 0
error = (status & 0x0008) != 0
print(f"Logic1: enabled={enabled}, compiled={compiled}, error={error}")

# Read Logic1 avg execution time (IR 268-269, 32-bit)
high = client.read_input_registers(268, 1).registers[0]
low = client.read_input_registers(269, 1).registers[0]
avg_us = (high << 16) | low
print(f"Logic1 avg execution: {avg_us} µs")

# Read execution interval (IR 292-293, 32-bit)
high = client.read_input_registers(292, 1).registers[0]
low = client.read_input_registers(293, 1).registers[0]
interval = (high << 16) | low
print(f"Execution interval: {interval} ms")
```

### Control ST Logic via Modbus
```python
# Enable Logic1 (HR 200, bit 0)
client.write_register(200, 0x0001)

# Reset Logic2 error (HR 201, bit 2)
client.write_register(201, 0x0004)

# Set execution interval to 50ms (HR 236-237, 32-bit)
client.write_register(236, 0)   # High word
client.write_register(237, 50)  # Low word
```

### Read Counter Values
```python
# Antag Counter1: index-reg=10, freq-reg=12
scaled_value = client.read_holding_registers(10, 1).registers[0]
frequency = client.read_holding_registers(12, 1).registers[0]
print(f"Counter1: value={scaled_value}, freq={frequency} Hz")

# Reset Counter1 via ctrl-reg (bit 3)
client.write_register(104, 0x0008)
```

### Control GPIO via Coils
```python
# Set Coil #10 (GPIO25 output) HIGH
client.write_coil(10, True)

# Read Discrete Input #20 (GPIO26 input)
state = client.read_discrete_inputs(20, 1).bits[0]
print(f"GPIO26 state: {state}")
```

---

## 📚 Related Documentation

- `ST_MONITORING.md` - Performance monitoring og tuning guide
- `TIMING_ANALYSIS.md` - ST Logic timing deep dive
- `CLAUDE.md` - Architecture overview (Layer 5: ST Logic Engine)
- `BUGS.md` - Bug tracking (BUG-001 til BUG-007 fixed)

---

## 🔗 Quick Links

**CLI Help:**
```bash
help                    # General help
show logic stats        # Performance statistics
show counter 1          # Counter 1 status
show timer 1            # Timer 1 status
show gpio               # GPIO mappings
```

**Modbus Function Codes:**
- FC01 (0x01): Read Coils
- FC02 (0x02): Read Discrete Inputs
- FC03 (0x03): Read Holding Registers
- FC04 (0x04): Read Input Registers
- FC05 (0x05): Write Single Coil
- FC06 (0x06): Write Single Register
- FC0F (0x0F): Write Multiple Coils
- FC10 (0x10): Write Multiple Registers

---

**Version:** v4.1.0
**Author:** Claude Code
**Last Updated:** 2025-12-12
