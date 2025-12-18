# Modbus RTU Server (ESP32)

**Version:** v4.2.5 | **Status:** Production-Ready | **Platform:** ESP32-WROOM-32

En komplet, modulær **Modbus RTU Server** implementation til ESP32-WROOM-32 mikrocontroller med avancerede features inklusiv ST Structured Text Logic programmering med **performance monitoring**, Wi-Fi netværk, telnet CLI interface, og **komplet Modbus register dokumentation**.

---

## 🚀 Features

### Core Modbus RTU Protocol
- **Modbus RTU Function Codes:** FC01, FC02, FC03, FC04, FC05, FC06, FC0F (15), FC10 (16)
  - FC01: Read Coils
  - FC02: Read Discrete Inputs
  - FC03: Read Holding Registers
  - FC04: Read Input Registers
  - FC05: Write Single Coil
  - FC06: Write Single Register
  - FC0F: Write Multiple Coils
  - FC10: Write Multiple Registers
- **Configurable Slave ID:** 1-247 (default: 1)
- **Configurable Baudrate:** 300-115200 bps (default: 9600)
- **256 Holding Registers** (addresser 0-255)
- **256 Input Registers** (addresser 0-255)
- **256 Coils** (bit-addressable 0-255, packed i 32 bytes)
- **256 Discrete Inputs** (bit-addressable 0-255, packed i 32 bytes)
- **CRC16-CCITT Validation** på alle frames
- **Hardware RS-485 Support** med GPIO15 direction control
- **Timeout Handling:** 3.5 character times (baudrate-adaptive)
- **Error Detection:** CRC mismatch, illegal function, illegal address

### Counter Engine (4 Uafhængige Counters)
Hver counter har **3 hardware modes:**

#### **Mode 1: Software Polling (SW)**
- GPIO pin læses i main loop (~1ms rate)
- Edge detection via level change tracking
- CPU overhead: lav (polling)
- Max frequency: ~500Hz (limited by loop rate)
- Debounce: software (konfigurerbar ms delay)

#### **Mode 2: Software ISR (SW-ISR)**
- GPIO interrupt-driven counting
- ESP32 GPIO interrupt på INT0-5
- Edge detection: rising, falling, or both
- Max frequency: ~10kHz
- Debounce: hardware + software

#### **Mode 3: Hardware PCNT (HW)**
- ESP32 Pulse Counter Unit (PCNT)
- Hardware counting uden CPU overhead
- 32-bit counter range
- Max frequency: 40MHz (hardware limit)
- Prescaler: hardware support (1-65535)
- No debounce needed (hardware filters)

**Counter Features:**
- **Prescaler:** 1-65535 (deler counter value før output)
- **Scale Factor:** float multiplier (0.0001-10000.0)
- **Bit Width:** 8, 16, 32, eller 64-bit output (multi-word support)
- **Direction:** up eller down counting
- **Frequency Measurement:** 0-20kHz med 1-sekund update rate
- **Compare Feature (v4.2.4):**
  - Threshold detection med edge detection (≥, >, ==)
  - Rising edge trigger (kun sæt ved threshold crossing)
  - Runtime modificerbar threshold via Modbus
  - Auto-reset on read (ctrl-reg eller index-reg)
  - Output til control register bit 4
- **Register Outputs (v4.2.4 - Smart Defaults):**
  - Counter 1: HR100-114 (20 registers total)
  - Counter 2: HR120-134
  - Counter 3: HR140-154
  - Counter 4: HR160-174
  - **Per counter (32-bit example):**
    - HR100-101: Index (scaled value, 2 words)
    - HR104-105: Raw (prescaled value, 2 words)
    - HR108: Frequency (Hz)
    - HR109: Overload flag
    - HR110: Control register (bit7=running, bit4=compare-match)
    - HR111-112: Compare value (2 words, runtime writable)

### Timer Engine (4 Uafhængige Timers)
Hver timer har **4 modes:**

#### **Mode 1: One-shot (3-phase sequence)**
- Sekventiel 3-phase timing
- Phase 1: duration_ms, output_state
- Phase 2: duration_ms, output_state
- Phase 3: duration_ms, output_state
- Auto-stop efter phase 3
- Trigger via control register bit 1

#### **Mode 2: Monostable (Retriggerable Pulse)**
- Single pulse output
- Retriggerable (pulse extends on new trigger)
- Rest state og pulse state konfigurerbar
- Pulse duration: 1ms - 4294967295ms (49 days max)
- Trigger via control register bit 1

#### **Mode 3: Astable (Oscillator/Blink)**
- Kontinuerlig toggle mellem ON og OFF states
- ON duration og OFF duration konfigurerbar uafhængigt
- Start/stop via control register
- Perfekt til LED blink, PWM simulation, watchdog

#### **Mode 4: Input-Triggered (Edge Detection)**
- Monitorer discrete input for edge
- Trigger edge: rising (0→1) eller falling (1→0)
- Delay efter edge detection (0ms - 4294967295ms)
- Output level efter delay
- Auto-reset eller hold state (konfigurerbar)

**Timer Features:**
- **Output:** Coil register (0-255)
- **Control Register:** Start (bit 1), Stop (bit 2), Reset (bit 0)
- **Timing Precision:** ±1ms (millis() baseret)
- **Status Readback:** current phase, running state, output state
- **Persistent Configuration:** gemmes til NVS

### ST Logic Programming (Structured Text) - v4.1.0
4 uafhængige logic programmer med IEC 61131-3 ST syntax og **advanced performance monitoring**:

**Language Features:**
- **Variable Types:** INT, BOOL, REAL (16-bit, 1-bit, float)
- **Operators:** +, -, *, /, MOD, AND, OR, NOT, XOR
- **Comparisons:** =, <>, <, >, <=, >=
- **Control Flow:** IF/THEN/ELSIF/ELSE, WHILE, FOR, REPEAT/UNTIL
- **Variable Sections:** VAR_INPUT, VAR_OUTPUT, VAR (persistent)
- **Comments:** (* multi-line *) og // single-line
- **Built-in Functions:** ABS(), SQRT(), MIN(), MAX()

**Compiler & Runtime:**
- **Bytecode Compilation:** Real-time compilation ved upload
- **Zero Interpreter Overhead:** Direct bytecode execution
- **Fixed Rate Scheduler:** Deterministic 10ms execution cycle (±1ms jitter)
- **Dynamic Interval Control:** Adjust execution interval (10/20/25/50/75/100 ms)
- **Variable Binding:** ST variables ↔ Modbus registers/coils (with type checking)
- **Program Size:** Max 2KB source code per program
- **Bytecode Size:** Max 1KB compiled bytecode per program
- **Error Handling:** Compile errors with line numbers, runtime timeout protection

**Performance Monitoring (v4.1.0):**
- **Execution Statistics:** Min/Max/Avg execution time (microsecond precision)
- **Overrun Tracking:** Counts executions where time > target interval
- **Global Cycle Stats:** Min/Max cycle time, total cycles executed
- **Performance Ratings:** EXCELLENT/GOOD/ACCEPTABLE/POOR with auto recommendations
- **CLI Commands:**
  - `show logic stats` - Display all program performance statistics
  - `show logic X timing` - Detailed analysis for specific program
  - `set logic stats reset [all|cycle|1-4]` - Reset statistics
  - `set logic interval:X` - Change execution interval dynamically
- **Modbus Access:** Read statistics from IR 252-293, control interval via HR 236-237

**Variable I/O Binding:**
```
ST Variable ↔ Holding Register (read/write)
ST Variable ↔ Input Register (read-only)
ST Variable ↔ Coil (read/write bit)
ST Variable ↔ Discrete Input (read-only bit)
```

**CLI Commands:**
```bash
# Program Management
set logic <id> upload            # Upload ST source code
set logic <id> enabled:true      # Enable program execution
set logic <id> enabled:false     # Disable program
set logic <id> delete            # Delete program
set logic <id> bind <var> reg:X  # Bind variable to register

# Performance Monitoring (v4.1.0)
show logic stats                 # All programs performance stats
show logic <id> timing           # Detailed timing analysis
show logic <id>                  # Specific program details
set logic stats reset all        # Reset all statistics
set logic stats reset cycle      # Reset global cycle stats
set logic stats reset <1-4>      # Reset specific program stats

# Execution Control (v4.1.0)
set logic interval:10            # Set execution interval (10/20/25/50/75/100 ms)
set logic debug:true             # Enable debug output
set logic debug:false            # Disable debug output
```

