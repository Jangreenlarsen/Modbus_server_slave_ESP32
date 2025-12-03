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
#include "timer_engine.h"
#include "timer_config.h"
#include "registers.h"
#include "cli_shell.h"
#include "config_struct.h"
#include "config_save.h"
#include "config_load.h"
#include "config_apply.h"
#include "gpio_driver.h"
#include "heartbeat.h"
#include "debug.h"
#include <Arduino.h>
#include <esp_system.h>
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
      debug_print("  DEBUG: interrupt_pin = ");
      debug_print_uint(cfg.interrupt_pin);
      debug_println("");
    } else if (!strcmp(key, "hw-gpio")) {
      cfg.hw_gpio = atoi(value);  // BUG FIX 1.9: Parse hw-gpio parameter
      debug_print("  DEBUG: hw_gpio = ");
      debug_print_uint(cfg.hw_gpio);
      debug_println("");
    }
  }

  cfg.enabled = 1;  // Implicit enable

  // DEBUG: Print final config before applying
  debug_print("Counter ");
  debug_print_uint(id);
  debug_print(" config: enabled=");
  debug_print_uint(cfg.enabled);
  debug_print(" hw_mode=");
  debug_print_uint(cfg.hw_mode);
  debug_print(" hw_gpio=");
  debug_print_uint(cfg.hw_gpio);
  debug_println("");

  // Validate register usage - check for overlaps with other counters
  for (uint8_t other_id = 1; other_id <= COUNTER_COUNT; other_id++) {
    if (other_id == id) continue;  // Skip self

    CounterConfig other_cfg;
    if (!counter_config_get(other_id, &other_cfg) || !other_cfg.enabled) continue;

    // Check if any registers overlap
    bool overlap = false;
    uint16_t overlap_reg = 0;
    const char* overlap_type = "";

    if (cfg.index_reg > 0 && cfg.index_reg == other_cfg.index_reg) {
      overlap = true; overlap_reg = cfg.index_reg; overlap_type = "index-reg";
    }
    else if (cfg.raw_reg > 0 && cfg.raw_reg == other_cfg.raw_reg) {
      overlap = true; overlap_reg = cfg.raw_reg; overlap_type = "raw-reg";
    }
    else if (cfg.freq_reg > 0 && cfg.freq_reg == other_cfg.freq_reg) {
      overlap = true; overlap_reg = cfg.freq_reg; overlap_type = "freq-reg";
    }
    else if (cfg.ctrl_reg > 0 && cfg.ctrl_reg < 1000 && cfg.ctrl_reg == other_cfg.ctrl_reg) {
      overlap = true; overlap_reg = cfg.ctrl_reg; overlap_type = "ctrl-reg";
    }
    else if (cfg.overload_reg > 0 && cfg.overload_reg < 1000 && cfg.overload_reg == other_cfg.overload_reg) {
      overlap = true; overlap_reg = cfg.overload_reg; overlap_type = "overload-reg";
    }

    if (overlap) {
      debug_print("WARNING: Counter ");
      debug_print_uint(id);
      debug_print(" ");
      debug_print(overlap_type);
      debug_print("=");
      debug_print_uint(overlap_reg);
      debug_print(" overlaps with Counter ");
      debug_print_uint(other_id);
      debug_println("!");
      debug_println("  This will cause register conflicts!");
      debug_println("  Please use different register addresses for each counter.");
    }
  }

  // Store in persistent config
  if (id >= 1 && id <= COUNTER_COUNT) {
    g_persist_config.counters[id - 1] = cfg;
  }

  // Apply configuration to engine
  if (counter_engine_configure(id, &cfg)) {
    debug_print("Counter ");
    debug_print_uint(id);
    debug_println(" configured");

    // BUG FIX: Auto-save config to NVS after successful configure
    if (config_save_to_nvs(&g_persist_config)) {
      debug_println("  (Config auto-saved to NVS)");
    } else {
      debug_println("  WARNING: Failed to save config to NVS!");
    }

    // Warn if input-dis or interrupt-pin not set for SW modes
    if (cfg.hw_mode == COUNTER_HW_SW && cfg.input_dis == 0) {
      debug_println("  HINT: SW mode requires input-dis:<pin>");
      debug_println("        Example: input-dis:45");
    }
    if (cfg.hw_mode == COUNTER_HW_SW_ISR && cfg.interrupt_pin == 0) {
      debug_println("  HINT: SW-ISR mode requires interrupt-pin:<gpio>");
      debug_println("        Example: interrupt-pin:23");
    }

    debug_println("NOTE: Use 'save' to persist to NVS");
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

void cli_cmd_set_counter_control(uint8_t argc, char* argv[]) {
  // set counter <id> control reset-on-read:on|off auto-start:on|off running:on|off
  if (argc < 2) {
    debug_println("SET COUNTER CONTROL: missing parameters");
    debug_println("  Usage: set counter <id> control reset-on-read:<on|off>");
    debug_println("         set counter <id> control auto-start:<on|off>");
    debug_println("         set counter <id> control running:<on|off>");
    debug_println("  Example: set counter 1 control auto-start:on running:on");
    return;
  }

  uint8_t id = atoi(argv[0]);
  if (id < 1 || id > 4) {
    debug_println("SET COUNTER CONTROL: invalid counter ID (must be 1-4)");
    return;
  }

  // Get counter config to find ctrl-reg address
  CounterConfig cfg;
  if (!counter_config_get(id, &cfg)) {
    debug_println("SET COUNTER CONTROL: counter not found");
    return;
  }

  if (cfg.ctrl_reg >= HOLDING_REGS_SIZE) {
    debug_println("SET COUNTER CONTROL: ctrl-reg not configured");
    debug_println("  Hint: First configure counter with ctrl-reg:<address>");
    return;
  }

  // Read current ctrl-reg value
  uint16_t ctrl_value = registers_get_holding_register(cfg.ctrl_reg);

  // Parse control parameters (argv[1] onwards, skip "control" keyword)
  for (uint8_t i = 1; i < argc; i++) {
    char* arg = argv[i];
    char* colon = strchr(arg, ':');

    if (!colon) {
      debug_print("SET COUNTER CONTROL: invalid parameter format: ");
      debug_println(arg);
      continue;
    }

    *colon = '\0';
    const char* key = arg;
    const char* value = colon + 1;

    // Parse control flags
    if (!strcmp(key, "reset-on-read")) {
      if (!strcmp(value, "on") || !strcmp(value, "ON")) {
        ctrl_value |= 0x01;  // Set bit 0
      } else if (!strcmp(value, "off") || !strcmp(value, "OFF")) {
        ctrl_value &= ~0x01; // Clear bit 0
      } else {
        debug_print("SET COUNTER CONTROL: invalid value for reset-on-read: ");
        debug_println(value);
      }
    } else if (!strcmp(key, "auto-start")) {
      if (!strcmp(value, "on") || !strcmp(value, "ON")) {
        ctrl_value |= 0x02;  // Set bit 1
      } else if (!strcmp(value, "off") || !strcmp(value, "OFF")) {
        ctrl_value &= ~0x02; // Clear bit 1
      } else {
        debug_print("SET COUNTER CONTROL: invalid value for auto-start: ");
        debug_println(value);
      }
    } else if (!strcmp(key, "running")) {
      if (!strcmp(value, "on") || !strcmp(value, "ON")) {
        ctrl_value |= 0x80;  // Set bit 7
      } else if (!strcmp(value, "off") || !strcmp(value, "OFF")) {
        ctrl_value &= ~0x80; // Clear bit 7
      } else {
        debug_print("SET COUNTER CONTROL: invalid value for running: ");
        debug_println(value);
      }
    } else {
      debug_print("SET COUNTER CONTROL: unknown control flag: ");
      debug_println(key);
    }
  }

  // Write updated ctrl-reg value
  registers_set_holding_register(cfg.ctrl_reg, ctrl_value);

  debug_print("Counter ");
  debug_print_uint(id);
  debug_print(" control updated: ctrl-reg[");
  debug_print_uint(cfg.ctrl_reg);
  debug_print("] = ");
  debug_print_uint(ctrl_value);
  debug_println("");

  // Show status
  bool reset_on_read = (ctrl_value & 0x01) != 0;
  bool auto_start = (ctrl_value & 0x02) != 0;
  bool running = (ctrl_value & 0x80) != 0;

  debug_print("  reset-on-read: ");
  debug_println(reset_on_read ? "ENABLED" : "DISABLED");
  debug_print("  auto-start: ");
  debug_println(auto_start ? "ENABLED" : "DISABLED");
  debug_print("  running: ");
  debug_println(running ? "YES" : "NO");
}

/* ============================================================================
 * TIMER COMMANDS
 * ============================================================================ */

void cli_cmd_set_timer(uint8_t argc, char* argv[]) {
  // set timer <id> mode <1|2|3|4> parameter key:value ...
  if (argc < 3) {
    debug_println("SET TIMER: missing parameters");
    return;
  }

  uint8_t id = atoi(argv[0]);
  if (id < 1 || id > TIMER_COUNT) {
    debug_println("SET TIMER: invalid timer ID (must be 1-4)");
    return;
  }

  // argv[1] = "mode", argv[2] = mode number
  if (strcmp(argv[1], "mode") != 0) {
    debug_println("SET TIMER: expected 'mode' keyword");
    return;
  }

  uint8_t mode = atoi(argv[2]);
  if (mode < 1 || mode > 4) {
    debug_println("SET TIMER: invalid mode (must be 1-4)");
    return;
  }

  if (argc < 4) {
    debug_println("SET TIMER: missing 'parameter' keyword or parameters");
    return;
  }

  // Create default config and update with provided parameters
  TimerConfig cfg = {0};
  cfg.enabled = 1;
  cfg.mode = (TimerMode)mode;
  cfg.output_coil = 65535;  // No output by default

  // Set mode-specific defaults
  if (mode == TIMER_MODE_3_ASTABLE) {
    // Astable mode: toggle between HIGH (1) and LOW (0)
    cfg.phase1_output_state = 1;  // Phase 1 = HIGH
    cfg.phase2_output_state = 0;  // Phase 2 = LOW
  } else if (mode == TIMER_MODE_4_INPUT_TRIGGERED) {
    // Input-triggered mode: output HIGH when triggered
    cfg.phase1_output_state = 1;  // Output = HIGH when triggered
    cfg.phase2_output_state = 0;  // Unused, set to LOW
  }

  // Parse key:value parameters
  for (uint8_t i = 3; i < argc; i++) {
    char* arg = argv[i];
    char* colon = strchr(arg, ':');

    if (!colon) {
      debug_print("SET TIMER: invalid parameter format: ");
      debug_println(arg);
      continue;
    }

    *colon = '\0';
    const char* key = arg;
    const char* value = colon + 1;

    // Parse mode 1 parameters (one-shot)
    if (!strcmp(key, "p1-duration")) {
      cfg.phase1_duration_ms = atol(value);
    } else if (!strcmp(key, "p1-output")) {
      cfg.phase1_output_state = atoi(value);
    } else if (!strcmp(key, "p2-duration")) {
      cfg.phase2_duration_ms = atol(value);
    } else if (!strcmp(key, "p2-output")) {
      cfg.phase2_output_state = atoi(value);
    } else if (!strcmp(key, "p3-duration")) {
      cfg.phase3_duration_ms = atol(value);
    } else if (!strcmp(key, "p3-output")) {
      cfg.phase3_output_state = atoi(value);
    }
    // Mode 2 parameters (monostable)
    else if (!strcmp(key, "pulse-ms")) {
      cfg.pulse_duration_ms = atol(value);
    } else if (!strcmp(key, "trigger-level")) {
      cfg.trigger_level = atoi(value);
    }
    // Mode 3 parameters (astable)
    else if (!strcmp(key, "on-ms") || !strcmp(key, "on")) {
      cfg.on_duration_ms = atol(value);
    } else if (!strcmp(key, "off-ms") || !strcmp(key, "off")) {
      cfg.off_duration_ms = atol(value);
    }
    // Mode 4 parameters (input-triggered)
    else if (!strcmp(key, "input-dis")) {
      cfg.input_dis = atoi(value);
    } else if (!strcmp(key, "delay-ms")) {
      cfg.delay_ms = atol(value);
    } else if (!strcmp(key, "trigger-edge")) {
      cfg.trigger_edge = atoi(value);
    }
    // Common parameters
    else if (!strcmp(key, "output-coil")) {
      cfg.output_coil = atoi(value);
    } else if (!strcmp(key, "ctrl-reg")) {
      cfg.ctrl_reg = atoi(value);
    } else if (!strcmp(key, "enabled")) {
      cfg.enabled = (!strcmp(value, "on") || !strcmp(value, "1")) ? 1 : 0;
    }
  }

  // Store in persistent config
  if (id >= 1 && id <= TIMER_COUNT) {
    g_persist_config.timers[id - 1] = cfg;

    // Activate timer in timer_engine immediately
    timer_engine_configure(id, &cfg);
  }

  debug_print("Timer ");
  debug_print_uint(id);
  debug_print(" configured (mode ");
  debug_print_uint(mode);
  debug_println(")");
  debug_println("NOTE: Use 'save' to persist to NVS");
}

/* ============================================================================
 * SYSTEM COMMANDS
 * ============================================================================ */

void cli_cmd_set_hostname(const char* hostname) {
  if (!hostname || strlen(hostname) == 0) {
    debug_println("SET HOSTNAME: empty hostname");
    return;
  }

  // Note: Hostname storage would be in config_apply.cpp
  debug_print("Hostname set to: ");
  debug_println(hostname);
}

void cli_cmd_set_baud(uint32_t baud) {
  // Validate baudrate
  if (baud < 300 || baud > 115200) {
    debug_println("SET BAUD: invalid baud rate (must be 300-115200)");
    return;
  }

  // Update configuration
  g_persist_config.baudrate = baud;

  debug_print("Baud rate set to: ");
  debug_print_uint(baud);
  debug_println(" (will apply on next boot)");
  debug_println("NOTE: Use 'save' to persist to NVS");
}

void cli_cmd_set_id(uint8_t id) {
  if (id > 247) {
    debug_println("SET ID: invalid slave ID (must be 0-247)");
    return;
  }

  // Update configuration
  g_persist_config.slave_id = id;

  debug_print("Slave ID set to: ");
  debug_print_uint(id);
  debug_println(" (will apply on next boot)");
  debug_println("NOTE: Use 'save' to persist to NVS");
}

void cli_cmd_set_reg(uint16_t addr, uint16_t value) {
  if (addr >= HOLDING_REGS_SIZE) {
    debug_print("SET REG: address out of range (max ");
    debug_print_uint(HOLDING_REGS_SIZE - 1);
    debug_println(")");
    return;
  }

  // Write to holding register
  registers_set_holding_register(addr, value);
  debug_print("Register ");
  debug_print_uint(addr);
  debug_print(" = ");
  debug_print_uint(value);
  debug_println("");
}

void cli_cmd_set_coil(uint16_t idx, uint8_t value) {
  if (idx >= (COILS_SIZE * 8)) {
    debug_print("SET COIL: index out of range (max ");
    debug_print_uint((COILS_SIZE * 8) - 1);
    debug_println(")");
    return;
  }

  // Write to coil
  registers_set_coil(idx, value ? 1 : 0);
  debug_print("Coil ");
  debug_print_uint(idx);
  debug_print(" = ");
  debug_print_uint(value ? 1 : 0);
  debug_println("");
}

/**
 * no set gpio <pin>   (Remove GPIO mapping)
 */
void cli_cmd_no_set_gpio(uint8_t argc, char* argv[]) {
  if (argc < 1) {
    debug_println("NO SET GPIO: missing pin");
    debug_println("  Usage: no set gpio <pin>");
    debug_println("  Example: no set gpio 23");
    return;
  }

  uint16_t gpio_pin = atoi(argv[0]);

  // Validate pin: ESP32 has pins 0-39, or virtual GPIO 100+ (reads from COIL)
  if ((gpio_pin >= 40 && gpio_pin < 100) || gpio_pin >= 256) {
    debug_print("NO SET GPIO: invalid pin ");
    debug_print_uint(gpio_pin);
    debug_println(" (0-39 or 100-255 for virtual GPIO)");
    return;
  }

  // Find GPIO mapping
  uint8_t found_idx = 0xff;
  for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
    if (g_persist_config.var_maps[i].gpio_pin == gpio_pin) {
      found_idx = i;
      break;
    }
  }

  if (found_idx == 0xff) {
    debug_print("NO SET GPIO: pin ");
    debug_print_uint(gpio_pin);
    debug_println(" is not mapped");
    return;
  }

  // Remove mapping by shifting all following entries one position down
  for (uint8_t i = found_idx; i < g_persist_config.var_map_count - 1; i++) {
    g_persist_config.var_maps[i] = g_persist_config.var_maps[i + 1];
  }
  g_persist_config.var_map_count--;

  debug_print("GPIO ");
  debug_print_uint(gpio_pin);
  debug_println(" mapping removed");
  debug_println("NOTE: Use 'save' to persist to NVS");
}

