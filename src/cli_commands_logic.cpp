/**
 * @file cli_commands_logic.cpp
 * @brief CLI Commands for Structured Text Logic Mode (Fase 4)
 *
 * Commands for managing logic programs:
 *   set logic <id> upload <code>
 *   set logic <id> enabled:true|false
 *   set logic <id> delete
 *   set logic <id> bind <var_index> <register> [input|output|both]
 *   show logic <id>
 *   show logic all
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "debug.h"

/* ST Logic Engine includes */
#include "st_logic_config.h"
#include "st_logic_engine.h"
#include "st_compiler.h"

/* Config & Mapping includes */
#include "config_struct.h"
#include "constants.h"

/* Forward declarations - from existing CLI infrastructure */
extern void debug_println(const char *msg);

/* Forward declarations - config persistence */
extern bool config_save_to_nvs(const PersistConfig* cfg);

/* Forward declaration for bind function */
int cli_cmd_set_logic_bind(st_logic_engine_state_t *logic_state, uint8_t program_id,
                           uint8_t var_index, uint16_t modbus_reg, const char *direction, uint8_t input_type, uint8_t output_type);

/* ============================================================================
 * COMMAND HANDLERS
 * ============================================================================ */

/**
 * @brief set logic <id> upload "<st_code>"
 *
 * Upload ST source code for a logic program
 *
 * Example:
 *   set logic 1 upload "VAR x: INT; END_VAR IF x > 10 THEN x := 1; END_IF;"
 */
int cli_cmd_set_logic_upload(st_logic_engine_state_t *logic_state, uint8_t program_id,
                             const char *source_code) {
  if (program_id >= 4) {
    debug_println("ERROR: Invalid program ID (0-3)");
    return -1;
  }

  if (!source_code || strlen(source_code) == 0) {
    debug_println("ERROR: Source code cannot be empty");
    return -1;
  }

  // Upload source
  if (!st_logic_upload(logic_state, program_id, source_code, strlen(source_code))) {
    debug_println("ERROR: Failed to upload source code");
    return -1;
  }

  // Try to compile immediately
  st_logic_program_config_t *prog_before = st_logic_get_program(logic_state, program_id);
  uint8_t compiled_before = prog_before ? prog_before->compiled : 0;

  if (!st_logic_compile(logic_state, program_id)) {
    st_logic_program_config_t *prog = st_logic_get_program(logic_state, program_id);

    // Better error output
    debug_println("");
    debug_println("╔════════════════════════════════════════════════════════╗");
    debug_println("║            COMPILATION ERROR - Logic Program          ║");
    debug_println("╚════════════════════════════════════════════════════════╝");
    debug_printf("Program ID: Logic%d\n", program_id + 1);
    debug_printf("Error: %s\n", prog->last_error);
    debug_printf("Source size: %d bytes\n", prog->source_size);
    debug_println("");

    return -1;
  }

  st_logic_program_config_t *prog = st_logic_get_program(logic_state, program_id);
  uint8_t compiled_after = prog ? prog->compiled : 0;

  // Success output
  debug_println("");
  debug_println("✓ COMPILATION SUCCESSFUL");
  debug_printf("  Program: Logic%d\n", program_id + 1);
  debug_printf("  Source: %d bytes\n", strlen(source_code));
  debug_printf("  Bytecode: %d instructions\n", prog->bytecode.instr_count);
  debug_printf("  Variables: %d\n", prog->bytecode.var_count);
  debug_printf("  [DEBUG] compiled: %d → %d\n", compiled_before, compiled_after);
  debug_println("");

  // Debug: Print bytecode instructions (if debug mode enabled)
  if (logic_state && logic_state->debug) {
    st_bytecode_print(&prog->bytecode);
  }

  return 0;
}

/**
 * @brief set logic <id> enabled:true|false
 *
 * Enable or disable a logic program
 *
 * Example:
 *   set logic 1 enabled:true
 */
