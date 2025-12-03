/**
 * @file cli_shell.cpp
 * @brief CLI shell implementation - serial I/O and command loop (LAYER 7)
 *
 * Handles:
 * - Non-blocking serial input reading
 * - Command line buffering
 * - Prompt display
 * - Command execution via cli_parser
 * - Command history navigation (up/down arrows)
 * - Remote echo control
 */

#include "cli_shell.h"
#include "cli_parser.h"
#include "cli_history.h"
#include "debug.h"
#include <Arduino.h>
#include <string.h>

/* Forward declarations */
extern void cli_parser_execute_st_upload(uint8_t program_id, const char* source_code);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * Trim whitespace from both ends of a string (in-place)
 * @param str Null-terminated string to trim
 * @param len Pointer to string length (will be updated)
 */
static void cli_trim_string(char* str, uint16_t* len) {
  if (!str || !len || *len == 0) return;

  // Trim from end
  while (*len > 0 && (str[*len - 1] == ' ' || str[*len - 1] == '\t')) {
    str[*len - 1] = '\0';
    (*len)--;
  }

  // Trim from start
  uint16_t start = 0;
  while (start < *len && (str[start] == ' ' || str[start] == '\t')) {
    start++;
  }

  if (start > 0) {
    // Shift remaining string to the beginning
    for (uint16_t i = start; i < *len; i++) {
      str[i - start] = str[i];
    }
    *len -= start;
    str[*len] = '\0';
  }
}

/* ============================================================================
 * INPUT BUFFER & STATE
 * ============================================================================ */

#define CLI_INPUT_BUFFER_SIZE 256
#define CLI_UPLOAD_BUFFER_SIZE 5000

#define CLI_MODE_NORMAL 0
#define CLI_MODE_ST_UPLOAD 1

static char cli_input_buffer[CLI_INPUT_BUFFER_SIZE];
static uint16_t cli_input_pos = 0;          // Length of input
static uint16_t cli_cursor_pos = 0;         // Cursor position (0 = start)
static uint8_t cli_initialized = 0;
static uint8_t cli_remote_echo_enabled = 1;  // Default: echo enabled
static uint8_t cli_escape_seq_state = 0;    // For parsing arrow keys

// Multi-line ST Logic Upload Mode
static uint8_t cli_mode = CLI_MODE_NORMAL;
static uint8_t cli_upload_program_id = 0xFF;
static char cli_upload_buffer[CLI_UPLOAD_BUFFER_SIZE];
static uint16_t cli_upload_buffer_pos = 0;

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void cli_shell_init(void) {
  if (cli_initialized) return;

  cli_history_init();
  debug_println("\nModbus CLI Ready. Type 'help' for commands.\n");
  debug_print("> ");

  cli_input_pos = 0;
  cli_cursor_pos = 0;
  memset(cli_input_buffer, 0, CLI_INPUT_BUFFER_SIZE);
  cli_escape_seq_state = 0;
  cli_initialized = 1;
}

/* ============================================================================
 * MULTI-LINE ST UPLOAD MODE CONTROL
 * ============================================================================ */

void cli_shell_start_st_upload(uint8_t program_id) {
  cli_mode = CLI_MODE_ST_UPLOAD;
  cli_upload_program_id = program_id;
  cli_upload_buffer_pos = 0;
  memset(cli_upload_buffer, 0, CLI_UPLOAD_BUFFER_SIZE);

  // Clear input buffer for next command entry
  cli_input_pos = 0;
  cli_cursor_pos = 0;
  memset(cli_input_buffer, 0, CLI_INPUT_BUFFER_SIZE);

  debug_println("Entering ST Logic upload mode. Type code and end with 'END_UPLOAD':");
  debug_print(">>> ");
}

void cli_shell_reset_upload_mode(void) {
  cli_mode = CLI_MODE_NORMAL;
  cli_upload_program_id = 0xFF;
  cli_upload_buffer_pos = 0;
  memset(cli_upload_buffer, 0, CLI_UPLOAD_BUFFER_SIZE);

  debug_print("> ");
}

uint8_t cli_shell_is_in_upload_mode(void) {
  return (cli_mode == CLI_MODE_ST_UPLOAD);
}

/* ============================================================================
 * REMOTE ECHO CONTROL
 * ============================================================================ */

void cli_shell_set_remote_echo(uint8_t enabled) {
  cli_remote_echo_enabled = enabled ? 1 : 0;
}

uint8_t cli_shell_get_remote_echo(void) {
  return cli_remote_echo_enabled;
}

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

