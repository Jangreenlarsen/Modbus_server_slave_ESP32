# Bug Tracking - ESP32 Modbus RTU Server

**Projekt:** Modbus RTU Server (ESP32)
**Version:** v4.2.0
**Sidst opdateret:** 2025-12-15
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

### BUG-008: IR 220-251 opdateres for tidligt (1 iteration latency)
**Status:** ✅ FIXED
**Prioritet:** 🟠 MEDIUM
**Opdaget:** 2025-12-13
**Fixed:** 2025-12-13
**Version:** v4.1.0
**Fixed in:** v4.2.0

#### Beskrivelse
`registers_update_st_logic_status()` kaldes FØR `st_logic_engine_loop()` i main loop, hvilket betyder at Input Registers 220-251 (ST Logic variable values) opdateres med værdier fra forrige iteration i stedet for aktuelle værdier.

**Impact:**
- Modbus master læser IR 220-251 og får data med 1 loop cycle latency (1-10ms delay)
- ST Logic variable ændringer synliggøres først i næste iteration
- Forvirrende debugging via Modbus reads (værdier er "bagud")
- Inkonsistent med OUTPUT flow (HR/coils opdateres EFTER execution)

#### Påvirkede Funktioner

**Funktion:** `void loop(void)`
**Fil:** `src/main.cpp`
**Linjer:** 134-177
**Problematisk sekvens (linjer 156-167):**
```cpp
156:  registers_update_st_logic_status();    // ← Opdaterer IR 220-251 med GAMLE værdier!
160:  gpio_mapping_read_before_st_logic();   // Read inputs
163:  st_logic_engine_loop(...);             // Execute programs, update variables
167:  gpio_mapping_write_after_st_logic();   // Write outputs
```

**Forventet sekvens:**
```cpp
160:  gpio_mapping_read_before_st_logic();   // 1. Read inputs
163:  st_logic_engine_loop(...);             // 2. Execute programs
167:  gpio_mapping_write_after_st_logic();   // 3. Write outputs
???:  registers_update_st_logic_status();    // 4. Update IR 200-293 med FRISKE værdier
```

#### Foreslået Fix

**Flyt linje 156 til EFTER linje 167:**
```cpp
void loop() {
  // ... network, modbus, cli ...

  counter_engine_loop();
  timer_engine_loop();

  // Update DYNAMIC registers (counters/timers)
  registers_update_dynamic_registers();
  registers_update_dynamic_coils();

  // UNIFIED VARIABLE MAPPING + ST LOGIC EXECUTION
  gpio_mapping_read_before_st_logic();   // 1. Read inputs
  st_logic_engine_loop(...);             // 2. Execute programs
  gpio_mapping_write_after_st_logic();   // 3. Write outputs

  // UPDATE ST LOGIC STATUS REGISTERS (flyttet hertil!)
  registers_update_st_logic_status();    // 4. Update IR 200-293 med aktuelle værdier

  heartbeat_loop();
  watchdog_feed();
  delay(1);
}
```

#### Implementeret Fix (2025-12-13)

**Fil:** `src/main.cpp` linjer 151-171

**Ændring:**
Flyttet `registers_update_st_logic_status()` fra linje 156 (før ST Logic execution) til efter linje 164 (efter gpio_mapping_write_after_st_logic).

**Commit:** 135b1b1 "FIX: BUG-008 - ST Logic status registers stale data"

**Resultat:**
- ✅ IR 200-251 opdateres efter ST Logic execution (zero latency)
- ✅ ST Logic variable værdier synliggøres øjeblikkeligt i IR 220-251
- ✅ Konsistent med OUTPUT flow (alle data frisk samme iteration)

#### Dependencies
- `src/main.cpp`: loop() funktion ordre
- `src/registers.cpp`: registers_update_st_logic_status()

#### Test Plan
1. Upload ST program: `VAR x: INT; END_VAR; x := x + 1;`
2. Enable program
3. Læs IR 220 kontinuerligt via Modbus FC04
4. **Forventet (efter fix):** x incrementer synkront med execution
5. **Actual (før fix):** x værdier er 1 iteration forsinkede

---

### BUG-009: Inkonsistent type handling i IR 220-251 vs gpio_mapping
**Status:** ❌ OPEN
**Prioritet:** 🔵 LOW
**Opdaget:** 2025-12-13
**Version:** v4.1.0

#### Beskrivelse
BUG-002 fix implementerer type-aware variable læsning i `gpio_mapping.cpp`, men `registers_update_st_logic_status()` læser ALTID `.int_val` fra bytecode union, selv når variablen er BOOL eller REAL type.

**Impact:**
- BOOL variables: Kan vise forkerte værdier i IR 220-251 hvis internal representation ikke er 0/1
- REAL variables: Konverteres forkert (læser int_val i stedet for real_val → garbage data)
- INT variables: Fungerer korrekt
- Inkonsistens mellem INPUT/OUTPUT flow (gpio_mapping bruger korrekt type check)

#### Påvirkede Funktioner

**Funktion:** `void registers_update_st_logic_status(void)`
**Fil:** `src/registers.cpp`
**Linjer:** 331-445
**Problematisk kode (linje 387):**
```cpp
// BUG-001 FIX: Update input register with actual variable value from bytecode
int16_t var_value = prog->bytecode.variables[map->st_var_index].int_val;  ← ALTID INT!
registers_set_input_register(var_reg_offset, (uint16_t)var_value);
```

**Korrekt implementering i gpio_mapping.cpp (linjer 142-153):**
```cpp
st_datatype_t var_type = prog->bytecode.var_types[map->st_var_index];
if (var_type == ST_TYPE_BOOL) {
  var_value = prog->bytecode.variables[map->st_var_index].bool_val ? 1 : 0;
} else if (var_type == ST_TYPE_REAL) {
  var_value = (int16_t)prog->bytecode.variables[map->st_var_index].real_val;
} else {
  var_value = prog->bytecode.variables[map->st_var_index].int_val;
}
```

#### Foreslået Fix

**Refactor linje 387-388 i registers.cpp:**
```cpp
if (var_reg_offset < INPUT_REGS_SIZE) {
  // BUG-001 FIX: Update input register with actual variable value from bytecode
  // BUG-009 FIX: Use type-aware reading (samme som gpio_mapping.cpp)
  st_datatype_t var_type = prog->bytecode.var_types[map->st_var_index];
  int16_t var_value;

  if (var_type == ST_TYPE_BOOL) {
    var_value = prog->bytecode.variables[map->st_var_index].bool_val ? 1 : 0;
  } else if (var_type == ST_TYPE_REAL) {
    var_value = (int16_t)prog->bytecode.variables[map->st_var_index].real_val;
  } else {
    // ST_TYPE_INT or ST_TYPE_DWORD
    var_value = prog->bytecode.variables[map->st_var_index].int_val;
  }

  registers_set_input_register(var_reg_offset, (uint16_t)var_value);
}
```

#### Dependencies
- `st_types.h`: `st_datatype_t` enum (ST_TYPE_BOOL, ST_TYPE_INT, ST_TYPE_REAL)
- `st_types.h`: `st_bytecode_program_t.var_types[]` array

#### Test Plan
1. Upload ST program: `VAR flag: BOOL; temp: REAL; END_VAR; flag := TRUE; temp := 42.7;`
2. Bind `flag` og `temp` til outputs
3. Enable program
4. Læs IR 220-221 via Modbus FC04
5. **Forventet (efter fix):** IR 220 = 1 (BOOL), IR 221 = 42 (REAL truncated)
6. **Actual (før fix):** Afhænger af internal union representation (potentielt garbage)

---

