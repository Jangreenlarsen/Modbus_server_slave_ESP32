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

typedef struct {
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

  // Reserved for alignment
  uint8_t reserved[4];
} CounterConfig;

typedef struct {
  uint32_t counter_value;
  uint32_t last_level;
  uint32_t debounce_timer;
  uint8_t is_counting;
} CounterSWState;

typedef struct {
  uint32_t pcnt_value;
  uint32_t last_count;
  uint32_t overflow_count;
  uint8_t is_counting;
} CounterHWState;

typedef struct {
  CounterConfig config;
  CounterSWState sw_state;
  CounterHWState hw_state;
  uint32_t measured_frequency;
  uint32_t freq_sample_time;
} Counter;

/* ============================================================================
 * TIMER CONFIGURATION
 * ============================================================================ */

typedef struct {
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

  // Reserved for alignment
  uint8_t reserved[8];
} TimerConfig;

typedef struct {
  TimerConfig config;
  uint32_t start_time;
  uint32_t current_phase;
  uint8_t is_running;
  uint8_t output_state;
} Timer;

/* ============================================================================
 * GPIO MAPPING
 * ============================================================================ */

typedef struct {
  uint8_t gpio_pin;
  uint8_t is_input;
  uint8_t coil_index;
  uint8_t associated_counter;  // 0xff if none
  uint8_t associated_timer;    // 0xff if none
} GPIOMapping;

/* ============================================================================
 * PERSISTENT CONFIGURATION (EEPROM/NVS)
 * ============================================================================ */

typedef struct {
  // Schema versioning
  uint8_t schema_version;

  // Modbus configuration
  uint8_t slave_id;
  uint32_t baudrate;

  // Counters (4 maximum)
  CounterConfig counters[COUNTER_COUNT];

  // Timers (4 maximum)
  TimerConfig timers[TIMER_COUNT];

  // GPIO mappings (reserved for future)
  uint8_t gpio_map_count;
  GPIOMapping gpio_maps[16];  // Max 16 static mappings

  // Reserved for future features
  uint8_t reserved[64];

  // CRC checksum (last)
  uint16_t crc16;
} PersistConfig;

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
