/**
 * @file cli_show.cpp
 * @brief CLI `show` command handlers (LAYER 7)
 *
 * Ported from: Mega2560 v3.6.5 cli_shell.cpp (show command handlers)
 * Adapted to: ESP32 modular architecture
 *
 * Responsibility:
 * - Retrieve system state (config, status, register values)
 * - Format for terminal display (tables, lists, etc.)
 * - Print via debug_println()
 *
 * Context: Formatting optimized for 80-char serial terminals
 */

#include "cli_show.h"
#include "counter_engine.h"
#include "counter_config.h"
#include "counter_frequency.h"
#include "timer_engine.h"
#include "timer_config.h"
#include "registers.h"
#include "version.h"
#include "cli_shell.h"
#include "config_struct.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * SHOW CONFIG
 * ============================================================================ */

void cli_cmd_show_config(void) {
  debug_println("\n=== CONFIGURATION ===");
  debug_print("Version: ");
  debug_print(PROJECT_VERSION);
  debug_print(" Build #");
  debug_print_uint(BUILD_NUMBER);
  debug_println("");
  debug_print("Built: ");
  debug_print(BUILD_TIMESTAMP);
  debug_print(" (");
  debug_print(GIT_BRANCH);
  debug_print("@");
  debug_print(GIT_HASH);
  debug_println(")");

  // Show actual slave ID from config
  debug_print("Unit-ID: ");
  debug_print_uint(g_persist_config.slave_id);
  debug_println(" (SLAVE)");

  // Show actual baudrate from config
  debug_print("Baud: ");
  debug_print_uint(g_persist_config.baudrate);
  debug_println("");

  debug_println("Server: RUNNING");
  debug_println("Mode: SERVER");
  debug_println("Hostname: esp32-modbus");
  debug_println("=====================\n");

  // Counter configuration block (Mega2560 format)
  bool any_counter = false;
  debug_println("counters");
  for (uint8_t id = 1; id <= 4; id++) {
    CounterConfig cfg;
    if (counter_config_get(id, &cfg) && cfg.enabled) {
      any_counter = true;
      debug_print("  counter ");
      debug_print_uint(id);
      debug_print(" ENABLED");

      // edge mode
      const char* edge_str = "rising";
      if (cfg.edge_type == COUNTER_EDGE_FALLING) edge_str = "falling";
      else if (cfg.edge_type == COUNTER_EDGE_BOTH) edge_str = "both";
      debug_print(" edge=");
      debug_print(edge_str);

      // prescaler and resolution
      debug_print(" prescaler=");
      debug_print_uint(cfg.prescaler);
      debug_print(" res=");
      debug_print_uint(cfg.bit_width);

      // direction
      const char* dir_str = (cfg.direction == COUNTER_DIR_DOWN) ? "down" : "up";
      debug_print(" dir=");
      debug_print(dir_str);

      // scale (format as float)
      debug_print(" scale=");
      debug_print_float(cfg.scale_factor);

      // Register indices
      debug_print(" input-dis=");
      debug_print_uint(cfg.input_dis);
      debug_print(" index-reg=");
      debug_print_uint(cfg.index_reg);

      if (cfg.raw_reg > 0) {
        debug_print(" raw-reg=");
        debug_print_uint(cfg.raw_reg);
      }
      if (cfg.freq_reg > 0) {
        debug_print(" freq-reg=");
        debug_print_uint(cfg.freq_reg);
      }

      debug_print(" overload-reg=");
      if (cfg.overload_reg < 1000) debug_print_uint(cfg.overload_reg);
      else debug_print("n/a");

      debug_print(" ctrl-reg=");
      if (cfg.ctrl_reg < 1000) debug_print_uint(cfg.ctrl_reg);
      else debug_print("n/a");

      // start value
      debug_print(" start=");
      debug_print_uint((uint32_t)cfg.start_value);

      // debounce
      debug_print(" debounce=");
      if (cfg.debounce_enabled && cfg.debounce_ms > 0) {
        debug_print("on/");
        debug_print_uint(cfg.debounce_ms);
        debug_print("ms");
      } else {
        debug_print("off");
      }

      // hw-mode
      debug_print(" hw-mode=");
      if (cfg.hw_mode == COUNTER_HW_SW) {
        if (cfg.interrupt_pin > 0) {
          debug_print("sw-isr");
        } else {
          debug_print("sw");
        }
      } else if (cfg.hw_mode == COUNTER_HW_SW_ISR) {
        debug_print("sw-isr");
      } else if (cfg.hw_mode == COUNTER_HW_PCNT) {
        debug_print("hw");
      } else {
        debug_print("unknown");
      }

      // interrupt pin for ISR mode
      if ((cfg.hw_mode == COUNTER_HW_SW || cfg.hw_mode == COUNTER_HW_SW_ISR) && cfg.interrupt_pin > 0) {
        debug_print(" interrupt-pin=");
        debug_print_uint(cfg.interrupt_pin);
      }

      debug_println("");
    }
  }
  if (!any_counter) {
    debug_println("counters (none enabled)");
  }

  // Counter control block (if any enabled)
  bool show_control = false;
  for (uint8_t id = 1; id <= 4; id++) {
    CounterConfig cfg;
    if (counter_config_get(id, &cfg) && cfg.enabled) {
      show_control = true;
      break;
    }
  }

  if (show_control) {
    debug_println("counters control");
    for (uint8_t id = 1; id <= 4; id++) {
      CounterConfig cfg;
      if (counter_config_get(id, &cfg) && cfg.enabled) {
        debug_print("  counter");
        debug_print_uint(id);
        debug_print(" reset-on-read DISABLE auto-start DISABLE");
        debug_println("");
      }
    }
  }

  debug_println("\n(Full timer configuration not yet displayed)\n");

  // GPIO Mappings section
  debug_println("=== GPIO MAPPINGS ===\n");

  // GPIO2 status (heartbeat control)
  debug_print("GPIO 2 Status: ");
  if (g_persist_config.gpio2_user_mode == 0) {
    debug_println("HEARTBEAT (LED blink aktiv)");
  } else {
    debug_println("USER MODE (frigivet til bruger)");
  }
  debug_println("  Kommandoer: 'set gpio 2 enable' / 'set gpio 2 disable'");
  debug_println("");

  debug_println("Hardware (fixed):");
  debug_println("  GPIO 4  - UART1 RX (Modbus)");
  debug_println("  GPIO 5  - UART1 TX (Modbus)");
  debug_println("  GPIO 15 - RS485 DIR");
  debug_println("  GPIO 19 - Counter 1 HW (PCNT Unit 0)");
  debug_println("  GPIO 25 - Counter 2 HW (PCNT Unit 1)");
  debug_println("  GPIO 27 - Counter 3 HW (PCNT Unit 2)");
  debug_println("  GPIO 33 - Counter 4 HW (PCNT Unit 3)");
  debug_println("");

  // Check if any GPIO mappings exist
  if (g_persist_config.gpio_map_count > 0) {
    debug_print("User configured (");
    debug_print_uint(g_persist_config.gpio_map_count);
    debug_println(" mappings):");
    for (uint8_t i = 0; i < g_persist_config.gpio_map_count; i++) {
      const GPIOMapping* gpio = &g_persist_config.gpio_maps[i];
      debug_print("  GPIO ");
      debug_print_uint(gpio->gpio_pin);
      debug_print(" - ");
      if (gpio->is_input) {
        debug_print("INPUT:");
        debug_print_uint(gpio->input_reg);
      } else {
        debug_print("COIL:");
        debug_print_uint(gpio->coil_reg);
      }
      if (gpio->associated_counter != 0xff) {
        debug_print(" (Counter ");
        debug_print_uint(gpio->associated_counter);
        debug_print(")");
      }
      if (gpio->associated_timer != 0xff) {
        debug_print(" (Timer ");
        debug_print_uint(gpio->associated_timer);
        debug_print(")");
      }
      debug_print("  [fjern: 'no set gpio ");
      debug_print_uint(gpio->gpio_pin);
      debug_print("']");
      debug_println("");
    }
  } else {
    debug_println("(No user-configured GPIO mappings)");
    debug_println("  Brug: 'set gpio <pin> static map input:<idx>' eller 'coil:<idx>'");
  }
  debug_println("");
}

