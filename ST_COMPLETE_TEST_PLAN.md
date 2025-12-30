# ST Logic - Komplet Testplan

**Version:** v4.4.2
**Build:** #787+
**Formål:** Systematisk test af ALLE ST-funktioner

---

## 📋 Indholdsfortegnelse

1. [Test Oversigt](#test-oversigt)
2. [Hardware Setup](#hardware-setup)
3. [Fase 1: Individuelle Funktionstests](#fase-1-individuelle-funktionstests)
   - [1.1 Aritmetiske Operatorer](#11-aritmetiske-operatorer)
   - [1.2 Logiske Operatorer](#12-logiske-operatorer)
   - [1.3 Bit-Shift Operatorer](#13-bit-shift-operatorer)
   - [1.4 Sammenlignings Operatorer](#14-sammenlignings-operatorer)
   - [1.5 Builtin Funktioner - Matematiske](#15-builtin-funktioner---matematiske)
   - [1.6 Builtin Funktioner - Clamping/Selection](#16-builtin-funktioner---clampingselection)
   - [1.7 Builtin Funktioner - Trigonometriske](#17-builtin-funktioner---trigonometriske)
   - [1.8 Builtin Funktioner - Type Conversion](#18-builtin-funktioner---type-conversion)
   - [1.9 Builtin Funktioner - Persistence](#19-builtin-funktioner---persistence)
   - [1.10 GPIO & Hardware Tests](#110-gpio--hardware-tests)
   - [1.11 Kontrolstrukturer - IF/THEN/ELSE](#111-kontrolstrukturer---ifthenelse)
   - [1.12 Kontrolstrukturer - CASE](#112-kontrolstrukturer---case)
   - [1.13 Kontrolstrukturer - FOR Loop](#113-kontrolstrukturer---for-loop)
   - [1.14 Kontrolstrukturer - WHILE Loop](#114-kontrolstrukturer---while-loop)
   - [1.15 Kontrolstrukturer - REPEAT Loop](#115-kontrolstrukturer---repeat-loop)
   - [1.16 Type System - INT vs DINT](#116-type-system---int-vs-dint)
4. [Fase 2: Kombinerede Tests](#fase-2-kombinerede-tests)
5. [Test Execution Workflow](#test-execution-workflow)
6. [Fejlhåndtering](#fejlhåndtering)
7. [Udsatte Tests](#udsatte-tests)

---

## Test Oversigt

### Test Statistik
| Kategori | Antal Tests | Estimeret Tid |
|----------|-------------|---------------|
| Aritmetiske operatorer | 6 | 3 min |
| Logiske operatorer | 4 | 2 min |
| Bit-shift operatorer | 2 | 1 min |
| Sammenlignings operatorer | 6 | 3 min |
| Builtin matematiske | 9 | 5 min |
| Builtin clamping/selection | 2 | 2 min |
| Builtin trigonometriske | 3 | 2 min |
| Builtin type conversion | 6 | 3 min |
| Builtin persistence | 2 | 2 min |
| GPIO & Hardware tests | 4 | 5 min |
| Kontrolstrukturer | 5 | 5 min |
| Type System (INT/DINT) | 3 | 5 min |
| **Fase 1 Total** | **52** | **38 min** |
| Kombinerede tests | 10 | 15 min |
| **Total** | **62** | **53 min** |
| **Udsat (Modbus Master)** | **6** | *Senere* |

### Test Konventioner
- ✅ = Test bestået
- ❌ = Test fejlet
- ⚠️ = Advarsel/bemærkning
- 🔄 = Test skal gentages efter fix

---

## Hardware Setup

### GPIO Konfiguration

**Output LEDs:**
| GPIO Pin | Funktion | Binding |
|----------|----------|---------|
| GPIO17 | LED 1 | Coil/Discrete Output |
| GPIO18 | LED 2 | Coil/Discrete Output |
| GPIO19 | LED 3 | Coil/Discrete Output |
| GPIO21 | LED 4 | Coil/Discrete Output |

**Input Switches:**
| GPIO Pin | Funktion | Binding |
|----------|----------|---------|
| GPIO32 | Switch 1 | Discrete Input |
| GPIO33 | Switch 2 | Discrete Input |
| GPIO34 | Switch 3 | Discrete Input |

**Test Signal:**
| GPIO Pin | Signal | Frekvens | Anvendelse |
|----------|--------|----------|------------|
| GPIO25 | Square Wave | 1 kHz | Counter/Timer Test |

### Hardware Test Setup Diagram

```
ESP32
┌─────────────────────────────────┐
│                                 │
│  GPIO17 ──────────────── LED1   │  Output Tests
│  GPIO18 ──────────────── LED2   │  (Coils)
│  GPIO19 ──────────────── LED3   │
│  GPIO21 ──────────────── LED4   │
│                                 │
│  GPIO32 ──────────────── SW1    │  Input Tests
│  GPIO33 ──────────────── SW2    │  (Discrete Inputs)
│  GPIO34 ──────────────── SW3    │
│                                 │
│  GPIO25 ◄────── 1kHz Signal     │  Counter/Timer Tests
│                                 │
└─────────────────────────────────┘
```

### GPIO Mapping i ST Logic

**Output (BOOL → GPIO):**
```bash
# LED control via ST Logic
set logic 1 bind led1 coil:17    # GPIO17
set logic 1 bind led2 coil:18    # GPIO18
set logic 1 bind led3 coil:19    # GPIO19
set logic 1 bind led4 coil:21    # GPIO21
```

**Input (GPIO → BOOL):**
```bash
# Switch read via ST Logic
set logic 1 bind sw1 discrete:32  # GPIO32
set logic 1 bind sw2 discrete:33  # GPIO33
set logic 1 bind sw3 discrete:34  # GPIO34
```

---

# Fase 1: Individuelle Funktionstests

## 1.1 Aritmetiske Operatorer

### Test 1.1.1: Addition (+)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: INT;
END_VAR
BEGIN
  result := a + b;
END_PROGRAM
END_UPLOAD

set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result reg:22 output

set logic 1 enabled:true
```

**Test Cases:**
```bash
# Via Modbus eller CLI
# Test 1: 5 + 3 = 8
write reg 20 value int 5
write reg 21 value int 3
read reg 22 int
# Forventet: 8

# Test 2: -10 + 15 = 5
write reg 20 value int -10
write reg 21 value int 15
read reg 22 int
# Forventet: 5

# Test 3: 0 + 0 = 0
write reg 20 value int 0
write reg 21 value int 0
read reg 22 int
# Forventet: 0

# Test 4: Overflow test (INT_MAX + 1)
write reg 20 value int 32767
write reg 21 value int 1
read reg 22 int
# Forventet: -32768
```

**Forventet Resultat:**
```
✅ 5 + 3 = 8
✅ -10 + 15 = 5
✅ 0 + 0 = 0
⚠️ 32767 + 1 = -32768 (overflow behavior)
```

---

### Test 1.1.2: Subtraktion (-)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: INT;
END_VAR
BEGIN
  result := a - b;
END_PROGRAM
END_UPLOAD

set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: 10 - 3 = 7
write reg 20 value int 10
write reg 21 value int 3
read reg 22 int
# Forventet: 7

# Test 2: 5 - 10 = -5
write reg 20 value int 5
write reg 21 value int 10
read reg 22 int
# Forventet: -5

# Test 3: 0 - 0 = 0
write reg 20 value int 0
write reg 21 value int 0
read reg 22 int
# Forventet: 0
```

**Forventet Resultat:**
```
✅ 10 - 3 = 7
✅ 5 - 10 = -5
✅ 0 - 0 = 0
```

---

### Test 1.1.3: Multiplikation (*)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: INT;
END_VAR
BEGIN
  result := a * b;
END_PROGRAM
END_UPLOAD

set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: 5 * 3 = 15
write reg 20 value int 5
write reg 21 value int 3
read reg 22 int
# Forventet: 15

# Test 2: -4 * 5 = -20
write reg 20 value int -4
write reg 21 value int 5
read reg 22 int
# Forventet: -20

# Test 3: 0 * 100 = 0
write reg 20 value int 0
write reg 21 value int 100
read reg 22 int
# Forventet: 0
```

**Forventet Resultat:**
```
✅ 5 * 3 = 15
✅ -4 * 5 = -20
✅ 0 * 100 = 0
```

---

### Test 1.1.4: Division (/)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: INT;
END_VAR
BEGIN
  result := a / b;
END_PROGRAM
END_UPLOAD

set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: 10 / 2 = 5
write reg 20 value int 10
write reg 21 value int 2
read reg 22 int
# Forventet: 5

# Test 2: 7 / 2 = 3 (integer division)
write reg 20 value int 7
write reg 21 value int 2
read reg 22 int
# Forventet: 3

# Test 3: -10 / 3 = -3
write reg 20 value int -10
write reg 21 value int 3
read reg 22 int
# Forventet: -3

# Test 4: Division by zero (should handle gracefully)
write reg 20 value int 10
write reg 21 value int 0
read reg 22 int
# Forventet: 0
```

**Forventet Resultat:**
```
✅ 10 / 2 = 5
✅ 7 / 2 = 3 (integer division)
✅ -10 / 3 = -3
⚠️ 10 / 0 → handled gracefully (no crash)
```

---

### Test 1.1.5: Modulo (MOD)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: INT;
END_VAR
BEGIN
  result := a MOD b;
END_PROGRAM
END_UPLOAD

set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: 10 MOD 3 = 1
write reg 20 value int 10
write reg 21 value int 3
read reg 22 int
# Forventet: 1

# Test 2: 7 MOD 2 = 1
write reg 20 value int 7
write reg 21 value int 2
read reg 22 int
# Forventet: 1

# Test 3: 8 MOD 4 = 0
write reg 20 value int 8
write reg 21 value int 4
read reg 22 int
# Forventet: 0
```

**Forventet Resultat:**
```
✅ 10 MOD 3 = 1
✅ 7 MOD 2 = 1
✅ 8 MOD 4 = 0
```

---

### Test 1.1.6: Negation (unary -)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  result: INT;
END_VAR
BEGIN
  result := -a;
END_PROGRAM
END_UPLOAD

set logic 1 bind a reg:20 input
set logic 1 bind result reg:21 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: -5 → -5
write reg 20 value int 5
read reg 21 int
# Forventet: -5

# Test 2: -(-10) → 10
write reg 20 value int -10
read reg 21 int
# Forventet: 10

# Test 3: -0 → 0
write reg 20 value int 0
read reg 21 int
# Forventet: 0
```

**Forventet Resultat:**
```
✅ -5 = -5
✅ -(-10) = 10
✅ -0 = 0
```

---

## 1.2 Logiske Operatorer

### Test 1.2.1: Logical AND

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: BOOL;
  b: BOOL;
  result: BOOL;
END_VAR
BEGIN
  result := a AND b;
END_PROGRAM
END_UPLOAD

set logic 1 bind a coil:0 input
set logic 1 bind b coil:1 input
set logic 1 bind result coil:2 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: TRUE AND TRUE = TRUE
write coil 0 value 1
read coil 2
# Forventet: 1

# Test 2: TRUE AND FALSE = FALSE
write coil 0 value 1
read coil 2
# Forventet: 0

# Test 3: FALSE AND TRUE = FALSE
write coil 0 value 0
read coil 2
# Forventet: 0

# Test 4: FALSE AND FALSE = FALSE
write coil 0 value 0
read coil 2
# Forventet: 0
```

**Forventet Resultat:**
```
✅ TRUE AND TRUE = TRUE
✅ TRUE AND FALSE = FALSE
✅ FALSE AND TRUE = FALSE
✅ FALSE AND FALSE = FALSE
```

---

### Test 1.2.2: Logical OR

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: BOOL;
  b: BOOL;
  result: BOOL;
END_VAR
BEGIN
  result := a OR b;
END_PROGRAM
END_UPLOAD

set logic 1 bind a coil:0 input
set logic 1 bind b coil:1 input
set logic 1 bind result coil:2 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: TRUE OR TRUE = TRUE
write coil 0 value 1
read coil 2
# Forventet: 1

# Test 2: TRUE OR FALSE = TRUE
write coil 0 value 1
read coil 2
# Forventet: 1

# Test 3: FALSE OR TRUE = TRUE
write coil 0 value 0
read coil 2
# Forventet: 1

# Test 4: FALSE OR FALSE = FALSE
write coil 0 value 0
read coil 2
# Forventet: 0
```

**Forventet Resultat:**
```
✅ TRUE OR TRUE = TRUE
✅ TRUE OR FALSE = TRUE
✅ FALSE OR TRUE = TRUE
✅ FALSE OR FALSE = FALSE
```

---

### Test 1.2.3: Logical NOT

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: BOOL;
  result: BOOL;
END_VAR
BEGIN
  result := NOT a;
END_PROGRAM
END_UPLOAD
set logic 1 bind a coil:0 input
set logic 1 bind result coil:1 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: NOT TRUE = FALSE
write coil 0 value 1
read coil 1
# Forventet: 0

# Test 2: NOT FALSE = TRUE
write coil 0 value 0
read coil 1
# Forventet: 1
```

**Forventet Resultat:**
```
✅ NOT TRUE = FALSE
✅ NOT FALSE = TRUE
```

---

### Test 1.2.4: Logical XOR

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: BOOL;
  b: BOOL;
  result: BOOL;
END_VAR
BEGIN
  result := a XOR b;
END_PROGRAM
END_UPLOAD
set logic 1 bind a coil:0 input
set logic 1 bind b coil:1 input
set logic 1 bind result coil:2 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: TRUE XOR TRUE = FALSE
write coil 0 value 1
read coil 2
# Forventet: 0

# Test 2: TRUE XOR FALSE = TRUE
write coil 0 value 1
read coil 2
# Forventet: 1

# Test 3: FALSE XOR TRUE = TRUE
write coil 0 value 0
read coil 2
# Forventet: 1

# Test 4: FALSE XOR FALSE = FALSE
write coil 0 value 0
read coil 2
# Forventet: 0
```

**Forventet Resultat:**
```
✅ TRUE XOR TRUE = FALSE
✅ TRUE XOR FALSE = TRUE
✅ FALSE XOR TRUE = TRUE
✅ FALSE XOR FALSE = FALSE
```

---

## 1.3 Bit-Shift Operatorer

### Test 1.3.1: Shift Left (SHL)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  value: INT;
  shift: INT;
  result: INT;
END_VAR
BEGIN
  result := value SHL shift;
END_PROGRAM
END_UPLOAD
set logic 1 bind value reg:20 input
set logic 1 bind shift reg:21 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: 1 SHL 3 = 8
write reg 20 value int 1
write reg 21 value int 3
read reg 22 int
# Forventet: 8

# Test 2: 5 SHL 2 = 20
write reg 20 value int 5
write reg 21 value int 2
read reg 22 int
# Forventet: 20

# Test 3: 1 SHL 0 = 1
write reg 20 value int 1
write reg 21 value int 0
read reg 22 int
# Forventet: 1
```

**Forventet Resultat:**
```
✅ 1 SHL 3 = 8
✅ 5 SHL 2 = 20
✅ 1 SHL 0 = 1
```

---

### Test 1.3.2: Shift Right (SHR)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  value: INT;
  shift: INT;
  result: INT;
END_VAR
BEGIN
  result := value SHR shift;
END_PROGRAM
END_UPLOAD
set logic 1 bind value reg:20 input
set logic 1 bind shift reg:21 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: 8 SHR 3 = 1
write reg 20 value int 8
write reg 21 value int 3
read reg 22 int
# Forventet: 1

# Test 2: 20 SHR 2 = 5
write reg 20 value int 20
write reg 21 value int 2
read reg 22 int
# Forventet: 5

# Test 3: 1 SHR 0 = 1
write reg 20 value int 1
write reg 21 value int 0
read reg 22 int
# Forventet: 1
```

**Forventet Resultat:**
```
✅ 8 SHR 3 = 1
✅ 20 SHR 2 = 5
✅ 1 SHR 0 = 1
```

---

## 1.4 Sammenlignings Operatorer

### Test 1.4.1: Equal (=)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: BOOL;
END_VAR
BEGIN
  result := (a = b);
END_PROGRAM
END_UPLOAD
set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result coil:0 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: 5 = 5 → TRUE
write reg 20 value int 5
write reg 21 value int 5
read coil 0
# Forventet: 1

# Test 2: 5 = 3 → FALSE
write reg 20 value int 5
write reg 21 value int 3
read coil 0
# Forventet: 0

# Test 3: 0 = 0 → TRUE
write reg 20 value int 0
write reg 21 value int 0
read coil 0
# Forventet: 1
```

**Forventet Resultat:**
```
✅ 5 = 5 → TRUE
✅ 5 = 3 → FALSE
✅ 0 = 0 → TRUE
```

---

### Test 1.4.2: Not Equal (<>)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: BOOL;
END_VAR
BEGIN
  result := (a <> b);
END_PROGRAM
END_UPLOAD
set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result coil:0 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: 5 <> 3 → TRUE
write reg 20 value int 5
write reg 21 value int 3
read coil 0
# Forventet: 1

# Test 2: 5 <> 5 → FALSE
write reg 20 value int 5
write reg 21 value int 5
read coil 0
# Forventet: 0
```

**Forventet Resultat:**
```
✅ 5 <> 3 → TRUE
✅ 5 <> 5 → FALSE
```

---

### Test 1.4.3: Less Than (<)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: BOOL;
END_VAR
BEGIN
  result := (a < b);
END_PROGRAM
END_UPLOAD
set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result coil:0 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: 3 < 5 → TRUE
write reg 20 value int 3
write reg 21 value int 5
read coil 0
# Forventet: 1

# Test 2: 5 < 3 → FALSE
write reg 20 value int 5
write reg 21 value int 3
read coil 0
# Forventet: 0

# Test 3: 5 < 5 → FALSE
write reg 20 value int 5
write reg 21 value int 5
read coil 0
# Forventet: 0
```

**Forventet Resultat:**
```
✅ 3 < 5 → TRUE
✅ 5 < 3 → FALSE
✅ 5 < 5 → FALSE
```

---

### Test 1.4.4: Greater Than (>)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: BOOL;
END_VAR
BEGIN
  result := (a > b);
END_PROGRAM
END_UPLOAD
set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result coil:0 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: 5 > 3 → TRUE
write reg 20 value int 5
write reg 21 value int 3
read coil 0
# Forventet: 1

# Test 2: 3 > 5 → FALSE
write reg 20 value int 3
write reg 21 value int 5
read coil 0
# Forventet: 0

# Test 3: 5 > 5 → FALSE
write reg 20 value int 5
write reg 21 value int 5
read coil 0
# Forventet: 0
```

**Forventet Resultat:**
```
✅ 5 > 3 → TRUE
✅ 3 > 5 → FALSE
✅ 5 > 5 → FALSE
```

---

### Test 1.4.5: Less or Equal (<=)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: BOOL;
END_VAR
BEGIN
  result := (a <= b);
END_PROGRAM
END_UPLOAD
set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result coil:0 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: 3 <= 5 → TRUE
write reg 20 value int 3
write reg 21 value int 5
read coil 0
# Forventet: 1

# Test 2: 5 <= 5 → TRUE
write reg 20 value int 5
write reg 21 value int 5
read coil 0
# Forventet: 1

# Test 3: 5 <= 3 → FALSE
write reg 20 value int 5
write reg 21 value int 3
read coil 0
# Forventet: 0
```

**Forventet Resultat:**
```
✅ 3 <= 5 → TRUE
✅ 5 <= 5 → TRUE
✅ 5 <= 3 → FALSE
```

---

### Test 1.4.6: Greater or Equal (>=)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: BOOL;
END_VAR
BEGIN
  result := (a >= b);
END_PROGRAM
END_UPLOAD
set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result coil:0 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: 5 >= 3 → TRUE
write reg 20 value int 5
write reg 21 value int 3
read coil 0
# Forventet: 1

# Test 2: 5 >= 5 → TRUE
write reg 20 value int 5
write reg 21 value int 5
read coil 0
# Forventet: 1

# Test 3: 3 >= 5 → FALSE
write reg 20 value int 3
write reg 21 value int 5
read coil 0
# Forventet: 0
```

**Forventet Resultat:**
```
✅ 5 >= 3 → TRUE
✅ 5 >= 5 → TRUE
✅ 3 >= 5 → FALSE
```

---

## 1.5 Builtin Funktioner - Matematiske

### Test 1.5.1: ABS (Absolut Værdi)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  input: INT;
  output: INT;
END_VAR
BEGIN
  output := ABS(input);
END_PROGRAM
END_UPLOAD
set logic 1 bind input reg:20 input
set logic 1 bind output reg:21 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: ABS(-5) = 5
write reg 20 value int -5
read reg 21 int
# Forventet: 5

# Test 2: ABS(10) = 10
write reg 20 value int 10
read reg 21 int
# Forventet: 10

# Test 3: ABS(0) = 0
write reg 20 value int 0
read reg 21 int
# Forventet: 0
```

**Forventet Resultat:**
```
✅ ABS(-5) = 5
✅ ABS(10) = 10
✅ ABS(0) = 0
```

---

### Test 1.5.2: MIN (Minimum)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: INT;
END_VAR
BEGIN
  result := MIN(a, b);
END_PROGRAM
END_UPLOAD
set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: MIN(5, 3) = 3
write reg 20 value int 5
write reg 21 value int 3
read reg 22 int
# Forventet: 3

# Test 2: MIN(-10, 5) = -10
write reg 20 value int -10
write reg 21 value int 5
read reg 22 int
# Forventet: -10

# Test 3: MIN(7, 7) = 7
write reg 20 value int 7
write reg 21 value int 7
read reg 22 int
# Forventet: 7
```

**Forventet Resultat:**
```
✅ MIN(5, 3) = 3
✅ MIN(-10, 5) = -10
✅ MIN(7, 7) = 7
```

---

### Test 1.5.3: MAX (Maximum)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: INT;
END_VAR
BEGIN
  result := MAX(a, b);
END_PROGRAM
END_UPLOAD
set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: MAX(5, 3) = 5
write reg 20 value int 5
write reg 21 value int 3
read reg 22 int
# Forventet: 5

# Test 2: MAX(-10, 5) = 5
write reg 20 value int -10
write reg 21 value int 5
read reg 22 int
# Forventet: 5

# Test 3: MAX(7, 7) = 7
write reg 20 value int 7
write reg 21 value int 7
read reg 22 int
# Forventet: 7
```

**Forventet Resultat:**
```
✅ MAX(5, 3) = 5
✅ MAX(-10, 5) = 5
✅ MAX(7, 7) = 7
```

---

### Test 1.5.4: SQRT (Kvadratrod)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  input: REAL;
  output: REAL;
END_VAR
BEGIN
  output := SQRT(input);
END_PROGRAM
END_UPLOAD
set logic 1 bind input reg:20 input
set logic 1 bind output reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: SQRT(16.0) = 4.0
write reg 20 value real 16.0
read reg 22 real
# Forventet: 4.000000 (0x40800000)

# Test 2: SQRT(2.0) ≈ 1.414
write reg 20 value real 2.0
read reg 22 real
# Forventet: 1.414214 (0x3FB504F3)

# Test 3: SQRT(9.0) = 3.0
write reg 20 value real 9.0
read reg 22 real
# Forventet: 3.000000 (0x40400000)

# Test 4: SQRT(0.0) = 0.0
write reg 20 value real 0.0
read reg 22 real
# Forventet: 0.000000 (0x00000000)
```

**Forventet Resultat:**
```
✅ SQRT(16.0) = 4.000000
✅ SQRT(2.0) ≈ 1.414214
✅ SQRT(9.0) = 3.000000
✅ SQRT(0.0) = 0.000000
```

---

### Test 1.5.5: ROUND (Afrunding)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  input: REAL;
  output: INT;
END_VAR
BEGIN
  output := ROUND(input);
END_PROGRAM
END_UPLOAD
set logic 1 bind input reg:20 input
set logic 1 bind output reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: ROUND(3.4) = 3
write reg 20 value real 3.4
read reg 22 int
# Forventet: 3

# Test 2: ROUND(3.6) = 4
write reg 20 value real 3.6
read reg 22 int
# Forventet: 4

# Test 3: ROUND(3.5) = 4 (banker's rounding eller 3?)
write reg 20 value real 3.5
read reg 22 int
# Forventet: 4 (or 3 depending on implementation)

# Test 4: ROUND(-2.7) = -3
write reg 20 value real -2.7
read reg 22 int
# Forventet: -3
```

**Forventet Resultat:**
```
✅ ROUND(3.4) = 3
✅ ROUND(3.6) = 4
⚠️ ROUND(3.5) = ? (verify implementation)
✅ ROUND(-2.7) = -3
```

---

### Test 1.5.6: TRUNC (Afkort)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  input: REAL;
  output: INT;
END_VAR
BEGIN
  output := TRUNC(input);
END_PROGRAM
END_UPLOAD
set logic 1 bind input reg:20 input
set logic 1 bind output reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: TRUNC(3.9) = 3
write reg 20 value real 3.9
read reg 22 int
# Forventet: 3

# Test 2: TRUNC(-3.9) = -3
write reg 20 value real -3.9
read reg 22 int
# Forventet: -3

# Test 3: TRUNC(5.1) = 5
write reg 20 value real 5.1
read reg 22 int
# Forventet: 5
```

**Forventet Resultat:**
```
✅ TRUNC(3.9) = 3
✅ TRUNC(-3.9) = -3
✅ TRUNC(5.1) = 5
```

---

### Test 1.5.7: FLOOR (Gulv)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  input: REAL;
  output: INT;
END_VAR
BEGIN
  output := FLOOR(input);
END_PROGRAM
END_UPLOAD
set logic 1 bind input reg:20 input
set logic 1 bind output reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: FLOOR(3.9) = 3
write reg 20 value real 3.9
read reg 22 int
# Forventet: 3

# Test 2: FLOOR(-3.1) = -4
write reg 20 value real -3.1
read reg 22 int
# Forventet: -4

# Test 3: FLOOR(5.0) = 5
write reg 20 value real 5.0
read reg 22 int
# Forventet: 5
```

**Forventet Resultat:**
```
✅ FLOOR(3.9) = 3
✅ FLOOR(-3.1) = -4
✅ FLOOR(5.0) = 5
```

---

### Test 1.5.8: CEIL (Loft)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  input: REAL;
  output: INT;
END_VAR
BEGIN
  output := CEIL(input);
END_PROGRAM
END_UPLOAD
set logic 1 bind input reg:20 input
set logic 1 bind output reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: CEIL(3.1) = 4
write reg 20 value real 3.1
read reg 22 int
# Forventet: 4

# Test 2: CEIL(-3.9) = -3
write reg 20 value real -3.9
read reg 22 int
# Forventet: -3

# Test 3: CEIL(5.0) = 5
write reg 20 value real 5.0
read reg 22 int
# Forventet: 5
```

**Forventet Resultat:**
```
✅ CEIL(3.1) = 4
✅ CEIL(-3.9) = -3
✅ CEIL(5.0) = 5
```

---

### Test 1.5.9: SUM (Sum)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: INT;
END_VAR
BEGIN
  result := SUM(a, b);
END_PROGRAM
END_UPLOAD
set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: SUM(5, 3) = 8
write reg 20 value int 5
write reg 21 value int 3
read reg 22 int
# Forventet: 8

# Test 2: SUM(-10, 15) = 5
write reg 20 value int -10
write reg 21 value int 15
read reg 22 int
# Forventet: 5
```

**Forventet Resultat:**
```
✅ SUM(5, 3) = 8
✅ SUM(-10, 15) = 5
```

---

## 1.6 Builtin Funktioner - Clamping/Selection

### Test 1.6.1: LIMIT (Begrænsning)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  min_val: INT;
  value: INT;
  max_val: INT;
  result: INT;
END_VAR
BEGIN
  result := LIMIT(min_val, value, max_val);
END_PROGRAM
END_UPLOAD
set logic 1 bind min_val reg:20 input
set logic 1 bind value reg:21 input
set logic 1 bind max_val reg:22 input
set logic 1 bind result reg:23 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: LIMIT(0, -10, 100) = 0 (clamped to min)
write reg 20 value int 0
write reg 21 value int -10
write reg 22 value int 100
read reg 23 int
# Forventet: 0

# Test 2: LIMIT(0, 150, 100) = 100 (clamped to max)
write reg 20 value int 0
write reg 21 value int 150
write reg 22 value int 100
read reg 23 int
# Forventet: 100

# Test 3: LIMIT(0, 50, 100) = 50 (no clamping)
write reg 20 value int 0
write reg 21 value int 50
write reg 22 value int 100
read reg 23 int
# Forventet: 50
```

**Forventet Resultat:**
```
✅ LIMIT(0, -10, 100) = 0
✅ LIMIT(0, 150, 100) = 100
✅ LIMIT(0, 50, 100) = 50
```

---

### Test 1.6.2: SEL (Selector)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  selector: BOOL;
  value_false: INT;
  value_true: INT;
  result: INT;
END_VAR
BEGIN
  result := SEL(selector, value_false, value_true);
END_PROGRAM
END_UPLOAD
set logic 1 bind selector coil:0 input
set logic 1 bind value_false reg:20 input
set logic 1 bind value_true reg:21 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
write reg 20 value int 50
write reg 21 value int 75

# Test 1: SEL(FALSE, 50, 75) = 50
write coil 0 value 0
read reg 22 int
# Forventet: 50

# Test 2: SEL(TRUE, 50, 75) = 75
write coil 0 value 1
read reg 22 int
# Forventet: 75
```

**Forventet Resultat:**
```
✅ SEL(FALSE, 50, 75) = 50
✅ SEL(TRUE, 50, 75) = 75
```

---

## 1.7 Builtin Funktioner - Trigonometriske

### Test 1.7.1: SIN (Sinus)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  angle: REAL;
  result: REAL;
END_VAR
BEGIN
  result := SIN(angle);
END_PROGRAM
END_UPLOAD
set logic 1 bind angle reg:20 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: SIN(0) = 0.0
write reg 20 value real 0.0
read reg 22 real
# Forventet: 0.000000 (0x00000000)

# Test 2: SIN(π/2) ≈ 1.0
write reg 20 value real 1.5708
read reg 22 real
# Forventet: 1.000000 (0x3F800000)

# Test 3: SIN(π) ≈ 0.0
write reg 20 value real 3.1416
read reg 22 real
# Forventet: ~0.000000 (meget tæt på 0)

# Test 4: SIN(π/6) = 0.5
write reg 20 value real 0.5236
read reg 22 real
# Forventet: 0.500000 (0x3F000000)
```

**Forventet Resultat:**
```
✅ SIN(0) = 0.000000
✅ SIN(π/2) ≈ 1.000000
✅ SIN(π) ≈ 0.000000
✅ SIN(π/6) ≈ 0.500000
```

---

### Test 1.7.2: COS (Cosinus)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  angle: REAL;
  result: REAL;
END_VAR
BEGIN
  result := COS(angle);
END_PROGRAM
END_UPLOAD
set logic 1 bind angle reg:20 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: COS(0) = 1.0
write reg 20 value real 0.0
read reg 22 real
# Forventet: 1.000000 (0x3F800000)

# Test 2: COS(π/2) ≈ 0.0
write reg 20 value real 1.5708
read reg 22 real
# Forventet: ~0.000000 (meget tæt på 0)

# Test 3: COS(π) ≈ -1.0
write reg 20 value real 3.1416
read reg 22 real
# Forventet: -1.000000 (0xBF800000)
```

**Forventet Resultat:**
```
✅ COS(0) = 1.000000
✅ COS(π/2) ≈ 0.000000
✅ COS(π) ≈ -1.0
```

---

### Test 1.7.3: TAN (Tangens)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  angle: REAL;
  result: REAL;
END_VAR
BEGIN
  result := TAN(angle);
END_PROGRAM
END_UPLOAD
set logic 1 bind angle reg:20 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: TAN(0) = 0.0
write reg 20 value real 0.0
read reg 22 real
# Forventet: 0.000000 (0x00000000)

# Test 2: TAN(π/4) ≈ 1.0
write reg 20 value real 0.7854
read reg 22 real
# Forventet: 1.000000 (0x3F800000)
```

**Forventet Resultat:**
```
✅ TAN(0) = 0.000000
✅ TAN(π/4) ≈ 1.000000
```

---

## 1.8 Builtin Funktioner - Type Conversion

### Test 1.8.1: INT_TO_REAL

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  int_val: INT;
  real_val: REAL;
END_VAR
BEGIN
  real_val := INT_TO_REAL(int_val);
END_PROGRAM
END_UPLOAD
set logic 1 bind int_val reg:20 input
set logic 1 bind real_val reg:21 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: INT_TO_REAL(10) = 10.0
write reg 20 value int 10
read reg 21 real
# Forventet: 10.000000 (0x41200000)

# Test 2: INT_TO_REAL(-5) = -5.0
write reg 20 value int -5
read reg 21 real
# Forventet: -5.000000 (0xC0A00000)
```

**Forventet Resultat:**
```
✅ INT_TO_REAL(10) = 10.0
✅ INT_TO_REAL(-5) = -5.0
```

---

### Test 1.8.2: REAL_TO_INT

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  real_val: REAL;
  int_val: INT;
END_VAR
BEGIN
  int_val := REAL_TO_INT(real_val);
END_PROGRAM
END_UPLOAD
set logic 1 bind real_val reg:20 input
set logic 1 bind int_val reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: REAL_TO_INT(10.7) = 10
write reg 20 value real 10.7
read reg 22 int
# Forventet: 10

# Test 2: REAL_TO_INT(-5.3) = -5
write reg 20 value real -5.3
read reg 22 int
# Forventet: -5
```

**Forventet Resultat:**
```
✅ REAL_TO_INT(10.7) = 10
✅ REAL_TO_INT(-5.3) = -5
```

---

### Test 1.8.3: BOOL_TO_INT

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  bool_val: BOOL;
  int_val: INT;
END_VAR
BEGIN
  int_val := BOOL_TO_INT(bool_val);
END_PROGRAM
END_UPLOAD
set logic 1 bind bool_val coil:0 input
set logic 1 bind int_val reg:20 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: BOOL_TO_INT(TRUE) = 1
write coil 0 value 1
read reg 20 int
# Forventet: 1

# Test 2: BOOL_TO_INT(FALSE) = 0
write coil 0 value 0
read reg 20 int
# Forventet: 0
```

**Forventet Resultat:**
```
✅ BOOL_TO_INT(TRUE) = 1
✅ BOOL_TO_INT(FALSE) = 0
```

---

### Test 1.8.4: INT_TO_BOOL

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  int_val: INT;
  bool_val: BOOL;
END_VAR
BEGIN
  bool_val := INT_TO_BOOL(int_val);
END_PROGRAM
END_UPLOAD
set logic 1 bind int_val reg:20 input
set logic 1 bind bool_val coil:0 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: INT_TO_BOOL(1) = TRUE
write reg 20 value int 1
read coil 0
# Forventet: 1

# Test 2: INT_TO_BOOL(0) = FALSE
write reg 20 value int 0
read coil 0
# Forventet: 0

# Test 3: INT_TO_BOOL(42) = TRUE (non-zero)
write reg 20 value int 42
read coil 0
# Forventet: 1
```

**Forventet Resultat:**
```
✅ INT_TO_BOOL(1) = TRUE
✅ INT_TO_BOOL(0) = FALSE
✅ INT_TO_BOOL(42) = TRUE
```

---

### Test 1.8.5: DWORD_TO_INT

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  dword_val: DWORD;
  int_val: INT;
END_VAR
BEGIN
  int_val := DWORD_TO_INT(dword_val);
END_PROGRAM
END_UPLOAD
set logic 1 bind dword_val reg:20 output
set logic 1 bind int_val reg:24 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: DWORD_TO_INT(1000) = 1000
write HR20-103 = 1000 (DWORD format)
read reg 24 uint
# Forventet: 1000
```

**Forventet Resultat:**
```
✅ DWORD_TO_INT(1000) = 1000
```

---

### Test 1.8.6: INT_TO_DWORD

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  int_val: INT;
  dword_val: DWORD;
END_VAR
BEGIN
  dword_val := INT_TO_DWORD(int_val);
END_PROGRAM
END_UPLOAD
set logic 1 bind int_val reg:20 input
set logic 1 bind dword_val reg:21 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: INT_TO_DWORD(1000) = 1000
write reg 20 value int 1000
read HR21-104 → Forventet: 1000 (DWORD format)
```

**Forventet Resultat:**
```
✅ INT_TO_DWORD(1000) = 1000
```

---

## 1.9 Builtin Funktioner - Persistence

**VIGTIGE NOTER:**
- SAVE() og LOAD() kræver 1 argument: `group_id` (0=alle grupper, 1-8=specifik gruppe)
- Return type: INT (ikke BOOL!)
  - `0` = Success
  - `-1` = Error (gruppe ikke fundet, NVS fejl, persistence disabled)
  - `-2` = Rate limited (max 1 SAVE per 5 sekunder)
- Der skal være mindst én persist gruppe defineret
- Persistence system skal være enabled

---

### Test 1.9.1: SAVE (Gem til NVS)

**Prerequisites (kør disse først):**
```bash
# Step 1: Opret en persist gruppe (hvis ikke allerede gjort)
set persist group test_gruppe add 100-105

# Step 2: Skriv nogle testværdier til gruppen
write reg 20 value uint 1234
write reg 21 value uint 5678
write reg 22 value uint 9999

# Step 3: Verificer at gruppen er oprettet
show persist
# Forventet: test_gruppe med start=100, count=6
```

**CLI Kommandoer (Copy/Paste):**
```bash
  set logic 1 delete
  set logic 1 upload
  PROGRAM test
  VAR
    trigger: BOOL;
    trigger_prev: BOOL;
    save_result: INT;
  END_VAR
  BEGIN
    IF trigger AND NOT trigger_prev THEN
      (* Rising edge - kun kald SAVE én gang *)
      save_result := SAVE(0);
    END_IF;
    trigger_prev := trigger;
  END_PROGRAM
  END_UPLOAD
  set logic 1 bind trigger coil:0 input
  set logic 1 bind save_result reg:40 output
  set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: Første SAVE (skal succeed)
write coil 0 value 1
read reg 40 int
# Forventet: 0 (success)
# Console output: "✓ SAVE(0) completed: 1 groups saved to NVS"

# Test 2: Nulstil trigger
write coil 0 value 0
read reg 40 int
# Forventet: stadig 0 (sidste result)

# Test 3: SAVE igen indenfor 5 sekunder (rate limited)
write coil 0 value 1
read reg 40 int
# Forventet: -2 (rate limited)
# Console output: "SAVE(0) rate limited (wait 5s between saves)"

# Test 4: Vent 5+ sekunder og prøv igen
# Wait 6 seconds...
write coil 0 value 0
write coil 0 value 1
read reg 40 int
# Forventet: 0 (success igen)
```

**Forventet Console Output (success):**
```
SAVE(0): Snapshotting register groups...
SAVE(0): Writing to NVS...
✓ SAVE(0) completed: 1 groups saved to NVS
```

**Forventet Console Output (rate limited):**
```
SAVE(0) rate limited (wait 5s between saves)
```

**Verificer at data er gemt:**
```bash
# Reboot ESP32 eller kør LOAD test for at verificere
```

---

### Test 1.9.2: LOAD (Læs fra NVS)

**Prerequisites (kør disse først):**
```bash
# Step 1: Gem data først (via SAVE test ovenfor eller CLI)
set persist save-group test_gruppe

# Step 2: Modificer register værdier (så vi kan se LOAD virker)
write reg 20 value uint 0
write reg 21 value uint 0
write reg 22 value uint 0

# Step 3: Verificer at værdier er ændret
read reg 20 3
# Forventet: alle 0
```

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  trigger: BOOL;
  load_result: INT;  (* Vigtigt: INT, ikke BOOL! *)
END_VAR
BEGIN
  IF trigger THEN
    load_result := LOAD(0);  (* 0 = load alle grupper *)
  END_IF;
END_PROGRAM
END_UPLOAD
set logic 1 bind trigger coil:0 input
set logic 1 bind load_result reg:40 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: Verificer at registers er reset (fra prerequisites)
read reg 20 3
# Forventet: 0, 0, 0

# Test 2: Trigger LOAD operation
write coil 0 value 1
read reg 40 int
# Forventet: 0 (success)
# Console output: "✓ LOAD(0) completed: 1 groups restored from NVS"

# Test 3: Verificer at registers er restored
read reg 20 3
# Forventet: 1234, 5678, 9999 (oprindelige værdier fra SAVE test)

# Test 4: LOAD uden saved data (error)
set persist delete-group test_gruppe
write coil 0 value 0
write coil 0 value 1
read reg 40 int
# Forventet: -1 (error - gruppe ikke fundet)
```

**Forventet Console Output (success):**
```
LOAD(0): Reading from NVS...
LOAD(0): Restoring register values...
✓ LOAD(0) completed: 1 groups restored from NVS
```

**Forventet Console Output (error):**
```
LOAD(0): Reading from NVS...
LOAD(0): Restoring register values...
LOAD(0) failed: Could not restore group
```

---

### Test 1.9.3: SAVE/LOAD Integration (Complete Workflow)

**Complete test scenario: Gem counter værdi ved shutdown, restore ved boot**

**Step 1: Setup persist gruppe:**
```bash
# Opret gruppe til counter data
set persist group counter_backup add 200-202
```

**Step 2: Upload ST program:**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM counter_backup
VAR
  counter: INT;
  save_trigger: BOOL;
  load_trigger: BOOL;
  save_status: INT;
  load_status: INT;
END_VAR
BEGIN
  (* Increment counter *)
  counter := counter + 1;

  (* Save when triggered *)
  IF save_trigger THEN
    save_status := SAVE(0);
  END_IF;

  (* Load when triggered *)
  IF load_trigger THEN
    load_status := LOAD(0);
  END_IF;
END_PROGRAM
END_UPLOAD

set logic 1 bind counter reg:60 output
set logic 1 bind save_trigger coil:10 input
set logic 1 bind load_trigger coil:11 input
set logic 1 bind save_status reg:61 output
set logic 1 bind load_status reg:62 output
set logic 1 enabled:true
```

**Step 3: Test workflow:**
```bash
# 1. Lad counter køre i 2 sekunder (~200 cycles @ 10ms interval)
# Wait 2 seconds...
read reg 60 int
# Forventet: ~200

# 2. Gem counter værdi
write coil 10 value 1
read reg 61 int
# Forventet: 0 (success)

# 3. Nulstil trigger
write coil 10 value 0

# 4. Lad counter fortsætte
# Wait 2 seconds...
read reg 60 int
# Forventet: ~400

# 5. Reset counter til 0 (simulér restart)
set logic 1 enabled:false
write reg 60 value int 0
set logic 1 enabled:true

# 6. Verificer reset
read reg 60 int
# Forventet: ~1-10 (lige startet)

# 7. Load saved værdi
write coil 11 value 1
read reg 62 int
# Forventet: 0 (success)

# 8. Verificer at counter er restored
read reg 60 int
# Forventet: ~200 (originale gemte værdi + nye cycles)
```

**Forventet Resultat:**
```
✅ Counter værdi gemt korrekt
✅ Counter værdi restored efter "reboot"
✅ SAVE/LOAD workflow fungerer end-to-end
```

---

## 1.10 GPIO & Hardware Tests

**Hardware Required:** LEDs på GPIO17-21, switches på GPIO32-34, 1kHz signal på GPIO25

### Test 1.10.1: LED Control via GPIO Output (Coils)

**Hardware:** LED på GPIO17

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  led_state: BOOL;
  counter: INT;
END_VAR
BEGIN
  counter := counter + 1;
  IF counter >= 50 THEN led_state := NOT led_state;
  counter := 0;
  END_IF;
END_PROGRAM
END_UPLOAD
set logic 1 bind led_state coil:17 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: Verify LED blinks
# Expected: LED på GPIO17 blinker med ~1Hz (on 500ms, off 500ms)
# Visual verification: LED skal blinke synligt
```

**Forventet Resultat:**
```
✅ LED blinker med ~1Hz
✅ ST Logic kan styre GPIO output
✅ BOOL variable mapped til coil fungerer
```

---

### Test 1.10.2: Switch Input via GPIO Discrete Input

**Hardware:** Switch på GPIO32

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  switch_input: BOOL;
  led_output: BOOL;
  count: INT;
END_VAR
BEGIN
  led_output := switch_input;
  IF switch_input THEN count := count + 1;
  END_IF;
END_PROGRAM
END_UPLOAD
set logic 1 bind switch_input discrete:32
set logic 1 bind led_output coil:17 output
set logic 1 bind count reg:20 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: Press switch på GPIO32
# Expected: LED på GPIO17 tænder når switch er pressed
#           HR20 incrementerer (kan være meget hurtigt pga. 10ms loop)

# Test 2: Release switch
# Expected: LED slukker
```

**Forventet Resultat:**
```
✅ Switch input læses korrekt
✅ LED følger switch state
✅ Discrete input binding fungerer
⚠️ Count kan være høj pga. 10ms sampling (ikke debounced)
```

---

### Test 1.10.3: Multiple LED Sequence (Chaser Pattern)

**Hardware:** LEDs på GPIO17, 18, 19, 21

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  led1: BOOL;
  led2: BOOL;
  led3: BOOL;
  led4: BOOL;
  state: INT;
  counter: INT;
END_VAR
BEGIN
  counter := counter + 1;
  IF counter >= 30 THEN counter := 0;
  state := state + 1;
  IF state > 3 THEN state := 0;
  END_IF;
  END_IF;
  led1 := (state = 0);
  led2 := (state = 1);
  led3 := (state = 2);
  led4 := (state = 3);
END_PROGRAM
END_UPLOAD
set logic 1 bind led1 coil:17 output
set logic 1 bind led2 coil:18 output
set logic 1 bind led3 coil:19 output
set logic 1 bind led4 coil:21 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: Visual verification
# Expected: LEDs lyser sekventielt:
#   LED1 (GPIO17) → LED2 (GPIO18) → LED3 (GPIO19) → LED4 (GPIO21) → repeat
# Hver LED er tændt i ~300ms
```

**Forventet Resultat:**
```
✅ LED chaser pattern fungerer
✅ Multiple coil outputs kan styres
✅ Sequential control logic fungerer
✅ Timing er korrekt (~300ms per step)
```

---

### Test 1.10.4: Frequency Counter med 1kHz Signal

**Hardware:** 1kHz signal på GPIO25

**Note:** Denne test kræver integration med counter engine, ikke kun ST Logic.

**Setup:**
```bash
# Configure counter 1 for GPIO25 (hardware pulse counter)
set counter 1 mode:hw pin:25 scale:1 prescale:1 enabled:true

# Counter 1 registers (default):
# HR20-101: Index (scaled value, 2 words for 32-bit)
# HR24-105: Raw (prescaled value)
# HR28: Frequency (Hz)
```

**Test Cases:**
```bash
# Manual test via CLI
show counter 1
# Expected: Frequency (HR28) reads ~1000 Hz

# Verify pulse counting
# HR24-105 (raw count) should increment steadily
# Rate: ~1000 counts per second
```

**Forventet Resultat:**
```
✅ Counter registrerer 1kHz signal
✅ Frequency measurement er ~1000 Hz (±5%)
✅ Raw count incrementerer korrekt
⚠️ Dette er primært en counter engine test, ikke ST Logic
```

---

## 1.11 Kontrolstrukturer - IF/THEN/ELSE

### Test 1.11.1: Simple IF/THEN

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  condition: BOOL;
  output: INT;
END_VAR
BEGIN
  IF condition THEN output := 100;
  END_IF;
END_PROGRAM
END_UPLOAD
set logic 1 bind condition coil:0 input
set logic 1 bind output reg:20 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: condition = TRUE
write coil 0 value 1
read reg 20 int
# Forventet: 100

# Test 2: condition = FALSE
write coil 0 value 0
read reg 20 int
# Forventet: 100
```

**Forventet Resultat:**
```
✅ IF TRUE THEN executes
✅ IF FALSE skips THEN block
```

---

### Test 1.11.2: IF/THEN/ELSE

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  condition: BOOL;
  output: INT;
END_VAR
BEGIN
  IF condition THEN output := 100;
  ELSE output := 200;
  END_IF;
END_PROGRAM
END_UPLOAD
set logic 1 bind condition coil:0 input
set logic 1 bind output reg:20 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: condition = TRUE
write coil 0 value 1
read reg 20 int
# Forventet: 100

# Test 2: condition = FALSE
write coil 0 value 0
read reg 20 int
# Forventet: 200
```

**Forventet Resultat:**
```
✅ IF TRUE → THEN branch
✅ IF FALSE → ELSE branch
```

---

### Test 1.11.3: IF/ELSIF/ELSE

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  value: INT;
  output: INT;
END_VAR
BEGIN
  IF value < 10 THEN output := 1;
  ELSIF value < 20 THEN output := 2;
  ELSE output := 3;
  END_IF;
END_PROGRAM
END_UPLOAD
set logic 1 bind value reg:20 input
set logic 1 bind output reg:21 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: value = 5 (< 10)
write reg 20 value int 5
read reg 21 int
# Forventet: 1

# Test 2: value = 15 (< 20 but not < 10)
write reg 20 value int 15
read reg 21 int
# Forventet: 2

# Test 3: value = 25 (>= 20)
write reg 20 value int 25
read reg 21 int
# Forventet: 3
```

**Forventet Resultat:**
```
✅ value < 10 → output = 1
✅ 10 <= value < 20 → output = 2
✅ value >= 20 → output = 3
```

---

## 1.12 Kontrolstrukturer - CASE

### Test 1.12.1: CASE Statement

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  selector: INT;
  output: INT;
END_VAR
BEGIN
  CASE selector OF 1: output := 100;
  2: output := 200;
  3: output := 300;
  ELSE output := 999;
  END_CASE;
END_PROGRAM
END_UPLOAD
set logic 1 bind selector reg:20 input
set logic 1 bind output reg:21 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: selector = 1
write reg 20 value int 1
read reg 21 int
# Forventet: 100

# Test 2: selector = 2
write reg 20 value int 2
read reg 21 int
# Forventet: 200

# Test 3: selector = 3
write reg 20 value int 3
read reg 21 int
# Forventet: 300

# Test 4: selector = 99 (no match)
write reg 20 value int 99
read reg 21 int
# Forventet: 999
```

**Forventet Resultat:**
```
✅ CASE 1 → output = 100
✅ CASE 2 → output = 200
✅ CASE 3 → output = 300
✅ CASE ELSE → output = 999
```

---

## 1.13 Kontrolstrukturer - FOR Loop

### Test 1.13.1: FOR Loop (Count to 10)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  i: INT;
  sum: INT;
END_VAR
BEGIN
  sum := 0;
  FOR i := 1 TO 10 DO sum := sum + i;
  END_FOR;
END_PROGRAM
END_UPLOAD
set logic 1 bind sum reg:20 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: Sum 1+2+3+...+10 = 55
read reg 20 int
# Forventet: 55
```

**Forventet Resultat:**
```
✅ FOR loop executes 10 times
✅ sum = 1+2+3+...+10 = 55
```

---

### Test 1.13.2: FOR Loop with BY step

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  i: INT;
  sum: INT;
END_VAR
BEGIN
  sum := 0;
  FOR i := 0 TO 10 BY 2 DO sum := sum + i;
  END_FOR;
END_PROGRAM
END_UPLOAD
set logic 1 bind sum reg:20 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: Sum 0+2+4+6+8+10 = 30
read reg 20 int
# Forventet: 30
```

**Forventet Resultat:**
```
✅ FOR loop with BY 2 step
✅ sum = 0+2+4+6+8+10 = 30
```

---

## 1.14 Kontrolstrukturer - WHILE Loop

### Test 1.14.1: WHILE Loop (Count to 10)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  counter: INT;
  sum: INT;
END_VAR
BEGIN
  counter := 1;
  sum := 0;
  WHILE counter <= 10 DO sum := sum + counter;
  counter := counter + 1;
  END_WHILE;
END_PROGRAM
END_UPLOAD
set logic 1 bind sum reg:20 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: Sum 1+2+3+...+10 = 55
read reg 20 int
# Forventet: 55
```

**Forventet Resultat:**
```
✅ WHILE loop executes while condition true
✅ sum = 1+2+3+...+10 = 55
```

---

## 1.15 Kontrolstrukturer - REPEAT Loop

### Test 1.15.1: REPEAT/UNTIL Loop

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  counter: INT;
  sum: INT;
END_VAR
BEGIN
  counter := 1;
  sum := 0;
  REPEAT sum := sum + counter;
  counter := counter + 1;
  UNTIL counter > 10;
END_PROGRAM
END_UPLOAD
set logic 1 bind sum reg:20 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: Sum 1+2+3+...+10 = 55
read reg 20 int
# Forventet: 55
```

**Forventet Resultat:**
```
✅ REPEAT executes at least once
✅ sum = 1+2+3+...+10 = 55
```

---

## 1.16 Type System - INT vs DINT

### Test 1.16.1: INT Overflow Behavior (16-bit)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: INT;
END_VAR
BEGIN
  result := a + b;
END_PROGRAM
END_UPLOAD

set logic 1 bind a reg:20 input
set logic 1 bind b reg:21 input
set logic 1 bind result reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: INT overflow (32767 + 1 = -32768)
write reg 20 value int 32767
write reg 21 value int 1
read reg 22 int
# Forventet: -32768

# Test 2: INT underflow (-32768 - 1 = 32767)
write reg 20 value int -32768
write reg 21 value int -1
read reg 22 int
# Forventet: 32767

# Test 3: Normal operation (100 + 200 = 300)
write reg 20 value int 100
write reg 21 value int 200
read reg 22 int
# Forventet: 300
```

**Forventet Resultat:**
```
✅ INT is 16-bit: 32767 + 1 = -32768 (wrapping overflow)
✅ INT underflow: -32768 - 1 = 32767
✅ Normal arithmetic within range works correctly
```

---

### Test 1.16.2: DINT Large Values (32-bit)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: DINT;
  b: DINT;
  result: DINT;
END_VAR
BEGIN
  result := a + b;
END_PROGRAM
END_UPLOAD

set logic 1 bind a reg:40 input
set logic 1 bind b reg:42 input
set logic 1 bind result reg:44 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: Large DINT values (100000 + 200000 = 300000)
write reg 40 value dint 100000
write reg 42 value dint 200000
# Læs resultat (2 registers)
read reg 44 2
# Forventet: HR44=4, HR45=37856 (LSW, MSW) = 300000

# Test 2: DINT negative values (-500000 + 100 = -499900)
write reg 40 value dint -500000
write reg 42 value dint 100
read reg 44 2
# Forventet: HR44=24416, HR45=65527 (LSW, MSW) = -499900

# Test 3: Simple DINT addition (1 + 3 = 4)
write reg 40 value dint 1
write reg 42 value dint 3
read reg 44 2
# Forventet: HR44=4, HR45=0 (LSW, MSW) = 4
```

**Forventet Resultat:**
```
✅ DINT uses 2 consecutive registers (LSW first, MSW second)
✅ DINT supports values beyond INT16 range (-32768 to 32767)
✅ Large positive values (100000 + 200000 = 300000)
✅ Negative DINT values work correctly (-500000 + 100 = -499900)
✅ Simple DINT addition works (1 + 3 = 4)
✅ CLI command 'write reg <addr> value dint <value>' automatically calculates LSW/MSW
```

---

### Test 1.16.3: INT → DINT Type Promotion

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  int_val: INT;
  dint_val: DINT;
  result: DINT;
END_VAR
BEGIN
  result := int_val + dint_val;
END_PROGRAM
END_UPLOAD

set logic 1 bind int_val reg:50 input
set logic 1 bind dint_val reg:52 input
set logic 1 bind result reg:54 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: INT (1000) + DINT (200000) = DINT (201000)
write reg 50 value int 1000
write reg 52 value dint 200000
read reg 54 2
# Forventet: HR54=4392, HR55=3 (LSW, MSW) = 201000

# Test 2: INT (-100) + DINT (50000) = DINT (49900)
write reg 50 value int -100
write reg 52 value dint 50000
read reg 54 2
# Forventet: HR54=49900, HR55=0 (LSW, MSW) = 49900
```

**Forventet Resultat:**
```
✅ INT automatically promoted to DINT when mixed with DINT
✅ Result uses DINT (32-bit) arithmetic
✅ Multi-register I/O works for DINT result (LSW first, MSW second)
✅ Positive INT + large DINT (1000 + 200000 = 201000)
✅ Negative INT + positive DINT (-100 + 50000 = 49900)
✅ CLI command 'write reg <addr> value dint <value>' simplifies testing
```

---

# Fase 2: Kombinerede Tests

## Test 2.1: Aritmetik + Logik + Sammenligning

**Use Case:** PID-lignende controller (forenklet)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  setpoint: INT;
  actual: INT;
  error: INT;
  kp: INT;
  output: INT;
  output_limited: INT;
END_VAR
BEGIN
  error := setpoint - actual;
  output := kp * error;
  output_limited := LIMIT(0, output, 100);
END_PROGRAM
END_UPLOAD
set logic 1 bind setpoint reg:20 input
set logic 1 bind actual reg:21 input
set logic 1 bind kp reg:22 input
set logic 1 bind output_limited reg:23 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
write reg 22 value int 2

# Test 1: setpoint=80, actual=70 → error=10, output=20
write reg 20 value int 80
write reg 21 value int 70
read reg 23 int
# Forventet: 20

# Test 2: setpoint=50, actual=60 → error=-10, output=-20 (limited to 0)
write reg 20 value int 50
write reg 21 value int 60
read reg 23 int
# Forventet: 0

# Test 3: setpoint=100, actual=50 → error=50, output=100
write reg 20 value int 100
write reg 21 value int 50
read reg 23 int
# Forventet: 100

# Test 4: setpoint=100, actual=0 → error=100, output=200 (limited to 100)
write reg 20 value int 100
write reg 21 value int 0
read reg 23 int
# Forventet: 100
```

**Forventet Resultat:**
```
✅ Kombineret aritmetik, multiplikation, og LIMIT
✅ Alle 4 test cases bestået
```

---

## Test 2.2: Nested IF + Multiple Conditions

**Use Case:** Multi-level alarm system

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  temperature: INT;
  pressure: INT;
  alarm_level: INT;
END_VAR
BEGIN
  alarm_level := 0;
  IF temperature > 100 THEN IF pressure > 50 THEN alarm_level := 3;
  ELSE alarm_level := 2;
  END_IF;
  ELSIF temperature > 80 THEN alarm_level := 1;
  ELSE alarm_level := 0;
  END_IF;
END_PROGRAM
END_UPLOAD
set logic 1 bind temperature reg:20 input
set logic 1 bind pressure reg:21 input
set logic 1 bind alarm_level reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: temp=70, press=30 → Normal
write reg 20 value int 70
write reg 21 value int 30
read reg 22 int
# Forventet: 0

# Test 2: temp=90, press=30 → Warning
write reg 20 value int 90
write reg 21 value int 30
read reg 22 int
# Forventet: 1

# Test 3: temp=110, press=40 → High temp
write reg 20 value int 110
write reg 21 value int 40
read reg 22 int
# Forventet: 2

# Test 4: temp=110, press=60 → Critical
write reg 20 value int 110
write reg 21 value int 60
read reg 22 int
# Forventet: 3
```

**Forventet Resultat:**
```
✅ Nested IF statements work correctly
✅ Alle alarm levels fungerer
```

---

## Test 2.3: Loops + Arrays (Simulation)

**Use Case:** Average calculation over time

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  value1: INT;
  value2: INT;
  value3: INT;
  value4: INT;
  sum: INT;
  average: INT;
END_VAR
BEGIN
  sum := value1 + value2 + value3 + value4;
  average := sum / 4;
END_PROGRAM
END_UPLOAD
set logic 1 bind value1 reg:20 input
set logic 1 bind value2 reg:21 input
set logic 1 bind value3 reg:22 input
set logic 1 bind value4 reg:23 input
set logic 1 bind average reg:24 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: Average of 10, 20, 30, 40 = 25
write reg 20 value int 10
write reg 21 value int 20
write reg 22 value int 30
write reg 23 value int 40
read reg 24 int
# Forventet: 25

# Test 2: Average of 0, 0, 0, 100 = 25
write reg 20 value int 0
write reg 21 value int 0
write reg 22 value int 0
write reg 23 value int 100
read reg 24 int
# Forventet: 25
```

**Forventet Resultat:**
```
✅ Multi-variable sum calculation
✅ Integer division average
```

---

## Test 2.4: CASE + Builtin Functions

**Use Case:** Mode-based calculation

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  mode: INT;
  input: INT;
  output: INT;
END_VAR
BEGIN
  CASE mode OF 1: output := ABS(input);
  2: output := input * 2;
  3: output := input / 2;
  4: output := MIN(input, 100);
  ELSE output := input;
  END_CASE;
END_PROGRAM
END_UPLOAD
set logic 1 bind mode reg:20 input
set logic 1 bind input reg:21 input
set logic 1 bind output reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
write reg 21 value int -50

# Test 1: mode=1 (ABS)
write reg 20 value int 1
read reg 22 int
# Forventet: 50

# Test 2: mode=2 (multiply by 2)
write reg 20 value int 2
read reg 22 int
# Forventet: -100

# Test 3: mode=3 (divide by 2)
write reg 20 value int 3
read reg 22 int
# Forventet: -25

# Test 4: mode=4 (MIN with 100)
write reg 20 value int 4
read reg 22 int
# Forventet: -50

# Test 5: mode=99 (ELSE)
write reg 20 value int 99
read reg 22 int
# Forventet: -50
```

**Forventet Resultat:**
```
✅ CASE kombineret med builtin functions
✅ Alle modes fungerer korrekt
```

---

## Test 2.5: FOR Loop + Conditional Logic

**Use Case:** Count values in range

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  start_count: BOOL;
  threshold: INT;
  i: INT;
  count: INT;
END_VAR
BEGIN
  IF start_count THEN count := 0;
  FOR i := 1 TO 100 DO IF i < threshold THEN count := count + 1;
  END_IF;
  END_FOR;
  END_IF;
END_PROGRAM
END_UPLOAD
set logic 1 bind start_count coil:0 input
set logic 1 bind threshold reg:20 input
set logic 1 bind count reg:21 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: threshold=50, count values 1-49 = 49
write reg 20 value int 50
write coil 0 value 1
# Wait 100ms
read reg 21 int
# Forventet: 49

# Test 2: threshold=10, count values 1-9 = 9
write reg 20 value int 10
write coil 0 value 0
# Wait 100ms
read reg 21 int
# Forventet: 9
```

**Forventet Resultat:**
```
✅ FOR loop + nested IF
✅ Conditional counting fungerer
```

---

## Test 2.6: WHILE Loop + Math Functions

**Use Case:** Binary search (simplified)

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  target: INT;
  low: INT;
  high: INT;
  mid: INT;
  iterations: INT;
END_VAR
BEGIN
  low := 0;
  high := 100;
  iterations := 0;
  WHILE (high - low) > 1 DO mid := (low + high) / 2;
  IF mid < target THEN low := mid;
  ELSE high := mid;
  END_IF;
  iterations := iterations + 1;
  END_WHILE;
END_PROGRAM
END_UPLOAD
set logic 1 bind target reg:20 input
set logic 1 bind iterations reg:21 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: target=50
write reg 20 value int 50
# Wait 100ms
read HR21 → Forventet: ~6-7 iterations
```

**Forventet Resultat:**
```
✅ WHILE loop + arithmetic
✅ Binary search completes
```

---

## Test 2.7: Trigonometry + REAL Math

**Use Case:** Vector magnitude calculation

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  x: REAL;
  y: REAL;
  magnitude: REAL;
  x_squared: REAL;
  y_squared: REAL;
  sum_squares: REAL;
END_VAR
BEGIN
  x_squared := x * x;
  y_squared := y * y;
  sum_squares := x_squared + y_squared;
  magnitude := SQRT(sum_squares);
END_PROGRAM
END_UPLOAD
set logic 1 bind x reg:20 input
set logic 1 bind y reg:22 input
set logic 1 bind magnitude reg:24 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: magnitude(3.0, 4.0) = 5.0
write reg 20 value real 3.0
write reg 22 value real 4.0
read reg 24 real
# Forventet: 5.000000 (0x40A00000)

# Test 2: magnitude(5.0, 12.0) = 13.0
write reg 20 value real 5.0
write reg 22 value real 12.0
read reg 24 real
# Forventet: 13.000000 (0x41500000)
```

**Forventet Resultat:**
```
✅ REAL arithmetic fungerer
✅ SQRT beregning korrekt
✅ Pythagorean theorem verified
```

---

## Test 2.8: Type Conversion Chain

**Use Case:** Multi-step conversion

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  int_input: INT;
  real_temp: REAL;
  scaled: REAL;
  int_output: INT;
END_VAR
BEGIN
  real_temp := INT_TO_REAL(int_input);
  scaled := real_temp * 1.5;
  int_output := REAL_TO_INT(scaled);
END_PROGRAM
END_UPLOAD
set logic 1 bind int_input reg:20 input
set logic 1 bind int_output reg:21 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Test 1: 10 * 1.5 = 15
write reg 20 value int 10
read reg 21 uint
# Forventet: 15

# Test 2: 7 * 1.5 = 10.5 → 10
write reg 20 value int 7
read reg 21 uint
# Forventet: 10

# Test 3: 100 * 1.5 = 150
write reg 20 value int 100
read reg 21 uint
# Forventet: 150
```

**Forventet Resultat:**
```
✅ INT → REAL conversion
✅ REAL arithmetic
✅ REAL → INT conversion
```

---

## Test 2.9: Boolean Logic + SEL

**Use Case:** Mode selection with validation

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  auto_mode: BOOL;
  manual_override: BOOL;
  auto_value: INT;
  manual_value: INT;
  final_output: INT;
  mode_active: BOOL;
END_VAR
BEGIN
  mode_active := auto_mode AND NOT manual_override;
  final_output := SEL(mode_active, manual_value, auto_value);
END_PROGRAM
END_UPLOAD
set logic 1 bind auto_mode coil:0 input
set logic 1 bind manual_override coil:1 input
set logic 1 bind auto_value reg:20 input
set logic 1 bind manual_value reg:21 input
set logic 1 bind final_output reg:22 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
write reg 20 value int 75
write reg 21 value int 50

# Test 1: auto=TRUE, override=FALSE → mode_active=TRUE → auto_value
write coil 0 value 1
read reg 22 int
# Forventet: 75

# Test 2: auto=TRUE, override=TRUE → mode_active=FALSE → manual_value
write coil 0 value 1
read reg 22 int
# Forventet: 50

# Test 3: auto=FALSE, override=FALSE → mode_active=FALSE → manual_value
write coil 0 value 0
read reg 22 int
# Forventet: 50
```

**Forventet Resultat:**
```
✅ Boolean logic kombineret med SEL
✅ Override funktionalitet fungerer
```

---

## Test 2.10: Complete System Test - Alle Features

**Use Case:** Mini production line controller

**CLI Kommandoer (Copy/Paste):**
```bash
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  start_button: BOOL;
  stop_button: BOOL;
  sensor_count: INT;
  temperature: REAL;
  motor_running: BOOL;
  alarm: BOOL;
  production_count: INT;
  state: INT;
  temp_ok: BOOL;
  count_ok: BOOL;
  temp_int: INT;
END_VAR
BEGIN
  temp_int := REAL_TO_INT(temperature);
  temp_ok := (temp_int >= 20) AND (temp_int <= 80);
  count_ok := sensor_count < 1000;
  CASE state OF 0: motor_running := FALSE;
  IF start_button AND temp_ok AND count_ok THEN state := 1;
  END_IF;
  1: motor_running := TRUE;
  production_count := production_count + 1;
  IF stop_button OR NOT temp_ok OR NOT count_ok THEN state := 2;
  END_IF;
  2: motor_running := FALSE;
  state := 0;
  ELSE state := 0;
  END_CASE;
  alarm := NOT temp_ok OR NOT count_ok;
END_PROGRAM
END_UPLOAD

set logic 1 bind start_button coil:0 input
set logic 1 bind stop_button coil:1 input
set logic 1 bind sensor_count reg:20 input
set logic 1 bind temperature reg:21 input
set logic 1 bind motor_running coil:10 output
set logic 1 bind alarm coil:11 output
set logic 1 bind production_count reg:40 output
set logic 1 enabled:true
```

**Test Cases:**
```bash
# Setup initial values
write reg 20 value int 50
write reg 21 value real 25.0

# Test 1: Start sequence
write coil 0 value 1
read coil 10
# Forventet: 1 (motor running)
read coil 11
# Forventet: 0 (no alarm)
read reg 40 int
# Forventet: > 0 (production counting)

# Test 2: Stop sequence
write coil 0 value 0
read coil 10
# Forventet: 0 (motor stopped)

# Test 3: Temperature alarm
write reg 21 value real 90.0
write coil 0 value 1
read coil 10
# Forventet: 0 (motor not starting)
read coil 11
# Forventet: 1 (alarm active)

# Test 4: Count alarm
write reg 21 value real 25.0
write reg 20 value int 1500
read coil 11
# Forventet: 1 (alarm active)
```

**Forventet Resultat:**
```
✅ State machine fungerer
✅ Type conversion (REAL_TO_INT)
✅ Boolean logic (AND, NOT)
✅ Comparison operators (>=, <=, <)
✅ CASE statement
✅ IF/THEN logic
✅ Arithmetic (increment)
✅ All outputs korrekte
```

---

# Test Execution Workflow

## Forberedelse

1. **Hardware Setup:**
   - ESP32 connected via USB
   - Serial terminal @ 115200 baud
   - (Optional) Modbus Master device på UART1 for MB_* tests

2. **Software Tools:**
   - PlatformIO CLI eller IDE
   - Serial terminal (PuTTY, screen, etc.)
   - Python 3 + pymodbus (for automated tests)

3. **Pre-Test Checklist:**
   ```bash
   # Verify build number
   show version
   # Expected: Build #787+

   # Clear all logic programs
   set logic 1 delete
   set logic 2 delete
   set logic 3 delete
   set logic 4 delete

   # Reset registers if needed
   # (Manual via Modbus or reboot device)
   ```

---

## Execution Steps (Fase 1)

### Quick Test (Single Function)

1. **Upload program** (copy/paste CLI command)
2. **Bind variables** (copy/paste bind commands)
3. **Enable program**
4. **Write test inputs** (via Modbus eller CLI)
5. **Read test outputs** (verify expected values)
6. **Mark result** (✅ eller ❌ i checklist)
7. **Cleanup** (delete program for next test)

### Example (Test 1.1.1: Addition):
```bash
# 1. Upload
set logic 1 delete
set logic 1 upload
PROGRAM test
VAR
  a: INT;
  b: INT;
  result: INT;
END_VAR
BEGIN
  result := a + b;
END_PROGRAM
END_UPLOAD

# 2. Bind
set logic 1 bind a reg:20 output
set logic 1 bind b reg:21 output
set logic 1 bind result reg:22 output

# 3. Enable
set logic 1 enabled:true

# 4. Test (via Modbus Python script)
client.write_register(100, 5)
client.write_register(101, 3)
time.sleep(0.1)
result = client.read_holding_registers(102, 1).registers[0]
assert result == 8, f"Expected 8, got {result}"
print("✅ Test 1.1.1 passed")

# 5. Cleanup
set logic 1 delete
```

---

## Execution Steps (Fase 2)

Fase 2 tests er mere komplekse og kræver:
- Længere ST-programmer
- Flere variable bindings
- Multiple test scenarios per program

Følg samme workflow som Fase 1, men forvent:
- Længere upload tid (større programmer)
- Mere tid til verify outputs
- Brug Python scripts for automation

---

# Fejlhåndtering

## Compilation Errors

**Symptom:** `set logic upload` returnerer fejl

**Actions:**
1. Kør `show logic 1` for at se compilation error details
2. Tjek syntax (mellemrum, semikolon, END_PROGRAM)
3. Verify variable types match operations
4. Tjek at builtin function names er korrekte (case-sensitive)

**Common Fixes:**
```bash
# Missing semicolon
❌ result := a + b END_PROGRAM
✅ result := a + b; END_PROGRAM

# Wrong type
❌ result: BOOL := 5 + 3;
✅ result: INT := 5 + 3;

# Unknown function
❌ output := ABSOLUTE(input);
✅ output := ABS(input);
```

---

## Runtime Errors

**Symptom:** Program compiles but outputs forkerte

**Actions:**
1. Verify variable bindings: `show logic 1`
2. Tjek Modbus register mapping
3. Verify input values er skrevet korrekt
4. Tjek for timing issues (wait mellem write/read)

**Common Fixes:**
```bash
# Wrong binding direction
❌ set logic 1 bind output reg:20  (default: input)
✅ set logic 1 bind output reg:20  (verify it's output in program)

# Timing issue
❌ write_register(); result = read_register();
✅ write_register(); time.sleep(0.1); result = read_register();
```

---

## Divergerende Resultater

**Symptom:** Test fungerer nogle gange, andre gange fejler

**Actions:**
1. Tjek for race conditions (variable opdateres i loop)
2. Verify cycle time er sufficient (`show logic 1`)
3. Increase sleep time mellem tests
4. Tjek for ST-program side effects (persistent variables)

---

## Test Automation Script Template

**BEMÆRK (v5.0.0):** Python automation scripts er ikke længere nødvendige!

Alle test cases er nu opdateret til **CLI kommandoer** der kan kopieres direkte til telnet session.

### CLI Commands for REAL/INT/UINT Values

Brug de indbyggede CLI kommandoer i stedet for Python helpers:

```bash
# SKRIVE VÆRDIER
write reg <addr> value int <value>     # 16-bit signed INT
write reg <addr> value uint <value>    # 16-bit unsigned
write reg <addr> value real <value>    # 32-bit REAL (2 registers)

# LÆSE VÆRDIER
read reg <addr> int                    # Læs som signed INT
read reg <addr> uint                   # Læs som unsigned
read reg <addr> real                   # Læs som REAL (2 registers)

# EKSEMPLER
write reg 20 value real 3.14159
read reg 22 real
write reg 20 value int -5
read reg 20 int
```

### Legacy Python Helpers (Deprecated)

**Python helpers er nu DEPRECATED** - brug CLI kommandoer i stedet.

Hvis du stadig ønsker at bruge Python automation:

```python
from pymodbus.client import ModbusSerialClient
import time
import struct

def write_real(client, address, value):
    """DEPRECATED: Brug 'write reg X value real Y' CLI kommando"""
    bytes_val = struct.pack('!f', value)
    words = struct.unpack('!HH', bytes_val)
    client.write_registers(address, list(words))

def read_real(client, address):
    """DEPRECATED: Brug 'read reg X real' CLI kommando"""
    result = client.read_holding_registers(address, 2)
    bytes_val = struct.pack('!HH', *result.registers)
    return struct.unpack('!f', bytes_val)[0]

def test_addition():
    """Test 1.1.1: Addition"""
    print("\n" + "="*60)
    print("TEST 1.1.1: Addition (+)")
    print("="*60)

    # Test cases
    tests = [
        (5, 3, 8, "5 + 3 = 8"),
        (-10, 15, 5, "-10 + 15 = 5"),
        (0, 0, 0, "0 + 0 = 0"),
    ]

    for a, b, expected, desc in tests:
        client.write_register(100, a)
        client.write_register(101, b)
        time.sleep(0.05)
        result = client.read_holding_registers(102, 1).registers[0]

        if result == expected:
            print(f"✅ {desc} → {result}")
        else:
            print(f"❌ {desc} → {result} (expected {expected})")

# ============================
# Main Test Runner
# ============================

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

    print("✅ Connected to ESP32")

    # Run tests
    test_addition()
    # ... add more tests

    # Cleanup
    client.close()
    print("\n✅ All tests completed!")

if __name__ == "__main__":
    main()
```

---

# Appendix: Quick Reference

## ST Operator Precedence (High to Low)

1. `()` - Parentheses
2. `NOT` - Logical NOT
3. `*`, `/`, `MOD` - Multiplicative
4. `+`, `-` - Additive
5. `<`, `>`, `<=`, `>=`, `=`, `<>` - Comparison
6. `AND` - Logical AND
7. `XOR` - Logical XOR
8. `OR` - Logical OR

## Builtin Function Summary

| Function | Args | Return | Description |
|----------|------|--------|-------------|
| ABS | 1 | INT/REAL | Absolute value |
| MIN | 2 | INT/REAL | Minimum |
| MAX | 2 | INT/REAL | Maximum |
| SUM | 2 | INT/REAL | Sum (alias for +) |
| SQRT | 1 | REAL | Square root |
| ROUND | 1 | INT | Round to nearest |
| TRUNC | 1 | INT | Truncate |
| FLOOR | 1 | INT | Floor |
| CEIL | 1 | INT | Ceiling |
| LIMIT | 3 | INT/REAL | Clamp to range |
| SEL | 3 | ANY | Boolean selector |
| SIN | 1 | REAL | Sine (radians) |
| COS | 1 | REAL | Cosine (radians) |
| TAN | 1 | REAL | Tangent (radians) |
| INT_TO_REAL | 1 | REAL | Type conversion |
| REAL_TO_INT | 1 | INT | Type conversion |
| BOOL_TO_INT | 1 | INT | Type conversion |
| INT_TO_BOOL | 1 | BOOL | Type conversion |
| DWORD_TO_INT | 1 | INT | Type conversion |
| INT_TO_DWORD | 1 | DWORD | Type conversion |
| SAVE | 0 | BOOL | Save persistent vars |
| LOAD | 0 | BOOL | Load persistent vars |
| MB_READ_COIL | 2 | BOOL | Read Modbus coil |
| MB_READ_INPUT | 2 | BOOL | Read discrete input |
| MB_READ_HOLDING | 2 | INT | Read holding register |
| MB_READ_INPUT_REG | 2 | INT | Read input register |
| MB_WRITE_COIL | 3 | BOOL | Write Modbus coil |
| MB_WRITE_HOLDING | 3 | BOOL | Write holding register |

---

# Udsatte Tests

## Modbus Master Funktioner

**Status:** 🔄 Udsat til senere test

**Funktioner der IKKE testes i denne omgang:**
- MB_READ_COIL(slave_id, address) → BOOL
- MB_READ_INPUT(slave_id, address) → BOOL
- MB_READ_HOLDING(slave_id, address) → INT
- MB_READ_INPUT_REG(slave_id, address) → INT
- MB_WRITE_COIL(slave_id, address, value) → BOOL
- MB_WRITE_HOLDING(slave_id, address, value) → BOOL

**Begrundelse:** Disse tests kræver en Modbus Slave device forbundet til UART1.

**Når klar til test:**
1. Forbind Modbus Slave device til UART1 (TX/RX pins)
2. Konfigurer slave device med kendt ID (f.eks. ID=1)
3. Setup test registers/coils på slave
4. Kør ST Logic programmer der kalder MB_* funktioner
5. Verify at værdier læses/skrives korrekt

**Estimeret testtid:** 5-10 minutter (når hardware er klar)

**Test eksempel:**
```structured-text
PROGRAM
VAR
  slave_id: INT;
  address: INT;
  read_value: INT;
END_VAR
BEGIN
  slave_id := 1;
  address := 100;
  read_value := MB_READ_HOLDING(slave_id, address);
END_PROGRAM
```

---

**Version:** 1.0
**Last Updated:** 2025-12-27
**Status:** ✅ Ready for Execution (59 tests aktive, 6 udsat)

**God test! 🚀**
