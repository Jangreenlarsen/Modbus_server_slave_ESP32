/**
 * @file types.h
 * @brief Central location for ALL struct definitions
 *
 * This file should be the ONLY place where structs are defined.
 * Prevents duplicate definitions and makes the project structure clear.
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include "constants.h"

/* ============================================================================
 * MODBUS REQUEST STRUCTS
 * ============================================================================ */

/* NOTE: ModbusFrame is defined in modbus_frame.h */

typedef struct {
  uint16_t starting_address;
  uint16_t quantity;
} ModbusReadRequest;

typedef struct {
  uint16_t output_address;
  uint16_t output_value;
} ModbusWriteSingleCoilRequest;

typedef struct {
  uint16_t register_address;
  uint16_t register_value;
} ModbusWriteSingleRegisterRequest;

typedef struct {
  uint16_t starting_address;
  uint16_t quantity_of_outputs;
  uint8_t byte_count;
  uint8_t output_values[MODBUS_FRAME_MAX];
} ModbusWriteMultipleCoilsRequest;

typedef struct {
  uint16_t starting_address;
  uint16_t quantity_of_registers;
  uint8_t byte_count;
  uint16_t register_values[MODBUS_FRAME_MAX / 2];
} ModbusWriteMultipleRegistersRequest;

/* ============================================================================
 * COUNTER CONFIGURATION
 * ============================================================================ */

typedef struct __attribute__((packed)) {
  uint8_t enabled;
  CounterModeEnable mode_enable;
  CounterEdgeType edge_type;
  CounterDirection direction;
  CounterHWMode hw_mode;

  uint16_t prescaler;
  uint8_t bit_width;  // 8, 16, 32, 64
  float scale_factor;

  // Register addresses
  uint16_t index_reg;     // Scaled value register
  uint16_t raw_reg;       // Prescaled value register
  uint16_t freq_reg;      // Frequency (Hz) register
  uint16_t overload_reg;  // Overflow flag register
  uint16_t ctrl_reg;      // Control register

  // Mode-specific
  uint16_t start_value;   // For reset-on-read
  uint8_t debounce_enabled;
  uint16_t debounce_ms;

  // SW polling mode
  uint8_t input_dis;      // Discrete input index

  // SW-ISR mode
  uint8_t interrupt_pin;  // GPIO pin for interrupt

  // HW (PCNT) mode
  uint8_t hw_gpio;        // GPIO pin for PCNT input (BUG FIX 1.9)

  // COMPARE FEATURE (v2.3+)
  uint8_t compare_enabled;      // Enable compare check
  uint8_t compare_mode;         // 0=≥, 1=>, 2=== (exact match)
  uint64_t compare_value;       // Værdi at sammenligne med
  uint8_t reset_on_read;        // Auto-clear bit 4 ved ctrl-reg read

  // Note: Compare status stored in ctrl_reg bit 4 (no separate fields needed)

  // Reserved for alignment
  uint8_t reserved[2];
} CounterConfig;

typedef struct {
  uint64_t counter_value;      // Changed from uint32_t to match usage
  uint32_t last_level;
  uint32_t debounce_timer;
  uint8_t is_counting;
  uint8_t overflow_flag;       // BUG FIX 1.1: Track overflow
} CounterSWState;

typedef struct {
  uint64_t pcnt_value;         // Changed from uint32_t for consistency
  uint32_t last_count;         // Stores last PCNT read (int16_t range)
  uint32_t overflow_count;
  uint8_t is_counting;
} CounterHWState;

typedef struct {
  CounterConfig config;
  CounterSWState sw_state;
  CounterHWState hw_state;
  uint32_t measured_frequency;
  uint32_t freq_sample_time;

  // COMPARE FEATURE RUNTIME STATE (v2.3+)
  uint8_t compare_triggered;  // Flag: Compare værdi nået denne iteration
  uint32_t compare_time_ms;   // Timestamp når triggered
  uint64_t last_value;        // Previous counter value (for exact match detection)
} Counter;

/* ============================================================================
 * TIMER CONFIGURATION
 * ============================================================================ */

