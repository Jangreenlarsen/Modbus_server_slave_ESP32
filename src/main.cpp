/**
 * @file main.cpp
 * @brief Main entry point for ESP32 Modbus RTU Server
 *
 * Arduino setup() and loop() only. All subsystems are called from here.
 * No business logic in this file.
 */

#include <Arduino.h>
#include <nvs_flash.h>
#include "constants.h"
#include "types.h"
#include "version.h"
#include "cli_shell.h"
#include "counter_engine.h"
#include "timer_engine.h"
#include "gpio_driver.h"
#include "gpio_mapping.h"
#include "uart_driver.h"
#include "modbus_server.h"
#include "heartbeat.h"
#include "config_load.h"
#include "config_apply.h"
#include "config_struct.h"
#include "registers.h"

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // Initialize serial ports
  Serial.begin(SERIAL_BAUD_DEBUG);      // USB debug (UART0)
  delay(1000);  // Wait for serial monitor

  Serial.printf("=== Modbus RTU Server (ESP32) ===\n");
  Serial.printf("Version: %s Build #%d\n", PROJECT_VERSION, BUILD_NUMBER);
  Serial.printf("Built: %s\n", BUILD_TIMESTAMP);
  Serial.printf("Git: %s@%s\n", GIT_BRANCH, GIT_HASH);
  Serial.println("");

  // Initialize NVS flash (for configuration persistence)
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    Serial.println("NVS: Erasing flash...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  Serial.println("NVS: Initialized");

  // Load configuration from NVS
  config_load_from_nvs(&g_persist_config);

  // Initialize hardware drivers
  gpio_driver_init();       // GPIO system (RS485 DIR on GPIO15)
  uart_driver_init();       // UART0/UART1 initialization

  // Initialize subsystems (with default configs)
  counter_engine_init();    // Counter feature (SW/SW-ISR/HW modes)
  timer_engine_init();      // Timer feature (4 modes)
  modbus_server_init(g_persist_config.slave_id);    // Modbus RTU server (UART1, from config)
  heartbeat_init();         // LED blink on GPIO2

  // Apply loaded configuration (MUST be after subsystem init to override defaults)
  config_apply(&g_persist_config);

  Serial.println("\nSetup complete.");
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

  // Update DYNAMIC register/coil mappings (counter/timer → registers/coils)
  registers_update_dynamic_registers();
  registers_update_dynamic_coils();

  // GPIO STATIC mapping sync (GPIO ↔ registers/coils)
  gpio_mapping_update();

  // Heartbeat/watchdog
  heartbeat_loop();

  // Small delay to prevent watchdog timeout
  delay(1);
}
