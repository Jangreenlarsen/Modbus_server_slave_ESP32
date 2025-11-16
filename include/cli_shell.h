/**
 * @file cli_shell.h
 * @brief CLI shell - serial input/output and command loop (LAYER 7)
 *
 * LAYER 7: User Interface - CLI Shell
 * Responsibility: Serial I/O, prompt display, command execution loop
 *
 * This file handles:
 * - Serial input buffering and line reading
 * - Prompt display
 * - Command loop integration
 * - User feedback
 */

#ifndef cli_shell_H
#define cli_shell_H

#include <stdint.h>
#include "types.h"

/**
 * @brief Initialize CLI shell (called once from setup)
 */
void cli_shell_init(void);

/**
 * @brief Main CLI loop (called from main loop, non-blocking)
 */
void cli_shell_loop(void);

#endif // cli_shell_H
