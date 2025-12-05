/**
 * @file telnet_server.cpp
 * @brief Telnet protocol server implementation
 *
 * Handles Telnet IAC protocol, line buffering, and basic option negotiation.
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <esp_log.h>
#include <Arduino.h>

#include "telnet_server.h"
#include "constants.h"
#include "types.h"  // For NetworkConfig
#include "cli_shell.h"  // For cli_shell_execute_command()
#include "cli_history.h"  // For command history (up/down arrow support)
#include "debug.h"  // For debug_register_telnet_callbacks()

static const char *TAG = "TELNET_SRV";

/* Telnet command macros */
#define TELNET_CMD_NULL     0
#define TELNET_CMD_SE       240    // Subnegotiation End
#define TELNET_CMD_NOP      241    // No Operation
#define TELNET_CMD_DM       242    // Data Mark
#define TELNET_CMD_BRK      243    // Break
#define TELNET_CMD_IP       244    // Interrupt Process
#define TELNET_CMD_AO       245    // Abort Output
#define TELNET_CMD_AYT      246    // Are You There
#define TELNET_CMD_EC       247    // Erase Character
#define TELNET_CMD_EL       248    // Erase Line
#define TELNET_CMD_GA       249    // Go Ahead
#define TELNET_CMD_SB       250    // Subnegotiation Begin
#define TELNET_CMD_WILL     251
#define TELNET_CMD_WONT     252
#define TELNET_CMD_DO       253
#define TELNET_CMD_DONT     254
#define TELNET_CMD_IAC      255

/* ============================================================================
 * INITIALIZATION & CONTROL
 * ============================================================================ */

TelnetServer* telnet_server_create(uint16_t port, NetworkConfig *network_config)
{
  TelnetServer *server = (TelnetServer*)malloc(sizeof(TelnetServer));
  if (!server) {
    ESP_LOGE(TAG, "Failed to allocate Telnet server");
    return NULL;
  }

  memset(server, 0, sizeof(TelnetServer));

  server->tcp_server = tcp_server_create(port);
  if (!server->tcp_server) {
    free(server);
    return NULL;
  }

  server->port = port;
  server->network_config = network_config;  // Store config pointer
  server->parse_state = TELNET_STATE_NONE;
  server->escape_seq_state = 0;             // Initialize escape sequence state
  server->input_pos = 0;
  server->input_ready = 0;
  server->echo_enabled = 1;
  server->linemode_enabled = 1;

  // SECURITY: Initialize authentication (enabled by default)
  server->auth_required = 1;
  server->auth_state = TELNET_AUTH_WAITING;
  server->auth_attempts = 0;
  server->auth_lockout_time = 0;
  memset(server->auth_username, 0, sizeof(server->auth_username));

  // Register debug callbacks (cast TelnetServer functions to void*)
  debug_register_telnet_callbacks(
      (int (*)(void*, const char*))telnet_server_write,
      (int (*)(void*, const char*))telnet_server_writeline);

  ESP_LOGI(TAG, "Telnet server created for port %d", port);
  return server;
}

int telnet_server_start(TelnetServer *server)
{
  if (!server || !server->tcp_server) {
    return -1;
  }

  int ret = tcp_server_start(server->tcp_server);
  if (ret != 0) {
    return ret;
  }

  ESP_LOGI(TAG, "Telnet server started");
  return 0;
}

int telnet_server_stop(TelnetServer *server)
{
  if (!server || !server->tcp_server) {
    return -1;
  }

  tcp_server_stop(server->tcp_server);
  ESP_LOGI(TAG, "Telnet server stopped");
  return 0;
}

void telnet_server_destroy(TelnetServer *server)
{
  if (!server) {
    return;
  }

  telnet_server_stop(server);
  if (server->tcp_server) {
    tcp_server_destroy(server->tcp_server);
  }
  free(server);
}

/* ============================================================================
 * CLIENT MANAGEMENT
 * ============================================================================ */

uint8_t telnet_server_client_connected(const TelnetServer *server)
{
  if (!server) {
    return 0;
  }

  return tcp_server_client_is_connected(server->tcp_server, 0);
}

int telnet_server_disconnect_client(TelnetServer *server)
{
  if (!server || !server->tcp_server) {
    return -1;
  }

  server->input_pos = 0;
  server->input_ready = 0;
  server->parse_state = TELNET_STATE_NONE;

  return tcp_server_disconnect_client(server->tcp_server, 0);
}

/* ============================================================================
 * TELNET IAC PROTOCOL HANDLING
 * ============================================================================ */