/* ============================================================================
 * SHOW COUNTERS
 * ============================================================================ */

void cli_cmd_show_counters(void) {
  // Header med forkortelser (3 linjer)
  debug_println("----------------------------------------------------------------------------------------------------------------------------------------------");
  debug_println("co = count-on, sv = startValue, res = resolution, ps = prescaler, ir = index-reg, rr = raw-reg, fr = freq-reg");
  debug_println("or = overload-reg, cr = ctrl-reg, dir = direction, sf = scaleFloat, dis = input-dis, d = debounce, dt = debounce-ms");
  debug_println("hw = HW/SW mode (SW|ISR|HW), pin = GPIO pin (actual hardware pin), hz = measured freq (Hz)");
  debug_println("value = scaled value, raw = raw counter value");
  debug_println("----------------------------------------------------------------------------------------------------------------------------------------------");

  // Kolonne headers
  debug_println("counter | mode | hw  | pin  | co      | sv       | res | ps   | ir   | rr   | fr   | or   | cr   | dir   | sf     | d   | dt   | hz    | value     | raw");

  // Data rækker for hver counter
  for (uint8_t id = 1; id <= 4; id++) {
    CounterConfig cfg;
    if (!counter_config_get(id, &cfg)) continue;

    char line[256];
    char* p = line;

    // Counter ID (7 chars left-aligned)
    p += snprintf(p, sizeof(line) - (p - line), " %-7d", id);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Mode (4 chars left-aligned)
    p += snprintf(p, sizeof(line) - (p - line), "%-4d ", cfg.enabled ? 1 : 0);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // HW mode (3 chars left-aligned)
    const char* hw_str = "???";
    if (cfg.hw_mode == COUNTER_HW_SW) hw_str = "SW";
    else if (cfg.hw_mode == COUNTER_HW_SW_ISR) hw_str = "ISR";
    else if (cfg.hw_mode == COUNTER_HW_PCNT) hw_str = "HW";
    p += snprintf(p, sizeof(line) - (p - line), "%-3s ", hw_str);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Pin (4 chars left-aligned)
    // BUG FIX 1.9: Show hw_gpio from config instead of hardcoded values
    if (cfg.hw_mode == COUNTER_HW_PCNT && cfg.hw_gpio > 0) {
      p += snprintf(p, sizeof(line) - (p - line), "%-4d ", cfg.hw_gpio);
    } else if (cfg.hw_mode == COUNTER_HW_SW_ISR && cfg.interrupt_pin > 0) {
      p += snprintf(p, sizeof(line) - (p - line), "%-4d ", cfg.interrupt_pin);
    } else if (cfg.input_dis > 0) {
      p += snprintf(p, sizeof(line) - (p - line), "%-4d ", cfg.input_dis);
    } else {
      p += snprintf(p, sizeof(line) - (p - line), "%-4s ", "-");
    }
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Count-on (7 chars left-aligned)
    const char* edge_str = "???";
    if (cfg.edge_type == COUNTER_EDGE_RISING) edge_str = "rising";
    else if (cfg.edge_type == COUNTER_EDGE_FALLING) edge_str = "falling";
    else if (cfg.edge_type == COUNTER_EDGE_BOTH) edge_str = "both";
    p += snprintf(p, sizeof(line) - (p - line), "%-7s ", edge_str);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Start value (8 chars right-aligned)
    p += snprintf(p, sizeof(line) - (p - line), "%8u ", (unsigned int)cfg.start_value);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Resolution (3 chars right-aligned)
    p += snprintf(p, sizeof(line) - (p - line), "%3d ", cfg.bit_width);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Prescaler (4 chars right-aligned)
    p += snprintf(p, sizeof(line) - (p - line), "%4u ", cfg.prescaler);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // index-reg (4 chars right-aligned)
    p += snprintf(p, sizeof(line) - (p - line), "%4u ", cfg.index_reg);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // raw-reg (4 chars right-aligned)
    p += snprintf(p, sizeof(line) - (p - line), "%4u ", cfg.raw_reg);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // freq-reg (4 chars right-aligned)
    p += snprintf(p, sizeof(line) - (p - line), "%4u ", cfg.freq_reg);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // overload-reg (4 chars right-aligned)
    if (cfg.overload_reg < 1000) {
      p += snprintf(p, sizeof(line) - (p - line), "%4u ", cfg.overload_reg);
    } else {
      p += snprintf(p, sizeof(line) - (p - line), " n/a ");
    }
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // ctrl-reg (4 chars right-aligned)
    if (cfg.ctrl_reg < 1000) {
      p += snprintf(p, sizeof(line) - (p - line), "%4u ", cfg.ctrl_reg);
    } else {
      p += snprintf(p, sizeof(line) - (p - line), " n/a ");
    }
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Direction (5 chars left-aligned)
    const char* dir_str = cfg.direction == COUNTER_DIR_UP ? "up" : "down";
    p += snprintf(p, sizeof(line) - (p - line), "%-5s ", dir_str);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Scale factor (6 chars, format: X.XXX)
    p += snprintf(p, sizeof(line) - (p - line), "%6.3f ", cfg.scale_factor);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Debounce enabled (3 chars left-aligned)
    const char* deb_str = cfg.debounce_enabled ? "on" : "off";
    p += snprintf(p, sizeof(line) - (p - line), "%-3s ", deb_str);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Debounce time (4 chars right-aligned)
    p += snprintf(p, sizeof(line) - (p - line), "%4u ", cfg.debounce_ms);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Measured frequency (5 chars right-aligned)
    uint32_t freq = counter_frequency_get(id);
    p += snprintf(p, sizeof(line) - (p - line), "%5u ", freq);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Scaled value (9 chars right-aligned)
    uint64_t raw_value = counter_engine_get_value(id);
    uint64_t scaled_value = (uint64_t)(raw_value * cfg.scale_factor);
    p += snprintf(p, sizeof(line) - (p - line), "%9u ", (unsigned int)scaled_value);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Raw value (prescaled, 10 chars right-aligned, sidste kolonne)
    uint64_t raw_prescaled = raw_value / cfg.prescaler;
    p += snprintf(p, sizeof(line) - (p - line), "%10u", (unsigned int)raw_prescaled);

    // Print hele linjen
    debug_println(line);
  }

  // Counter control status sektion
  debug_println("\n=== COUNTER CONTROL STATUS ===");
  for (uint8_t id = 1; id <= 4; id++) {
    CounterConfig cfg;
    if (!counter_config_get(id, &cfg) || !cfg.enabled) continue;

    // Læs ctrl-reg hvis konfigureret
    uint16_t ctrl_value = 0;
    if (cfg.ctrl_reg < HOLDING_REGS_SIZE) {
      ctrl_value = registers_get_holding_register(cfg.ctrl_reg);
    }

    // Bit 0 = reset-on-read, Bit 1 = auto-start, Bit 7 = running
    bool reset_on_read = (ctrl_value & 0x01) != 0;
    bool auto_start = (ctrl_value & 0x02) != 0;
    bool running = (ctrl_value & 0x80) != 0;

    debug_print("Counter");
    debug_print_uint(id);
    debug_print(" reset-on-read: ");
    debug_print(reset_on_read ? "ENABLED" : "DISABLED");
    debug_print(" | auto-start: ");
    debug_print(auto_start ? "ENABLED" : "DISABLED");
    debug_print(" | running: ");
    debug_println(running ? "YES" : "NO");
  }
  debug_println("==============================\n");
}

