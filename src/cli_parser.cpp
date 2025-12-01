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
#include "cli_commands_logic.h"
#include "st_logic_config.h"
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
  if (!strcmp(s, "NO") || !strcmp(s, "no")) return "NO";
  if (!strcmp(s, "RESET") || !strcmp(s, "reset")) return "RESET";
  if (!strcmp(s, "CLEAR") || !strcmp(s, "clear")) return "CLEAR";
  if (!strcmp(s, "SAVE") || !strcmp(s, "save") || !strcmp(s, "SV")) return "SAVE";
  if (!strcmp(s, "LOAD") || !strcmp(s, "load") || !strcmp(s, "LD")) return "LOAD";
  if (!strcmp(s, "DEFAULTS") || !strcmp(s, "defaults") || !strcmp(s, "DEF")) return "DEFAULTS";
  if (!strcmp(s, "REBOOT") || !strcmp(s, "reboot")) return "REBOOT";
  if (!strcmp(s, "HELP") || !strcmp(s, "help") || !strcmp(s, "?")) return "HELP";
  if (!strcmp(s, "READ") || !strcmp(s, "read") || !strcmp(s, "RD")) return "READ";
  if (!strcmp(s, "WRITE") || !strcmp(s, "write") || !strcmp(s, "WR")) return "WRITE";

  // Nouns
  if (!strcmp(s, "COUNTER") || !strcmp(s, "counter")) return "COUNTER";
  if (!strcmp(s, "COUNTERS") || !strcmp(s, "counters")) return "COUNTERS";
  if (!strcmp(s, "TIMER") || !strcmp(s, "timer")) return "TIMER";
  if (!strcmp(s, "TIMERS") || !strcmp(s, "timers")) return "TIMERS";
  if (!strcmp(s, "LOGIC") || !strcmp(s, "logic")) return "LOGIC";
  if (!strcmp(s, "CONFIG") || !strcmp(s, "config")) return "CONFIG";
  if (!strcmp(s, "REGISTERS") || !strcmp(s, "registers")) return "REGISTERS";
  if (!strcmp(s, "COILS") || !strcmp(s, "coils")) return "COILS";
  if (!strcmp(s, "INPUTS") || !strcmp(s, "inputs")) return "INPUTS";
  if (!strcmp(s, "INPUT") || !strcmp(s, "input")) return "INPUT";
  if (!strcmp(s, "VERSION") || !strcmp(s, "version")) return "VERSION";
  if (!strcmp(s, "GPIO") || !strcmp(s, "gpio")) return "GPIO";
  if (!strcmp(s, "ECHO") || !strcmp(s, "echo")) return "ECHO";

  // System commands (for SET context)
  if (!strcmp(s, "REG") || !strcmp(s, "reg")) return "REG";
  if (!strcmp(s, "COIL") || !strcmp(s, "coil")) return "COIL";
  if (!strcmp(s, "GPIO") || !strcmp(s, "gpio")) return "GPIO";
  if (!strcmp(s, "ID") || !strcmp(s, "id")) return "ID";
  if (!strcmp(s, "HOSTNAME") || !strcmp(s, "hostname")) return "HOSTNAME";
  if (!strcmp(s, "BAUD") || !strcmp(s, "baud")) return "BAUD";
  if (!strcmp(s, "ENABLE") || !strcmp(s, "enable")) return "ENABLE";
  if (!strcmp(s, "DISABLE") || !strcmp(s, "disable")) return "DISABLE";

  // Logic Mode subcommands
  if (!strcmp(s, "UPLOAD") || !strcmp(s, "upload")) return "UPLOAD";
  if (!strcmp(s, "BIND") || !strcmp(s, "bind")) return "BIND";
  if (!strcmp(s, "DELETE") || !strcmp(s, "delete")) return "DELETE";
  if (!strcmp(s, "ENABLED") || !strcmp(s, "enabled")) return "ENABLED";

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
    } else if (!strcmp(what, "ECHO")) {
      cli_cmd_show_echo();
      return true;
    } else if (!strcmp(what, "REG")) {
      // show reg - Display register configuration
      cli_cmd_show_regs();
      return true;
    } else if (!strcmp(what, "COIL")) {
      // show coil - Display coil configuration
      cli_cmd_show_coils();
      return true;
    } else if (!strcmp(what, "LOGIC")) {
      // show logic <id|all|stats>
      if (argc < 3) {
        debug_println("SHOW LOGIC: missing argument (expected <id|all|stats>)");
        return false;
      }

      const char* subcommand = argv[2];

      if (!strcmp(subcommand, "all")) {
        cli_cmd_show_logic_all(st_logic_get_state());
        return true;
      } else if (!strcmp(subcommand, "stats")) {
        cli_cmd_show_logic_stats(st_logic_get_state());
        return true;
      } else {
        // show logic <id>
        uint8_t program_id = atoi(subcommand);
        cli_cmd_show_logic_program(st_logic_get_state(), program_id);
        return true;
      }
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
      // Check if third argument is "control"
      if (argc >= 4 && (!strcmp(argv[3], "control") || !strcmp(argv[3], "CONTROL"))) {
        // set counter <id> control ... → skip "counter" and "control", pass <id> + params
        // argv[0] = "set", argv[1] = "counter", argv[2] = id, argv[3] = "control", argv[4+] = params
        char* control_argv[32];
        control_argv[0] = argv[2];  // counter id
        for (uint8_t i = 4; i < argc && i < 32; i++) {
          control_argv[i - 3] = argv[i];
        }
        cli_cmd_set_counter_control(argc - 3, control_argv);
      } else {
        cli_cmd_set_counter(argc - 2, argv + 2);
      }
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
      // Check if it's GPIO 2 enable/disable command
      if (argc >= 4 && atoi(argv[2]) == 2) {
        const char* action = normalize_alias(argv[3]);
        if (!strcmp(action, "ENABLE") || !strcmp(action, "DISABLE")) {
          // set gpio 2 enable/disable
          cli_cmd_set_gpio2(argc - 2, argv + 2);
          return true;
        }
      }
      // Otherwise, handle as normal GPIO mapping
      cli_cmd_set_gpio(argc - 2, argv + 2);
      return true;
    } else if (!strcmp(what, "ECHO")) {
      cli_cmd_set_echo(argc - 2, argv + 2);
      return true;
    } else if (!strcmp(what, "LOGIC")) {
      // set logic <id> <subcommand> [params...]
      if (argc < 4) {
        debug_println("SET LOGIC: missing arguments");
        debug_println("  Usage: set logic <id> upload \"<code>\"");
        debug_println("         set logic <id> enabled:true|false");
        debug_println("         set logic <id> delete");
        debug_println("         set logic <id> bind <var_idx> <register> [input|output|both]");
        return false;
      }

      uint8_t program_id = atoi(argv[2]);
      const char* subcommand = argv[3];  // Don't normalize yet - may be key:value

      // Check for enabled:true|false format first (special case)
      if (strstr(subcommand, "enabled:")) {
        bool enabled = (strstr(subcommand, "true")) ? true : false;
        cli_cmd_set_logic_enabled(st_logic_get_state(), program_id, enabled);
        return true;
      }

      // Now normalize for other commands
      const char* cmd_normalized = normalize_alias(subcommand);

      if (!strcmp(cmd_normalized, "UPLOAD")) {
        // set logic <id> upload "<code>"
        if (argc < 5) {
          debug_println("SET LOGIC UPLOAD: missing source code");
          return false;
        }
        cli_cmd_set_logic_upload(st_logic_get_state(), program_id, argv[4]);
        return true;
      } else if (!strcmp(cmd_normalized, "DELETE")) {
        // set logic <id> delete
        cli_cmd_set_logic_delete(st_logic_get_state(), program_id);
        return true;
      } else if (!strcmp(cmd_normalized, "BIND")) {
        // set logic <id> bind <var_spec> <register_spec>
        if (argc < 6) {
          debug_println("SET LOGIC BIND: missing parameters");
          debug_println("  Usage (NEW):  set logic <id> bind <var_name> reg:100|coil:10|input-dis:5");
          debug_println("  Usage (OLD):  set logic <id> bind <var_idx> <register> [input|output|both]");
          return false;
        }

        const char* arg4 = argv[4];
        const char* arg5 = argv[5];

        // Detect new syntax: check if arg5 contains "reg:", "coil:", or "input-dis:"
        if (strstr(arg5, "reg:") || strstr(arg5, "coil:") || strstr(arg5, "input-dis:")) {
          // NEW SYNTAX: variable name + binding spec
          cli_cmd_set_logic_bind_by_name(st_logic_get_state(), program_id, arg4, arg5);
          return true;
        }

        // OLD SYNTAX: variable index + register + direction
        // Backward compatible: set logic <id> bind <var_idx> <register> [input|output|both]
        uint8_t var_idx = atoi(arg4);
        uint16_t register_addr = atoi(arg5);
        const char* direction = (argc > 6) ? argv[6] : "both";

        cli_cmd_set_logic_bind(st_logic_get_state(), program_id, var_idx, register_addr, direction);
        return true;
      } else {
        debug_println("SET LOGIC: unknown subcommand");
        return false;
      }
    } else {
      debug_println("SET: unknown argument");
      return false;
    }

  } else if (!strcmp(cmd, "NO")) {
    // no set <what> <params...>
    if (argc < 2) {
      debug_println("NO: missing argument (expected 'set')");
      return false;
    }

    if (!strcmp(normalize_alias(argv[1]), "SET")) {
      // no set <what> <params...>
      if (argc < 3) {
        debug_println("NO SET: missing argument");
        return false;
      }

      const char* what = normalize_alias(argv[2]);

      if (!strcmp(what, "GPIO")) {
        cli_cmd_no_set_gpio(argc - 3, argv + 3);
        return true;
      } else {
        debug_println("NO SET: unknown argument (only GPIO supported)");
        return false;
      }
    } else {
      debug_println("NO: unknown argument (expected 'set')");
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

  } else if (!strcmp(cmd, "READ")) {
    // read <what> <params...>
    if (argc < 2) {
      debug_println("READ: manglende argument");
      debug_println("  Brug: read reg <id> <antal>");
      debug_println("        read coil <id> <antal>");
      debug_println("        read input <id> <antal>");
      return false;
    }

    const char* what = normalize_alias(argv[1]);

    if (!strcmp(what, "REG")) {
      cli_cmd_read_reg(argc - 2, argv + 2);
      return true;
    } else if (!strcmp(what, "COIL")) {
      cli_cmd_read_coil(argc - 2, argv + 2);
      return true;
    } else if (!strcmp(what, "INPUT")) {
      cli_cmd_read_input(argc - 2, argv + 2);
      return true;
    } else {
      debug_println("READ: ukendt argument (brug: reg, coil, input)");
      return false;
    }

  } else if (!strcmp(cmd, "WRITE")) {
    // write <what> <params...>
    if (argc < 2) {
      debug_println("WRITE: manglende argument");
      debug_println("  Brug: write reg <id> value <v\u00e6rdi>");
      debug_println("        write coil <id> value <on|off>");
      return false;
    }

    const char* what = normalize_alias(argv[1]);

    if (!strcmp(what, "REG")) {
      cli_cmd_write_reg(argc - 2, argv + 2);
      return true;
    } else if (!strcmp(what, "COIL")) {
      cli_cmd_write_coil(argc - 2, argv + 2);
      return true;
    } else {
      debug_println("WRITE: ukendt argument (brug: reg, coil)");
      return false;
    }

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
  debug_println("  show gpio           - Display GPIO mappings");
  debug_println("  show echo           - Display remote echo status");
  debug_println("  show reg            - Display register mappings");
  debug_println("  show coil           - Display coil mappings");
  debug_println("");
  debug_println("Read/Write Commands:");
  debug_println("  read reg <id> <antal>              - Read holding registers");
  debug_println("  read coil <id> <antal>             - Read coils");
  debug_println("  read input <id> <antal>            - Read discrete inputs");
  debug_println("  write reg <id> value <0..65535>    - Write holding register");
  debug_println("  write coil <id> value <on|off>     - Write coil");
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
  debug_println("");
  debug_println("    Common parameters:");
  debug_println("      hw-mode:<sw|sw-isr|hw>    - Hardware mode");
  debug_println("        sw     = Software polling (needs input-dis:<pin>)");
  debug_println("        sw-isr = Software interrupt (needs interrupt-pin:<gpio>)");
  debug_println("        hw     = Hardware PCNT (GPIO 19/25/27/33)");
  debug_println("      edge:<rising|falling|both> - Edge detection");
  debug_println("      prescaler:<1|4|8|16|64|256|1024> - Divide counter");
  debug_println("      start-value:<n>           - Initial counter value");
  debug_println("      scale:<float>             - Multiply output (e.g. 2.5)");
  debug_println("      bit-width:<8|16|32|64>    - Counter resolution");
  debug_println("      dir:<up|down>             - Count direction");
  debug_println("      debounce:<on|off>         - Enable/disable debounce (default: on)");
  debug_println("      debounce-ms:<ms>          - Debounce time in ms (default: 10)");
  debug_println("");
  debug_println("    Register mapping:");
  debug_println("      index-reg:<addr>    - Scaled value register");
  debug_println("      raw-reg:<addr>      - Prescaled value register");
  debug_println("      freq-reg:<addr>     - Frequency (Hz) register");
  debug_println("      ctrl-reg:<addr>     - Control register");
  debug_println("      overload-reg:<addr> - Overflow flag register");
  debug_println("");
  debug_println("    Mode-specific:");
  debug_println("      input-dis:<pin>       - For SW mode: discrete input pin");
  debug_println("      interrupt-pin:<gpio>  - For SW-ISR mode: GPIO interrupt pin");
  debug_println("      hw-gpio:<gpio>        - For HW mode: PCNT GPIO pin (BUG FIX 1.9)");
  debug_println("");
  debug_println("    Examples:");
  debug_println("      HW mode:  set counter 1 mode 1 hw-mode:hw edge:rising \\");
  debug_println("                hw-gpio:19 prescaler:16 index-reg:20 raw-reg:30 \\");
  debug_println("                freq-reg:35 ctrl-reg:40 debounce:on");
  debug_println("      ISR mode: set counter 2 mode 1 hw-mode:sw-isr edge:falling \\");
  debug_println("                interrupt-pin:13 index-reg:40 raw-reg:45 \\");
  debug_println("                debounce:on debounce-ms:50");
  debug_println("      SW mode:  set counter 3 mode 1 hw-mode:sw edge:rising \\");
  debug_println("                input-dis:50 index-reg:60 raw-reg:65 debounce:off");
  debug_println("");
  debug_println("  Counter control:");
  debug_println("    set counter <id> control reset-on-read:<on|off>");
  debug_println("    set counter <id> control auto-start:<on|off>");
  debug_println("    set counter <id> control running:<on|off>");
  debug_println("    Example: set counter 1 control auto-start:on running:on");
  debug_println("");
  debug_println("  Counter operations:");
  debug_println("    reset counter <id>  - Reset single counter to start-value");
  debug_println("    clear counters      - Reset all counters");
  debug_println("");
  debug_println("  Timers (4 max, Mode 1-4 with control via ctrl_reg):");
  debug_println("    set timer <id> mode 1 p1-duration:<ms> p1-output:<0|1> \\");
  debug_println("                      p2-duration:<ms> p2-output:<0|1> \\");
  debug_println("                      p3-duration:<ms> p3-output:<0|1> \\");
  debug_println("                      output-coil:<addr> [ctrl-reg:<reg>]");
  debug_println("      Mode 1: One-shot (3-phase sequence) - manual START via ctrl_reg");
  debug_println("");
  debug_println("    set timer <id> mode 2 pulse-ms:<ms> \\");
  debug_println("                      p1-output:<level> p2-output:<level> \\");
  debug_println("                      output-coil:<addr> [ctrl-reg:<reg>]");
  debug_println("      Mode 2: Monostable (retriggerable pulse)");
  debug_println("");
  debug_println("    set timer <id> mode 3 on-ms:<ms> off-ms:<ms> \\");
  debug_println("                      p1-output:<level> p2-output:<level> \\");
  debug_println("                      output-coil:<addr> [ctrl-reg:<reg>] enabled:<0|1>");
  debug_println("      Mode 3: Astable (oscillator/blink - auto-start when enabled)");
  debug_println("");
  debug_println("    set timer <id> mode 4 input-dis:<coil> trigger-edge:<0|1> \\");
  debug_println("                      delay-ms:<ms> output-coil:<addr> [ctrl-reg:<reg>]");
  debug_println("      Mode 4: Input-triggered (edge detection)");
  debug_println("        trigger-edge: 1=rising (0->1), 0=falling (1->0)");
  debug_println("        input-dis: COIL address to monitor (can be virtual GPIO 100-255)");
  debug_println("");
  debug_println("    Control Register (ctrl_reg) - Control timer via Modbus register:");
  debug_println("      write reg <ctrl-reg> value 1   - START timer (Bit 0)");
  debug_println("      write reg <ctrl-reg> value 2   - STOP timer (Bit 1)");
  debug_println("      write reg <ctrl-reg> value 4   - RESET timer (Bit 2)");
  debug_println("      Bits auto-clear after execution");
  debug_println("");
  debug_println("    Timer examples:");
  debug_println("      Mode 1 with START: set timer 1 mode 1 p1-dur:500 p1-out:1 ctrl-reg:100 \\");
  debug_println("                         output-coil:200");
  debug_println("                         write reg 100 value 1   ← START!");
  debug_println("      Mode 3: set timer 1 mode 3 on-ms:1000 off-ms:1000 \\");
  debug_println("              p1-output:1 p2-output:0 output-coil:200 enabled:1");
  debug_println("      Mode 4: set timer 2 mode 4 input-dis:30 trigger-edge:1 \\");
  debug_println("              delay-ms:0 output-coil:250");
  debug_println("");
  debug_println("GPIO Management:");
  debug_println("  Physical GPIO: 0-39 (direct ESP32 pins)");
  debug_println("  Virtual GPIO:  100-255 (reads/writes COIL directly - perfect for testing!)");
  debug_println("");
  debug_println("  set gpio <pin> static map input:<idx>   - Map GPIO input to discrete input");
  debug_println("  set gpio <pin> static map coil:<idx>    - Map GPIO output to coil");
  debug_println("  no set gpio <pin>                       - Remove GPIO mapping");
  debug_println("");
  debug_println("  Virtual GPIO - Simulate GPIO without hardware!");
  debug_println("    FORMULA: Virtual GPIO N (N>=100) reads COIL (N-100) → Discrete Input");
  debug_println("");
  debug_println("    Example: set gpio 140 static map input:10");
  debug_println("      Virtual GPIO 140 → reads COIL 40 (140-100=40) → Discrete Input 10");
  debug_println("");
  debug_println("    HOW IT WORKS:");
  debug_println("      write coil 40 value 1");
  debug_println("        ↓ COIL 40 = 1");
  debug_println("        ↓ Virtual GPIO 140 detects level");
  debug_println("        ↓ Discrete Input 10 = 1");
  debug_println("      read input 10 1  →  Result: 1 ✅");
  debug_println("");
  debug_println("    USE CASE - Test Timer Mode 4 without GPIO hardware:");
  debug_println("      set gpio 140 static map input:10");
  debug_println("      set timer 1 mode 4 input-dis:10 trigger-edge:1 output-coil:250");
  debug_println("      write coil 40 value 0");
  debug_println("      write coil 40 value 1     ← Rising edge (0→1) triggers Timer!");
  debug_println("      read coil 250 1           ← Output = 1 ✅");
  debug_println("");
  debug_println("  GPIO2 special (heartbeat LED):");
  debug_println("    set gpio 2 enable   - Release GPIO2 for user code (disable LED)");
  debug_println("    set gpio 2 disable  - Reserve GPIO2 for heartbeat (enable LED, default)");
  debug_println("");
  debug_println("System:");
  debug_println("  set hostname <name>      - Set system name");
  debug_println("  set baud <rate>          - Set Modbus baudrate (300-115200, requires reboot)");
  debug_println("  set id <slave_id>        - Set Modbus slave ID (0-247)");
  debug_println("  set echo <on|off>        - Enable/disable remote echo");
  debug_println("");
  debug_println("Persistence (NVS - Non-Volatile Storage):");
  debug_println("  save                - Save all configs to NVS (persistent across reboot)");
  debug_println("  load                - Load configs from NVS");
  debug_println("  defaults            - Reset to factory defaults");
  debug_println("  reboot              - Restart ESP32");
  debug_println("");
  debug_println("Persistence features:");
  debug_println("  - All timers, counters, GPIO mappings saved");
  debug_println("  - Schema versioning (v2 with v1 migration)");
  debug_println("  - CRC16 validation for data integrity");
  debug_println("  - Auto-save after counter/timer configuration");
  debug_println("");
  debug_println("Quick examples:");
  debug_println("  show config              - View all settings");
  debug_println("  help  or  ?              - This help message");
  debug_println("");
}
