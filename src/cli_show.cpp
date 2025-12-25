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
#include "registers_persist.h"
#include "watchdog_monitor.h"
#include "version.h"
#include "cli_shell.h"
#include "config_struct.h"
#include "st_logic_config.h"
#include "network_manager.h"
#include "network_config.h"
#include "debug_flags.h"
#include "debug.h"
#include <Arduino.h>
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

  // Show hostname
  debug_print("Hostname: ");
  debug_println(g_persist_config.hostname[0] ? g_persist_config.hostname : "(NOT SET)");

  // Show Modbus Slave configuration
  debug_print("Modbus Slave: ");
  debug_println(g_persist_config.modbus_slave.enabled ? "ENABLED" : "DISABLED");
  debug_print("  Unit-ID: ");
  debug_print_uint(g_persist_config.modbus_slave.slave_id);
  debug_println("");
  debug_print("  Baud: ");
  debug_print_uint(g_persist_config.modbus_slave.baudrate);
  debug_println("");
  debug_print("  Parity: ");
  if (g_persist_config.modbus_slave.parity == 0) debug_println("NONE");
  else if (g_persist_config.modbus_slave.parity == 1) debug_println("EVEN");
  else if (g_persist_config.modbus_slave.parity == 2) debug_println("ODD");
  else debug_println("UNKNOWN");
  debug_print("  Stop Bits: ");
  debug_print_uint(g_persist_config.modbus_slave.stop_bits);
  debug_println("");
  debug_print("  Inter-frame Delay: ");
  debug_print_uint(g_persist_config.modbus_slave.inter_frame_delay);
  debug_println(" ms");

  // Show Modbus Master configuration (v4.4+)
  debug_println("");
  debug_print("Modbus Master: ");
  debug_println(g_persist_config.modbus_master.enabled ? "ENABLED" : "DISABLED");
  if (g_persist_config.modbus_master.enabled) {
    debug_print("  Baudrate: ");
    debug_print_uint(g_persist_config.modbus_master.baudrate);
    debug_println("");
    debug_print("  Parity: ");
    if (g_persist_config.modbus_master.parity == 0) debug_println("NONE");
    else if (g_persist_config.modbus_master.parity == 1) debug_println("EVEN");
    else if (g_persist_config.modbus_master.parity == 2) debug_println("ODD");
    else debug_println("UNKNOWN");
    debug_print("  Stop Bits: ");
    debug_print_uint(g_persist_config.modbus_master.stop_bits);
    debug_println("");
    debug_print("  Timeout: ");
    debug_print_uint(g_persist_config.modbus_master.timeout_ms);
    debug_println(" ms");
    debug_print("  Inter-frame Delay: ");
    debug_print_uint(g_persist_config.modbus_master.inter_frame_delay);
    debug_println(" ms");
    debug_print("  Max Requests/Cycle: ");
    debug_print_uint(g_persist_config.modbus_master.max_requests_per_cycle);
    debug_println("");
  }

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
      debug_print(": enabled");

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
    debug_println("  (none configured)");
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
        debug_print(" reset-on-read disabled auto-start disabled");
        debug_println("");
      }
    }
  }

  // Timer configuration block
  bool any_timer = false;
  debug_println("\ntimers");
  for (uint8_t id = 1; id <= 4; id++) {
    TimerConfig cfg;
    if (timer_engine_get_config(id, &cfg) && cfg.enabled) {
      any_timer = true;
      debug_print("  timer ");
      debug_print_uint(id);
      debug_print(" mode=");
      debug_print_uint(cfg.mode);

      switch (cfg.mode) {
        case TIMER_MODE_1_ONESHOT:
          debug_print(" p1-dur=");
          debug_print_uint(cfg.phase1_duration_ms);
          debug_print(" p1-out=");
          debug_print_uint(cfg.phase1_output_state);
          debug_print(" p2-dur=");
          debug_print_uint(cfg.phase2_duration_ms);
          debug_print(" p2-out=");
          debug_print_uint(cfg.phase2_output_state);
          debug_print(" p3-dur=");
          debug_print_uint(cfg.phase3_duration_ms);
          debug_print(" p3-out=");
          debug_print_uint(cfg.phase3_output_state);
          break;
        case TIMER_MODE_2_MONOSTABLE:
          debug_print(" pulse-ms=");
          debug_print_uint(cfg.pulse_duration_ms);
          debug_print(" p1-out=");
          debug_print_uint(cfg.phase1_output_state);
          debug_print(" p2-out=");
          debug_print_uint(cfg.phase2_output_state);
          break;
        case TIMER_MODE_3_ASTABLE:
          debug_print(" on-ms=");
          debug_print_uint(cfg.on_duration_ms);
          debug_print(" off-ms=");
          debug_print_uint(cfg.off_duration_ms);
          debug_print(" p1-out=");
          debug_print_uint(cfg.phase1_output_state);
          debug_print(" p2-out=");
          debug_print_uint(cfg.phase2_output_state);
          break;
        case TIMER_MODE_4_INPUT_TRIGGERED:
          debug_print(" input-dis=");
          debug_print_uint(cfg.input_dis);
          debug_print(" trigger-edge=");
          debug_print_uint(cfg.trigger_edge);
          debug_print(" delay-ms=");
          debug_print_uint(cfg.delay_ms);
          debug_print(" out=");
          debug_print_uint(cfg.phase1_output_state);
          break;
        default:
          debug_print(" UNKNOWN");
      }

      debug_print(" output-coil=");
      debug_print_uint(cfg.output_coil);
      debug_print(" ctrl-reg=");
      debug_print_uint(cfg.ctrl_reg);
      debug_println("");
    }
  }
  if (!any_timer) {
    debug_println("  (none configured)");
  }
  debug_println("");

  // GPIO Mappings section
  debug_println("gpio");

  // GPIO2 status (heartbeat control)
  debug_print("  gpio2: ");
  if (g_persist_config.gpio2_user_mode == 0) {
    debug_println("heartbeat (led blink)");
  } else {
    debug_println("user mode");
  }
  debug_println("");

  debug_println("hardware (fixed):");
  debug_println("  GPIO 4  - UART1 RX (Modbus)");
  debug_println("  GPIO 5  - UART1 TX (Modbus)");
  debug_println("  GPIO 15 - RS485 DIR");
  debug_println("  GPIO 19 - Counter 1 HW (PCNT Unit 0)");
  debug_println("  GPIO 25 - Counter 2 HW (PCNT Unit 1)");
  debug_println("  GPIO 27 - Counter 3 HW (PCNT Unit 2)");
  debug_println("  GPIO 33 - Counter 4 HW (PCNT Unit 3)");
  debug_println("");

  // Separate GPIO and ST Logic mappings
  uint8_t gpio_count = 0;
  uint8_t st_count = 0;

  // Count each type
  for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
    const VariableMapping* map = &g_persist_config.var_maps[i];
    if (map->source_type == MAPPING_SOURCE_GPIO) {
      gpio_count++;
    } else if (map->source_type == MAPPING_SOURCE_ST_VAR) {
      st_count++;
    }
  }

  // Show GPIO mappings
  if (gpio_count > 0) {
    debug_print("mappings (");
    debug_print_uint(gpio_count);
    debug_println("):");
    for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
      const VariableMapping* map = &g_persist_config.var_maps[i];
      if (map->source_type != MAPPING_SOURCE_GPIO) continue;

      debug_print("  gpio");
      debug_print_uint(map->gpio_pin);
      debug_print(" -> ");
      if (map->is_input) {
        debug_print("input:");
        debug_print_uint(map->input_reg);
      } else {
        debug_print("coil:");
        debug_print_uint(map->coil_reg);
      }
      if (map->associated_counter != 0xff) {
        debug_print(" (counter");
        debug_print_uint(map->associated_counter);
        debug_print(")");
      }
      if (map->associated_timer != 0xff) {
        debug_print(" (timer");
        debug_print_uint(map->associated_timer);
        debug_print(")");
      }
      debug_println("");
    }
  } else {
    debug_println("mappings: (none)");
  }

  // Show ST Logic variable bindings
  if (st_count > 0) {
    debug_print("bindings (");
    debug_print_uint(st_count);
    debug_println("):");

    st_logic_engine_state_t *st_state = st_logic_get_state();

    for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
      const VariableMapping* map = &g_persist_config.var_maps[i];
      if (map->source_type != MAPPING_SOURCE_ST_VAR) continue;

      debug_print("  logic");
      debug_print_uint(map->st_program_id + 1);
      debug_print(".");

      // Lookup variable name from bytecode
      st_logic_program_config_t *prog = st_logic_get_program(st_state, map->st_program_id);
      if (prog && prog->compiled && map->st_var_index < prog->bytecode.var_count) {
        debug_print(prog->bytecode.var_names[map->st_var_index]);
      } else {
        debug_print("var");
        debug_print_uint(map->st_var_index);
      }

      debug_print(" ");

      if (map->is_input) {
        // INPUT mode: check if it's a Holding Register or Discrete Input
        debug_print("<- ");
        if (map->input_type == 1) {
          debug_print("input:");  // Discrete Input
        } else {
          debug_print("reg:");    // Holding Register input
        }
        debug_print_uint(map->input_reg);
      } else {
        // OUTPUT mode: check if it's a Coil or Holding Register
        debug_print("-> ");
        if (map->output_type == 1) {
          debug_print("coil:");  // Coil output
        } else {
          debug_print("reg:");   // Holding Register output
        }
        debug_print_uint(map->coil_reg);
      }
      debug_println("");
    }
  } else {
    debug_println("bindings: (none)");
  }
  debug_println("");

  // ST Logic section moved to later (after persistence) for better grouping
  st_logic_engine_state_t *st_state = st_logic_get_state();

  // =========================================================================
  // WIFI/NETWORK (v3.0+)
  // =========================================================================
  debug_println("wifi");
  debug_print("  status: ");
  debug_println(g_persist_config.network.enabled ? "enabled" : "disabled");

  debug_print("  ssid: ");
  debug_println(g_persist_config.network.ssid[0] ? g_persist_config.network.ssid : "(not set)");

  debug_print("  password: ");
  debug_println(g_persist_config.network.password[0] ? "********" : "(not set)");

  debug_print("  dhcp: ");
  debug_println(g_persist_config.network.dhcp_enabled ? "enabled" : "disabled");

  if (!g_persist_config.network.dhcp_enabled) {
    char ip_str[16];
    debug_print("  static ip: ");
    network_config_ip_to_str(g_persist_config.network.static_ip, ip_str);
    debug_println(ip_str);

    debug_print("  gateway: ");
    network_config_ip_to_str(g_persist_config.network.static_gateway, ip_str);
    debug_println(ip_str);

    debug_print("  netmask: ");
    network_config_ip_to_str(g_persist_config.network.static_netmask, ip_str);
    debug_println(ip_str);

    debug_print("  dns: ");
    network_config_ip_to_str(g_persist_config.network.static_dns, ip_str);
    debug_println(ip_str);
  }

  debug_println("telnet");
  debug_print("  status: ");
  debug_println(g_persist_config.network.telnet_enabled ? "enabled" : "disabled");

  debug_print("  port: ");
  debug_print_uint(g_persist_config.network.telnet_port);
  debug_println("");

  debug_print("  username: ");
  debug_println(g_persist_config.network.telnet_username[0] ? g_persist_config.network.telnet_username : "(not set)");

  debug_print("  password: ");
  debug_println(g_persist_config.network.telnet_password[0] ? "********" : "(not set)");

  // =========================================================================
  // PERSISTENCE (v4.0+)
  // =========================================================================
  debug_println("persistence");
  debug_print("  status: ");
  debug_println(g_persist_config.persist_regs.enabled ? "enabled" : "disabled");

  debug_print("  groups: ");
  debug_print_uint(g_persist_config.persist_regs.group_count);
  debug_print(" / ");
  debug_print_uint(PERSIST_MAX_GROUPS);
  debug_println("");

  if (g_persist_config.persist_regs.group_count > 0) {
    for (uint8_t i = 0; i < g_persist_config.persist_regs.group_count; i++) {
      PersistGroup* grp = &g_persist_config.persist_regs.groups[i];
      debug_print("  grp");
      debug_print_uint(i + 1);  // Group number (1-indexed for ST Logic)
      debug_print(" \"");
      debug_print(grp->name);
      debug_print("\" (");
      debug_print_uint(grp->reg_count);
      debug_print(" regs): ");

      // Show register addresses
      for (uint8_t j = 0; j < grp->reg_count; j++) {
        if (j > 0) debug_print(", ");
        debug_print_uint(grp->reg_addresses[j]);
      }
      debug_println("");
    }
  } else {
    debug_println("  (none configured)");
  }

  // =========================================================================
  // ST LOGIC (v3.0+)
  // =========================================================================
  // st_state already declared earlier in this function (line 397)

  debug_println("st-logic");
  debug_print("  status: ");
  debug_println(st_state->enabled ? "enabled" : "disabled");

  debug_print("  debug: ");
  debug_println(st_state->debug ? "enabled" : "disabled");

  debug_print("  interval: ");
  debug_print_uint(st_state->execution_interval_ms);
  debug_println(" ms");

  debug_print("  cycles: ");
  debug_print_uint(st_state->total_cycles);
  debug_println("");

  // Show individual programs (all 4 slots, even if empty)
  for (uint8_t i = 0; i < 4; i++) {
    st_logic_program_config_t* prog = &st_state->programs[i];

    debug_print("  logic");
    debug_print_uint(i + 1);
    debug_print(": ");

    if (prog->source_size == 0 && !prog->compiled) {
      // Empty slot - show (none configured) and binding count if any
      debug_print("(none configured)");
      if (prog->binding_count > 0) {
        debug_print(", register bindings=");
        debug_print_uint(prog->binding_count);
      }
    } else if (!prog->compiled) {
      debug_print("not compiled");
      if (prog->binding_count > 0) {
        debug_print(", register bindings=");
        debug_print_uint(prog->binding_count);
      }
    } else if (!prog->enabled) {
      // Disabled but compiled
      debug_print("disabled");
      if (prog->binding_count > 0) {
        debug_print(", register bindings=");
        debug_print_uint(prog->binding_count);
      }
    } else {
      // Enabled and compiled
      debug_print("enabled");
      debug_print(", register bindings=");
      debug_print_uint(prog->binding_count);
      debug_print(", exec=");
      debug_print_uint(prog->execution_count);

      if (prog->error_count > 0) {
        debug_print(", errors=");
        debug_print_uint(prog->error_count);
      }
    }
    debug_println("");
  }

  // =========================================================================
  // CONFIGURATION AS SET COMMANDS (copy/paste ready)
  // =========================================================================
  debug_println("\n=== CONFIGURATION AS SET COMMANDS ===\n");
  debug_println("# Modbus Slave");
  debug_print("set modbus-slave enabled ");
  debug_println(g_persist_config.modbus_slave.enabled ? "on" : "off");
  debug_print("set modbus-slave slave-id ");
  debug_print_uint(g_persist_config.modbus_slave.slave_id);
  debug_println("");
  debug_print("set modbus-slave baudrate ");
  debug_print_uint(g_persist_config.modbus_slave.baudrate);
  debug_println("");
  debug_print("set modbus-slave parity ");
  if (g_persist_config.modbus_slave.parity == 0) debug_println("none");
  else if (g_persist_config.modbus_slave.parity == 1) debug_println("even");
  else if (g_persist_config.modbus_slave.parity == 2) debug_println("odd");
  else debug_println("none");
  debug_print("set modbus-slave stop-bits ");
  debug_print_uint(g_persist_config.modbus_slave.stop_bits);
  debug_println("");
  debug_print("set modbus-slave inter-frame-delay ");
  debug_print_uint(g_persist_config.modbus_slave.inter_frame_delay);
  debug_println("");

  // Modbus Master
  debug_println("\n# Modbus Master");
  debug_print("set modbus-master enabled ");
  debug_println(g_persist_config.modbus_master.enabled ? "on" : "off");
  if (g_persist_config.modbus_master.enabled) {
    debug_print("set modbus-master baudrate ");
    debug_print_uint(g_persist_config.modbus_master.baudrate);
    debug_println("");
    debug_print("set modbus-master parity ");
    if (g_persist_config.modbus_master.parity == 0) debug_println("none");
    else if (g_persist_config.modbus_master.parity == 1) debug_println("even");
    else if (g_persist_config.modbus_master.parity == 2) debug_println("odd");
    else debug_println("none");
    debug_print("set modbus-master stop-bits ");
    debug_print_uint(g_persist_config.modbus_master.stop_bits);
    debug_println("");
    debug_print("set modbus-master timeout ");
    debug_print_uint(g_persist_config.modbus_master.timeout_ms);
    debug_println("");
    debug_print("set modbus-master inter-frame-delay ");
    debug_print_uint(g_persist_config.modbus_master.inter_frame_delay);
    debug_println("");
    debug_print("set modbus-master max-requests ");
    debug_print_uint(g_persist_config.modbus_master.max_requests_per_cycle);
    debug_println("");
  }

  // Hostname
  debug_println("\n# System");
  debug_print("set hostname ");
  debug_println(g_persist_config.hostname[0] ? g_persist_config.hostname : "modbus-esp32");
  debug_print("set echo ");
  debug_println(g_persist_config.remote_echo ? "on" : "off");
  debug_print("set gpio 2 ");
  debug_println(g_persist_config.gpio2_user_mode ? "enable" : "disable");

  // WiFi
  debug_println("\n# WiFi");
  debug_print("set wifi status ");
  debug_println(g_persist_config.network.enabled ? "enable" : "disable");
  if (g_persist_config.network.enabled) {
    if (g_persist_config.network.ssid[0]) {
      debug_print("set wifi ssid ");
      debug_println(g_persist_config.network.ssid);
    }
    if (g_persist_config.network.password[0]) {
      debug_println("set wifi password ********");
    }
    debug_print("set wifi dhcp ");
    debug_println(g_persist_config.network.dhcp_enabled ? "on" : "off");
    if (!g_persist_config.network.dhcp_enabled) {
      char ip_str[16];
      network_config_ip_to_str(g_persist_config.network.static_ip, ip_str);
      debug_print("set wifi ip ");
      debug_println(ip_str);
      network_config_ip_to_str(g_persist_config.network.static_gateway, ip_str);
      debug_print("set wifi gateway ");
      debug_println(ip_str);
      network_config_ip_to_str(g_persist_config.network.static_netmask, ip_str);
      debug_print("set wifi netmask ");
      debug_println(ip_str);
      network_config_ip_to_str(g_persist_config.network.static_dns, ip_str);
      debug_print("set wifi dns ");
      debug_println(ip_str);
    }
    debug_print("set wifi telnet ");
    debug_println(g_persist_config.network.telnet_enabled ? "enable" : "disable");
    if (g_persist_config.network.telnet_enabled) {
      if (g_persist_config.network.telnet_username[0]) {
        debug_print("set wifi telnet-user ");
        debug_println(g_persist_config.network.telnet_username);
      }
      if (g_persist_config.network.telnet_password[0]) {
        debug_println("set wifi telnet-pass ********");
      }
      debug_print("set wifi telnet-port ");
      debug_print_uint(g_persist_config.network.telnet_port);
      debug_println("");
    }
  }

  // ST Logic execution interval
  debug_println("\n# ST Logic");
  debug_print("set logic interval ");
  debug_print_uint(g_persist_config.st_logic_interval_ms);
  debug_println("");

  // Persistence
  if (g_persist_config.persist_regs.enabled && g_persist_config.persist_regs.group_count > 0) {
    debug_println("\n# Persistence");
    debug_println("set persist enable");
    for (uint8_t i = 0; i < g_persist_config.persist_regs.group_count; i++) {
      PersistGroup* grp = &g_persist_config.persist_regs.groups[i];
      debug_print("set persist group ");
      debug_print_uint(i + 1);
      debug_print(" name ");
      debug_print(grp->name);
      debug_print(" regs ");
      for (uint8_t j = 0; j < grp->reg_count; j++) {
        if (j > 0) debug_print(",");
        debug_print_uint(grp->reg_addresses[j]);
      }
      debug_println("");
    }
  }

  // Counters (enabled only)
  bool any_counter_enabled = false;
  for (uint8_t id = 1; id <= 4; id++) {
    CounterConfig cfg;
    if (counter_config_get(id, &cfg) && cfg.enabled) {
      if (!any_counter_enabled) {
        debug_println("\n# Counters");
        any_counter_enabled = true;
      }

      debug_print("set counter ");
      debug_print_uint(id);
      debug_print(" mode 1 parameter");

      // HW mode
      debug_print(" hw-mode:");
      if (cfg.hw_mode == COUNTER_HW_SW) debug_print("sw");
      else if (cfg.hw_mode == COUNTER_HW_SW_ISR) debug_print("sw-isr");
      else if (cfg.hw_mode == COUNTER_HW_PCNT) debug_print("hw");

      // Edge type
      debug_print(" edge:");
      if (cfg.edge_type == COUNTER_EDGE_RISING) debug_print("rising");
      else if (cfg.edge_type == COUNTER_EDGE_FALLING) debug_print("falling");
      else if (cfg.edge_type == COUNTER_EDGE_BOTH) debug_print("both");

      // Prescaler and resolution
      debug_print(" prescaler:");
      debug_print_uint(cfg.prescaler);
      debug_print(" resolution:");
      debug_print_uint(cfg.bit_width);

      // Direction
      debug_print(" direction:");
      debug_print(cfg.direction == COUNTER_DIR_DOWN ? "down" : "up");

      // Scale factor
      debug_print(" scale:");
      debug_print_float(cfg.scale_factor);

      // Start value
      debug_print(" start:");
      debug_print_uint((uint32_t)cfg.start_value);

      // Debounce
      debug_print(" debounce:");
      debug_print(cfg.debounce_enabled ? "on" : "off");
      if (cfg.debounce_enabled) {
        debug_print(" debounce-time:");
        debug_print_uint(cfg.debounce_ms);
      }

      // Input/Pin based on hw_mode
      if (cfg.hw_mode == COUNTER_HW_PCNT && cfg.hw_gpio > 0) {
        debug_print(" hw-gpio:");
        debug_print_uint(cfg.hw_gpio);
      } else if (cfg.hw_mode == COUNTER_HW_SW_ISR && cfg.interrupt_pin > 0) {
        debug_print(" interrupt-pin:");
        debug_print_uint(cfg.interrupt_pin);
      } else if (cfg.hw_mode == COUNTER_HW_SW && cfg.input_dis > 0) {
        debug_print(" input-dis:");
        debug_print_uint(cfg.input_dis);
      }

      // Compare feature
      if (cfg.compare_enabled) {
        debug_print(" compare:enable");
        debug_print(" compare-mode:");
        debug_print_uint(cfg.compare_mode);
        debug_print(" compare-source:");
        debug_print_uint(cfg.compare_source);
        debug_print(" compare-value:");
        debug_print_uint((uint32_t)cfg.compare_value);
        debug_print(" reset-on-read:");
        debug_print(cfg.reset_on_read ? "on" : "off");
      }

      debug_println("");
    }
  }

  // Timers (enabled only)
  bool any_timer_enabled = false;
  for (uint8_t id = 1; id <= 4; id++) {
    TimerConfig cfg;
    if (timer_engine_get_config(id, &cfg) && cfg.enabled) {
      if (!any_timer_enabled) {
        debug_println("\n# Timers");
        any_timer_enabled = true;
      }

      debug_print("set timer ");
      debug_print_uint(id);
      debug_print(" mode ");
      debug_print_uint(cfg.mode);
      debug_print(" parameter");

      // Mode-specific parameters
      switch (cfg.mode) {
        case TIMER_MODE_1_ONESHOT:
          debug_print(" p1-duration:");
          debug_print_uint(cfg.phase1_duration_ms);
          debug_print(" p1-output:");
          debug_print_uint(cfg.phase1_output_state);
          debug_print(" p2-duration:");
          debug_print_uint(cfg.phase2_duration_ms);
          debug_print(" p2-output:");
          debug_print_uint(cfg.phase2_output_state);
          debug_print(" p3-duration:");
          debug_print_uint(cfg.phase3_duration_ms);
          debug_print(" p3-output:");
          debug_print_uint(cfg.phase3_output_state);
          break;

        case TIMER_MODE_2_MONOSTABLE:
          debug_print(" pulse-ms:");
          debug_print_uint(cfg.pulse_duration_ms);
          debug_print(" p1-output:");
          debug_print_uint(cfg.phase1_output_state);
          debug_print(" p2-output:");
          debug_print_uint(cfg.phase2_output_state);
          break;

        case TIMER_MODE_3_ASTABLE:
          debug_print(" on-ms:");
          debug_print_uint(cfg.on_duration_ms);
          debug_print(" off-ms:");
          debug_print_uint(cfg.off_duration_ms);
          debug_print(" p1-output:");
          debug_print_uint(cfg.phase1_output_state);
          debug_print(" p2-output:");
          debug_print_uint(cfg.phase2_output_state);
          break;

        case TIMER_MODE_4_INPUT_TRIGGERED:
          debug_print(" input-dis:");
          debug_print_uint(cfg.input_dis);
          debug_print(" trigger-edge:");
          debug_print_uint(cfg.trigger_edge);
          debug_print(" delay-ms:");
          debug_print_uint(cfg.delay_ms);
          debug_print(" output:");
          debug_print_uint(cfg.phase1_output_state);
          break;
      }

      // Output coil
      debug_print(" output-coil:");
      debug_print_uint(cfg.output_coil);

      // Control register
      debug_print(" ctrl-reg:");
      debug_print_uint(cfg.ctrl_reg);

      debug_println("");
    }
  }

  // GPIO mappings (only GPIO source type, not ST Logic bindings)
  bool any_gpio_mapping = false;
  for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
    const VariableMapping* map = &g_persist_config.var_maps[i];
    if (map->source_type != MAPPING_SOURCE_GPIO) continue;

    // Skip counter/timer associated pins (they are configured via counter/timer commands)
    if (map->associated_counter != 0xff || map->associated_timer != 0xff) continue;

    if (!any_gpio_mapping) {
      debug_println("\n# GPIO Mappings");
      any_gpio_mapping = true;
    }

    debug_print("set gpio ");
    debug_print_uint(map->gpio_pin);

    if (map->is_input) {
      debug_print(" input ");
      debug_print_uint(map->input_reg);
    } else {
      debug_print(" coil ");
      debug_print_uint(map->coil_reg);
    }

    debug_println("");
  }

  debug_println("\n# Note: Remember to run 'save' after making changes!");
  debug_println("");
}

