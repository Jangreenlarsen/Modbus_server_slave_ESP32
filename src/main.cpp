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
#include "cli_remote.h"
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
#include "st_logic_config.h"
#include "st_logic_engine.h"
#include "network_manager.h"

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
  st_logic_init(st_logic_get_state());  // ST Logic Mode (4 independent programs)
  modbus_server_init(g_persist_config.slave_id);    // Modbus RTU server (UART1, from config)
  heartbeat_init();         // LED blink on GPIO2

  // Load ST Logic programs from persistent config
  st_logic_load_from_persist_config(&g_persist_config);

  // Apply loaded configuration (MUST be after subsystem init to override defaults)
  config_apply(&g_persist_config);

  Serial.println("\nSetup complete.");
  Serial.println("Modbus RTU Server ready on UART1 (GPIO4/5, 9600 baud)");
  Serial.println("RS485 DIR control on GPIO15");
  Serial.println("Registers: 256 holding (0-255), 256 input (0-255)");
  Serial.println("  ST Logic status: Input registers 200-251");
  Serial.println("  ST Logic control: Holding registers 200-235");
  Serial.println("Coils: 32 (256 bits), Discrete inputs: 32 (256 bits)\n");

  // Initialize network subsystem (v3.0+)
  if (network_manager_init() == 0) {
    Serial.println("Network manager initialized (Wi-Fi client mode)");

    // If Wi-Fi is enabled in config, start connection
    if (g_persist_config.network.enabled) {
      Serial.print("Connecting to Wi-Fi: ");
      Serial.println(g_persist_config.network.ssid);

      network_manager_connect(&g_persist_config.network);
    } else {
      Serial.println("Wi-Fi disabled in config");
    }
  } else {
    Serial.println("ERROR: Failed to initialize network manager");
  }

  // Initialize CLI remote (unified serial + Telnet)
  if (cli_remote_init() == 0) {
    Serial.println("CLI remote initialized (serial + Telnet support)");
  }

  cli_shell_init();         // CLI system (last, shows prompt)
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Network subsystem (v3.0+ Wi-Fi auto-reconnect, Telnet server)
  network_manager_loop();
  cli_remote_loop();

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

  // Update ST Logic status registers (200-251)
  registers_update_st_logic_status();

  // UNIFIED VARIABLE MAPPING: Read INPUT bindings (GPIO + ST variables)
  // This must happen BEFORE st_logic_engine_loop() to provide fresh inputs
  gpio_mapping_update();

  // ST Logic Mode execution (non-blocking, runs compiled programs)
  st_logic_engine_loop(st_logic_get_state(), registers_get_holding_regs(), registers_get_input_regs());

  // UNIFIED VARIABLE MAPPING: Write OUTPUT bindings (GPIO + ST variables)
  // This must happen AFTER st_logic_engine_loop() to push results to registers
  gpio_mapping_update();

  // Heartbeat/watchdog
  heartbeat_loop();

  // Small delay to prevent watchdog timeout
  delay(1);
}
