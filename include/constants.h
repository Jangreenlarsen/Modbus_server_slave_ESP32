/**
 * @file constants.h
 * @brief Central location for ALL constants and enums
 *
 * This file should be the ONLY place where constants and enums are defined.
 * Prevents duplicate definitions across multiple files.
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>

/* ============================================================================
 * MODBUS CONFIGURATION
 * ============================================================================ */

#define SLAVE_ID            1           // Default Modbus slave address
#define BAUDRATE            115200      // Default Modbus RTU baudrate
#define MODBUS_FRAME_MAX    256         // Max Modbus frame size
#define MODBUS_TIMEOUT_MS   3500        // Inter-character timeout (ms)

/* Modbus Function Codes */
#define FC_READ_COILS           0x01
#define FC_READ_DISCRETE_INPUTS 0x02
#define FC_READ_HOLDING_REGS    0x03
#define FC_READ_INPUT_REGS      0x04
#define FC_WRITE_SINGLE_COIL    0x05
#define FC_WRITE_SINGLE_REG     0x06
#define FC_WRITE_MULTIPLE_COILS 0x0F
#define FC_WRITE_MULTIPLE_REGS  0x10

/* ============================================================================
 * REGISTER/COIL CONFIGURATION
 * ============================================================================ */

#define HOLDING_REGS_SIZE   256         // Number of holding registers (0-255)
#define INPUT_REGS_SIZE     256         // Number of input registers (0-255)
#define COILS_SIZE          32          // Coil bits (0-255 packed)
#define DISCRETE_INPUTS_SIZE 32         // Discrete input bits (0-255 packed)

/* ============================================================================
 * ST LOGIC REGISTER MAPPING (Input/Holding Registers 200+)
 * ============================================================================ */

// INPUT REGISTERS (Read-only status)
#define ST_LOGIC_STATUS_REG_BASE        200  // Logic1-4 Status (200-203)
#define ST_LOGIC_EXEC_COUNT_REG_BASE    204  // Logic1-4 Execution Count (204-207)
#define ST_LOGIC_ERROR_COUNT_REG_BASE   208  // Logic1-4 Error Count (208-211)
#define ST_LOGIC_ERROR_CODE_REG_BASE    212  // Logic1-4 Last Error Code (212-215)
#define ST_LOGIC_VAR_COUNT_REG_BASE     216  // Logic1-4 Variable Count (216-219)
#define ST_LOGIC_VAR_VALUES_REG_BASE    220  // Logic1-4 Variable Values (220-251)

// PERFORMANCE STATISTICS (v4.1.0) - Input Registers 252-291
#define ST_LOGIC_MIN_EXEC_TIME_REG_BASE 252  // Logic1-4 Min Execution Time µs, 32-bit (252-259, 2 regs each)
#define ST_LOGIC_MAX_EXEC_TIME_REG_BASE 260  // Logic1-4 Max Execution Time µs, 32-bit (260-267, 2 regs each)
#define ST_LOGIC_AVG_EXEC_TIME_REG_BASE 268  // Logic1-4 Avg Execution Time µs, 32-bit (268-275, 2 regs each)
#define ST_LOGIC_OVERRUN_COUNT_REG_BASE 276  // Logic1-4 Overrun Count, 32-bit (276-283, 2 regs each)

// GLOBAL CYCLE STATISTICS (v4.1.0) - Input Registers 284-293
#define ST_LOGIC_CYCLE_MIN_REG          284  // Global cycle min time ms, 32-bit (284-285)
#define ST_LOGIC_CYCLE_MAX_REG          286  // Global cycle max time ms, 32-bit (286-287)
#define ST_LOGIC_CYCLE_OVERRUN_REG      288  // Global cycle overrun count, 32-bit (288-289)
#define ST_LOGIC_TOTAL_CYCLES_REG       290  // Total cycles executed, 32-bit (290-291)
#define ST_LOGIC_EXEC_INTERVAL_RO_REG   292  // Execution interval ms (read-only copy), 32-bit (292-293)

// HOLDING REGISTERS (Read/Write control)
#define ST_LOGIC_CONTROL_REG_BASE       200  // Logic1-4 Control (200-203)
#define ST_LOGIC_VAR_INPUT_REG_BASE     204  // Logic1-4 Variable Input (204-235)
#define ST_LOGIC_EXEC_INTERVAL_RW_REG   236  // Execution interval ms (read-write), 32-bit (236-237)

// Status Register Bit Definitions
#define ST_LOGIC_STATUS_ENABLED         0x0001  // Bit 0: Program enabled
#define ST_LOGIC_STATUS_COMPILED        0x0002  // Bit 1: Program compiled
#define ST_LOGIC_STATUS_RUNNING         0x0004  // Bit 2: Currently executing
#define ST_LOGIC_STATUS_ERROR           0x0008  // Bit 3: Has error

