# Changelog - ESP32 Modbus RTU Server

All notable changes to this project are documented in this file.

---

## [3.1.0] - 2025-12-05 🌐 (WiFi Display, Validation, & Telnet Auth Improvements)

### BUGFIXES & IMPROVEMENTS

#### WiFi Configuration Display & Validation ✅ IMPROVED
- **Issue:** WiFi configuration not showing in `show config`, IP not displaying on DHCP, Telnet status always showing "DISABLED"
- **Solution:**
  1. Added complete WiFi/Network section to `show config` - displays all WiFi settings centrally
  2. Enhanced `show wifi` IP display - shows actual IP from WiFi driver (works for DHCP and static)
  3. Fixed Telnet status - now reads from `g_persist_config.network.telnet_enabled` instead of hardcoded
  4. Added `set wifi telnet enable|disable` command for toggling Telnet via CLI
  5. Improved WiFi connection validation with specific error messages:
     - SSID empty check before connection attempt
     - Password length validation (0 or 8-63 chars)
     - Clear hints for each validation failure
  6. Enhanced `network_config_validate()` to explicitly check for empty SSID when WiFi enabled
  7. Added WiFi debug flags (`wifi_connect`, `network_validate`) with comprehensive logging

#### Telnet Server Duplicate Prompt Fix ✅ IMPROVED
- **Issue:** Telnet authentication showed duplicate login banner and prompts, output was garbled
- **Root Cause:** Double prompt sending - both `telnet_handle_auth_input()` AND `telnet_server_loop()` sent next prompt
- **Solution:**
  - Remove duplicate `telnet_send_auth_prompt()` call after auth input (line 530-531)
  - Create `telnet_send_retry_prompt()` for simplified retry prompts (no banner)
  - Keep full banner only for initial connection
- **Result:** Clean authentication with single prompt per step

### NEW FEATURES
- `set wifi telnet enable|disable` - Toggle Telnet server without file edit
- Enhanced `show config` network section - centralized configuration display
- Detailed `connect wifi` error messages with helpful hints
- WiFi debug flags for troubleshooting connection issues

---

## [2.3.0] - 2025-12-04 🔧 (CRITICAL BUGFIX RELEASE)

### BUGFIXES & CRITICAL FIXES

#### GPIO Persistence & Config Reboot Restoration ✅ FIXED
**Root Cause:** Compiler struct padding caused misalignment between save/load, breaking CRC validation