/**
 * set gpio <pin> static map input:<idx>   (INPUT mode: GPIO pin → discrete input)
 * set gpio <pin> static map coil:<idx>    (OUTPUT mode: coil → GPIO pin)
 */
void cli_cmd_set_gpio(uint8_t argc, char* argv[]) {
  if (argc < 4) {
    debug_println("SET GPIO: missing arguments");
    debug_println("  Usage: set gpio <pin> static map input:<idx>");
    debug_println("         set gpio <pin> static map coil:<idx>");
    debug_println("  Examples:");
    debug_println("    set gpio 23 static map input:45   (GPIO23 input → discrete input 45)");
    debug_println("    set gpio 12 static map coil:112   (Coil 112 → GPIO12 output)");
    return;
  }

  uint16_t gpio_pin = atoi(argv[0]);

  // Validate pin: ESP32 has pins 0-39, or virtual GPIO 100+ (reads from COIL)
  if ((gpio_pin >= 40 && gpio_pin < 100) || gpio_pin >= 256) {
    debug_print("SET GPIO: invalid pin ");
    debug_print_uint(gpio_pin);
    debug_println(" (0-39 or 100-255 for virtual GPIO)");
    return;
  }

  // Expect: set gpio <pin> static map input:<idx> OR set gpio <pin> static map coil:<idx>
  if (strcmp(argv[1], "static") != 0) {
    debug_println("SET GPIO: expected 'static'");
    return;
  }

  if (strcmp(argv[2], "map") != 0) {
    debug_println("SET GPIO: expected 'map'");
    return;
  }

  // Parse mapping: input:<idx> or coil:<idx>
  char* arg = argv[3];
  char* colon = strchr(arg, ':');
  if (!colon) {
    debug_println("SET GPIO: invalid format (expected input:<idx> or coil:<idx>)");
    return;
  }

  *colon = '\0';
  const char* key = arg;
  const char* value = colon + 1;
  uint16_t index = atoi(value);

  uint16_t input_index = 65535;
  uint16_t coil_index = 65535;
  uint8_t is_input = 0;

  if (!strcmp(key, "input")) {
    // INPUT mode: GPIO pin → discrete input
    if (index >= (DISCRETE_INPUTS_SIZE * 8)) {
      debug_print("SET GPIO: input index out of range (max ");
      debug_print_uint((DISCRETE_INPUTS_SIZE * 8) - 1);
      debug_println(")");
      return;
    }
    input_index = index;
    is_input = 1;
  } else if (!strcmp(key, "coil")) {
    // OUTPUT mode: coil → GPIO pin
    if (index >= (COILS_SIZE * 8)) {
      debug_print("SET GPIO: coil index out of range (max ");
      debug_print_uint((COILS_SIZE * 8) - 1);
      debug_println(")");
      return;
    }
    coil_index = index;
    is_input = 0;
  } else {
    debug_print("SET GPIO: unknown key '");
    debug_print(key);
    debug_println("' (expected 'input' or 'coil')");
    return;
  }

  // Find existing GPIO mapping or create new
  uint8_t found_idx = 0xff;
  for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
    if (g_persist_config.var_maps[i].gpio_pin == gpio_pin) {
      found_idx = i;
      break;
    }
  }

  if (found_idx == 0xff) {
    // Create new mapping
    if (g_persist_config.var_map_count >= 32) {
      debug_println("SET GPIO: max GPIO mappings reached (32)");
      return;
    }
    found_idx = g_persist_config.var_map_count;
    g_persist_config.var_map_count++;
  }

  // Store STATIC mapping
  g_persist_config.var_maps[found_idx].gpio_pin = gpio_pin;
  g_persist_config.var_maps[found_idx].is_input = is_input;
  g_persist_config.var_maps[found_idx].associated_counter = 0xff;  // No counter in STATIC mode
  g_persist_config.var_maps[found_idx].associated_timer = 0xff;    // No timer in STATIC mode
  g_persist_config.var_maps[found_idx].input_reg = input_index;
  g_persist_config.var_maps[found_idx].coil_reg = coil_index;

  // Initialize GPIO pin direction
  if (is_input) {
    gpio_set_direction(gpio_pin, GPIO_INPUT);
  } else {
    gpio_set_direction(gpio_pin, GPIO_OUTPUT);
  }

  // Print confirmation
  debug_print("GPIO ");
  debug_print_uint(gpio_pin);
  if (is_input) {
    debug_print(" - INPUT:");
    debug_print_uint(input_index);
  } else {
    debug_print(" - COIL:");
    debug_print_uint(coil_index);
  }
  debug_println("");
  debug_println("NOTE: Use 'save' to persist to NVS");
}