/* ============================================================================
 * SHOW COUNTERS
 * ============================================================================ */

void cli_cmd_show_counters(void) {
  // Header med forkortelser (3 linjer)
  debug_println("───────────────────────────────────────────────────────────────────---------------─────────────────────────────────────────────────────────────────────────────────────────────────────────────");
  debug_println("co = count-on, sv = startValue, res = resolution, ps = prescaler, ir = index-reg, rr = raw-reg, fr = freq-reg");
  debug_println("or = overload-reg, cr = ctrl-reg, dir = direction, sf = scaleFloat, d = debounce, dt = debounce-ms");
  debug_println("hw = HW/SW mode (SW|ISR|HW), hz = measured freq (Hz), value = scaled value, raw = prescaled counter value");
  debug_println("cmp-en = compare enabled, cmp-mode = 0:≥ 1:> 2:exact, cmp-src = 0:raw 1:prescaled 2:scaled, cmp-val = threshold, ror = reset-on-read");
  debug_println("─────────────────────────────────────────────────────────────---------------───────────────────────────────────────────────────────────────────────────────────────────────────────────────────");

  // Kolonne headers
  debug_println("counter |  en  | hw  | pin  |    co   |    sv    | res |  ps  |  ir  |  rr  |  fr  |   or |  cr  |  dir  |   sf   | d   |  dt  |  hz   |     val     |    raw    | cmp-en | cmp-md | cmp-src | cmp-val | ror");

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

    // BUG-019 FIX: Atomic counter value read to avoid race condition
    // Get counter value ONCE and calculate scaled/raw from same value
    uint64_t counter_value = counter_engine_get_value(id);

    // Calculate bit-width max value ONCE for clamping
    uint64_t max_val = 0;
    switch (cfg.bit_width) {
      case 8:  max_val = 0xFFULL; break;
      case 16: max_val = 0xFFFFULL; break;
      case 32: max_val = 0xFFFFFFFFULL; break;
      case 64:
      default: max_val = 0xFFFFFFFFFFFFFFFFULL; break;
    }

    // Clamp counter_value to bit-width FIRST to avoid overflow in scaling
    uint64_t clamped_value = counter_value & max_val;

    // Scaled value (10 chars right-aligned for 32-bit max: 4294967295)
    // CORRECT: scaled = counter × scale (not raw × scale!)
    double scaled_float = (double)clamped_value * cfg.scale_factor;

    // Clamp scaled result to bit-width range
    if (scaled_float < 0.0) scaled_float = 0.0;
    if (scaled_float > (double)max_val) scaled_float = (double)max_val;

    uint64_t scaled_value = (uint64_t)(scaled_float + 0.5);  // Round to nearest
    scaled_value &= max_val;

    p += snprintf(p, sizeof(line) - (p - line), "%10llu ", (unsigned long long)scaled_value);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // Raw value (prescaled, 9 chars right-aligned for 32-bit max: 268435455)
    // CORRECT: raw = counter / prescaler (same clamped_value!)
    uint64_t raw_prescaled = clamped_value / cfg.prescaler;
    raw_prescaled &= max_val;

    p += snprintf(p, sizeof(line) - (p - line), "%9llu ", (unsigned long long)raw_prescaled);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // COMPARE FEATURE COLUMNS
    // cmp-en (compare enabled, 3 chars left-aligned)
    p += snprintf(p, sizeof(line) - (p - line), "%-6s ", cfg.compare_enabled ? "yes" : "no");
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // cmp-md (compare mode: 0=≥, 1=>, 2=exact, 1 char)
    if (cfg.compare_enabled) {
      const char* cmp_mode_char = "?";
      if (cfg.compare_mode == 0) cmp_mode_char = "≥";
      else if (cfg.compare_mode == 1) cmp_mode_char = ">";
      else if (cfg.compare_mode == 2) cmp_mode_char = "=";
      p += snprintf(p, sizeof(line) - (p - line), "%-8s ", cmp_mode_char);
    } else {
      p += snprintf(p, sizeof(line) - (p - line), "%-8s ", "—");
    }
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // cmp-src (compare source: 0=raw, 1=prescaled, 2=scaled)
    if (cfg.compare_enabled) {
      const char* cmp_src_str = "?";
      if (cfg.compare_source == 0) cmp_src_str = "raw";
      else if (cfg.compare_source == 1) cmp_src_str = "presc";
      else if (cfg.compare_source == 2) cmp_src_str = "scale";
      p += snprintf(p, sizeof(line) - (p - line), "%-8s ", cmp_src_str);
    } else {
      p += snprintf(p, sizeof(line) - (p - line), "%-8s ", "—");
    }
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // cmp-val (compare value, 8 chars right-aligned)
    if (cfg.compare_enabled) {
      p += snprintf(p, sizeof(line) - (p - line), "%8llu ", (unsigned long long)cfg.compare_value);
    } else {
      p += snprintf(p, sizeof(line) - (p - line), "%-9s ", "—");
    }
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // ror (reset-on-read, 3 chars left-aligned)
    if (cfg.compare_enabled) {
      p += snprintf(p, sizeof(line) - (p - line), "%-3s", cfg.reset_on_read ? "yes" : "no");
    } else {
      p += snprintf(p, sizeof(line) - (p - line), "%-3s", "—");
    }

    // Print hele linjen
    debug_println(line);
  }

  // COUNTER CONTROL STATUS (samme stil som counter status)
  debug_println("");
  debug_println("Control register status (bit0=reset, bit1=start, bit2=stop, bit4=compare-match)");
  debug_println("─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────");
  debug_println("counter | ctrl-reg | raw-value | reset | start | stop | running | compare-match");

  for (uint8_t id = 1; id <= 4; id++) {
    CounterConfig cfg;
    if (!counter_config_get(id, &cfg) || !cfg.enabled) continue;

    char line[256];
    char* p = line;

    // Counter ID (8 chars left-aligned)
    p += snprintf(p, sizeof(line) - (p - line), " %-7d", id);
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    // ctrl-reg address (10 chars)
    if (cfg.ctrl_reg < HOLDING_REGS_SIZE) {
      p += snprintf(p, sizeof(line) - (p - line), "%-9u ", cfg.ctrl_reg);
    } else {
      p += snprintf(p, sizeof(line) - (p - line), "%-10s ", "n/a");
    }
    p += snprintf(p, sizeof(line) - (p - line), "| ");

    if (cfg.ctrl_reg < HOLDING_REGS_SIZE) {
      uint16_t ctrl_value = registers_get_holding_register(cfg.ctrl_reg);

      // Raw value (hex, 10 chars)
      p += snprintf(p, sizeof(line) - (p - line), "0x%-7x ", ctrl_value);
      p += snprintf(p, sizeof(line) - (p - line), "| ");

      // Bit 0: reset (7 chars)
      uint8_t bit0 = (ctrl_value >> 0) & 1;
      p += snprintf(p, sizeof(line) - (p - line), "%-6s ", bit0 ? "yes" : "no");
      p += snprintf(p, sizeof(line) - (p - line), "| ");

      // Bit 1: start (7 chars)
      uint8_t bit1 = (ctrl_value >> 1) & 1;
      p += snprintf(p, sizeof(line) - (p - line), "%-6s ", bit1 ? "yes" : "no");
      p += snprintf(p, sizeof(line) - (p - line), "| ");

      // Bit 2: stop (6 chars)
      uint8_t bit2 = (ctrl_value >> 2) & 1;
      p += snprintf(p, sizeof(line) - (p - line), "%-5s ", bit2 ? "yes" : "no");
      p += snprintf(p, sizeof(line) - (p - line), "| ");

      // Running status (derived from start/stop, 9 chars)
      const char* running_status = "—";
      if (bit1 && !bit2) running_status = "yes";
      else if (!bit1 && bit2) running_status = "no";
      else if (bit1 && bit2) running_status = "conflict";
      p += snprintf(p, sizeof(line) - (p - line), "%-8s ", running_status);
      p += snprintf(p, sizeof(line) - (p - line), "| ");

      // Bit 4: compare-match (15 chars)
      uint8_t bit4 = (ctrl_value >> 4) & 1;
      p += snprintf(p, sizeof(line) - (p - line), "%-14s", bit4 ? "yes" : "no");
    } else {
      p += snprintf(p, sizeof(line) - (p - line), "%-10s | %-6s | %-6s | %-5s | %-8s | %-14s",
                   "—", "—", "—", "—", "—", "—");
    }

    debug_println(line);
  }
}