### Networking Features (v3.0+)

#### Wi-Fi Client Mode
- **DHCP Support:** Automatic IP assignment
- **Static IP Support:** Manual IP, gateway, netmask, DNS configuration
- **WPA/WPA2 Authentication:** Password-protected networks
- **SSID:** Max 32 characters
- **Password:** 8-63 characters (WPA2 requirement)
- **Connection Status:** Real-time monitoring via `show wifi`
- **Auto-Reconnect:** Automatic reconnection on disconnect
- **IP Display:** Shows assigned IP (DHCP or static)

#### Telnet Server
- **Port:** 23 (default, konfigurerbar)
- **Authentication:** Username/password (optional)
  - Default: empty username, empty password (disabled)
  - Configurable via CLI: `set wifi telnet-user`, `set wifi telnet-pass`
- **Concurrent Connections:** 1 simultaneous client
- **Line Editing:** Arrow keys (←→) for cursor movement
- **Insert Mode:** Character insertion without overwriting
- **Command History:** Arrow keys (↑↓) for history navigation
- **Echo Control:** Configurable remote echo (on/off)
- **Graceful Disconnect:** `exit` command
- **Session Timeout:** Configurable inactivity timeout

#### Network Configuration Persistence
- **NVS Storage:** All network settings saved to non-volatile storage
- **CRC Validation:** Data integrity check on load
- **Schema Migration:** Automatic backward compatibility
- **Configuration Commands:**
```bash
set wifi ssid <name>           # Set Wi-Fi SSID
set wifi password <pass>       # Set Wi-Fi password
set wifi dhcp on|off           # Enable/disable DHCP
set wifi ip <addr>             # Static IP (e.g., 192.168.1.100)
set wifi gateway <addr>        # Gateway IP
set wifi netmask <addr>        # Netmask (e.g., 255.255.255.0)
set wifi dns <addr>            # DNS server
set wifi telnet enable         # Enable telnet server
set wifi telnet-port <port>    # Set telnet port
set wifi telnet-user <user>    # Set telnet username
set wifi telnet-pass <pass>    # Set telnet password
save                           # Persist to NVS
```

### CLI Interface (Command Line Interface)

#### Serial Console (USB)
- **Port:** UART0 (USB CDC)
- **Baudrate:** 115200 bps (fixed)
- **Line Ending:** CR+LF (\\r\\n)
- **Buffer:** 256 bytes input buffer
- **Echo:** Always ON (local echo)

#### Telnet Console (Network)
- **Port:** 23 (default)
- **Authentication:** Username/password
- **Line Editing:** Full cursor control
- **Remote Echo:** Configurable (on/off)
- **Buffer:** 256 bytes input buffer

#### CLI Commands (~46 funktioner)

**Configuration Commands:**
```bash
set id <1-247>                 # Set Modbus slave ID
set baud <300-115200>          # Set Modbus baudrate
set hostname <name>            # Set system hostname (max 31 chars)
set echo on|off                # Enable/disable remote echo
save                           # Save config to NVS
load                           # Load config from NVS (auto on boot)
reset                          # Reset to factory defaults
```

**Show Commands (Status & Monitoring):**
```bash
show config                    # Full system configuration
show version                   # Firmware version & build info
show counters                  # All counters status (table)
show counter <1-4>             # Specific counter detailed status
show timers                    # All timers status (table)
show timer <1-4>               # Specific timer detailed status
show logic                     # All ST Logic programs status
show logic <id>                # Specific program details
show logic <id> code           # Show compiled bytecode
show registers [start] [count] # Holding registers (0-255)
show inputs [start] [count]    # Input registers (0-255)
show coils [start] [count]     # Coils (0-255)
show gpio                      # GPIO mappings
show wifi                      # Wi-Fi connection status
show debug                     # Debug flags status
show echo                      # Echo status
```

**Counter Commands (v4.2.4):**
```bash
# Configure counter (register addresses AUTO-ASSIGNED)
# NOTE: Register addresses are NOW READ-ONLY smart defaults (v4.2.4+)
set counter <id> mode <1-3> hw-mode:<sw|sw-isr|hw> \
  edge:<rising|falling|both> prescaler:<1-65535> \
  scale:<float> bit-width:<8|16|32|64> \
  direction:<up|down>

# Mode-specific configuration
set counter <id> mode 1 input-dis:<idx>        # SW mode: discrete input
set counter <id> mode 2 interrupt-pin:<gpio>   # SW-ISR mode: GPIO interrupt
set counter <id> mode 3 hw-gpio:<gpio>         # HW mode: PCNT unit

# Compare feature (v4.2.4 - BUG-029, BUG-030)
set counter <id> compare:on compare-value:<val> compare-mode:<0|1|2>
  # Mode 0: ≥ (greater-or-equal, rising edge trigger)
  # Mode 1: > (strictly greater, rising edge trigger)
  # Mode 2: === (exact match, rising edge trigger)

# Control commands
set counter <id> control reset         # Reset counter to 0
set counter <id> control running:on    # Start counter
set counter <id> control running:off   # Stop counter

# Enable/disable (Cisco-style)
set counter <id> enable:<on|off>       # Enable/disable counter
no set counter <id>                    # Delete counter (reset to defaults)

# Read compare value from Modbus (v4.2.4 - BUG-030)
read reg <base+11> <words>             # Counter 1: HR111, Counter 2: HR131, etc.
write reg <base+11> value <threshold>  # Modify compare threshold runtime
```

**Register Layout (v4.2.4 - Smart Defaults, BUG-028):**
```
Counter 1: HR100-114 (index+raw+freq+overload+ctrl+compare, 5 reserved)
Counter 2: HR120-134
Counter 3: HR140-154
Counter 4: HR160-174

Example (Counter 1, 32-bit):
  HR100-101: Index (scaled, 2 words)
  HR104-105: Raw (prescaled, 2 words)
  HR108:     Frequency (Hz, 1 word)
  HR109:     Overload flag (1 word)
  HR110:     Control (bit7=running, bit4=compare-match, 1 word)
  HR111-112: Compare value (2 words, writable via FC06/FC16)
```

**Timer Commands:**
```bash
# Mode 1: One-shot
set timer <id> mode 1 parameter \
  phase1-duration:<ms> phase1-output:<0|1> \
  phase2-duration:<ms> phase2-output:<0|1> \
  phase3-duration:<ms> phase3-output:<0|1> \
  output-coil:<0-255> ctrl-reg:<0-255>

# Mode 2: Monostable
set timer <id> mode 2 parameter \
  pulse-duration:<ms> trigger-level:<0|1> \
  phase1-output:<0|1> phase2-output:<0|1> \
  output-coil:<0-255> ctrl-reg:<0-255>

# Mode 3: Astable
set timer <id> mode 3 parameter \
  on-ms:<ms> off-ms:<ms> \
  phase1-output:<0|1> phase2-output:<0|1> \
  output-coil:<0-255> ctrl-reg:<0-255>

# Mode 4: Input-triggered
set timer <id> mode 4 parameter \
  input-dis:<idx> delay-ms:<ms> \
  trigger-edge:<rising|falling> \
  phase1-output:<0|1> \
  output-coil:<0-255> ctrl-reg:<0-255>

set timer <id> control start                             # Start timer
set timer <id> control stop                              # Stop timer
set timer <id> control reset                             # Reset timer
set timer <id> enable on|off                             # Enable/disable
```

**GPIO Mapping Commands:**
```bash
set gpio <pin> static map input:<idx>    # GPIO input → discrete input
set gpio <pin> static map coil:<idx>     # Coil → GPIO output
no set gpio <pin>                        # Remove GPIO mapping
```