void cli_cmd_save(void) {
  debug_println("Saving configuration to NVS...");

  // Calculate CRC before saving
  g_persist_config.crc16 = config_calculate_crc16(&g_persist_config);

  // Save to NVS
  bool success = config_save_to_nvs(&g_persist_config);

  if (success) {
    debug_println("SAVE: Configuration saved successfully");

    // Count enabled counters and timers
    uint8_t enabled_counters = 0, enabled_timers = 0;
    for (uint8_t i = 0; i < COUNTER_COUNT; i++) {
      if (g_persist_config.counters[i].enabled) enabled_counters++;
    }
    for (uint8_t i = 0; i < TIMER_COUNT; i++) {
      if (g_persist_config.timers[i].enabled) enabled_timers++;
    }

    debug_print("  - ");
    debug_print_uint(enabled_counters);
    debug_println(" counters");
    debug_print("  - ");
    debug_print_uint(enabled_timers);
    debug_println(" timers");
    debug_print("  - ");
    debug_print_uint(g_persist_config.static_reg_count);
    debug_println(" static registers");
    debug_print("  - ");
    debug_print_uint(g_persist_config.dynamic_reg_count);
    debug_println(" dynamic registers");
    debug_print("  - ");
    debug_print_uint(g_persist_config.static_coil_count);
    debug_println(" static coils");
    debug_print("  - ");
    debug_print_uint(g_persist_config.dynamic_coil_count);
    debug_println(" dynamic coils");
    debug_print("  - ");
    debug_print_uint(g_persist_config.var_map_count);
    debug_println(" GPIO mappings");
  } else {
    debug_println("SAVE: Failed to save configuration");
  }
}

