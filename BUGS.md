# Bug Tracking - ESP32 Modbus RTU Server

**Projekt:** Modbus RTU Server (ESP32)
**Version:** v4.0.2
**Sidst opdateret:** 2025-12-12
**Build:** Se `build_version.h` for aktuel build number

---

## Status Legend

- 🔴 **CRITICAL** - Funktionalitet virker ikke, skal fixes straks
- 🟡 **HIGH** - Vigtig bug, skal fixes snart
- 🟠 **MEDIUM** - Bør fixes, men ikke kritisk
- 🔵 **LOW** - Nice-to-have, kan vente

**Status:**
- ❌ **OPEN** - Bug ikke løst endnu
- 🔧 **IN PROGRESS** - Bug bliver arbejdet på
- ✅ **FIXED** - Bug løst, venter på verification
- ✔️ **VERIFIED** - Bug løst og verificeret

---

## Bug Liste

### BUG-001: Input Register 220-251 opdateres ikke med ST Logic variable værdier
**Status:** ✅ FIXED
**Prioritet:** 🔴 CRITICAL
**Opdaget:** 2025-12-12
**Fixed:** 2025-12-12
**Version:** v4.0.2

#### Beskrivelse
ST Logic variable værdier skrives aldrig til input registers 220-251, selvom kommentarerne i koden siger de skulle. Modbus master kan derfor ikke læse aktuelle variable værdier - kun status registers (200-219) opdateres korrekt.

**Impact:**
- Input binding virker (Modbus → ST Logic)
- Output visibility er brudt (ST Logic → Modbus læsning virker IKKE)
- Registers 220-251 forbliver 0 eller uinitialiserede

#### Påvirkede Funktioner

**Funktion:** `void registers_update_st_logic_status(void)`
**Fil:** `src/registers.cpp`
**Linjer:** 326-409
**Signatur (v4.0.2):**
```cpp
void registers_update_st_logic_status(void) {
  st_logic_engine_state_t *st_state = st_logic_get_state();

  for (uint8_t prog_id = 0; prog_id < 4; prog_id++) {
    st_logic_program_config_t *prog = st_logic_get_program(st_state, prog_id);
    // ...
    // Linjer 372-395: Variable Values (220-251) kommenterer kun, opdaterer ALDRIG
  }
}
```

**Problematisk kode (linjer 372-395):**
```cpp
// 220-251: Variable Values (32 registers total for 4 programs * 8 vars each)
for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
  const VariableMapping *map = &g_persist_config.var_maps[i];
  if (map->source_type == MAPPING_SOURCE_ST_VAR && map->st_program_id == prog_id) {
    uint16_t var_reg_offset = ST_LOGIC_VAR_VALUES_REG_BASE +
                              (prog_id * 8) + map->st_var_index;
    if (var_reg_offset < INPUT_REGS_SIZE) {
      // Variable values should be updated by ST Logic engine during execution
      // Here we just read/preserve them
      // (actual update happens in st_logic_engine.cpp)  ← LØG!
    }
  }
}
```

**Faktum:** Der er INGEN kode i `st_logic_engine.cpp` der opdaterer disse registers!

#### Foreslået Fix

**Metode 1:** Tilføj til `registers_update_st_logic_status()` (linjer 385-393):
```cpp
if (var_reg_offset < INPUT_REGS_SIZE) {
  // Read variable value from program bytecode
  int16_t var_value = prog->bytecode.variables[map->st_var_index].int_val;
  registers_set_input_register(var_reg_offset, (uint16_t)var_value);
}
```

**Metode 2:** Tilføj separat funktion efter `st_logic_engine_loop()` i main.cpp:
```cpp
void registers_sync_st_variables_to_input_regs(void) {
  // Loop gennem alle ST variable bindings og sync til IR 220-251
}
```

#### Dependencies
- `constants.h`: `ST_LOGIC_VAR_VALUES_REG_BASE` (linje 48)
- `st_types.h`: `st_bytecode_program_t` struct (variabel storage)
- `st_logic_config.h`: `st_logic_program_config_t` struct

