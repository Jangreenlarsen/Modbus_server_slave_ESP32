/**
 * @file cli_commands_logic.h
 * @brief CLI Commands for Structured Text Logic Mode
 */

#ifndef CLI_COMMANDS_LOGIC_H
#define CLI_COMMANDS_LOGIC_H

#include <stdint.h>
#include "st_logic_config.h"
#include "st_logic_engine.h"

/* ============================================================================
 * CLI COMMAND HANDLERS FOR LOGIC MODE
 * ============================================================================ */

/**
 * @brief set logic <id> upload "<st_code>"
 * Upload ST source code for a logic program
 */
int cli_cmd_set_logic_upload(st_logic_engine_state_t *logic_state, uint8_t program_id,
                             const char *source_code);

/**
 * @brief set logic <id> enabled:true|false
 * Enable or disable a logic program
 */
int cli_cmd_set_logic_enabled(st_logic_engine_state_t *logic_state, uint8_t program_id,
                              bool enabled);

/**
 * @brief set logic debug:true|false
 * Enable/disable debug output for ST Logic
 */
int cli_cmd_set_logic_debug(st_logic_engine_state_t *logic_state, bool debug);

/**
 * @brief set logic <id> delete
 * Delete a logic program
 */
int cli_cmd_set_logic_delete(st_logic_engine_state_t *logic_state, uint8_t program_id);

/**
 * @brief set logic <id> bind <var_name> reg:100|coil:10|input-dis:5
 * Bind ST variable to Modbus register using variable name and binding spec
 */
int cli_cmd_set_logic_bind_by_name(st_logic_engine_state_t *logic_state, uint8_t program_id,
                                    const char *var_name, const char *binding_spec);

/**
 * @brief set logic <id> bind <var_index> <register> [input|output|both]
 * Bind ST variable to Modbus register (LEGACY - use bind_by_name for new syntax)
 */
int cli_cmd_set_logic_bind(st_logic_engine_state_t *logic_state, uint8_t program_id,
                           uint8_t var_idx, uint16_t register_address,
                           const char *direction, uint8_t input_type);

/**
 * @brief show logic <id>
 * Show details for a specific logic program
 */
int cli_cmd_show_logic_program(st_logic_engine_state_t *logic_state, uint8_t program_id);

/**
 * @brief show logic all
 * Show all logic programs
 */
int cli_cmd_show_logic_all(st_logic_engine_state_t *logic_state);

/**
 * @brief show logic stats
 * Show execution statistics for logic programs
 */
int cli_cmd_show_logic_stats(st_logic_engine_state_t *logic_state);

/**
 * @brief show logic programs
 * Show overview of all programs with status summary
 */
int cli_cmd_show_logic_programs(st_logic_engine_state_t *logic_state);

/**
 * @brief show logic errors
 * Show only programs with compilation or runtime errors
 */
int cli_cmd_show_logic_errors(st_logic_engine_state_t *logic_state);

/**
 * @brief show logic <id> code
 * Show source code for a specific logic program with line breaks
 */
int cli_cmd_show_logic_code(st_logic_engine_state_t *logic_state, uint8_t program_id);

/**
 * @brief show logic all code
 * Show source code for all logic programs
 */
int cli_cmd_show_logic_code_all(st_logic_engine_state_t *logic_state);

#endif /* CLI_COMMANDS_LOGIC_H */