/* ============================================================================
 * SHOW TIMERS
 * ============================================================================ */

void cli_cmd_show_timers(void) {
  debug_println("\n=== TIMER STATUS (Timer 1-4) ===\n");

  for (uint8_t id = 1; id <= 4; id++) {
    TimerConfig cfg;
    if (!timer_engine_get_config(id, &cfg)) {
      continue;
    }

    debug_print("Timer ");
    debug_print_uint(id);
    debug_print(": ");

    if (!cfg.enabled) {
      debug_println("DISABLED");
      continue;
    }

    // Show mode and status
    switch (cfg.mode) {
      case TIMER_MODE_1_ONESHOT:
        debug_println("ONE-SHOT (Mode 1)");
        debug_print("  P1: ");
        debug_print_uint(cfg.phase1_duration_ms);
        debug_print("ms → ");
        debug_print_uint(cfg.phase1_output_state);
        debug_println("");
        debug_print("  P2: ");
        debug_print_uint(cfg.phase2_duration_ms);
        debug_print("ms → ");
        debug_print_uint(cfg.phase2_output_state);
        debug_println("");
        debug_print("  P3: ");
        debug_print_uint(cfg.phase3_duration_ms);
        debug_print("ms → ");
        debug_print_uint(cfg.phase3_output_state);
        debug_println("");
        break;

      case TIMER_MODE_2_MONOSTABLE:
        debug_println("MONOSTABLE (Mode 2)");
        debug_print("  Pulse: ");
        debug_print_uint(cfg.pulse_duration_ms);
        debug_println("ms");
        debug_print("  P1 (rest): ");
        debug_print_uint(cfg.phase1_output_state);
        debug_println("");
        debug_print("  P2 (pulse): ");
        debug_print_uint(cfg.phase2_output_state);
        debug_println("");
        break;

      case TIMER_MODE_3_ASTABLE:
        debug_println("ASTABLE (Mode 3)");
        debug_print("  ON: ");
        debug_print_uint(cfg.on_duration_ms);
        debug_println("ms");
        debug_print("  OFF: ");
        debug_print_uint(cfg.off_duration_ms);
        debug_println("ms");
        debug_print("  P1 (ON): ");
        debug_print_uint(cfg.phase1_output_state);
        debug_println("");
        debug_print("  P2 (OFF): ");
        debug_print_uint(cfg.phase2_output_state);
        debug_println("");
        break;

      case TIMER_MODE_4_INPUT_TRIGGERED:
        debug_println("INPUT-TRIGGERED (Mode 4)");
        debug_print("  Input COIL: ");
        debug_print_uint(cfg.input_dis);
        debug_println("");
        debug_print("  Trigger edge: ");
        debug_println(cfg.trigger_edge == 1 ? "RISING (0→1)" : "FALLING (1→0)");
        debug_print("  Delay: ");
        debug_print_uint(cfg.delay_ms);
        debug_println("ms");
        debug_print("  Output level: ");
        debug_print_uint(cfg.phase1_output_state);
        debug_println("");
        break;

      default:
        debug_println("UNKNOWN MODE");
        break;
    }

    debug_print("  Output COIL: ");
    debug_print_uint(cfg.output_coil);
    debug_println("");
    debug_println("");
  }
}