#### Test Plan
1. Upload ST program: `VAR x: INT; END_VAR; x := 42;`
2. Bind `x` til output (ingen output register nødvendig for denne test)
3. Enable program
4. Læs Input Register 220 via Modbus FC03
5. **Forventet:** Value = 42
6. **Actual (før fix):** Value = 0

---

### BUG-002: Manglende type checking i ST Logic variable bindings
**Status:** ✅ FIXED
**Prioritet:** 🔴 CRITICAL
**Opdaget:** 2025-12-12
**Fixed:** 2025-12-12
**Version:** v4.0.2

#### Beskrivelse
Variable bindings bruger altid INT type konvertering, selv når ST variablen er defineret som BOOL eller REAL. Dette medfører forkerte værdier ved output.

**Impact:**
- BOOL variable kan outputte høje værdier (fx 1000) i stedet for 0/1
- REAL variable konverteres forkert til INT (truncation)
- Type safety er brudt mellem ST Logic og Modbus

#### Påvirkede Funktioner

**Funktion:** `static void gpio_mapping_write_outputs(void)`
**Fil:** `src/gpio_mapping.cpp`
**Linjer:** 98-150
**Signatur (v4.0.2):**
```cpp
static void gpio_mapping_write_outputs(void) {
  for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
    const VariableMapping* map = &g_persist_config.var_maps[i];
    // ...
  }
}
```

**Problematisk kode (linjer 131-146):**
```cpp
if (!map->is_input) {
  // OUTPUT mode: Read from ST variable, write to Modbus
  int16_t var_value = prog->bytecode.variables[map->st_var_index].int_val;  ← ALTID INT!

  if (map->coil_reg != 65535) {
    if (map->output_type == 1) {
      // Output to COIL (BOOL variables)
      uint8_t coil_value = (var_value != 0) ? 1 : 0;  ← Korrekt casting
      registers_set_coil(map->coil_reg, coil_value);
    } else {
      // Output to HOLDING REGISTER (INT variables)
      registers_set_holding_register(map->coil_reg, (uint16_t)var_value);  ← Ingen type check!
    }
  }
}
```

**Problem:** Koden læser altid `.int_val` fra union, selvom variablen kan være `.bool_val` eller `.real_val`.

#### Foreslået Fix

Tilføj type check (linjer 131-146 refactor):
```cpp
if (!map->is_input) {
  // Get variable type from bytecode
  st_datatype_t var_type = prog->bytecode.var_types[map->st_var_index];

  // Read value based on actual type
  int16_t var_value;
  if (var_type == ST_TYPE_BOOL) {
    var_value = prog->bytecode.variables[map->st_var_index].bool_val ? 1 : 0;
  } else if (var_type == ST_TYPE_REAL) {
    // Convert float to scaled INT (eller skip if output_type mismatch)
    var_value = (int16_t)prog->bytecode.variables[map->st_var_index].real_val;
  } else {
    // ST_TYPE_INT
    var_value = prog->bytecode.variables[map->st_var_index].int_val;
  }

  // Write to destination
  if (map->coil_reg != 65535) {
    if (map->output_type == 1) {
      uint8_t coil_value = (var_value != 0) ? 1 : 0;
      registers_set_coil(map->coil_reg, coil_value);
    } else {
      registers_set_holding_register(map->coil_reg, (uint16_t)var_value);
    }
  }
}
```

**Samme fix nødvendig i INPUT mode** (linjer 69-86):
```cpp
// Check variable type before writing
st_datatype_t var_type = prog->bytecode.var_types[map->st_var_index];
if (var_type == ST_TYPE_BOOL) {
  prog->bytecode.variables[map->st_var_index].bool_val = (reg_value != 0);
} else if (var_type == ST_TYPE_REAL) {
  prog->bytecode.variables[map->st_var_index].real_val = (float)reg_value;
} else {
  prog->bytecode.variables[map->st_var_index].int_val = (int16_t)reg_value;
}
```

#### Dependencies
- `st_types.h`: `st_datatype_t` enum (BOOL, INT, REAL)
- `st_types.h`: `st_bytecode_program_t.var_types[]` array

