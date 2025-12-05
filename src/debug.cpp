/**
 * @file debug.cpp
 * @brief Debug output implementation via Serial or Telnet
 *
 * Uses Arduino Serial library (Serial object initialized in main.cpp)
 * Can also redirect output to Telnet when context is set.
 */

#include "debug.h"
#include <Arduino.h>
#include <stdio.h>
#include <stdarg.h>

/* ============================================================================
 * OUTPUT CONTEXT & CALLBACKS
 * ============================================================================ */

// Global context for Telnet output redirection
static void *g_telnet_output_context = NULL;

// Callback functions set by Telnet module (to avoid circular includes)
typedef int (*telnet_write_fn)(void *server, const char *text);
typedef int (*telnet_writeline_fn)(void *server, const char *line);

static telnet_write_fn g_telnet_write_fn = NULL;
static telnet_writeline_fn g_telnet_writeline_fn = NULL;

/**
 * Register Telnet output callbacks
 * Called by telnet_server.cpp to provide function pointers
 */
void debug_register_telnet_callbacks(
    int (*write_fn)(void *server, const char *text),
    int (*writeline_fn)(void *server, const char *line)) {
  g_telnet_write_fn = write_fn;
  g_telnet_writeline_fn = writeline_fn;
}

void debug_set_telnet_output(void *server) {
  g_telnet_output_context = server;
}

void* debug_get_telnet_output(void) {
  return g_telnet_output_context;
}

void debug_clear_telnet_output(void) {
  g_telnet_output_context = NULL;
}

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static void debug_write_to_output(const char *text) {
  if (g_telnet_output_context && g_telnet_write_fn) {
    // Send to Telnet
    g_telnet_write_fn(g_telnet_output_context, text);
  } else {
    // Send to Serial
    Serial.print(text);
  }
}

static void debug_writeline_to_output(const char *text) {
  if (g_telnet_output_context && g_telnet_writeline_fn) {
    // Send to Telnet
    g_telnet_writeline_fn(g_telnet_output_context, text);
  } else {
    // Send to Serial
    Serial.println(text);
  }
}

/* ============================================================================
 * PUBLIC DEBUG FUNCTIONS
 * ============================================================================ */

void debug_println(const char* str) {
  if (str) {
    debug_writeline_to_output(str);
  }
}

void debug_print(const char* str) {
  if (str) {
    debug_write_to_output(str);
  }
}

void debug_print_uint(uint32_t value) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)value);
  debug_write_to_output(buffer);
}

void debug_print_ulong(uint64_t value) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
  debug_write_to_output(buffer);
}

void debug_print_float(double value) {
  // Print with 2 decimal places
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.2f", value);
  debug_write_to_output(buffer);
}

void debug_newline(void) {
  if (g_telnet_output_context && g_telnet_writeline_fn) {
    g_telnet_writeline_fn(g_telnet_output_context, "");
  } else {
    Serial.println();
  }
}

void debug_printf(const char* fmt, ...) {
  char buffer[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  debug_write_to_output(buffer);
}