// Control Register Bit Definitions
#define ST_LOGIC_CONTROL_ENABLE         0x0001  // Bit 0: Enable/disable
#define ST_LOGIC_CONTROL_START          0x0002  // Bit 1: Start/stop
#define ST_LOGIC_CONTROL_RESET_ERROR    0x0004  // Bit 2: Reset error flag

/* ============================================================================
 * COUNTER CONFIGURATION
 * ============================================================================ */

#define COUNTER_COUNT       4           // 4 counters maximum
#define COUNTER_VALUE_MAX   0xFFFFFFFF  // 32-bit max

typedef enum {
  COUNTER_MODE_DISABLED = 0,
  COUNTER_MODE_ENABLED = 1
} CounterModeEnable;

typedef enum {
  COUNTER_EDGE_RISING = 0,
  COUNTER_EDGE_FALLING = 1,
  COUNTER_EDGE_BOTH = 2
} CounterEdgeType;

typedef enum {
  COUNTER_HW_SW = 0,           // Software polling
  COUNTER_HW_SW_ISR = 1,       // Software ISR (GPIO interrupt)
  COUNTER_HW_PCNT = 2          // Hardware PCNT (ESP32 Timer5-equivalent)
} CounterHWMode;

typedef enum {
  COUNTER_DIR_UP = 0,
  COUNTER_DIR_DOWN = 1
} CounterDirection;

#define COUNTER_PRESCALER_VALUES {1, 4, 8, 16, 64, 256, 1024}  // Supported prescalers

/* ============================================================================
 * TIMER CONFIGURATION
 * ============================================================================ */

#define TIMER_COUNT         4           // 4 timers maximum

typedef enum {
  TIMER_MODE_DISABLED = 0,
  TIMER_MODE_1_ONESHOT = 1,
  TIMER_MODE_2_MONOSTABLE = 2,
  TIMER_MODE_3_ASTABLE = 3,
  TIMER_MODE_4_INPUT_TRIGGERED = 4
} TimerMode;

/* ============================================================================
 * MODBUS VALUE TYPES (for multi-register support)
 * ============================================================================ */

typedef enum {
  MODBUS_TYPE_UINT = 0,     // 16-bit unsigned (0-65535)
  MODBUS_TYPE_INT = 1,      // 16-bit signed (-32768 to 32767)
  MODBUS_TYPE_DINT = 2,     // 32-bit signed (2 registers)
  MODBUS_TYPE_DWORD = 3,    // 32-bit unsigned (2 registers)
  MODBUS_TYPE_REAL = 4      // 32-bit IEEE-754 float (2 registers)
} ModbusValueType;

/* ============================================================================
 * DYNAMIC REGISTER/COIL CONFIGURATION
 * ============================================================================ */

#define MAX_DYNAMIC_REGS    16          // Max DYNAMIC register mappings
#define MAX_DYNAMIC_COILS   16          // Max DYNAMIC coil mappings

typedef enum {
  DYNAMIC_SOURCE_COUNTER = 0,
  DYNAMIC_SOURCE_TIMER = 1
} DynamicSourceType;

// Counter functions
typedef enum {
  COUNTER_FUNC_INDEX = 0,       // Scaled value (index-reg)
  COUNTER_FUNC_RAW = 1,         // Prescaled value (raw-reg)
  COUNTER_FUNC_FREQ = 2,        // Frequency in Hz (freq-reg)
  COUNTER_FUNC_OVERFLOW = 3,    // Overflow flag (overload-reg)
  COUNTER_FUNC_CTRL = 4         // Control register (ctrl-reg)
} CounterFunction;

// Timer functions
typedef enum {
  TIMER_FUNC_OUTPUT = 0          // Output state (phase output, astable state, etc)
} TimerFunction;

/* ============================================================================
 * VARIABLE MAPPING CONFIGURATION (UNIFIED GPIO + ST VARIABLES)
 * ============================================================================ */

typedef enum {
  MAPPING_SOURCE_GPIO = 0,       // Map GPIO pin
  MAPPING_SOURCE_ST_VAR = 1      // Map ST Logic variable
} VariableMappingSourceType;

/* ============================================================================
 * EEPROM / NVS CONFIGURATION
 * ============================================================================ */

#define CONFIG_SCHEMA_VERSION   9       // Current config schema version (v4.7.1+: STATIC register multi-register type support)
#define CONFIG_CRC_SEED         0xFFFF  // CRC16 initial value

/* ============================================================================
 * CLI CONFIGURATION
 * ============================================================================ */

#define CLI_BUFFER_SIZE     256         // CLI command buffer
#define CLI_HISTORY_SIZE    10          // Command history buffer size
#define CLI_TOKEN_MAX       20          // Max tokens per command