#### Test Plan
1. Upload ST program: `VAR flag: BOOL; END_VAR; flag := TRUE;`
2. Bind `flag` til coil output
3. Enable program
4. Læs coil via Modbus FC01
5. **Forventet:** Coil = 1 (ON)
6. **Actual (før fix):** Afhænger af internal representation (kan være 0xFFFF)

---

### BUG-003: Manglende bounds checking på variable index
**Status:** ✅ FIXED
**Prioritet:** 🟡 HIGH
**Opdaget:** 2025-12-12
**Fixed:** 2025-12-12
**Version:** v4.0.2

#### Beskrivelse
Variable mapping kan indeholde index til variabler der ikke eksisterer i bytecode (hvis program slettes, eller recompiles med færre variabler). Ingen bounds checking før array access.

**Impact:**
- Potentiel memory corruption (array out of bounds)
- Crash hvis `prog` er NULL
- Undefined behavior

#### Påvirkede Funktioner

**Funktion:** `void registers_update_st_logic_status(void)`
**Fil:** `src/registers.cpp`
**Linjer:** 374-387

**Problematisk kode:**
```cpp
for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
  const VariableMapping *map = &g_persist_config.var_maps[i];
  if (map->source_type == MAPPING_SOURCE_ST_VAR && map->st_program_id == prog_id) {
    // INGEN CHECK: prog kunne være NULL, map->st_var_index kunne være >= var_count
    uint16_t var_reg_offset = ST_LOGIC_VAR_VALUES_REG_BASE +
                              (prog_id * 8) +
                              map->st_var_index;  // ← OUT OF BOUNDS RISK!
```

**Note:** `gpio_mapping.cpp` HAR bounds check (linjer 65-67, 127-129), men `registers.cpp` har IKKE!

#### Foreslået Fix

Tilføj bounds check (linje 376 indsæt):
```cpp
if (map->source_type == MAPPING_SOURCE_ST_VAR && map->st_program_id == prog_id) {

  // Bounds check (tilføj DETTE)
  if (!prog || map->st_var_index >= prog->bytecode.var_count) {
    continue;  // Skip invalid mapping
  }

  uint16_t var_reg_offset = ST_LOGIC_VAR_VALUES_REG_BASE + ...
```

#### Dependencies
- `st_logic_config.h`: `st_logic_program_config_t.bytecode.var_count`

#### Test Plan
1. Upload program med 3 variabler
2. Bind variabel index 5 (ud over grænsen)
3. **Forventet (efter fix):** Binding ignoreres, ingen crash
4. **Actual (før fix):** Memory corruption eller crash

---

### BUG-004: Control register reset bit cleares ikke
**Status:** ✅ FIXED
**Prioritet:** 🟡 HIGH
**Opdaget:** 2025-12-12
**Fixed:** 2025-12-12
**Version:** v4.0.2

#### Beskrivelse
Når Modbus master skriver bit 2 (RESET_ERROR) til control register (HR 200-203), bliver error count clearet, men selve bit 2 bliver ALDRIG clearet. Master ved derfor ikke at kommandoen blev konsumeret.

**Impact:**
- Master kan ikke detektere at RESET_ERROR blev processed
- Hvis bit 2 sættes igen, ingen effekt (idempotent, men ikke transparent)
- Følger ikke standard pattern for control bits (burde auto-clear efter handling)

#### Påvirkede Funktioner

**Funktion:** `void registers_process_st_logic_control(uint16_t addr, uint16_t value)`
**Fil:** `src/registers.cpp`
**Linjer:** 415-458
**Signatur (v4.0.2):**
```cpp
void registers_process_st_logic_control(uint16_t addr, uint16_t value) {
  // Determine which program this control register is for
  if (addr < ST_LOGIC_CONTROL_REG_BASE || addr >= ST_LOGIC_CONTROL_REG_BASE + 4) {
    return;
  }
  uint8_t prog_id = addr - ST_LOGIC_CONTROL_REG_BASE;
  // ...
}
```

