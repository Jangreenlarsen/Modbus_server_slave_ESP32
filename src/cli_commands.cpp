/**
 * @file cli_commands.cpp
 * @brief CLI `set` command handlers (LAYER 7)
 *
 * Ported from: Mega2560 v3.6.5 cli_shell.cpp (command handlers)
 * Adapted to: ESP32 modular architecture
 *
 * Responsibility:
 * - Parse command parameters (key:value pairs, etc.)
 * - Call appropriate subsystem APIs
 * - Error handling and user feedback
 *
 * Context: Each handler is independent, can be tested separately
 */

#include "cli_commands.h"
#include "counter_engine.h"
#include "counter_config.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ============================================================================
 * COUNTER COMMANDS
 * ============================================================================ */

void cli_cmd_set_counter(uint8_t argc, char* argv[]) {
  // set counter <id> mode 1 parameter hw-mode:... edge:... prescaler:... ...
  if (argc < 3) {
    debug_println("SET COUNTER: missing parameters");
    return;
  }

  uint8_t id = atoi(argv[0]);
  if (id < 1 || id > 4) {
    debug_println("SET COUNTER: invalid counter ID (must be 1-4)");
    return;
  }

  // Skip past "mode 1" (argv[1] = "mode", argv[2] = "1")
  // argv[3] onwards = parameters

  if (argc < 4) {
    debug_println("SET COUNTER: missing 'parameter' keyword");
    return;
  }

  // Parse key:value parameters (TODO: implement full parser)
  CounterConfig cfg = counter_config_defaults(id);

  for (uint8_t i = 3; i < argc; i++) {
    char* arg = argv[i];
    char* colon = strchr(arg, ':');

    if (!colon) {
      debug_print("SET COUNTER: invalid parameter format: ");
      debug_println(arg);
      continue;
    }

    *colon = '\0';
    const char* key = arg;
    const char* value = colon + 1;

    // Parse known keys
    if (!strcmp(key, "hw-mode")) {
      if (!strcmp(value, "sw")) cfg.hw_mode = COUNTER_HW_SW;
      else if (!strcmp(value, "sw-isr")) cfg.hw_mode = COUNTER_HW_SW_ISR;
      else if (!strcmp(value, "hw")) cfg.hw_mode = COUNTER_HW_PCNT;
    } else if (!strcmp(key, "edge")) {
      if (!strcmp(value, "rising")) cfg.edge_type = COUNTER_EDGE_RISING;
      else if (!strcmp(value, "falling")) cfg.edge_type = COUNTER_EDGE_FALLING;
      else if (!strcmp(value, "both")) cfg.edge_type = COUNTER_EDGE_BOTH;
    } else if (!strcmp(key, "prescaler")) {
      cfg.prescaler = atoi(value);
    } else if (!strcmp(key, "index-reg") || !strcmp(key, "reg")) {
      cfg.index_reg = atoi(value);
    } else if (!strcmp(key, "raw-reg")) {
      cfg.raw_reg = atoi(value);
    } else if (!strcmp(key, "freq-reg")) {
      cfg.freq_reg = atoi(value);
    } else if (!strcmp(key, "ctrl-reg")) {
      cfg.ctrl_reg = atoi(value);
    } else if (!strcmp(key, "overload-reg")) {
      cfg.overload_reg = atoi(value);
    } else if (!strcmp(key, "start-value")) {
      cfg.start_value = atol(value);
    } else if (!strcmp(key, "scale")) {
      cfg.scale_factor = atof(value);
    } else if (!strcmp(key, "bit-width")) {
      cfg.bit_width = atoi(value);
    } else if (!strcmp(key, "direction")) {
      if (!strcmp(value, "down")) cfg.direction = COUNTER_DIR_DOWN;
      else cfg.direction = COUNTER_DIR_UP;
    } else if (!strcmp(key, "debounce")) {
      cfg.debounce_enabled = (!strcmp(value, "on")) ? 1 : 0;
    } else if (!strcmp(key, "debounce-ms")) {
      cfg.debounce_ms = atoi(value);
    } else if (!strcmp(key, "input-dis")) {
      cfg.input_dis = atoi(value);
    } else if (!strcmp(key, "interrupt-pin")) {
      cfg.interrupt_pin = atoi(value);
    }
  }

  cfg.enabled = 1;  // Implicit enable

  // Apply configuration
  if (counter_engine_configure(id, &cfg)) {
    debug_print("Counter ");
    debug_print_uint(id);
    debug_println(" configured");
  } else {
    debug_println("Failed to configure counter");
  }
}

void cli_cmd_reset_counter(uint8_t argc, char* argv[]) {
  if (argc < 1) {
    debug_println("RESET COUNTER: missing counter ID");
    return;
  }

  uint8_t id = atoi(argv[0]);
  if (id < 1 || id > 4) {
    debug_println("RESET COUNTER: invalid counter ID");
    return;
  }

  counter_engine_reset(id);
  debug_print("Counter ");
  debug_print_uint(id);
  debug_println(" reset");
}

void cli_cmd_clear_counters(void) {
  counter_engine_reset_all();
  debug_println("All counters cleared");
}

/* ============================================================================
 * TIMER COMMANDS
 * ============================================================================ */

void cli_cmd_set_timer(uint8_t argc, char* argv[]) {
  // TODO: Implement timer configuration
  debug_println("SET TIMER: not yet implemented");
}

/* ============================================================================
 * SYSTEM COMMANDS
 * ============================================================================ */

void cli_cmd_set_hostname(const char* hostname) {
  if (!hostname || strlen(hostname) == 0) {
    debug_println("SET HOSTNAME: empty hostname");
    return;
  }

  // TODO: Set hostname (would need global config storage)
  debug_print("Hostname set to: ");
  debug_println(hostname);
}

void cli_cmd_set_baud(uint32_t baud) {
  // TODO: Change UART baud rate
  debug_print("Baud rate set to: ");
  debug_print_uint(baud);
  debug_println(" (not yet implemented)");
}

void cli_cmd_set_id(uint8_t id) {
  if (id > 247) {
    debug_println("SET ID: invalid slave ID (must be 0-247)");
    return;
  }

  // TODO: Set Modbus slave ID
  debug_print("Slave ID set to: ");
  debug_print_uint(id);
  debug_println(" (not yet implemented)");
}

void cli_cmd_set_reg(uint16_t addr, uint16_t value) {
  // TODO: Write holding register
  debug_print("Set register ");
  debug_print_uint(addr);
  debug_print(" = ");
  debug_print_uint(value);
  debug_println(" (not yet implemented)");
}

void cli_cmd_set_coil(uint16_t idx, uint8_t value) {
  // TODO: Write coil
  debug_print("Set coil ");
  debug_print_uint(idx);
  debug_print(" = ");
  debug_print_uint(value);
  debug_println(" (not yet implemented)");
}

void cli_cmd_save(void) {
  // TODO: Save config to NVS
  debug_println("SAVE: config saved to NVS (not yet implemented)");
}

void cli_cmd_load(void) {
  // TODO: Load config from NVS
  debug_println("LOAD: config loaded from NVS (not yet implemented)");
}

void cli_cmd_defaults(void) {
  // TODO: Reset to factory defaults
  debug_println("DEFAULTS: factory defaults restored (not yet implemented)");
}

void cli_cmd_reboot(void) {
  debug_println("Rebooting...");
  // TODO: Trigger ESP32 reboot
  // esp_restart();
}
