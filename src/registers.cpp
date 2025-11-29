/**
 * @file registers.cpp
 * @brief Register and coil storage implementation
 *
 * Provides arrays for holding/input registers and coils/discrete inputs
 * All Modbus read/write operations go through these functions
 *
 * Also handles DYNAMIC register/coil updates from counter/timer sources
 */

#include "registers.h"
#include "counter_engine.h"
#include "counter_config.h"
#include "timer_engine.h"
#include "config_struct.h"
#include "types.h"
#include "constants.h"
#include <Arduino.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * STATIC STORAGE (all registers and coils in RAM)
 * ============================================================================ */

static uint16_t holding_regs[HOLDING_REGS_SIZE] = {0};      // 16-bit registers
static uint16_t input_regs[INPUT_REGS_SIZE] = {0};          // 16-bit registers
static uint8_t coils[COILS_SIZE] = {0};                     // Packed bits (8 per byte)
static uint8_t discrete_inputs[DISCRETE_INPUTS_SIZE] = {0}; // Packed bits (8 per byte)

/* ============================================================================
 * HOLDING REGISTERS (Read/Write)
 * ============================================================================ */

uint16_t registers_get_holding_register(uint16_t addr) {
  if (addr >= HOLDING_REGS_SIZE) return 0;
  return holding_regs[addr];
}

void registers_set_holding_register(uint16_t addr, uint16_t value) {
  if (addr >= HOLDING_REGS_SIZE) return;
  holding_regs[addr] = value;
}

uint16_t* registers_get_holding_regs(void) {
  return holding_regs;
}

/* ============================================================================
 * INPUT REGISTERS (Read-Only from Modbus, Write from drivers)
 * ============================================================================ */

uint16_t registers_get_input_register(uint16_t addr) {
  if (addr >= INPUT_REGS_SIZE) return 0;
  return input_regs[addr];
}

void registers_set_input_register(uint16_t addr, uint16_t value) {
  if (addr >= INPUT_REGS_SIZE) return;
  input_regs[addr] = value;
}

uint16_t* registers_get_input_regs(void) {
  return input_regs;
}

/* ============================================================================
 * COILS (Read/Write) - Packed bits
 * ============================================================================ */

uint8_t registers_get_coil(uint16_t idx) {
  if (idx >= (COILS_SIZE * 8)) return 0;
  uint16_t byte_idx = idx / 8;
  uint16_t bit_idx = idx % 8;
  return (coils[byte_idx] >> bit_idx) & 1;
}

void registers_set_coil(uint16_t idx, uint8_t value) {
  if (idx >= (COILS_SIZE * 8)) return;
  uint16_t byte_idx = idx / 8;
  uint16_t bit_idx = idx % 8;

  if (value) {
    coils[byte_idx] |= (1 << bit_idx);  // Set bit
  } else {
    coils[byte_idx] &= ~(1 << bit_idx); // Clear bit
  }
}

uint8_t* registers_get_coils(void) {
  return coils;
}

/* ============================================================================
 * DISCRETE INPUTS (Read-Only from Modbus, Write from GPIO/sensors)
 * ============================================================================ */

uint8_t registers_get_discrete_input(uint16_t idx) {
  if (idx >= (DISCRETE_INPUTS_SIZE * 8)) return 0;
  uint16_t byte_idx = idx / 8;
  uint16_t bit_idx = idx % 8;
  return (discrete_inputs[byte_idx] >> bit_idx) & 1;
}

void registers_set_discrete_input(uint16_t idx, uint8_t value) {
  if (idx >= (DISCRETE_INPUTS_SIZE * 8)) return;
  uint16_t byte_idx = idx / 8;
  uint16_t bit_idx = idx % 8;

  if (value) {
    discrete_inputs[byte_idx] |= (1 << bit_idx);  // Set bit
  } else {
    discrete_inputs[byte_idx] &= ~(1 << bit_idx); // Clear bit
  }
}

uint8_t* registers_get_discrete_inputs(void) {
  return discrete_inputs;
}

/* ============================================================================
 * UTILITY / INITIALIZATION
 * ============================================================================ */

void registers_init(void) {
  memset(holding_regs, 0, sizeof(holding_regs));
  memset(input_regs, 0, sizeof(input_regs));
  memset(coils, 0, sizeof(coils));
  memset(discrete_inputs, 0, sizeof(discrete_inputs));
}

uint32_t registers_get_millis(void) {
  return millis();
}

/* ============================================================================
 * DYNAMIC REGISTER/COIL UPDATES
 * ============================================================================ */

/**
 * @brief Update DYNAMIC registers from counter/timer sources
 *
 * Iterates through all DYNAMIC register mappings and:
 * 1. Gets value from counter or timer
 * 2. Writes to specified holding register
 *
 * Called once per main loop iteration
 */
