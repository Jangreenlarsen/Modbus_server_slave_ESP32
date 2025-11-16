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
#include "registers.h"
#include "version.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * SHOW CONFIG
 * ============================================================================ */

void cli_cmd_show_config(void) {
  debug_println("\n=== SYSTEM CONFIGURATION ===\n");

  // Counter configuration block
  debug_println("COUNTERS:");
  for (uint8_t id = 1; id <= 4; id++) {
    CounterConfig cfg;
    if (counter_config_get(id, &cfg)) {
      debug_print("  Counter ");
      debug_print_uint(id);
      debug_print(": ");

      if (!cfg.enabled) {
        debug_println("disabled");
      } else {
        const char* mode_str = "unknown";
        if (cfg.hw_mode == COUNTER_HW_SW) mode_str = "SW";
        else if (cfg.hw_mode == COUNTER_HW_SW_ISR) mode_str = "SW-ISR";
        else if (cfg.hw_mode == COUNTER_HW_PCNT) mode_str = "HW-PCNT";

        debug_print(mode_str);
        debug_print(" mode, prescaler=");
        debug_print_uint(cfg.prescaler);
        debug_print(", ");
        debug_print_uint(cfg.bit_width);
        debug_println("-bit");
      }
    }
  }

  debug_println("\n(Full timer configuration not yet displayed)\n");
}

/* ============================================================================
 * SHOW COUNTERS
 * ============================================================================ */

void cli_cmd_show_counters(void) {
  debug_println("\n=== COUNTER STATUS ===\n");
  debug_println("ID   Mode     Enabled  Value        Hz");
  debug_println("--   ----     -------  -----------  ------");

  for (uint8_t id = 1; id <= 4; id++) {
    CounterConfig cfg;
    if (!counter_config_get(id, &cfg)) continue;

    char mode_str[10];
    if (cfg.hw_mode == COUNTER_HW_SW) strcpy(mode_str, "SW");
    else if (cfg.hw_mode == COUNTER_HW_SW_ISR) strcpy(mode_str, "ISR");
    else if (cfg.hw_mode == COUNTER_HW_PCNT) strcpy(mode_str, "HW");
    else strcpy(mode_str, "???");

    uint64_t value = counter_engine_get_value(id);

    debug_print_uint(id);
    debug_print("    ");
    debug_print(mode_str);
    debug_print("       ");
    if (cfg.enabled) {
      debug_print("Yes");
    } else {
      debug_print("No");
    }
    debug_print("      ");
    debug_print_uint((uint32_t)value);  // Simplified: truncate to 32-bit
    debug_println("");
  }

  debug_println("");
}

/* ============================================================================
 * SHOW TIMERS
 * ============================================================================ */

void cli_cmd_show_timers(void) {
  debug_println("\n=== TIMER STATUS ===\n");
  debug_println("(Timer functionality not yet ported)\n");
}

/* ============================================================================
 * SHOW REGISTERS
 * ============================================================================ */

void cli_cmd_show_registers(uint16_t start, uint16_t count) {
  if (start == 0 && count == 0) {
    // Show all holding registers (limited output)
    count = 32;  // Show first 32 by default
  }

  debug_println("\n=== HOLDING REGISTERS ===\n");

  for (uint16_t i = start; i < start + count && i < HOLDING_REGS_SIZE; i++) {
    uint16_t value = registers_get_holding_register(i);

    debug_print("Reg[");
    debug_print_uint(i);
    debug_print("] = ");
    debug_print_uint(value);
    debug_println("");

    // Limit output to prevent flood
    if ((i - start) >= 20) {
      debug_println("(... truncated, showing first 20 registers)");
      break;
    }
  }

  debug_println("");
}

/* ============================================================================
 * SHOW COILS
 * ============================================================================ */

void cli_cmd_show_coils(void) {
  debug_println("\n=== COILS ===\n");

  for (uint16_t i = 0; i < (COILS_SIZE * 8) && i < 16; i++) {  // Limit output
    uint8_t state = registers_get_coil(i) ? 1 : 0;

    debug_print("Coil[");
    debug_print_uint(i);
    debug_print("] = ");
    if (state) {
      debug_println("ON");
    } else {
      debug_println("OFF");
    }
  }

  if ((COILS_SIZE * 8) > 16) {
    debug_println("(... and more)");
  }

  debug_println("");
}

/* ============================================================================
 * SHOW INPUTS
 * ============================================================================ */

void cli_cmd_show_inputs(void) {
  debug_println("\n=== DISCRETE INPUTS ===\n");

  for (uint16_t i = 0; i < (DISCRETE_INPUTS_SIZE * 8) && i < 16; i++) {  // Limit
    uint8_t state = registers_get_discrete_input(i) ? 1 : 0;

    debug_print("Input[");
    debug_print_uint(i);
    debug_print("] = ");
    if (state) {
      debug_println("HIGH");
    } else {
      debug_println("LOW");
    }
  }

  if ((DISCRETE_INPUTS_SIZE * 8) > 16) {
    debug_println("(... and more)");
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
  debug_println("Target:  ESP32-WROOM-32");
  debug_println("Project: Modbus RTU Server");
  debug_println("");
}

/* ============================================================================
 * SHOW GPIO
 * ============================================================================ */

void cli_cmd_show_gpio(void) {
  debug_println("\n=== GPIO MAPPING ===\n");
  debug_println("UART1 (Modbus):");
  debug_println("  GPIO4  - RX");
  debug_println("  GPIO5  - TX");
  debug_println("  GPIO15 - RS485 DIR");
  debug_println("");
  debug_println("PCNT Counters:");
  debug_println("  GPIO19 - Counter 1 (PCNT Unit 0)");
  debug_println("  GPIO25 - Counter 2 (PCNT Unit 1)");
  debug_println("  GPIO27 - Counter 3 (PCNT Unit 2)");
  debug_println("  GPIO33 - Counter 4 (PCNT Unit 3)");
  debug_println("");
}