**ST Logic Commands:**
```bash
set logic <id> upload                    # Upload ST source (multiline)
set logic <id> enable on|off             # Enable/disable program
set logic <id> delete                    # Delete program
set logic <id> bind <var> reg:<addr>     # Bind variable to register
set logic <id> bind <var> coil:<addr>    # Bind variable to coil
```

**Register/Coil Direct Access:**
```bash
read reg <addr>                          # Read holding register
read input <addr>                        # Read input register
read coil <addr>                         # Read coil
write reg <addr> value <val>             # Write holding register
write coil <addr> value <on|off>         # Write coil
```

**System Commands:**
```bash
help                                     # Show command help
exit                                     # Exit telnet session (telnet only)
```

### System Features

#### Persistent Configuration (NVS Storage)
- **Storage Backend:** ESP32 NVS (Non-Volatile Storage)
- **CRC Protection:** CRC16-CCITT checksum on all saved data
- **Schema Versioning:** v8 (current)
  - v5 → v6: Added `hostname` field
  - v6 → v7: Added `remote_echo` field
  - v7 → v8: Added `persist_regs` (persistence groups)
- **Auto-Migration:** Old configs auto-upgrade on schema mismatch
- **Config Size:** ~30KB (PersistConfig struct)
- **Save Command:** `save` (manual)
- **Load:** Automatic on boot
- **Factory Reset:** `reset` command or schema corruption detection

**Saved Configuration:**
- Modbus slave ID & baudrate
- System hostname (max 31 chars)
- Remote echo setting (on/off)
- Wi-Fi settings (SSID, password, IP config, telnet config)
- All 4 counters (configuration, not values)
- All 4 timers (configuration, not state)
- All ST Logic programs (source code + bytecode)
- GPIO mappings (64 slots)
- GPIO2 user mode (heartbeat on/off)

#### GPIO2 Heartbeat LED
- **Default:** Enabled (LED blink at 500ms interval)
- **User-Controllable:** `set gpio 2 enable|disable`
- **Purpose:** Visual indication of firmware running
- **Pin:** GPIO2 (onboard LED på de fleste ESP32 boards)
- **Mode:** Toggle at 500ms intervals
- **Disable:** Frigør GPIO2 til user applications

#### Persistent Registers (v4.0+)
- **Named Groups:** Organize registers in groups for selective persistence
  - Max 8 groups, 16 registers per group
  - Groups saved to NVS, auto-restored at boot
- **ST Logic Integration:** Built-in SAVE()/LOAD() functions
  - Save from ST programs when conditions are met
  - Rate limited (max 1 save per 5 seconds)
- **Use Cases:**
  - Sensor calibration data
  - Last known good setpoints
  - Production counters
  - Configuration parameters

**CLI Commands:**
```bash
set persist group "sensors" add 100 101 102    # Create group
save registers all                              # Save all groups
save registers group "sensors"                  # Save specific group
show persist                                    # Display groups
```

**ST Logic Example:**
```st
PROGRAM Logic1
VAR
  sensor_value: INT;
  save_trigger: BOOL;
END_VAR

sensor_value := 42;  (* Bound to HR#100 *)

IF save_trigger THEN
  SAVE();  (* Save all persistence groups *)
END_IF;
END_PROGRAM
```

#### Watchdog Monitor (v4.0+)
- **Auto-Restart:** ESP32 Task Watchdog Timer (30s timeout)
  - Automatically restarts system if main loop hangs
  - Persistent reboot counter in NVS
- **Diagnostics:** Last reset reason and error message
  - Power-on, Panic, Brownout, Watchdog timeout
  - Last error message (max 127 chars)
  - Uptime before last reboot
- **Production Ready:**
  - Prevents system hang in field deployments
  - Debug-friendly: Error visible after reboot

**CLI Commands:**
```bash
show watchdog                                   # Display status
```

**Example Output:**
```
=== Watchdog Monitor (v4.0+) ===
Status: ENABLED
Timeout: 30 seconds
Reboot counter: 12
Current uptime: 3600 seconds
Last reset reason: Power-on
```

#### Build System & Versioning
- **Build Tool:** PlatformIO
- **Auto-Versioning:** Build number auto-increments hver compilation
- **Build Info:** Generated i `include/build_version.h`
  - BUILD_NUMBER (auto-increment)
  - BUILD_TIMESTAMP (ISO 8601)
  - GIT_BRANCH (current branch)
  - GIT_HASH (commit SHA)
- **Version Format:** vMAJOR.MINOR.PATCH (SemVer)
- **Version Display:** `show version` command

#### Debug System
- **Debug Output:** Serial console (UART0)
- **Debug Flags:** Selective enabling af debug output
  - `config-save`: Debug NVS save operations
  - `config-load`: Debug NVS load operations
  - `wifi-connect`: Debug Wi-Fi connection
  - `network-validate`: Debug network config validation
- **Debug Commands:**
```bash
set debug <flag> on|off        # Enable/disable specific debug
show debug                     # Show debug flags status
```

---

## 📋 Hardware Requirements

### MCU Specifications
| Component | Specification |
|-----------|---------------|
| **Microcontroller** | ESP32-WROOM-32 |
| **CPU** | Dual-core Xtensa LX6, 240MHz |
| **RAM (SRAM)** | 520KB total (320KB DRAM + 200KB IRAM) |
| **RAM Usage** | ~120KB used, ~200KB available for application |
| **Flash** | 4MB (SPI Flash) |
| **Flash Usage** | ~835KB used, ~475KB available |
| **Wi-Fi** | 802.11 b/g/n (2.4GHz) |
| **Bluetooth** | BLE 4.2 (not used in this project) |

### Peripheral Hardware
| Component | Specification | Connection |
|-----------|---------------|------------|
| **RS-485 Transceiver** | MAX485, MAX3485, or equivalent | GPIO4 (RX), GPIO5 (TX), GPIO15 (DIR) |
| **Power Supply** | 3.3V regulated, 500mA minimum | USB or external 5V→3.3V regulator |
| **USB-Serial** | Built-in (CP2102 or CH340) | For programming & debug console |

### Pin Configuration (ESP32-WROOM-32)
```
=== MODBUS RTU (UART1) ===
GPIO4   (PIN_UART1_RX)      ← Modbus RX (RS-485 RO pin)
GPIO5   (PIN_UART1_TX)      → Modbus TX (RS-485 DI pin)
GPIO15  (PIN_RS485_DIR)     → RS-485 direction (DE/RE pins)

=== SYSTEM ===
GPIO2   (Heartbeat)         → Onboard LED (500ms blink)

=== DEBUG (UART0) ===
GPIO1   (TXD0)              → USB Serial TX (115200 bps)
GPIO3   (RXD0)              ← USB Serial RX (115200 bps)

=== AVAILABLE FOR USER ===
GPIO12, GPIO13, GPIO14      → Available for counters/GPIO mapping
GPIO16, GPIO17, GPIO18      → Available for counters/GPIO mapping
GPIO19, GPIO21, GPIO22      → Available for PCNT/I2C/GPIO mapping
GPIO23, GPIO25, GPIO26      → Available for GPIO mapping
GPIO27, GPIO32, GPIO33      → Available for GPIO mapping

=== RESERVED (STRAPPING PINS) ===
GPIO0   (BOOT)              ⚠️ Used during flash programming
GPIO2   (BOOT)              ⚠️ Must be LOW during boot (heartbeat compatible)
GPIO15  (RS485_DIR)         ⚠️ Must be HIGH during boot (OK for RS-485)

=== DO NOT USE ===
GPIO6-11                    ❌ Connected to SPI flash (internal use)
GPIO34-39                   ❌ Input-only pins (no output capability)
```

### RS-485 Wiring
```
ESP32         MAX485        Modbus Bus
-----         ------        ----------
GPIO4 (RX) ←  RO
GPIO5 (TX) →  DI
GPIO15     →  DE/RE
3.3V       →  VCC
GND        →  GND
              A          →  A (Data+)
              B          →  B (Data-)
```

