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
#include "registers.h"
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
    else if (!strcmp(key, "on-ms")) {
      cfg.on_duration_ms = atol(value);
    } else if (!strcmp(key, "off-ms")) {
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
    } else if (!strcmp(key, "enabled")) {
      cfg.enabled = (!strcmp(value, "on") || !strcmp(value, "1")) ? 1 : 0;
    }
  }

  // TODO: Store config and activate timer
  debug_print("Timer ");
  debug_print_uint(id);
  debug_print(" configured (mode ");
  debug_print_uint(mode);
  debug_println(")");
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

  // Note: UART baud changes would be in uart_driver.cpp
  debug_print("Baud rate set to: ");
  debug_print_uint(baud);
  debug_println(" (would apply on next boot)");
}

void cli_cmd_set_id(uint8_t id) {
  if (id > 247) {
    debug_println("SET ID: invalid slave ID (must be 0-247)");
    return;
  }

  // Note: Slave ID storage would be in config_apply.cpp
  debug_print("Slave ID set to: ");
  debug_print_uint(id);
  debug_println(" (would apply on next boot)");
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

void cli_cmd_set_gpio(uint8_t argc, char* argv[]) {
  // set gpio <pin> STATIC [coil:<idx>] [reg:<id>] [input:<id>]
  // set gpio <pin> DYNAMIC [coil:<idx>] [reg:<id>] [coil:<id>] [counter:<id> input-dis=<id> ...]
  // set gpio <pin> DYNAMIC [coil:<idx>] [reg:<id>] [coil:<id>] [timer:<id> ...]

  if (argc < 2) {
    debug_println("SET GPIO: missing arguments");
    debug_println("  Usage: set gpio <pin> STATIC [coil:<idx>] [reg:<id>] [input:<id>]");
    debug_println("         set gpio <pin> DYNAMIC [coil:<idx>] [reg:<id>] [coil:<id>] [counter:<id> ...] or [timer:<id> ...]");
    return;
  }

  uint8_t gpio_pin = atoi(argv[0]);

  // Validate pin (ESP32 has pins 0-39, but some are reserved)
  if (gpio_pin >= 40) {
    debug_print("SET GPIO: invalid pin ");
    debug_print_uint(gpio_pin);
    debug_println(" (must be 0-39)");
    return;
  }

  const char* mode = argv[1];

  // Validate mode
  if (!strcmp(mode, "STATIC")) {
    // STATIC mode: set gpio <pin> STATIC [coil:<idx>] [reg:<id>] [input:<id>]
    uint16_t coil_index = 65535;
    uint16_t reg_index = 65535;
    uint16_t input_index = 65535;

    // Parse parameters
    for (uint8_t i = 2; i < argc; i++) {
      char* arg = argv[i];
      char* colon = strchr(arg, ':');
      if (!colon) {
        debug_print("SET GPIO STATIC: invalid parameter format: ");
        debug_println(arg);
        return;
      }

      *colon = '\0';
      const char* key = arg;
      const char* value = colon + 1;

      if (!strcmp(key, "coil")) {
        coil_index = atoi(value);
        if (coil_index >= (COILS_SIZE * 8)) {
          debug_print("SET GPIO: coil index out of range (max ");
          debug_print_uint((COILS_SIZE * 8) - 1);
          debug_println(")");
          return;
        }
      } else if (!strcmp(key, "reg")) {
        reg_index = atoi(value);
        if (reg_index >= HOLDING_REGS_SIZE) {
          debug_print("SET GPIO: register index out of range (max ");
          debug_print_uint(HOLDING_REGS_SIZE - 1);
          debug_println(")");
          return;
        }
      } else if (!strcmp(key, "input")) {
        input_index = atoi(value);
        if (input_index >= (DISCRETE_INPUTS_SIZE * 8)) {
          debug_print("SET GPIO: input index out of range (max ");
          debug_print_uint((DISCRETE_INPUTS_SIZE * 8) - 1);
          debug_println(")");
          return;
        }
      } else {
        debug_print("SET GPIO STATIC: unknown parameter key: ");
        debug_println(key);
        return;
      }
    }

    // TODO: Store GPIO mapping to config
    debug_print("GPIO ");
    debug_print_uint(gpio_pin);
    debug_println(" STATIC mapping:");
    if (coil_index != 65535) {
      debug_print("  -> coil ");
      debug_print_uint(coil_index);
      debug_println("");
    }
    if (reg_index != 65535) {
      debug_print("  -> register ");
      debug_print_uint(reg_index);
      debug_println("");
    }
    if (input_index != 65535) {
      debug_print("  -> input ");
      debug_print_uint(input_index);
      debug_println("");
    }

  } else if (!strcmp(mode, "DYNAMIC")) {
    // DYNAMIC mode: set gpio <pin> DYNAMIC [coil:<idx>] [reg:<id>] [output-coil:<id>] [counter:<id>] [input-dis=<n>] [index-reg=<n>] ... or [timer:<id>]
    uint16_t coil_index = 65535;
    uint16_t reg_index = 65535;
    uint16_t output_coil = 65535;
    uint8_t counter_id = 0xff;
    uint8_t timer_id = 0xff;

    // Counter-specific parameters (for functions)
    uint16_t counter_input_dis = 65535;
    uint16_t counter_index_reg = 65535;
    uint16_t counter_raw_reg = 65535;
    uint16_t counter_freq_reg = 65535;
    uint16_t counter_overload_reg = 65535;
    uint16_t counter_ctrl_reg = 65535;

    // Parse parameters
    for (uint8_t i = 2; i < argc; i++) {
      char* arg = argv[i];

      // Look for both ':' (key:value) and '=' (key=value) separators
      char* sep = strchr(arg, ':');
      bool has_colon = sep != NULL;

      if (!has_colon) {
        sep = strchr(arg, '=');
        if (!sep) {
          debug_print("SET GPIO DYNAMIC: invalid parameter format: ");
          debug_println(arg);
          return;
        }
      }

      *sep = '\0';
      const char* key = arg;
      const char* value = sep + 1;

      if (!strcmp(key, "coil")) {
        coil_index = atoi(value);
        if (coil_index >= (COILS_SIZE * 8)) {
          debug_print("SET GPIO: coil index out of range (max ");
          debug_print_uint((COILS_SIZE * 8) - 1);
          debug_println(")");
          return;
        }
      } else if (!strcmp(key, "reg")) {
        reg_index = atoi(value);
        if (reg_index >= HOLDING_REGS_SIZE) {
          debug_print("SET GPIO: register index out of range (max ");
          debug_print_uint(HOLDING_REGS_SIZE - 1);
          debug_println(")");
          return;
        }
      } else if (!strcmp(key, "output-coil")) {
        output_coil = atoi(value);
        if (output_coil >= (COILS_SIZE * 8)) {
          debug_print("SET GPIO: output coil index out of range (max ");
          debug_print_uint((COILS_SIZE * 8) - 1);
          debug_println(")");
          return;
        }
      } else if (!strcmp(key, "counter")) {
        uint8_t id = atoi(value);
        if (id >= 1 && id <= 4) {
          counter_id = id;
        } else {
          debug_println("SET GPIO: invalid counter ID (1-4)");
          return;
        }
      } else if (!strcmp(key, "input-dis")) {
        // Counter function parameter
        counter_input_dis = atoi(value);
      } else if (!strcmp(key, "index-reg")) {
        // Counter function parameter
        counter_index_reg = atoi(value);
        if (counter_index_reg >= HOLDING_REGS_SIZE) {
          debug_print("SET GPIO: index-reg out of range (max ");
          debug_print_uint(HOLDING_REGS_SIZE - 1);
          debug_println(")");
          return;
        }
      } else if (!strcmp(key, "raw-reg")) {
        // Counter function parameter
        counter_raw_reg = atoi(value);
        if (counter_raw_reg >= HOLDING_REGS_SIZE) {
          debug_print("SET GPIO: raw-reg out of range (max ");
          debug_print_uint(HOLDING_REGS_SIZE - 1);
          debug_println(")");
          return;
        }
      } else if (!strcmp(key, "freq-reg") || !strcmp(key, "frekvens-reg")) {
        // Counter function parameter (both English and Danish naming)
        counter_freq_reg = atoi(value);
        if (counter_freq_reg >= HOLDING_REGS_SIZE) {
          debug_print("SET GPIO: freq-reg out of range (max ");
          debug_print_uint(HOLDING_REGS_SIZE - 1);
          debug_println(")");
          return;
        }
      } else if (!strcmp(key, "overload-reg")) {
        // Counter function parameter
        counter_overload_reg = atoi(value);
        if (counter_overload_reg >= HOLDING_REGS_SIZE) {
          debug_print("SET GPIO: overload-reg out of range (max ");
          debug_print_uint(HOLDING_REGS_SIZE - 1);
          debug_println(")");
          return;
        }
      } else if (!strcmp(key, "ctrl-reg")) {
        // Counter function parameter
        counter_ctrl_reg = atoi(value);
        if (counter_ctrl_reg >= HOLDING_REGS_SIZE) {
          debug_print("SET GPIO: ctrl-reg out of range (max ");
          debug_print_uint(HOLDING_REGS_SIZE - 1);
          debug_println(")");
          return;
        }
      } else if (!strcmp(key, "timer")) {
        uint8_t id = atoi(value);
        if (id >= 1 && id <= 4) {
          timer_id = id;
        } else {
          debug_println("SET GPIO: invalid timer ID (1-4)");
          return;
        }
        // NOTE: Timer mode-specific parameters (phase1-dur, pulse-dur, on-dur, input-dis, etc.)
        // can be parsed and stored similarly to counter parameters if needed in future
      } else {
        debug_print("SET GPIO DYNAMIC: unknown parameter key: ");
        debug_println(key);
        return;
      }
    }

    // Validate that counter or timer is specified
    if (counter_id == 0xff && timer_id == 0xff) {
      debug_println("SET GPIO DYNAMIC: must specify either counter:<id> or timer:<id>");
      return;
    }

    // TODO: Store GPIO DYNAMIC mapping to config
    debug_print("GPIO ");
    debug_print_uint(gpio_pin);
    debug_println(" DYNAMIC mapping:");
    if (coil_index != 65535) {
      debug_print("  -> coil ");
      debug_print_uint(coil_index);
      debug_println("");
    }
    if (reg_index != 65535) {
      debug_print("  -> register ");
      debug_print_uint(reg_index);
      debug_println("");
    }
    if (output_coil != 65535) {
      debug_print("  -> output-coil ");
      debug_print_uint(output_coil);
      debug_println("");
    }
    if (counter_id != 0xff) {
      debug_print("  -> counter ");
      debug_print_uint(counter_id);
      if (counter_input_dis != 65535) {
        debug_print(" (input-dis=");
        debug_print_uint(counter_input_dis);
        debug_print(")");
      }
      debug_println("");

      // Show counter function parameters if any were set
      if (counter_index_reg != 65535 || counter_raw_reg != 65535 ||
          counter_freq_reg != 65535 || counter_overload_reg != 65535 ||
          counter_ctrl_reg != 65535) {
        debug_println("    Counter function parameters:");
        if (counter_index_reg != 65535) {
          debug_print("      index-reg=");
          debug_print_uint(counter_index_reg);
          debug_println("");
        }
        if (counter_raw_reg != 65535) {
          debug_print("      raw-reg=");
          debug_print_uint(counter_raw_reg);
          debug_println("");
        }
        if (counter_freq_reg != 65535) {
          debug_print("      freq-reg=");
          debug_print_uint(counter_freq_reg);
          debug_println("");
        }
        if (counter_overload_reg != 65535) {
          debug_print("      overload-reg=");
          debug_print_uint(counter_overload_reg);
          debug_println("");
        }
        if (counter_ctrl_reg != 65535) {
          debug_print("      ctrl-reg=");
          debug_print_uint(counter_ctrl_reg);
          debug_println("");
        }
      }
    }
    if (timer_id != 0xff) {
      debug_print("  -> timer ");
      debug_print_uint(timer_id);
      debug_println("");
    }

  } else {
    debug_print("SET GPIO: unknown mode: ");
    debug_println(mode);
    debug_println("  Valid modes: STATIC, DYNAMIC");
    return;
  }
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
