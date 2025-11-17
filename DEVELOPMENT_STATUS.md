# Development Status - Register/Coil STATIC/DYNAMIC Configuration

**Last Updated:** 2025-11-17
**Status:** ✅ Core Implementation Complete - Build Successful

---

## What Was Implemented

### 1. Data Structures (types.h, constants.h)
- ✅ `StaticRegisterMapping` - Maps register address → fixed value
- ✅ `DynamicRegisterMapping` - Maps register address → counter/timer source
- ✅ `StaticCoilMapping` - Maps coil address → fixed ON/OFF
- ✅ `DynamicCoilMapping` - Maps coil address → counter/timer source
- ✅ Enums: `DynamicSourceType`, `CounterFunction`, `TimerFunction`
- ✅ Updated `PersistConfig` to include static/dynamic mapping arrays
- ✅ Schema version bumped to 2

### 2. CLI Commands (cli_config_regs.cpp, cli_config_coils.cpp)
**Register commands:**
```
set reg STATIC 0 Value 42
set reg DYNAMIC 100 counter1:index
set reg DYNAMIC 101 counter1:raw
set reg DYNAMIC 102 counter1:freq
set reg DYNAMIC 103 timer2:output
show reg
```

**Coil commands:**
```
set coil STATIC 5 Value ON
set coil STATIC 6 Value OFF
set coil DYNAMIC 10 counter1:overflow
set coil DYNAMIC 15 timer2:output
show coil
```

### 3. Dynamic Value Updates (registers.cpp)
- ✅ `registers_update_dynamic_registers()` - Syncs DYNAMIC registers each loop
- ✅ `registers_update_dynamic_coils()` - Syncs DYNAMIC coils each loop
- ✅ Counter functions: index, raw, freq, overflow, ctrl
- ✅ Timer functions: output

### 4. Global Configuration (config_struct.cpp/h)
- ✅ `g_persist_config` - Global persistent config structure
- ✅ Accessible from all modules via `#include "config_struct.h"`
- ✅ Properly exported as `extern` in header

### 5. CLI Parser Updates (cli_parser.cpp)
- ✅ Dispatches `set reg STATIC/DYNAMIC` to new handlers
- ✅ Dispatches `set coil STATIC/DYNAMIC` to new handlers
- ✅ Added `show reg` and `show coil` commands
- ✅ Updated HELP text with new commands

---

## Build Status
✅ **SUCCESS** - Compiles without errors
- RAM: 7.6% used (24800 / 327680 bytes)
- Flash: 23.0% used (300993 / 1310720 bytes)

---

## What Still Needs To Be Done

### 1. Integrate Dynamic Updates into Main Loop
**File:** `src/main.cpp`
**Task:** Call `registers_update_dynamic_registers()` and `registers_update_dynamic_coils()` once per loop iteration

**Pseudocode:**
```cpp
void loop() {
  // ... existing code ...

  // Sync DYNAMIC register/coil values from counter/timer sources
  registers_update_dynamic_registers();
  registers_update_dynamic_coils();

  // ... rest of loop ...
}
```

### 2. Implement Counter/Timer Value Accessors
**Files:** `src/counter_engine.cpp`, `src/timer_engine.cpp`
**Task:** Implement functions to get overflow flag and output state:

```cpp
// In counter_engine.cpp:
bool counter_engine_get_overflow_flag(uint8_t id);
uint32_t counter_engine_get_frequency(uint8_t id);

// In timer_engine.cpp:
uint8_t timer_engine_get_output_state(uint8_t id);
```

**Usage in registers.cpp:**
```cpp
case COUNTER_FUNC_OVERFLOW:
  value = counter_engine_get_overflow_flag(counter_id) ? 1 : 0;
  break;

case COUNTER_FUNC_FREQ:
  value = counter_engine_get_frequency(counter_id);
  break;

case TIMER_FUNC_OUTPUT:
  value = timer_engine_get_output_state(timer_id);
  break;
```

### 3. Config Load/Save (NVS Integration)
**Files:** `src/config_load.cpp`, `src/config_save.cpp`
**Task:** Implement NVS read/write for STATIC and DYNAMIC mappings