static void telnet_send_iac_command(TelnetServer *server, uint8_t cmd, uint8_t opt)
{
  uint8_t buf[3] = {TELNET_CMD_IAC, cmd, opt};
  tcp_server_send(server->tcp_server, 0, buf, 3);
}

static void telnet_handle_iac_command(TelnetServer *server, uint8_t cmd, uint8_t opt)
{
  switch (cmd) {
    case TELNET_CMD_WILL:
      // Client says it will enable option
      if (opt == TELNET_OPT_ECHO) {
        server->options.client_echo = 1;
        telnet_send_iac_command(server, TELNET_CMD_DO, TELNET_OPT_ECHO);
      } else if (opt == TELNET_OPT_SUPPRESS_GA) {
        server->options.client_suppress_ga = 1;
        telnet_send_iac_command(server, TELNET_CMD_DO, TELNET_OPT_SUPPRESS_GA);
      }
      break;

    case TELNET_CMD_WONT:
      // Client says it won't enable option
      if (opt == TELNET_OPT_ECHO) {
        server->options.client_echo = 0;
        telnet_send_iac_command(server, TELNET_CMD_DONT, TELNET_OPT_ECHO);
      } else if (opt == TELNET_OPT_SUPPRESS_GA) {
        server->options.client_suppress_ga = 0;
        telnet_send_iac_command(server, TELNET_CMD_DONT, TELNET_OPT_SUPPRESS_GA);
      }
      break;

    case TELNET_CMD_DO:
      // Client asks server to enable option
      if (opt == TELNET_OPT_ECHO) {
        server->options.server_echo = 1;
        telnet_send_iac_command(server, TELNET_CMD_WILL, TELNET_OPT_ECHO);
      } else if (opt == TELNET_OPT_SUPPRESS_GA) {
        server->options.server_suppress_ga = 1;
        telnet_send_iac_command(server, TELNET_CMD_WILL, TELNET_OPT_SUPPRESS_GA);
      }
      break;

    case TELNET_CMD_DONT:
      // Client asks server not to enable option
      if (opt == TELNET_OPT_ECHO) {
        server->options.server_echo = 0;
        telnet_send_iac_command(server, TELNET_CMD_WONT, TELNET_OPT_ECHO);
      } else if (opt == TELNET_OPT_SUPPRESS_GA) {
        server->options.server_suppress_ga = 0;
        telnet_send_iac_command(server, TELNET_CMD_WONT, TELNET_OPT_SUPPRESS_GA);
      }
      break;

    default:
      break;
  }
}

/* ============================================================================
 * AUTHENTICATION (v3.0+)
 * ============================================================================ */

// Authentication configuration
#define TELNET_MAX_AUTH_ATTEMPTS 3
#define TELNET_LOCKOUT_TIME_MS 30000  // 30 second lockout
// Credentials now come from NetworkConfig in telnet_server->network_config

static void telnet_send_auth_prompt(TelnetServer *server) {
  if (server->auth_state == TELNET_AUTH_WAITING) {
    // Full banner only for initial prompt
    telnet_server_writeline(server, "");
    telnet_server_writeline(server, "=== Telnet Server (v3.0) ===");
    telnet_server_writeline(server, "LOGIN REQUIRED");
    telnet_server_writeline(server, "");
    telnet_server_write(server, "Username: ");
  } else if (server->auth_state == TELNET_AUTH_USERNAME) {
    // Just password prompt (username already entered successfully)
    telnet_server_write(server, "Password: ");
  }
}

// Simpler version that just sends the prompt without full banner
static void telnet_send_retry_prompt(TelnetServer *server) {
  if (server->auth_state == TELNET_AUTH_WAITING) {
    // Simplified retry prompt
    telnet_server_write(server, "Username: ");
  } else if (server->auth_state == TELNET_AUTH_USERNAME) {
    telnet_server_write(server, "Password: ");
  }
}