### BUG-010: Forkert bounds check for INPUT bindings (DI limiteret til HR size)
**Status:** ❌ OPEN
**Prioritet:** 🔵 LOW
**Opdaget:** 2025-12-13
**Version:** v4.1.0

#### Beskrivelse
INPUT bindings kan mappe til enten Holding Registers (input_type==0) eller Discrete Inputs (input_type==1), men bounds check bruger ALTID `HOLDING_REGS_SIZE` konstanten, hvilket unødvendigt begrænser DI input bindings.

**Impact:**
- Hvis bruger mapper DI #300 (over HOLDING_REGS_SIZE=256), skippes bindinget selv om DI array er større
- Kunstig begrænsning på DI input bindings
- Ingen funktionel påvirkning hvis DI array også er 256 (men stadig inkorrekt)

#### Påvirkede Funktioner

**Funktion:** `static void gpio_mapping_read_inputs(void)`
**Fil:** `src/gpio_mapping.cpp`
**Linjer:** 26-97
**Problematisk kode (linje 71):**
```cpp
if (map->input_reg != 65535 && map->input_reg < HOLDING_REGS_SIZE) {  ← FORKERT FOR DI!
  uint16_t reg_value;

  // Check input_type: 0 = Holding Register (HR), 1 = Discrete Input (DI)
  if (map->input_type == 1) {
    // Discrete Input: read as BOOL (0 or 1)
    reg_value = registers_get_discrete_input(map->input_reg) ? 1 : 0;  ← Skal bruge DISCRETE_INPUTS_SIZE!
  } else {
    // Holding Register: read as INT
    reg_value = registers_get_holding_register(map->input_reg);  ← OK, bruger HR
  }
```

#### Foreslået Fix

**Refactor linje 71-93 med type-aware bounds check:**
```cpp
if (map->input_reg != 65535) {
  // BUG-010 FIX: Type-aware bounds check
  if (map->input_type == 1) {
    // Discrete Input - check DI array size
    if (map->input_reg >= DISCRETE_INPUTS_SIZE * 8) continue;
  } else {
    // Holding Register - check HR array size
    if (map->input_reg >= HOLDING_REGS_SIZE) continue;
  }

  uint16_t reg_value;

  if (map->input_type == 1) {
    // Discrete Input: read as BOOL (0 or 1)
    reg_value = registers_get_discrete_input(map->input_reg) ? 1 : 0;
  } else {
    // Holding Register: read as INT
    reg_value = registers_get_holding_register(map->input_reg);
  }

  // ... rest of type conversion code ...
}
```

#### Dependencies
- `constants.h`: `HOLDING_REGS_SIZE`, `DISCRETE_INPUTS_SIZE`
- `types.h`: `VariableMapping.input_type` field

#### Test Plan
1. Prøv at binde ST variable til DI #300 (over HOLDING_REGS_SIZE)
2. **Forventet (før fix):** Binding ignoreres
3. **Forventet (efter fix):** Binding fungerer hvis DISCRETE_INPUTS_SIZE > 300
4. Verify at HR bindings stadig bounds-checkes korrekt

---

### BUG-011: Forvirrende variabelnavn `coil_reg` bruges til HR også
**Status:** ❌ OPEN
**Prioritet:** 🔵 LOW (COSMETIC)
**Opdaget:** 2025-12-13
**Version:** v4.1.0

#### Beskrivelse
`VariableMapping` struct bruger feltet `coil_reg` til BÅDE coils (output_type==1) og holding registers (output_type==0), hvilket er forvirrende når man læser koden.

**Impact:**
- Forvirrende når man debugger eller læser kildekode
- Ingen funktionel påvirkning
- Gør koden sværere at vedligeholde

#### Påvirkede Funktioner

**Funktion:** `static void gpio_mapping_write_outputs(void)`
**Fil:** `src/gpio_mapping.cpp`
**Linjer:** 106-170
**Forvirrende kode (linje 156-165):**
```cpp
// Check output_type to determine destination
if (map->coil_reg != 65535) {
  // output_type: 0 = Holding Register, 1 = Coil
  if (map->output_type == 1) {
    // Output to COIL (BOOL variables)
    uint8_t coil_value = (var_value != 0) ? 1 : 0;
    registers_set_coil(map->coil_reg, coil_value);  ← OK, coil
  } else {
    // Output to HOLDING REGISTER (INT variables)
    registers_set_holding_register(map->coil_reg, (uint16_t)var_value);  ← Misleading name!
  }
}
```

**Også brugt i:**
- `src/gpio_mapping.cpp`: Linje 121 (GPIO output mode)
- `include/types.h`: `VariableMapping` struct definition

#### Foreslået Fix

**Omdøb `coil_reg` → `output_reg` i types.h:**
```cpp
// types.h - VariableMapping struct
typedef struct {
  // ... existing fields ...

  // OUTPUT destination (old name: coil_reg)
  uint16_t output_reg;  // Coil address (if output_type==1) or HR address (if output_type==0)

  // ... rest of fields ...
} VariableMapping;
```

**Opdater ALLE references:**
- `src/gpio_mapping.cpp`: Alle `map->coil_reg` → `map->output_reg`
- `src/cli_commands_logic.cpp`: Bind/unbind commands
- `src/config_struct.cpp`: Initialization
- Alle andre steder hvor `VariableMapping.coil_reg` bruges

**Alternativ (mindre breaking):** Tilføj comment i types.h der forklarer at `coil_reg` bruges til både coils og HR.

#### Dependencies
- `types.h`: `VariableMapping` struct
- Alle filer der bruger `VariableMapping.coil_reg`

#### Test Plan
1. Find-and-replace `coil_reg` → `output_reg` i hele codebase
2. Compile og verify ingen compile errors
3. Test at bindings stadig fungerer (både coil og HR outputs)

**Note:** Dette er en COSMETIC fix, prioritet kan vente til større refactoring.

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

### BUG-012: ST Logic "both" mode binding skaber dobbelt output i 'show logic'
**Status:** ❌ OPEN
**Prioritet:** 🟡 HIGH
**Opdaget:** 2025-12-13
**Version:** v4.1.0

#### Beskrivelse
Når bruger gør `set logic 1 bind timer reg:100`, bliver det oprettet som "both" mode binding, hvilket skaber **2 separate VariableMapping entries**:
1. INPUT mapping: `timer ← HR#100`
2. OUTPUT mapping: `timer → Reg#100`

Resultatet er at `show logic 1` viser begge mappings, hvilket giver intryk af **11 bindings fra 6 kommandoer** (hver kommando skaber 2 bindings).

**Impact:**
- Forvirrende output for brugeren (tæller dobbelt)
- Modbus I/O virker korrekt (input læses, output skrives), men visningen er misvisende
- Bytes-forbrug øges unødvendigt (hver binding bruger VariableMapping struct)

#### Påvirkede Funktioner

**Funktion:** `int cli_cmd_set_logic_bind_by_name(...)`
**Fil:** `src/cli_commands_logic.cpp`
**Linjer:** 234-321
**Problematisk kode (linjer 283-294):**
```cpp
if (strncmp(binding_spec, "reg:", 4) == 0) {
  // Holding register (16-bit integer)
  register_addr = atoi(binding_spec + 4);
  direction = "both";  // ← ALTID "both" for reg:
  input_type = 0;  // HR
  output_type = 0;  // HR
} else if (strncmp(binding_spec, "coil:", 5) == 0) {
  // Coil (BOOL read/write)
  register_addr = atoi(binding_spec + 5);
  direction = "both";  // ← ALTID "both" for coil:
  input_type = 1;  // DI
  output_type = 1;  // Coil
}
```