void registers_update_dynamic_registers(void) {
  for (uint8_t i = 0; i < g_persist_config.dynamic_reg_count; i++) {
    const DynamicRegisterMapping* dyn = &g_persist_config.dynamic_regs[i];
    uint16_t reg_addr = dyn->register_address;
    uint16_t value = 0;

    if (dyn->source_type == DYNAMIC_SOURCE_COUNTER) {
      uint8_t counter_id = dyn->source_id;
      CounterConfig cfg = {0};

      if (!counter_engine_get_config(counter_id, &cfg) || !cfg.enabled) {
        continue;  // Counter not configured or disabled
      }

      // Get counter value based on function type
      uint64_t raw_value = counter_engine_get_value(counter_id);

      switch (dyn->source_function) {
        case COUNTER_FUNC_INDEX:
          // Scaled value = counterValue × scale_factor
          value = (uint16_t)(raw_value * cfg.scale_factor);
          break;

        case COUNTER_FUNC_RAW:
          // Prescaled value = counterValue / prescaler
          if (cfg.prescaler > 0) {
            value = (uint16_t)(raw_value / cfg.prescaler);
          } else {
            value = (uint16_t)raw_value;
          }
          break;

        case COUNTER_FUNC_FREQ:
          // Frequency in Hz (already updated by counter_frequency_update)
          // Read from freq_reg if configured
          if (cfg.freq_reg < HOLDING_REGS_SIZE) {
            value = registers_get_holding_register(cfg.freq_reg);
          } else {
            value = 0;
          }
          break;

        case COUNTER_FUNC_OVERFLOW:
          // Overflow flag (1 if overflow occurred)
          // Read from overload_reg if configured
          if (cfg.overload_reg < HOLDING_REGS_SIZE) {
            value = registers_get_holding_register(cfg.overload_reg);
          } else {
            value = 0;
          }
          break;

        case COUNTER_FUNC_CTRL:
          // Control register (read current value)
          if (cfg.ctrl_reg < HOLDING_REGS_SIZE) {
            value = registers_get_holding_register(cfg.ctrl_reg);
          } else {
            value = 0;
          }
          break;

        default:
          continue;
      }

      // Write value to holding register
      registers_set_holding_register(reg_addr, value);

    } else if (dyn->source_type == DYNAMIC_SOURCE_TIMER) {
      uint8_t timer_id = dyn->source_id;
      TimerConfig cfg = {0};

      if (!timer_engine_get_config(timer_id, &cfg) || !cfg.enabled) {
        continue;  // Timer not configured or disabled
      }

      // Get timer value based on function type
      uint16_t value = 0;

      switch (dyn->source_function) {
        case TIMER_FUNC_OUTPUT:
          // Timer output state (0 or 1)
          // Read current coil state (timer writes to output_coil via set_coil_level)
          value = registers_get_coil(cfg.output_coil) ? 1 : 0;
          break;

        default:
          continue;
      }

      // Write value to holding register
      registers_set_holding_register(reg_addr, value);
    }
  }
}

/**
 * @brief Update DYNAMIC coils from counter/timer sources
 *
 * Iterates through all DYNAMIC coil mappings and:
 * 1. Gets state from counter or timer
 * 2. Writes to specified coil
 *
 * Called once per main loop iteration
 */
void registers_update_dynamic_coils(void) {
  for (uint8_t i = 0; i < g_persist_config.dynamic_coil_count; i++) {
    const DynamicCoilMapping* dyn = &g_persist_config.dynamic_coils[i];
    uint16_t coil_addr = dyn->coil_address;
    uint8_t value = 0;

    if (dyn->source_type == DYNAMIC_SOURCE_COUNTER) {
      uint8_t counter_id = dyn->source_id;
      CounterConfig cfg = {0};

      if (!counter_engine_get_config(counter_id, &cfg) || !cfg.enabled) {
        continue;  // Counter not configured or disabled
      }

      // Get counter state based on function type
      switch (dyn->source_function) {
        case COUNTER_FUNC_OVERFLOW:
          // Overflow flag
          // TODO: Get overflow state from counter state
          value = 0;  // Placeholder
          break;

        default:
          continue;
      }

      // Write value to coil
      registers_set_coil(coil_addr, value);

    } else if (dyn->source_type == DYNAMIC_SOURCE_TIMER) {
      uint8_t timer_id = dyn->source_id;
      TimerConfig cfg = {0};

      if (!timer_engine_get_config(timer_id, &cfg) || !cfg.enabled) {
        continue;  // Timer not configured or disabled
      }

      // Get timer state based on function type
      uint8_t value = 0;

      switch (dyn->source_function) {
        case TIMER_FUNC_OUTPUT:
          // Timer output state (0 or 1)
          // Read current coil state (timer writes to output_coil via set_coil_level)
          value = registers_get_coil(cfg.output_coil) ? 1 : 0;
          break;

        default:
          continue;
      }

      // Write value to coil
      registers_set_coil(coil_addr, value);
    }
  }
}
