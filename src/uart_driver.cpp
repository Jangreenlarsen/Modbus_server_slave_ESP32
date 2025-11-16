/**
 * @file uart_driver.cpp
 * @brief UART hardware abstraction driver implementation (LAYER 0)
 *
 * Uses Arduino Serial/Serial1 for ESP32 UART access
 * - UART0 (Serial): USB/Debug on default pins
 * - UART1 (Serial1): Modbus RTU on GPIO4 (RX), GPIO5 (TX)
 */

#include "uart_driver.h"
#include "constants.h"
#include <Arduino.h>

/* ============================================================================
 * UART INITIALIZATION
 * ============================================================================ */

void uart_driver_init(void) {
  // UART0 (USB/Debug) - already initialized by Arduino framework
  uart0_init(SERIAL_BAUD_DEBUG);

  // UART1 (Modbus RTU) on GPIO4/5
  uart1_init(SERIAL_BAUD_MODBUS);
}

void uart0_init(uint32_t baudrate) {
  Serial.begin(baudrate);
  while (!Serial) {
    ; // Wait for serial port to connect (USB)
  }
}

void uart1_init(uint32_t baudrate) {
  // ESP32: Serial1 on GPIO9/10 by default, remap to GPIO4/5
  Serial1.begin(baudrate, SERIAL_8N1, PIN_UART1_RX, PIN_UART1_TX);
}

/* ============================================================================
 * UART0 (DEBUG) OPERATIONS
 * ============================================================================ */

uint16_t uart0_available(void) {
  return Serial.available();
}

int uart0_read(void) {
  return Serial.read();
}

void uart0_write(uint8_t byte) {
  Serial.write(byte);
}

void uart0_write_buffer(const uint8_t* data, uint16_t length) {
  if (data == NULL || length == 0) return;
  Serial.write(data, length);
}

/* ============================================================================
 * UART1 (MODBUS) OPERATIONS
 * ============================================================================ */

uint16_t uart1_available(void) {
  return Serial1.available();
}

int uart1_read(void) {
  return Serial1.read();
}

void uart1_write(uint8_t byte) {
  Serial1.write(byte);
}

void uart1_write_buffer(const uint8_t* data, uint16_t length) {
  if (data == NULL || length == 0) return;
  Serial1.write(data, length);
}

void uart1_flush_rx(void) {
  // Clear all pending RX data
  while (Serial1.available() > 0) {
    Serial1.read();
  }
}

void uart1_flush_tx(void) {
  // Wait for TX to complete
  Serial1.flush();
}