static void telnet_handle_auth_input(TelnetServer *server, const char *input) {
  if (!input || !server) return;

  // Check if in lockout period
  if (server->auth_lockout_time > 0) {
    if (millis() < server->auth_lockout_time) {
      telnet_server_writeline(server, "ERROR: Too many failed attempts. Please try again later.");
      server->input_pos = 0;
      server->input_ready = 0;
      return;
    } else {
      // Lockout expired
      server->auth_lockout_time = 0;
      server->auth_attempts = 0;
      telnet_send_auth_prompt(server);
      server->input_pos = 0;
      server->input_ready = 0;
      return;
    }
  }

  if (server->auth_state == TELNET_AUTH_WAITING) {
    // Username entry
    strncpy(server->auth_username, input, sizeof(server->auth_username) - 1);
    server->auth_username[sizeof(server->auth_username) - 1] = '\0';
    server->auth_state = TELNET_AUTH_USERNAME;
    telnet_send_auth_prompt(server);
  } else if (server->auth_state == TELNET_AUTH_USERNAME) {
    // Password entry
    // Use credentials from config (or fallback to defaults if config not available)
    const char *expected_user = (server->network_config) ? server->network_config->telnet_username : "admin";
    const char *expected_pass = (server->network_config) ? server->network_config->telnet_password : "telnet123";

    if (strcmp(server->auth_username, expected_user) == 0 &&
        strcmp(input, expected_pass) == 0) {
      // Authentication successful!
      server->auth_state = TELNET_AUTH_AUTHENTICATED;
      server->auth_attempts = 0;
      telnet_server_writeline(server, "");
      telnet_server_writeline(server, "Authentication successful. Welcome!");
      telnet_server_writeline(server, "");
    } else {
      // Authentication failed
      server->auth_attempts++;
      telnet_server_writeline(server, "");
      telnet_server_writeline(server, "ERROR: Invalid username or password");

      if (server->auth_attempts >= TELNET_MAX_AUTH_ATTEMPTS) {
        // Trigger lockout
        server->auth_lockout_time = millis() + TELNET_LOCKOUT_TIME_MS;
        telnet_server_writeline(server, "Too many failed attempts. Locking out for 30 seconds.");
        telnet_server_writeline(server, "");
      } else {
        // Reset to username prompt (show simple retry prompt, not full banner)
        server->auth_state = TELNET_AUTH_WAITING;
        memset(server->auth_username, 0, sizeof(server->auth_username));
        telnet_send_retry_prompt(server);
      }
    }
  }

  server->input_pos = 0;
  server->input_ready = 0;
}

/* ============================================================================
 * INPUT PROCESSING
 * ============================================================================ */

// Store for IAC state machine
static uint8_t telnet_iac_cmd = 0;

