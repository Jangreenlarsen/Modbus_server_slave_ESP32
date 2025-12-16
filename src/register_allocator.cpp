/**
 * @file register_allocator.cpp
 * @brief Global register allocation tracker (LAYER 4.5)
 *
 * Prevents register conflicts between Counter, Timer, and ST Logic subsystems.
 *
 * (NEW in v4.2.0 - BUG-025 fix)
 */

#include "register_allocator.h"
#include "constants.h"
#include "types.h"
#include "counter_config.h"
#include "timer_config.h"
#include "timer_engine.h"
#include "gpio_mapping.h"
#include "debug.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * GLOBAL STATE
 * ============================================================================ */

// Global allocation map - tracks which subsystem owns each register
// Note: Only track registers 0-99 (main allocation zone)
// Registers 100-159 for future expansion (can track later if needed)
// RegisterOwner size: 1+1+16+4 = 22 bytes
// Total: 100*22 = 2200 bytes in DRAM (minimal footprint)
#define ALLOCATOR_SIZE 100

// Allocate in DRAM (initialized at boot)
static RegisterOwner allocation_map[ALLOCATOR_SIZE];

// Track if allocator is initialized
static bool allocator_initialized = false;

/* ============================================================================
 * PUBLIC API IMPLEMENTATION
 * ============================================================================ */

void register_allocator_init(void) {
  if (allocator_initialized) {
    return;  // Already initialized
  }

  // 1. Mark all registers as free
  memset(allocation_map, 0, sizeof(allocation_map));

  // 2. Pre-allocate ST Logic fixed registers (200-293)
  // These are READ-ONLY system registers, always reserved
  // Note: We don't track 200-293 in allocation map (only track 0-99)
  // ST Logic registers are protected at a higher level (cli_commands_logic.cpp)

  // 3. Pre-allocate counter smart defaults (if enabled)
  for (uint8_t id = 1; id <= 4; id++) {
    CounterConfig cfg;
    if (counter_config_get(id, &cfg) && cfg.enabled) {
      // Allocate all 5 counter registers
      register_allocator_allocate(cfg.index_reg, REG_OWNER_COUNTER, id, "index-reg");
      register_allocator_allocate(cfg.raw_reg, REG_OWNER_COUNTER, id, "raw-reg");
      register_allocator_allocate(cfg.freq_reg, REG_OWNER_COUNTER, id, "freq-reg");
      register_allocator_allocate(cfg.overload_reg, REG_OWNER_COUNTER, id, "overload-reg");
      register_allocator_allocate(cfg.ctrl_reg, REG_OWNER_COUNTER, id, "ctrl-reg");
    }
  }

  // 4. Pre-allocate timer ctrl-regs (if configured)
  for (uint8_t id = 1; id <= 4; id++) {
    TimerConfig cfg;
    if (timer_config_get(id, &cfg) && cfg.enabled && cfg.ctrl_reg < HOLDING_REGS_SIZE) {
      char desc[32];
      snprintf(desc, sizeof(desc), "Timer %d ctrl-reg", id);
      register_allocator_allocate(cfg.ctrl_reg, REG_OWNER_TIMER, id, desc);
    }
  }

  // 5. Pre-allocate ST variable bindings (from VariableMapping array)
  // BUG FIX: Must be done here at boot to prevent conflicts with counters
  // Register bindings from persistent config before system fully starts
  extern PersistConfig g_persist_config;
  for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
    VariableMapping *map = &g_persist_config.var_maps[i];

    // Only allocate holding register bindings (input_type=0 or output_type=0)
    if (map->source_type == MAPPING_SOURCE_ST_VAR) {
      // Check INPUT bindings
      if (map->is_input && map->input_type == 0 && map->input_reg < 100) {
        char desc[32];
        snprintf(desc, sizeof(desc), "Logic%d var%d (input)",
                 map->st_program_id + 1, map->st_var_index);
        register_allocator_allocate(map->input_reg, REG_OWNER_ST_VAR,
                                   map->st_program_id + 1, desc);
      }

      // Check OUTPUT bindings (only allocate if different from input)
      if (!map->is_input && map->output_type == 0 && map->coil_reg < 100) {
        char desc[32];
        snprintf(desc, sizeof(desc), "Logic%d var%d (output)",
                 map->st_program_id + 1, map->st_var_index);
        register_allocator_allocate(map->coil_reg, REG_OWNER_ST_VAR,
                                   map->st_program_id + 1, desc);
      }
    }
  }

  allocator_initialized = true;

  debug_println("[ALLOCATOR] Register allocator initialized");
}

