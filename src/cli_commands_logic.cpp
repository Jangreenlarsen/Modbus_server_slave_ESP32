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

/* ST Logic Engine includes */
#include "st_logic_config.h"
#include "st_logic_engine.h"

/* Config & Mapping includes */
#include "config_struct.h"
#include "constants.h"

/* Forward declarations - from existing CLI infrastructure */
extern void debug_println(const char *msg);
extern void debug_printf(const char *fmt, ...);

/* Forward declarations - config persistence */
extern bool config_save_to_nvs(const PersistConfig* cfg);

/* Forward declaration for bind function */
int cli_cmd_set_logic_bind(st_logic_engine_state_t *logic_state, uint8_t program_id,
                           uint8_t var_index, uint16_t modbus_reg, const char *direction);

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

  // Success output
  debug_println("");
  debug_println("✓ COMPILATION SUCCESSFUL");
  debug_printf("  Program: Logic%d\n", program_id + 1);
  debug_printf("  Source: %d bytes\n", strlen(source_code));
  debug_printf("  Bytecode: %d instructions\n", prog->bytecode.instr_count);
  debug_printf("  Variables: %d\n", prog->bytecode.var_count);
  debug_println("");

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

  // Parse binding spec: reg:100, coil:10, or input-dis:5
  uint16_t register_addr = 0;
  const char *direction = "both";  // default

  if (strncmp(binding_spec, "reg:", 4) == 0) {
    // Holding register (16-bit integer)
    register_addr = atoi(binding_spec + 4);
    direction = "both";  // INT variables work as input/output
  } else if (strncmp(binding_spec, "coil:", 5) == 0) {
    // Coil output (BOOL write)
    register_addr = atoi(binding_spec + 5);
    direction = "output";
  } else if (strncmp(binding_spec, "input-dis:", 10) == 0) {
    // Discrete input (BOOL read)
    register_addr = atoi(binding_spec + 10);
    direction = "input";
  } else {
    debug_printf("ERROR: Invalid binding spec '%s' (use reg:, coil:, or input-dis:)\n", binding_spec);
    return -1;
  }

  // Validate register address
  if (register_addr >= 160) {  // HOLDING_REGS_SIZE
    debug_printf("ERROR: Invalid Modbus register %d (0-159)\n", register_addr);
    return -1;
  }

  // Call original bind function
  return cli_cmd_set_logic_bind(logic_state, program_id, var_index, register_addr, direction);
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
                           uint8_t var_index, uint16_t modbus_reg, const char *direction) {
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

  // Use unified VariableMapping system
  if (g_persist_config.var_map_count >= 16) {
    debug_println("ERROR: Maximum variable mappings (16) reached");
    return -1;
  }

  // Check for existing binding for this variable
  for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
    VariableMapping *map = &g_persist_config.var_maps[i];
    if (map->source_type == MAPPING_SOURCE_ST_VAR &&
        map->st_program_id == program_id &&
        map->st_var_index == var_index) {
      // Update existing binding
      if (is_input) {
        map->is_input = 1;
        map->input_reg = modbus_reg;
      }
      if (is_output) {
        map->is_input = 0;
        map->coil_reg = modbus_reg;
      }
      debug_printf("[OK] Logic%d: var[%d] %s Modbus HR#%d (updated)\n",
                   program_id + 1, var_index,
                   (is_input && is_output) ? "<->" : (is_input) ? "<-" : "->",
                   modbus_reg);
      return 0;
    }
  }

  // Add new binding
  VariableMapping *map = &g_persist_config.var_maps[g_persist_config.var_map_count++];
  memset(map, 0xff, sizeof(*map));  // Initialize to 0xff (unused)
  map->source_type = MAPPING_SOURCE_ST_VAR;
  map->st_program_id = program_id;
  map->st_var_index = var_index;

  if (is_input) {
    map->is_input = 1;
    map->input_reg = modbus_reg;
  }
  if (is_output) {
    map->is_input = 0;  // OUTPUT mode
    map->coil_reg = modbus_reg;
  }

  debug_printf("[OK] Logic%d: var[%d] (%s) %s Modbus ",
               program_id + 1, var_index, prog->bytecode.var_names[var_index],
               (is_input && is_output) ? "<->" : (is_input) ? "<-" : "->");

  if (is_input && !is_output) {
    debug_printf("HR#%d (input)\n", modbus_reg);
  } else if (is_output && !is_input) {
    debug_printf("Coil#%d (output)\n", modbus_reg);
  } else {
    debug_printf("HR#%d (bidirectional)\n", modbus_reg);
  }

  // Save config to NVS to make binding persistent
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
 * @brief show logic stats
 *
 * Show execution statistics for logic mode
 */