int cli_cmd_set_logic_enabled(st_logic_engine_state_t *logic_state, uint8_t program_id,
                              bool enabled) {
  if (program_id >= 4) {
    debug_println("ERROR: Invalid program ID (0-3)");
    return -1;
  }

  st_logic_program_config_t *prog = st_logic_get_program(logic_state, program_id);
  if (logic_state && logic_state->debug) {
    debug_printf("[ENABLED_DEBUG] program_id=%d, prog=%p, compiled=%d\n",
                 program_id, prog, prog ? prog->compiled : -1);
  }

  if (!prog->compiled) {
    debug_println("ERROR: Program not compiled. Upload source code first.");
    return -1;
  }

  if (!st_logic_set_enabled(logic_state, program_id, enabled)) {
    debug_println("ERROR: Failed to set enabled state");
    return -1;
  }

  debug_printf("[OK] Logic%d %s\n", program_id + 1, enabled ? "ENABLED" : "DISABLED");
  return 0;
}

/**
 * @brief set logic debug:true|false
 *
 * Enable/disable debug output for ST Logic (bytecode printing, execution trace, etc.)
 *
 * Example:
 *   set logic debug:true
 *   set logic debug:false
 */
int cli_cmd_set_logic_debug(st_logic_engine_state_t *logic_state, bool debug) {
  if (!logic_state) {
    debug_println("ERROR: Logic state not initialized");
    return -1;
  }

  logic_state->debug = debug ? 1 : 0;
  debug_printf("[OK] ST Logic debug %s\n", debug ? "ENABLED" : "DISABLED");
  return 0;
}

/**
 * @brief set logic interval:X
 *
 * Set global execution interval for all ST Logic programs
 *
 * Allowed values: 10, 20, 25, 50, 75, 100 ms
 *
 * Example:
 *   set logic interval:10
 *   set logic interval:50
 *
 * @param logic_state Logic engine state
 * @param interval_ms Execution interval in milliseconds
 * @return 0 on success, -1 on error
 */
int cli_cmd_set_logic_interval(st_logic_engine_state_t *logic_state, uint32_t interval_ms) {
  if (!logic_state) {
    debug_println("ERROR: Logic state not initialized");
    return -1;
  }

  // Validate interval (only allow specific values)
  if (interval_ms != 10 && interval_ms != 20 && interval_ms != 25 &&
      interval_ms != 50 && interval_ms != 75 && interval_ms != 100) {
    debug_printf("ERROR: Invalid interval %ums (allowed: 10, 20, 25, 50, 75, 100)\n",
                 (unsigned int)interval_ms);
    return -1;
  }

  logic_state->execution_interval_ms = interval_ms;
  debug_printf("[OK] ST Logic execution interval set to %ums\n", (unsigned int)interval_ms);
  debug_println("Note: Use 'save' command to persist to NVS");
  return 0;
}

/**
 * @brief set logic <id> delete
 *
 * Delete a logic program
 *
 * Example:
 *   set logic 1 delete
 */
int cli_cmd_set_logic_delete(st_logic_engine_state_t *logic_state, uint8_t program_id) {
  if (program_id >= 4) {
    debug_println("ERROR: Invalid program ID (0-3)");
    return -1;
  }

  if (!st_logic_delete(logic_state, program_id)) {
    debug_println("ERROR: Failed to delete program");
    return -1;
  }

  debug_printf("[OK] Logic%d deleted\n", program_id + 1);
  return 0;
}

/**
 * @brief Parse new bind syntax: set logic <id> bind <var_name> reg:100|coil:10|input-dis:5
 *
 * Finds variable by name and calls cli_cmd_set_logic_bind_by_index
 *
 * Example:
 *   set logic 1 bind sensor reg:100      # sensor reads from HR#100
 *   set logic 1 bind led coil:10         # led writes to coil
 *   set logic 1 bind switch input-dis:5  # switch reads from discrete input
 */
