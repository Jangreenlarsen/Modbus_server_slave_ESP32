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

#endif // debug_H
