# ST Logic Type System Refactoring Plan

## Mål
Implementer IEC 61131-3 standard datatyper med korrekt Modbus register mapping.

## Nuværende System (FORKERT)
```c
INT  = int32_t  (-2^31 til 2^31-1)  → 1 register (FEJL: truncates til 16-bit)
DWORD = uint32_t (0 til 2^32-1)    → 1 register (FEJL: truncates til 16-bit)
REAL = float   (32-bit IEEE 754)   → 1 register (FEJL: truncates til 16-bit)
```

## Nyt System (KORREKT IEC 61131-3)
```c
BOOL  = bool    (0/1)               → 1 coil eller 1 register
INT   = int16_t (-32768 til 32767)  → 1 register (16-bit)
DINT  = int32_t (-2^31 til 2^31-1)  → 2 registre (low+high word)
DWORD = uint32_t (0 til 2^32-1)     → 2 registre (low+high word)
REAL  = float   (32-bit IEEE 754)   → 2 registre (low+high word)
```

## Filer Der Skal Ændres

### 1. Type Definitioner (Core)
- [x] `include/st_types.h`
  - Tilføj ST_TYPE_DINT
  - Opdater ST_TYPE_INT kommentar til 16-bit
  - Tilføj int16_t felt til st_value_t union
  - Tilføj dint_val felt

### 2. Lexer & Parser (Syntax)
- [x] `src/st_lexer.cpp`
  - Tilføj "DINT" keyword (ST_TOK_DINT_KW)
  - Opdater INT literal parsing for 16-bit range check

- [x] `src/st_parser.cpp`
  - Tilføj DINT type parsing
  - Opdater INT literal range: -32768 til 32767
  - Opdater DINT literal range: -2^31 til 2^31-1

### 3. Compiler (Bytecode Generation)
- [x] `src/st_compiler.cpp`
  - Opdater type coercion rules
  - INT + INT = INT (16-bit overflow wrapping)
  - DINT + DINT = DINT (32-bit overflow wrapping)
  - INT + DINT = DINT (promotion)

### 4. Virtual Machine (Execution)
- [x] `src/st_vm.cpp`
  - **KRITISK:** Opdater aritmetik operationer
  - st_vm_exec_add() - 16-bit signed overflow for INT
  - st_vm_exec_sub() - 16-bit signed overflow for INT
  - st_vm_exec_mul() - 16-bit signed overflow for INT
  - st_vm_exec_div() - 16-bit signed division for INT
  - Tilsvarende for DINT med 32-bit aritmetik

### 5. Builtin Functions
- [x] `src/st_builtins.cpp`
  - Opdater ABS(), MIN(), MAX() til at håndtere INT vs DINT
  - Type promotion regler

### 6. Modbus Register Mapping (I/O)
- [x] `src/gpio_mapping.cpp`
  - **KRITISK:** Multi-register support
  - INT → 1 register (direct read/write)
  - DINT → 2 registre (low word = reg, high word = reg+1)
  - DWORD → 2 registre (samme layout)
  - REAL → 2 registre (IEEE 754 split i 2x16-bit)

- [x] `src/cli_commands_logic.cpp`
  - Opdater bind kommando til at auto-allokere multi-register
  - "set logic 1 bind myDint reg:100" → allocate HR100+HR101

### 7. Register Allocator
- [x] `src/register_allocator.cpp`
  - Opdater til at tracke multi-register allocations
  - DINT/DWORD/REAL bruger 2 consecutive registre

## Breaking Changes

### Backward Compatibility Issues
1. **Eksisterende ST programmer** med INT vil ændre opførsel:
   - INT overflow vil nu wrappe ved ±32768 i stedet for ±2^31
   - Programs skal recompiles

2. **Register bindings** skal opdateres:
   - DINT/REAL bruger nu 2 registre i stedet for 1
   - Eksisterende bindings kan konflikte

### Migration Path
1. **Version bump:** v4.4.2 → v5.0.0 (breaking change)
2. **NVS migration:** Auto-detect gammel config, vis warning
3. **Test plan opdatering:** Alle tests skal opdateres til nye typer

## Implementation Steps (Estimeret 3-4 timer)

### Fase 1: Core Type System (1 time)
1. Opdater st_types.h med DINT
2. Opdater st_lexer.cpp med DINT keyword
3. Opdater st_parser.cpp med DINT parsing
4. Compile test

### Fase 2: VM Execution (1 time)
1. Opdater st_vm.cpp aritmetik til 16-bit INT
2. Tilføj DINT aritmetik (32-bit)
3. Type promotion regler
4. Unit test aritmetik

### Fase 3: Multi-Register I/O (1-2 timer)
1. Opdater gpio_mapping.cpp read/write
2. Opdater register_allocator.cpp
3. Test multi-register bindings

### Fase 4: Testing & Documentation (30 min)
1. Opdater ST_COMPLETE_TEST_PLAN.md
2. Tilføj DINT/multi-register tests
3. Opdater CHANGELOG.md

## Test Cases (Efter Implementation)

```bash
# Test 1: INT 16-bit overflow
VAR a: INT; b: INT; result: INT; END_VAR
result := 32767 + 1;  # Forventet: -32768 ✓

# Test 2: DINT 32-bit math
VAR a: DINT; b: DINT; result: DINT; END_VAR
result := 100000 + 200000;  # Forventet: 300000 (kræver 2 registre)

# Test 3: REAL 32-bit float
VAR a: REAL; b: REAL; result: REAL; END_VAR
result := 3.14159 + 2.71828;  # Forventet: 5.85987

# Test 4: Type promotion
VAR a: INT; b: DINT; result: DINT; END_VAR
result := a + b;  # INT promotes til DINT
```

## Risici & Overvejelser

1. **Performance:** Multi-register read/write er langsommere
2. **Memory:** DINT/REAL bruger 2x register space
3. **Complexity:** Mere kompleks register allocation logic
4. **Testing:** Meget mere omfattende test matrix

## Anbefaling

**STOR opgave!** Vil tage 3-4 timer at implementere korrekt.

**Alternativer:**
1. **Quick fix:** Ret kun INT til 16-bit (lad DINT vente)
2. **Full implementation:** Implementer alt på én gang
3. **Staged rollout:** INT først, derefter DINT i næste release

**Hvad foretrækker du?**