int cli_cmd_set_logic_bind_by_name(st_logic_engine_state_t *logic_state, uint8_t program_id,
                                    const char *var_name, const char *binding_spec) {
  if (program_id >= 4) {
    debug_println("ERROR: Invalid program ID (0-3)");
    return -1;
  }

  st_logic_program_config_t *prog = st_logic_get_program(logic_state, program_id);
  if (logic_state && logic_state->debug) {
    debug_printf("[BIND_DEBUG] program_id=%d, var=%s, prog=%p, compiled=%d\n",
                 program_id, var_name, prog, prog ? prog->compiled : -1);
  }

  if (!prog || !prog->compiled) {
    debug_println("ERROR: Program not compiled. Upload source code first.");
    return -1;
  }

  // Find variable by name
  uint8_t var_index = 0xff;
  for (uint8_t i = 0; i < prog->bytecode.var_count; i++) {
    if (strcmp(prog->bytecode.var_names[i], var_name) == 0) {
      var_index = i;
      break;
    }
  }

  if (var_index == 0xff) {
    debug_printf("ERROR: Variable '%s' not found in Logic%d\n", var_name, program_id + 1);
    debug_printf("Available variables: ");
    for (uint8_t i = 0; i < prog->bytecode.var_count; i++) {
      if (i > 0) debug_printf(", ");
      debug_printf(prog->bytecode.var_names[i]);
    }
    debug_println("");
    return -1;
  }

  // Debug output if debug mode enabled
  if (logic_state && logic_state->debug) {
    debug_printf("[BIND_DEBUG] Found variable '%s' at index %d\n", var_name, var_index);
  }

  // Parse binding spec: reg:100, coil:10, or input-dis:5
  uint16_t register_addr = 0;
  const char *direction = "output";  // default: output mode for ST variables (typically write to Modbus)
  uint8_t input_type = 0;  // 0 = Holding Register (HR), 1 = Discrete Input (DI)
  uint8_t output_type = 0;  // 0 = Holding Register (HR), 1 = Coil

  if (strncmp(binding_spec, "reg:", 4) == 0) {
    // Holding register (16-bit integer) - OUTPUT mode (ST var → HR)
    register_addr = atoi(binding_spec + 4);
    direction = "output";  // BUG-012 FIX: Changed from "both" to "output" (only 1 mapping, not 2)
    input_type = 0;  // HR (unused in output mode)
    output_type = 0;  // HR
  } else if (strncmp(binding_spec, "coil:", 5) == 0) {
    // Coil (BOOL read/write) - OUTPUT mode (ST var → Coil)
    register_addr = atoi(binding_spec + 5);
    direction = "output";  // BUG-012 FIX: Changed from "both" to "output" (only 1 mapping, not 2)
    input_type = 1;  // DI (unused in output mode)
    output_type = 1;  // Coil
  } else if (strncmp(binding_spec, "input-dis:", 10) == 0 || strncmp(binding_spec, "input:", 6) == 0) {
    // Discrete input (BOOL read) - supports both "input-dis:5" and "input:5" shortcuts
    register_addr = (strncmp(binding_spec, "input-dis:", 10) == 0) ?
                    atoi(binding_spec + 10) : atoi(binding_spec + 6);
    direction = "input";
    input_type = 1;  // DI
    output_type = 0;  // N/A for input
  } else {
    debug_printf("ERROR: Invalid binding spec '%s' (use reg:, coil:, or input:)\n", binding_spec);
    return -1;
  }

  // Validate register address
  if (register_addr >= 160) {  // HOLDING_REGS_SIZE
    debug_printf("ERROR: Invalid Modbus register %d (0-159)\n", register_addr);
    return -1;
  }

  // Debug output if debug mode enabled
  if (logic_state && logic_state->debug) {
    debug_printf("[BIND_DEBUG] Parsed binding: reg=%d, dir=%s, input_type=%d, output_type=%d\n",
                 register_addr, direction, input_type, output_type);
  }

  // Call original bind function with input_type and output_type
  return cli_cmd_set_logic_bind(logic_state, program_id, var_index, register_addr, direction, input_type, output_type);
}

/**
 * @brief set logic <id> bind <var_idx> <register> [input|output|both]
 *
 * Bind ST variable to Modbus register (unified VariableMapping system)
 * LEGACY: Use cli_cmd_set_logic_bind_by_name for new syntax
 *
 * Example:
 *   set logic 1 bind 0 100 input   # ST var[0] reads from HR#100
 *   set logic 1 bind 1 101 output  # ST var[1] writes to HR#101
 */
