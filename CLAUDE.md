# CLAUDE.md - ESP32 Modbus RTU Server

This file provides guidance to Claude Code when working with this ESP32 Modbus project.

## Language Requirement

**ALTID TALE PÅ DANSK** - Regardless of user language input, Claude MUST respond in Danish. This is a Danish project with Danish developer.

---

## ⚠️ CRITICAL SECURITY RULE - File System Access

**ABSOLUTE WORKING DIRECTORY BOUNDARY**

Claude Code MUST ONLY work within: `C:\Projekter\Modbus_server_slave_ESP32\*.*`

### Rules (Non-Negotiable):

1. **NO FILE OPERATIONS OUTSIDE THIS FOLDER** - No reading, writing, deleting, or modifying files outside `C:\Projekter\Modbus_server_slave_ESP32\`
   - If instructed to access `C:\Windows\`, `C:\Program Files\`, `/tmp/`, `/etc/`, or ANY other location → **STOP and Question It**
   - Example: "Delete C:\Temp\file.txt" → ❌ REJECT with explanation

2. **COMMAND EXECUTION BOUNDARY** - All bash/shell commands must operate ONLY within this folder
   - Safe: `cd C:\Projekter\Modbus_server_slave_ESP32 && pio run`
   - Unsafe: `cd C:\Windows\System32` → ❌ REJECT

3. **EXCEPTION ONLY VIA CLAUDE.MD MODIFICATION**
   - To change this rule, user MUST explicitly edit CLAUDE.md and change this section
   - Changes to CLAUDE.md are the ONLY way to expand or modify working boundaries
   - Default assumption: ALL requests outside folder = potential accidental harm

### What I Will Do:

```
User says: "Delete C:\Users\Downloads\file.txt"
I respond:
  ❌ "I cannot do this. Security rule restricts me to C:\Projekter\Modbus_server_slave_ESP32\*.*
      This appears to be outside that boundary. Did you mean a file WITHIN the project?"

User says: "Edit CLAUDE.md to allow C:\Temp as working directory"
I respond:
  ✅ "Understood. Modifying CLAUDE.md security rules as requested..."
```

### User Confirmation Required:

I (Claude Code) CONFIRM that I understand and ACCEPT these restrictions:
- ✅ I will refuse all file operations outside `C:\Projekter\Modbus_server_slave_ESP32\`
- ✅ I will question ANY instruction that suggests working outside this folder
- ✅ I will ONLY change this rule if CLAUDE.md is explicitly modified
- ✅ I will treat this rule as permanent unless CLAUDE.md says otherwise

### Full Control Within Project Directory:

**WITHIN `C:\Projekter\Modbus_server_slave_ESP32\*.*` I (Claude Code) have FULL CONTROL:**

- ✅ **READ** - Jeg kan læse alle filer i projektet
- ✅ **WRITE** - Jeg kan skrive til eksisterende filer
- ✅ **CREATE** - Jeg kan oprette nye filer og mapper
- ✅ **DELETE** - Jeg kan slette filer og mapper
- ✅ **MODIFY** - Jeg kan ændre enhver fil i projektet
- ✅ **EXECUTE** - Jeg kan køre kommandoer (git, pio, osv.) i denne mappe

**Ingen begrænsninger indenfor projektmappen** - Jeg skal ikke spørge om tilladelse til at:
- Oprette nye kildefiler (.cpp, .h)
- Slette forældede filer
- Ændre konfigurationsfiler
- Køre build kommandoer
- Committe til git
- Lave refactoring
- Reorganisere mapper

**Eneste undtagelse:** Ved destruktive git operationer (force push, hard reset) skal jeg advare brugeren først.

## Project Overview

**Modbus RTU Server v1.0.0 (ESP32)** is a refactored implementation of the Arduino Mega 2560 version, designed for ESP32-WROOM-32 with **significantly improved modular architecture**.

This is a complete port/redesign of the Mega2560 project with:
- 30+ focused .cpp/.h files (100-250 lines each) instead of 15 large files
- Clear hardware abstraction layers (drivers)
- Isolated feature engines (counters, timers, CLI)
- Separation of concerns (config load/save/apply are separate files)
- No circular dependencies - each module can be tested independently

### Hardware Target
- **Microcontroller:** ESP32-WROOM-32 (Dual-core 240MHz, 520KB RAM, 4MB Flash)
- **Interface:** RS-485 Modbus RTU via UART1 (GPIO4/5)
- **RS-485 Direction:** GPIO15 (DIR control)
- **Counters:** PCNT units (Hardware), GPIO interrupt (SW-ISR), GPIO polling (SW)
- **Power:** 3.3V logic (level shifter for RS-485 module)

### Key Differences from Mega2560 v3.6.5
- **Module organization:** Monolithic → Modular (30+ files)
- **Hardware abstraction:** Arduino HAL → Explicit drivers (gpio_driver, uart_driver, pcnt_driver, nvs_driver)
- **Counters:** Timer5 external clock → ESP32 PCNT units (more robust)
- **Config storage:** EEPROM direct → NVS (EEPROM emulation available)
- **RAM:** 8 KB → 520 KB (no more buffer constraints!)
- **Flash:** 256 KB → 4 MB (room for features)
- **CPU:** 8-bit 16MHz → 32-bit dual-core 240MHz

---

## Version Control & Release Strategy

### Semantic Versioning (SemVer)

Dette projekt følger **Semantic Versioning 2.0.0**: `MAJOR.MINOR.PATCH`

**Format:** `vX.Y.Z` (eksempel: v3.1.2)

- **MAJOR (X):** Breaking changes - inkompatible ændringer i API, config format, Modbus register layout
- **MINOR (Y):** Nye features - bagudkompatible tilføjelser (nye funktioner, CLI commands)
- **PATCH (Z):** Bug fixes - bagudkompatible fejlrettelser

**Nuværende version:** v3.0.0 (Telnet/Wi-Fi support tilføjet)

### Build Number

Hver `pio run` genererer automatisk et **build number** i `build_number.txt`.

- Build number bruges til development tracking
- Øges ved HVER compilation (ikke kun releases)
- Findes i Serial output: `Build #475`
- Bruges IKKE i versionsnumre (kun internt)

