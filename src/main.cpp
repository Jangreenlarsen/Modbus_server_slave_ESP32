/**
 * @file main.cpp
 * @brief Main entry point for ESP32 Modbus RTU Server
 *
 * Arduino setup() and loop() only. All subsystems are called from here.
 * No business logic in this file.
 */

#include <Arduino.h>
#include "constants.h"
#include "types.h"
#include "cli_shell.h"
#include "counter_engine.h"
#include "timer_engine.h"
#include "gpio_driver.h"
#include "uart_driver.h"
#include "modbus_server.h"
#include "heartbeat.h"

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // Initialize serial ports
  Serial.begin(SERIAL_BAUD_DEBUG);      // USB debug (UART0)
  delay(1000);  // Wait for serial monitor

  Serial.printf("=== Modbus RTU Server (ESP32) ===\n");
  Serial.printf("Version: %s\n", PROJECT_VERSION);

  // Initialize hardware drivers
  gpio_driver_init();       // GPIO system (RS485 DIR on GPIO15)
  uart_driver_init();       // UART0/UART1 initialization

  // Initialize subsystems
  counter_engine_init();    // Counter feature (SW/SW-ISR/HW modes)
  timer_engine_init();      // Timer feature (4 modes)
  modbus_server_init(1);    // Modbus RTU server (UART1, slave ID=1)
  heartbeat_init();         // LED blink on GPIO2

  Serial.println("Setup complete.");
  Serial.println("Modbus RTU Server ready on UART1 (GPIO4/5, 9600 baud)");
  Serial.println("RS485 DIR control on GPIO15");
  Serial.println("Registers: 160 holding, 160 input");
  Serial.println("Coils: 32 (256 bits), Discrete inputs: 32 (256 bits)\n");

  cli_shell_init();         // CLI system (last, shows prompt)
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Modbus server (primary function - handles FC01-10)
  modbus_server_loop();

  // CLI interface (responsive while Modbus runs)
  cli_shell_loop();

  // Background feature engines
  counter_engine_loop();
  timer_engine_loop();

  // Heartbeat/watchdog
  heartbeat_loop();

  // Small delay to prevent watchdog timeout
  delay(1);
}