int cli_cmd_set_logic_bind(st_logic_engine_state_t *logic_state, uint8_t program_id,
                           uint8_t var_index, uint16_t modbus_reg, const char *direction, uint8_t input_type, uint8_t output_type) {
  if (program_id >= 4) {
    debug_println("ERROR: Invalid program ID (0-3)");
    return -1;
  }

  st_logic_program_config_t *prog = st_logic_get_program(logic_state, program_id);
  if (!prog || !prog->compiled) {
    debug_println("ERROR: Program not compiled. Upload source code first.");
    return -1;
  }

  if (var_index >= prog->bytecode.var_count) {
    debug_printf("ERROR: Invalid variable index (0-%d)\n", prog->bytecode.var_count - 1);
    return -1;
  }

  if (modbus_reg >= HOLDING_REGS_SIZE) {
    debug_printf("ERROR: Invalid Modbus register (0-%d)\n", HOLDING_REGS_SIZE - 1);
    return -1;
  }

  uint8_t is_input = 0, is_output = 0;

  if (strcmp(direction, "input") == 0) {
    is_input = 1;
  } else if (strcmp(direction, "output") == 0) {
    is_output = 1;
  } else if (strcmp(direction, "both") == 0 || strcmp(direction, "inout") == 0) {
    is_input = 1;
    is_output = 1;
  } else {
    debug_println("ERROR: Direction must be 'input', 'output', or 'both'");
    return -1;
  }

  // IMPORTANT: VariableMapping.is_input is a MODE flag (1=INPUT, 0=OUTPUT), NOT a capability flag!
  // This means we CANNOT have a single mapping that does both INPUT and OUTPUT.
  // For "both" mode, we need to create TWO separate mappings:
  //   - Mapping 1: INPUT mode (Modbus → ST var)
  //   - Mapping 2: OUTPUT mode (ST var → Modbus)

  // Step 1: Delete ALL existing bindings for this ST variable (we'll recreate them)
  for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
    VariableMapping *map = &g_persist_config.var_maps[i];
    if (map->source_type == MAPPING_SOURCE_ST_VAR &&
        map->st_program_id == program_id &&
        map->st_var_index == var_index) {
      // Delete this mapping by shifting all subsequent mappings down
      for (uint8_t j = i; j < g_persist_config.var_map_count - 1; j++) {
        g_persist_config.var_maps[j] = g_persist_config.var_maps[j + 1];
      }
      g_persist_config.var_map_count--;
      i--;  // Re-check this index (we just shifted)
    }
  }

  // Step 2: Check if we have space for new mapping(s)
  uint8_t mappings_needed = (is_input && is_output) ? 2 : 1;
  if (g_persist_config.var_map_count + mappings_needed > 64) {
    debug_printf("ERROR: Maximum variable mappings (64) would be exceeded (need %d more)\n", mappings_needed);
    return -1;
  }

  // Step 3: Create new mapping(s)
  if (is_input && is_output) {
    // "both" mode: Create TWO mappings (INPUT + OUTPUT)

    // Mapping 1: INPUT (Modbus → ST var)
    VariableMapping *map_in = &g_persist_config.var_maps[g_persist_config.var_map_count++];
    memset(map_in, 0xff, sizeof(*map_in));
    map_in->source_type = MAPPING_SOURCE_ST_VAR;
    map_in->st_program_id = program_id;
    map_in->st_var_index = var_index;
    map_in->is_input = 1;
    map_in->input_type = input_type;  // Use parameter (0=HR, 1=DI/Coil)
    map_in->input_reg = modbus_reg;

    // Mapping 2: OUTPUT (ST var → Modbus)
    VariableMapping *map_out = &g_persist_config.var_maps[g_persist_config.var_map_count++];
    memset(map_out, 0xff, sizeof(*map_out));
    map_out->source_type = MAPPING_SOURCE_ST_VAR;
    map_out->st_program_id = program_id;
    map_out->st_var_index = var_index;
    map_out->is_input = 0;
    map_out->output_type = output_type;  // Use parameter (0=HR, 1=Coil)
    map_out->coil_reg = modbus_reg;

    debug_printf("[OK] Logic%d: var[%d] (%s) <-> Modbus HR#%d (2 mappings created)\n",
                 program_id + 1, var_index, prog->bytecode.var_names[var_index], modbus_reg);
  } else {
    // "input" or "output" mode: Create ONE mapping
    VariableMapping *map = &g_persist_config.var_maps[g_persist_config.var_map_count++];
    memset(map, 0xff, sizeof(*map));
    map->source_type = MAPPING_SOURCE_ST_VAR;
    map->st_program_id = program_id;
    map->st_var_index = var_index;

    if (is_input) {
      map->is_input = 1;
      map->input_type = input_type;
      map->input_reg = modbus_reg;
    } else {
      map->is_input = 0;
      map->output_type = output_type;
      map->coil_reg = modbus_reg;
    }

    // Print confirmation
    debug_printf("[OK] Logic%d: var[%d] (%s) ", program_id + 1, var_index, prog->bytecode.var_names[var_index]);

    if (is_input && input_type == 1) {
      debug_printf("<- Modbus INPUT#%d\n", modbus_reg);
    } else if (is_input) {
      debug_printf("<- Modbus HR#%d\n", modbus_reg);
    } else if (output_type == 1) {
      debug_printf("-> Modbus COIL#%d\n", modbus_reg);
    } else {
      debug_printf("-> Modbus HR#%d\n", modbus_reg);
    }
  }

  // BUG-005 FIX: Update binding count cache after modifying bindings
  st_logic_update_binding_counts(logic_state);

  // Save config to NVS to make binding persistent
  // First, copy ST Logic programs to persistent config
  st_logic_save_to_persist_config(&g_persist_config);
  if (!config_save_to_nvs(&g_persist_config)) {
    debug_println("WARNING: Failed to save config to NVS (binding still active in runtime)");
  }

  return 0;
}

