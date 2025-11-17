/**
 * @file cli_config_regs.cpp
 * @brief CLI `set reg` command handlers (LAYER 7)
 *
 * Responsibility:
 * - Parse `set reg` commands (STATIC and DYNAMIC)
 * - Add/update register mappings to PersistConfig
 * - Display current register configuration
 */

#include "cli_config_regs.h"
#include "config_struct.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * set reg STATIC <address> Value <value>
 *
 * Example:
 *   set reg STATIC 0 Value 42
 *   set reg STATIC 100 Value 0
 */
void cli_cmd_set_reg_static(uint8_t argc, char* argv[]) {
  // set reg STATIC <address> Value <value>

  if (argc < 4) {
    debug_println("SET REG STATIC: missing arguments");
    debug_println("  Usage: set reg STATIC <address> Value <value>");
    return;
  }

  // Parse address
  uint16_t address = atoi(argv[0]);
  if (address >= HOLDING_REGS_SIZE) {
    debug_print("SET REG STATIC: address out of range (max ");
    debug_print_uint(HOLDING_REGS_SIZE - 1);
    debug_println(")");
    return;
  }

  // argv[1] should be "Value"
  if (strcmp(argv[1], "Value") != 0) {
    debug_println("SET REG STATIC: expected 'Value' keyword");
    debug_println("  Usage: set reg STATIC <address> Value <value>");
    return;
  }

  // Parse value
  uint16_t value = atoi(argv[2]);

  // Add or update STATIC register mapping
  uint8_t found = 0;
  for (uint8_t i = 0; i < g_persist_config.static_reg_count; i++) {
    if (g_persist_config.static_regs[i].register_address == address) {
      g_persist_config.static_regs[i].static_value = value;
      found = 1;
      break;
    }
  }

  if (!found) {
    if (g_persist_config.static_reg_count >= MAX_DYNAMIC_REGS) {
      debug_println("SET REG STATIC: max STATIC registers reached");
      return;
    }

    uint8_t idx = g_persist_config.static_reg_count;
    g_persist_config.static_regs[idx].register_address = address;
    g_persist_config.static_regs[idx].static_value = value;
    g_persist_config.static_reg_count++;
  }

  debug_print("Register ");
  debug_print_uint(address);
  debug_print(" STATIC = ");
  debug_print_uint(value);
  debug_println("");
}

/**
 * set reg DYNAMIC <address> counter<id>:<function>
 * set reg DYNAMIC <address> timer<id>:<function>
 *
 * Counter functions: index, raw, freq, overflow, ctrl
 * Timer functions: output
 *
 * Examples:
 *   set reg DYNAMIC 100 counter1:index
 *   set reg DYNAMIC 101 counter1:raw
 *   set reg DYNAMIC 102 counter1:freq
 *   set reg DYNAMIC 103 counter1:overflow
 *   set reg DYNAMIC 150 timer2:output
 */