/* ============================================================================
 * SHOW COUNTER (SPECIFIC ID)
 * ============================================================================ */

void cli_cmd_show_counter(uint8_t id) {
  if (id < 1 || id > 4) {
    debug_printf("SHOW COUNTER: invalid ID %d (expected 1-4)\n", id);
    return;
  }

  CounterConfig cfg;
  if (!counter_config_get(id, &cfg)) {
    debug_printf("SHOW COUNTER: could not get config for counter %d\n", id);
    return;
  }

  debug_println("");
  debug_print("=== COUNTER ");
  debug_print_uint(id);
  debug_println(" ===\n");

  if (!cfg.enabled) {
    debug_println("Status: DISABLED\n");
    return;
  }

  debug_println("Status: ENABLED");
  debug_println("");

  // Hardware mode
  const char* hw_str = "unknown";
  if (cfg.hw_mode == COUNTER_HW_SW) hw_str = "SW (polling)";
  else if (cfg.hw_mode == COUNTER_HW_SW_ISR) hw_str = "SW-ISR (interrupt)";
  else if (cfg.hw_mode == COUNTER_HW_PCNT) hw_str = "HW (PCNT)";
  debug_print("Hardware Mode: ");
  debug_println(hw_str);

  // Edge type
  const char* edge_str = "rising";
  if (cfg.edge_type == COUNTER_EDGE_FALLING) edge_str = "falling";
  else if (cfg.edge_type == COUNTER_EDGE_BOTH) edge_str = "both";
  debug_print("Edge Type: ");
  debug_println(edge_str);

  // Prescaler
  debug_print("Prescaler: ");
  debug_print_uint(cfg.prescaler);
  debug_println("");

  // Scale factor
  debug_print("Scale Factor: ");
  debug_print_float(cfg.scale_factor);
  debug_println("");

  // Bit width
  debug_print("Bit Width: ");
  debug_print_uint(cfg.bit_width);
  debug_println("");

  // Direction
  const char* dir_str = (cfg.direction == COUNTER_DIR_DOWN) ? "down" : "up";
  debug_print("Direction: ");
  debug_println(dir_str);

  // Debounce
  debug_print("Debounce: ");
  if (cfg.debounce_enabled && cfg.debounce_ms > 0) {
    debug_print("ON (");
    debug_print_uint(cfg.debounce_ms);
    debug_println("ms)");
  } else {
    debug_println("OFF");
  }

  debug_println("");

  // Register mappings
  debug_println("Register Mappings:");
  debug_print("  Index Register (scaled value): ");
  debug_print_uint(cfg.index_reg);
  debug_println("");

  if (cfg.raw_reg > 0) {
    debug_print("  Raw Register (prescaled): ");
    debug_print_uint(cfg.raw_reg);
    debug_println("");
  }

  if (cfg.freq_reg > 0) {
    debug_print("  Frequency Register (Hz): ");
    debug_print_uint(cfg.freq_reg);
    debug_println("");
  }

  if (cfg.ctrl_reg < 1000) {
    debug_print("  Control Register: ");
    debug_print_uint(cfg.ctrl_reg);
    debug_println("");
  }

  if (cfg.overload_reg < 1000) {
    debug_print("  Overload Register: ");
    debug_print_uint(cfg.overload_reg);
    debug_println("");
  }

  debug_println("");

  // Input/Pin configuration
  debug_println("Input Configuration:");
  if (cfg.hw_mode == COUNTER_HW_SW && cfg.input_dis > 0) {
    debug_print("  Discrete Input: ");
    debug_print_uint(cfg.input_dis);
    debug_println("");
  } else if (cfg.hw_mode == COUNTER_HW_SW_ISR && cfg.interrupt_pin > 0) {
    debug_print("  Interrupt GPIO: ");
    debug_print_uint(cfg.interrupt_pin);
    debug_println("");
  } else if (cfg.hw_mode == COUNTER_HW_PCNT && cfg.hw_gpio > 0) {
    debug_print("  PCNT GPIO: ");
    debug_print_uint(cfg.hw_gpio);
    debug_println("");
  }

  debug_println("");

  // Current values
  debug_println("Current Values:");
  uint64_t raw_value = counter_engine_get_value(id);
  uint64_t scaled_value = (uint64_t)(raw_value * cfg.scale_factor);
  uint64_t raw_prescaled = raw_value / cfg.prescaler;
  uint32_t freq = counter_frequency_get(id);

  debug_print("  Raw Value: ");
  debug_print_uint((unsigned int)raw_value);
  debug_println("");

  debug_print("  Prescaled Value: ");
  debug_print_uint((unsigned int)raw_prescaled);
  debug_println("");

  debug_print("  Scaled Value: ");
  debug_print_uint((unsigned int)scaled_value);
  debug_println("");

  debug_print("  Frequency: ");
  debug_print_uint(freq);
  debug_println(" Hz");

  // Compare feature
  if (cfg.compare_enabled) {
    debug_println("");
    debug_println("Compare Feature: ENABLED");
    debug_print("  Threshold: ");
    debug_print_uint((unsigned int)cfg.compare_value);
    debug_println("");

    const char* cmp_mode = "unknown";
    if (cfg.compare_mode == 0) cmp_mode = ">= (greater or equal)";
    else if (cfg.compare_mode == 1) cmp_mode = "> (greater than)";
    else if (cfg.compare_mode == 2) cmp_mode = "= (exact match)";
    debug_print("  Mode: ");
    debug_println(cmp_mode);

    debug_print("  Reset on Read: ");
    debug_println(cfg.reset_on_read ? "ON" : "OFF");
  }

  debug_println("");
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
        debug_print("  Discrete Input: ");
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
    debug_print("  Control Register: ");
    debug_print_uint(cfg.ctrl_reg);
    debug_println("");
    debug_println("");
  }
}