/* ============================================================================
 * SHOW REGISTERS
 * ============================================================================ */

void cli_cmd_show_registers(uint16_t start, uint16_t count) {
  debug_println("\n=== HOLDING REGISTERS (0..159) ===\n");

  // Show registers in groups of 8 (Mega2560 format)
  for (uint16_t base = 0; base < HOLDING_REGS_SIZE && base < 160; base += 8) {
    debug_print_uint(base);
    debug_print(": ");

    for (uint8_t i = 0; i < 8 && (base + i) < HOLDING_REGS_SIZE; i++) {
      uint16_t value = registers_get_holding_register(base + i);
      debug_print_uint(value);
      debug_print("\t");
    }
    debug_println("");
  }

  debug_println("");
}

/* ============================================================================
 * SHOW COILS
 * ============================================================================ */

void cli_cmd_show_coils(void) {
  debug_println("\n=== COILS (0..63) ===\n");

  // Show coils in groups of 16 (Mega2560 format)
  for (uint16_t base = 0; base < (COILS_SIZE * 8) && base < 64; base += 16) {
    debug_print_uint(base);
    debug_print(": ");

    for (uint8_t i = 0; i < 16 && (base + i) < (COILS_SIZE * 8); i++) {
      bool state = registers_get_coil(base + i);
      debug_print(state ? "1" : "0");
      debug_print(" ");
    }
    debug_println("");
  }

  debug_println("");
}

