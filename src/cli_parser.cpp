/**
 * @file cli_parser.cpp
 * @brief CLI command parser and dispatcher (LAYER 7)
 *
 * Ported from: Mega2560 v3.6.5 cli_shell.cpp (tokenizer + dispatcher)
 * Adapted to: ESP32 modular architecture
 *
 * Responsibility:
 * - Command line tokenization (split by whitespace)
 * - Alias normalization (sh→show, wr→write, etc.)
 * - Command dispatch based on first token
 * - Parameter parsing and validation
 * - Error messages
 *
 * Context: This is the CLI "router" - maps user input to handlers
 */

#include "cli_parser.h"
#include "cli_commands.h"
#include "cli_show.h"
#include "cli_config_regs.h"
#include "cli_config_coils.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ============================================================================
 * TOKENIZER
 * ============================================================================ */

static bool is_whitespace(char c) {
  return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

/**
 * @brief Tokenize a command line into argv array
 * @return Number of tokens
 */
static uint8_t tokenize(char* line, char* argv[], uint8_t max_argv) {
  uint8_t argc = 0;
  char* p = line;

  while (*p && argc < max_argv) {
    // Skip whitespace
    while (*p && is_whitespace(*p)) p++;
    if (!*p) break;

    // Mark token start
    argv[argc++] = p;

    // Skip until whitespace
    while (*p && !is_whitespace(*p)) p++;

    // Null-terminate token
    if (*p) {
      *p = '\0';
      p++;
    }
  }

  return argc;
}

/* ============================================================================
 * ALIAS NORMALIZATION
 * ============================================================================ */

static const char* normalize_alias(const char* s) {
  if (!s) return "";

  // Verbs
  if (!strcmp(s, "SHOW") || !strcmp(s, "show") || !strcmp(s, "SH") || !strcmp(s, "sh")) return "SHOW";
  if (!strcmp(s, "SET") || !strcmp(s, "set") || !strcmp(s, "CONF")) return "SET";
  if (!strcmp(s, "RESET") || !strcmp(s, "reset")) return "RESET";
  if (!strcmp(s, "CLEAR") || !strcmp(s, "clear")) return "CLEAR";
  if (!strcmp(s, "SAVE") || !strcmp(s, "save") || !strcmp(s, "SV")) return "SAVE";
  if (!strcmp(s, "LOAD") || !strcmp(s, "load") || !strcmp(s, "LD")) return "LOAD";
  if (!strcmp(s, "DEFAULTS") || !strcmp(s, "defaults") || !strcmp(s, "DEF")) return "DEFAULTS";
  if (!strcmp(s, "REBOOT") || !strcmp(s, "reboot")) return "REBOOT";
  if (!strcmp(s, "HELP") || !strcmp(s, "help") || !strcmp(s, "?")) return "HELP";

  // Nouns
  if (!strcmp(s, "COUNTER") || !strcmp(s, "counter")) return "COUNTER";
  if (!strcmp(s, "COUNTERS") || !strcmp(s, "counters")) return "COUNTERS";
  if (!strcmp(s, "TIMER") || !strcmp(s, "timer")) return "TIMER";
  if (!strcmp(s, "TIMERS") || !strcmp(s, "timers")) return "TIMERS";
  if (!strcmp(s, "CONFIG") || !strcmp(s, "config")) return "CONFIG";
  if (!strcmp(s, "REGISTERS") || !strcmp(s, "registers")) return "REGISTERS";
  if (!strcmp(s, "COILS") || !strcmp(s, "coils")) return "COILS";
  if (!strcmp(s, "INPUTS") || !strcmp(s, "inputs")) return "INPUTS";
  if (!strcmp(s, "VERSION") || !strcmp(s, "version")) return "VERSION";
  if (!strcmp(s, "GPIO") || !strcmp(s, "gpio")) return "GPIO";

  // System commands (for SET context)
  if (!strcmp(s, "REG") || !strcmp(s, "reg")) return "REG";
  if (!strcmp(s, "COIL") || !strcmp(s, "coil")) return "COIL";
  if (!strcmp(s, "GPIO") || !strcmp(s, "gpio")) return "GPIO";
  if (!strcmp(s, "ID") || !strcmp(s, "id")) return "ID";
  if (!strcmp(s, "HOSTNAME") || !strcmp(s, "hostname")) return "HOSTNAME";
  if (!strcmp(s, "BAUD") || !strcmp(s, "baud")) return "BAUD";

  return s;  // Return as-is if not an alias
}

/* ============================================================================
 * COMMAND DISPATCH
 * ============================================================================ */

bool cli_parser_execute(char* line) {
  if (!line || !line[0]) return false;

  // Tokenize
  #define MAX_ARGV 32
  char* argv[MAX_ARGV];
  uint8_t argc = tokenize(line, argv, MAX_ARGV);

  if (argc == 0) return false;

  // Normalize first token
  const char* cmd = normalize_alias(argv[0]);

  // Dispatch
  if (!strcmp(cmd, "SHOW")) {
    // show <what> [params...]
    if (argc < 2) {
      debug_println("SHOW: missing argument");
      return false;
    }

    const char* what = normalize_alias(argv[1]);

    if (!strcmp(what, "CONFIG")) {
      cli_cmd_show_config();
      return true;
    } else if (!strcmp(what, "COUNTERS")) {
      cli_cmd_show_counters();
      return true;
    } else if (!strcmp(what, "TIMERS")) {
      cli_cmd_show_timers();
      return true;
    } else if (!strcmp(what, "REGISTERS")) {
      uint16_t start = 0, count = 0;
      if (argc > 2) start = atoi(argv[2]);
      if (argc > 3) count = atoi(argv[3]);
      cli_cmd_show_registers(start, count);
      return true;
    } else if (!strcmp(what, "COILS")) {
      cli_cmd_show_coils();
      return true;
    } else if (!strcmp(what, "INPUTS")) {
      cli_cmd_show_inputs();
      return true;
    } else if (!strcmp(what, "VERSION")) {
      cli_cmd_show_version();
      return true;
    } else if (!strcmp(what, "GPIO")) {
      cli_cmd_show_gpio();
      return true;
    } else if (!strcmp(what, "REG")) {
      // show reg - Display register configuration
      cli_cmd_show_regs();
      return true;
    } else if (!strcmp(what, "COIL")) {
      // show coil - Display coil configuration
      cli_cmd_show_coils();
      return true;
    } else {
      debug_println("SHOW: unknown argument");
      return false;
    }

  } else if (!strcmp(cmd, "SET")) {
    // set <what> <params...>
    if (argc < 2) {
      debug_println("SET: missing argument");
      return false;
    }

    const char* what = normalize_alias(argv[1]);

    if (!strcmp(what, "COUNTER")) {
      cli_cmd_set_counter(argc - 2, argv + 2);
      return true;
    } else if (!strcmp(what, "TIMER")) {
      cli_cmd_set_timer(argc - 2, argv + 2);
      return true;
    } else if (!strcmp(what, "HOSTNAME")) {
      if (argc < 3) {
        debug_println("SET HOSTNAME: missing value");
        return false;
      }
      cli_cmd_set_hostname(argv[2]);
      return true;
    } else if (!strcmp(what, "BAUD")) {
      if (argc < 3) {
        debug_println("SET BAUD: missing value");
        return false;
      }
      uint32_t baud = atol(argv[2]);
      cli_cmd_set_baud(baud);
      return true;
    } else if (!strcmp(what, "ID")) {
      if (argc < 3) {
        debug_println("SET ID: missing value");
        return false;
      }
      uint8_t id = atoi(argv[2]);
      cli_cmd_set_id(id);
      return true;
    } else if (!strcmp(what, "REG")) {
      if (argc < 3) {
        debug_println("SET REG: missing parameters");
        debug_println("  Usage: set reg STATIC <address> Value <value>");
        debug_println("         set reg DYNAMIC <address> counter<id>:<function> or timer<id>:<function>");
        return false;
      }

      const char* mode = normalize_alias(argv[2]);

      if (!strcmp(mode, "STATIC")) {
        // set reg STATIC <address> Value <value>
        cli_cmd_set_reg_static(argc - 3, argv + 3);
      } else if (!strcmp(mode, "DYNAMIC")) {
        // set reg DYNAMIC <address> counter<id>:<function> or timer<id>:<function>
        cli_cmd_set_reg_dynamic(argc - 3, argv + 3);
      } else {
        debug_println("SET REG: invalid mode (must be STATIC or DYNAMIC)");
        return false;
      }
      return true;

    } else if (!strcmp(what, "COIL")) {
      if (argc < 3) {
        debug_println("SET COIL: missing parameters");
        debug_println("  Usage: set coil STATIC <address> Value <ON|OFF>");
        debug_println("         set coil DYNAMIC <address> counter<id>:<function> or timer<id>:<function>");
        return false;
      }

      const char* mode = normalize_alias(argv[2]);

      if (!strcmp(mode, "STATIC")) {
        // set coil STATIC <address> Value <ON|OFF>
        cli_cmd_set_coil_static(argc - 3, argv + 3);
      } else if (!strcmp(mode, "DYNAMIC")) {
        // set coil DYNAMIC <address> counter<id>:<function> or timer<id>:<function>
        cli_cmd_set_coil_dynamic(argc - 3, argv + 3);
      } else {
        debug_println("SET COIL: invalid mode (must be STATIC or DYNAMIC)");
        return false;
      }
      return true;
    } else if (!strcmp(what, "GPIO")) {
      cli_cmd_set_gpio(argc - 2, argv + 2);
      return true;
    } else {
      debug_println("SET: unknown argument");
      return false;
    }

  } else if (!strcmp(cmd, "RESET")) {
    // reset counter <id>
    if (argc < 2) {
      debug_println("RESET: missing argument");
      return false;
    }

    const char* what = normalize_alias(argv[1]);

    if (!strcmp(what, "COUNTER")) {
      cli_cmd_reset_counter(argc - 2, argv + 2);
      return true;
    } else {
      debug_println("RESET: unknown argument");
      return false;
    }

  } else if (!strcmp(cmd, "CLEAR")) {
    // clear counters
    if (argc < 2) {
      debug_println("CLEAR: missing argument");
      return false;
    }

    const char* what = normalize_alias(argv[1]);

    if (!strcmp(what, "COUNTERS")) {
      cli_cmd_clear_counters();
      return true;
    } else {
      debug_println("CLEAR: unknown argument");
      return false;
    }

  } else if (!strcmp(cmd, "SAVE")) {
    cli_cmd_save();
    return true;

  } else if (!strcmp(cmd, "LOAD")) {
    cli_cmd_load();
    return true;

  } else if (!strcmp(cmd, "DEFAULTS")) {
    cli_cmd_defaults();
    return true;

  } else if (!strcmp(cmd, "REBOOT")) {
    cli_cmd_reboot();
    return true;

  } else if (!strcmp(cmd, "HELP")) {
    cli_parser_print_help();
    return true;

  } else {
    debug_println("Unknown command");
    return false;
  }
}

/* ============================================================================
 * HELP
 * ============================================================================ */

void cli_parser_print_help(void) {
  debug_println("\n=== Modbus RTU Server v1.0.0 (ESP32) ===\n");
  debug_println("Commands:");
  debug_println("  show config         - Display full configuration");
  debug_println("  show counters       - Display counter status");
  debug_println("  show timers         - Display timer status");
  debug_println("  show registers [start] [count]");
  debug_println("  show coils          - Display coil states");
  debug_println("  show inputs         - Display discrete inputs");
  debug_println("  show version        - Display firmware version");
  debug_println("  show reg            - Display register mappings");
  debug_println("  show coil           - Display coil mappings");
  debug_println("");
  debug_println("Configuration:");
  debug_println("  set reg STATIC <address> Value <value>");
  debug_println("  set reg DYNAMIC <address> counter<id>:<func> or timer<id>:<func>");
  debug_println("    Counter functions: index, raw, freq, overflow, ctrl");
  debug_println("    Timer functions: output");
  debug_println("");
  debug_println("  set coil STATIC <address> Value <ON|OFF>");
  debug_println("  set coil DYNAMIC <address> counter<id>:<func> or timer<id>:<func>");
  debug_println("    Counter functions: overflow");
  debug_println("    Timer functions: output");
  debug_println("");
  debug_println("Counters & Timers:");
  debug_println("  set counter <id> mode 1 parameter ...");
  debug_println("    hw-mode:<sw|sw-isr|hw> edge:<rising|falling|both>");
  debug_println("    prescaler:<1|4|8|16|64|256|1024>");
  debug_println("    index-reg:<reg> raw-reg:<reg> freq-reg:<reg>");
  debug_println("    ctrl-reg:<reg> overload-reg:<reg>");
  debug_println("    start-value:<n> scale:<float>");
  debug_println("    bit-width:<8|16|32|64>");
  debug_println("");
  debug_println("  set timer <id> mode <1|2|3|4> parameter ...");
  debug_println("  reset counter <id>");
  debug_println("  clear counters");
  debug_println("");
  debug_println("System:");
  debug_println("  set hostname <name>");
  debug_println("  set baud <rate>");
  debug_println("  set id <slave_id>");
  debug_println("");
  debug_println("  save                - Save config to NVS");
  debug_println("  load                - Load config from NVS");
  debug_println("  defaults            - Reset to factory defaults");
  debug_println("  reboot              - Restart system");
  debug_println("");
}
