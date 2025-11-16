/**
 * @file cli_parser.h
 * @brief CLI command parser and dispatcher (LAYER 7)
 *
 * LAYER 7: User Interface - CLI Parser
 * Responsibility: Tokenize input, dispatch to handlers
 *
 * This file handles:
 * - Command line tokenization
 * - Alias normalization (sh→show, wr→write, etc.)
 * - Main command dispatch (show, set, read, write, etc.)
 * - Error messages
 *
 * Does NOT handle:
 * - Command implementation (→ cli_commands.h, cli_show.h)
 * - Serial I/O (→ cli_shell.h)
 * - Command history (→ cli_history.h)
 */

#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include <stdint.h>
#include "types.h"

/* ============================================================================
 * PUBLIC API
 * ============================================================================ */

/**
 * @brief Parse and execute a command line
 * @param line Command line string (will be modified during tokenization)
 * @return true if command executed, false if error
 */
bool cli_parser_execute(char* line);

/**
 * @brief Print command help
 */
void cli_parser_print_help(void);

#endif // CLI_PARSER_H