typedef struct __attribute__((packed)) {
  uint8_t enabled;
  TimerMode mode;

  // Mode 1: One-shot
  uint32_t phase1_duration_ms;
  uint32_t phase2_duration_ms;
  uint32_t phase3_duration_ms;
  uint8_t phase1_output_state;
  uint8_t phase2_output_state;
  uint8_t phase3_output_state;

  // Mode 2: Monostable
  uint32_t pulse_duration_ms;
  uint8_t trigger_level;

  // Mode 3: Astable
  uint32_t on_duration_ms;
  uint32_t off_duration_ms;

  // Mode 4: Input-triggered
  uint8_t input_dis;
  uint32_t delay_ms;
  uint8_t trigger_edge;

  // Output
  uint16_t output_coil;

  // Control register (Modbus holding register for start/stop/reset)
  uint16_t ctrl_reg;

  // Reserved for alignment
  uint8_t reserved[6];
} TimerConfig;

typedef struct {
  TimerConfig config;
  uint32_t start_time;
  uint32_t current_phase;
  uint8_t is_running;
  uint8_t output_state;
} Timer;

/* ============================================================================
 * REGISTER MAPPING (STATIC & DYNAMIC)
 * ============================================================================ */

typedef struct __attribute__((packed)) {
  uint16_t register_address;
  uint16_t static_value;        // STATIC: hardcoded value
} StaticRegisterMapping;

typedef struct __attribute__((packed)) {
  uint16_t register_address;
  uint8_t source_type;          // DYNAMIC_SOURCE_COUNTER or DYNAMIC_SOURCE_TIMER
  uint8_t source_id;            // Counter/Timer ID (1-4)
  uint8_t source_function;      // CounterFunction or TimerFunction enum
} DynamicRegisterMapping;

/* ============================================================================
 * COIL MAPPING (STATIC & DYNAMIC)
 * ============================================================================ */

typedef struct __attribute__((packed)) {
  uint16_t coil_address;
  uint8_t static_value;         // STATIC: 0 (OFF) or 1 (ON)
} StaticCoilMapping;

typedef struct __attribute__((packed)) {
  uint16_t coil_address;
  uint8_t source_type;          // DYNAMIC_SOURCE_COUNTER or DYNAMIC_SOURCE_TIMER
  uint8_t source_id;            // Counter/Timer ID (1-4)
  uint8_t source_function;      // CounterFunction or TimerFunction enum
} DynamicCoilMapping;

/* ============================================================================
 * UNIFIED VARIABLE MAPPING (GPIO pins + ST variables ↔ Modbus registers)
 * ============================================================================ */

typedef struct __attribute__((packed)) {
  // Source type: what is being mapped
  uint8_t source_type;          // MAPPING_SOURCE_GPIO, MAPPING_SOURCE_ST_VAR

  // GPIO mapping (if source_type == MAPPING_SOURCE_GPIO)
  uint8_t gpio_pin;
  uint8_t associated_counter;   // 0xff if none (set via input-dis=<pin>)
  uint8_t associated_timer;     // 0xff if none

  // ST Variable mapping (if source_type == MAPPING_SOURCE_ST_VAR)
  uint8_t st_program_id;        // Logic program ID (0-3), 0xff if none
  uint8_t st_var_index;         // ST variable index (0-31)

  // I/O Configuration
  uint8_t is_input;             // 1 = INPUT mode (source → register), 0 = OUTPUT mode (register → source)
  uint8_t input_type;           // 0 = Holding Register (HR), 1 = Discrete Input (DI) - only for INPUT mode
  uint16_t input_reg;           // Input register index (65535 if none) - for INPUT mode
  uint16_t coil_reg;            // Coil/output register index (65535 if none) - for OUTPUT mode
} VariableMapping;

/* ============================================================================
 * NETWORK CONFIGURATION (v3.0+)
 * ============================================================================ */

