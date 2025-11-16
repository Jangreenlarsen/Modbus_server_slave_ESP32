/**
 * @file debug.cpp
 * @brief Debug output implementation via Serial
 *
 * Uses Arduino Serial library (Serial object initialized in main.cpp)
 */

#include "debug.h"
#include <Arduino.h>
#include <stdio.h>

void debug_println(const char* str) {
  if (str) {
    Serial.println(str);
  }
}

void debug_print(const char* str) {
  if (str) {
    Serial.print(str);
  }
}

void debug_print_uint(uint32_t value) {
  Serial.print(value);
}

void debug_print_ulong(uint64_t value) {
  Serial.print((unsigned long)value);  // Truncate to unsigned long for compatibility
}

void debug_print_float(double value) {
  // Print with 2 decimal places
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.2f", value);
  Serial.print(buffer);
}

void debug_newline(void) {
  Serial.println();
}
