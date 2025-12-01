# Structured Text Logic Mode - Usage Guide

**ESP32 Modbus RTU Server v2.1.0+** (with unified VariableMapping system)

---

## 📚 Table of Contents

1. [Quick Start](#quick-start)
2. [CLI Commands](#cli-commands)
3. [ST Language Examples](#st-language-examples)
4. [Variable Bindings (Modbus I/O)](#variable-bindings-modbus-io)
5. [Built-in Functions](#built-in-functions)
6. [Troubleshooting](#troubleshooting)

---

## Quick Start

### 1. Upload a Simple Logic Program

```bash
# ST program: If counter > 100, set relay ON
set logic 1 upload "VAR counter: INT; relay: BOOL; END_VAR IF counter > 100 THEN relay := TRUE; ELSE relay := FALSE; END_IF;"
```

✓ Output: `Logic1 compiled successfully (xxx bytes, xx instructions)`

### 2. Bind Variables to Modbus Registers

```bash
# NEW SYNTAX: Bind by variable name
set logic 1 bind counter reg:100      # counter reads from holding register #100
set logic 1 bind relay coil:101       # relay writes to coil #101
```

✓ Output:
```
✓ Logic1: var[0] (counter) ← Modbus HR#100
✓ Logic1: var[1] (relay) → Modbus Coil#101
```

**Alternative (legacy) syntax:**
```bash
set logic 1 bind 0 100 input          # Variable index 0, register 100, input mode
set logic 1 bind 1 101 output         # Variable index 1, register 101, output mode
```

### 3. Enable the Program

```bash
set logic 1 enabled:true
```

✓ Output: `✓ Logic1 ENABLED`

### 4. Verify Configuration

```bash
show logic 1
```

Output:
```
=== Logic Program: Logic1 ===
Enabled: YES
Compiled: YES
Source Code: xxx bytes

Source:
VAR counter: INT; relay: BOOL; END_VAR IF counter > 100...

Variable Bindings: 2
  [0] ST var[0] ← Modbus HR#100
  [1] ST var[1] → Modbus HR#101

Executions: 42
Errors: 0
```

---

## CLI Commands

### Upload & Manage Programs

```bash
# Upload ST source code
set logic <id> upload "<st_code>"

# Enable/disable program
set logic <id> enabled:true
set logic <id> enabled:false

# Delete program
set logic <id> delete

# Bind variable to Modbus register (NEW SYNTAX - recommended)
set logic <id> bind <var_name> reg:<register>      # Holding register (INT/DWORD)
set logic <id> bind <var_name> coil:<register>     # Coil output (BOOL write)
set logic <id> bind <var_name> input-dis:<input>   # Discrete input (BOOL read)

# Legacy syntax (still supported for backward compatibility)
set logic <id> bind <var_idx> <register> [input|output|both]
```

### View Status

```bash
# Show detailed info about one program
show logic <id>

# Show all programs
show logic all

# Show execution statistics
show logic stats
```

---

## ST Language Examples

### Example 1: Simple IF/ELSE (Threshold Logic)

```structured-text
VAR
  temperature: INT;
  heating: BOOL;
  cooling: BOOL;
END_VAR

IF temperature < 18 THEN
  heating := TRUE;
  cooling := FALSE;
ELSIF temperature > 25 THEN
  heating := FALSE;
  cooling := TRUE;
ELSE
  heating := FALSE;
  cooling := FALSE;
END_IF;
```

**CLI:**
```bash
set logic 1 upload "VAR temperature: INT; heating: BOOL; cooling: BOOL; END_VAR IF temperature < 18 THEN heating := TRUE; cooling := FALSE; ELSIF temperature > 25 THEN heating := FALSE; cooling := TRUE; ELSE heating := FALSE; cooling := FALSE; END_IF;"
set logic 1 bind temperature reg:10  # temperature reads from HR#10
set logic 1 bind heating coil:11     # heating writes to coil #11
set logic 1 bind cooling coil:12     # cooling writes to coil #12
set logic 1 enabled:true
```

---

### Example 2: Counting Loop

```structured-text
VAR
  count: INT;
  total: INT;
END_VAR

count := 0;
total := 0;

FOR count := 1 TO 10 DO
  total := total + count;
END_FOR;
```

**CLI:**
```bash
set logic 1 upload "VAR count: INT; total: INT; END_VAR count := 0; total := 0; FOR count := 1 TO 10 DO total := total + count; END_FOR;"
set logic 1 bind count reg:20   # count writes to HR#20
set logic 1 bind total reg:21   # total writes to HR#21
set logic 1 enabled:true
```

Result after execution: `total = 55` (1+2+3+...+10)

---

### Example 3: Using WHILE Loop with Condition

```structured-text
VAR
  sensor: INT;
  pulses: INT;
END_VAR

sensor := 0;
pulses := 0;

WHILE sensor < 100 DO
  sensor := sensor + 10;
  pulses := pulses + 1;
END_WHILE;
```

---

### Example 4: Using Built-in Functions

```structured-text
VAR
  value1: INT;
  value2: INT;
  min_value: INT;
  max_value: INT;
  abs_value: INT;
END_VAR

min_value := MIN(value1, value2);
max_value := MAX(value1, value2);
abs_value := ABS(value1);
```

**CLI:**
```bash
set logic 1 upload "VAR value1: INT; value2: INT; min_value: INT; max_value: INT; abs_value: INT; END_VAR min_value := MIN(value1, value2); max_value := MAX(value1, value2); abs_value := ABS(value1);"
set logic 1 bind value1 reg:30      # value1 reads from HR#30
set logic 1 bind value2 reg:31      # value2 reads from HR#31
set logic 1 bind min_value reg:32   # min_value writes to HR#32
set logic 1 bind max_value reg:33   # max_value writes to HR#33
set logic 1 bind abs_value reg:34   # abs_value writes to HR#34
set logic 1 enabled:true
```

---

### Example 5: Nested IF Statements

```structured-text
VAR
  x: INT;
  y: INT;
  result: INT;
END_VAR

IF x > 10 THEN
  IF y > 20 THEN
    result := 3;
  ELSE
    result := 2;
  END_IF;
ELSE
  result := 1;
END_IF;
```

---

## Variable Bindings (Modbus I/O)

**[V2.1.0+]** Variable bindings use the unified **VariableMapping system** which also handles GPIO pins. Both GPIO and ST variables use the same I/O engine.

**3-Phase Execution Model (every 10ms):**
```
Phase 1: Read all INPUTs (GPIO + ST variables from Modbus)
Phase 2: Execute enabled ST programs
Phase 3: Write all OUTPUTs (GPIO + ST variables to Modbus)
```

This means ST variables and GPIO pins are treated identically - they both sync with Modbus through the same unified mapping system.

### Input Variables (Read from Registers)

Variables read from Modbus holding registers **before** program execution:

```bash
# NEW SYNTAX: Use reg: for holding registers (automatic input/output based on variable)
set logic 1 bind counter reg:100   # counter ← HR#100 before execution
```

When the program runs, `counter` will contain the value from `HR#100`.

### Output Variables (Write to Registers)

Variables write to Modbus registers **after** program execution:

```bash
# Write to coil (BOOL output)
set logic 1 bind relay coil:10     # relay → Coil#10 after execution

# Write to holding register (INT output)
set logic 1 bind result reg:101    # result → HR#101 after execution
```

When the program completes, the value is written to the register.

### Input-Only BOOL Variables

For discrete inputs (read-only boolean):

```bash
set logic 1 bind button input-dis:5   # button ← Discrete Input#5 before execution
```

**Legend:**
- `reg:` → Holding Register (INT/DWORD, can be input or output)
- `coil:` → Coil (BOOL, write-only)
- `input-dis:` → Discrete Input (BOOL, read-only)

---

## Built-in Functions

### Mathematical Functions

```structured-text
result := ABS(-42);              (* → 42 *)
min_val := MIN(10, 5);           (* → 5 *)
max_val := MAX(10, 5);           (* → 10 *)
sum_val := SUM(10, 5);           (* → 15 *)
sqrt_val := SQRT(16.0);          (* → 4.0 *)
rounded := ROUND(3.7);           (* → 4 *)
truncated := TRUNC(3.9);         (* → 3 *)
floored := FLOOR(3.7);           (* → 3 *)
ceiled := CEIL(3.2);             (* → 4 *)
```

### Type Conversion Functions

```structured-text
real_val := INT_TO_REAL(42);     (* → 42.0 *)
int_val := REAL_TO_INT(3.7);     (* → 3 *)
int_from_bool := BOOL_TO_INT(TRUE);  (* → 1 *)
bool_from_int := INT_TO_BOOL(42);    (* → TRUE *)
int_from_dword := DWORD_TO_INT(1000);
dword_from_int := INT_TO_DWORD(42);
```

---

## Execution Flow (Unified Mapping Model)

```
┌──────────────────────────────────────────────────────────┐
│ Modbus Server Main Loop (every 10ms)                     │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  PHASE 1: Sync INPUTs                                   │
│  ┌─────────────────────────────────────┐                │
│  │ gpio_mapping_update()                │                │
│  │  - Read GPIO pins → Discrete inputs  │                │
│  │  - Read HR# → ST variables (input)   │                │
│  └─────────────────────────────────────┘                │
│                                                          │
│  PHASE 2: Execute Programs                              │
│  ┌─────────────────────────────────────┐                │
│  │ st_logic_engine_loop()                │                │
│  │  - For each enabled Logic program:    │                │
│  │    * Execute compiled bytecode        │                │
│  │    * Programs run independently       │                │
│  │    * Non-blocking (< 1ms)             │                │
│  └─────────────────────────────────────┘                │
│                                                          │
│  PHASE 3: Sync OUTPUTs                                  │
│  ┌─────────────────────────────────────┐                │
│  │ gpio_mapping_update()                │                │
│  │  - Read Coils → GPIO pins            │                │
│  │  - Read ST variables (output) → HR# │                │
│  └─────────────────────────────────────┘                │
│                                                          │
│  Continue Modbus RTU service...                          │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

**Key Points:**
- Each program executes **non-blocking** and **independently**
- GPIO mapping and ST variables use the same **unified VariableMapping** engine
- **Both** read inputs before program execution
- **Both** write outputs after program execution
- ST variables and GPIO pins are treated identically
- Programs don't interfere with each other or the Modbus RTU service

---

## Troubleshooting

### Program Won't Compile

**Error:** `Compile error: Parse error...`

**Solution:** Check ST syntax. Common issues:
- Missing `END_VAR` after variable declarations
- Missing `;` at end of statements
- Misspelled keywords (keywords are case-insensitive)

**Example (WRONG):**
```
VAR x: INT END_VAR IF x > 10 THEN x := 1 END_IF
                  ↑ Missing semicolon
```

**Example (CORRECT):**
```
VAR x: INT; END_VAR IF x > 10 THEN x := 1; END_IF;
```

---

### Program Compiles But No Output

**Cause:** Program not enabled or bindings not configured

**Solution:**
```bash
show logic 1   # Check if compiled and enabled
set logic 1 enabled:true
set logic 1 bind myvar reg:100   # Configure bindings first! (replace myvar with actual variable name)
```

---

### Variables Not Updating in Modbus

**Cause:** Output variables not bound to registers

**Solution:**
```bash
set logic 1 bind myoutput reg:101   # Bind output variable (replace myoutput with actual variable name)
```

Then check Modbus register #101 after executing the program.

---

### Program Runs But Wrong Results

**Cause:** Logic error or type mismatch

**Solution:**
1. Verify variable types (INT, BOOL, REAL, DWORD)
2. Check Modbus register ranges (0-159)
3. Verify operator precedence (use parentheses when unsure)
4. Test with simpler program first

---

## Performance Notes

- **Execution Time:** ~1-5ms per program (depends on complexity)
- **Memory:** ~50KB for 4 programs with source + bytecode
- **Cycle Time:** Programs run every ~10ms (non-blocking)
- **Register Limit:** 160 holding registers (0-159) for all I/O

---

## Security & Safety

⚠️ **Important:**

- Logic programs run in **sandbox** (limited to bytecode execution)
- No arbitrary code execution or memory access
- Programs can only read/write assigned registers
- Runtime errors stop execution of that program (others continue)
- Maximum execution steps: 10,000 (prevents infinite loops)

---

## Next Steps

1. Upload first logic program: `set logic 1 upload "..."`
2. Bind variables to registers: `set logic 1 bind ...`
3. Enable program: `set logic 1 enabled:true`
4. Use Modbus master to read/write registers
5. Monitor with: `show logic stats`

**Happy Programming!** 🚀

---

## Version Information

- **Feature:** Structured Text Logic Mode
- **IEC Standard:** 61131-3 (ST-Light Profile)
- **First Release:** v2.0.0 (2025-11-30)
- **Status:** Production Ready ✅