/* ============================================================================
 * SHOW COMMANDS
 * ============================================================================ */

/**
 * @brief show logic <id>
 *
 * Show detailed info about a logic program
 */
int cli_cmd_show_logic_program(st_logic_engine_state_t *logic_state, uint8_t program_id) {
  if (program_id >= 4) {
    debug_println("ERROR: Invalid program ID (0-3)");
    return -1;
  }

  st_logic_print_program(logic_state, program_id);
  return 0;
}

/**
 * @brief show logic all
 *
 * Show status of all logic programs
 */
int cli_cmd_show_logic_all(st_logic_engine_state_t *logic_state) {
  st_logic_print_status(logic_state);
  return 0;
}


/**
 * @brief show logic program
 *
 * Show overview of all programs with status summary
 */
int cli_cmd_show_logic_programs(st_logic_engine_state_t *logic_state) {
  debug_printf("\n=== All Logic Programs ===\n\n");

  for (int i = 0; i < 4; i++) {
    st_logic_program_config_t *prog = &logic_state->programs[i];

    // Program header with status indicator
    const char *status = "";
    if (!prog->source_size) {
      status = "⚪ EMPTY";
    } else if (!prog->compiled) {
      status = "🔴 FAILED";
    } else if (!prog->enabled) {
      status = "🟡 DISABLED";
    } else {
      status = "🟢 ACTIVE";
    }

    debug_printf("  [%d] %s %s\n", i + 1, prog->name, status);

    if (prog->source_size > 0) {
      debug_printf("      Source: %d bytes | Variables: %d\n", prog->source_size, prog->bytecode.var_count);
      debug_printf("      Executions: %u | Errors: %u\n", prog->execution_count, prog->error_count);

      if (prog->error_count > 0 && prog->last_error[0] != '\0') {
        debug_printf("      Last error: %s\n", prog->last_error);
      }
    }
  }

  debug_printf("\n");
  return 0;
}

/**
 * @brief show logic errors
 *
 * Show only programs with compilation or runtime errors
 */