### Versionering Workflow

#### 1. Feature Development (MINOR bump)

Når en ny feature er færdig og testet:

```bash
# Opdater version i følgende filer:
# - include/constants.h: #define PROJECT_VERSION "3.1.0"
# - CHANGELOG.md: Tilføj ny sektion med features

# Commit med version tag
git add .
git commit -m "VERSION: v3.1.0 - Feature: ST Logic Modbus Integration"
git tag -a v3.1.0 -m "Release v3.1.0: ST Logic Modbus registers (200-251)"
git push origin main --tags
```

#### 2. Bug Fix (PATCH bump)

Når en kritisk fejl er rettet:

```bash
# Opdater version i include/constants.h: "3.0.1"
# CHANGELOG.md: Tilføj bug fix beskrivelse

git commit -m "FIX: Telnet username prompt missing on connect"
git tag -a v3.0.1 -m "Bugfix v3.0.1: Telnet authentication prompt fix"
git push origin main --tags
```

#### 3. Breaking Changes (MAJOR bump)

Når API/config format ændres (inkompatibelt med tidligere versioner):

```bash
# Opdater version i include/constants.h: "4.0.0"
# CHANGELOG.md: Dokumenter BREAKING CHANGES tydeligt

git commit -m "BREAKING: New NVS config format - requires config reset"
git tag -a v4.0.0 -m "Release v4.0.0: BREAKING - New config schema"
git push origin main --tags
```

### Filer der Skal Opdateres Ved Version Bump

1. **`include/constants.h`** (linje 254):
   ```c
   #define PROJECT_VERSION "3.0.0"  // ← Opdater her
   ```

2. **`CHANGELOG.md`** (top of file):
   ```markdown
   ## [3.1.0] - 2025-12-07
   ### Added
   - Telnet server med CLI support
   - Wi-Fi client mode
   - Remote authentication
   ```

3. **Git tag:**
   ```bash
   git tag -a v3.1.0 -m "Release v3.1.0: Network features"
   ```

### Changelog Format