bool register_allocator_check(uint16_t reg_addr, RegisterOwner* owner) {
  if (reg_addr >= ALLOCATOR_SIZE) {
    return false;  // Out of bounds
  }

  RegisterOwner* reg_owner = &allocation_map[reg_addr];

  if (owner != NULL) {
    *owner = *reg_owner;
  }

  return (reg_owner->type == REG_OWNER_NONE);  // true if free
}

bool register_allocator_allocate(uint16_t reg_addr, RegisterOwnerType type,
                                 uint8_t subsystem_id, const char* description) {
  if (reg_addr >= ALLOCATOR_SIZE) {
    debug_println("[ALLOCATOR] ERROR: Register address out of bounds");
    return false;
  }

  RegisterOwner* owner = &allocation_map[reg_addr];

  // Check if already allocated
  if (owner->type != REG_OWNER_NONE) {
    debug_print("[ALLOCATOR] ERROR: HR");
    debug_print_uint(reg_addr);
    debug_print(" already allocated to ");
    debug_print(owner->description);
    debug_println("");
    return false;
  }

  // Allocate
  owner->type = type;
  owner->subsystem_id = subsystem_id;
  owner->timestamp = 0;  // Could use millis() for debugging

  if (description != NULL) {
    strncpy(owner->description, description, sizeof(owner->description) - 1);
    owner->description[sizeof(owner->description) - 1] = '\0';
  } else {
    owner->description[0] = '\0';
  }

  return true;
}

void register_allocator_free(uint16_t reg_addr) {
  if (reg_addr >= ALLOCATOR_SIZE) {
    return;
  }

  RegisterOwner* owner = &allocation_map[reg_addr];

  if (owner->type != REG_OWNER_NONE) {
    debug_print("[ALLOCATOR] Freed HR");
    debug_print_uint(reg_addr);
    debug_print(" (was ");
    debug_print(owner->description);
    debug_println(")");
  }

  memset(owner, 0, sizeof(RegisterOwner));
}

uint16_t register_allocator_find_free(uint16_t start, uint16_t end) {
  if (start >= ALLOCATOR_SIZE || end >= ALLOCATOR_SIZE) {
    return 0xFFFF;  // Invalid range
  }

  for (uint16_t i = start; i <= end; i++) {
    if (allocation_map[i].type == REG_OWNER_NONE) {
      return i;
    }
  }

  return 0xFFFF;  // No free register found
}

const RegisterOwner* register_allocator_get(uint16_t reg_addr) {
  if (reg_addr >= ALLOCATOR_SIZE) {
    return NULL;
  }

  return &allocation_map[reg_addr];
}

bool register_allocator_allocate_range(uint16_t start_addr, uint8_t count,
                                       RegisterOwnerType type, uint8_t subsystem_id,
                                       const char* description) {
  if (start_addr >= ALLOCATOR_SIZE || start_addr + count > ALLOCATOR_SIZE) {
    return false;  // Out of bounds
  }

  // First check if all registers are free
  for (uint8_t i = 0; i < count; i++) {
    if (allocation_map[start_addr + i].type != REG_OWNER_NONE) {
      return false;  // Conflict found
    }
  }

  // Allocate all
  for (uint8_t i = 0; i < count; i++) {
    register_allocator_allocate(start_addr + i, type, subsystem_id, description);
  }

  return true;
}

void register_allocator_free_range(uint16_t start_addr, uint8_t count) {
  if (start_addr >= ALLOCATOR_SIZE || start_addr + count > ALLOCATOR_SIZE) {
    return;
  }

  for (uint8_t i = 0; i < count; i++) {
    register_allocator_free(start_addr + i);
  }
}