**Problematisk kode (linjer 448-457):**
```cpp
// Bit 2: Reset Error flag
if (value & ST_LOGIC_CONTROL_RESET_ERROR) {
  if (prog->error_count > 0) {
    prog->error_count = 0;
    prog->last_error[0] = '\0';
    debug_print("[ST_LOGIC] Logic");
    debug_print_uint(prog_id + 1);
    debug_println(" error cleared via Modbus");
  }
  // BIT 2 ALDRIG CLEARET I CONTROL REGISTER!
}
```

#### Foreslået Fix

Auto-clear bit efter handling (tilføj efter linje 456):
```cpp
// Bit 2: Reset Error flag
if (value & ST_LOGIC_CONTROL_RESET_ERROR) {
  if (prog->error_count > 0) {
    prog->error_count = 0;
    prog->last_error[0] = '\0';
    debug_print("[ST_LOGIC] Logic");
    debug_print_uint(prog_id + 1);
    debug_println(" error cleared via Modbus");
  }

  // Clear bit 2 in control register (auto-acknowledge)
  uint16_t ctrl_val = registers_get_holding_register(addr);
  ctrl_val &= ~ST_LOGIC_CONTROL_RESET_ERROR;  // Clear bit 2
  registers_set_holding_register(addr, ctrl_val);
}
```

**Inspiration:** Counter reset-on-read pattern i `modbus_fc_read.cpp` linjer 30-55.

#### Dependencies
- `registers.cpp`: `registers_get_holding_register()` (linje 32)
- `registers.cpp`: `registers_set_holding_register()` (linje 42)
- `constants.h`: `ST_LOGIC_CONTROL_RESET_ERROR` (bit mask)

#### Test Plan
1. Sæt error på Logic1 (force fejl i program)
2. Læs HR#200 → Bit 3 (ERROR) = 1
3. Skriv HR#200 = 0x0004 (bit 2 = RESET_ERROR)
4. Læs HR#200 igen
5. **Forventet (efter fix):** Value = 0x0000 (bit 2 clearet)
6. **Actual (før fix):** Value = 0x0004 (bit 2 stadig sat)

---

### BUG-005: Inefficient variable binding count lookup
**Status:** ✅ FIXED
**Prioritet:** 🟠 MEDIUM
**Opdaget:** 2025-12-12
**Fixed:** 2025-12-12
**Version:** v4.0.2

#### Beskrivelse
Input register 216-219 (variable binding count) opdateres via O(n×m) nested loop HVER main loop iteration. Med 64 mappings × 4 programs = 256 checks per loop.

**Impact:**
- CPU waste (minimal, men uelegant)
- Skalerer dårligt hvis MAX_VAR_MAPS øges
- Kunne caches i `st_logic_program_config_t`

#### Påvirkede Funktioner

**Funktion:** `void registers_update_st_logic_status(void)`
**Fil:** `src/registers.cpp`
**Linjer:** 362-370

**Inefficient kode:**
```cpp
// 216-219: Variable Count
uint16_t var_count = 0;
for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {  // 0-64 iterations
  const VariableMapping *map = &g_persist_config.var_maps[i];
  if (map->source_type == MAPPING_SOURCE_ST_VAR &&
      map->st_program_id == prog_id) {
    var_count++;
  }
}
registers_set_input_register(ST_LOGIC_VAR_COUNT_REG_BASE + prog_id, var_count);
```

**Problem:** Dette køres HVER main loop iteration (100+ Hz), selvom binding count kun ændres ved CLI `set logic bind/unbind` kommandoer.

#### Foreslået Fix

**Metode 1:** Cache i program config

1. Tilføj felt til `st_logic_program_config_t` (st_logic_config.h linje 41):
```cpp
typedef struct {
  // ... existing fields
  uint8_t binding_count;  // Cached count of variable bindings
} st_logic_program_config_t;
```

2. Opdater count når bind/unbind sker (`cli_commands_logic.cpp` eller `gpio_mapping.cpp`)

3. I `registers_update_st_logic_status()` (linje 362-370 erstat):
```cpp
// 216-219: Variable Count (cached)
registers_set_input_register(ST_LOGIC_VAR_COUNT_REG_BASE + prog_id,
                              prog->binding_count);
```

**Metode 2:** Rate-limit update (kun opdater binding count hvert sekund)