static void telnet_process_input(TelnetServer *server, uint8_t byte)
{
  switch (server->parse_state) {
    case TELNET_STATE_NONE:
      // ====================================================================
      // ARROW KEY / ESCAPE SEQUENCE HANDLING
      // ====================================================================
      if (server->escape_seq_state == 1) {
        // We received ESC, now expecting [
        if (byte == '[') {
          server->escape_seq_state = 2;
          return;
        } else {
          // Invalid sequence, reset state
          server->escape_seq_state = 0;
          // Fall through to normal character processing
        }
      } else if (server->escape_seq_state == 2) {
        // We received ESC[, now expecting arrow key (A, B, C, D)
        server->escape_seq_state = 0;

        if (byte == 'A') {
          // Up arrow - get previous command from history
          const char* prev_cmd = cli_history_get_prev();
          if (prev_cmd != NULL && prev_cmd[0] != '\0') {
            // Clear current line and redraw with history command
            memset(server->input_buffer, 0, TELNET_INPUT_BUFFER_SIZE);
            strncpy(server->input_buffer, prev_cmd, TELNET_INPUT_BUFFER_SIZE - 1);
            server->input_pos = strlen(server->input_buffer);

            // Redraw line: clear current, show new
            if (server->echo_enabled) {
              // Send clear line + redraw
              telnet_server_write(server, "\r> ");
              telnet_server_write(server, server->input_buffer);
            }
          }
          return;
        } else if (byte == 'B') {
          // Down arrow - get next command from history
          const char* next_cmd = cli_history_get_next();
          if (next_cmd != NULL) {
            // Clear current line and redraw with history command
            memset(server->input_buffer, 0, TELNET_INPUT_BUFFER_SIZE);
            if (next_cmd[0] != '\0') {
              strncpy(server->input_buffer, next_cmd, TELNET_INPUT_BUFFER_SIZE - 1);
              server->input_pos = strlen(server->input_buffer);
            } else {
              server->input_pos = 0;
            }

            // Redraw line
            if (server->echo_enabled) {
              telnet_server_write(server, "\r> ");
              if (server->input_pos > 0) {
                telnet_server_write(server, server->input_buffer);
              }
            }
          }
          return;
        } else if (byte == 'C' || byte == 'D') {
          // Left/Right arrows - ignore for now
          return;
        }
        // For any other character, fall through to normal processing
      }

      // Check if this is start of escape sequence
      if (byte == 0x1B) {  // ESC
        server->escape_seq_state = 1;
        // NOTE: Don't echo ESC character - it's part of control sequence
        return;
      }

      // ====================================================================
      // NORMAL CHARACTER PROCESSING
      // ====================================================================
      if (byte == TELNET_CMD_IAC) {
        server->parse_state = TELNET_STATE_IAC;
      } else if (byte == '\r') {
        // Carriage return - ignore (waiting for \n)
      } else if (byte == '\n') {
        // Line complete (with boundary check)
        // SECURITY FIX: Ensure we don't write past buffer boundary
        if (server->input_pos >= TELNET_INPUT_BUFFER_SIZE) {
          server->input_pos = TELNET_INPUT_BUFFER_SIZE - 1;
        }
        server->input_buffer[server->input_pos] = '\0';
        server->input_ready = 1;
        server->input_pos = 0;

        // Echo if enabled
        if (server->echo_enabled) {
          tcp_server_send(server->tcp_server, 0, (uint8_t*)"\r\n", 2);
        }
      } else if (byte >= 32 && byte < 127) {
        // Printable character (with strict boundary check)
        // NOTE: Skip if we're in the middle of processing escape sequence
        // (escape sequences contain printable chars like '[', 'A', 'B', etc.)
        if (server->escape_seq_state == 0 && server->input_pos < TELNET_INPUT_BUFFER_SIZE - 1) {
          server->input_buffer[server->input_pos++] = byte;

          // Echo back
          if (server->echo_enabled) {
            tcp_server_send(server->tcp_server, 0, &byte, 1);
          }
        }
      } else if (byte == 8 || byte == 127) {
        // Backspace - always process, even if in escape sequence (to allow correction)
        // Reset escape state if user is trying to backspace
        server->escape_seq_state = 0;

        if (server->input_pos > 0) {
          server->input_pos--;

          if (server->echo_enabled) {
            tcp_server_send(server->tcp_server, 0, (uint8_t*)"\b \b", 3);
          }
        }
      }
      break;

    case TELNET_STATE_IAC:
      // SECURITY FIX: Properly handle IAC state machine
      if (byte >= TELNET_CMD_WILL && byte <= TELNET_CMD_DONT) {
        // Store command byte for next state
        telnet_iac_cmd = byte;
        server->parse_state = TELNET_STATE_IAC_CMD;
      } else if (byte == TELNET_CMD_IAC) {
        // Escaped IAC (literal 255)
        if (server->input_pos < TELNET_INPUT_BUFFER_SIZE - 1) {
          server->input_buffer[server->input_pos++] = byte;
          if (server->echo_enabled) {
            tcp_server_send(server->tcp_server, 0, &byte, 1);
          }
        }
        server->parse_state = TELNET_STATE_NONE;
      } else {
        server->parse_state = TELNET_STATE_NONE;
      }
      break;

    case TELNET_STATE_IAC_CMD:
      // Now we have: IAC, cmd (WILL/WONT/DO/DONT), opt
      // cmd is stored in telnet_iac_cmd, current byte is the option
      // For now, we just ignore the negotiation and return to NONE state
      // (In a full implementation, we'd handle the negotiation here)
      server->parse_state = TELNET_STATE_NONE;
      break;

    default:
      server->parse_state = TELNET_STATE_NONE;
      break;
  }
}

/* ============================================================================
 * LINE-ORIENTED I/O
 * ============================================================================ */

int telnet_server_readline(TelnetServer *server, char *buf, size_t max_len)
{
  if (!server || !buf || !max_len) {
    return -1;
  }

  if (!server->input_ready) {
    return 0;  // No complete line yet
  }

  int len = strlen(server->input_buffer);
  if (len >= (int)max_len) {
    len = max_len - 1;
  }

  strncpy(buf, server->input_buffer, len);
  buf[len] = '\0';

  server->input_ready = 0;
  return len;
}

int telnet_server_writeline(TelnetServer *server, const char *line)
{
  if (!server || !line || !server->tcp_server) {
    return -1;
  }

  int len = tcp_server_send(server->tcp_server, 0, (uint8_t*)line, strlen(line));
  if (len > 0) {
    tcp_server_send(server->tcp_server, 0, (uint8_t*)"\r\n", 2);
    len += 2;
  }

  return len;
}

int telnet_server_writelinef(TelnetServer *server, const char *format, ...)
{
  if (!server || !format) {
    return -1;
  }

  char buf[256];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buf, sizeof(buf) - 1, format, args);
  va_end(args);

  if (len < 0 || len >= (int)sizeof(buf)) {
    return -1;
  }

  return telnet_server_writeline(server, buf);
}

