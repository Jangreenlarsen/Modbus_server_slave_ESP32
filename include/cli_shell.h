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

/**
 * @brief Enable/disable remote echo (for serial terminals)
 * @param enabled 1 to enable echo, 0 to disable
 */
void cli_shell_set_remote_echo(uint8_t enabled);

/**
 * @brief Get current remote echo state
 * @return 1 if enabled, 0 if disabled
 */
uint8_t cli_shell_get_remote_echo(void);

/**
 * @brief Start ST Logic multi-line upload mode
 * @param program_id Program ID (0-3) for Logic1-Logic4
 */
void cli_shell_start_st_upload(uint8_t program_id);

/**
 * @brief Reset from ST Logic upload mode back to normal CLI
 */
void cli_shell_reset_upload_mode(void);

/**
 * @brief Check if currently in ST Logic upload mode
 * @return 1 if in upload mode, 0 if in normal mode
 */
uint8_t cli_shell_is_in_upload_mode(void);

#endif // cli_shell_H
