/**
 * @file cli_show.h
 * @brief CLI `show` command handlers (LAYER 7)
 *
 * LAYER 7: User Interface - CLI Show Commands
 * Responsibility: Implement all `show` commands for status/config display
 *
 * This file handles:
 * - show config (full system configuration)
 * - show counters (counter status table)
 * - show timers (timer status)
 * - show registers [start] [count] (holding registers)
 * - show coils (coil states)
 * - show inputs (discrete inputs)
 * - show version (firmware version)
 *
 * Each command is a standalone handler function
 * Formatting optimized for serial terminal display
 *
 * Does NOT handle:
 * - Command parsing (→ cli_parser.h)
 * - `set` commands (→ cli_commands.h)
 * - Serial I/O (→ cli_shell.h)
 */

#ifndef CLI_SHOW_H
#define CLI_SHOW_H

#include <stdint.h>
#include "types.h"

/* ============================================================================
 * PUBLIC API - Show Command Handlers
 * ============================================================================ */

/**
 * @brief Handle "show config" command (full system configuration)
 */
void cli_cmd_show_config(void);

/**
 * @brief Handle "show counters" command (counter status table)
 */
void cli_cmd_show_counters(void);

/**
 * @brief Handle "show timers" command (timer status)
 */
void cli_cmd_show_timers(void);

/**
 * @brief Handle "show registers" command
 * @param start Starting register address (0 if all)
 * @param count Number of registers to show (0 if all)
 */
void cli_cmd_show_registers(uint16_t start, uint16_t count);

/**
 * @brief Handle "show coils" command
 */
void cli_cmd_show_coils(void);

/**
 * @brief Handle "show inputs" command (discrete inputs)
 */
void cli_cmd_show_inputs(void);

/**
 * @brief Handle "show version" command
 */
void cli_cmd_show_version(void);

/**
 * @brief Handle "show gpio" command (GPIO mappings)
 */
void cli_cmd_show_gpio(void);

#endif // CLI_SHOW_H