**Schema migration needed:**
- Old format (v1): Only held counter/timer configs
- New format (v2): Also holds register/coil mappings
- Migration path: Load v1 → convert → save as v2

### 4. STATIC Register/Coil Auto-Apply
**File:** `src/config_apply.cpp`
**Task:** When config is loaded, auto-populate STATIC registers and coils with their saved values

**Pseudocode:**
```cpp
void config_apply(...) {
  // ... existing code ...

  // Apply STATIC register values
  for (uint8_t i = 0; i < config->static_reg_count; i++) {
    registers_set_holding_register(
      config->static_regs[i].register_address,
      config->static_regs[i].static_value
    );
  }

  // Apply STATIC coil values
  for (uint8_t i = 0; i < config->static_coil_count; i++) {
    registers_set_coil(
      config->static_coils[i].coil_address,
      config->static_coils[i].static_value
    );
  }
}
```

---

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│           REGISTER/COIL SYSTEM                  │
├─────────────────────────────────────────────────┤
│                                                 │
│  CLI Commands          Configuration Storage   │
│  ──────────────        ───────────────────     │
│  set reg STATIC  ─────→ g_persist_config      │
│  set reg DYNAMIC ─────→ .static_regs[]        │
│  set coil STATIC ─────→ .static_coils[]       │
│  set coil DYNAMIC ────→ .dynamic_regs[]       │
│                        .dynamic_coils[]       │
│                                                 │
│        ↓ registers_update_dynamic_registers()  │
│        ↓ registers_update_dynamic_coils()      │
│                                                 │
│  Counter/Timer State                           │
│  ────────────────────                          │
│  counter_engine_get_value() ──→ index-reg     │
│  counter_engine_get_overflow() ─→ overflow-reg│
│  timer_engine_get_output() ────→ output-coil  │
│                                                 │
│        ↓ Modbus Read/Write                     │
│                                                 │
│  Holding Registers / Coils (FC03/FC05/FC06)   │
│                                                 │
└─────────────────────────────────────────────────┘
```

---

## Files Modified
- ✅ `include/constants.h` - Added enums and defines
- ✅ `include/types.h` - Added mapping structs
- ✅ `include/registers.h` - Added DYNAMIC update functions
- ✅ `include/config_struct.h` - Exported g_persist_config
- ✅ `include/cli_config_regs.h` - New file
- ✅ `include/cli_config_coils.h` - New file
- ✅ `src/registers.cpp` - Implemented DYNAMIC updates
- ✅ `src/config_struct.cpp` - Created g_persist_config
- ✅ `src/cli_parser.cpp` - Added command dispatching
- ✅ `src/cli_config_regs.cpp` - New file
- ✅ `src/cli_config_coils.cpp` - New file

---

## Testing Recommendations

### Manual CLI Testing
```
# Set STATIC values
set reg STATIC 0 Value 42
set reg STATIC 1 Value 100
set coil STATIC 5 Value ON
set coil STATIC 6 Value OFF

# View configuration
show reg
show coil

# Set DYNAMIC mappings (after counters configured)
set counter 1 mode 1 parameter hw-mode:sw edge:rising prescaler:1 index-reg:100 raw-reg:101 freq-reg:102
set reg DYNAMIC 100 counter1:index
set reg DYNAMIC 101 counter1:raw
set reg DYNAMIC 102 counter1:freq

# Verify dynamic updates
show registers 100 3    # Should show changing values
```

### Functional Testing
1. Configure STATIC registers/coils and verify they persist after save/load
2. Configure counter, then set DYNAMIC register → verify register updates in real-time
3. Configure timer output to DYNAMIC coil → verify coil changes as timer state changes
4. Test schema migration from v1 → v2 config

---

## Next Session Checklist

- [ ] Integrate `registers_update_dynamic_*()` calls in main loop
- [ ] Implement counter/timer accessor functions
- [ ] Implement NVS config load/save with schema migration
- [ ] Test STATIC register/coil persistence
- [ ] Test DYNAMIC register updates from counters
- [ ] Test DYNAMIC coil updates from timers
- [ ] Verify Modbus read/write works with DYNAMIC values
- [ ] Update CLAUDE.md with configuration examples

---

**Status:** Ready for next development phase. Core architecture in place, build successful.