/* ============================================================================
 * SHOW INPUTS
 * ============================================================================ */

void cli_cmd_show_inputs(void) {
  debug_println("\n=== DISCRETE INPUTS (0..255) ===\n");

  // Show discrete inputs in groups of 16 (Mega2560 format)
  for (uint16_t base = 0; base < (DISCRETE_INPUTS_SIZE * 8) && base < 256; base += 16) {
    debug_print_uint(base);
    debug_print(": ");

    for (uint8_t i = 0; i < 16 && (base + i) < (DISCRETE_INPUTS_SIZE * 8); i++) {
      bool state = registers_get_discrete_input(base + i);
      debug_print(state ? "1" : "0");
      debug_print(" ");
    }
    debug_println("");
  }

  debug_println("");
}

/* ============================================================================
 * SHOW VERSION
 * ============================================================================ */

void cli_cmd_show_version(void) {
  debug_println("\n=== FIRMWARE VERSION ===\n");
  debug_print("Version: ");
  debug_println(PROJECT_VERSION);
  debug_print("Build:   #");
  debug_print_uint(BUILD_NUMBER);
  debug_print(" (");
  debug_print(BUILD_TIMESTAMP);
  debug_println(")");
  debug_print("Git:     ");
  debug_print(GIT_BRANCH);
  debug_print("@");
  debug_println(GIT_HASH);
  debug_println("Target:  ESP32-WROOM-32");
  debug_println("Project: Modbus RTU Server");
  debug_println("");
}

