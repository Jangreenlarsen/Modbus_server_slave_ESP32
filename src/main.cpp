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
  // TODO: gpio_driver_init()
  // TODO: uart_driver_init()
  // TODO: nvs_driver_init()

  // Initialize subsystems
  // TODO: config_load_from_nvs()
  // TODO: modbus_server_init()
  // TODO: counter_engine_init()
  // TODO: timer_engine_init()
  // TODO: cli_shell_init()
  // TODO: heartbeat_init()

  Serial.println("Setup complete.");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // TODO: Check if CLI is active
  // if (cli_is_active()) {
  //   cli_shell_loop();
  // } else {
  //   Normal operation: Modbus + features
  //   modbus_server_loop();
  //   counter_engine_loop();
  //   timer_engine_loop();
  //   heartbeat_loop();
  // }

  // Small delay to prevent watchdog timeout
  delay(1);
}
