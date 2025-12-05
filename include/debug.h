/**
 * @file debug.h
 * @brief Debug output helpers for serial communication
 *
 * Simple wrappers around Serial.print/println for debugging
 */

#ifndef debug_H
#define debug_H

#include <stdint.h>

/**
 * @brief Print string followed by newline
 * @param str String to print (null-terminated)
 */
void debug_println(const char* str);

/**
 * @brief Print string without newline
 * @param str String to print (null-terminated)
 */
void debug_print(const char* str);

/**
 * @brief Print unsigned integer (decimal)
 * @param value Integer value to print
 */
void debug_print_uint(uint32_t value);

/**
 * @brief Print unsigned long (decimal)
 * @param value Long integer value to print
 */
void debug_print_ulong(uint64_t value);

/**
 * @brief Print double/float (with 2 decimal places)
 * @param value Float value to print
 */
void debug_print_float(double value);

/**
 * @brief Print newline character
 */
void debug_newline(void);

/**
 * @brief Print formatted string (like printf)
 * @param fmt Format string
 * @param ... Variable arguments
 */
void debug_printf(const char* fmt, ...);

/* ============================================================================
 * OUTPUT CONTEXT - Allow redirecting debug output to different destinations
 * ============================================================================ */

/**
 * @brief Set Telnet server as the output destination for debug functions
 * @param server Telnet server instance (void* to avoid circular includes, NULL to disable)
 *
 * When a Telnet server is set, all debug_print* calls will send output to the
 * Telnet client instead of Serial. This allows CLI commands executed via Telnet
 * to have their output displayed to the Telnet user.
 */
void debug_set_telnet_output(void *server);

/**
 * @brief Get current Telnet output context
 * @return Pointer to Telnet server (void*) or NULL if not set
 */
void* debug_get_telnet_output(void);

/**
 * @brief Clear Telnet output context (go back to Serial only)
 */
void debug_clear_telnet_output(void);

/**
 * @brief Register Telnet output callbacks (for internal use by telnet_server.cpp)
 * @param write_fn Callback for writing text without newline
 * @param writeline_fn Callback for writing text with newline
 */
void debug_register_telnet_callbacks(
    int (*write_fn)(void *server, const char *text),
    int (*writeline_fn)(void *server, const char *line));

#endif // debug_H
