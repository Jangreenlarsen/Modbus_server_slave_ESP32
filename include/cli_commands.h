/**
 * @file cli_commands.h
 * @brief CLI `set` command handlers (LAYER 7)
 *
 * LAYER 7: User Interface - CLI Commands
 * Responsibility: Implement all `set` commands
 *
 * This file handles:
 * - set counter ... (configuration)
 * - set timer ... (configuration)
 * - set hostname <name>
 * - set baud <rate>
 * - set id <slave_id>
 * - set reg <addr> <value>
 * - set coil <idx> <0|1>
 *
 * Each command is a standalone handler function
 * No cross-command dependencies (modular)
 *
 * Does NOT handle:
 * - Command parsing (→ cli_parser.h)
 * - `show` commands (→ cli_show.h)
 * - Serial I/O (→ cli_shell.h)
 */

#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#include <stdint.h>
#include "types.h"

/* ============================================================================
 * PUBLIC API - Command Handlers
 * ============================================================================ */

/**
 * @brief Handle "set counter" command
 * @param argc Argument count
 * @param argv Argument array
 */
void cli_cmd_set_counter(uint8_t argc, char* argv[]);

/**
 * @brief Handle "set timer" command
 * @param argc Argument count
 * @param argv Argument array
 */
void cli_cmd_set_timer(uint8_t argc, char* argv[]);

/**
 * @brief Handle "reset counter" command
 * @param argc Argument count
 * @param argv Argument array
 */
void cli_cmd_reset_counter(uint8_t argc, char* argv[]);

/**
 * @brief Handle "clear counters" command
 */
void cli_cmd_clear_counters(void);

/**
 * @brief Handle "set hostname" command
 * @param hostname New hostname (max 32 chars)
 */
void cli_cmd_set_hostname(const char* hostname);

/**
 * @brief Handle "set baud" command
 * @param baud Baud rate
 */
void cli_cmd_set_baud(uint32_t baud);

/**
 * @brief Handle "set id" command
 * @param id Slave ID (0-247)
 */
void cli_cmd_set_id(uint8_t id);

/**
 * @brief Handle "set reg" command (write holding register)
 * @param addr Register address
 * @param value Register value
 */
void cli_cmd_set_reg(uint16_t addr, uint16_t value);

/**
 * @brief Handle "set coil" command
 * @param idx Coil index
 * @param value 0 or 1
 */
void cli_cmd_set_coil(uint16_t idx, uint8_t value);

/**
 * @brief Handle "set gpio" command (map GPIO pin to coil/input)
 * @param argc Argument count
 * @param argv Argument array
 */
void cli_cmd_set_gpio(uint8_t argc, char* argv[]);

/**
 * @brief Handle "save" command (persist config)
 */
void cli_cmd_save(void);

/**
 * @brief Handle "load" command (load config from storage)
 */
void cli_cmd_load(void);

/**
 * @brief Handle "defaults" command (reset to factory defaults)
 */
void cli_cmd_defaults(void);

/**
 * @brief Handle "reboot" command
 */
void cli_cmd_reboot(void);

#endif // CLI_COMMANDS_H

