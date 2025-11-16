/**
 * @file counter_engine.cpp
 * @brief Counter orchestration and state machine (LAYER 5)
 *
 * Ported from: Mega2560 v3.6.5 modbus_counters.cpp (complete orchestration)
 * Adapted to: ESP32 modular architecture
 *
 * Responsibility:
 * - Initialize all counter modes (SW/ISR/HW)
 * - Dispatch to mode-specific handlers via loop()
 * - Write counter values to Modbus registers with prescaler division
 * - Handle control register commands (reset, start, stop)
 * - Update frequency and overflow registers
 *
 * KEY: UNIFIED PRESCALER STRATEGY
 * - Mode files count ALL edges (no skipping)
 * - This file divides by prescaler at output only
 * - value register = counterValue × scale
 * - raw register = counterValue / prescaler
 * - frequency register = measured Hz (no prescaler)
 *
 * Context: This is the "orchestrator" that ties all modes together
 */

#include "counter_engine.h"
#include "counter_config.h"
#include "counter_sw.h"
#include "counter_sw_isr.h"
#include "counter_hw.h"
#include "counter_frequency.h"
#include "registers.h"
#include "constants.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void counter_engine_init(void) {
  // Initialize configuration system
  counter_config_init();

  // Initialize all 4 counters for all modes
  for (uint8_t id = 1; id <= 4; id++) {
    counter_sw_init(id);
    counter_sw_isr_init(id);
    counter_hw_init(id);
    counter_frequency_init(id);
  }
}

/* ============================================================================
 * MAIN LOOP - DISPATCH TO MODE HANDLERS
 * ============================================================================ */

void counter_engine_loop(void) {
  for (uint8_t id = 1; id <= 4; id++) {
    CounterConfig cfg;
    if (!counter_config_get(id, &cfg) || !cfg.enabled) {
      continue;
    }

    // Handle control register commands first
    counter_engine_handle_control(id);

    // Dispatch to mode-specific handler
    switch (cfg.hw_mode) {
      case COUNTER_HW_SW:
        // Software polling mode
        counter_sw_loop(id);
        break;

      case COUNTER_HW_SW_ISR:
        // Software ISR mode
        counter_sw_isr_loop(id);
        break;

      case COUNTER_HW_PCNT:
        // Hardware PCNT mode
        counter_hw_loop(id);
        break;

      default:
        // Unknown mode - skip
        continue;
    }

    // Update frequency measurement (works for all modes)
    uint64_t current_value = counter_engine_get_value(id);
    counter_frequency_update(id, current_value);

    // Write to Modbus registers (prescaler division happens here)
    counter_engine_store_value_to_registers(id);
  }
}

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

bool counter_engine_configure(uint8_t id, const CounterConfig* cfg) {
  if (cfg == NULL) return false;

  // Validate and set configuration
  if (!counter_config_set(id, cfg)) {
    return false;
  }

  // Initialize the chosen mode
  switch (cfg->hw_mode) {
    case COUNTER_HW_SW:
      counter_sw_init(id);
      break;

    case COUNTER_HW_SW_ISR:
      counter_sw_isr_init(id);
      // Attach interrupt if pin configured
      if (cfg->interrupt_pin > 0) {
        counter_sw_isr_attach(id, cfg->interrupt_pin);
      }
      break;

    case COUNTER_HW_PCNT:
      counter_hw_init(id);
      // Configure PCNT if mode enabled
      if (cfg->enabled) {
        // Determine GPIO pin for this counter
        // For now: use a default based on counter ID
        uint8_t gpio_pin = 19 + (id - 1) * 6;  // GPIO 19, 25, 31, 37
        counter_hw_configure(id, gpio_pin);
      }
      break;

    default:
      return false;
  }

  // Reset frequency tracking
  counter_frequency_reset(id);

  return true;
}

/* ============================================================================
 * RESET
 * ============================================================================ */

void counter_engine_reset(uint8_t id) {
  if (id < 1 || id > 4) return;

  CounterConfig cfg;
  if (!counter_config_get(id, &cfg)) return;

  // Reset based on mode
  switch (cfg.hw_mode) {
    case COUNTER_HW_SW:
      counter_sw_reset(id);
      break;
    case COUNTER_HW_SW_ISR:
      counter_sw_isr_reset(id);
      break;
    case COUNTER_HW_PCNT:
      counter_hw_reset(id);
      break;
    default:
      break;
  }

  // Reset frequency tracking
  counter_frequency_reset(id);

  // Clear overflow register
  if (cfg.overload_reg < HOLDING_REGS_SIZE) {
    registers_set_holding_register(cfg.overload_reg, 0);
  }
}

void counter_engine_reset_all(void) {
  for (uint8_t id = 1; id <= 4; id++) {
    counter_engine_reset(id);
  }
}

/* ============================================================================
 * CONTROL REGISTER HANDLING (reset, start, stop bits)
 * ============================================================================ */

void counter_engine_handle_control(uint8_t id) {
  if (id < 1 || id > 4) return;

  CounterConfig cfg;
  if (!counter_config_get(id, &cfg) || cfg.ctrl_reg >= HOLDING_REGS_SIZE) {
    return;
  }

  uint16_t ctrl_val = registers_get_holding_register(cfg.ctrl_reg);

  // Bit 0: Reset command
  if (ctrl_val & 0x0001) {
    counter_engine_reset(id);
    registers_set_holding_register(cfg.ctrl_reg, ctrl_val & ~0x0001);
  }

  // Bit 1: Start command (not implemented in modular version yet)
  // Bit 2: Stop command (not implemented in modular version yet)
  // These would enable/disable the counter runtime

  // Bit 3: Reset-on-read (sticky bit, remains set until cleared by user)
  // This is handled at Modbus FC level, not here
}

