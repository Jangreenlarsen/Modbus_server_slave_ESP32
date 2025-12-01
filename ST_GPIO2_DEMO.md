# ST Logic Mode - GPIO2 LED Demo

**Mål:** Styre GPIO2 LED via ST Logic program

GPIO2 har en blåt LED forbundet (heartbeat). Vi kan styre det via ST Logic!

---

## 📋 Demo 1: Simple LED Control (Tænd/Sluk baseret på register)

### 1. Aktivér GPIO2 til brugerbrug

```bash
# Deaktivér heartbeat (gør GPIO2 tilgængelig)
set gpio2 user_mode:true
```

**Output:**
```
[OK] GPIO2 user mode enabled (heartbeat disabled)
```

### 2. Opret ST Logic program

```bash
set logic 1 upload "VAR led_on: BOOL; counter: INT; END_VAR IF counter > 50 THEN led_on := TRUE; ELSE led_on := FALSE; END_IF;"
```

**Output:**
```
✓ COMPILATION SUCCESSFUL
  Program: Logic1
  Source: 180 bytes
  Bytecode: 32 instructions
  Variables: 2
```

### 3. Bind variabler til registre

```bash
# counter læser fra HR#10 (du skriver værdier her)
set logic 1 bind counter reg:10

# led_on skriver til Coil#0 (GPIO2 mapper til coil 0)
set logic 1 bind led_on coil:0
```

**Output:**
```
[OK] Logic1: var[0] (counter) ← Modbus HR#10 (input)
[OK] Logic1: var[1] (led_on) → Modbus Coil#0 (output)
```

### 4. Aktivér programmet

```bash
set logic 1 enabled:true
```

### 5. Test det!

**Test A: Tænd LED**
```bash
# Sæt counter > 50 → LED tændes
set holding_register 10 75

# Tjek status
show logic 1
```

**Output:**
```
Variable Bindings:
  [0] counter ← HR#10 (input)
  [1] led_on → Coil#0 (output)
  Total: 2 bindings

Statistics:
  Executions: 150
  Errors: 0
```

**Du skulle se:** 🔵 **LED tænder!**

**Test B: Sluk LED**
```bash
# Sæt counter < 50 → LED slukker
set holding_register 10 25

# Tjek
show logic 1
```

**Du skulle se:** 🔵 **LED slukker!**

---

## 📋 Demo 2: Pulsing LED (Blinking effect)

Lad LED blinke når en betingelse er opfyldt:

```bash
set logic 2 upload "VAR counter: INT; pulse: INT; led: BOOL; END_VAR counter := 0; FOR counter := 0 TO 100 DO pulse := pulse + 1; END_FOR; IF pulse > 50 THEN led := TRUE; ELSE led := FALSE; END_IF;"
```

**Bind variabler:**
```bash
set logic 2 bind counter reg:20
set logic 2 bind led coil:0
set logic 2 enabled:true
```

**Resultat:** LED blinker med ~100ms frekvens fordi programmet kører hver 10ms

---

## 📋 Demo 3: Kompleks kontrol - Timer med LED

Lav en "timer" hvor LED tænder i 3 sekunder når du sætter et flag:

```bash
set logic 3 upload "VAR timer: INT; trigger: BOOL; led: BOOL; END_VAR IF trigger THEN timer := timer + 1; ELSE timer := 0; END_IF; IF timer < 300 AND trigger THEN led := TRUE; ELSE led := FALSE; END_IF;"
```

**Bind:**
```bash
set logic 3 bind timer reg:30
set logic 3 bind trigger reg:31
set logic 3 bind led coil:0
set logic 3 enabled:true
```

**Test:**
```bash
# Start timer
set holding_register 31 1

# After 3 seconds it auto-stops
set holding_register 31 0
```

---

## 🔍 Diagnostik

### Se alle bindings for GPIO2:
```bash
# Vis program detaljer
show logic 1

# Output viser alle coil bindings:
# [1] led_on → Coil#0 (output)
```

### Se fejl hvis der er nogle:
```bash
show logic errors
```

### Se samlet status:
```bash
show logic program

# Output:
#   [1] Logic1 🟢 ACTIVE
#       Source: 180 bytes | Variables: 2
#       Executions: 1540 | Errors: 0
```

---

## 📌 GPIO2 Mapping Reference

GPIO2 LED er mapped til **Coil #0**:

```
ST Variable (BOOL)  →  Coil#0  →  GPIO2 LED
     led_on             0         Physical LED
```

Når `led_on := TRUE` → Coil#0 = 1 → GPIO2 LED tændes 🔵

---

## 🎯 Quick Command Reference

```bash
# Setup
set gpio2 user_mode:true                                          # Enable GPIO2 user mode
set logic 1 upload "VAR led: BOOL; counter: INT; END_VAR ..."   # Upload program

# Binding
set logic 1 bind counter reg:10                                  # Input from register
set logic 1 bind led coil:0                                      # Output to GPIO2 LED

# Control
set logic 1 enabled:true                                         # Start program
set holding_register 10 75                                       # Write input value

# Monitor
show logic 1                                                      # See bindings & stats
show logic program                                               # Overview
show logic errors                                                # Check errors
```

---

## 🚀 Complete Example (Copy-Paste Ready)

```bash
# 1. Enable GPIO2
set gpio2 user_mode:true

# 2. Upload program (LED on when counter > 50)
set logic 1 upload "VAR led: BOOL; counter: INT; END_VAR IF counter > 50 THEN led := TRUE; ELSE led := FALSE; END_IF;"

# 3. Bind variables
set logic 1 bind counter reg:10
set logic 1 bind led coil:0

# 4. Enable
set logic 1 enabled:true

# 5. Test - LED should turn ON
set holding_register 10 75

# 6. Verify
show logic 1

# 7. Test - LED should turn OFF
set holding_register 10 25

# 8. Check status
show logic program
```

---

## 📊 Expected Results

After following the demo:

```
=== All Logic Programs ===

  [1] Logic1 🟢 ACTIVE
      Source: 150 bytes | Variables: 2
      Executions: 2340 | Errors: 0

  [2] Logic2 ⚪ EMPTY
  [3] Logic3 ⚪ EMPTY
  [4] Logic4 ⚪ EMPTY
```

**Physical LED:** Changes state based on `set holding_register 10 <value>`

- `set holding_register 10 75` → 🔵 **ON** (counter > 50)
- `set holding_register 10 25` → ⚫ **OFF** (counter ≤ 50)

---

## 🔧 Troubleshooting

**Problem:** LED doesn't respond
```bash
# Check if GPIO2 is in user mode
show gpio

# Should show: GPIO2 user_mode: YES
```

**Problem:** Program shows error
```bash
# Check compilation
show logic errors

# Fix syntax and re-upload
```

**Problem:** Binding not showing
```bash
# Verify binding created
show logic 1

# Should show:
# Variable Bindings:
#   [0] counter ← HR#10 (input)
#   [1] led → Coil#0 (output)
```

---

Enjoy! 🎉
