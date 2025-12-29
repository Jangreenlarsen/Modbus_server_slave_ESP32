# ST Logic Module Dybdeanalyse

**Dato:** 2025-12-29
**Build:** #821
**Analyseret af:** Claude Opus 4.5

---

## Executive Summary

Analyseret ST Logic modulet for korrekt implementering af alle kommandoer og funktioner.
Fundet **5 bugs** der skal udbedres, hvoraf **2 er kritiske**.

| Severity | Count | Status |
|----------|-------|--------|
| 🔴 CRITICAL | 2 | Skal fixes ASAP |
| 🟡 HIGH | 3 | Bør fixes snart |
| TOTAL | 5 | Alle påvirker funktionalitet |

---

## Fundne Bugs

### BUG-116: Modbus Master funktioner IKKE registreret i compiler 🔴 CRITICAL

**Problem:** MB_READ_COIL, MB_READ_INPUT, MB_READ_HOLDING, MB_READ_INPUT_REG, MB_WRITE_COIL, MB_WRITE_HOLDING er implementeret i st_builtins.cpp og st_builtin_modbus.cpp, men **compileren kender dem IKKE**.

**Symptom:** ST program kompilering fejler med "Unknown function: MB_READ_COIL" osv.

**Impact:** Modbus Master funktionalitet er **100% ubrugelig** i ST Logic programmer, selvom det er dokumenteret i README.md.

**Lokation:** `src/st_compiler.cpp` linje 244-272

**Manglende entries i function mapper:**
```cpp
// MANGLER:
else if (strcasecmp(node->data.function_call.func_name, "MB_READ_COIL") == 0) func_id = ST_BUILTIN_MB_READ_COIL;
else if (strcasecmp(node->data.function_call.func_name, "MB_READ_INPUT") == 0) func_id = ST_BUILTIN_MB_READ_INPUT;
else if (strcasecmp(node->data.function_call.func_name, "MB_READ_HOLDING") == 0) func_id = ST_BUILTIN_MB_READ_HOLDING;
else if (strcasecmp(node->data.function_call.func_name, "MB_READ_INPUT_REG") == 0) func_id = ST_BUILTIN_MB_READ_INPUT_REG;
else if (strcasecmp(node->data.function_call.func_name, "MB_WRITE_COIL") == 0) func_id = ST_BUILTIN_MB_WRITE_COIL;
else if (strcasecmp(node->data.function_call.func_name, "MB_WRITE_HOLDING") == 0) func_id = ST_BUILTIN_MB_WRITE_HOLDING;
```

**Test case:**
```structured-text
PROGRAM test
VAR
  temp: INT;
END_VAR
BEGIN
  temp := MB_READ_HOLDING(1, 100);  (* Fejler: "Unknown function: MB_READ_HOLDING" *)
END_PROGRAM
```

---

### BUG-117: MIN/MAX funktioner ikke type-aware 🔴 CRITICAL

**Problem:** MIN og MAX funktionerne sammenligner kun `int_val`, ignorerer type information.

**Symptom:** MIN(1.5, 2.5) returnerer forkert resultat (sammenligner bit-representation af float som int).

**Impact:** Matematiske beregninger med REAL værdier giver **forkerte resultater**.

**Lokation:** `src/st_builtins.cpp` linje 30-40

**Nuværende kode (forkert):**
```cpp
st_value_t st_builtin_min(st_value_t a, st_value_t b) {
  st_value_t result;
  result.int_val = (a.int_val < b.int_val) ? a.int_val : b.int_val;  // FORKERT!
  return result;
}
```