int cli_cmd_show_logic_errors(st_logic_engine_state_t *logic_state) {
  debug_printf("\n=== Logic Program Errors ===\n\n");

  int error_count = 0;

  for (int i = 0; i < 4; i++) {
    st_logic_program_config_t *prog = &logic_state->programs[i];

    // Show if: has compilation error OR has runtime errors
    bool has_compilation_error = (prog->source_size > 0 && !prog->compiled);
    bool has_runtime_errors = (prog->error_count > 0);

    if (has_compilation_error || has_runtime_errors) {
      error_count++;

      debug_printf("  [Logic%d] %s\n", i + 1, prog->name);
      debug_printf("    Status: %s\n", prog->compiled ? "COMPILED" : "NOT COMPILED");

      if (has_compilation_error || (has_runtime_errors && prog->last_error[0] != '\0')) {
        debug_printf("    Error: %s\n", prog->last_error);
      }

      if (has_runtime_errors) {
        debug_printf("    Runtime Errors: %u\n", prog->error_count);
        float error_rate = prog->execution_count > 0 ?
          (float)prog->error_count / prog->execution_count * 100.0f : 0.0f;
        debug_printf("    Error Rate: %.2f%% (%u/%u executions)\n",
               error_rate, prog->error_count, prog->execution_count);
      }

      debug_printf("\n");
    }
  }

  if (error_count == 0) {
    debug_printf("  ✓ No errors found!\n\n");
  } else {
    debug_printf("  Total programs with errors: %d/4\n\n", error_count);
  }

  return 0;
}

/**
 * @brief show logic <id> code
 *
 * Show source code for a specific logic program with line breaks
 */
int cli_cmd_show_logic_code(st_logic_engine_state_t *logic_state, uint8_t program_id) {
  st_logic_program_config_t *prog = st_logic_get_program(logic_state, program_id);
  if (!prog) {
    debug_printf("[ERROR] Invalid program ID: %d\n", program_id);
    return 1;
  }

  debug_printf("\n=== Logic Program: %s - Source Code ===\n\n", prog->name);
  debug_printf("Status: %s | Compiled: %s | Size: %d bytes\n\n",
         prog->enabled ? "ENABLED" : "DISABLED",
         prog->compiled ? "YES" : "NO",
         prog->source_size);

  if (prog->source_size == 0) {
    debug_printf("(empty - no program uploaded)\n\n");
    return 0;
  }

  // Print source code with proper line breaks
  debug_printf("--- SOURCE CODE ---\n");
  const char *source = prog->source_code;

  // Print source code - use debug_printf with %.*s to respect Telnet routing
  debug_printf("%.*s\n", prog->source_size, source);

  debug_printf("--- END SOURCE CODE ---\n\n");
  return 0;
}

/**
 * @brief show logic all code
 *
 * Show source code for all logic programs
 */
int cli_cmd_show_logic_code_all(st_logic_engine_state_t *logic_state) {
  debug_printf("\n========================================\n");
  debug_printf("  All Logic Programs - Source Code\n");
  debug_printf("========================================\n\n");

  for (int i = 0; i < 4; i++) {
    st_logic_program_config_t *prog = &logic_state->programs[i];

    debug_printf("--- [%d] %s ---\n", i + 1, prog->name);
    debug_printf("Status: %s | Compiled: %s | Size: %d bytes\n",
           prog->enabled ? "ENABLED" : "DISABLED",
           prog->compiled ? "YES" : "NO",
           prog->source_size);

    if (prog->source_size == 0) {
      debug_printf("(empty - no program uploaded)\n");
    } else {
      debug_printf("\nSource:\n");
      const char *source = prog->source_code;
      // Print source code - use debug_printf with %.*s to respect Telnet routing
      debug_printf("%.*s\n", prog->source_size, source);
    }

    debug_printf("\n");
  }

  debug_printf("========================================\n\n");
  return 0;
}

/**
 * @brief show logic stats - Display performance statistics for all programs (v4.1.0)
 */
