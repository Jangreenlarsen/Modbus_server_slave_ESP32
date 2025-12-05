/**
 * @file st_logic_config.h
 * @brief Structured Text Logic Mode Configuration
 *
 * Configuration for logic programs and Modbus register bindings.
 * Supports 4 independent logic programs with register I/O.
 */

#ifndef ST_LOGIC_CONFIG_H
#define ST_LOGIC_CONFIG_H

#include <stdint.h>
#include "st_types.h"
#include "config_struct.h"

/* ============================================================================
 * LOGIC PROGRAM CONFIGURATION
 *
 * NOTE: Variable bindings are now handled by unified VariableMapping system
 * in gpio_mapping.cpp. No longer duplicated here.
 * ============================================================================ */

typedef struct {
  // Program identification
  char name[32];              // "Logic1", "Logic2", etc.
  uint8_t enabled;            // Is this program enabled?

  // Source code storage
  char source_code[2000];     // ST source code (max 2KB per program)
  uint32_t source_size;

  // Compiled bytecode
  st_bytecode_program_t bytecode; // Compiled and ready to execute
  uint8_t compiled;           // Is bytecode valid?

  // Execution statistics
  uint32_t execution_count;   // Number of times executed
  uint32_t error_count;       // Number of execution errors
  uint32_t last_execution_ms; // Last execution time (milliseconds)
  char last_error[128];       // Last error message

} st_logic_program_config_t;

/* ============================================================================
 * GLOBAL LOGIC ENGINE STATE
 * ============================================================================ */

typedef struct {
  // 4 independent logic programs
  st_logic_program_config_t programs[4];

  // Global settings
  uint8_t enabled;            // Logic mode enabled/disabled globally
  uint8_t debug;              // Debug output enabled (bytecode, execution trace, etc.)
  uint32_t execution_interval_ms; // How often to run programs (10ms default)
  uint32_t last_run_time;     // Timestamp of last execution

} st_logic_engine_state_t;

/* ============================================================================
 * FUNCTIONS
 * ============================================================================ */

/**
 * @brief Initialize logic engine state
 * @param state Logic engine state
 */
void st_logic_init(st_logic_engine_state_t *state);

/**
 * @brief Upload ST source code for a program
 * @param state Logic engine state
 * @param program_id Program ID (0-3)
 * @param source ST source code
 * @param source_size Size of source code
 * @return true if successful
 */
bool st_logic_upload(st_logic_engine_state_t *state, uint8_t program_id,
                      const char *source, uint32_t source_size);

/**
 * @brief Compile and prepare logic program for execution
 * @param state Logic engine state
 * @param program_id Program ID (0-3)
 * @return true if successful
 */
bool st_logic_compile(st_logic_engine_state_t *state, uint8_t program_id);

/**
 * @brief Set variable binding (ST variable ↔ Modbus register)
 *
 * DEPRECATED: Use the unified VariableMapping system in gpio_mapping.cpp instead.
 * To bind a variable:
 *   1. Create a VariableMapping entry in g_persist_config.var_maps
 *   2. Set source_type = MAPPING_SOURCE_ST_VAR
 *   3. Set st_program_id and st_var_index
 *   4. Set is_input/coil_reg or is_output fields
 *   5. Call config_save() to persist
 *
 * The mapping engine will handle all I/O automatically.
 */
// FUNCTION REMOVED - use VariableMapping system instead

/**
 * @brief Enable/disable a logic program
 * @param state Logic engine state
 * @param program_id Program ID (0-3)
 * @param enabled true to enable, false to disable
 * @return true if successful
 */
bool st_logic_set_enabled(st_logic_engine_state_t *state, uint8_t program_id, uint8_t enabled);

/**
 * @brief Delete/clear a logic program
 * @param state Logic engine state
 * @param program_id Program ID (0-3)
 * @return true if successful
 */
bool st_logic_delete(st_logic_engine_state_t *state, uint8_t program_id);

/**
 * @brief Get program info
 * @param state Logic engine state
 * @param program_id Program ID (0-3)
 * @return Program configuration (NULL if invalid ID)
 */
st_logic_program_config_t *st_logic_get_program(st_logic_engine_state_t *state, uint8_t program_id);

/**
 * @brief Get pointer to global logic engine state
 * @return Pointer to the global ST logic engine state
 */
st_logic_engine_state_t *st_logic_get_state(void);

/**
 * @brief Save ST Logic programs to PersistConfig (before config_save_to_nvs)
 * @param config Persistent config to save programs into
 * @return true if successful
 */
bool st_logic_save_to_persist_config(PersistConfig *config);

/**
 * @brief Load ST Logic programs from PersistConfig (after config_load_from_nvs)
 * @param config Persistent config containing programs to load
 * @return true if successful
 */
bool st_logic_load_from_persist_config(const PersistConfig *config);

#endif // ST_LOGIC_CONFIG_H
