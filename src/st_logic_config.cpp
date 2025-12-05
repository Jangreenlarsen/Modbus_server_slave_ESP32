/**
 * @file st_logic_config.cpp
 * @brief Structured Text Logic Configuration Implementation
 */

#include "st_logic_config.h"
#include "st_parser.h"
#include "st_compiler.h"
#include "debug.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <nvs_flash.h>
#include <nvs.h>

/* ============================================================================
 * GLOBAL STATE
 * ============================================================================ */

// Global logic engine state - single instance for all 4 programs
static st_logic_engine_state_t g_logic_state;

// Global parser and compiler to avoid stack overflow
// (These are large structures, don't allocate on stack)
static st_parser_t g_parser;
static st_compiler_t g_compiler;

/**
 * @brief Get pointer to global logic engine state
 */
st_logic_engine_state_t *st_logic_get_state(void) {
  return &g_logic_state;
}

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void st_logic_init(st_logic_engine_state_t *state) {
  memset(state, 0, sizeof(*state));
  state->enabled = 1;
  state->execution_interval_ms = 10;  // Run every 10ms by default

  // Initialize each program
  for (int i = 0; i < 4; i++) {
    st_logic_program_config_t *prog = &state->programs[i];
    memset(prog, 0, sizeof(*prog));
    snprintf(prog->name, sizeof(prog->name), "Logic%d", i + 1);
    prog->enabled = 0;  // Disabled by default
    prog->compiled = 0;
  }
}

/* ============================================================================
 * PROGRAM UPLOAD & COMPILATION
 * ============================================================================ */

bool st_logic_upload(st_logic_engine_state_t *state, uint8_t program_id,
                      const char *source, uint32_t source_size) {
  if (program_id >= 4) return false;
  if (!source || source_size == 0 || source_size > 2000) return false;

  st_logic_program_config_t *prog = &state->programs[program_id];
  memcpy(prog->source_code, source, source_size);
  prog->source_size = source_size;
  prog->compiled = 0;  // Mark as needing compilation

  return true;
}

bool st_logic_compile(st_logic_engine_state_t *state, uint8_t program_id) {
  if (program_id >= 4) return false;

  st_logic_program_config_t *prog = &state->programs[program_id];

  if (!prog->source_code || prog->source_size == 0) {
    snprintf(prog->last_error, sizeof(prog->last_error), "No source code uploaded");
    return false;
  }

  // Parse the source code (use global parser to avoid stack overflow)
  st_parser_init(&g_parser, prog->source_code);
  st_program_t *program = st_parser_parse_program(&g_parser);

  if (!program) {
    snprintf(prog->last_error, sizeof(prog->last_error), "Parse error: %s", g_parser.error_msg);
    return false;
  }

  // Compile to bytecode (use global compiler to avoid stack overflow)
  st_compiler_init(&g_compiler);
  st_bytecode_program_t *bytecode = st_compiler_compile(&g_compiler, program);

  if (!bytecode) {
    snprintf(prog->last_error, sizeof(prog->last_error), "Compile error: %s", g_compiler.error_msg);
    st_program_free(program);
    return false;
  }

  // Store compiled bytecode in config
  memcpy(&prog->bytecode, bytecode, sizeof(*bytecode));
  prog->compiled = 1;
  prog->execution_count = 0;
  prog->error_count = 0;

  st_program_free(program);
  free(bytecode);

  return true;
}

/* ============================================================================
 * VARIABLE BINDING (DEPRECATED)
 *
 * NOTE: Variable binding is now handled by the unified VariableMapping system.
 * Instead of using st_logic_bind_variable(), create a VariableMapping entry:
 *
 * Example:
 *   VariableMapping map = {
 *     .source_type = MAPPING_SOURCE_ST_VAR,
 *     .st_program_id = 0,
 *     .st_var_index = 2,
 *     .is_input = 0,  // OUTPUT mode
 *     .coil_reg = 100  // Write to HR#100
 *   };
 *   g_persist_config.var_maps[g_persist_config.var_map_count++] = map;
 *   config_save();
 *
 * This eliminates code duplication and provides a single place to manage
 * all variable bindings (GPIO + ST).
 * ============================================================================ */

/* ============================================================================
 * PROGRAM MANAGEMENT
 * ============================================================================ */

bool st_logic_set_enabled(st_logic_engine_state_t *state, uint8_t program_id, uint8_t enabled) {
  if (program_id >= 4) return false;

  st_logic_program_config_t *prog = &state->programs[program_id];
  prog->enabled = (enabled != 0);

  return true;
}

bool st_logic_delete(st_logic_engine_state_t *state, uint8_t program_id) {
  if (program_id >= 4) return false;

  st_logic_program_config_t *prog = &state->programs[program_id];
  memset(prog, 0, sizeof(*prog));
  snprintf(prog->name, sizeof(prog->name), "Logic%d", program_id + 1);
  prog->enabled = 0;
  prog->compiled = 0;

  return true;
}

st_logic_program_config_t *st_logic_get_program(st_logic_engine_state_t *state, uint8_t program_id) {
  if (program_id >= 4) return NULL;
  return &state->programs[program_id];
}