**RS-485 Notes:**
- **Termination:** 120Ω resistor mellem A-B på bus ends
- **Biasing:** 120Ω pullup på A, 120Ω pulldown på B (optional)
- **Cable:** Twisted pair, shielded (Cat5e eller bedre)
- **Max Distance:** 1200m (4000ft) at 9600 bps
- **Max Nodes:** 32 devices (247 med repeaters)

---

## 🔧 Installation & Build

### Prerequisites
1. **PlatformIO Core** - [Installation guide](https://platformio.org/install/cli)
   ```bash
   # Via Python pip
   pip install platformio

   # Verify installation
   pio --version
   ```

2. **Python 3.7+** - For PlatformIO scripts
   ```bash
   python --version  # Should show 3.7 or higher
   ```

3. **Git** - For source control
   ```bash
   git --version
   ```

4. **USB Drivers** - For ESP32 programming
   - CP2102: [Silabs drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
   - CH340: [WCH drivers](https://www.wch-ic.com/downloads/CH341SER_ZIP.html)

### Clone Repository
```bash
git clone https://github.com/Jangreenlarsen/Modbus_server_slave_ESP32.git
cd Modbus_server_slave_ESP32
```

### Build Firmware
```bash
# Clean build (recommended first time)
pio run --target clean

# Build firmware
pio run

# Output:
# ✓ Build SUCCESS
# RAM:   [====      ]  36.4% (119416 bytes / 327680 bytes)
# Flash: [======    ]  63.8% (835621 bytes / 1310720 bytes)
```

### Upload to ESP32
```bash
# List available ports
pio device list

# Upload firmware (auto-detect port)
pio run --target upload

# Upload to specific port
pio run --target upload --upload-port COM3    # Windows
pio run --target upload --upload-port /dev/ttyUSB0  # Linux
```

### Monitor Serial Output
```bash
# Start serial monitor (115200 bps)
pio device monitor

# Monitor with filters
pio device monitor --filter colorize --filter time

# Exit monitor: Ctrl+C
```

### All-in-One Build & Upload
```bash
# Build, upload, and monitor in one command
pio run --target upload && pio device monitor
```

### Configuration Files

#### `platformio.ini` (Build Configuration)
```ini
[env:esp32]
platform = espressif32@6.12.0
board = esp32doit-devkit-v1
framework = arduino

# Serial monitor
monitor_speed = 115200
monitor_filters = colorize, time

# Build flags
build_flags =
    -DCORE_DEBUG_LEVEL=0
    -DBOARD_HAS_PSRAM=0

# Upload settings
upload_speed = 921600
upload_port = COM3    # Change to your port

# Extra scripts
extra_scripts =
    pre:scripts/generate_build_info.py
```

---

## 📖 Usage Examples

### Quick Start (First Boot)
1. **Upload firmware** via USB
   ```bash
   pio run --target upload
   ```

2. **Connect serial terminal** (115200 bps)
   ```bash
   pio device monitor
   ```

3. **Check firmware version**
   ```
   > show version
   Version: 3.2.0 Build #545
   Built: 2025-12-09 16:52:33 (main@ca6b723)
   ```

4. **View default configuration**
   ```
   > show config
   Hostname: modbus-esp32
   Unit-ID: 1 (SLAVE)
   Baud: 9600
   ```

5. **Configure Modbus slave ID**
   ```
   > set id 14
   Slave ID set to: 14 (will apply on next boot)
   NOTE: Use 'save' to persist to NVS

   > save
   [OK] Configuration saved to NVS
   ```

### Example 1: Hardware Counter med PCNT
```bash
# Tilslut pulse signal til GPIO19

# Konfigurer counter 1 i hardware mode
> set counter 1 mode 1 parameter hw-mode:hw edge:rising prescaler:100 scale:0.1 index-reg:20 raw-reg:30 freq-reg:35 ctrl-reg:40 hw-gpio:19

# Enable counter
> set counter 1 enable on

# Check counter status
> show counter 1

=== COUNTER 1 ===
Status: ENABLED
Hardware Mode: HW (PCNT)
Edge Type: rising
Prescaler: 100
Scale Factor: 0.1
PCNT GPIO: 19

Register Mappings:
  Index Register (scaled value): 20
  Raw Register (prescaled): 30
  Frequency Register (Hz): 35
  Control Register: 40

Current Values:
  Raw Value: 12345
  Prescaled Value: 123
  Scaled Value: 12
  Frequency: 1523 Hz

# Read registers via Modbus
> read reg 20    # Scaled value
> read reg 30    # Prescaled value
> read reg 35    # Frequency in Hz

# Reset counter
> write reg 40 value 1
> read reg 40    # Should show 0 (auto-cleared)
```

### Example 2: Astable Timer (LED Blink)
```bash
# Konfigurer timer 1 til blink mode
> set timer 1 mode 3 parameter on-ms:1000 off-ms:500 phase1-output:1 phase2-output:0 output-coil:50 ctrl-reg:45

# Enable timer
> set timer 1 enable on

# Start blinking
> write reg 45 value 2    # Bit 1 = START

# Check status
> show timer 1

=== TIMER 1 ===
Status: ENABLED
Mode: ASTABLE (Mode 3 - Oscillator/Blink)
ON Duration: 1000ms
OFF Duration: 500ms
Output Coil: 50
Control Register: 45

# Stop blinking
> write reg 45 value 4    # Bit 2 = STOP

# Read coil status
> read coil 50
```

### Example 3: ST Logic Program
```bash
# Upload ST Logic program
> set logic 1 upload
VAR_INPUT
  sensor : INT;      (* Temperature sensor input *)
  setpoint : INT;    (* Desired temperature *)
END_VAR

VAR_OUTPUT
  heater : INT;      (* Heater control *)
  alarm : INT;       (* Temperature alarm *)
END_VAR

VAR
  hysteresis : INT := 5;
END_VAR

BEGIN
  (* Simple thermostat control *)
  IF sensor < (setpoint - hysteresis) THEN
    heater := 1;     (* Turn ON heater *)
  ELSIF sensor > (setpoint + hysteresis) THEN
    heater := 0;     (* Turn OFF heater *)
  END_IF;

  (* High temperature alarm *)
  IF sensor > 100 THEN
    alarm := 1;
  ELSE
    alarm := 0;
  END_IF;
END

<blank line to finish upload>

# Bind variables to registers
> set logic 1 bind sensor reg:100
> set logic 1 bind setpoint reg:101
> set logic 1 bind heater reg:102
> set logic 1 bind alarm reg:103

# Enable program
> set logic 1 enable on

# Check program status
> show logic 1

=== LOGIC PROGRAM 1 ===
Status: ENABLED
Execution Count: 1523
Error Count: 0
Last Execution: 12345ms

Variables:
  sensor (INPUT) ← Reg 100
  setpoint (INPUT) ← Reg 101
  heater (OUTPUT) → Reg 102
  alarm (OUTPUT) → Reg 103

# Write setpoint via Modbus
> write reg 101 value 75

# Simulate sensor reading
> write reg 100 value 68    # Below setpoint → heater ON
> read reg 102              # Should show 1 (heater ON)

> write reg 100 value 82    # Above setpoint → heater OFF
> read reg 102              # Should show 0 (heater OFF)
```

### Example 4: Wi-Fi & Telnet Setup
```bash
# Configure Wi-Fi (via serial console)
> set wifi ssid MyNetwork
Wi-Fi SSID set to: MyNetwork

> set wifi password MyPassword123
Wi-Fi password set (not shown for security)

> set wifi dhcp on
DHCP enabled (automatic IP assignment)

> set wifi telnet enable
Telnet enabled

> set wifi telnet-user admin
Telnet username set to: admin

> set wifi telnet-pass secret123
Telnet password set (hidden for security)

> save
[OK] Configuration saved to NVS

> connect wifi
Connecting to Wi-Fi: MyNetwork
Wi-Fi connection started (async)
Use 'show wifi' to check connection status

# Wait 5 seconds...

> show wifi
Wi-Fi Status: CONNECTED
SSID: MyNetwork
IP Address: 192.168.1.145
Gateway: 192.168.1.1
Netmask: 255.255.255.0
DNS: 192.168.1.1
Signal Strength: -45 dBm (Excellent)
Telnet: ENABLED (port 23)

# Now connect via telnet from another computer
# telnet 192.168.1.145 23
# Username: admin
# Password: secret123
```

### Example 5: Persistent Registers with ST Logic (v4.0+)
```bash
# Create persistence group for sensor calibration
> set persist group "calibration" add 200 201 202
✓ Added 3 registers to group 'calibration'

# Enable persistence system
> set persist enable on
Persistence system enabled

# Upload ST Logic program with SAVE/LOAD
> set logic 1 upload
PROGRAM CalibrationManager
VAR
  cal_offset: INT;      (* Calibration offset *)
  cal_scale: INT;       (* Calibration scale factor *)
  save_trigger: BOOL;   (* Manual save trigger *)
  load_trigger: BOOL;   (* Manual restore trigger *)
  save_result: INT;     (* SAVE() return value *)
END_VAR

BEGIN
  (* Manual save when triggered *)
  IF save_trigger THEN
    save_result := SAVE();  (* Save all groups to NVS *)
    save_trigger := 0;      (* Clear trigger *)
  END_IF;

  (* Manual restore when triggered *)
  IF load_trigger THEN
    save_result := LOAD();  (* Restore all groups from NVS *)
    load_trigger := 0;      (* Clear trigger *)
  END_IF;
END

<blank line to finish upload>

# Bind variables to persistence group registers
> set logic 1 bind cal_offset reg:200
> set logic 1 bind cal_scale reg:201
> set logic 1 bind save_trigger reg:202
> set logic 1 bind load_trigger reg:203
> set logic 1 bind save_result reg:204

# Enable program
> set logic 1 enabled:true

# Write calibration values
> write reg 200 value 42    # Set offset
> write reg 201 value 100   # Set scale

# Trigger save from ST Logic
> write reg 202 value 1     # Trigger SAVE()

# Check save result
> read reg 204 1
Result: 0 (success)

# View persistence status
> show persist

=== Persistent Registers (v4.0+) ===
Enabled: YES
Groups: 1

Group "calibration" (3 registers)
  Reg 200 = 42
  Reg 201 = 100
  Reg 202 = 0
  Last save: 5 sec ago

# Save to NVS (manual CLI save)
> save registers all
✓ Saved 1 groups to NVS

# Reboot to test restore
> reboot

# After reboot, check if values restored
> read reg 200 1
Result: 42 ✓ (restored from NVS)

> read reg 201 1
Result: 100 ✓ (restored from NVS)
```

---

## 📁 Project Structure

```
Modbus_server_slave_ESP32/
│
├── README.md                      # This comprehensive documentation
├── CHANGELOG.md                   # Detailed version history
├── CLAUDE.md                      # Developer guidelines (Danish)
├── platformio.ini                 # PlatformIO build configuration
├── build_number.txt               # Auto-generated build counter
│
├── include/                       # C++ Header Files (~40 files)
│   ├── types.h                   # ★ ALL struct definitions (single source)
│   ├── constants.h               # ★ ALL #defines and enums (single source)
│   │
│   ├── config_struct.h           # Configuration persistence
│   ├── config_save.h             # Save to NVS
│   ├── config_load.h             # Load from NVS
│   ├── config_apply.h            # Apply config to system
│   │
│   ├── modbus_frame.h            # Modbus frame struct & CRC
│   ├── modbus_parser.h           # Parse raw bytes → request
│   ├── modbus_serializer.h       # Build response frames
│   ├── modbus_fc_read.h          # FC01-04 implementations
│   ├── modbus_fc_write.h         # FC05-06, FC0F-10 implementations
│   ├── modbus_fc_dispatch.h      # Route FC → handler
│   ├── modbus_server.h           # Main state machine
│   ├── modbus_rx.h               # Serial RX & frame detection
│   ├── modbus_tx.h               # RS-485 TX with DIR control
│   │
│   ├── registers.h               # Holding/input register arrays
│   ├── coils.h                   # Coil/discrete input bit arrays
│   ├── gpio_mapping.h            # GPIO ↔ coil/register bindings
│   │
│   ├── counter_config.h          # CounterConfig struct & validation
│   ├── counter_sw.h              # SW polling mode
│   ├── counter_sw_isr.h          # SW-ISR interrupt mode
│   ├── counter_hw.h              # HW PCNT mode
│   ├── counter_frequency.h       # Frequency measurement
│   ├── counter_engine.h          # Orchestration & prescaler
│   │
│   ├── timer_config.h            # TimerConfig struct & validation
│   ├── timer_engine.h            # Timer state machines
│   │
│   ├── st_types.h                # ST Logic types & bytecode
│   ├── st_compiler.h             # ST → bytecode compiler
│   ├── st_parser.h               # ST syntax parser
│   ├── st_vm.h                   # Bytecode VM executor
│   ├── st_logic_config.h         # Logic program config
│   ├── st_logic_engine.h         # Main ST execution loop
│   │
│   ├── cli_parser.h              # Command tokenizer & dispatcher
│   ├── cli_commands.h            # `set` command handlers
│   ├── cli_show.h                # `show` command handlers
│   ├── cli_shell.h               # CLI main loop & I/O
│   ├── cli_history.h             # Command history buffer
│   │
│   ├── network_manager.h         # Wi-Fi connection manager
│   ├── network_config.h          # Network config validation
│   ├── wifi_driver.h             # ESP32 Wi-Fi HAL wrapper
│   ├── telnet_server.h           # Telnet protocol handler
│   │
│   ├── gpio_driver.h             # GPIO abstraction layer
│   ├── uart_driver.h             # UART abstraction layer
│   ├── pcnt_driver.h             # PCNT (Pulse Counter) abstraction
│   ├── nvs_driver.h              # NVS (storage) abstraction
│   │
│   ├── heartbeat.h               # GPIO2 LED blink
│   ├── debug.h                   # Debug printf wrappers
│   ├── debug_flags.h             # Selective debug output
│   └── build_version.h           # Auto-generated build info
│
├── src/                           # C++ Implementation Files (~40 files)
│   ├── main.cpp                  # ★ Entry point: setup() & loop()
│   │
│   ├── config_struct.cpp         # Config defaults
│   ├── config_save.cpp           # NVS save with CRC
│   ├── config_load.cpp           # NVS load & migration
│   ├── config_apply.cpp          # Apply config to hardware
│   │
│   ├── modbus_frame.cpp          # CRC16 calculation
│   ├── modbus_parser.cpp         # Byte → request parsing
│   ├── modbus_serializer.cpp     # Response building
│   ├── modbus_fc_read.cpp        # FC01-04 handlers
│   ├── modbus_fc_write.cpp       # FC05-06, FC0F-10 handlers
│   ├── modbus_fc_dispatch.cpp    # FC routing logic
│   ├── modbus_server.cpp         # State machine (idle→RX→process→TX)
│   ├── modbus_rx.cpp             # RX with 3.5 char timeout
│   ├── modbus_tx.cpp             # TX with RS-485 DIR control
│   │
│   ├── registers.cpp             # Register array access
│   ├── coils.cpp                 # Coil bit array access
│   ├── gpio_mapping.cpp          # GPIO ↔ register mapping
│   │
│   ├── counter_config.cpp        # Counter validation
│   ├── counter_sw.cpp            # SW polling implementation
│   ├── counter_sw_isr.cpp        # SW-ISR interrupt handling
│   ├── counter_hw.cpp            # HW PCNT setup & read
│   ├── counter_frequency.cpp     # Hz calculation (~1sec)
│   ├── counter_engine.cpp        # Main counter loop
│   │
│   ├── timer_config.cpp          # Timer validation
│   ├── timer_engine.cpp          # Timer state machines
│   │
│   ├── st_compiler.cpp           # ST lexer & code generator
│   ├── st_parser.cpp             # ST syntax parsing
│   ├── st_vm.cpp                 # Bytecode execution
│   ├── st_logic_config.cpp       # Program storage
│   ├── st_logic_engine.cpp       # Main ST loop (10ms rate)
│   │
│   ├── cli_parser.cpp            # Command parsing & dispatch
│   ├── cli_commands.cpp          # `set` implementations
│   ├── cli_show.cpp              # `show` implementations
│   ├── cli_shell.cpp             # CLI I/O & line editing
│   ├── cli_history.cpp           # History buffer (arrow keys)
│   │
│   ├── network_manager.cpp       # Wi-Fi state machine
│   ├── network_config.cpp        # Config validation
│   ├── wifi_driver.cpp           # ESP32 WiFi.h wrapper
│   ├── telnet_server.cpp         # Telnet protocol (IAC, echo)
│   │
│   ├── gpio_driver.cpp           # GPIO init/read/write
│   ├── uart_driver.cpp           # UART init/TX/RX
│   ├── pcnt_driver.cpp           # PCNT unit configuration
│   ├── nvs_driver.cpp            # NVS read/write/commit
│   │
│   ├── heartbeat.cpp             # LED blink loop
│   ├── debug.cpp                 # Debug output functions
│   ├── debug_flags.cpp           # Debug flag storage
│   └── version.cpp               # Version strings
│
├── scripts/                       # Build & Test Scripts
│   ├── generate_build_info.py    # Auto-generate build_version.h
│   ├── quick_count_test.py       # Counter testing
│   ├── read_boot_debug.py        # Read serial boot output
│   ├── reconfigure_counters.py   # Counter CLI automation
│   ├── run_counter_tests.py      # Counter integration tests
│   └── ...                       # Additional test scripts
│
├── docs/                          # Documentation
│   ├── README_ST_LOGIC.md        # ST Logic programming guide
│   ├── FEATURE_GUIDE.md          # Feature-by-feature documentation
│   ├── ESP32_Module_Architecture.md  # Architecture deep-dive
│   ├── GPIO_MAPPING_GUIDE.md     # GPIO configuration guide
│   ├── COUNTER_COMPARE_REFERENCE.md  # Counter compare feature
│   ├── ST_USAGE_GUIDE.md         # ST Logic usage examples
│   └── ARCHIVED/                 # Historical reports (41 files)
│
├── .pio/                          # PlatformIO build artifacts (ignored)
├── .vscode/                       # VS Code workspace settings
├── .git/                          # Git version control
└── .gitignore                     # Git ignore patterns
```

---

## 🏗️ Architecture

### 8-Layer Modular Design

#### **Layer 0: Hardware Drivers** (GPIO, UART, PCNT, NVS)
**Purpose:** Abstract ESP32 hardware APIs
**Files:** `gpio_driver.cpp`, `uart_driver.cpp`, `pcnt_driver.cpp`, `nvs_driver.cpp`
**Principle:** Only layer that knows about ESP32 registers. Can be mocked for testing.

#### **Layer 1: Protocol Core** (Modbus Frame Handling)
**Purpose:** Modbus RTU frame parsing & serialization
**Files:** `modbus_frame.cpp`, `modbus_parser.cpp`, `modbus_serializer.cpp`
**Principle:** Pure functions, testable without hardware.

#### **Layer 2: Function Code Handlers** (FC01-06, FC0F-10)
**Purpose:** Implement Modbus function code logic
**Files:** `modbus_fc_read.cpp`, `modbus_fc_write.cpp`, `modbus_fc_dispatch.cpp`
**Principle:** Each FC isolated, adding new FC = one file change.

#### **Layer 3: Modbus Server Runtime**
**Purpose:** Main Modbus state machine
**Files:** `modbus_server.cpp`, `modbus_rx.cpp`, `modbus_tx.cpp`
**State Machine:** idle → RX → process → TX → idle
**Timing:** 3.5 character timeout for frame detection.

#### **Layer 4: Data Storage** (Registers & Coils)
**Purpose:** Holding/input register arrays, coil/discrete input bit arrays
**Files:** `registers.cpp`, `coils.cpp`, `gpio_mapping.cpp`
**Access:** All register/coil access goes through these APIs.

#### **Layer 5: Feature Engines** (Counters, Timers, ST Logic)
**Counter Engine:**
- `counter_engine.cpp`: Orchestration + prescaler division
- `counter_sw.cpp`: Software polling mode
- `counter_sw_isr.cpp`: Interrupt mode
- `counter_hw.cpp`: PCNT hardware mode
- `counter_frequency.cpp`: Hz measurement

**Timer Engine:**
- `timer_engine.cpp`: 4 timer state machines

**ST Logic Engine:**
- `st_compiler.cpp`: Source → bytecode
- `st_vm.cpp`: Bytecode executor
- `st_logic_engine.cpp`: Main loop (10ms rate)

#### **Layer 6: Persistence** (Config Save/Load/Apply)
**Purpose:** Configuration management
**Files:**
- `config_struct.cpp`: Default config
- `config_save.cpp`: Save to NVS with CRC
- `config_load.cpp`: Load from NVS, schema migration
- `config_apply.cpp`: Apply config to running system

**Principle:**
- Load = read from NVS (can fail gracefully)
- Save = write to NVS (atomic with CRC)
- Apply = activate in running system (idempotent)

#### **Layer 7: User Interface** (CLI & Telnet)
**CLI Components:**
- `cli_parser.cpp`: Tokenize & dispatch
- `cli_commands.cpp`: `set` command handlers
- `cli_show.cpp`: `show` command handlers
- `cli_shell.cpp`: Main CLI loop, line editing
- `cli_history.cpp`: Command history buffer

**Network UI:**
- `telnet_server.cpp`: Telnet protocol (IAC, WILL/DO/DONT, echo)
- `network_manager.cpp`: Wi-Fi connection state machine

#### **Layer 8: System** (Main Loop & Utilities)
**Entry Point:**
- `main.cpp`: setup() og loop() only

**Utilities:**
- `heartbeat.cpp`: GPIO2 LED blink
- `debug.cpp`: Debug output wrapper
- `version.cpp`: Version strings

### Key Design Principles

**1. Modularity**
- 40+ files, hver med single responsibility
- Typical file size: 100-250 lines
- No file depends on "everything"

**2. No Circular Dependencies**
- Dependency graph is acyclic (DAG)
- Layer N can only depend on Layer N-1 or lower
- Each module independently testable

**3. Single Source of Truth**
- **types.h:** ALL struct definitions
- **constants.h:** ALL #defines and enums
- No duplicate definitions across files

**4. CRC-Protected Persistence**
- All saved data has CRC16 checksum
- CRC calculated over entire struct (dynamic size)
- Corruption detected on load → factory defaults

**5. Schema Versioning**
- Each config has schema version number
- Mismatch → automatic migration or reset
- Backward compatible (old configs auto-upgrade)

**6. Dual-Core Friendly**
- Core 0: FreeRTOS, Wi-Fi, Bluetooth stack
- Core 1: Application (Modbus, CLI, counters, timers)
- No mutex needed (single-threaded application)

---

## 📊 Specifications

### Modbus RTU Protocol
| Parameter | Value |
|-----------|-------|
| **Function Codes** | FC01, FC02, FC03, FC04, FC05, FC06, FC0F (15), FC10 (16) |
| **Holding Registers** | 256 (addresses 0-255) |
| **Input Registers** | 256 (addresses 0-255) |
| **Coils** | 256 (bit-addressable 0-255) |
| **Discrete Inputs** | 256 (bit-addressable 0-255) |
| **Slave ID Range** | 1-247 |
| **Baudrate Range** | 300-115200 bps |
| **Frame Timeout** | 3.5 character times (baudrate-adaptive) |
| **CRC Algorithm** | CRC16-CCITT (XMODEM polynomial) |
| **Error Codes** | 01 (Illegal Function), 02 (Illegal Address), 03 (Illegal Value) |
| **Max Frame Size** | 256 bytes |
| **Response Time** | <100ms typical, <500ms worst-case |

### Counters (4 Independent) - v4.2.4
| Feature | Specification |
|---------|---------------|
| **Modes** | 3 (SW polling, SW-ISR interrupt, HW PCNT) |
| **Max Frequency** | 500Hz (SW), 10kHz (ISR), 40MHz (HW) |
| **Counter Range** | 64-bit internal (0 to 18,446,744,073,709,551,615) |
| **Prescaler Range** | 1-65535 |
| **Scale Factor** | 0.0001-10000.0 (float) |
| **Bit Width Output** | 8, 16, 32, or 64-bit (multi-word Modbus support) |
| **Edge Detection** | Rising, falling, or both |
| **Direction** | Up or down counting |
| **Frequency Measurement** | 0-20kHz, ±1Hz accuracy |
| **Compare Feature** | ✅ Edge detection (v4.2.4 - BUG-029) |
| **Compare Modes** | ≥ (mode 0), > (mode 1), === (mode 2) |
| **Compare Threshold** | Runtime modifiable via Modbus (v4.2.4 - BUG-030) |
| **Register Spacing** | 20 registers per counter (v4.2.4 - BUG-028) |
| **Smart Defaults** | HR100-114 (C1), HR120-134 (C2), HR140-154 (C3), HR160-174 (C4) |
| **Debounce** | Software (1-1000ms) |

### Timers (4 Independent)
| Feature | Specification |
|---------|---------------|
| **Modes** | 4 (One-shot, Monostable, Astable, Input-triggered) |
| **Timing Precision** | ±1ms (millis() based) |
| **Min Duration** | 1ms |
| **Max Duration** | 4,294,967,295ms (~49 days) |
| **Output** | Coil register (0-255) |
| **Control** | Holding register (start/stop/reset bits) |
| **Retriggerable** | Yes (Mode 2: Monostable) |

### ST Logic Programming (4 Programs)
| Feature | Specification |
|---------|---------------|
| **Source Code Size** | 2KB max per program |
| **Bytecode Size** | 1KB max per program |
| **Variables** | 32 max per program |
| **Variable Types** | INT (16-bit), BOOL (1-bit), REAL (float) |
| **Execution Rate** | 10ms default (configurable) |
| **Compilation** | Real-time on upload |
| **Language** | IEC 61131-3 Structured Text subset |

### Networking
| Feature | Specification |
|---------|---------------|
| **Wi-Fi Standard** | 802.11 b/g/n (2.4GHz) |
| **Security** | WPA/WPA2-PSK |
| **IP Assignment** | DHCP or static |
| **Telnet Port** | 23 (default, configurable) |
| **Concurrent Telnet** | 1 client |
| **Authentication** | Username/password (optional) |
| **Session Timeout** | Configurable |

### System Resources
| Resource | Specification |
|----------|---------------|
| **RAM Total** | 520KB (320KB DRAM + 200KB IRAM) |
| **RAM Used** | ~120KB |
| **RAM Available** | ~200KB for application |
| **Flash Total** | 4MB |
| **Flash Used** | ~835KB (firmware + partition table) |
| **Flash Available** | ~475KB |
| **NVS Size** | 20KB (persistent storage) |
| **CPU Speed** | 240MHz (dual-core) |
| **Watchdog** | Disabled (can be enabled) |

---

## 🔐 Security Considerations

### Authentication
- **Telnet:** Optional username/password authentication
- **Default:** Empty username, empty password (disabled by default)
- **Password Storage:** Plain text in NVS (⚠️ not encrypted)
- **Recommendation:** Enable authentication for production deployments

### Network Security
- **Encryption:** ❌ None (telnet protocol is plain text)
- **Firewall:** ⚠️ ESP32 has no built-in firewall
- **Recommendation:**
  - Use on trusted networks only
  - Consider VPN tunnel for remote access
  - Do NOT expose telnet to Internet directly

### Data Integrity
- **CRC16 Validation:** ✅ All Modbus frames
- **CRC16 Validation:** ✅ All NVS stored data
- **Schema Versioning:** ✅ Automatic migration with safety checks
- **Corruption Detection:** ✅ Reverts to factory defaults on corruption

### Access Control
- **Physical:** USB serial console has full access (no authentication)
- **Network:** Telnet has optional authentication
- **Modbus:** No authentication (standard Modbus limitation)

### Recommendations
⚠️ **Not suitable for:**
- Internet-exposed devices (no encryption)
- Critical infrastructure (no redundancy/watchdog)
- Financial/medical applications (no audit logging)

✅ **Suitable for:**
- Industrial automation (local network)
- Building control systems (isolated VLAN)
- Test/development environments
- Educational/hobby projects

**Security Hardening Checklist:**
- [ ] Enable telnet authentication
- [ ] Change default passwords
- [ ] Use dedicated VLAN for Modbus devices
- [ ] Implement network firewall rules
- [ ] Enable watchdog timer
- [ ] Disable unused features (telnet if not needed)
- [ ] Regular firmware updates

---

## 🧪 Testing

### Unit Testing (Local)
```bash
# Run PlatformIO unit tests (if available)
pio test
```

**Note:** Unit tests are currently minimal. Expansion recommended:
- Modbus frame parsing tests
- CRC calculation tests
- Counter logic tests
- Timer state machine tests

### Integration Testing (Hardware Required)
1. **Upload firmware**
   ```bash
   pio run --target upload
   ```

2. **Connect serial monitor**
   ```bash
   pio device monitor
   ```

3. **Run manual CLI tests**
   ```bash
   > show config
   > set counter 1 mode 1 parameter hw-mode:sw
   > show counter 1
   ```

4. **Verify via Modbus master**
   - Use Modbus Poll (Windows) or pymodbus (Python)
   - Read holding register 0: `READ 03 00 00 00 01`
   - Write register 0: `WRITE 06 00 00 00 64`

### Python Test Scripts
Located in `scripts/` folder:

```bash
# Counter accuracy test
python scripts/quick_count_test.py

# Timer timing verification
python scripts/run_counter_tests.py

# Serial boot debug
python scripts/read_boot_debug.py

# Counter reconfiguration automation
python scripts/reconfigure_counters.py
```

**Example Test Workflow:**
```bash
# 1. Build & upload firmware
pio run --target upload

# 2. Run Python test script (separate terminal)
python scripts/quick_count_test.py

# 3. Monitor serial output
pio device monitor --filter colorize
```

### Modbus Master Testing (pymodbus)
```python
from pymodbus.client import ModbusSerialClient

# Connect to ESP32 via USB-RS485 adapter
client = ModbusSerialClient(
    port='COM3',         # Change to your port
    baudrate=9600,
    parity='N',
    stopbits=1,
    bytesize=8,
    timeout=1
)

if client.connect():
    # Read holding registers 0-9
    result = client.read_holding_registers(address=0, count=10, slave=1)
    print(f"Registers: {result.registers}")

    # Write holding register 5
    client.write_register(address=5, value=1234, slave=1)

    # Read coils 0-15
    result = client.read_coils(address=0, count=16, slave=1)
    print(f"Coils: {result.bits}")

    client.close()
```

---

## 📝 Version History

- **v4.2.5** (2025-12-18) - 🔒 Security & Stability Fixes
  - **BUG-031 FIXED:** Counter write lock now used by Modbus FC03 (prevents 64-bit read corruption)
  - **BUG-032 FIXED:** ST parser buffer overflow fixed (strcpy → strncpy with bounds)
  - **BUG-033 FIXED:** Variable declaration bounds check moved before array access
  - **NEW BUGS DOCUMENTED:** BUG-034 to BUG-038 (ISR volatile, overflow clear, etc.)
  - **Documentation updates:**
    - CLAUDE_ARCH.md: Added 15+ missing modules (Network, Console, Watchdog, ST Builtins)
    - All CLAUDE_*.md files updated to v4.2.5
    - BUGS_INDEX.md: 8 new bug entries added
  - **Code quality:** Dynamic version in CLI help banner (no more hardcoded version)

- **v4.2.4** (2025-12-17) - ⭐ Counter Compare Edge Detection & Modbus Control
  - **BUG-030 FIXED:** Compare value accessible via Modbus register (runtime modifiable)
  - **BUG-029 FIXED:** Compare modes use edge detection (rising edge trigger only)
  - **BUG-028 FIXED:** Register spacing increased to 20 per counter (64-bit support)
  - **BUG-027 FIXED:** Counter display overflow clamping (bit-width respected)
  - **Compare feature improvements:**
    - All modes (≥, >, ===) now use edge detection
    - Compare threshold writable via Modbus FC06/FC16
    - Reset-on-read works correctly (bit4 clears and stays cleared)
  - **Register layout updates:**
    - Counter 1: HR100-114, Counter 2: HR120-134, Counter 3: HR140-154, Counter 4: HR160-174
    - Each counter: index (1-4 words), raw (1-4 words), freq, overload, ctrl, compare_value (1-4 words)
  - **Documentation:** Updated CLI help, BUGS.md (BUG-027 to BUG-030), BUGS_INDEX.md, CLAUDE_ARCH.md

- **v4.1.0** (2025-12-12) - ⭐ ST Logic Performance Monitoring & Modbus Integration
  - **Performance monitoring system:** Min/Max/Avg execution time tracking (µs precision)
  - **CLI commands:** `show logic stats`, `show logic X timing`, `set logic stats reset`
  - **Modbus statistics:** IR 252-293 (42 new registers for statistics)
  - **Dynamic interval control:** `set logic interval:X` (10/20/25/50/75/100 ms)
  - **Modbus interval control:** HR 236-237 (32-bit read-write)
  - **Fixed 7 bugs:** BUG-001 to BUG-007 (all CRITICAL-MEDIUM severity)
  - **Documentation:** MODBUS_REGISTER_MAP.md, ST_MONITORING.md, TIMING_ANALYSIS.md, BUGS.md
  - Fixed rate scheduler with deterministic 10ms timing
  - Performance ratings with automatic tuning recommendations

- **v4.0.2** (2025-12-11) - Telnet Auth Fix
  - Fixed password validation failure due to whitespace
  - Added `trim_string()` helper for input sanitization

- **v4.0.1** (2025-12-11) - CLI Persist Enable Fix
  - Fixed `set persist enable` command error

- **v4.0.0** (2025-12-10) - Persistent Registers & Watchdog Monitor
  - Persistent register groups (save/restore to NVS)
  - ST Logic SAVE()/LOAD() functions
  - Watchdog monitor with auto-restart
  - Reboot counter and diagnostics

- **v3.2.0** (2025-12-09) - CLI Commands Complete + Persistent Settings
  - `show counter <id>` and `show timer <id>` detailed views
  - `set hostname` now persistent (saved to NVS)
  - `set echo` now persistent (saved to NVS)
  - Schema v7 with automatic migration

- **v3.1.1** (2025-12-08) - Telnet Insert Mode & ST Upload Copy/Paste
  - Telnet cursor position editing (insert mode)
  - Fixed multi-line ST Logic copy/paste into telnet

- **v3.1.0** (2025-12-05) - Wi-Fi Display & Telnet Auth Improvements
  - Enhanced `show config` with Wi-Fi section
  - `show wifi` displays actual IP (DHCP/static)
  - Wi-Fi connection validation with error messages

- **v3.0.0** (2025-12-02) - Telnet Server & Console Layer
  - Telnet CLI on port 23
  - Console abstraction (Serial/Telnet unified)
  - Remote authentication (username/password)
  - Arrow key command history

See [CHANGELOG.md](CHANGELOG.md) for complete version history with all details.

---

## 🤝 Contributing

### Code Standards
- **Language:** C++ (Arduino framework)
- **Style:** K&R with 2-space indents
- **File Size:** Keep files under 300 lines
- **Naming:** `snake_case` for functions, `UPPER_CASE` for constants
- **Comments:** Doxygen-style for public APIs

### Semantic Versioning
Follow SemVer (MAJOR.MINOR.PATCH):
- **MAJOR:** Breaking changes (incompatible API, config format)
- **MINOR:** New features (backward compatible)
- **PATCH:** Bug fixes (backward compatible)

### Adding Features
1. **Plan:** Consider which layer the feature belongs to
2. **Implement:** Create new .cpp/.h files in appropriate layer
3. **Test:** Verify on hardware
4. **Document:** Update CHANGELOG.md and this README
5. **Version:** Bump version in `include/constants.h`
6. **Commit:** Use descriptive commit message with `git tag`

### Pull Request Process
1. Fork the repository
2. Create feature branch (`git checkout -b feature/my-feature`)
3. Make changes (follow code standards)
4. Test on hardware
5. Commit with descriptive message
6. Push to your fork
7. Open Pull Request with description

---

## 📞 Support & Issues

### Getting Help
- **GitHub Issues:** [Report bugs & request features](https://github.com/Jangreenlarsen/Modbus_server_slave_ESP32/issues)
- **Documentation:** See `docs/` folder for detailed guides
- **Serial Debug:** Use `show debug` and `set debug <flag> on` for diagnostics

### Reporting Bugs
Please include:
- Firmware version (`show version`)
- Build number
- Hardware configuration (board, RS-485 module)
- Steps to reproduce
- Serial console output
- Expected vs actual behavior

### Feature Requests
Please describe:
- Use case & motivation
- Proposed implementation (if applicable)
- Impact on existing features

---

## 📄 License

**Copyright © 2025 Jan Green Larsen**

[Specify your license here - options: MIT, GPL-3.0, Apache-2.0, proprietary, etc.]

---

## 🙏 Acknowledgments

- **Original Architecture:** Mega2560 v3.6.5 (reference implementation)
- **ESP32 Platform:** Espressif Systems
- **Modbus Protocol:** Modicon/Schneider Electric
- **IEC 61131-3 ST:** International Electrotechnical Commission
- **PlatformIO:** Build system & toolchain
- **Arduino Framework:** ESP32 Arduino Core
- **Community:** Stack Overflow, ESP32 forums

---

**Maintained by:** Jan Green Larsen
**Last Updated:** 2025-12-12
**Repository:** https://github.com/Jangreenlarsen/Modbus_server_slave_ESP32

---

## 📚 Additional Documentation

### Core Documentation (Root)
- **[MODBUS_REGISTER_MAP.md](MODBUS_REGISTER_MAP.md)** - ⭐ **COMPLETE Modbus register reference**
  - All registers: Fixed (ST Logic) + Dynamic (Counters/Timers/GPIO)
  - IR 200-293: ST Logic status & performance statistics
  - HR 200-237: ST Logic control & interval
  - Address collision avoidance guide
  - Python pymodbus examples

- **[ST_MONITORING.md](ST_MONITORING.md)** - ⭐ **ST Logic performance tuning guide**
  - CLI commands: show logic stats, show logic X timing
  - Performance monitoring workflow
  - Optimization strategies
  - Modbus register access examples
  - Common issues & solutions

- **[TIMING_ANALYSIS.md](TIMING_ANALYSIS.md)** - ST Logic timing deep dive
  - Fixed rate scheduler analysis
  - Execution interval control
  - Jitter analysis and recommendations

- **[BUGS.md](BUGS.md)** - Bug tracking system
  - 7 bugs (all FIXED in v4.1.0)
  - Test plans and function references

- **[CHANGELOG.md](CHANGELOG.md)** - Complete version history
  - v4.1.0: Performance monitoring & Modbus integration
  - v4.0.0-v4.0.2: Persistent registers & watchdog
  - v3.0.0-v3.3.0: Telnet & Wi-Fi features

- **[CLAUDE.md](CLAUDE.md)** - Developer guidelines (Danish)
  - Architecture overview
  - Coding standards
  - Version control workflow

### Feature Guides (docs/)
- **[docs/README_ST_LOGIC.md](docs/README_ST_LOGIC.md)** - ST Logic programming guide
- **[docs/FEATURE_GUIDE.md](docs/FEATURE_GUIDE.md)** - Feature-by-feature documentation
- **[docs/ESP32_Module_Architecture.md](docs/ESP32_Module_Architecture.md)** - Architecture deep-dive
- **[docs/GPIO_MAPPING_GUIDE.md](docs/GPIO_MAPPING_GUIDE.md)** - GPIO configuration guide
- **[docs/COUNTER_COMPARE_REFERENCE.md](docs/COUNTER_COMPARE_REFERENCE.md)** - Counter compare feature