/* ============================================================================
 * REGISTER VALUE STORAGE (with prescaler division)
 * UNIFIED PRESCALER STRATEGY: ALL division happens here
 * ============================================================================ */

void counter_engine_store_value_to_registers(uint8_t id) {
  if (id < 1 || id > 4) return;

  CounterConfig cfg;
  if (!counter_config_get(id, &cfg) || !cfg.enabled) {
    return;
  }

  // Get current value from appropriate mode
  uint64_t counter_value = counter_engine_get_value(id);

  // PRESCALER DIVISION: raw = counterValue / prescaler
  // (counterValue already includes ALL edges, no skipping)
  uint64_t raw_value = counter_value;
  if (cfg.prescaler > 1) {
    raw_value = counter_value / cfg.prescaler;
  }

  // SCALING: scaled = counterValue × scale
  double scale = (cfg.scale_factor > 0.0f) ? (double)cfg.scale_factor : 1.0;
  double scaled_float = (double)counter_value * scale;

  // Clamp to bit width
  uint8_t bw = cfg.bit_width;
  uint64_t max_val = 0;
  switch (bw) {
    case 8:
      max_val = 0xFFULL;
      break;
    case 16:
      max_val = 0xFFFFULL;
      break;
    case 32:
      max_val = 0xFFFFFFFFULL;
      break;
    case 64:
    default:
      max_val = 0xFFFFFFFFFFFFFFFFULL;
      break;
  }

  if (scaled_float < 0.0) scaled_float = 0.0;
  if (scaled_float > (double)max_val) scaled_float = (double)max_val;

  uint64_t scaled_value = (uint64_t)(scaled_float + 0.5);  // Round to nearest
  scaled_value &= max_val;
  raw_value &= max_val;

  // Write scaled value to index register (multi-word if 32/64 bit)
  if (cfg.index_reg < HOLDING_REGS_SIZE) {
    uint8_t words = (bw <= 16) ? 1 : (bw == 32) ? 2 : 4;
    for (uint8_t w = 0; w < words && cfg.index_reg + w < HOLDING_REGS_SIZE; w++) {
      uint16_t word = (uint16_t)((scaled_value >> (16 * w)) & 0xFFFF);
      registers_set_holding_register(cfg.index_reg + w, word);
    }
  }

  // Write raw (prescaled) value to raw register
  if (cfg.raw_reg < HOLDING_REGS_SIZE) {
    uint8_t words = (bw <= 16) ? 1 : (bw == 32) ? 2 : 4;
    for (uint8_t w = 0; w < words && cfg.raw_reg + w < HOLDING_REGS_SIZE; w++) {
      uint16_t word = (uint16_t)((raw_value >> (16 * w)) & 0xFFFF);
      registers_set_holding_register(cfg.raw_reg + w, word);
    }
  }

  // Write frequency to freq register (no prescaler compensation)
  if (cfg.freq_reg < HOLDING_REGS_SIZE) {
    uint16_t freq_hz = counter_frequency_get(id);
    registers_set_holding_register(cfg.freq_reg, freq_hz);
  }

  // Write overflow flag to overflow register
  if (cfg.overload_reg < HOLDING_REGS_SIZE) {
    uint8_t overflow = 0;
    switch (cfg.hw_mode) {
      case COUNTER_HW_SW:
        overflow = counter_sw_get_overflow(id);
        break;
      case COUNTER_HW_SW_ISR:
        overflow = counter_sw_isr_get_overflow(id);
        break;
      case COUNTER_HW_PCNT:
        // HW mode: overflow tracking is built into PCNT ISR (not used currently)
        overflow = 0;
        break;
      default:
        break;
    }
    registers_set_holding_register(cfg.overload_reg, overflow ? 1 : 0);
  }
}

/* ============================================================================
 * CONFIGURATION ACCESS
 * ============================================================================ */

bool counter_engine_get_config(uint8_t id, CounterConfig* out) {
  return counter_config_get(id, out);
}

/* ============================================================================
 * VALUE ACCESS (returns counter value before prescaler/scale)
 * ============================================================================ */

uint64_t counter_engine_get_value(uint8_t id) {
  if (id < 1 || id > 4) return 0;

  CounterConfig cfg;
  if (!counter_config_get(id, &cfg)) return 0;

  // Get value from appropriate mode
  switch (cfg.hw_mode) {
    case COUNTER_HW_SW:
      return counter_sw_get_value(id);

    case COUNTER_HW_SW_ISR:
      return counter_sw_isr_get_value(id);

    case COUNTER_HW_PCNT:
      return counter_hw_get_value(id);

    default:
      return 0;
  }
}

void counter_engine_set_value(uint8_t id, uint64_t value) {
  if (id < 1 || id > 4) return;

  CounterConfig cfg;
  if (!counter_config_get(id, &cfg)) return;

  // Set value in appropriate mode
  switch (cfg.hw_mode) {
    case COUNTER_HW_SW:
      counter_sw_set_value(id, value);
      break;

    case COUNTER_HW_SW_ISR:
      counter_sw_isr_set_value(id, value);
      break;

    case COUNTER_HW_PCNT:
      counter_hw_set_value(id, value);
      break;

    default:
      break;
  }

  // Reset frequency tracking after manual set
  counter_frequency_reset(id);
}

/* ============================================================================
 * INTERNAL HELPERS (for mode-specific overflow access)
 * ============================================================================ */

uint8_t counter_sw_get_overflow(uint8_t id);
uint8_t counter_sw_isr_get_overflow(uint8_t id);
uint8_t counter_hw_get_overflow(uint8_t id);