void cli_cmd_load(void) {
  debug_println("Loading configuration from NVS...");

  bool success = config_load_from_nvs(&g_persist_config);

  if (success) {
    debug_println("LOAD: Configuration loaded successfully");
    debug_print("  - ");
    debug_print_uint(g_persist_config.static_reg_count);
    debug_println(" static registers");
    debug_print("  - ");
    debug_print_uint(g_persist_config.dynamic_reg_count);
    debug_println(" dynamic registers");
    debug_print("  - ");
    debug_print_uint(g_persist_config.static_coil_count);
    debug_println(" static coils");
    debug_print("  - ");
    debug_print_uint(g_persist_config.dynamic_coil_count);
    debug_println(" dynamic coils");
    debug_print("  - ");
    debug_print_uint(g_persist_config.var_map_count);
    debug_println(" GPIO mappings");

    // Apply configuration to running system
    config_apply(&g_persist_config);
    debug_println("NOTE: Configuration applied. Some changes may require reboot.");
  } else {
    debug_println("LOAD: Failed to load configuration (using defaults)");
  }
}

void cli_cmd_defaults(void) {
  debug_println("Resetting to factory defaults...");

  // Reset config to defaults (zero out everything)
  memset(&g_persist_config, 0, sizeof(PersistConfig));
  g_persist_config.schema_version = 1;
  g_persist_config.slave_id = 1;
  g_persist_config.baudrate = 9600;

  // Initialize all GPIO mappings as unused
  for (uint8_t i = 0; i < 8; i++) {
    g_persist_config.var_maps[i].input_reg = 65535;
    g_persist_config.var_maps[i].coil_reg = 65535;
    g_persist_config.var_maps[i].associated_counter = 0xff;
    g_persist_config.var_maps[i].associated_timer = 0xff;
  }

  debug_println("DEFAULTS: Configuration reset to factory defaults");
  debug_println("NOTE: Use 'save' to persist, or 'reboot' to discard");
}