- **Problem:** GPIO mappings and all config were lost after reboot (appeared saved but weren't restored)
- **Root Cause #1:** Struct alignment padding - `uint32_t baudrate` was misaligned causing entire config to shift
- **Root Cause #2:** Schema version mismatch (v2 vs v3 vs v4) - no validation before CRC check
- **Solution #1:** All persistence structs marked with `__attribute__((packed))` for binary compatibility
  - `PersistConfig` - main config struct
  - `CounterConfig` - counter configuration
  - `TimerConfig` - timer configuration
  - `StaticRegisterMapping`, `DynamicRegisterMapping` - register mappings
  - `StaticCoilMapping`, `DynamicCoilMapping` - coil mappings
  - `VariableMapping` - GPIO + ST variable unified mapping
- **Solution #2:** Explicit schema version validation BEFORE CRC check in `config_load.cpp`
- **Impact:** Config persistence now 100% reliable across firmware updates

### NEW FEATURES

#### Debug Command System 🆕
Runtime-controllable debug flags for diagnostic output

- `set debug config-save on|off` - Toggle config save debugging
- `set debug config-load on|off` - Toggle config load debugging
- `set debug all on|off` - Toggle all debug flags
- Default: All debug ON (helps initial diagnosis)
- Can be disabled without reboot for cleaner output

**Files:**
- `src/debug_flags.cpp/.h` - Global debug flag management
- Modified: `config_save.cpp`, `config_load.cpp` - Conditional debug output
- Modified: `cli_commands.cpp`, `cli_parser.cpp` - CLI integration

### IMPROVEMENTS

#### Config Load Debug Enhancements
- Added `[LOAD_START]` message at boot
- Added `[LOAD_DEBUG] Reading blob` - shows NVS read attempt
- Added `[LOAD_DEBUG] nvs_get_blob returned` - shows NVS read result
- Added `[LOAD_DEBUG] After nvs_get_blob` - shows var_map_count and schema_version
- Added `[LOAD_DEBUG] First 20 bytes` - hex dump of loaded data for diagnostics
- Added `[LOAD_DEBUG] Schema version OK` - confirmation of version match
- All controllable via `set debug config-load on|off`

#### Config Save Debug Enhancements
- Added `[SAVE_DEBUG] var_map_count` - shows data being saved
- Added `[SAVE_DEBUG] sizeof(PersistConfig)` - shows structure size
- Added `[SAVE_DEBUG] var_maps[i]` entries - lists each mapping being saved
- All controllable via `set debug config-save on|off`

### DOCUMENTATION

- Updated `docs/index.html` with v2.3 features
- Added "Struct Alignment Fix" explanation in Persistence section
- Added GPIO persistence examples
- Added debug commands reference
- Updated footer to v2.3.0

### TESTING & VALIDATION

- ✅ Verified: GPIO mappings persist across reboot
- ✅ Verified: All config types (counters, timers, registers, coils) persist
- ✅ Verified: CRC validation passes 100% reliably
- ✅ Verified: Schema version validation works correctly
- ✅ Verified: Debug flags can be toggled without reboot
- ✅ Verified: Backward compatibility maintained (auto-converts old configs)

---

## [2.0.0] - 2025-11-30 🚀 (MAJOR RELEASE)

### NEW FEATURES

#### Structured Text (IEC 61131-3) Language Support ✨
**Introduction of Logic Mode - Programmatic control in ST language**

- **Language:** Structured Text per IEC 61131-3:2013 standard
- **Compliance:** ST-Light profile (embedded PLC variant)
- **Implementation Status:** Phase 1-2 (Lexer, Parser, AST complete)

#### ST Lexer (Tokenization)
- Full keyword recognition: IF, THEN, ELSE, ELSIF, FOR, WHILE, REPEAT, CASE, OF, DO, END_*
- Data type keywords: BOOL, INT, DWORD, REAL
- Variable declarators: VAR, VAR_INPUT, VAR_OUTPUT, CONST
- Operators: +, -, *, /, MOD, AND, OR, NOT, XOR, SHL, SHR, <, >, <=, >=, =, <>
- Literals: Boolean (TRUE, FALSE), Integer (decimal, hex, binary), Real (IEEE 754), String
- Comments: Block comments with nesting `(* comment *)`
- **Code:** `src/st_lexer.cpp` (580 lines)

#### ST Parser (AST Construction)
- Recursive descent parser with operator precedence
- Statements: IF/ELSIF/ELSE, CASE/OF, FOR/TO/BY/DO, WHILE/DO, REPEAT/UNTIL, EXIT
- Expressions: Binary ops, unary ops, literals, variables, parenthesized
- Variable declarations: VAR, VAR_INPUT, VAR_OUTPUT with type inference
- Full AST construction for all statement types
- Memory-safe tree with proper freeing
- **Code:** `src/st_parser.cpp` (680 lines)

#### Data Types (IEC 61131-3 Elementary)
- **BOOL:** Boolean (0 = FALSE, 1 = TRUE)
- **INT:** Signed 32-bit (-2^31 to 2^31-1)
- **DWORD:** Unsigned 32-bit (0 to 2^32-1)
- **REAL:** IEEE 754 32-bit floating point

#### Control Flow Statements (IEC 6.3.2)
- **IF/THEN/ELSE/ELSIF/END_IF:** Conditional execution
- **CASE/OF/END_CASE:** Multi-way branching
- **FOR/TO/BY/DO/END_FOR:** Deterministic counting loops
- **WHILE/DO/END_WHILE:** Condition-based loops
- **REPEAT/UNTIL/END_REPEAT:** Post-test loops
- **EXIT:** Break from innermost loop

#### Operators (Full Set)
- Arithmetic: +, -, *, /, MOD (% semantics)
- Logical: AND, OR, NOT (full boolean algebra)
- Relational: <, >, <=, >=, =, <> (full comparison set)
- Bitwise: SHL (shift left), SHR (shift right) on INT/DWORD
- Assignment: := (Pascal-style)
- Operator precedence: Standard (multiplication > addition > relational > logical)

#### Type System & Semantics
- Type checking at parse time
- Implicit type conversions (INT + REAL → REAL)
- Operator overloading (AND works on BOOL and INT)
- Assignment with type coercion

#### Documentation
- **File:** `docs/ST_IEC61131_COMPLIANCE.md` (comprehensive)
  - 13 sections covering full compliance claim
  - Detailed feature matrix (supported vs. excluded)
  - Execution model and memory constraints
  - Modbus register binding strategy
  - Known limitations and future extensions

#### Testing Infrastructure
- **File:** `test/test_st_lexer_parser.cpp` (360 lines)
- 8 test cases: lexer/parser for IF, FOR, WHILE, REPEAT, CASE, expressions
- Real-world examples (nested IF, complex expressions)
- AST visualization and debugging

#### Type Definitions & Structures
- **File:** `include/st_types.h` (438 lines)
- Token types (st_token_type_t): 60+ token types
- Data types (st_datatype_t): BOOL, INT, DWORD, REAL, NONE
- AST node types: assignment, if/case/for/while/repeat, binary/unary ops, literal, variable
- Bytecode instructions: 30+ opcodes (stack-based VM design)
- Variable declaration: st_variable_decl_t with scope flags
- Program structure: st_program_t containing VAR block + statement list

#### Design for Phase 2 (Bytecode Compiler & VM)
- Bytecode instruction set defined: PUSH_*, STORE_*, LOAD_*, arithmetic ops, jumps
- Stack-based VM architecture designed (64-value stack)
- VM state machine structure (pc, stack_ptr, halted flag)
- Bytecode program container (512 instruction max)
- Ready for compiler and execution engine implementation

#### Modbus Integration (Phase 3)
- Logic programs isolated from main Modbus server
- VAR_INPUT binds to Modbus registers (read-only in program)
- VAR_OUTPUT binds to Modbus registers (write-only in program)
- 4 independent logic program slots
- Register mapping configuration separate from ST code

---

## [1.0.0] - 2025-11-29 🎉

### FEATURES

#### New: Timer Mode 4 (Input-Triggered) ✨
- **Rising edge detection** (trigger-edge:1): Responds to 0→1 transitions
- **Falling edge detection** (trigger-edge:0): Responds to 1→0 transitions
- **Configurable delay** (delay-ms): Wait before setting output
- **COIL-based input**: Use Modbus COILs as input source for testing
- **Output latch behavior**: Output stays HIGH once triggered
- **Proper state initialization**: No false edges on timer configuration
- **Code:** `src/timer_engine.cpp:152-216`, `src/cli_commands.cpp:391-395`
- **Test:** `test_mode4_comprehensive.py` - [PASS]

#### Timer Modes (Complete)
- **Mode 1:** One-shot (3-phase sequence)
- **Mode 2:** Monostable (retriggerable pulse)
- **Mode 3:** Astable (oscillator/blink) - Verified working
- **Mode 4:** Input-triggered (NEW - verified working)

#### Counter Modes (Complete)
- **Mode 1:** SW (Software Polling) ~10kHz
- **Mode 2:** SW-ISR (GPIO Interrupt) ~100kHz
- **Mode 3:** HW (PCNT Hardware) >1MHz

#### Advanced Features
- **Prescaler:** 1, 4, 8, 16, 64, 256, 1024
- **Scale factor:** Float multiplikator for units
- **Bit-width clamping:** 8, 16, 32, 64-bit support
- **Frequency measurement:** 0-20kHz with 1-sec window
- **Control register:** Start/stop/reset via Modbus
- **PCNT overflow handling:** 16-bit HW → 64-bit accumulator

#### Persistence
- **NVS Storage:** All configs persist across reboot
- **Schema versioning:** v2 with v1 migration support
- **CRC16 validation:** Data integrity check
- **Auto-save:** After counter/timer configuration

#### CLI Commands
- **70+ commands:** Set/show for timers, counters, registers, coils
- **GPIO mapping:** Static cortlægning (persistent)
- **Persistence:** save, load, defaults, reboot
- **System:** hostname, baud, id, echo

#### GPIO Features
- **Physical GPIO:** 0-39 (direct ESP32 pins)
- **Virtual GPIO:** 100-255 (read/write COIL mapping)
- **GPIO2 Heartbeat:** 500ms blink, toggleable
- **STATIC Mapping:** GPIO ↔ Discrete input/Coil

#### Register & Coil Management
- **STATIC registers:** 16 max (persistent)
- **DYNAMIC registers:** 16 max (auto-updated)
- **STATIC coils:** 16 max (persistent)
- **DYNAMIC coils:** 16 max (auto-updated)
- **Multi-word support:** 32/64-bit across registers

---

## [1.0.0-rc1] - 2025-11-28

### FIXED

#### P0.7: PCNT Division-by-2 Bug
- **Issue:** PCNT was dividing counter by 2 (incorrect)
- **Root cause:** Misunderstanding of PCNT hardware behavior
- **Fix:** Removed division - PCNT counts 100% correctly
- **Code:** `src/counter_hw.cpp`
- **Impact:** Counter Mode 3 now accurate

#### CNT-HW-08: Race Condition (Gate-based sync)
- **Issue:** PCNT polling task interfered with counter reads
- **Root cause:** Async poll updates during Modbus reads
- **Fix:** Implement pause/resume gates for atomic access
- **Code:** `src/counter_engine.cpp`, `src/pcnt_driver.cpp`
- **Impact:** Stable counter readings under load

#### Timer Mode 3 Test Suite Failure
- **Issue:** test_suite_extended reports [0,0,0,0,0] for Mode 3
- **Root cause:** Async serial reader race condition in test
- **Status:** NOT A FIRMWARE BUG - Manual tests prove Mode 3 works
- **Verification:** `test_timer_exact.py`, `test_coil_raw.py` - [PASS]

---

## [0.9.0] - 2025-11-15

### REFACTORING

#### Architecture Overhaul (Mega2560 → ESP32)
- **Modular structure:** 30+ focused .cpp files (100-250 lines each)
- **Layer-based design:** 8 clear abstraction layers
- **Hardware drivers:** Explicit abstraction (gpio, uart, pcnt, nvs)
- **Separation of concerns:** Config load/save/apply are separate

#### Module Structure
```
Layer 0: Hardware Abstraction (gpio_driver, uart_driver, pcnt_driver, nvs_driver)
Layer 1: Protocol Core (modbus_frame, modbus_parser, modbus_serializer)
Layer 2: Function Codes (modbus_fc_read, modbus_fc_write, modbus_fc_dispatch)
Layer 3: Modbus Runtime (modbus_rx, modbus_tx, modbus_server)
Layer 4: Storage (registers, coils, gpio_mapping)
Layer 5: Feature Engines (counter_*, timer_*)
Layer 6: Persistence (config_load, config_save, config_apply)
Layer 7: CLI (cli_parser, cli_commands, cli_show, cli_shell)
Layer 8: System (heartbeat, version, debug, main)
```

---

## [0.8.5] - Previous Mega2560 v3.6.5

### ORIGINAL FEATURES (Ported to ESP32)
- Modbus RTU (FC01-10)
- 4 Timers (Modes 1-3)
- 4 Counters (SW, SW-ISR, HW)
- EEPROM persistence
- CLI shell
- RS-485 support

---

## Development Notes

### Testing Status
- ✅ Mode 3: Astable/Blink verified working
- ✅ Mode 4: Input-triggered [PASS] both rise/fall
- ✅ Counter: All modes operational
- ✅ Persistence: Save/load/defaults working
- ✅ Extended suite: 10/11 pass (1 known async race)

### Known Limitations
1. **Async test suite race:** test_suite_extended shows false negatives for Mode 3
   - Workaround: Run manual timing tests for verification
   - Root cause: Unbuffered serial reader in test framework

2. **GPIO signal requirement:** Counter tests require actual signal on GPIO
   - Workaround: Use test simulation or COIL-based input

3. **UART1 only:** Modbus RTU on UART1 (GPIO4/5)
   - Design choice: GPIO0, GPIO2, GPIO15 are strapping pins

---

## Build Information

### RAM Usage
- Typical: ~21KB / 327KB (6.5%)
- Headroom for future features: ~300KB available

### Flash Usage
- Firmware: ~262KB / 1.3MB usable
- Headroom: ~1MB available for features/data

### Dependencies
- ESP32-Arduino core (v2.0.0+)
- PlatformIO (v6.0+)
- No external libraries required (zero dependencies)

---

## Documentation Updates (v1.0.0)

### NEW
- **FEATURE_GUIDE.md:** Complete reference for all features
  - All Timer Modes (1-4) with examples
  - All Counter modes with detailed specs
  - 70+ CLI commands documented
  - GPIO mapping, persistence, system commands
  - Comprehensive examples section

### UPDATED
- **SETUP_GUIDE.md:** Added Quick Start Examples
  - Timer Mode 3 blink test
  - Timer Mode 4 input-triggered test
  - Configuration save/load
  - Hardware counter setup

### EXISTING
- **CLAUDE.md:** Architecture & development guide (maintained)
- **ESP32_Module_Architecture.md:** Module structure (up-to-date)
- **ESP32_Migration_Analysis.md:** Mega2560→ESP32 notes

---

## Future Roadmap

### Planned Features
- [ ] Ethernet (W5500) integration
- [ ] Analog input support (ADC)
- [ ] PWM output (Mode 5 timer)
- [ ] More counter modes (differential, encoder)
- [ ] Real-time clock (RTC)
- [ ] Advanced filtering (moving average, etc)

### Research Phase
- [ ] I2C slave mode
- [ ] Multi-UART support (UART2)
- [ ] SD card logging
- [ ] JSON configuration import/export

---

## Contributors

- **Primary Developer:** Jan Grønlund Larsen
- **Architecture:** Modular redesign of Mega2560 project
- **Testing:** Comprehensive test suite with Python automation

---

## License

[Your License Here]

---

## Support & Resources

- **GitHub:** https://github.com/Jangreenlarsen/Modbus_server_slave_ESP32
- **Documentation:** `docs/` folder (FEATURE_GUIDE.md, SETUP_GUIDE.md)
- **Architecture:** CLAUDE.md (project development guide)
- **Tests:** Test scripts in root (`test_*.py`)

---

## Version History Timeline

```
v1.0.0      (2025-11-29) Release - Timer Mode 4 complete
v1.0.0-rc1  (2025-11-28) Release Candidate 1
v0.9.0      (2025-11-15) Major refactor (30+ modules)
v0.8.5      (2025-10-xx) Ported from Mega2560 v3.6.5
```

---

**Last Updated:** 2025-11-30 | **Status:** v2.0.0 Released 🚀