static void cli_redraw_line(void) {
  // Choose prompt based on mode
  const char* prompt = (cli_mode == CLI_MODE_ST_UPLOAD) ? ">>> " : "> ";

  // Move to start of line after prompt
  Serial.write('\r');  // Carriage return
  Serial.print(prompt);

  // Clear rest of line with spaces
  for (uint16_t i = 0; i < CLI_INPUT_BUFFER_SIZE - 10; i++) {
    Serial.write(' ');
  }

  // Go back to start of input area
  Serial.write('\r');
  Serial.print(prompt);

  // Display buffer content
  if (cli_input_pos > 0) {
    for (uint16_t i = 0; i < cli_input_pos; i++) {
      Serial.write(cli_input_buffer[i]);
    }
  }

  // Position cursor at correct location
  if (cli_cursor_pos < cli_input_pos) {
    // Move cursor back to position
    uint16_t move_back = cli_input_pos - cli_cursor_pos;
    for (uint16_t i = 0; i < move_back; i++) {
      Serial.write('\b');  // Backspace
    }
  }

  Serial.flush();  // Force output to terminal immediately
}

static void cli_insert_char_at_cursor(char c) {
  if (cli_input_pos >= CLI_INPUT_BUFFER_SIZE - 1) {
    return;  // Buffer full
  }

  if (!cli_remote_echo_enabled) {
    // Simple case: no echo, just add to buffer
    if (cli_cursor_pos < cli_input_pos) {
      // Shift characters forward
      for (uint16_t i = cli_input_pos; i > cli_cursor_pos; i--) {
        cli_input_buffer[i] = cli_input_buffer[i - 1];
      }
    }
    cli_input_buffer[cli_cursor_pos] = c;
    cli_input_pos++;
    cli_cursor_pos++;
    return;
  }

  // Echo enabled: need to redraw
  if (cli_cursor_pos == cli_input_pos) {
    // Simple case: cursor at end, just append
    cli_input_buffer[cli_cursor_pos] = c;
    cli_input_pos++;
    cli_cursor_pos++;
    Serial.write(c);
    Serial.flush();
  } else {
    // Complex case: cursor in middle, need to redraw
    // Shift characters forward
    for (uint16_t i = cli_input_pos; i > cli_cursor_pos; i--) {
      cli_input_buffer[i] = cli_input_buffer[i - 1];
    }
    cli_input_buffer[cli_cursor_pos] = c;
    cli_input_pos++;
    cli_cursor_pos++;

    // Redraw from current position to end
    for (uint16_t i = cli_cursor_pos - 1; i < cli_input_pos; i++) {
      Serial.write(cli_input_buffer[i]);
    }

    // Move cursor back to right position
    uint16_t move_back = cli_input_pos - cli_cursor_pos;
    for (uint16_t i = 0; i < move_back; i++) {
      Serial.write('\b');
    }
    Serial.flush();
  }
}

/* ============================================================================
 * MAIN LOOP
 * ============================================================================ */