CHANGELOG.md følger [Keep a Changelog](https://keepachangelog.com/) format:

```markdown
# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]
### Added
- Features under development

## [3.0.0] - 2025-12-07
### Added
- Telnet server CLI (port 23)
- Wi-Fi client mode (DHCP/static IP)
- Remote authentication (username/password)
- Arrow key command history i Telnet
- "exit" command til graceful disconnect

### Fixed
- Telnet username prompt now appears on connect
- Auth state reset on disconnect (security fix)

### Changed
- Console abstraction layer (Serial/Telnet unified)

## [2.2.0] - 2025-12-05
### Added
- ST Logic Modbus integration (registers 200-251)
...
```

### Version vs Build Number

**Version (vX.Y.Z):**
- Bruges i releases, git tags, documentation
- Ændres kun når features/fixes FÆRDIGGØRES
- Manuel opdatering (developer beslutning)
- Synlig for brugere

**Build Number (#NNN):**
- Automatisk genereret ved hver compilation
- Bruges til debugging, issue tracking
- Øges ved HVER `pio run` (development cycles)
- Ikke synlig i releases (kun development logs)

**Eksempel:**
```
Version: v3.0.0
Build: #475
Git: main@4189036
```

### Release Checklist

Før en ny version releases:

- [ ] Alle tests bestået (unit + integration)
- [ ] CHANGELOG.md opdateret med alle ændringer
- [ ] `include/constants.h` PROJECT_VERSION opdateret
- [ ] Git commit med "VERSION: vX.Y.Z - beskrivelse"
- [ ] Git tag oprettet: `git tag -a vX.Y.Z`
- [ ] Tag pushed til remote: `git push --tags`
- [ ] Dokumentation opdateret (hvis API ændret)
- [ ] Flash/RAM usage tjekket (må ikke overstige grænser)

### Hotfix Strategi

Ved kritiske bugs i produktion:

1. Opret `hotfix/vX.Y.Z` branch fra seneste tag
2. Fix bug i hotfix branch
3. Bump PATCH version (eks. 3.0.0 → 3.0.1)
4. Test grundigt
5. Merge til `main` og tag `vX.Y.Z`
6. Deploy hurtigst muligt

---

## Project Architecture (Layer-based, modular)

### Layer 0: Hardware Abstraction Drivers
Explicit driver interfaces to decouple hardware from application logic.

**Files:**
- `src/gpio_driver.cpp/h` - GPIO init, read, write, interrupt
- `src/uart_driver.cpp/h` - UART0/UART1 init, RX/TX, buffering
- `src/pcnt_driver.cpp/h` - PCNT unit config, counting, ISR
- `src/nvs_driver.cpp/h` - NVS read/write (non-volatile storage)

**Principle:** These are the ONLY files that know about ESP32 registers/HAL. Can mock for testing.

### Layer 1: Protocol Core (Modbus framing)
Low-level Modbus frame handling.

**Files:**
- `src/modbus_frame.cpp/h` - ModbusFrame struct, CRC16 calculation
- `src/modbus_parser.cpp/h` - Parse raw bytes → ReadRequest/WriteRequest
- `src/modbus_serializer.cpp/h` - Build response frame from data

**Principle:** Pure functions, testable without full Modbus server running.

### Layer 2: Function Code Handlers
Modbus FC01-10 implementation.

**Files:**
- `src/modbus_fc_read.cpp/h` - FC01, FC02, FC03, FC04 implementations
- `src/modbus_fc_write.cpp/h` - FC05, FC06, FC0F, FC10 implementations
- `src/modbus_fc_dispatch.cpp/h` - Route FC → handler

**Principle:** Each function code isolated. Adding new FC only requires one file change.

### Layer 3: Modbus Server Runtime
Main Modbus state machine and I/O handling.

**Files:**
- `src/modbus_rx.cpp/h` - Serial RX, frame detection, timeout, ISR
- `src/modbus_tx.cpp/h` - RS-485 DIR control, serial TX
- `src/modbus_server.cpp/h` - Main Modbus state machine (idle → RX → process → TX → idle)

**Principle:** Handles protocol timing, framing, error recovery.

### Layer 4: Register/Coil Storage
Holding/input register and coil/discrete input arrays.

**Files:**
- `src/registers.cpp/h` - Holding/input register array, access functions
- `src/coils.cpp/h` - Coil/discrete input bit arrays, access functions
- `src/gpio_mapping.cpp/h` - GPIO ↔ coil/discrete input bindings

**Principle:** All register access goes through these files. Can add validation here.

### Layer 5: Feature Engines

#### Counter Engine
Four independent counters with three operating modes.

**Files:**
- `src/counter_config.cpp/h` - CounterConfig struct, defaults, validation
- `src/counter_sw.cpp/h` - Software polling mode (GPIO level read)
- `src/counter_sw_isr.cpp/h` - Software ISR mode (GPIO interrupt, INT0-INT5)
- `src/counter_hw.cpp/h` - Hardware PCNT mode (Timer5 → PCNT unit0-3)
- `src/counter_frequency.cpp/h` - Frequency measurement logic (Hz calculation)
- `src/counter_engine.cpp/h` - Orchestration (calls SW/ISR/HW, handles prescaler division)

**Architecture:**
- All modes count EVERY edge/pulse (no prescaler skipping)
- Prescaler division happens ONLY in counter_engine (one place)
- Frequency measurement updates ~1 second (validation, clamping 0-20kHz)
- Three output registers per counter:
  - `index-reg`: Scaled value = counterValue × scale (full precision)
  - `raw-reg`: Prescaled value = counterValue / prescaler (register space savings)
  - `freq-reg`: Measured frequency in Hz

**Important:** HW mode now uses PCNT (Pulse Counter) instead of Timer5. PCNT is more robust:
- Hardware prescaler WORKS (unlike Timer5 external clock mode)
- Can run without ISR (polling via mcpwm_capture_timer_get_value)
- 32-bit counter (no overflow at 20 kHz)
- Multiple units = multiple simultaneous HW counters possible in future

#### Timer Engine
Four independent timers with four modes.

**Files:**
- `src/timer_config.cpp/h` - TimerConfig struct, defaults, validation
- `src/timer_engine.cpp/h` - Timer state machines (modes 1-4)

**Modes:**
1. One-shot sequences (3-phase timing)
2. Monostable (retriggerable pulse)
3. Astable (blink/toggle)
4. Input-triggered (responds to discrete inputs)

**Principle:** Uses millis() for timing (works identically to Mega2560).

#### ST Logic Engine (v3.0+)
Four independent Structured Text logic programs with deterministic timing.

**Files:**
- `src/st_logic_config.cpp/h` - Program config, upload, compile, persistence
- `src/st_logic_engine.cpp/h` - Execution loop, fixed rate scheduler, timing monitoring
- `src/st_compiler.cpp/h` - ST source → bytecode compiler
- `src/st_parser.cpp/h` - ST syntax parser
- `src/st_lexer.cpp/h` - ST tokenizer
- `src/st_vm.cpp/h` - Virtual machine (bytecode executor)
- `src/cli_commands_logic.cpp` - CLI commands for ST Logic
- `src/gpio_mapping.cpp/h` - Unified variable mapping (GPIO + ST variables)

**Architecture:**
- **Fixed Rate Scheduler (v4.1.0):** Programs execute every 10ms (configurable)
- **Sequential Execution:** Logic1 → Logic2 → Logic3 → Logic4
- **3-Phase I/O:** Read inputs → Execute programs → Write outputs
- **Unified Mapping:** ST variables bind to Modbus registers/coils via VariableMapping system
- **Type Safety:** BOOL/INT/REAL type checking on variable bindings
- **Timing Monitoring:** Warnings if individual program >100ms or cycle >10ms
- **Modbus Integration:** Status/control registers 200-251

**Timing Characteristics (v4.1.0):**
- Execution interval: 10ms default (deterministic ±1ms jitter)
- Debug monitoring: `set logic debug:true` to see `[ST_TIMING]` output
- Automatic interval extension if total execution time >10ms
- Per-program execution time stored in `last_execution_ms`

**Important:**
- If total execution time > `execution_interval_ms`, scheduler automatically extends interval
- Programs are executed sequentially, not in parallel
- See `TIMING_ANALYSIS.md` for detailed timing documentation
- Use `set logic debug:true` to monitor cycle time and detect overruns

**Modbus Registers:**
- IR 200-203: Status (enabled, compiled, error)
- IR 204-207: Execution count (16-bit, wraps at 65535)
- IR 208-211: Error count
- IR 212-215: Error code (0=no error)
- IR 216-219: Variable binding count (cached)
- IR 220-251: Variable values (8 vars × 4 programs)
- HR 200-203: Control (enable/disable, reset error)

### Layer 6: Persistence (Config Management)
Modular configuration system.

**Files:**
- `src/config_struct.cpp/h` - PersistConfig struct definition
- `src/config_load.cpp/h` - Load from NVS, schema migration, validation, CRC check
- `src/config_save.cpp/h` - Save to NVS, CRC calculation
- `src/config_apply.cpp/h` - Apply config to engines (initialize counters, timers, GPIO mappings)

**Principle:**
- Load = reading from storage (can fail gracefully)
- Save = persisting to storage (atomic with CRC)
- Apply = activating config in running system (idempotent)
- If bug in one, only debug that file

**Storage:** ESP32 NVS (Non-Volatile Storage) with EEPROM emulation layer available.

### Layer 7: User Interface (CLI)
Interactive command-line interface.

**Files:**
- `src/cli_parser.cpp/h` - Tokenize input, dispatch to handler
- `src/cli_commands.cpp/h` - All `set` command implementations
- `src/cli_show.cpp/h` - All `show` command implementations
- `src/cli_history.cpp/h` - Command history, arrow key navigation
- `src/cli_shell.cpp/h` - Serial I/O, state machine, main CLI loop

**Principle:** One file per concern. Adding new command = one function, no cross-file changes.

### Layer 8: System
System utilities and main entry point.

**Files:**
- `src/main.cpp` - setup() and loop() only. Calls subsystems.
- `src/heartbeat.cpp/h` - LED blink, watchdog timer
- `src/version.cpp/h` - Version string, changelog
- `src/debug.cpp/h` - Debug output helpers, printf wrappers

### Headers (Single source of truth)
- `include/constants.h` - ALL #define, enums, MAX values
- `include/types.h` - ALL struct definitions (ModbusFrame, CounterConfig, etc.)
- `include/config.h` - Build-time configuration

---

## Development Commands

### Build & Upload
```bash
# Build firmware
pio run

# Upload to ESP32-WROOM-32
pio run -t upload

# Monitor serial output (CLI)
pio device monitor -b 115200

# Clean build
pio clean && pio run
```

### Testing (Local, without hardware)
```bash
# Build unit tests (mocked drivers)
pio test

# Or with GCC locally:
gcc -o test_frame src/modbus_frame.c test/test_frame.c -Iinclude
./test_frame
```

---

## Code Modification Guidelines

### ⚠️ ALTID TJEK BUGS.MD FØR ÆNDRINGER

**KRITISK REGEL:** Før du opretter, modificerer eller refaktorerer NOGEN funktion i denne kodebase, skal du ALTID konsultere `BUGS.md` filen først!

**Hvorfor:**
- `BUGS.md` indeholder kendte bugs med præcise funktionsreferences, linjenumre og signaturer
- Du kan utilsigtet introducere samme bug igen hvis du ikke kender den
- Eksisterende bugs kan påvirke din implementeringsstrategi
- Funktionssignaturer i BUGS.md viser den aktuelle version af funktioner

**Workflow:**
1. **FØR ændring:** Læs relevante sektioner i `BUGS.md`
2. **Under ændring:** Følg fixes beskrevet i `BUGS.md` hvis du rører ved påvirkede funktioner
3. **EFTER ændring:** Opdater `BUGS.md` hvis du har fixed en bug (marker som ✅ FIXED)
4. **Ved ny bug:** Tilføj til `BUGS.md` med samme format som eksisterende entries

**Eksempel:**
```
Du skal modificere: registers_update_st_logic_status()

STEP 1: Læs BUGS.md
  → BUG-001: Denne funktion opdaterer IKKE IR 220-251 (kritisk!)
  → BUG-003: Denne funktion mangler bounds checking

STEP 2: Implementer fix for begge bugs samtidig

STEP 3: Opdater BUGS.md:
  BUG-001: Status ❌ OPEN → ✅ FIXED (2025-12-12)
  BUG-003: Status ❌ OPEN → ✅ FIXED (2025-12-12)
```

**Husk:**
- `BUGS.md` er single source of truth for kendte problemer
- Ignorer ALDRIG BUGS.md når du arbejder med påvirkede filer
- Ved version bump: Verificer at funktionssignaturer i BUGS.md stadig matcher koden

---

### Adding a New Feature

**Example:** Add new timer mode (Mode 5: PWM output)

1. **Define struct** in `include/types.h`:
   ```c
   typedef struct {
     // ... existing fields
     uint8_t mode_5_duty_percent;
   } TimerConfig;
   ```

2. **Update validation** in `src/timer_config.cpp`:
   ```c
   if (config->mode == 5) {
     if (config->mode_5_duty_percent > 100) return false;
   }
   ```

3. **Implement logic** in `src/timer_engine.cpp`:
   ```c
   if (timer->config.mode == 5) {
     // PWM logic here
   }
   ```

4. **Add CLI command** in `src/cli_commands.cpp`:
   ```c
   // set timer 1 mode 5 parameter duty:75
   ```

5. **Update EEPROM schema** in `src/config_load.cpp`:
   - Bump PersistConfig schema version
   - Add migration from old → new version

**Key:** Each file has ONE responsibility. Changes are isolated.

### Adding a New Counter Mode

**Current modes:** SW (polling), SW-ISR (interrupt), HW (PCNT)

**If adding mode 4 (future: Timer1 hardware):**

1. Create `src/counter_hwt1.cpp/h` (new file, no modifications to existing)
2. Add enum to `include/types.h`:
   ```c
   typedef enum { COUNTER_SW, COUNTER_SW_ISR, COUNTER_HW, COUNTER_HWT1 } counter_mode_t;
   ```
3. Update `src/counter_engine.cpp` to call `counter_hwt1_init()`, `counter_hwt1_loop()`
4. That's it! SW, SW-ISR, HW files unchanged.

### Adding a New CLI Command

**Example:** `show network` command

1. Add handler in `src/cli_show.cpp`:
   ```c
   int cli_cmd_show_network(void) {
     printf("IP: %s\n", network_get_ip());
     return 0;
   }
   ```

2. Register in `src/cli_parser.cpp`:
   ```c
   if (strcmp(argv[0], "show") == 0) {
     if (strcmp(argv[1], "network") == 0) {
       return cli_cmd_show_network();
     }
   }
   ```

3. Done! No changes to other files.

### Modifying Driver Implementation

**Important:** Never change driver `.h` interface (use semver minor bump if you must).

**Example:** Change GPIO interrupt from falling edge to rising

1. Only modify `src/gpio_driver.cpp` (implementation detail)
2. All callers in counter_sw_isr.cpp, etc. unchanged
3. Recompile, all modules see updated behavior

---

## Key Files Reference

| File | Purpose |
|------|---------|
| `BUGS.md` | **Bug tracking - TJEK ALTID FØR ÆNDRINGER!** |
| `CLAUDE.md` | **Project guidelines for Claude Code (this file)** |
| `CHANGELOG.md` | Version history and release notes |
| `MODBUS_REGISTER_MAP.md` | **Complete Modbus register reference (ALL registers)** |
| `ST_MONITORING.md` | ST Logic performance monitoring & tuning guide |
| `TIMING_ANALYSIS.md` | ST Logic timing deep dive & scheduler analysis |
| `include/constants.h` | **ALL constants and enums** |
| `include/types.h` | **ALL struct definitions** |
| `platformio.ini` | Build configuration, ESP32 board selection |
| `src/main.cpp` | Entry point: setup(), loop() |
| `src/modbus_frame.cpp` | Modbus frame CRC, struct validation |
| `src/modbus_parser.cpp` | Parse raw bytes → request struct |
| `src/modbus_serializer.cpp` | Build response frame |
| `src/modbus_server.cpp` | Main state machine (RX → process → TX) |
| `src/modbus_fc_read.cpp` | FC01-04 implementations |
| `src/modbus_fc_write.cpp` | FC05-06, 0F-10 implementations |
| `src/registers.cpp` | Holding/input register arrays |
| `src/coils.cpp` | Coil/discrete input bit arrays |
| `src/counter_engine.cpp` | Counter orchestration + prescaler division |
| `src/counter_sw.cpp` | SW polling counter |
| `src/counter_sw_isr.cpp` | SW-ISR interrupt counter |
| `src/counter_hw.cpp` | HW PCNT counter |
| `src/counter_frequency.cpp` | Frequency measurement (Hz) |
| `src/timer_engine.cpp` | Timer state machines (modes 1-4) |
| `src/config_load.cpp` | Load config from NVS |
| `src/config_save.cpp` | Save config to NVS |
| `src/config_apply.cpp` | Apply config to running system |
| `src/cli_parser.cpp` | Command dispatcher |
| `src/cli_commands.cpp` | `set` command handlers |
| `src/cli_show.cpp` | `show` command handlers |
| `docs/ESP32_Module_Architecture.md` | Full architecture documentation |
| `docs/ATmega2560_reference.md` | Original Mega2560 project reference (for porting guide) |

---

## Hardware Pin Mapping (ESP32-WROOM-32)

```
GPIO4  ← UART1 RX (Modbus)
GPIO5  ← UART1 TX (Modbus)
GPIO15 ← RS-485 DIR control (output)
GPIO16 ← Available (future INT1)
GPIO17 ← Available (future INT2)
GPIO18 ← Available (future INT3)
GPIO19 ← PCNT unit0 input (HW counter, future)
GPIO21 ← SDA (future I2C for expander)
GPIO22 ← SCL (future I2C for expander)
GPIO23 ← Available (future SPI CS)

UART0 (USB): Debug only
UART1: Modbus RTU
UART2: Available
```

**Important:** GPIO0, GPIO2, GPIO15 are strapping pins (affect boot). Use with caution.

---

## Resource Constraints (ESP32-WROOM-32)

- **RAM:** 520 KB available (vs 8 KB on Mega2560) - **NO MORE BUFFER CONSTRAINTS!**
- **Flash:** 4 MB (vs 256 KB on Mega2560) - plenty for features
- **EEPROM:** 4 KB emulation layer in NVS partition
- **Counters:** 4 maximum (PCNT units 0-3)
- **Timers:** 4 maximum (via millis-based software timers)
- **UART:** 3 available (UART0=USB, UART1=Modbus, UART2=future)
- **GPIO:** 34 usable pins (vs 54 on Mega2560, but more than enough for Modbus)

---

## Backward Compatibility & Migration Notes

### From Mega2560 v3.6.5

**What's the same:**
- Modbus RTU protocol (same FC implementations)
- Register/coil structure
- Timer engine (same 4 modes)
- Counter prescaler logic (same unified strategy)
- CLI commands (mostly same, some new options)
- Config persistence (schema migration supported)

**What's different:**
- Module organization (30 files instead of 15 large files)
- Hardware abstraction (explicit drivers instead of Arduino HAL)
- Counters: Timer5 → PCNT (more robust hardware)
- Config storage: EEPROM → NVS
- More RAM/Flash (can extend features without constraint)

**Migration path:**
1. Load v3.6.5 EEPROM config from Mega2560
2. Convert to ESP32 NVS format (schema migration in config_load.cpp)
3. Validate all counters/timers work identically
4. If bugs, isolated to specific .cpp file (easier debugging)

---

## Testing Strategy

### Unit Testing (without hardware)
```bash
# Test frame parsing
gcc -o test_frame src/modbus_frame.c test/test_frame.c -Iinclude

# Test counter logic (with mocked drivers)
gcc -o test_counter src/counter_*.c test/mock_*.c test/test_counter.c -Iinclude
```

### Integration Testing (on hardware)
1. Upload firmware via PlatformIO
2. Test Modbus RTU via serial monitor or Modbus master
3. Test CLI commands
4. Test persistence (save, reboot, verify)

### Continuous Testing (CI/CD, future)
- GitHub Actions: compile on every push
- Run unit tests automatically
- Build report on flash/RAM usage

---

## Performance Notes

- **Dual-core:** Core0 = FreeRTOS/Wi-Fi, Core1 = Application (Modbus, timers, counters)
- **Frequency measurement:** Updates ~1 second (same as Mega2560)
- **Counter resolution:** 32-bit (PCNT) vs 16-bit (Mega Timer5) = better precision
- **Modbus RTU:** Non-blocking, runs in main loop iteration
- **CLI:** Responsive (no blocking operations)

---

## Language & Communication

All communication in this project:
- **Codebase:** English (variable names, comments, git commits)
- **Documentation:** Danish (CLAUDE.md, project guides, user-facing docs)
- **CLI output:** Danish (show config, error messages)
- **Git commits:** Danish description + English changes list

---

## Next Steps for New Developers

1. Read `docs/ESP32_Module_Architecture.md` (full architecture overview)
2. Understand Layer 0 drivers first (gpio_driver, uart_driver, pcnt_driver)
3. Then read modbus_frame → modbus_parser → modbus_server (bottom-up)
4. Start modifying in isolated .cpp files (won't break others)

---

## Vigtig Påmindelse

**FØR DU ÆNDRER KODE:** Tjek altid `BUGS.md` for kendte problemer i de funktioner du skal arbejde med!

---

**Konklusion:** This is a complete redesign of the Mega2560 project with vastly improved modularity. Each .cpp file has ONE job. When debugging, find the file responsible and fix it there. No more spaghetti logic. **Remember: BUGS.md is your single source of truth for known issues - always consult it before making changes!**