/* ============================================================================
 * HARDWARE PINS (ESP32-WROOM-32)
 * ============================================================================ */

#define PIN_UART1_RX        4           // GPIO4
#define PIN_UART1_TX        5           // GPIO5
#define PIN_RS485_DIR       15          // GPIO15 (RS-485 direction control)

// Counter pins (GPIO interrupt for SW-ISR mode)
#define PIN_INT1            16          // GPIO16 (available)
#define PIN_INT2            17          // GPIO17 (available)
#define PIN_INT3            18          // GPIO18 (available)
#define PIN_INT4            19          // GPIO19 (PCNT unit0 input)

// I2C pins (future expansion)
#define PIN_I2C_SDA         21          // GPIO21
#define PIN_I2C_SCL         22          // GPIO22

// SPI pins (future W5500)
#define PIN_SPI_MISO        12          // GPIO12 (HSPI MISO)
#define PIN_SPI_MOSI        13          // GPIO13 (HSPI MOSI)
#define PIN_SPI_CLK         14          // GPIO14 (HSPI CLK)
#define PIN_SPI_CS          23          // GPIO23 (HSPI CS)

/* ============================================================================
 * SERIAL CONFIGURATION
 * ============================================================================ */

#define SERIAL_BAUD_DEBUG   115200      // USB serial (UART0) baud rate
#define SERIAL_BAUD_MODBUS  115200      // Modbus RTU (UART1) baud rate

/* ============================================================================
 * TIMING CONSTANTS
 * ============================================================================ */

#define HEARTBEAT_INTERVAL_MS   500     // LED blink interval
#define FREQUENCY_MEAS_WINDOW_MS 1000   // Frequency measurement window (1-2 sec)
#define COUNTER_DEBOUNCE_MS     10      // Default debounce filter

/* ============================================================================
 * DEBUG FLAGS (runtime controllable)
 * ============================================================================ */

#define DEBUG_CONFIG_SAVE       1       // Show debug when saving config to NVS
#define DEBUG_CONFIG_LOAD       1       // Show debug when loading config from NVS

/* ============================================================================
 * NETWORK CONFIGURATION (Wi-Fi, Telnet)
 * ============================================================================ */

#define NETWORK_ENABLED                 1           // 0 = disabled, 1 = enabled
#define WIFI_MODE_STATION               1           // Client mode (not AP mode)
#define WIFI_SSID_MAX_LEN               32          // Max SSID length (IEEE 802.11)
#define WIFI_PASSWORD_MAX_LEN           64          // Max password length (WPA2)
#define WIFI_SCAN_TIMEOUT_MS            10000       // Wi-Fi scan timeout
#define WIFI_CONNECT_TIMEOUT_MS         15000       // Connection timeout
#define WIFI_RECONNECT_INTERVAL_MS      5000        // Retry interval on disconnect
#define WIFI_RECONNECT_MAX_RETRIES      10          // Max reconnection attempts

#define TELNET_PORT                     23          // Telnet standard port
#define TELNET_MAX_CLIENTS              1           // Single client for simplicity
#define TELNET_BUFFER_SIZE              256         // Per-client input buffer
#define TELNET_READ_TIMEOUT_MS          0           // Disabled - no idle timeout (use "exit" to disconnect)
#define TELNET_NEWLINE_CHAR             '\n'        // Telnet uses LF for line ending

/* Telnet IAC (Interpret As Command) protocol bytes */
#define TELNET_IAC                      255         // Interpret As Command
#define TELNET_DONT                     254         // Don't enable option
#define TELNET_DO                       253         // Enable option
#define TELNET_WONT                     252         // Won't enable option
#define TELNET_WILL                     251         // Will enable option
#define TELNET_SB                       250         // Subnegotiation start
#define TELNET_SE                       240         // Subnegotiation end

/* Telnet options */
#define TELNET_OPT_ECHO                 1           // Echo
#define TELNET_OPT_SUPPRESS_GA          3           // Suppress Go Ahead
#define TELNET_OPT_LINEMODE             34          // Line mode

#define DHCP_ENABLED                    1           // Use DHCP (vs static IP)
#define DHCP_HOSTNAME                   "modbus-esp32"  // DHCP hostname

/* NVS namespace for network config */
#define NVS_NAMESPACE_NETWORK           "network"

/* ============================================================================
 * MODBUS MASTER CONFIGURATION (UART1)
 * ============================================================================ */

// Hardware pins (fixed)
#define MODBUS_MASTER_TX_PIN      25    // UART1 TX
#define MODBUS_MASTER_RX_PIN      26    // UART1 RX
#define MODBUS_MASTER_DE_PIN      27    // MAX485 DE/RE control

