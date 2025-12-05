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

#include "telnet_server.h"
#include "constants.h"

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

TelnetServer* telnet_server_create(uint16_t port)
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
  server->parse_state = TELNET_STATE_NONE;
  server->input_pos = 0;
  server->input_ready = 0;
  server->echo_enabled = 1;
  server->linemode_enabled = 1;

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
 * INPUT PROCESSING
 * ============================================================================ */

static void telnet_process_input(TelnetServer *server, uint8_t byte)
{
  switch (server->parse_state) {
    case TELNET_STATE_NONE:
      if (byte == TELNET_CMD_IAC) {
        server->parse_state = TELNET_STATE_IAC;
      } else if (byte == '\r') {
        // Carriage return - ignore (waiting for \n)
      } else if (byte == '\n') {
        // Line complete
        server->input_buffer[server->input_pos] = '\0';
        server->input_ready = 1;
        server->input_pos = 0;

        // Echo if enabled
        if (server->echo_enabled) {
          tcp_server_send(server->tcp_server, 0, (uint8_t*)"\r\n", 2);
        }
      } else if (byte >= 32 && byte < 127) {
        // Printable character
        if (server->input_pos < TELNET_BUFFER_SIZE - 1) {
          server->input_buffer[server->input_pos++] = byte;

          // Echo back
          if (server->echo_enabled) {
            tcp_server_send(server->tcp_server, 0, &byte, 1);
          }
        }
      } else if (byte == 8 || byte == 127) {
        // Backspace
        if (server->input_pos > 0) {
          server->input_pos--;

          if (server->echo_enabled) {
            tcp_server_send(server->tcp_server, 0, (uint8_t*)"\b \b", 3);
          }
        }
      }
      break;

    case TELNET_STATE_IAC:
      if (byte >= TELNET_CMD_WILL && byte <= TELNET_CMD_DONT) {
        server->parse_state = TELNET_STATE_IAC_OPTION;
        // Store command for next byte
        server->parse_state = TELNET_STATE_IAC_CMD;
      } else if (byte == TELNET_CMD_IAC) {
        // Escaped IAC (literal 255)
        if (server->input_pos < TELNET_BUFFER_SIZE - 1) {
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
      // Need to reparse properly - for now, simplify
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

  // Accept new connections
  events += tcp_server_accept(server->tcp_server);

  // Process incoming data from client
  if (telnet_server_client_connected(server)) {
    uint8_t byte;
    while (tcp_server_recv_byte(server->tcp_server, 0, &byte) > 0) {
      telnet_process_input(server, byte);
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
