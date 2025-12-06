/**
 * @file st_logic_engine.cpp
 * @brief Structured Text Logic Engine Implementation
 *
 * Main execution loop integrating ST bytecode with Modbus I/O.
 */

#include "st_logic_engine.h"
#include "st_compiler.h"
#include "st_parser.h"
#include "st_vm.h"
#include "config_struct.h"
#include "constants.h"
#include "debug.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * INPUT/OUTPUT OPERATIONS (MOVED TO gpio_mapping.cpp)
 *
 * NOTE: Variable I/O is now handled by the unified VariableMapping system
 * in gpio_mapping.cpp. This eliminates code duplication and provides
 * a single place to manage all variable bindings (GPIO + ST).
 *
 * The mapping engine:
 * - Reads variables from registers (INPUT mode) BEFORE program execution
 * - Writes variables to registers (OUTPUT mode) AFTER program execution
 * - Handles both GPIO pins and ST variables using the same mechanism
 * ============================================================================ */

/* ============================================================================
 * PROGRAM EXECUTION
 * ============================================================================ */

bool st_logic_execute_program(st_logic_engine_state_t *state, uint8_t program_id) {
  st_logic_program_config_t *prog = st_logic_get_program(state, program_id);
  if (!prog || !prog->compiled || !prog->enabled) return false;

  // Create VM and initialize with bytecode
  st_vm_t vm;
  st_vm_init(&vm, &prog->bytecode);

  // Execute until halt or error (max 10000 steps for safety)
  bool success = st_vm_run(&vm, 10000);

  // Copy variables back from VM to program config
  memcpy(prog->bytecode.variables, vm.variables, vm.var_count * sizeof(st_value_t));

  // Update statistics
  prog->execution_count++;
  if (!success || vm.error) {
    prog->error_count++;
    snprintf(prog->last_error, sizeof(prog->last_error), "%s", vm.error_msg);
    return false;
  }

  prog->last_execution_ms = 0;  // TODO: Add timestamp
  return true;
}

/* ============================================================================
 * MAIN LOGIC ENGINE LOOP
 *
 * EXECUTION ORDER:
 * 1. gpio_mapping_update() - Reads INPUT bindings (register→ST var)
 * 2. st_logic_engine_loop() - Executes all enabled programs
 * 3. gpio_mapping_update() - Writes OUTPUT bindings (ST var→register)
 *
 * This 3-phase approach ensures:
 * - Variables are synced BEFORE execution (fresh inputs)
 * - Variables are synced AFTER execution (outputs pushed to registers)
 * - All I/O goes through unified VariableMapping system
 * ============================================================================ */

bool st_logic_engine_loop(st_logic_engine_state_t *state,
                           uint16_t *holding_regs, uint16_t *input_regs) {
  if (!state || !state->enabled) return true;  // Logic mode disabled

  bool all_success = true;

  // Execute each program in sequence
  // NOTE: I/O is handled by gpio_mapping_update() in main loop, not here
  for (int prog_id = 0; prog_id < 4; prog_id++) {
    st_logic_program_config_t *prog = &state->programs[prog_id];

    if (!prog->enabled || !prog->compiled) continue;

    // Execute program bytecode
    bool success = st_logic_execute_program(state, prog_id);
    if (!success) {
      all_success = false;
      // Continue executing other programs despite error
    }
  }

  return all_success;
}

/* ============================================================================
 * DEBUGGING & STATUS
 * ============================================================================ */

void st_logic_print_status(st_logic_engine_state_t *state) {
  debug_printf("\n=== Logic Engine Status ===\n");
  debug_printf("Enabled: %s\n", state->enabled ? "YES" : "NO");
  debug_printf("Execution Interval: %ums\n", state->execution_interval_ms);
  debug_printf("\nPrograms:\n");

  for (int i = 0; i < 4; i++) {
    st_logic_program_config_t *prog = &state->programs[i];
    debug_printf("  %s:\n", prog->name);
    debug_printf("    Enabled: %s\n", prog->enabled ? "YES" : "NO");
    debug_printf("    Compiled: %s\n", prog->compiled ? "YES" : "NO");
    debug_printf("    Source: %d bytes\n", prog->source_size);
    debug_printf("    Executions: %u (errors: %u)\n", prog->execution_count, prog->error_count);
    if (prog->error_count > 0) {
      debug_printf("    Last error: %s\n", prog->last_error);
    }
  }

  debug_printf("\n");
}

void st_logic_print_program(st_logic_engine_state_t *state, uint8_t program_id) {
  st_logic_program_config_t *prog = st_logic_get_program(state, program_id);
  if (!prog) {
    debug_printf("Invalid program ID: %d\n", program_id);
    return;
  }

  debug_printf("\n=== Logic Program: %s ===\n", prog->name);
  debug_printf("Enabled: %s\n", prog->enabled ? "YES" : "NO");
  debug_printf("Compiled: %s\n", prog->compiled ? "YES" : "NO");
  debug_printf("Source Code: %d bytes\n", prog->source_size);

  if (prog->source_size > 0) {
    debug_printf("\nSource:\n");
    // Print first 500 chars of source
    int chars = (prog->source_size > 500) ? 500 : prog->source_size;
    debug_printf("%.*s\n", chars, prog->source_code);
    if (prog->source_size > 500) {
      debug_printf("... (%d more bytes)\n", prog->source_size - 500);
    }
  }

  debug_printf("\nVariable Bindings:\n");

  // Find and display all ST bindings for this program from g_persist_config
  extern PersistConfig g_persist_config;
  uint8_t binding_count = 0;

  for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
    VariableMapping *map = &g_persist_config.var_maps[i];

    // Check if this is an ST variable binding for this program
    if (map->source_type == MAPPING_SOURCE_ST_VAR &&
        map->st_program_id == program_id &&
        map->st_var_index < prog->bytecode.var_count) {

      binding_count++;
      const char *var_name = prog->bytecode.var_names[map->st_var_index];

      if (map->is_input) {
        debug_printf("  [%d] %s ← HR#%d (input)\n",
               map->st_var_index, var_name, map->input_reg);
      } else {
        debug_printf("  [%d] %s → Coil#%d (output)\n",
               map->st_var_index, var_name, map->coil_reg);
      }
    }
  }

  if (binding_count == 0) {
    debug_printf("  (No bindings configured)\n");
    debug_printf("  Syntax: set logic %d bind <var_name> reg:100|coil:10|input-dis:5\n", program_id + 1);
  } else {
    debug_printf("  Total: %d binding%s\n", binding_count, binding_count != 1 ? "s" : "");
  }

  debug_printf("\nStatistics:\n");
  debug_printf("  Executions: %u\n", prog->execution_count);
  debug_printf("  Errors: %u\n", prog->error_count);

  if (prog->compiled) {
    debug_printf("\nCompiled Bytecode: %d instructions\n", prog->bytecode.instr_count);
  }

  if (prog->error_count > 0) {
    debug_printf("\nLast Error: %s\n", prog->last_error);
  }

  debug_printf("\n");
}