int telnet_server_write(TelnetServer *server, const char *text)
{
  if (!server || !text || !server->tcp_server) {
    return -1;
  }

  return tcp_server_send(server->tcp_server, 0, (uint8_t*)text, strlen(text));
}

int telnet_server_writef(TelnetServer *server, const char *format, ...)
{
  if (!server || !format) {
    return -1;
  }

  char buf[256];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buf, sizeof(buf) - 1, format, args);
  va_end(args);

  if (len < 0 || len >= (int)sizeof(buf)) {
    return -1;
  }

  return telnet_server_write(server, buf);
}

int telnet_server_writech(TelnetServer *server, char ch)
{
  if (!server || !server->tcp_server) {
    return -1;
  }

  return tcp_server_send(server->tcp_server, 0, (uint8_t*)&ch, 1);
}

/* ============================================================================
 * STATUS & INFORMATION
 * ============================================================================ */

uint8_t telnet_server_has_input(const TelnetServer *server)
{
  return server ? server->input_ready : 0;
}

uint16_t telnet_server_available(const TelnetServer *server)
{
  if (!server || !server->tcp_server) {
    return 0;
  }

  return tcp_server_available(server->tcp_server, 0);
}

/* ============================================================================
 * BACKGROUND TASKS
 * ============================================================================ */

int telnet_server_loop(TelnetServer *server)
{
  if (!server || !server->tcp_server) {
    return 0;
  }

  int events = 0;

  // Process TCP server events
  events += tcp_server_loop(server->tcp_server);

  // Accept new connections and initialize auth on new client
  if (tcp_server_accept(server->tcp_server) > 0) {
    // New client detected - reset all state
    server->escape_seq_state = 0;  // Reset escape sequence parser
    server->input_pos = 0;         // Clear input buffer
    memset(server->input_buffer, 0, TELNET_INPUT_BUFFER_SIZE);

    if (server->auth_required) {
      server->auth_state = TELNET_AUTH_WAITING;
      server->auth_attempts = 0;
      server->auth_lockout_time = 0;
      memset(server->auth_username, 0, sizeof(server->auth_username));
      telnet_send_auth_prompt(server);
      events++;
    } else {
      // Auth disabled - go straight to authenticated
      server->auth_state = TELNET_AUTH_AUTHENTICATED;
    }
  }

  // Process incoming data from client
  if (telnet_server_client_connected(server)) {
    uint8_t byte;
    while (tcp_server_recv_byte(server->tcp_server, 0, &byte) > 0) {
      telnet_process_input(server, byte);
      events++;
    }

    // SECURITY FIX: Process authentication if input is ready and auth is needed
    if (server->input_ready && server->auth_required && server->auth_state != TELNET_AUTH_AUTHENTICATED) {
      telnet_handle_auth_input(server, server->input_buffer);
      // NOTE: telnet_handle_auth_input() already sends the next prompt if needed
      // DO NOT send prompt again here to avoid duplication!
    }

    // FEATURE: Execute CLI commands if authenticated and input is ready
    if (server->input_ready && server->auth_state == TELNET_AUTH_AUTHENTICATED) {
      // Set Telnet as the output destination for this command
      debug_set_telnet_output(server);

      // Execute the command through CLI shell
      cli_shell_execute_command(server->input_buffer);

      // Restore output to Serial
      debug_clear_telnet_output();

      // Show the CLI prompt after command execution
      telnet_server_write(server, "> ");

      // Clear the input buffer
      server->input_pos = 0;
      server->input_ready = 0;
      memset(server->input_buffer, 0, TELNET_INPUT_BUFFER_SIZE);
      events++;
    }
  }

  return events;
}

/* ============================================================================
 * DEBUGGING
 * ============================================================================ */

void telnet_server_print_status(const TelnetServer *server)
{
  if (!server) {
    printf("Telnet Server: NULL\n");
    return;
  }

  printf("\n=== Telnet Server Status ===\n");
  printf("Port: %d\n", server->port);
  printf("Client connected: %s\n", telnet_server_client_connected(server) ? "Yes" : "No");
  printf("Echo enabled: %s\n", server->echo_enabled ? "Yes" : "No");
  printf("Input buffer: %d/%d bytes\n", server->input_pos, TELNET_BUFFER_SIZE);
  printf("Input ready: %s\n", server->input_ready ? "Yes" : "No");
  printf("============================\n\n");
}