typedef struct __attribute__((packed)) {
  uint8_t enabled;                              // Wi-Fi enabled (1) or disabled (0)
  uint8_t dhcp_enabled;                         // 1 = DHCP, 0 = static IP
  char ssid[WIFI_SSID_MAX_LEN];                 // Wi-Fi network name
  char password[WIFI_PASSWORD_MAX_LEN];         // Wi-Fi password (WPA2)

  // Static IP configuration (used if dhcp_enabled == 0)
  uint32_t static_ip;                           // Static IP address (network byte order)
  uint32_t static_gateway;                      // Gateway IP
  uint32_t static_netmask;                      // Netmask
  uint32_t static_dns;                          // Primary DNS

  // Telnet configuration
  uint8_t telnet_enabled;                       // 1 = Telnet server enabled
  uint16_t telnet_port;                         // Telnet port (default 23)

  // Reserved for future (SSH, mDNS, etc.)
  uint8_t reserved[30];                         // Future: SSH, certificates, mDNS
} NetworkConfig;

/* ============================================================================
 * PERSISTENT CONFIGURATION (EEPROM/NVS)
 * ============================================================================ */

typedef struct __attribute__((packed)) {
  // Schema versioning
  uint8_t schema_version;

  // Modbus configuration
  uint8_t slave_id;
  uint32_t baudrate;

  // Network configuration (v3.0+)
  NetworkConfig network;

  // Counters (4 maximum)
  CounterConfig counters[COUNTER_COUNT];

  // Timers (4 maximum)
  TimerConfig timers[TIMER_COUNT];

  // STATIC Register mappings
  uint8_t static_reg_count;
  StaticRegisterMapping static_regs[MAX_DYNAMIC_REGS];

  // DYNAMIC Register mappings
  uint8_t dynamic_reg_count;
  DynamicRegisterMapping dynamic_regs[MAX_DYNAMIC_REGS];

  // STATIC Coil mappings
  uint8_t static_coil_count;
  StaticCoilMapping static_coils[MAX_DYNAMIC_COILS];

  // DYNAMIC Coil mappings
  uint8_t dynamic_coil_count;
  DynamicCoilMapping dynamic_coils[MAX_DYNAMIC_COILS];

  // Variable mappings (GPIO pins + ST variables)
  uint8_t var_map_count;
  VariableMapping var_maps[64];  // 32 GPIO + ST variable bindings

  // GPIO2 configuration (heartbeat control)
  uint8_t gpio2_user_mode;  // 0 = heartbeat mode (default), 1 = user mode (GPIO2 available)

  // Reserved for future features
  uint8_t reserved[8];  // Reserved space for future use

  // CRC checksum (last)
  uint16_t crc16;
} PersistConfig;

typedef struct {
  // Runtime state (not persisted)
  uint8_t wifi_connected;                       // Current Wi-Fi connection status
  uint8_t telnet_client_connected;              // Telnet client connected
  uint32_t wifi_connect_time_ms;                // When last connected
  uint32_t wifi_reconnect_retries;              // Current reconnect attempt count

  // IP information (DHCP or static)
  uint32_t local_ip;                            // Current local IP
  uint32_t gateway;                             // Current gateway
  uint32_t netmask;                             // Current netmask
  uint32_t dns;                                 // Current DNS

  // Client socket
  int telnet_socket;                            // Socket descriptor (-1 if none)
} NetworkState;

/* ============================================================================
 * DEBUG FLAGS (RUNTIME, NOT PERSISTED)
 * ============================================================================ */

typedef struct {
  uint8_t config_save;        // Show debug when saving config to NVS
  uint8_t config_load;        // Show debug when loading config from NVS
  uint8_t wifi_connect;       // Show debug when connecting WiFi (network_manager, wifi_driver)
  uint8_t network_validate;   // Show debug for network config validation
} DebugFlags;

/* ============================================================================
 * RUNTIME STATE (NOT PERSISTED)
 * ============================================================================ */

typedef struct {
  Counter counters[COUNTER_COUNT];
  Timer timers[TIMER_COUNT];

  // Modbus state
  uint8_t modbus_tx_in_progress;
  uint32_t modbus_last_rx_time;

  // CLI state
  uint8_t cli_active;
  uint32_t cli_last_input_time;
} RuntimeState;

#endif // TYPES_H