**Korrekt kode (skal tilføjes type-awareness som VM'en har):**
```cpp
st_value_t st_builtin_min_typed(st_value_t a, st_value_t b, st_datatype_t a_type, st_datatype_t b_type) {
  st_value_t result;

  // REAL type promotion
  if (a_type == ST_TYPE_REAL || b_type == ST_TYPE_REAL) {
    float a_f = (a_type == ST_TYPE_REAL) ? a.real_val : (float)a.int_val;
    float b_f = (b_type == ST_TYPE_REAL) ? b.real_val : (float)b.int_val;
    result.real_val = (a_f < b_f) ? a_f : b_f;
    return result;
  }

  // DINT comparison
  if (a_type == ST_TYPE_DINT || b_type == ST_TYPE_DINT) {
    int32_t a_d = (a_type == ST_TYPE_DINT) ? a.dint_val : (int32_t)a.int_val;
    int32_t b_d = (b_type == ST_TYPE_DINT) ? b.dint_val : (int32_t)b.int_val;
    result.dint_val = (a_d < b_d) ? a_d : b_d;
    return result;
  }

  // INT comparison (default)
  result.int_val = (a.int_val < b.int_val) ? a.int_val : b.int_val;
  return result;
}
```

**Test case:**
```structured-text
VAR
  a: REAL := 1.5;
  b: REAL := 2.5;
  result: REAL;
END_VAR
BEGIN
  result := MIN(a, b);  (* Forventer 1.5, får garbage *)
END
```

---

### BUG-118: ABS funktion kun INT type 🟡 HIGH

**Problem:** ABS understøtter kun INT type. REAL og DINT ignoreres.

**Symptom:** ABS(-1.5) returnerer 1 i stedet for 1.5.

**Impact:** Matematiske beregninger med REAL/DINT værdier giver forkerte resultater.

**Lokation:** `src/st_builtins.cpp` linje 19-28

**Nuværende kode:**
```cpp
st_value_t st_builtin_abs(st_value_t x) {
  st_value_t result;
  if (x.int_val == INT16_MIN) {
    result.int_val = INT16_MAX;
  } else {
    result.int_val = (x.int_val < 0) ? -x.int_val : x.int_val;  // Kun INT!
  }
  return result;
}
```

**Korrekt kode:**
```cpp
st_value_t st_builtin_abs_typed(st_value_t x, st_datatype_t x_type) {
  st_value_t result;

  if (x_type == ST_TYPE_REAL) {
    result.real_val = fabsf(x.real_val);
    return result;
  }

  if (x_type == ST_TYPE_DINT) {
    if (x.dint_val == INT32_MIN) {
      result.dint_val = INT32_MAX;
    } else {
      result.dint_val = (x.dint_val < 0) ? -x.dint_val : x.dint_val;
    }
    return result;
  }

  // INT (16-bit)
  if (x.int_val == INT16_MIN) {
    result.int_val = INT16_MAX;
  } else {
    result.int_val = (x.int_val < 0) ? -x.int_val : x.int_val;
  }
  return result;
}
```

---

### BUG-119: LIMIT funktion ikke type-aware 🟡 HIGH

**Problem:** LIMIT bruger kun int_val, ignorerer type information.

**Symptom:** LIMIT(0.5, 1.0, 2.0) returnerer forkert resultat.

**Impact:** Clamping af REAL værdier virker ikke korrekt.

**Lokation:** `src/st_builtins.cpp` linje 97-110

**Nuværende kode:**
```cpp
st_value_t st_builtin_limit(st_value_t min_val, st_value_t value, st_value_t max_val) {
  st_value_t result;
  if (value.int_val < min_val.int_val) {
    result.int_val = min_val.int_val;
  } else if (value.int_val > max_val.int_val) {
    result.int_val = max_val.int_val;
  } else {
    result.int_val = value.int_val;
  }
  return result;
}
```

---

### BUG-120: VM st_vm_exec_call_builtin mangler type-aware kald til MIN/MAX/ABS/LIMIT 🟡 HIGH

**Problem:** VM'en kalder `st_builtin_call()` uden type information for MIN/MAX/ABS/LIMIT.

**Lokation:** `src/st_vm.cpp` linje 762-854

**Nuværende kode (linje 821):**
```cpp
result = st_builtin_call(func_id, arg1, arg2);  // Ingen type info!
```

**Løsning:** Tilføj special case handling for MIN/MAX/ABS/LIMIT ligesom SUM (linje 798-819).

---

## Opsummering af Rettelser

### Prioritet 1: BUG-116 (Modbus Master i compiler)
**Estimeret kompleksitet:** Simpel
**Filer:** `src/st_compiler.cpp`
**Ændringer:** Tilføj 6 `else if` statements til function mapper

### Prioritet 2: BUG-117 (MIN/MAX type-aware)
**Estimeret kompleksitet:** Medium
**Filer:** `src/st_builtins.cpp`, `src/st_vm.cpp`
**Ændringer:**
1. Modificer MIN/MAX til at tage type parametre
2. Tilføj special case i VM ligesom SUM

### Prioritet 3: BUG-118 (ABS type-aware)
**Estimeret kompleksitet:** Medium
**Filer:** `src/st_builtins.cpp`, `src/st_vm.cpp`
**Ændringer:** Samme mønster som MIN/MAX

### Prioritet 4: BUG-119/120 (LIMIT type-aware)
**Estimeret kompleksitet:** Medium
**Filer:** `src/st_builtins.cpp`, `src/st_vm.cpp`
**Ændringer:** Samme mønster som MIN/MAX

---

## Verificerede Funktionaliteter (OK)

| Komponent | Status | Kommentar |
|-----------|--------|-----------|
| ST Lexer | ✅ OK | Alle tokens parsed korrekt |
| ST Parser | ✅ OK | AST bygges korrekt |
| ST Compiler | ⚠️ Delvist | Mangler Modbus Master funktioner |
| ST VM | ⚠️ Delvist | Type-awareness mangler for nogle builtins |
| CLI: upload/delete | ✅ OK | Virker korrekt |
| CLI: bind | ✅ OK | Virker korrekt |
| CLI: enabled/disabled | ✅ OK | Virker korrekt |
| CLI: show logic | ✅ OK | Viser alle detaljer |
| Arithmetic operators | ✅ OK | Type-aware (ADD/SUB/MUL/DIV/MOD) |
| Comparison operators | ✅ OK | Type-aware |
| Logical operators | ✅ OK | AND/OR/XOR/NOT virker |
| Bitwise operators | ✅ OK | SHL/SHR med validation |
| Control flow | ✅ OK | IF/FOR/WHILE/CASE virker |
| Type conversion | ✅ OK | INT_TO_REAL, REAL_TO_INT, etc. |
| Persistence | ✅ OK | SAVE/LOAD virker |
| Trig functions | ✅ OK | SIN/COS/TAN virker |

---

## Næste Skridt

1. **ASAP:** Fix BUG-116 (Modbus Master i compiler) - ~10 minutter
2. **Derefter:** Fix BUG-117/118/119/120 (type-aware builtins) - ~30 minutter
3. **Test:** Verificer alle funktioner med test cases
4. **Commit:** Build #822 med alle rettelser
5. **Opdater:** BUGS_INDEX.md med nye bug entries

---

**Analysens konklusion:** ST Logic modulet er generelt velimplementeret med god type-awareness i de fleste operatorer. De fundne bugs er afgrænsede og kan fixes relativt hurtigt. Den mest kritiske bug (BUG-116) blokerer komplet for Modbus Master funktionalitet i ST programmer.