void cli_cmd_reboot(void) {
  debug_println("Rebooting in 2 seconds...");
  delay(2000);
  esp_restart();
}

void cli_cmd_set_echo(uint8_t argc, char* argv[]) {
  // set echo <on|off>
  if (argc < 1) {
    debug_println("SET ECHO: missing parameter (on|off)");
    return;
  }

  const char* state = argv[0];
  if (!strcmp(state, "on")) {
    cli_shell_set_remote_echo(1);
    debug_println("Remote echo: ON");
  } else if (!strcmp(state, "off")) {
    cli_shell_set_remote_echo(0);
    debug_println("Remote echo: OFF");
  } else {
    debug_print("SET ECHO: invalid state '");
    debug_print(state);
    debug_println("' (use: on|off)");
  }
}

/* ============================================================================
 * WRITE REG
 * ============================================================================ */

void cli_cmd_write_reg(uint8_t argc, char* argv[]) {
  // write reg <id> value <0..65535>
  if (argc < 3) {
    debug_println("WRITE REG: manglende parametre");
    debug_println("  Brug: write reg <id> value <v\u00e6rdi>");
    debug_println("  Eksempel: write reg 90 value 12345");
    return;
  }

  uint16_t addr = atoi(argv[0]);

  // Check if second parameter is "value" (optional keyword)
  uint8_t value_idx = 1;
  if (argc >= 3 && !strcmp(argv[1], "value")) {
    value_idx = 2;
  }

  uint16_t value = atoi(argv[value_idx]);

  // Validate address
  if (addr >= HOLDING_REGS_SIZE) {
    debug_print("WRITE REG: adresse udenfor omr\u00e5de (max ");
    debug_print_uint(HOLDING_REGS_SIZE - 1);
    debug_println(")");
    return;
  }

  // Write to holding register
  registers_set_holding_register(addr, value);

  debug_print("Register ");
  debug_print_uint(addr);
  debug_print(" = ");
  debug_print_uint(value);
  debug_println(" (skrevet)");
}