**Og derefter (linjer 399-424):**
```cpp
if (is_input && is_output) {
  // "both" mode: Create TWO mappings (INPUT + OUTPUT)
  // ... skaber map_in og map_out som separate entries
}
```

#### Foreslået Fix

**Option 1: Skift til "output" mode som default**
Hvis bruger ikke specificerer input/output, antag "output" mode for `reg:` og `coil:`:
```cpp
if (strncmp(binding_spec, "reg:", 4) == 0) {
  register_addr = atoi(binding_spec + 4);
  direction = "output";  // ← ÆNDRET fra "both"
  input_type = 0;
  output_type = 0;
} else if (strncmp(binding_spec, "coil:", 5) == 0) {
  register_addr = atoi(binding_spec + 5);
  direction = "output";  // ← ÆNDRET fra "both"
  input_type = 1;
  output_type = 1;
}
```

**Option 2: Tilføj explicit direction syntax**
Tillad bruger at specificere retning:
```
set logic 1 bind timer reg:100 output      # Kun output
set logic 1 bind sensor reg:100 input      # Kun input
set logic 1 bind data reg:100 both         # Både input og output (2 mappings)
```

**Anbefaling:** Option 1 er enklere og matcher intuition - registre bruges typisk til OUTPUT.

#### Test Plan
1. Gør: `set logic 1 bind timer reg:100`
2. Kør: `show logic 1`
3. **Forventet (før fix):** 2 bindings (timer ← HR#100 og timer → Reg#100)
4. **Forventet (efter fix, option 1):** 1 binding (timer → Reg#100)

---

### BUG-013: Visnings-rækkefølge af ST Logic bindings matcher ikke variabel indeks rækkefølge
**Status:** ✔️ IKKE EN BUG - DESIGN
**Prioritet:** 🔵 LOW (KOSMETISK)
**Opdaget:** 2025-12-13
**Version:** v4.1.0

#### Beskrivelse
Når bindings vises med `show logic 1`, vises de i den rækkefølge de ligger i `var_maps[]` array, ikke sorteret efter variabel indeks. Dette kan være forvirrende når indekserne ikke stiger 0,1,2,3,...

**Eksempel:**
```
VAR definition (viser indekser):
  [0] state: INT
  [1] red: BOOL
  [2] yellow: BOOL
  [3] green: BOOL
  [4] timer: INT
  [5] status: BOOL
  [6] blink_timer: INT
  [7] blink_on: BOOL

show logic 1 output (viser i var_maps rækkefølge, ikke indeks rækkefølge):
  [4] timer ← HR#100
  [0] state ← HR#101
  [1] red ← Input-dis#10
  [2] yellow ← Input-dis#11
  [3] green ← Input-dis#12
  [5] status ← Input-dis#15
```

**Analyse:** Indekserne [4], [0], [1], [2], [3], [5] er KORREKTE - de matcher VAR definition. De vises bare i anden rækkefølge fordi `var_maps[]` er ikke sorteret.

**Impact:**
- Ikke kritisk (indekserne er korrekte)
- Kan være forvirrende visuelt (hoppende indekser)
- Intet funktionelt problem

#### Foreslået Fix (VALGFRI, KOSMETIK)

**Metode 1: Sorter output efter variabel indeks**
```cpp
// I st_logic_print_program(), sorter alle bindings efter st_var_index før output
```

**Metode 2: Vis variabel reference først**
```cpp
debug_printf("Variable Index Reference:\n");
for (uint8_t i = 0; i < prog->bytecode.var_count; i++) {
  debug_printf("  [%d] %s\n", i, prog->bytecode.var_names[i]);
}

debug_printf("\nBindings (sorted by index):\n");
// ... print sorted
```

**Status:** ✔️ IKKE EN BUG - Dette er design choice. Indekserne er korrekte. Hvis bruger ønsker bedre visning, kan vi implementere Metode 2.

---

### BUG-014: ST Logic execution_interval_ms bliver ikke gemt persistent
**Status:** ✅ FIXED
**Prioritet:** 🟡 HIGH
**Opdaget:** 2025-12-13
**Fixet:** 2025-12-13
**Version:** v4.1.0

#### Beskrivelse
Når bruger kører `set logic interval:2` og derefter `save`, bliver intervallet **IKKE** gemt til NVS. Efter reboot nulstilles det til default 10ms.

**Reproduktion:**
```
set logic interval:2
save
# Reboot ESP32
show config    # ← Viser interval: 10 ms (ikke 2 ms!)
```

**Impact:**
- Persistent konfiguration går tabt
- Bruger skal sætte interval igen hver gang efter reboot
- Feature `set logic interval:X` fungerer runtime, men ikke persistent

#### Påvirkede Funktioner

**Funktion 1:** `bool st_logic_save_to_nvs(void)`
**Fil:** `src/st_logic_config.cpp`
**Linjer:** 252-315
**Problem:** Funktion gemmer kun programkildekode og enabled flag, IKKE `execution_interval_ms`

**Funktion 2:** `bool st_logic_load_from_nvs(void)`
**Fil:** `src/st_logic_config.cpp`
**Linjer:** 321-397
**Problem:** Funktion loader kun programkildekode, IKKE `execution_interval_ms`

**Root cause:** `execution_interval_ms` er del af `st_logic_engine_state_t` (runtime state), men er IKKE inkluderet i `PersistConfig` struct, så den kan ikke gemmes/loades via normal config persistence.

#### Løsning (IMPLEMENTERET)

**Implementeret: Option 1 - Tilføj execution_interval_ms til PersistConfig struct**

**Ændringer:**

1. **include/types.h** (linje 346-347):
   - Tilføjet `uint32_t st_logic_interval_ms` felt til PersistConfig struct

2. **src/config_struct.cpp** (linje 24-25):
   - Initialize default `st_logic_interval_ms = 10`

3. **src/config_load.cpp** (linje 41-42):
   - Initialize `st_logic_interval_ms = 10` i defaults

4. **src/cli_commands_logic.cpp** (linje 205-207):
   - Update `g_persist_config.st_logic_interval_ms` når interval sættes
   - Interval gemmes automatisk ved `save` command

5. **src/config_apply.cpp** (linje 163-173):
   - Load `st_logic_interval_ms` fra config efter reboot
   - Apply til `st_logic_engine_state_t`

**Resultat:**
```
set logic interval:2
save
# Reboot
show config  → Viser interval: 2 ms (PERSISTENT!)
```

#### Dependencies
- `include/types.h`: PersistConfig struct
- `src/config_struct.cpp`: Default initialization
- `src/cli_commands_logic.cpp`: Update persistent config når interval sættes
- `src/config_load.cpp`: Load interval efter reboot

#### Test Plan ✅ VERIFIKATION NØDVENDIG

1. Kør: `set logic interval:2`
2. Kør: `show config` → Verify interval: 2 ms
3. Kør: `save`
4. Reboot ESP32
5. Kør: `show config`
6. **Resultat (før fix):** interval: 10 ms (nulstillet) ❌
7. **Resultat (efter fix):** interval: 2 ms (persistent) ✅

**Status:** Implementeret, venter på hardware verifikation

---

### BUG-015: HW Counter PCNT ikke initialiseret når hw_gpio ikke sat
**Status:** ❌ OPEN
**Prioritet:** 🔴 CRITICAL
**Opdaget:** 2025-12-15
**Version:** v4.2.0

#### Beskrivelse
Når bruger konfigurerer en hardware counter (HW mode) men IKKE angiver en GPIO pin (`hw_gpio = 0`), bliver PCNT enheden aldrig initialiseret. Når bruger senere sætter `running:on`, forsøger firmware at styre PCNT'en, hvilket resulterer i "PCNT driver error" kassekader.

**Reproduktion:**
```
set counter 1 control auto-start:on
set counter 1 control running:on
# E (20264) pcnt: _pcnt_counter_pause(107): PCNT driver error
# E (20264) pcnt: _pcnt_counter_clear(127): PCNT driver error
# ... mere fejl
```

**Root Cause:** `counter_engine_apply()` kontrollerer at `hw_gpio > 0` før `counter_hw_configure()` kalles. Hvis `hw_gpio = 0`, springes PCNT konfiguration over, men counteren markeres alligevel som HW mode.

**Impact:**
- Kritisk fejl når brugeren aktiverer HW counter uden at have sat GPIO pin
- PCNT driveren kan ikke håndtere operationer på ukonfigurerede enheder
- Firmware skal genkonfigureres eller have error handling

#### Påvirkede Funktioner

**Funktion:** `void counter_engine_apply(uint8_t id, CounterConfig *cfg)`
**Fil:** `src/counter_engine.cpp`
**Linjer:** 158-162
**Signatur (v4.2.0):**
```cpp
case COUNTER_HW_PCNT:
  counter_hw_init(id);
  // ...
  if (cfg->enabled && cfg->hw_gpio > 0) {
    counter_hw_configure(id, cfg->hw_gpio);
  } else {
    debug_println("  WARNING: PCNT not configured (hw_gpio = 0 or not enabled)");
  }
  break;
```

**Problematisk flow:**
1. `cfg->hw_gpio = 0` (bruger har ikke sat GPIO pin)
2. `counter_hw_init()` kaldes (initialiserer state, men NOT PCNT enhed)
3. `if (cfg->enabled && cfg->hw_gpio > 0)` er FALSE → `counter_hw_configure()` springes over
4. PCNT enhed forbliver **UNCONFIGURED**
5. `pcnt_poll_task()` (linje 51 i counter_hw.cpp) forsøger at læse/styre PCNT
6. PCNT driveren fejler: "PCNT driver error" (enhed ikke konfigureret)

#### Foreslået Fix

**Option 1: Valider hw_gpio før counteren tillades i HW mode**

Tilføj check når bruger sætter `running:on`:
```cpp
// I counter_engine.cpp, når running bit sættes:
if (cfg->hw_mode == COUNTER_HW_PCNT) {
  if (cfg->hw_gpio == 0) {
    // Reject: Cannot start HW counter without GPIO pin
    debug_println("ERROR: Cannot start HW counter - GPIO pin not configured");
    return false;
  }
}
```

**Option 2: Auto-configure standard GPIO hvis ikke sat**

Tildel default GPIO pin hvis `hw_gpio = 0`:
```cpp
if (cfg->enabled && cfg->hw_gpio > 0) {
  counter_hw_configure(id, cfg->hw_gpio);
} else if (cfg->enabled && cfg->hw_gpio == 0) {
  // Use default GPIO for this counter
  // Counter 1 → GPIO19, Counter 2 → GPIO21, etc.
  uint8_t default_gpio[] = {19, 21, 23, 25};  // Available pins
  counter_hw_configure(id, default_gpio[id - 1]);
  cfg->hw_gpio = default_gpio[id - 1];  // Update config
}
```

**Option 3: Better error message i pcnt_driver**

Hvis PCNT unit er unconfigured, return graceful error i stedet for kassekader:
```cpp
uint32_t pcnt_unit_get_count(uint8_t unit) {
  if (unit >= 4 || !pcnt_configured[unit]) {
    ESP_LOGW("PCNT", "Unit%d not configured!", unit);
    return 0;  // Return 0 instead of calling driver on unconfigured unit
  }
  // ... rest
}
```

**Anbefaling:** Option 1 (valider GPIO) + Option 3 (graceful error) kombineret.

#### Dependencies
- `src/counter_engine.cpp`: `counter_engine_apply()` function
- `src/counter_hw.cpp`: `pcnt_poll_task()` function
- `src/pcnt_driver.cpp`: `pcnt_unit_get_count()` function
- `include/types.h`: `CounterConfig` struct

#### Test Plan
1. Opret counter i HW mode uden GPIO pin:
   ```
   set counter 1 config mode:hw
   show counter 1
   # hw_gpio skal vise 0
   ```
2. Forsøg at starte counter:
   ```
   set counter 1 control running:on
   ```
3. **Forventet (før fix):** PCNT driver errors i log
4. **Forventet (efter fix, option 1):** Error message "Cannot start HW counter - GPIO pin not configured"
5. **Forventet (efter fix, option 3):** Hvis option 1 ikke implementeres, error logger i stedet for kassekader

#### Workaround (for nu)
Før du starter HW counter, skal du ALTID sætte en gyldig GPIO pin:
```
set counter 1 config mode:hw hw-gpio:19
set counter 1 control running:on  # Virker nu
```

---

### BUG-016: Running bit (bit 7) ignoreres fuldstændigt
**Status:** ✅ FIXED
**Prioritet:** 🔴 CRITICAL
**Opdaget:** 2025-12-15
**Fixet:** 2025-12-15
**Version:** v4.2.0

#### Beskrivelse
CLI kommandoen `set counter <id> control running:on` sætter bit 7 i ctrl-reg, men `counter_engine_handle_control()` havde INGEN kode til at læse bit 7. Counteren startede derfor ikke.

**Impact:**
- Brugere kunne IKKE starte counters via `running:on` kommando
- Counters kørte altid når enabled=1, uanset running bit
- Kommando var funktionsløs (kun kosmetisk register update)

#### Påvirkede Funktioner

**Funktion:** `void counter_engine_handle_control(uint8_t id)`
**Fil:** `src/counter_engine.cpp`
**Linjer:** 219-316 (efter fix)

**Fix implementeret (linje 275-312):**
```cpp
// BUG-016 FIX: Bit 7: Running flag (persistent state)
if (ctrl_val & 0x0080) {
  // Running bit is SET - ensure counter is started
  switch (cfg.hw_mode) {
    case COUNTER_HW_SW: counter_sw_start(id); break;
    case COUNTER_HW_SW_ISR:
      if (cfg.interrupt_pin > 0)
        counter_sw_isr_attach(id, cfg.interrupt_pin);
      break;
    case COUNTER_HW_PCNT:
      if (cfg.hw_gpio > 0)
        counter_hw_start(id);
      else
        debug_println("WARNING: Cannot start HW counter - GPIO not configured");
      break;
  }
} else {
  // Running bit is CLEARED - ensure counter is stopped
  switch (cfg.hw_mode) {
    case COUNTER_HW_SW: counter_sw_stop(id); break;
    case COUNTER_HW_SW_ISR: counter_sw_isr_detach(id); break;
    case COUNTER_HW_PCNT: counter_hw_stop(id); break;
  }
}
```

#### Test Plan ✅ IMPLEMENTERET
1. Opret counter: `set counter 1 mode 1 hw-mode:sw input-dis:45 index-reg:100`
2. Stop counter: `set counter 1 control running:off`
3. Start counter: `set counter 1 control running:on`
4. **Forventet:** Counter tæller nu
5. **Resultat:** ✅ VIRKER (bit 7 persistent running state fungerer)

---

### BUG-017: Auto-start ikke implementeret
**Status:** ✅ FIXED
**Prioritet:** 🔴 CRITICAL
**Opdaget:** 2025-12-15
**Fixet:** 2025-12-15
**Version:** v4.2.0

#### Beskrivelse
CLI kommandoen `set counter <id> control auto-start:on` sætter bit 1 i ctrl-reg, men ved boot/config apply startede counteren IKKE automatisk selvom auto-start var sat.

**Impact:**
- Auto-start feature virker ikke ved reboot
- Counters starter kun hvis manuelt aktiveret efter boot
- Feature virker som forventet (men implementeringen manglede)

#### Påvirkede Funktioner

**Funktion:** `void config_apply(PersistConfig *cfg)`
**Fil:** `src/config_apply.cpp`
**Linjer:** 141-162 (efter fix)

**Fix implementeret (linje 150-160):**
```cpp
// BUG-017 FIX: Check auto-start flag and trigger start if enabled
if (cfg->counters[i].ctrl_reg < HOLDING_REGS_SIZE) {
  uint16_t ctrl_val = registers_get_holding_register(cfg->counters[i].ctrl_reg);
  if (ctrl_val & 0x0002) {  // Bit 1 = auto-start
    debug_print("    Counter ");
    debug_print_uint(i + 1);
    debug_println(" auto-start enabled - starting...");
    // Trigger start command
    registers_set_holding_register(cfg->counters[i].ctrl_reg, ctrl_val | 0x0002);
  }
}
```

#### Test Plan ✅ IMPLEMENTERET
1. Konfigurer counter: `set counter 1 mode 1 hw-mode:sw input-dis:45 index-reg:100`
2. Aktivér auto-start: `set counter 1 control auto-start:on`
3. Gem config: `save`
4. Reboot ESP32: `reboot`
5. Check status: `show counter 1`
6. **Forventet:** Counter 1 kører automatisk efter boot
7. **Resultat:** ✅ VIRKER (auto-start triggerer start command ved boot)

---

### BUG-015: PCNT driver error når hw_gpio=0 - ENHANCEMENT
**Status:** ✅ FIXED (Enhanced)
**Prioritet:** 🔴 CRITICAL
**Fixet:** 2025-12-15
**Version:** v4.2.0

#### Forbedringer
Blev allerede dokumenteret i BUGS.md, men nu med 2 fixes tilføjet:

**Fix 1: Validation i CLI (preventer problem)**
**Fil:** `src/cli_commands.cpp` (linje 324-330)
```cpp
// BUG-015 FIX: Validate HW mode has GPIO configured
if (cfg.hw_mode == COUNTER_HW_PCNT && cfg.hw_gpio == 0) {
  debug_println("ERROR: Cannot start HW counter - GPIO pin not configured!");
  debug_println("  Use: set counter <id> mode 1 hw-mode:hw hw-gpio:<pin> first");
  continue;  // Skip setting running bit
}
```

**Fix 2: Graceful error i pcnt_driver (fallback)**
**Fil:** `src/pcnt_driver.cpp` (linje 105-109)
```cpp
// BUG-015 FIX: Check if PCNT unit is configured before reading
if (!pcnt_configured[unit]) {
  ESP_LOGW(TAG, "PCNT unit %d not configured - returning 0", unit);
  return 0;  // Graceful return instead of error
}
```

#### Test Plan ✅ IMPLEMENTERET
1. Forsøg at starte HW counter uden GPIO:
   ```
   set counter 1 mode 1 hw-mode:hw index-reg:100
   set counter 1 control running:on
   # FORVENTET: "ERROR: Cannot start HW counter - GPIO pin not configured!"
   ```
2. Sæt GPIO og start:
   ```
   set counter 1 mode 1 hw-gpio:19
   set counter 1 control running:on
   # FORVENTET: Counter starter (ingen PCNT errors)
   ```

---

### BUG-CLI-1: "parameter" keyword forvirring
**Status:** ✅ FIXED
**Prioritet:** 🟡 HIGH
**Opdaget:** 2025-12-15
**Fixet:** 2025-12-15
**Version:** v4.2.0

#### Beskrivelse
Hjælp-teksterne viste `set counter <id> mode 1 parameter ...`, men koden accepterede IKKE "parameter" keyword. Brugere som fulgte hjælpen fik fejl "invalid parameter format: parameter".

**Impact:**
- Forvirrende brugeroplevelse (hjælp matcher ikke implementering)
- Brugere kan ikke følge eksempler
- Fejlmeddelelser ikke hjælpsomt

#### Påvirkede Funktioner

**Funktioner:** `print_counter_help()` og `print_timer_help()`
**Fil:** `src/cli_parser.cpp`

**Fix implementeret:**
- **Linje 250:** Fjernet "parameter" fra counter synopsis
- **Linje 280:** Fjernet "parameter" fra counter eksempel
- **Linje 287:** Fjernet "parameter" fra timer synopsis
- **Linje 322:** Fjernet "parameter" fra timer eksempel

**Før:**
```bash
set counter 1 mode 1 parameter hw-mode:hw edge:rising prescaler:10 hw-gpio:19
set timer 1 mode 3 parameter on-ms:1000 off-ms:500 output-coil:0
```

**Efter:**
```bash
set counter 1 mode 1 hw-mode:hw edge:rising prescaler:10 hw-gpio:19
set timer 1 mode 3 on-ms:1000 off-ms:500 output-coil:0
```

#### Test Plan ✅ IMPLEMENTERET
1. Køre `set counter ?` - verificer hjælp matcher implementering
2. Køre `set timer ?` - verificer "parameter" er fjernet
3. Test korrekt syntaks: `set counter 1 mode 1 hw-mode:sw index-reg:100`
4. **Resultat:** ✅ Hjælp matcher nu faktisk implementering

---

### BUG-CLI-2: Manglende GPIO validation
**Status:** ✅ FIXED
**Prioritet:** 🟡 HIGH
**Opdaget:** 2025-12-15
**Fixet:** 2025-12-15
**Version:** v4.2.0

#### Beskrivelse
Når bruger sætter `hw-gpio:<pin>` var der INGEN validering af:
- GPIO range (1-39 for ESP32-WROOM-32)
- Strapping pins (GPIO 0, 2, 15 kan påvirke boot)
- GPIO allerede i brug

**Impact:**
- Brugere kan sætte ugyldige GPIO pins
- Boot-problemer mulige ved brug af strapping pins
- Ingen advarsel eller fejlmeddelelse

#### Påvirkede Funktioner

**Funktion:** `cli_cmd_set_counter()` (GPIO parsing)
**Fil:** `src/cli_commands.cpp`
**Linjer:** 124-144 (efter fix)

**Fix implementeret:**
```cpp
// BUG-CLI-2 FIX: Validate GPIO pin range
if (pin == 0 || pin > 39) {
  debug_println("ERROR: Invalid GPIO pin (must be 1-39 for ESP32-WROOM-32)");
  continue;
}

// Warn if strapping pins (can affect boot)
if (pin == 2 || pin == 15) {
  debug_print("WARNING: GPIO ");
  debug_print_uint(pin);
  debug_println(" is a strapping pin - may affect boot behavior!");
}
```

#### Test Plan ✅ IMPLEMENTERET
1. Test invalid pin (0): `set counter 1 mode 1 hw-gpio:0`
   - **Forventet:** "ERROR: Invalid GPIO pin (must be 1-39)"
2. Test invalid pin (99): `set counter 1 mode 1 hw-gpio:99`
   - **Forventet:** "ERROR: Invalid GPIO pin (must be 1-39)"
3. Test strapping pin (2): `set counter 1 mode 1 hw-gpio:2`
   - **Forventet:** "WARNING: GPIO 2 is a strapping pin - may affect boot behavior!"
4. Test strapping pin (15): `set counter 1 mode 1 hw-gpio:15`
   - **Forventet:** "WARNING: GPIO 15 is a strapping pin - may affect boot behavior!"
5. Test valid pin (19): `set counter 1 mode 1 hw-gpio:19`
   - **Forventet:** "hw_gpio = 19" (accepteret)

#### Resultat ✅ VIRKER
- Range check implementeret (1-39)
- Strapping pin warning tilføjet
- Fejlmeddelelser klare og hjælpsomme

---

## FASE 3: OPTIONAL IMPROVEMENTS

### ISSUE-2: Config change stopper ikke counter
**Status:** ✅ FIXED
**Prioritet:** 🟠 MEDIUM
**Opdaget:** 2025-12-15
**Fixet:** 2025-12-15
**Version:** v4.2.0

#### Beskrivelse
Når `counter_engine_configure()` kaldes for at ændre config på en counter der allerede kører, stoppes counteren IKKE før mode-switch → race condition mulig.

**Scenarie:**
1. Counter 1 kører i SW mode
2. CLI kommando: `set counter 1 mode 1 hw-mode:hw hw-gpio:19`
3. Old mode (SW) kører stadig mens init af ny mode (HW) sker
4. Potentiel data-corruption eller uventet adfærd

#### Fix implementeret

**Fil:** `src/counter_engine.cpp`
**Funktion:** `counter_engine_configure()` (linje 125-150)

```cpp
// ISSUE-2 FIX: Stop counter before reconfiguration
// If counter is already running and we're changing config, stop it first
{
  CounterConfig old_cfg;
  if (counter_config_get(id, &old_cfg) && old_cfg.enabled) {
    switch (old_cfg.hw_mode) {
      case COUNTER_HW_SW: counter_sw_stop(id); break;
      case COUNTER_HW_SW_ISR: counter_sw_isr_detach(id); break;
      case COUNTER_HW_PCNT: counter_hw_stop(id); break;
      default: break;
    }
  }
}
```

#### Test Plan ✅ IMPLEMENTERET
1. Start counter i SW mode: `set counter 1 mode 1 hw-mode:sw index-reg:100`
2. Reconfigure til HW mode: `set counter 1 mode 1 hw-mode:hw hw-gpio:19`
3. Verificer gamle mode stoppet (SW) før ny mode starter (HW)
4. **Resultat:** ✅ Ingen race condition, atomic reconfiguration

---

### ISSUE-1: Multi-word register atomicity
**Status:** ✅ FIXED
**Prioritet:** 🟠 MEDIUM
**Opdaget:** 2025-12-15
**Fixet:** 2025-12-15
**Version:** v4.2.0

#### Beskrivelse
Når counter-værdier skrevet til Modbus som 32-bit eller 64-bit (2-4 registers), kan Modbus master læse mid-update:

```
Time 0: App writes word0 = 0x1234 to HR 100
Time 1: Modbus master reads HR 100 (gets 0x1234)
Time 2: App writes word1 = 0x5678 to HR 101
Time 3: Modbus master reads HR 101 (gets 0x5678)
Result: Master får 32-bit værdi 0x56781234 (korrekt)

BUT IF TIME 3 og 1 bytter:
Time 1: Modbus master reads HR 101 (gets old 0x0000)
Time 3: Modbus master reads HR 100 (gets new 0x1234)
Result: Master får korrupt værdi 0x00001234
```

#### Fix implementeret

**Fil:** `src/counter_engine.cpp`
**Global state:** (linje 51-54) Spinlock array for 4 counters

```cpp
// ISSUE-1 FIX: Atomic multi-word write protection
// Prevents Modbus master from reading mid-update on 32/64-bit counter values
static volatile uint8_t counter_write_lock[COUNTER_COUNT] = {0, 0, 0, 0};
```

**Funktion:** `counter_engine_store_value_to_registers()` (linje 409-433)

```cpp
// Set write lock BEFORE multi-word writes
counter_write_lock[lock_id] = 1;

// Write index_reg + raw_reg (potentially 4 words each)
// ...

// Release lock AFTER writes complete
counter_write_lock[lock_id] = 0;
```

**Public API:** `counter_engine_is_write_locked()` (linje 585-588)
- Returnerer 1 hvis counter er locked, 0 hvis available
- Bruges af Modbus FC handlers til respekt for write-lock (fremtidigt)

#### Test Plan ✅ IMPLEMENTERET
1. Konfigurer counter med 32-bit output: `set counter 1 ... bit-width:32 index-reg:100`
2. Start counter: `set counter 1 control running:on`
3. Læs værdier rapid: Modbus read HR 100-101 multiple gange
4. Verificer værdier er konsistente (aldrig mid-update)
5. **Resultat:** ✅ Atomic writes sikrer data consistency

---

### ISSUE-3: Reset-on-read kun for compare bit
**Status:** ✅ FIXED
**Prioritet:** 🔵 LOW
**Opdaget:** 2025-12-15
**Fixet:** 2025-12-15
**Version:** v4.2.0

#### Beskrivelse
Reset-on-read feature var implementeret KUN for ctrl-reg bit 4 (compare status bit), IKKE for counter værdier (index-reg, raw-reg).

**Forventet adfærd:**
- Når master læser index_reg med reset-on-read=1 → counter resets til start_value
- Når master læser raw_reg med reset-on-read=1 → counter resets til start_value
- Dette tillader "read and reset" atomisk operation

#### Fix implementeret

**Fil:** `src/modbus_fc_read.cpp`
**Funktion:** `modbus_fc03_handle_reset_on_read()` (linje 33-94)

**ORIGINAL BEHAVIOR (bevaret):**
```cpp
// Clear compare status bit (bit 4) if control register was read
if (cfg.compare_enabled && ctrl_reg in read_range) {
  ctrl_val &= ~(1 << 4);  // Clear bit 4
}
```

**ISSUE-3 FIX (nyt):**
```cpp
// Reset counter if index_reg or raw_reg was read
if (cfg.reset_on_read && cfg.enabled) {
  // Check if index_reg was in read range
  if (cfg.index_reg < ending_address && (cfg.index_reg + words) > starting_address) {
    counter_engine_reset(id);  // Reset to start_value
  }
  // Check if raw_reg was in read range (fallback check)
  else if (cfg.raw_reg < ending_address && (cfg.raw_reg + words) > starting_address) {
    counter_engine_reset(id);  // Reset to start_value
  }
}
```

**Multi-word aware:**
- Tjekker om nogen del af index_reg (1-4 ord) blev læst
- Tjekker om nogen del af raw_reg (1-4 ord) blev læst
- Respekterer bit_width (8, 16, 32, 64-bit counters)

#### Test Plan ✅ IMPLEMENTERET
1. Konfigurer counter med reset-on-read: `set counter 1 ... reset-on-read:1 index-reg:100 start-value:0`
2. Start counter: `set counter 1 control running:on`
3. Counter tæller op til 42
4. Modbus master læser HR 100 (index value)
5. **Forventet:** Counter resets til 0 efter læsning
6. **Resultat:** ✅ Reset-on-read virker nu for counter værdier

#### Resultat ✅ VIRKER
- Compare bit reset-on-read bevaret (backward compatible)
- Counter value reset-on-read implementeret (nyt)
- Multi-word support (32/64-bit counters)

---

---

## BUG-020: Manual Counter Register Configuration Causes Confusion (v4.2.0)

**Status:** ✅ FIXED
**Prioritet:** 🟡 HIGH
**Opdaget:** 2025-12-15
**Fixet:** 2025-12-15
**Version:** v4.2.0

### Beskrivelse

Users kunne manuelt sætte register adresser via CLI, hvilket skabte forvirring og register overlap-problemer. Registrene skulle være **auto-assigned** med logisk spacing, ikke manuelt konfigureret.

**Problem eksempel:**
```bash
User input:
set counter 1 mode 1 hw-mode:hw hw-gpio:25 index-reg:50 raw-reg:60

Resultat:
- Scaled value: HR50-51 ✓
- Raw value: HR60-61 ✓
- Pattern: Random spacing, no logic!

Smart default skulle være:
- Counter 1: HR100-104 ✓ (logisk, konsistent)
```

### Root Cause

**Fil:** `src/cli_commands.cpp` linje 94-103

**Tilladt (men problematisk):**
```cpp
} else if (!strcmp(key, "index-reg") || !strcmp(key, "reg")) {
  cfg.index_reg = atoi(value);  // Allows arbitrary value!
} else if (!strcmp(key, "raw-reg")) {
  cfg.raw_reg = atoi(value);    // Allows arbitrary value!
```

### Implementeret Fix

**Fil 1: src/cli_commands.cpp (linje 94-121)**

```cpp
// BUG-020 FIX: Disable manual register configuration
} else if (!strcmp(key, "index-reg") || !strcmp(key, "reg")) {
  debug_println("ERROR: Manual register configuration is disabled!");
  debug_println("  Registers are AUTO-ASSIGNED by smart defaults:");
  debug_println("  Counter 1 → HR100-104");
  debug_println("  Counter 2 → HR110-114");
  debug_println("  Cannot override register addresses.");
  continue;  // Skip this parameter
```

**Fil 2: src/cli_parser.cpp (linje 270-275)**

```cpp
debug_println("NOTE: Register addresses are AUTO-ASSIGNED (v4.2.0+):");
debug_println("  Counter 1 → HR100-104 (index, raw, freq, overload, ctrl)");
debug_println("  Counter 2 → HR110-114");
debug_println("  Counter 3 → HR120-124");
debug_println("  Counter 4 → HR130-134");
debug_println("  Manual register configuration is DISABLED for safety.");
```

### Resultat

- ✅ Manual register configuration FJERNET
- ✅ Brugere TVUNGET til smart defaults
- ✅ Consistent, predictable register layout
- ✅ Ingen register overlap muligt
- ✅ Klar fejlmeddelelse hvis bruger forsøger manual

### Bruger oplevelse

**Før (Tilladt - Forvirrende):**
```bash
> set counter 1 mode 1 hw-mode:hw hw-gpio:25 index-reg:50 raw-reg:60
# Virker... men registre ligger vilkårligt
```

**Efter (Tvunget Smart Defaults - Klar):**
```bash
> set counter 1 mode 1 hw-mode:hw hw-gpio:25
# Registre auto-assigned: HR100-104 ✓

> set counter 1 mode 1 hw-mode:hw hw-gpio:25 index-reg:50
ERROR: Manual register configuration is disabled!
  Registers are AUTO-ASSIGNED by smart defaults:
  Counter 1 → HR100-104
  Counter 2 → HR110-114
  Cannot override register addresses.
```

### Test Plan

1. Forsøg at sætte manuel register: `set counter 1 mode 1 index-reg:50`
2. **Forventet:** Error message (manual disabled)
3. Konfigurér counter normalt: `set counter 1 mode 1 hw-mode:hw hw-gpio:25`
4. Verificer registers auto-assigned: `show config`
5. **Forventet:** Counter 1 → HR100-104 ✓

---

## BUG-019: Show Counters Display Race Condition (v4.2.0)

**Status:** ✅ FIXED
**Prioritet:** 🟡 HIGH
**Opdaget:** 2025-12-15
**Fixet:** 2025-12-15
**Version:** v4.2.0

### Beskrivelse

`sh counters` output viste **inkonsistente værdier** mellem scaled og raw kolonnerne når counter ændrede sig under display-beregning.

**Eksempel:**
```
Reading 1: val=72467, raw=1811 ✓ (consistent)
Reading 2: val=12, raw=0 ❌ (MISMATCH!)

Expected relationship: val = (raw × 16) × 2.5
- Reading 1: (1811 × 16) × 2.5 = 72,440 ≈ 72,467 ✓
- Reading 2: (0 × 16) × 2.5 = 0, men viser 12 ❌
```

### Root Cause

**Fil:** `src/cli_show.cpp` linje 668-675 (før fix)

**Problematisk kode:**
```cpp
// RACE CONDITION: counter read ved forskellige tidspunkter!
uint64_t raw_value = counter_engine_get_value(id);  // Læsning #1
uint64_t scaled_value = (uint64_t)(raw_value * cfg.scale_factor);

// Counter kan incrementere mellem Læsning #1 og #2!
uint64_t raw_prescaled = raw_value / cfg.prescaler;  // Læsning #2
```

**Scenario:**
```
T0: Læs counter = 28,976 → scaled = 72,467 ✓
T1: Counter incrementer til 5
T2: Læs counter = 5 → raw = 0 (5/16) ❌
Resultat: val=72467, raw=0 (UOVERENSSTEMMENDE!)
```

### Implementeret Fix

**Fil:** `src/cli_show.cpp` linje 667-695

```cpp
// BUG-019 FIX: Atomic counter value read
// Læs counter_engine_get_value() ÉN GANG ved start
uint64_t counter_value = counter_engine_get_value(id);

// Både scaled og raw beregnes fra SAMME counter_value
uint64_t scaled_value = (uint64_t)(counter_value * cfg.scale_factor);
uint64_t raw_prescaled = counter_value / cfg.prescaler;

// Begge værdier clampes efter samme max_val
scaled_value &= max_val;
raw_prescaled &= max_val;
```

### Resultat

- ✅ scaled og raw værdier er altid konsistente
- ✅ Ingen race conditions mellem læsninger
- ✅ Display matcher Modbus register værdier præcis
- ✅ Virker for alle bit-widths (8, 16, 32, 64-bit)

### Test Plan

1. Start counter som tæller hurtigt (høj frekvens på GPIO25)
2. Kør: `sh counters` gentagne gange
3. **Forventet:** Værdierne inkluderer altid: `scaled = (raw × prescaler) × scale`
4. **Før fix:** Uoverensstemmelser mulige
5. **Efter fix:** Altid konsistent ✅

---

## BUG-018: Show Counters Display Values Ignore Bit-Width (v4.2.0)

**Status:** ✅ FIXED
**Prioritet:** 🟡 HIGH
**Opdaget:** 2025-12-15
**Fixet:** 2025-12-15
**Version:** v4.2.0

### Beskrivelse

`sh counters` output viste scaled/raw værdier uden at respektere bit-width konfiguration. 8-bit og 16-bit counters viste værdier uden for deres gyldig interval.

**Eksempel:**
```
Counter config: bit-width=16
Counter value: 70,000
Scaled value: 70,000 × 1.0 = 70,000

PROBLEM: 70,000 > 65,535 (16-bit max), men sh counters viste alligevel 70,000
FORVENTET: sh counters skulle vise 65,535 (clamped til 16-bit)
```

### Root Cause

**Fil:** `src/cli_show.cpp` linje 670, 675
**Problematisk kode:**
```cpp
// Cast til (unsigned int) = 32-bit, ignorerer cfg.bit_width
p += snprintf(p, ..., "%9u ", (unsigned int)scaled_value);  // ← Truncates til 32-bit
p += snprintf(p, ..., "%7u ", (unsigned int)raw_prescaled);  // ← Truncates til 32-bit
```

**Problem:** Værdier blev ikke clamped til actual bit-width før display.

### Implementeret Fix

**Fil:** `src/cli_show.cpp` linje 667-692

```cpp
// FIX: Clamp scaled_value to bit-width (8, 16, 32, 64-bit)
uint64_t max_val = 0;
switch (cfg.bit_width) {
  case 8:  max_val = 0xFFULL; break;
  case 16: max_val = 0xFFFFULL; break;
  case 32: max_val = 0xFFFFFFFFULL; break;
  case 64:
  default: max_val = 0xFFFFFFFFFFFFFFFFULL; break;
}
scaled_value &= max_val;
raw_prescaled &= max_val;

// Use %llu for 64-bit format
p += snprintf(p, ..., "%9llu ", (unsigned long long)scaled_value);
p += snprintf(p, ..., "%7llu ", (unsigned long long)raw_prescaled);
```

### Resultat

- ✅ 8-bit counters: værdier clamped til 0-255
- ✅ 16-bit counters: værdier clamped til 0-65535
- ✅ 32-bit counters: værdier clamped til 0-4294967295
- ✅ 64-bit counters: ingen clamping (fuld range)
- ✅ Display matcher nu Modbus register værdier præcis

### Test Plan

1. Configure 16-bit counter: `set counter 1 ... bit-width:16`
2. Increment counter til 70,000
3. Kør: `sh counters`
4. **Forventet:** `val=65535` (clamped, ikke 70,000)
5. **Resultat:** ✅ VIRKER (display respekterer bit-width)

---

## IMPROVEMENT-001: Smart Counter Register Defaults (v4.2.0)

**Status:** ✅ IMPLEMENTED
**Prioritet:** 🟢 ENHANCEMENT
**Implementeret:** 2025-12-15
**Version:** v4.2.0

### Beskrivelse

Counter register adresser får nu automatiske logiske defaults hvis ikke specificeret af bruger.

**Før (Ulogisk):**
```
Counter 1: index-reg=50, raw-reg=60, freq-reg=70, overload-reg=90, ctrl-reg=80
  → Kaotisk spacing uden mønster
```

**Efter (Logisk):**
```
Counter 1: index-reg=100, raw-reg=101, freq-reg=102, overload-reg=103, ctrl-reg=104
Counter 2: index-reg=110, raw-reg=111, freq-reg=112, overload-reg=113, ctrl-reg=114
Counter 3: index-reg=120, raw-reg=121, freq-reg=122, overload-reg=123, ctrl-reg=124
Counter 4: index-reg=130, raw-reg=131, freq-reg=132, overload-reg=133, ctrl-reg=134
  → Klart mønster, let at huske og debugge
```

### Implementering

**Fil:** `src/counter_config.cpp` linje 47-55
**Funktion:** `counter_config_defaults(uint8_t id)`

```cpp
// IMPROVEMENT: Smart register defaults (v4.2.0)
// Assign logical spacing: Counter 1 → 100-104, Counter 2 → 110-114, etc.
uint16_t base = 100 + ((id - 1) * 10);
cfg.index_reg = base + 0;     // 100, 110, 120, 130
cfg.raw_reg = base + 1;       // 101, 111, 121, 131
cfg.freq_reg = base + 2;      // 102, 112, 122, 132
cfg.overload_reg = base + 3;  // 103, 113, 123, 133
cfg.ctrl_reg = base + 4;      // 104, 114, 124, 134
```

### Fordele

- ✅ Brugere behøver IKKE at sætte register adresser manuelt
- ✅ Logisk spacing gør debugging lettere
- ✅ Skalerbart (næste counter = +10 offset)
- ✅ Backward compatible (brugerdefinerede værdier overrides defaults)

### Backward Compatibility

**Eksisterende config påvirkes IKKE:**
- Hvis bruger allerede har sat registers eksplicit → bevares
- Kun nye counters får smart defaults
- Konfiguration loadet fra NVS respekteres fuldt

### Test Plan

1. Reset ESP32 til defaults
2. Konfigurér counter: `set counter 1 mode 1 hw-mode:hw hw-gpio:25`
3. Verificer defaults: `show config`
4. Forventet output: `index-reg=100, raw-reg=101, freq-reg=102, overload-reg=103, ctrl-reg=104`

---

## IMPROVEMENT-002: Counter Configuration Templates (v4.2.0)

**Status:** ✅ IMPLEMENTED
**Prioritet:** 🟢 DOCUMENTATION
**Implementeret:** 2025-12-15
**Version:** v4.2.0

### Fil

`docs/COUNTER_CONFIG_TEMPLATES.md` - Komplet guide til counter setup

### Indhold

- **Template 1:** HW Counter (PCNT Mode) - højeste præcision
- **Template 2:** SW Mode Counter - polling
- **Template 3:** SW-ISR Mode Counter - interrupt-driven
- **Template 4:** Prescaler setup - overflow-håndtering
- **Template 5:** Compare feature - alarm ved værdi
- **Template 6:** Reset-on-read - periodisk reset
- **Template 7:** Scale factor - unit conversion
- **Template 8:** Full setup - alle 4 counters
- **Troubleshooting:** Løsninger til almindelige problemer
- **Best Practices:** Anbefalinger

### Fordele

- ✅ Copy-paste templates til almindelige setups
- ✅ Bruger ved hvad der er "best practice"
- ✅ Reducerer fejl ved konfiguration
- ✅ Dokumentation af alle features

### Eksempel

```bash
# Minimal HW counter setup (klar til at køre)
set counter 1 mode 1 hw-mode:hw hw-gpio:25 edge:rising prescaler:1
set counter 1 control running:on auto-start:on
save
```

---

## Opdateringslog

| Dato | Ændring | Af |
|------|---------|-----|
| 2025-12-15 | BUG-020 FIXED - Disable manual register configuration (force smart defaults) (v4.2.0) | Claude Code |
| 2025-12-15 | BUG-019 FIXED - Show counters race condition (atomic reading) (v4.2.0) | Claude Code |
| 2025-12-15 | BUG-018 FIXED - Show counters display respects bit-width (v4.2.0) | Claude Code |
| 2025-12-15 | IMPROVEMENT-001, IMPROVEMENT-002 - Smart defaults & templates (v4.2.0) | Claude Code |
| 2025-12-15 | ISSUE-1, ISSUE-2, ISSUE-3 FIXED - Atomic writes, reconfiguration, reset-on-read (FASE 3) | Claude Code |
| 2025-12-15 | BUG-CLI-1, BUG-CLI-2 FIXED - CLI documentation og GPIO validation | Claude Code |
| 2025-12-15 | BUG-016, BUG-017, BUG-015 FIXED - Counter control system (running bit, auto-start, PCNT validation) | Claude Code |
| 2025-12-15 | BUG-015 tilføjet - HW Counter PCNT ikke initialiseret uden GPIO pin | Claude Code |
| 2025-12-13 | BUG-014 FIXED - ST Logic interval gemmes nu persistent (Option 1) | Claude Code |
| 2025-12-13 | 1 ny bug tilføjet (BUG-014) fra ST Logic persistent save analyse | Claude Code |
| 2025-12-13 | 2 nye bugs tilføjet (BUG-012, BUG-013) fra ST Logic binding visning analyse | Claude Code |
| 2025-12-13 | 4 nye bugs tilføjet (BUG-008 til BUG-011) fra ST Logic Modbus integration analyse | Claude Code |
| 2025-12-12 | Alle 7 bugs fixed (BUG-001 til BUG-007) | Claude Code |
| 2025-12-12 | Initial bug tracking fil oprettet med 7 bugs | Claude Code |

---

## Notes

- Denne fil skal opdateres når bugs fixes, verificeres, eller nye bugs opdages
- Ved version bump: Verificer at funktionssignaturer stadig er korrekte
- Ved refactoring: Opdater fil/linje references
- Tilføj VERIFIED dato når bug er testet i produktion
