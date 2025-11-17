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

#define HOLDING_REGS_SIZE   160         // Number of holding registers
#define INPUT_REGS_SIZE     160         // Number of input registers
#define COILS_SIZE          32          // Coil bits (0-255 packed)
#define DISCRETE_INPUTS_SIZE 32         // Discrete input bits (0-255 packed)

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
 * EEPROM / NVS CONFIGURATION
 * ============================================================================ */

#define CONFIG_SCHEMA_VERSION   2       // Current config schema version
#define CONFIG_CRC_SEED         0xFFFF  // CRC16 initial value

/* ============================================================================
 * CLI CONFIGURATION
 * ============================================================================ */

#define CLI_BUFFER_SIZE     256         // CLI command buffer
#define CLI_HISTORY_SIZE    3           // Command history buffer size
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
 * VERSION & BUILD
 * ============================================================================ */

#define PROJECT_NAME        "Modbus RTU Server (ESP32)"
#define PROJECT_VERSION     "1.0.0"
#define BUILD_DATE          __DATE__
#define BUILD_TIME          __TIME__

#endif // CONSTANTS_H