int cli_cmd_show_logic_stats(st_logic_engine_state_t *logic_state) {
  printf("\n=== Logic Engine Statistics ===\n");

  uint32_t total_executions = 0;
  uint32_t total_errors = 0;
  uint32_t enabled_count = 0;
  uint32_t compiled_count = 0;

  for (int i = 0; i < 4; i++) {
    st_logic_program_config_t *prog = &logic_state->programs[i];
    total_executions += prog->execution_count;
    total_errors += prog->error_count;
    if (prog->enabled) enabled_count++;
    if (prog->compiled) compiled_count++;
  }

  printf("Programs Compiled: %d/4\n", compiled_count);
  printf("Programs Enabled: %d/4\n", enabled_count);
  printf("Total Executions: %u\n", total_executions);
  printf("Total Errors: %u\n", total_errors);

  if (total_executions > 0) {
    float error_rate = (float)total_errors / total_executions * 100.0f;
    printf("Error Rate: %.2f%%\n", error_rate);
  }

  printf("\n");
  return 0;
}

/**
 * @brief show logic program
 *
 * Show overview of all programs with status summary
 */
int cli_cmd_show_logic_programs(st_logic_engine_state_t *logic_state) {
  printf("\n=== All Logic Programs ===\n\n");

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

    printf("  [%d] %s %s\n", i + 1, prog->name, status);

    if (prog->source_size > 0) {
      printf("      Source: %d bytes | Variables: %d\n", prog->source_size, prog->bytecode.var_count);
      printf("      Executions: %u | Errors: %u\n", prog->execution_count, prog->error_count);

      if (prog->error_count > 0 && prog->last_error[0] != '\0') {
        printf("      Last error: %s\n", prog->last_error);
      }
    }
  }

  printf("\n");
  return 0;
}

/**
 * @brief show logic errors
 *
 * Show only programs with compilation or runtime errors
 */
int cli_cmd_show_logic_errors(st_logic_engine_state_t *logic_state) {
  printf("\n=== Logic Program Errors ===\n\n");

  int error_count = 0;

  for (int i = 0; i < 4; i++) {
    st_logic_program_config_t *prog = &logic_state->programs[i];

    // Show if: has compilation error OR has runtime errors
    bool has_compilation_error = (prog->source_size > 0 && !prog->compiled);
    bool has_runtime_errors = (prog->error_count > 0);

    if (has_compilation_error || has_runtime_errors) {
      error_count++;

      printf("  [Logic%d] %s\n", i + 1, prog->name);
      printf("    Status: %s\n", prog->compiled ? "COMPILED" : "NOT COMPILED");

      if (has_compilation_error || (has_runtime_errors && prog->last_error[0] != '\0')) {
        printf("    Error: %s\n", prog->last_error);
      }

      if (has_runtime_errors) {
        printf("    Runtime Errors: %u\n", prog->error_count);
        float error_rate = prog->execution_count > 0 ?
          (float)prog->error_count / prog->execution_count * 100.0f : 0.0f;
        printf("    Error Rate: %.2f%% (%u/%u executions)\n",
               error_rate, prog->error_count, prog->execution_count);
      }

      printf("\n");
    }
  }

  if (error_count == 0) {
    printf("  ✓ No errors found!\n\n");
  } else {
    printf("  Total programs with errors: %d/4\n\n", error_count);
  }

  return 0;
}