/* ============================================================================
 * SHOW TIMER (SPECIFIC ID)
 * ============================================================================ */

void cli_cmd_show_timer(uint8_t id) {
  if (id < 1 || id > 4) {
    debug_printf("SHOW TIMER: invalid ID %d (expected 1-4)\n", id);
    return;
  }

  TimerConfig cfg;
  if (!timer_engine_get_config(id, &cfg)) {
    debug_printf("SHOW TIMER: could not get config for timer %d\n", id);
    return;
  }

  debug_println("");
  debug_print("=== TIMER ");
  debug_print_uint(id);
  debug_println(" ===\n");

  if (!cfg.enabled) {
    debug_println("Status: DISABLED\n");
    return;
  }

  debug_println("Status: ENABLED");
  debug_println("");

  // Show mode and parameters
  switch (cfg.mode) {
    case TIMER_MODE_1_ONESHOT:
      debug_println("Mode: ONE-SHOT (Mode 1 - 3-phase sequence)");
      debug_println("");
      debug_println("Phase 1:");
      debug_print("  Duration: ");
      debug_print_uint(cfg.phase1_duration_ms);
      debug_println("ms");
      debug_print("  Output State: ");
      debug_print_uint(cfg.phase1_output_state);
      debug_println("");

      debug_println("Phase 2:");
      debug_print("  Duration: ");
      debug_print_uint(cfg.phase2_duration_ms);
      debug_println("ms");
      debug_print("  Output State: ");
      debug_print_uint(cfg.phase2_output_state);
      debug_println("");

      debug_println("Phase 3:");
      debug_print("  Duration: ");
      debug_print_uint(cfg.phase3_duration_ms);
      debug_println("ms");
      debug_print("  Output State: ");
      debug_print_uint(cfg.phase3_output_state);
      debug_println("");
      break;

    case TIMER_MODE_2_MONOSTABLE:
      debug_println("Mode: MONOSTABLE (Mode 2 - Retriggerable pulse)");
      debug_println("");
      debug_print("Pulse Duration: ");
      debug_print_uint(cfg.pulse_duration_ms);
      debug_println("ms");
      debug_print("Rest Output State: ");
      debug_print_uint(cfg.phase1_output_state);
      debug_println("");
      debug_print("Pulse Output State: ");
      debug_print_uint(cfg.phase2_output_state);
      debug_println("");
      break;

    case TIMER_MODE_3_ASTABLE:
      debug_println("Mode: ASTABLE (Mode 3 - Oscillator/Blink)");
      debug_println("");
      debug_print("ON Duration: ");
      debug_print_uint(cfg.on_duration_ms);
      debug_println("ms");
      debug_print("OFF Duration: ");
      debug_print_uint(cfg.off_duration_ms);
      debug_println("ms");
      debug_print("ON Output State: ");
      debug_print_uint(cfg.phase1_output_state);
      debug_println("");
      debug_print("OFF Output State: ");
      debug_print_uint(cfg.phase2_output_state);
      debug_println("");
      break;

    case TIMER_MODE_4_INPUT_TRIGGERED:
      debug_println("Mode: INPUT-TRIGGERED (Mode 4 - Edge detection)");
      debug_println("");
      debug_print("Discrete Input: ");
      debug_print_uint(cfg.input_dis);
      debug_println("");
      debug_print("Trigger Edge: ");
      debug_println(cfg.trigger_edge == 1 ? "RISING (0→1)" : "FALLING (1→0)");
      debug_print("Delay: ");
      debug_print_uint(cfg.delay_ms);
      debug_println("ms");
      debug_print("Output Level: ");
      debug_print_uint(cfg.phase1_output_state);
      debug_println("");
      break;

    default:
      debug_println("Mode: UNKNOWN");
      break;
  }

  debug_println("");
  debug_print("Output Coil: ");
  debug_print_uint(cfg.output_coil);
  debug_println("");

  debug_print("Control Register: ");
  debug_print_uint(cfg.ctrl_reg);
  debug_println("");

  debug_println("");
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
 * SHOW ST LOGIC STATS (from Modbus Input Registers 252-293)
 * ============================================================================ */

void cli_cmd_show_st_logic_stats_modbus(void) {
  debug_println("\n=== ST Logic Performance Statistics (Modbus IR 252-293) ===\n");

  uint16_t *input_regs = registers_get_input_regs();

  // Helper to read 32-bit value from 2x input registers
  auto read_32bit = [&](uint16_t addr) -> uint32_t {
    uint32_t high = input_regs[addr];
    uint32_t low = input_regs[addr + 1];
    return (high << 16) | low;
  };

  debug_println("Per-Program Min Execution Time (µs):");
  for (uint8_t i = 0; i < 4; i++) {
    uint32_t min_us = read_32bit(252 + (i * 2));
    debug_printf("  Logic%d: %u µs (%.3f ms)\n", i + 1, (unsigned int)min_us, min_us / 1000.0);
  }

  debug_println("\nPer-Program Max Execution Time (µs):");
  for (uint8_t i = 0; i < 4; i++) {
    uint32_t max_us = read_32bit(260 + (i * 2));
    debug_printf("  Logic%d: %u µs (%.3f ms)\n", i + 1, (unsigned int)max_us, max_us / 1000.0);
  }

  debug_println("\nPer-Program Avg Execution Time (µs):");
  for (uint8_t i = 0; i < 4; i++) {
    uint32_t avg_us = read_32bit(268 + (i * 2));
    debug_printf("  Logic%d: %u µs (%.3f ms)\n", i + 1, (unsigned int)avg_us, avg_us / 1000.0);
  }

  debug_println("\nPer-Program Overrun Count:");
  for (uint8_t i = 0; i < 4; i++) {
    uint32_t overruns = read_32bit(276 + (i * 2));
    debug_printf("  Logic%d: %u\n", i + 1, (unsigned int)overruns);
  }

  debug_println("\nGlobal Cycle Statistics:");
  uint32_t cycle_min = read_32bit(284);
  uint32_t cycle_max = read_32bit(286);
  uint32_t cycle_overruns = read_32bit(288);
  uint32_t total_cycles = read_32bit(290);
  uint32_t interval = read_32bit(292);

  debug_printf("  Cycle Min: %u ms\n", (unsigned int)cycle_min);
  debug_printf("  Cycle Max: %u ms\n", (unsigned int)cycle_max);
  debug_printf("  Cycle Overruns: %u", (unsigned int)cycle_overruns);
  if (total_cycles > 0) {
    float overrun_pct = (float)cycle_overruns / total_cycles * 100.0f;
    debug_printf(" (%.1f%%)", overrun_pct);
  }
  debug_println("");
  debug_printf("  Total Cycles: %u\n", (unsigned int)total_cycles);
  debug_printf("  Execution Interval: %u ms\n\n", (unsigned int)interval);
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

  // Show user-configured GPIO mappings (exclude ST Logic bindings)
  uint8_t gpio_count = 0;
  for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
    const VariableMapping* map = &g_persist_config.var_maps[i];
    if (map->source_type == MAPPING_SOURCE_GPIO) {
      gpio_count++;
    }
  }

  if (gpio_count > 0) {
    debug_print("User configured GPIO (");
    debug_print_uint(gpio_count);
    debug_println(" mappings):");
    for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
      const VariableMapping* map = &g_persist_config.var_maps[i];
      if (map->source_type != MAPPING_SOURCE_GPIO) continue;

      debug_print("  GPIO ");
      debug_print_uint(map->gpio_pin);
      debug_print(" - ");
      if (map->is_input) {
        debug_print("INPUT:");
        debug_print_uint(map->input_reg);
      } else {
        debug_print("COIL:");
        debug_print_uint(map->coil_reg);
      }
      debug_print("  [fjern: 'no set gpio ");
      debug_print_uint(map->gpio_pin);
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
 * SHOW WIFI
 * ============================================================================ */

void cli_cmd_show_wifi(void) {
  debug_println("\n=== WI-FI STATUS ===");

  // Get network state from network manager
  if (!network_manager_is_wifi_connected()) {
    debug_println("Wi-Fi Status: NOT CONNECTED");
    debug_println("(Connect with: connect wifi)");
  } else {
    debug_println("Wi-Fi Status: CONNECTED");

    // Show actual IP address from WiFi driver (works for both DHCP and static)
    uint32_t ip = network_manager_get_local_ip();
    if (ip != 0) {
      char ip_str[16];
      network_config_ip_to_str(ip, ip_str);
      debug_print("Local IP: ");
      debug_println(ip_str);
    } else {
      debug_println("Local IP: Acquiring from DHCP...");
    }
  }

  // Show config
  debug_println("");
  debug_println("Configuration:");

  if (g_persist_config.network.enabled) {
    debug_println("  Enabled: YES");
  } else {
    debug_println("  Enabled: NO");
  }

  debug_print("  SSID: ");
  debug_println(g_persist_config.network.ssid[0] ? g_persist_config.network.ssid : "(not set)");

  if (g_persist_config.network.dhcp_enabled) {
    debug_println("  IP Mode: DHCP");
  } else {
    debug_println("  IP Mode: STATIC");
    char ip_str[16];
    network_config_ip_to_str(g_persist_config.network.static_ip, ip_str);
    debug_print("  Static IP: ");
    debug_println(ip_str);

    network_config_ip_to_str(g_persist_config.network.static_gateway, ip_str);
    debug_print("  Gateway: ");
    debug_println(ip_str);

    network_config_ip_to_str(g_persist_config.network.static_netmask, ip_str);
    debug_print("  Netmask: ");
    debug_println(ip_str);

    network_config_ip_to_str(g_persist_config.network.static_dns, ip_str);
    debug_print("  DNS: ");
    debug_println(ip_str);
  }

  // Show Telnet status
  if (g_persist_config.network.telnet_enabled) {
    debug_print("  Telnet: ENABLED (port ");
    debug_print_uint(g_persist_config.network.telnet_port);
    debug_print(", user: ");
    debug_print(g_persist_config.network.telnet_username);
    debug_print(")");
    if (!network_manager_is_wifi_connected()) {
      debug_println(" - waiting for WiFi connection...");
    } else {
      debug_println("");
    }
  } else {
    debug_println("  Telnet: DISABLED");
  }

  debug_println("\nCommands:");
  debug_println("  set wifi ssid <name>");
  debug_println("  set wifi password <pwd>");
  debug_println("  set wifi dhcp on|off");
  debug_println("  set wifi ip <ip>");
  debug_println("  set wifi gateway <ip>");
  debug_println("  set wifi netmask <ip>");
  debug_println("  set wifi dns <ip>");
  debug_println("  set wifi telnet enable|disable");
  debug_println("  set wifi telnet-user <username>");
  debug_println("  set wifi telnet-pass <password>");
  debug_println("  set wifi telnet-port <port>");
  debug_println("  connect wifi");
  debug_println("  disconnect wifi");
  debug_println("  save (to persist changes)\n");
}

/* ============================================================================
 * READ REG
 * ============================================================================ */

void cli_cmd_read_reg(uint8_t argc, char* argv[]) {
  // read reg <id> [antal]  (antal er optional, default = 1)
  if (argc < 1) {
    debug_println("READ REG: manglende parametre");
    debug_println("  Brug: read reg <id> [antal]");
    debug_println("  Eksempel: read reg 90     (l\u00e6ser 1 register)");
    debug_println("  Eksempel: read reg 90 10  (l\u00e6ser 10 registre)");
    return;
  }

  uint16_t start_addr = atoi(argv[0]);
  uint16_t count = (argc >= 2) ? atoi(argv[1]) : 1;  // Default: 1 register

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

  // Handle reset-on-read for counter compare status (same as Modbus FC03)
  // Must be called AFTER reading registers so user sees current value
  extern void modbus_handle_reset_on_read(uint16_t starting_address, uint16_t quantity);
  modbus_handle_reset_on_read(start_addr, count);
}

/* ============================================================================
 * READ INPUT REGISTERS (from Modbus IR, including stats IR 252-293)
 * ============================================================================ */

void cli_cmd_read_input_reg(uint8_t argc, char* argv[]) {
  // read input-reg <start> [count] - Read Input Registers (0-1023)
  if (argc < 1) {
    debug_println("READ INPUT-REG: manglende parametre");
    debug_println("  Brug: read input-reg <start> [count]");
    debug_println("  Eksempel: read input-reg 252     (l\u00e6ser 1 register)");
    debug_println("  Eksempel: read input-reg 252 8   (ST Logic stats)");
    return;
  }

  uint16_t start_addr = atoi(argv[0]);
  uint16_t count = (argc >= 2) ? atoi(argv[1]) : 1;  // Default: 1 register
  uint16_t *input_regs = registers_get_input_regs();

  // Validate parameters (input registers can be larger than holding regs)
  if (start_addr >= 1024) {  // Modbus limit
    debug_println("READ INPUT-REG: startadresse udenfor område (max 1023)");
    return;
  }

  if (count == 0) {
    debug_println("READ INPUT-REG: antal skal være større end 0");
    return;
  }

  // Adjust count if it exceeds available input registers
  if (start_addr + count > 1024) {
    count = 1024 - start_addr;
    debug_print("READ INPUT-REG: justeret antal til ");
    debug_print_uint(count);
    debug_println(" registre");
  }

  // Read and display input registers
  debug_println("\n=== LÆSNING AF INPUT REGISTERS ===");
  debug_print("Adresse ");
  debug_print_uint(start_addr);
  debug_print(" til ");
  debug_print_uint(start_addr + count - 1);
  debug_println(":\n");

  for (uint16_t i = 0; i < count; i++) {
    uint16_t addr = start_addr + i;
    uint16_t value = input_regs[addr];

    debug_print("IR[");
    debug_print_uint(addr);
    debug_print("]: ");
    debug_print_uint(value);

    // Special info for ST Logic stats registers
    if (addr >= 252 && addr <= 293) {
      debug_print(" [ST Logic stats]");
    }

    debug_println("");
  }
  debug_println("");
}

/* ============================================================================
 * READ COIL
 * ============================================================================ */

void cli_cmd_read_coil(uint8_t argc, char* argv[]) {
  // read coil <id> [antal]
  if (argc < 1) {
    debug_println("READ COIL: manglende parametre");
    debug_println("  Brug: read coil <id> [antal]");
    debug_println("  Eksempel: read coil 0      (l\u00e6ser 1 coil)");
    debug_println("  Eksempel: read coil 0 16   (l\u00e6ser 16 coils)");
    return;
  }

  uint16_t start_addr = atoi(argv[0]);
  uint16_t count = (argc >= 2) ? atoi(argv[1]) : 1;  // Default: 1 coil

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
  // read input <id> [antal]
  if (argc < 1) {
    debug_println("READ INPUT: manglende parametre");
    debug_println("  Brug: read input <id> [antal]");
    debug_println("  Eksempel: read input 0      (l\u00e6ser 1 input)");
    debug_println("  Eksempel: read input 0 16   (l\u00e6ser 16 inputs)");
    return;
  }

  uint16_t start_addr = atoi(argv[0]);
  uint16_t count = (argc >= 2) ? atoi(argv[1]) : 1;  // Default: 1 input

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

/* ============================================================================
 * SHOW DEBUG FLAGS
 * ============================================================================ */

void cli_cmd_show_debug(void) {
  debug_println("");
  debug_println("=== DEBUG FLAGS ===");
  debug_println("");

  DebugFlags* dbg = debug_flags_get();

  debug_print("  config_save:       ");
  debug_println(dbg->config_save ? "ENABLED" : "DISABLED");

  debug_print("  config_load:       ");
  debug_println(dbg->config_load ? "ENABLED" : "DISABLED");

  debug_print("  wifi_connect:      ");
  debug_println(dbg->wifi_connect ? "ENABLED" : "DISABLED");

  debug_print("  network_validate:  ");
  debug_println(dbg->network_validate ? "ENABLED" : "DISABLED");

  debug_println("");
  debug_println("Use 'set debug <flag> <on|off>' to toggle debug flags");
  debug_println("");
}

/* ============================================================================
 * SHOW PERSIST (v4.0+)
 * ============================================================================ */

void cli_cmd_show_persist(void) {
  // Simply call the persistence system's built-in list function
  registers_persist_list_groups();
}

void cli_cmd_show_watchdog(void) {
  WatchdogState* wdt = watchdog_get_state();

  debug_println("");
  debug_println("=== Watchdog Monitor (v4.0+) ===");
  debug_print("Status: ");
  debug_println(wdt->enabled ? "ENABLED" : "DISABLED");
  debug_print("Timeout: ");
  debug_print_uint(wdt->timeout_ms / 1000);
  debug_println(" seconds");
  debug_print("Reboot counter: ");
  debug_print_uint(wdt->reboot_counter);
  debug_println("");
  debug_print("Current uptime: ");
  debug_print_uint(millis() / 1000);
  debug_println(" seconds");

  // Last reset reason
  debug_print("Last reset reason: ");
  const char* reason_str = "Unknown";
  switch (wdt->last_reset_reason) {
    case 0:  reason_str = "Unknown"; break;
    case 1:  reason_str = "Power-on"; break;
    case 2:  reason_str = "External reset"; break;
    case 3:  reason_str = "Software reset"; break;
    case 4:  reason_str = "Panic/Exception"; break;
    case 5:  reason_str = "Interrupt watchdog"; break;
    case 6:  reason_str = "Task watchdog"; break;
    case 7:  reason_str = "Other watchdog"; break;
    case 8:  reason_str = "Deep sleep"; break;
    case 9:  reason_str = "Brownout"; break;
    case 10: reason_str = "SDIO reset"; break;
    default: reason_str = "Unknown"; break;
  }
  debug_println(reason_str);

  // Last error message
  if (strlen(wdt->last_error) > 0) {
    debug_print("Last error: ");
    debug_println(wdt->last_error);
  }

  debug_print("Last reboot uptime: ");
  debug_print_uint(wdt->last_reboot_uptime_ms / 1000);
  debug_println(" seconds");
  debug_println("");
}