int cli_cmd_show_logic_stats(st_logic_engine_state_t *logic_state) {
  if (!logic_state) return -1;

  debug_printf("\n======== ST Logic Performance Statistics ========\n\n");

  // Global cycle statistics
  debug_printf("Global Cycle Stats:\n");
  debug_printf("  Total cycles:    %u\n", (unsigned int)logic_state->total_cycles);
  debug_printf("  Cycle min:       %ums\n", (unsigned int)logic_state->cycle_min_ms);
  debug_printf("  Cycle max:       %ums\n", (unsigned int)logic_state->cycle_max_ms);

  if (logic_state->total_cycles > 0) {
    debug_printf("  Cycle target:    %ums\n", (unsigned int)logic_state->execution_interval_ms);
    debug_printf("  Overruns:        %u (%.1f%%)\n",
                 (unsigned int)logic_state->cycle_overrun_count,
                 (float)logic_state->cycle_overrun_count * 100.0 / logic_state->total_cycles);
  }

  debug_printf("\n");

  // Per-program statistics
  for (uint8_t i = 0; i < 4; i++) {
    st_logic_program_config_t *prog = &logic_state->programs[i];

    if (!prog->enabled && prog->execution_count == 0) {
      continue;  // Skip disabled programs with no history
    }

    debug_printf("Logic%d (%s):\n", i + 1, prog->name);
    debug_printf("  Status:        %s%s%s\n",
                 prog->enabled ? "ENABLED" : "disabled",
                 prog->compiled ? ", compiled" : "",
                 prog->error_count > 0 ? ", HAS ERRORS" : "");

    debug_printf("  Executions:    %u\n", (unsigned int)prog->execution_count);

    if (prog->execution_count > 0) {
      uint32_t avg_us = prog->total_execution_us / prog->execution_count;

      debug_printf("  Min time:      %u.%03ums\n",
                   (unsigned int)(prog->min_execution_ms / 1000),
                   (unsigned int)(prog->min_execution_ms % 1000));
      debug_printf("  Max time:      %u.%03ums\n",
                   (unsigned int)(prog->max_execution_ms / 1000),
                   (unsigned int)(prog->max_execution_ms % 1000));
      debug_printf("  Avg time:      %u.%03ums\n",
                   (unsigned int)(avg_us / 1000),
                   (unsigned int)(avg_us % 1000));
      debug_printf("  Last time:     %u.%03ums\n",
                   (unsigned int)(prog->last_execution_ms / 1000),
                   (unsigned int)(prog->last_execution_ms % 1000));

      if (prog->overrun_count > 0) {
        debug_printf("  Overruns:      %u (%.1f%%) ⚠️\n",
                     (unsigned int)prog->overrun_count,
                     (float)prog->overrun_count * 100.0 / prog->execution_count);
      }

      if (prog->error_count > 0) {
        debug_printf("  Errors:        %u (%.1f%%) ❌\n",
                     (unsigned int)prog->error_count,
                     (float)prog->error_count * 100.0 / prog->execution_count);
      }
    }

    debug_printf("\n");
  }

  debug_printf("Use 'show logic X timing' for detailed timing analysis\n");
  debug_printf("Use 'set logic stats reset' to clear statistics\n");
  debug_printf("=================================================\n\n");

  return 0;
}

/**
 * @brief show logic X timing - Display detailed timing analysis for specific program (v4.1.0)
 */