/* ============================================================================
 * PERSISTENCE (NVS STORAGE)
 * ============================================================================ */

/**
 * @brief Save ST Logic programs to NVS separately (avoid PersistConfig size limit)
 * @return true if successful
 */
bool st_logic_save_to_nvs(void) {
  st_logic_engine_state_t *state = st_logic_get_state();
  nvs_handle_t handle;

  esp_err_t err = nvs_open("st_logic", NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    debug_print("ST_LOGIC SAVE: NVS open failed: ");
    debug_print_uint(err);
    debug_println("");
    return false;
  }

  debug_println("ST_LOGIC SAVE: Saving programs to NVS");

  // Save each program
  uint8_t saved_count = 0;
  for (uint8_t i = 0; i < 4; i++) {
    st_logic_program_config_t *prog = &state->programs[i];

    // Skip empty programs
    if (prog->source_size == 0) continue;

    char key[16];
    snprintf(key, sizeof(key), "prog_%d", i);

    // Store: enabled flag + source size + source code
    uint8_t data[2050];
    data[0] = prog->enabled;
    memcpy(&data[1], &prog->source_size, sizeof(uint32_t));
    if (prog->source_size > 0 && prog->source_size <= 2000) {
      memcpy(&data[5], prog->source_code, prog->source_size);
    }

    uint32_t total_size = 5 + prog->source_size;
    err = nvs_set_blob(handle, key, data, total_size);
    if (err == ESP_OK) {
      debug_print("  Program ");
      debug_print_uint(i);
      debug_print(": saved ");
      debug_print_uint(prog->source_size);
      debug_print(" bytes (enabled=");
      debug_print_uint(prog->enabled);
      debug_println(")");
      saved_count++;
    } else {
      debug_print("  Program ");
      debug_print_uint(i);
      debug_print(": FAILED - error ");
      debug_print_uint(err);
      debug_println("");
    }
  }

  err = nvs_commit(handle);
  nvs_close(handle);

  debug_print("ST_LOGIC SAVE: Saved ");
  debug_print_uint(saved_count);
  debug_print(" programs (commit=");
  debug_print_uint(err);
  debug_println(")");

  return (err == ESP_OK);
}

/**
 * @brief Load ST Logic programs from NVS
 * @return true if successful
 */
bool st_logic_load_from_nvs(void) {
  st_logic_engine_state_t *state = st_logic_get_state();
  nvs_handle_t handle;

  esp_err_t err = nvs_open("st_logic", NVS_READONLY, &handle);
  if (err != ESP_OK) {
    debug_println("ST_LOGIC LOAD: No saved programs found (namespace not created yet)");
    return true;  // No saved programs yet - OK
  }

  debug_println("ST_LOGIC LOAD: Loading programs from NVS");

  // Load each program
  uint8_t loaded_count = 0;
  for (uint8_t i = 0; i < 4; i++) {
    st_logic_program_config_t *prog = &state->programs[i];

    char key[16];
    snprintf(key, sizeof(key), "prog_%d", i);

    uint8_t data[2050];
    size_t length = sizeof(data);

    err = nvs_get_blob(handle, key, data, &length);
    if (err == ESP_OK && length >= 5) {
      // Parse: enabled flag + source size + source code
      prog->enabled = data[0];
      memcpy(&prog->source_size, &data[1], sizeof(uint32_t));

      if (prog->source_size > 0 && prog->source_size <= 2000) {
        memcpy(prog->source_code, &data[5], prog->source_size);
        prog->compiled = 0;  // Mark as needing recompilation

        debug_print("  Program ");
        debug_print_uint(i);
        debug_print(": loaded ");
        debug_print_uint(prog->source_size);
        debug_print(" bytes, enabled=");
        debug_print_uint(prog->enabled);
        debug_print(", compiling...");

        // Immediately compile the program
        if (st_logic_compile(state, i)) {
          debug_println(" OK");
          loaded_count++;
        } else {
          debug_print(" FAILED: ");
          debug_println(prog->last_error);
        }
      } else {
        debug_print("  Program ");
        debug_print_uint(i);
        debug_print(": invalid size ");
        debug_print_uint(prog->source_size);
        debug_println("");
      }
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
      debug_print("  Program ");
      debug_print_uint(i);
      debug_print(": read error ");
      debug_print_uint(err);
      debug_println("");
    }
    // ESP_ERR_NVS_NOT_FOUND is normal (program slot empty)
  }

  nvs_close(handle);

  debug_print("ST_LOGIC LOAD: Loaded ");
  debug_print_uint(loaded_count);
  debug_println(" programs");

  return true;
}

// Legacy compatibility functions (empty implementations)
bool st_logic_save_to_persist_config(PersistConfig *config) {
  (void)config;  // Unused - ST Logic now saves to separate NVS namespace
  return st_logic_save_to_nvs();
}

bool st_logic_load_from_persist_config(const PersistConfig *config) {
  (void)config;  // Unused - ST Logic now loads from separate NVS namespace
  return st_logic_load_from_nvs();
}