void cli_shell_loop(void) {
  // Non-blocking serial input reading
  while (Serial.available() > 0) {
    int c = Serial.read();

    // ========================================================================
    // ESCAPE SEQUENCE PARSING FOR ARROW KEYS
    // ========================================================================

    if (cli_escape_seq_state == 1) {
      // We received ESC, now expecting [
      if (c == '[') {
        cli_escape_seq_state = 2;
        continue;
      } else {
        // Invalid sequence, reset state and process this character
        cli_escape_seq_state = 0;
        // Fall through to normal character processing
      }
    } else if (cli_escape_seq_state == 2) {
      // We received ESC[, now expecting arrow key (A, B, C, D)
      cli_escape_seq_state = 0;

      if (c == 'A') {
        // Up arrow - get previous command
        const char* prev_cmd = cli_history_get_prev();
        if (prev_cmd != NULL && prev_cmd[0] != '\0') {
          cli_redraw_line();  // Clear current line
          memset(cli_input_buffer, 0, CLI_INPUT_BUFFER_SIZE);
          strncpy(cli_input_buffer, prev_cmd, CLI_INPUT_BUFFER_SIZE - 1);
          cli_input_pos = strlen(cli_input_buffer);
          cli_cursor_pos = cli_input_pos;  // Cursor at end
          cli_redraw_line();
        }
        continue;
      } else if (c == 'B') {
        // Down arrow - get next command
        const char* next_cmd = cli_history_get_next();
        if (next_cmd != NULL) {
          cli_redraw_line();  // Clear current line
          memset(cli_input_buffer, 0, CLI_INPUT_BUFFER_SIZE);
          if (next_cmd[0] != '\0') {
            strncpy(cli_input_buffer, next_cmd, CLI_INPUT_BUFFER_SIZE - 1);
            cli_input_pos = strlen(cli_input_buffer);
          } else {
            cli_input_pos = 0;
          }
          cli_cursor_pos = cli_input_pos;  // Cursor at end
          cli_redraw_line();
        }
        continue;
      } else if (c == 'C') {
        // Right arrow - move cursor forward
        if (cli_cursor_pos < cli_input_pos) {
          cli_cursor_pos++;
        }
        // Always send the control sequence back
        Serial.write(0x1B);
        Serial.write('[');
        Serial.write('C');
        Serial.flush();
        continue;
      } else if (c == 'D') {
        // Left arrow - move cursor backward
        if (cli_cursor_pos > 0) {
          cli_cursor_pos--;
        }
        // Always send the control sequence back
        Serial.write(0x1B);
        Serial.write('[');
        Serial.write('D');
        Serial.flush();
        continue;
      }
      // For any other character, fall through to normal processing
    }

    // Check if this is start of escape sequence
    if (c == 0x1B) {
      cli_escape_seq_state = 1;
      continue;
    }

    // ========================================================================
    // NORMAL CHARACTER PROCESSING
    // ========================================================================

    if (c == '\r' || c == '\n') {
      // End of line
      cli_input_buffer[cli_input_pos] = '\0';

      if (cli_remote_echo_enabled) {
        debug_println("");  // Echo newline
      } else {
        Serial.write('\n');  // Just newline, no echo
      }

      // ====================================================================
      // ST LOGIC UPLOAD MODE
      // ====================================================================
      if (cli_mode == CLI_MODE_ST_UPLOAD) {
        // Trim whitespace from input in upload mode
        cli_trim_string(cli_input_buffer, &cli_input_pos);

        // Check for END_UPLOAD command (case-insensitive)
        if (strcasecmp(cli_input_buffer, "END_UPLOAD") == 0) {
          // End of upload - compile and return to normal mode
          debug_println("Compiling ST Logic program...");

          // Null-terminate the upload buffer (CRITICAL!)
          if (cli_upload_buffer_pos < CLI_UPLOAD_BUFFER_SIZE) {
            cli_upload_buffer[cli_upload_buffer_pos] = '\0';
          }

          // Execute upload with collected code
          cli_parser_execute_st_upload(cli_upload_program_id, cli_upload_buffer);

          // Reset to normal mode
          cli_shell_reset_upload_mode();
        } else if (cli_input_pos > 0) {
          // Add line to upload buffer (with newline)
          if (cli_upload_buffer_pos + cli_input_pos + 1 < CLI_UPLOAD_BUFFER_SIZE) {
            // Copy line
            strncpy(&cli_upload_buffer[cli_upload_buffer_pos],
                    cli_input_buffer,
                    cli_input_pos);
            cli_upload_buffer_pos += cli_input_pos;

            // Add newline
            cli_upload_buffer[cli_upload_buffer_pos] = '\n';
            cli_upload_buffer_pos++;

            debug_print(">>> ");  // Show upload prompt for next line
          } else {
            debug_println("ERROR: Upload buffer full!");
            cli_shell_reset_upload_mode();
          }
        } else {
          // Empty line in upload mode - just show prompt again
          debug_print(">>> ");
        }

        // Reset input buffer for next line
        cli_input_pos = 0;
        cli_cursor_pos = 0;
        memset(cli_input_buffer, 0, CLI_INPUT_BUFFER_SIZE);
      }
      // ====================================================================
      // NORMAL COMMAND MODE
      // ====================================================================
      else {
        if (cli_input_pos > 0) {
          // Trim whitespace from input
          cli_trim_string(cli_input_buffer, &cli_input_pos);

          // Store in history and execute (if not empty after trim)
          if (cli_input_pos > 0) {
            cli_history_add(cli_input_buffer);
            cli_history_reset_nav();
            cli_parser_execute(cli_input_buffer);
          }
        }

        // Reset buffer and show new prompt
        cli_input_pos = 0;
        cli_cursor_pos = 0;
        memset(cli_input_buffer, 0, CLI_INPUT_BUFFER_SIZE);
        debug_print("> ");
      }
    }
    else if (c == 0x08 || c == 0x7F) {
      // Backspace - delete character before cursor
      if (cli_cursor_pos > 0) {
        cli_cursor_pos--;

        // Shift characters back
        for (uint16_t i = cli_cursor_pos; i < cli_input_pos - 1; i++) {
          cli_input_buffer[i] = cli_input_buffer[i + 1];
        }
        cli_input_pos--;
        cli_input_buffer[cli_input_pos] = '\0';

        if (cli_remote_echo_enabled) {
          // Send backspace to terminal
          Serial.write('\b');
          Serial.write(' ');
          Serial.write('\b');

          // If there are characters after cursor, redraw them
          if (cli_cursor_pos < cli_input_pos) {
            for (uint16_t i = cli_cursor_pos; i < cli_input_pos; i++) {
              Serial.write(cli_input_buffer[i]);
            }
            Serial.write(' ');  // Clear the last position

            // Move cursor back to original position
            uint16_t move_back = cli_input_pos - cli_cursor_pos + 1;
            for (uint16_t i = 0; i < move_back; i++) {
              Serial.write('\b');
            }
          }
          Serial.flush();
        }
      }
    }
    else if (c >= 32 && c < 127) {
      // Printable character - insert at cursor position
      cli_insert_char_at_cursor(c);
    }
    // Ignore all other characters (control codes, etc.)
  }
}