#### Dependencies
- `st_logic_config.h`: `st_logic_program_config_t` struct
- `cli_commands_logic.cpp`: Bind/unbind commands (skal opdatere cache)

#### Test Plan
1. Profiler main loop performance UDEN og MED fix
2. Verify at binding count stadig opdateres korrekt efter bind/unbind

---

### BUG-006: Execution/error counters truncated til 16-bit
**Status:** ✅ FIXED
**Prioritet:** 🔵 LOW
**Opdaget:** 2025-12-12
**Fixed:** 2025-12-12
**Version:** v4.0.2

#### Beskrivelse
Execution count og error count gemmes som `uint32_t` internt, men rapporteres kun som `uint16_t` i input registers (204-211). Data tab efter 65536 executions.

**Impact:**
- Counter wraparound efter 65536 executions (ca. 10 minutter ved 100 Hz)
- Modbus master kan ikke detektere wraparound
- Begrænset til 16-bit uden at bruge 2-register 32-bit læsning

#### Påvirkede Funktioner

**Funktion:** `void registers_update_st_logic_status(void)`
**Fil:** `src/registers.cpp`
**Linjer:** 347-353

**Problematisk kode:**
```cpp
// 204-207: Execution Count (16-bit)
registers_set_input_register(ST_LOGIC_EXEC_COUNT_REG_BASE + prog_id,
                               (uint16_t)(prog->execution_count & 0xFFFF));  ← TRUNCATION!

// 208-211: Error Count (16-bit)
registers_set_input_register(ST_LOGIC_ERROR_COUNT_REG_BASE + prog_id,
                               (uint16_t)(prog->error_count & 0xFFFF));  ← TRUNCATION!
```

**Internal storage (st_logic_config.h linjer 37-38):**
```cpp
uint32_t execution_count;   // Number of times executed (32-bit)
uint32_t error_count;       // Number of execution errors (32-bit)
```

#### Foreslået Fix

**Option 1:** Accepter 16-bit limit (nok for de fleste use cases)
- Gem som `uint16_t` i config også
- Sparer RAM (8 bytes per program = 32 bytes total)

**Option 2:** Tilføj HIGH/LOW register pair
- IR 204-207: Execution count LOW (bits 0-15)
- IR 224-227: Execution count HIGH (bits 16-31) [nye registers]
- Master kan læse begge og kombinere til 32-bit

**Option 3:** Tilføj overflow counter
- IR 204-207: Execution count (16-bit)
- IR 228-231: Execution overflow count [nye registers]
- Value = (overflow × 65536) + count

#### Dependencies
- `constants.h`: Nye register definitions hvis option 2/3

#### Test Plan
1. Force execution count til 65535
2. Kør et program en gang mere
3. **Forventet (før fix):** IR wraps til 0
4. **Forventet (efter fix):** Afhænger af option

---

### BUG-007: Ingen timeout protection på program execution
**Status:** ✅ FIXED
**Prioritet:** 🟠 MEDIUM
**Opdaget:** 2025-12-12
**Fixed:** 2025-12-12
**Version:** v4.0.2

#### Beskrivelse
`st_logic_execute_program()` har kun instruction count limit (10000 steps), men ingen timeout check. Ved meget tight loops kunne program blokere main loop og trigge watchdog.

**Impact:**
- Main loop freeze ved infinite loops (selv med step limit)
- Watchdog timer kunne trigge reset
- Ingen logging af long-running programs

#### Påvirkede Funktioner

**Funktion:** `bool st_logic_execute_program(st_logic_engine_state_t *state, uint8_t program_id)`
**Fil:** `src/st_logic_engine.cpp`
**Linjer:** 35-59
**Signatur (v4.0.2):**
```cpp
bool st_logic_execute_program(st_logic_engine_state_t *state, uint8_t program_id) {
  st_logic_program_config_t *prog = st_logic_get_program(state, program_id);
  if (!prog || !prog->compiled || !prog->enabled) return false;

  st_vm_t vm;
  st_vm_init(&vm, &prog->bytecode);

  bool success = st_vm_run(&vm, 10000);  // Max 10000 steps, MEN ingen timeout!

  memcpy(prog->bytecode.variables, vm.variables, vm.var_count * sizeof(st_value_t));
  prog->execution_count++;
  if (!success || vm.error) {
    prog->error_count++;
    snprintf(prog->last_error, sizeof(prog->last_error), "%s", vm.error_msg);
    return false;
  }

  prog->last_execution_ms = 0;  // TODO: Add timestamp ← FAKTISK TODO I KODEN!
  return true;
}
```