/* ============================================================================
 * SHOW GPIO
 * ============================================================================ */

void cli_cmd_show_gpio(void) {
  debug_println("\n=== GPIO MAPPING ===\n");

  // GPIO2 status
  debug_print("GPIO 2 Status: ");
  if (g_persist_config.gpio2_user_mode == 0) {
    debug_println("HEARTBEAT (LED blink aktiv)");
  } else {
    debug_println("USER MODE (frigivet til bruger)");
  }
  debug_println("  Kommandoer: 'set gpio 2 enable' / 'set gpio 2 disable'");
  debug_println("");

  debug_println("UART1 (Modbus):");
  debug_println("  GPIO 4  - RX");
  debug_println("  GPIO 5  - TX");
  debug_println("  GPIO 15 - RS485 DIR");
  debug_println("");
  debug_println("PCNT Counters:");
  debug_println("  GPIO 19 - Counter 1 (PCNT Unit 0)");
  debug_println("  GPIO 25 - Counter 2 (PCNT Unit 1)");
  debug_println("  GPIO 27 - Counter 3 (PCNT Unit 2)");
  debug_println("  GPIO 33 - Counter 4 (PCNT Unit 3)");
  debug_println("");

  // Show user-configured GPIO mappings
  if (g_persist_config.gpio_map_count > 0) {
    debug_print("User configured (");
    debug_print_uint(g_persist_config.gpio_map_count);
    debug_println(" mappings):");
    for (uint8_t i = 0; i < g_persist_config.gpio_map_count; i++) {
      const GPIOMapping* gpio = &g_persist_config.gpio_maps[i];
      debug_print("  GPIO ");
      debug_print_uint(gpio->gpio_pin);
      debug_print(" - ");
      if (gpio->is_input) {
        debug_print("INPUT:");
        debug_print_uint(gpio->input_reg);
      } else {
        debug_print("COIL:");
        debug_print_uint(gpio->coil_reg);
      }
      debug_print("  [fjern: 'no set gpio ");
      debug_print_uint(gpio->gpio_pin);
      debug_print("']");
      debug_println("");
    }
  } else {
    debug_println("(No user-configured GPIO mappings)");
  }
  debug_println("");
}

/* ============================================================================
 * SHOW ECHO
 * ============================================================================ */

void cli_cmd_show_echo(void) {
  debug_println("\n=== REMOTE ECHO ===");
  if (cli_shell_get_remote_echo()) {
    debug_println("Status: ON (characters are echoed back to terminal)");
  } else {
    debug_println("Status: OFF (characters are NOT echoed back)");
  }
  debug_println("Set: set echo <on|off>");
  debug_println("=====================\n");
}

/* ============================================================================
 * READ REG
 * ============================================================================ */

void cli_cmd_read_reg(uint8_t argc, char* argv[]) {
  // read reg <id> <antal>
  if (argc < 2) {
    debug_println("READ REG: manglende parametre");
    debug_println("  Brug: read reg <id> <antal>");
    debug_println("  Eksempel: read reg 90 10");
    return;
  }

  uint16_t start_addr = atoi(argv[0]);
  uint16_t count = atoi(argv[1]);

  // Validate parameters
  if (start_addr >= HOLDING_REGS_SIZE) {
    debug_print("READ REG: startadresse udenfor omr\u00e5de (max ");
    debug_print_uint(HOLDING_REGS_SIZE - 1);
    debug_println(")");
    return;
  }

  if (count == 0) {
    debug_println("READ REG: antal skal v\u00e6re st\u00f8rre end 0");
    return;
  }

  // Adjust count if it exceeds available registers
  if (start_addr + count > HOLDING_REGS_SIZE) {
    count = HOLDING_REGS_SIZE - start_addr;
    debug_print("READ REG: justeret antal til ");
    debug_print_uint(count);
    debug_println(" registre");
  }

  // Read and display registers
  debug_println("\n=== L\u00c6SNING AF HOLDING REGISTERS ===");
  debug_print("Adresse ");
  debug_print_uint(start_addr);
  debug_print(" til ");
  debug_print_uint(start_addr + count - 1);
  debug_println(":\n");

  for (uint16_t i = 0; i < count; i++) {
    uint16_t addr = start_addr + i;
    uint16_t value = registers_get_holding_register(addr);

    debug_print("Reg[");
    debug_print_uint(addr);
    debug_print("]: ");
    debug_print_uint(value);
    debug_println("");
  }
  debug_println("");
}