int cli_cmd_show_logic_timing(st_logic_engine_state_t *logic_state, uint8_t program_id) {
  if (program_id >= 4) {
    debug_printf("ERROR: Invalid program ID (1-4)\n");
    return -1;
  }

  st_logic_program_config_t *prog = &logic_state->programs[program_id];

  debug_printf("\n======== Logic%d Timing Analysis ========\n\n", program_id + 1);

  if (!prog->enabled && prog->execution_count == 0) {
    debug_printf("Program has never been executed.\n\n");
    return 0;
  }

  debug_printf("Program: %s\n", prog->name);
  debug_printf("Status:  %s%s\n",
               prog->enabled ? "ENABLED" : "DISABLED",
               prog->compiled ? " (compiled)" : " (not compiled)");
  debug_printf("\n");

  // Execution statistics
  debug_printf("Execution Statistics:\n");
  debug_printf("  Total executions:  %u\n", (unsigned int)prog->execution_count);
  debug_printf("  Successful:        %u\n", (unsigned int)(prog->execution_count - prog->error_count));
  debug_printf("  Failed:            %u\n", (unsigned int)prog->error_count);

  if (prog->error_count > 0) {
    debug_printf("  Success rate:      %.1f%%\n",
                 (float)(prog->execution_count - prog->error_count) * 100.0 / prog->execution_count);
    debug_printf("  Last error:        %s\n", prog->last_error);
  }
  debug_printf("\n");

  // Timing statistics
  if (prog->execution_count > 0) {
    uint32_t avg_us = prog->total_execution_us / prog->execution_count;

    debug_printf("Timing Performance:\n");
    debug_printf("  Min:               %u.%03ums (%uµs)\n",
                 (unsigned int)(prog->min_execution_ms / 1000),
                 (unsigned int)(prog->min_execution_ms % 1000),
                 (unsigned int)prog->min_execution_ms);
    debug_printf("  Max:               %u.%03ums (%uµs)\n",
                 (unsigned int)(prog->max_execution_ms / 1000),
                 (unsigned int)(prog->max_execution_ms % 1000),
                 (unsigned int)prog->max_execution_ms);
    debug_printf("  Avg:               %u.%03ums (%uµs)\n",
                 (unsigned int)(avg_us / 1000),
                 (unsigned int)(avg_us % 1000),
                 (unsigned int)avg_us);
    debug_printf("  Last:              %u.%03ums (%uµs)\n",
                 (unsigned int)(prog->last_execution_ms / 1000),
                 (unsigned int)(prog->last_execution_ms % 1000),
                 (unsigned int)prog->last_execution_ms);
    debug_printf("\n");

    // Performance analysis
    debug_printf("Performance Analysis:\n");
    debug_printf("  Target interval:   %ums\n", (unsigned int)logic_state->execution_interval_ms);

    uint32_t avg_ms = avg_us / 1000;
    if (avg_ms < logic_state->execution_interval_ms / 4) {
      debug_printf("  Rating:            ✓ EXCELLENT (< 25%% of target)\n");
    } else if (avg_ms < logic_state->execution_interval_ms / 2) {
      debug_printf("  Rating:            ✓ GOOD (< 50%% of target)\n");
    } else if (avg_ms < logic_state->execution_interval_ms) {
      debug_printf("  Rating:            ⚠️  ACCEPTABLE (< 100%% of target)\n");
    } else {
      debug_printf("  Rating:            ❌ POOR (> 100%% of target) - REFACTOR NEEDED!\n");
    }

    if (prog->overrun_count > 0) {
      debug_printf("  Overruns:          %u (%.1f%%) ⚠️\n",
                   (unsigned int)prog->overrun_count,
                   (float)prog->overrun_count * 100.0 / prog->execution_count);
      debug_printf("                     Program exceeded target interval!\n");
    } else {
      debug_printf("  Overruns:          0 (0.0%%) ✓\n");
    }
    debug_printf("\n");

    // Recommendations
    if (avg_ms > logic_state->execution_interval_ms) {
      debug_printf("⚠️  RECOMMENDATIONS:\n");
      debug_printf("  - Simplify program logic (reduce loop iterations)\n");
      debug_printf("  - Increase execution interval (set logic interval:20)\n");
      debug_printf("  - Split program into smaller sub-programs\n");
      debug_printf("  - Consider moving heavy computation outside ST Logic\n");
    } else if (prog->overrun_count > 0) {
      debug_printf("⚠️  RECOMMENDATIONS:\n");
      debug_printf("  - Occasional overruns detected\n");
      debug_printf("  - Check for conditional paths that take longer\n");
      debug_printf("  - Monitor with 'set logic debug:true'\n");
    }
  }

  debug_printf("==========================================\n\n");
  return 0;
}

/**
 * @brief set logic stats reset [X] - Reset statistics (v4.1.0)
 */
int cli_cmd_reset_logic_stats(st_logic_engine_state_t *logic_state, const char *target) {
  if (!logic_state) return -1;

  if (strcmp(target, "all") == 0) {
    // Reset all program stats
    st_logic_reset_stats(logic_state, 0xFF);
    st_logic_reset_cycle_stats(logic_state);
    debug_printf("All ST Logic statistics reset.\n");
  } else if (strcmp(target, "cycle") == 0) {
    // Reset only global cycle stats
    st_logic_reset_cycle_stats(logic_state);
    debug_printf("Global cycle statistics reset.\n");
  } else {
    // Parse program ID (1-4)
    int prog_num = atoi(target);
    if (prog_num < 1 || prog_num > 4) {
      debug_printf("ERROR: Invalid target. Use: all, cycle, or 1-4\n");
      return -1;
    }

    st_logic_reset_stats(logic_state, prog_num - 1);
    debug_printf("Logic%d statistics reset.\n", prog_num);
  }

  return 0;
}