#### Foreslået Fix

Tilføj timing wrapper (linjer 43-57 refactor):
```cpp
// Execute until halt or error (max 10000 steps for safety)
uint32_t start_ms = millis();
bool success = st_vm_run(&vm, 10000);
uint32_t elapsed_ms = millis() - start_ms;

// Log warning if execution took too long
if (elapsed_ms > 100) {  // 100ms threshold
  debug_printf("[WARN] Logic%d execution took %dms (slow!)\n",
               program_id + 1, elapsed_ms);
}

// Store timestamp (implement TODO)
prog->last_execution_ms = elapsed_ms;

// Copy variables back from VM to program config
memcpy(prog->bytecode.variables, vm.variables, vm.var_count * sizeof(st_value_t));
```

**Bonus:** Også implementer den eksisterende TODO for timestamp.

#### Dependencies
- Arduino/ESP32: `millis()` function

#### Test Plan
1. Upload program med tight loop: `WHILE TRUE DO END_WHILE;`
2. Enable program
3. **Forventet (efter fix):** Warning logged efter 100ms
4. **Actual (før fix):** Ingen warning, main loop blokeret

---

## Conventions & References

### Function Signature Format

Alle funktioner dokumenteres med:
```cpp
<return_type> <function_name>(<parameters>)
```

Eksempel:
```cpp
void registers_update_st_logic_status(void)
bool st_logic_execute_program(st_logic_engine_state_t *state, uint8_t program_id)
```

### File References

**Format:** `<file>:<line>` eller `<file>:<start_line>-<end_line>`

Eksempel:
- `src/registers.cpp:326` - Enkelt linje
- `src/registers.cpp:326-409` - Range

### Constants & Defines

Se `include/constants.h` for alle register addresses:
- `ST_LOGIC_STATUS_REG_BASE = 200` (Input registers, status)
- `ST_LOGIC_CONTROL_REG_BASE = 200` (Holding registers, control)
- `ST_LOGIC_EXEC_COUNT_REG_BASE = 204`
- `ST_LOGIC_ERROR_COUNT_REG_BASE = 208`
- `ST_LOGIC_ERROR_CODE_REG_BASE = 212`
- `ST_LOGIC_VAR_COUNT_REG_BASE = 216`
- `ST_LOGIC_VAR_VALUES_REG_BASE = 220`

### Status Bits (constants.h)

**Input Register Status (IR 200-203):**
- Bit 0: `ST_LOGIC_STATUS_ENABLED = 0x0001`
- Bit 1: `ST_LOGIC_STATUS_COMPILED = 0x0002`
- Bit 2: `ST_LOGIC_STATUS_RUNNING = 0x0004` (not used)
- Bit 3: `ST_LOGIC_STATUS_ERROR = 0x0008`

**Control Register Bits (HR 200-203):**
- Bit 0: `ST_LOGIC_CONTROL_ENABLE = 0x0001`
- Bit 1: `ST_LOGIC_CONTROL_START = 0x0002` (future)
- Bit 2: `ST_LOGIC_CONTROL_RESET_ERROR = 0x0004`

---

## Opdateringslog

| Dato | Ændring | Af |
|------|---------|-----|
| 2025-12-12 | Alle 7 bugs fixed (BUG-001 til BUG-007) | Claude Code |
| 2025-12-12 | Initial bug tracking fil oprettet med 7 bugs | Claude Code |

---

## Notes

- Denne fil skal opdateres når bugs fixes, verificeres, eller nye bugs opdages
- Ved version bump: Verificer at funktionssignaturer stadig er korrekte
- Ved refactoring: Opdater fil/linje references
- Tilføj VERIFIED dato når bug er testet i produktion
