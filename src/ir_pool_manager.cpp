/**
 * @file ir_pool_manager.cpp
 * @brief IR Pool Manager Implementation
 *
 * v5.1.0 - Dynamic allocation of IR 220-251 for ST Logic EXPORT variables
 */

#include "ir_pool_manager.h"
#include "debug.h"
#include <string.h>

/* ============================================================================
 * SIZE CALCULATION
 * ============================================================================ */

uint8_t ir_pool_calculate_size(const st_bytecode_program_t *bytecode) {
  if (!bytecode) return 0;

  uint8_t size = 0;
  for (uint8_t i = 0; i < bytecode->var_count; i++) {
    if (bytecode->var_export_flags[i]) {
      // REAL and DINT use 2 registers, others use 1
      if (bytecode->var_types[i] == ST_TYPE_REAL ||
          bytecode->var_types[i] == ST_TYPE_DINT ||
          bytecode->var_types[i] == ST_TYPE_DWORD) {
        size += 2;
      } else {
        size += 1;
      }
    }
  }

  return size;
}

/* ============================================================================
 * POOL ALLOCATION
 * ============================================================================ */

uint8_t ir_pool_allocate(st_logic_engine_state_t *state, uint8_t program_id, uint8_t size_needed) {
  if (!state || program_id >= 4 || size_needed == 0 || size_needed > IR_POOL_SIZE) {
    return 255;  // Invalid parameters
  }

  // Find highest used offset across all programs
  uint8_t pool_used = 0;
  for (uint8_t i = 0; i < 4; i++) {
    st_logic_program_config_t *prog = &state->programs[i];
    if (prog->ir_pool_offset != 65535) {
      uint8_t end = prog->ir_pool_offset + prog->ir_pool_size;
      if (end > pool_used) {
        pool_used = end;
      }
    }
  }

  // Check if there's enough space
  if (pool_used + size_needed > IR_POOL_SIZE) {
    debug_printf("[IR_POOL] Allocation failed: need %d regs, only %d free\n",
                 size_needed, IR_POOL_SIZE - pool_used);
    return 255;  // Pool exhausted
  }

  // Allocate at end of used space
  uint8_t offset = pool_used;
  state->programs[program_id].ir_pool_offset = offset;
  state->programs[program_id].ir_pool_size = size_needed;

  debug_printf("[IR_POOL] Allocated Logic%d: IR %d-%d (%d regs)\n",
               program_id + 1, 220 + offset, 220 + offset + size_needed - 1, size_needed);

  return offset;
}

/* ============================================================================
 * POOL DEALLOCATION
 * ============================================================================ */

void ir_pool_free(st_logic_engine_state_t *state, uint8_t program_id) {
  if (!state || program_id >= 4) return;

  st_logic_program_config_t *prog = &state->programs[program_id];
  if (prog->ir_pool_offset == 65535) {
    return;  // Not allocated
  }

  debug_printf("[IR_POOL] Freed Logic%d: IR %d-%d (%d regs)\n",
               program_id + 1, 220 + prog->ir_pool_offset,
               220 + prog->ir_pool_offset + prog->ir_pool_size - 1,
               prog->ir_pool_size);

  prog->ir_pool_offset = 65535;  // Mark as not allocated
  prog->ir_pool_size = 0;
}

/* ============================================================================
 * POOL QUERIES
 * ============================================================================ */

uint8_t ir_pool_get_total_used(const st_logic_engine_state_t *state) {
  if (!state) return 0;

  uint8_t total = 0;
  for (uint8_t i = 0; i < 4; i++) {
    const st_logic_program_config_t *prog = &state->programs[i];
    if (prog->ir_pool_offset != 65535) {
      total += prog->ir_pool_size;
    }
  }

  return total;
}

uint8_t ir_pool_get_free_space(const st_logic_engine_state_t *state) {
  return IR_POOL_SIZE - ir_pool_get_total_used(state);
}

/* ============================================================================
 * POOL COMPACTION (OPTIONAL)
 * ============================================================================ */

void ir_pool_compact(st_logic_engine_state_t *state) {
  if (!state) return;

  // Build list of allocated programs sorted by offset
  struct {
    uint8_t program_id;
    uint16_t offset;
    uint8_t size;
  } allocations[4];
  uint8_t alloc_count = 0;

  for (uint8_t i = 0; i < 4; i++) {
    if (state->programs[i].ir_pool_offset != 65535) {
      allocations[alloc_count].program_id = i;
      allocations[alloc_count].offset = state->programs[i].ir_pool_offset;
      allocations[alloc_count].size = state->programs[i].ir_pool_size;
      alloc_count++;
    }
  }

  // Simple bubble sort by offset
  for (uint8_t i = 0; i < alloc_count - 1; i++) {
    for (uint8_t j = 0; j < alloc_count - i - 1; j++) {
      if (allocations[j].offset > allocations[j + 1].offset) {
        auto temp = allocations[j];
        allocations[j] = allocations[j + 1];
        allocations[j + 1] = temp;
      }
    }
  }

  // Reassign offsets sequentially
  uint8_t new_offset = 0;
  for (uint8_t i = 0; i < alloc_count; i++) {
    uint8_t prog_id = allocations[i].program_id;
    state->programs[prog_id].ir_pool_offset = new_offset;
    new_offset += allocations[i].size;

    debug_printf("[IR_POOL] Compacted Logic%d: IR %d-%d\n",
                 prog_id + 1, 220 + state->programs[prog_id].ir_pool_offset,
                 220 + state->programs[prog_id].ir_pool_offset + state->programs[prog_id].ir_pool_size - 1);
  }
}

/* ============================================================================
 * POOL INITIALIZATION
 * ============================================================================ */

void ir_pool_init(st_logic_engine_state_t *state) {
  if (!state) return;

  for (uint8_t i = 0; i < 4; i++) {
    state->programs[i].ir_pool_offset = 65535;  // Not allocated
    state->programs[i].ir_pool_size = 0;
  }

  debug_println("[IR_POOL] Initialized - all programs unallocated");
}

/* ============================================================================
 * POOL REALLOCATION (POST-LOAD)
 * ============================================================================ */

void ir_pool_reallocate_all(st_logic_engine_state_t *state) {
  if (!state) return;

  debug_println("[IR_POOL] Reallocating after NVS load...");

  // Free all allocations first
  for (uint8_t i = 0; i < 4; i++) {
    state->programs[i].ir_pool_offset = 65535;
    state->programs[i].ir_pool_size = 0;
  }

  // Reallocate for each compiled program
  for (uint8_t i = 0; i < 4; i++) {
    st_logic_program_config_t *prog = &state->programs[i];
    if (!prog->compiled) continue;

    // Calculate required size
    uint8_t ir_size_needed = ir_pool_calculate_size(&prog->bytecode);
    if (ir_size_needed > 0) {
      uint8_t offset = ir_pool_allocate(state, i, ir_size_needed);
      if (offset == 255) {
        debug_printf("[WARN] Logic%d: IR pool exhausted on reload\n", i + 1);
      }
    }
  }

  debug_printf("[IR_POOL] Reallocation complete: %d/%d regs used\n",
               ir_pool_get_total_used(state), IR_POOL_SIZE);
}