/* ============================================================================
 * READ COIL
 * ============================================================================ */

void cli_cmd_read_coil(uint8_t argc, char* argv[]) {
  // read coil <id> <antal>
  if (argc < 2) {
    debug_println("READ COIL: manglende parametre");
    debug_println("  Brug: read coil <id> <antal>");
    debug_println("  Eksempel: read coil 0 16");
    return;
  }

  uint16_t start_addr = atoi(argv[0]);
  uint16_t count = atoi(argv[1]);

  // Validate parameters
  if (start_addr >= (COILS_SIZE * 8)) {
    debug_print("READ COIL: startadresse udenfor omr\u00e5de (max ");
    debug_print_uint((COILS_SIZE * 8) - 1);
    debug_println(")");
    return;
  }

  if (count == 0) {
    debug_println("READ COIL: antal skal v\u00e6re st\u00f8rre end 0");
    return;
  }

  // Adjust count if it exceeds available coils
  if (start_addr + count > (COILS_SIZE * 8)) {
    count = (COILS_SIZE * 8) - start_addr;
    debug_print("READ COIL: justeret antal til ");
    debug_print_uint(count);
    debug_println(" coils");
  }

  // Read and display coils
  debug_println("\n=== L\u00c6SNING AF COILS ===");
  debug_print("Adresse ");
  debug_print_uint(start_addr);
  debug_print(" til ");
  debug_print_uint(start_addr + count - 1);
  debug_println(":\n");

  for (uint16_t i = 0; i < count; i++) {
    uint16_t addr = start_addr + i;
    uint8_t value = registers_get_coil(addr);

    debug_print("Coil[");
    debug_print_uint(addr);
    debug_print("]: ");
    debug_print(value ? "1" : "0");
    debug_println("");
  }
  debug_println("");
}

/* ============================================================================
 * READ INPUT
 * ============================================================================ */

void cli_cmd_read_input(uint8_t argc, char* argv[]) {
  // read input <id> <antal>
  if (argc < 2) {
    debug_println("READ INPUT: manglende parametre");
    debug_println("  Brug: read input <id> <antal>");
    debug_println("  Eksempel: read input 0 16");
    return;
  }

  uint16_t start_addr = atoi(argv[0]);
  uint16_t count = atoi(argv[1]);

  // Validate parameters
  if (start_addr >= (DISCRETE_INPUTS_SIZE * 8)) {
    debug_print("READ INPUT: startadresse udenfor omr\u00e5de (max ");
    debug_print_uint((DISCRETE_INPUTS_SIZE * 8) - 1);
    debug_println(")");
    return;
  }

  if (count == 0) {
    debug_println("READ INPUT: antal skal v\u00e6re st\u00f8rre end 0");
    return;
  }

  // Adjust count if it exceeds available inputs
  if (start_addr + count > (DISCRETE_INPUTS_SIZE * 8)) {
    count = (DISCRETE_INPUTS_SIZE * 8) - start_addr;
    debug_print("READ INPUT: justeret antal til ");
    debug_print_uint(count);
    debug_println(" discrete inputs");
  }

  // Read and display discrete inputs
  debug_println("\n=== L\u00c6SNING AF DISCRETE INPUTS ===");
  debug_print("Adresse ");
  debug_print_uint(start_addr);
  debug_print(" til ");
  debug_print_uint(start_addr + count - 1);
  debug_println(":\n");

  for (uint16_t i = 0; i < count; i++) {
    uint16_t addr = start_addr + i;
    uint8_t value = registers_get_discrete_input(addr);

    debug_print("Input[");
    debug_print_uint(addr);
    debug_print("]: ");
    debug_print(value ? "1" : "0");
    debug_println("");
  }
  debug_println("");
}