void cli_cmd_set_reg_dynamic(uint8_t argc, char* argv[]) {
  // set reg DYNAMIC <address> counter<id>:<function> or timer<id>:<function>

  if (argc < 2) {
    debug_println("SET REG DYNAMIC: missing arguments");
    debug_println("  Usage: set reg DYNAMIC <address> counter<id>:<function> or timer<id>:<function>");
    debug_println("  Counter functions: index, raw, freq, overflow, ctrl");
    debug_println("  Timer functions: output");
    return;
  }

  // Parse address
  uint16_t address = atoi(argv[0]);
  if (address >= HOLDING_REGS_SIZE) {
    debug_print("SET REG DYNAMIC: address out of range (max ");
    debug_print_uint(HOLDING_REGS_SIZE - 1);
    debug_println(")");
    return;
  }

  // Parse source:function
  const char* source_str = argv[1];
  char* colon = strchr(source_str, ':');

  if (!colon) {
    debug_println("SET REG DYNAMIC: invalid format (expected counter<id>:<func> or timer<id>:<func>)");
    return;
  }

  // Extract source type and ID
  uint8_t source_type = 0xff;
  uint8_t source_id = 0xff;
  const char* function_str = colon + 1;

  char source_copy[32];
  strncpy(source_copy, source_str, colon - source_str);
  source_copy[colon - source_str] = '\0';

  if (strncmp(source_copy, "counter", 7) == 0) {
    source_type = DYNAMIC_SOURCE_COUNTER;
    source_id = atoi(source_copy + 7);
    if (source_id < 1 || source_id > 4) {
      debug_println("SET REG DYNAMIC: invalid counter ID (must be 1-4)");
      return;
    }
  } else if (strncmp(source_copy, "timer", 5) == 0) {
    source_type = DYNAMIC_SOURCE_TIMER;
    source_id = atoi(source_copy + 5);
    if (source_id < 1 || source_id > 4) {
      debug_println("SET REG DYNAMIC: invalid timer ID (must be 1-4)");
      return;
    }
  } else {
    debug_println("SET REG DYNAMIC: invalid source (must be counter<id> or timer<id>)");
    return;
  }

  // Parse function
  uint8_t source_function = 0xff;

  if (source_type == DYNAMIC_SOURCE_COUNTER) {
    if (!strcmp(function_str, "index")) {
      source_function = COUNTER_FUNC_INDEX;
    } else if (!strcmp(function_str, "raw")) {
      source_function = COUNTER_FUNC_RAW;
    } else if (!strcmp(function_str, "freq")) {
      source_function = COUNTER_FUNC_FREQ;
    } else if (!strcmp(function_str, "overflow")) {
      source_function = COUNTER_FUNC_OVERFLOW;
    } else if (!strcmp(function_str, "ctrl")) {
      source_function = COUNTER_FUNC_CTRL;
    } else {
      debug_println("SET REG DYNAMIC: invalid counter function");
      debug_println("  Valid: index, raw, freq, overflow, ctrl");
      return;
    }
  } else if (source_type == DYNAMIC_SOURCE_TIMER) {
    if (!strcmp(function_str, "output")) {
      source_function = TIMER_FUNC_OUTPUT;
    } else {
      debug_println("SET REG DYNAMIC: invalid timer function");
      debug_println("  Valid: output");
      return;
    }
  }

  // Add or update DYNAMIC register mapping
  uint8_t found = 0;
  for (uint8_t i = 0; i < g_persist_config.dynamic_reg_count; i++) {
    if (g_persist_config.dynamic_regs[i].register_address == address) {
      g_persist_config.dynamic_regs[i].source_type = source_type;
      g_persist_config.dynamic_regs[i].source_id = source_id;
      g_persist_config.dynamic_regs[i].source_function = source_function;
      found = 1;
      break;
    }
  }

  if (!found) {
    if (g_persist_config.dynamic_reg_count >= MAX_DYNAMIC_REGS) {
      debug_println("SET REG DYNAMIC: max DYNAMIC registers reached");
      return;
    }

    uint8_t idx = g_persist_config.dynamic_reg_count;
    g_persist_config.dynamic_regs[idx].register_address = address;
    g_persist_config.dynamic_regs[idx].source_type = source_type;
    g_persist_config.dynamic_regs[idx].source_id = source_id;
    g_persist_config.dynamic_regs[idx].source_function = source_function;
    g_persist_config.dynamic_reg_count++;
  }

  debug_print("Register ");
  debug_print_uint(address);
  debug_print(" DYNAMIC = ");
  if (source_type == DYNAMIC_SOURCE_COUNTER) {
    debug_print("counter");
    debug_print_uint(source_id);
  } else {
    debug_print("timer");
    debug_print_uint(source_id);
  }
  debug_print(":");
  debug_println(function_str);
}

/**
 * show reg - Display all STATIC and DYNAMIC register mappings
 */
void cli_cmd_show_regs(void) {
  debug_println("[regs]");

  // Show STATIC registers
  if (g_persist_config.static_reg_count > 0) {
    debug_println("# STATIC registers");
    for (uint8_t i = 0; i < g_persist_config.static_reg_count; i++) {
      debug_print("reg STATIC ");
      debug_print_uint(g_persist_config.static_regs[i].register_address);
      debug_print(" Value ");
      debug_print_uint(g_persist_config.static_regs[i].static_value);
      debug_println("");
    }
  }

  // Show DYNAMIC registers
  if (g_persist_config.dynamic_reg_count > 0) {
    debug_println("# DYNAMIC registers");
    for (uint8_t i = 0; i < g_persist_config.dynamic_reg_count; i++) {
      const DynamicRegisterMapping* dyn = &g_persist_config.dynamic_regs[i];
      debug_print("reg DYNAMIC ");
      debug_print_uint(dyn->register_address);
      debug_print(" ");

      if (dyn->source_type == DYNAMIC_SOURCE_COUNTER) {
        debug_print("counter");
        debug_print_uint(dyn->source_id);
        debug_print(":");

        switch (dyn->source_function) {
          case COUNTER_FUNC_INDEX:
            debug_print("index");
            break;
          case COUNTER_FUNC_RAW:
            debug_print("raw");
            break;
          case COUNTER_FUNC_FREQ:
            debug_print("freq");
            break;
          case COUNTER_FUNC_OVERFLOW:
            debug_print("overflow");
            break;
          case COUNTER_FUNC_CTRL:
            debug_print("ctrl");
            break;
          default:
            debug_print("?");
        }
      } else if (dyn->source_type == DYNAMIC_SOURCE_TIMER) {
        debug_print("timer");
        debug_print_uint(dyn->source_id);
        debug_print(":");

        switch (dyn->source_function) {
          case TIMER_FUNC_OUTPUT:
            debug_print("output");
            break;
          default:
            debug_print("?");
        }
      }

      debug_println("");
    }
  }

  if (g_persist_config.static_reg_count == 0 && g_persist_config.dynamic_reg_count == 0) {
    debug_println("# No registers configured");
  }
}