// Default configuration
#define MODBUS_MASTER_DEFAULT_BAUDRATE     9600
#define MODBUS_MASTER_DEFAULT_PARITY       0     // 0=none, 1=even, 2=odd
#define MODBUS_MASTER_DEFAULT_STOP_BITS    1
#define MODBUS_MASTER_DEFAULT_TIMEOUT      500   // ms
#define MODBUS_MASTER_DEFAULT_INTER_FRAME  10    // ms
#define MODBUS_MASTER_DEFAULT_MAX_REQUESTS 10    // per cycle

// Protocol constants
#define MODBUS_MASTER_MIN_RESPONSE_TIME    3     // ms (minimum inter-frame delay)
#define MODBUS_MASTER_MAX_RETRIES          0     // No retries (ST Logic handles it)

/* ============================================================================
 * VERSION & BUILD
 * ============================================================================ */

#define PROJECT_NAME        "Modbus RTU Server (ESP32)"
#define PROJECT_VERSION     "4.8.0"
// BUILD_DATE and BUILD_NUMBER now in build_version.h (auto-generated)

/* Version history:
 * v4.8.0 (2026-01-07): Signal Processing Function Blocks + SR/RS Latches
 *                      - FEAT: SCALE(IN, IN_MIN, IN_MAX, OUT_MIN, OUT_MAX) - Linear scaling/mapping (5-arg stateless)
 *                      - FEAT: HYSTERESIS(IN, HIGH, LOW) - Schmitt trigger with dead zone (stateful, 1 byte)
 *                      - FEAT: BLINK(ENABLE, ON_TIME, OFF_TIME) - Pulse generator (stateful, 6 bytes)
 *                      - FEAT: FILTER(IN, TIME_CONSTANT) - Exponential moving average low-pass (stateful, 4 bytes)
 *                      - FEAT: SR/RS bistable latches with priority handling (v4.7.3, Build #999)
 *                      - Extended VM to support 5-argument functions (previously max 3)
 *                      - Memory: +88 bytes per program (452→540 bytes)
 *                      - Documentation: ST_SIGNAL_PROCESSING_DESIGN.md, ST_LATCH_TEST_CASES.md
 *                      - Build #999-1006
 * v4.7.2 (2026-01-04): CLI Consistency & Documentation - Holding-Reg vs Input-Reg Naming
 *                      - BUG-144: CLI naming standardiseret (holding-reg vs input-reg)
 *                      - BUG-145: Fixed missing "read input-reg" i help message
 *                      - BUG-143: Dokumenteret IR 220-251 begrænsning (8 vars/program)
 *                      - MODBUS_REGISTER_MAP.md opdateret med CLI kommando guide
 *                      - Alle help messages konsekvent: "holding-reg" vs "input-reg"
 *                      - Build #973-974
 * v4.7.1 (2026-01-04): Persistent Register Type Support & Documentation Updates
 *                      - FEAT-001: set reg STATIC multi-type support (UINT/INT/DINT/DWORD/REAL)
 *                      - Schema migration v8 → v9 (StaticRegisterMapping struct expansion)
 *                      - Register range validation (ST Logic HR 200-237 protection)
 *                      - Documentation corrections (MODBUS_REGISTER_MAP.md HR/IR ranges)
 *                      - BUG-142: Identified range validation too broad (pending fix)
 *                      - Build #966-972
 * v4.7.0 (2026-01-01): Advanced ST Functions & Memory Optimization
 *                      - 13 new ST functions: EXP/LN/LOG/POW, R_TRIG/F_TRIG, TON/TOF/TP, CTU/CTD/CTUD
 *                      - Stateful function blocks with automatic instance allocation
 *                      - Memory optimization: +17.8KB overflow → +32 bytes (99.8% reduction)
 *                      - Input validation: NaN/INF protection, overflow/underflow guards
 *                      - Documentation: ST_FUNCTIONS_V47.md, RELEASE_NOTES_V47.md
 * v4.4.0 (2025-12-30): CLI Documentation & ST Logic Debugging Enhancements
 *                      - Comprehensive CLI Commands Reference (650+ lines)
 *                      - show logic <id> bytecode command
 *                      - show config reorganization (8 sections with ST variable annotations)
 *                      - README Table of Contents refactor (all 16 documents)
 *                      Fixed: BUG-126, BUG-127, BUG-128 (normalize_alias BYTECODE/TIMING)
 * v4.3.0 (2025-12-20): Auto-Load Persistent Register Groups on boot + CLI parser fixes
 * v4.2.9 (2025-12-18): Counter control parameter restructuring
 * v3.1.1 (2025-12-08): Telnet insert mode & ST upload copy/paste
 * v3.1.0 (2025-12-05): WiFi display & validation improvements
 * v3.0.0 (2025-12-05): WiFi/Network subsystem + Phase 1 security fixes
 * v2.2.0 (2025-12-04): ST Logic Modbus integration & extended registers
 */

#endif // CONSTANTS_H