/* ============================================================================
 * WRITE COIL
 * ============================================================================ */

void cli_cmd_write_coil(uint8_t argc, char* argv[]) {
  // write coil <id> value <on|off>
  if (argc < 3) {
    debug_println("WRITE COIL: manglende parametre");
    debug_println("  Brug: write coil <id> value <on|off>");
    debug_println("  Eksempel: write coil 5 value on");
    return;
  }

  uint16_t addr = atoi(argv[0]);

  // Check if second parameter is "value" (optional keyword)
  uint8_t value_idx = 1;
  if (argc >= 3 && !strcmp(argv[1], "value")) {
    value_idx = 2;
  }

  const char* value_str = argv[value_idx];
  uint8_t value = 0;

  // Parse on/off or 1/0
  if (!strcmp(value_str, "on") || !strcmp(value_str, "ON") || !strcmp(value_str, "1")) {
    value = 1;
  } else if (!strcmp(value_str, "off") || !strcmp(value_str, "OFF") || !strcmp(value_str, "0")) {
    value = 0;
  } else {
    debug_print("WRITE COIL: ugyldig v\u00e6rdi '");
    debug_print(value_str);
    debug_println("' (brug: on|off eller 1|0)");
    return;
  }

  // Validate address
  if (addr >= (COILS_SIZE * 8)) {
    debug_print("WRITE COIL: adresse udenfor omr\u00e5de (max ");
    debug_print_uint((COILS_SIZE * 8) - 1);
    debug_println(")");
    return;
  }

  // Write to coil
  registers_set_coil(addr, value);

  debug_print("Coil ");
  debug_print_uint(addr);
  debug_print(" = ");
  debug_print(value ? "1 (ON)" : "0 (OFF)");
  debug_println(" (skrevet)");
}

