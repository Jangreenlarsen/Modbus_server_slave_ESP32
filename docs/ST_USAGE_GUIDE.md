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
# Bind counter to read from HR#100
set logic 1 bind 0 100 input

# Bind relay to write to HR#101
set logic 1 bind 1 101 output
```

✓ Output:
```
✓ Logic1: var[0] ← Modbus HR#100
✓ Logic1: var[1] → Modbus HR#101
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

# Bind variable to Modbus register
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
set logic 1 bind 0 10 input    # temperature reads from HR#10
set logic 1 bind 1 11 output   # heating writes to HR#11
set logic 1 bind 2 12 output   # cooling writes to HR#12
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
set logic 1 bind 0 20 output   # count writes to HR#20
set logic 1 bind 1 21 output   # total writes to HR#21
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
set logic 1 bind 0 30 input    # value1 reads from HR#30
set logic 1 bind 1 31 input    # value2 reads from HR#31
set logic 1 bind 2 32 output   # min_value writes to HR#32
set logic 1 bind 3 33 output   # max_value writes to HR#33
set logic 1 bind 4 34 output   # abs_value writes to HR#34
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

Variables marked as `input` read from Modbus holding registers **before** program execution:

```bash
set logic 1 bind 0 100 input   # var[0] ← HR#100
```

When the program runs, `var[0]` will contain the value from `HR#100`.

### Output Variables (Write to Registers)

Variables marked as `output` write to Modbus holding registers **after** program execution:

```bash
set logic 1 bind 1 101 output  # var[1] → HR#101
```

When the program completes, the value of `var[1]` is written to `HR#101`.

### Bidirectional Variables

Variables marked as `both` read before AND write after:

```bash
set logic 1 bind 0 100 both    # var[0] ↔ HR#100
```

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
set logic 1 bind 0 100 input   # Configure bindings first!
```

---

### Variables Not Updating in Modbus

**Cause:** Output variables not bound to registers

**Solution:**
```bash
set logic 1 bind 1 101 output   # Bind output variables
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
