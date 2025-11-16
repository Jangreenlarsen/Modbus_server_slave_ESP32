/**
 * @file cli_shell.cpp
 * @brief CLI shell implementation - serial I/O and command loop (LAYER 7)
 *
 * Handles:
 * - Non-blocking serial input reading
 * - Command line buffering
 * - Prompt display
 * - Command execution via cli_parser
 */

#include "cli_shell.h"
#include "cli_parser.h"
#include "debug.h"
#include <Arduino.h>
#include <string.h>

/* ============================================================================
 * INPUT BUFFER
 * ============================================================================ */

#define CLI_INPUT_BUFFER_SIZE 256

static char cli_input_buffer[CLI_INPUT_BUFFER_SIZE];
static uint16_t cli_input_pos = 0;
static uint8_t cli_initialized = 0;

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void cli_shell_init(void) {
  if (cli_initialized) return;

  debug_println("\nModbus CLI Ready. Type 'help' for commands.\n");
  debug_print("> ");

  cli_input_pos = 0;
  memset(cli_input_buffer, 0, CLI_INPUT_BUFFER_SIZE);
  cli_initialized = 1;
}

/* ============================================================================
 * MAIN LOOP
 * ============================================================================ */

void cli_shell_loop(void) {
  // Non-blocking serial input reading
  while (Serial.available() > 0) {
    int c = Serial.read();

    if (c == '\r' || c == '\n') {
      // End of line - execute command
      if (cli_input_pos > 0) {
        cli_input_buffer[cli_input_pos] = '\0';
        debug_println("");  // Echo newline

        // Parse and execute command
        cli_parser_execute(cli_input_buffer);

        // Reset buffer and show new prompt
        cli_input_pos = 0;
        memset(cli_input_buffer, 0, CLI_INPUT_BUFFER_SIZE);
        debug_print("> ");
      }
    }
    else if (c == 0x08 || c == 0x7F) {
      // Backspace
      if (cli_input_pos > 0) {
        cli_input_pos--;
        cli_input_buffer[cli_input_pos] = '\0';
        Serial.write(0x08);  // Backspace
        Serial.write(' ');   // Erase
        Serial.write(0x08);  // Move back
      }
    }
    else if (c >= 32 && c < 127) {
      // Printable character
      if (cli_input_pos < CLI_INPUT_BUFFER_SIZE - 1) {
        cli_input_buffer[cli_input_pos++] = c;
        Serial.write(c);  // Echo character
      }
    }
  }
}