/* ============================================================================
 * GPIO2 ENABLE/DISABLE (HEARTBEAT CONTROL)
 * ============================================================================ */

void cli_cmd_set_gpio2(uint8_t argc, char* argv[]) {
  // set gpio 2 enable/disable
  if (argc < 2) {
    debug_println("SET GPIO 2: manglende parametre");
    debug_println("  Brug: set gpio 2 enable   (frigiv GPIO2 til bruger)");
    debug_println("        set gpio 2 disable  (aktiver heartbeat igen)");
    return;
  }

  // argv[0] = "2", argv[1] = "enable" or "disable"
  const char* action = argv[1];

  if (!strcmp(action, "enable") || !strcmp(action, "ENABLE")) {
    // Enable user mode (disable heartbeat)
    g_persist_config.gpio2_user_mode = 1;
    heartbeat_disable();

    debug_println("GPIO2 frigivet til bruger (heartbeat deaktiveret)");
    debug_println("OBS: GPIO2 kan nu bruges til andre formål");
    debug_println("TIP: Brug 'set gpio 2 static map input:<idx>' eller 'coil:<idx>'");
    debug_println("NOTE: Husk 'save' for at gemme indstillingen");

  } else if (!strcmp(action, "disable") || !strcmp(action, "DISABLE")) {
    // Disable user mode (enable heartbeat)
    g_persist_config.gpio2_user_mode = 0;
    heartbeat_enable();

    debug_println("GPIO2 reserveret til heartbeat (LED blink aktiv)");
    debug_println("NOTE: Husk 'save' for at gemme indstillingen");

  } else {
    debug_print("SET GPIO 2: ugyldig handling '");
    debug_print(action);
    debug_println("' (brug: enable|disable)");
  }
}
