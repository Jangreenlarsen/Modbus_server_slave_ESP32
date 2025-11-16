/**
 * @file counter_hw.cpp
 * @brief Hardware PCNT mode counter (LAYER 5)
 *
 * Ported from: Mega2560 v3.6.5 modbus_counters_hw.cpp (Timer5 only)
 * Adapted to: ESP32 PCNT (Pulse Counter) hardware
 *
 * Responsibility:
 * - PCNT unit initialization and configuration
 * - Reading pulse counts directly from PCNT hardware
 * - Overflow handling
 * - Value accumulation and tracking
 */

#include "counter_hw.h"
#include "counter_config.h"
#include "pcnt_driver.h"
#include "registers.h"
#include "constants.h"
#include "types.h"
#include <string.h>

/* ============================================================================
 * HW MODE RUNTIME STATE (per counter)
 * Uses CounterHWState from types.h
 * ============================================================================ */

static CounterHWState hw_state[COUNTER_COUNT] = {0};

/* ============================================================================
 * PCNT UNIT MAPPING
 * For now: Counter 1 → PCNT0, Counter 2 → PCNT1, etc.
 * ============================================================================ */

static const uint8_t counter_to_pcnt[COUNTER_COUNT] = {0, 1, 2, 3};

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void counter_hw_init(uint8_t id) {
  if (id < 1 || id > COUNTER_COUNT) return;

  CounterHWState* state = &hw_state[id - 1];
  state->pcnt_value = 0;
  state->last_count = 0;
  state->overflow_count = 0;
  state->is_counting = 0;

  // Initialize PCNT unit (zero counter)
  uint8_t pcnt_unit = counter_to_pcnt[id - 1];
  pcnt_unit_init(pcnt_unit);

  // Get config to set start value
  CounterConfig cfg;
  if (counter_config_get(id, &cfg)) {
    state->pcnt_value = cfg.start_value;
  }
}

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

bool counter_hw_configure(uint8_t id, uint8_t gpio_pin) {
  if (id < 1 || id > COUNTER_COUNT) return false;

  CounterHWState* state = &hw_state[id - 1];
  uint8_t pcnt_unit = counter_to_pcnt[id - 1];

  // Configure PCNT with GPIO pin
  CounterConfig cfg;
  if (!counter_config_get(id, &cfg)) return false;

  // Determine edge mode for PCNT
  pcnt_edge_mode_t pos_edge = PCNT_EDGE_RISING;  // Default
  pcnt_edge_mode_t neg_edge = PCNT_EDGE_FALLING;

  if (cfg.edge_type == COUNTER_EDGE_RISING) {
    pos_edge = PCNT_EDGE_RISING;
    neg_edge = PCNT_EDGE_DISABLE;
  } else if (cfg.edge_type == COUNTER_EDGE_FALLING) {
    pos_edge = PCNT_EDGE_DISABLE;
    neg_edge = PCNT_EDGE_FALLING;
  } else {
    // COUNTER_EDGE_BOTH: count all edges
    pos_edge = PCNT_EDGE_RISING;
    neg_edge = PCNT_EDGE_FALLING;
  }

  // Configure PCNT unit
  pcnt_unit_configure(pcnt_unit, gpio_pin, pos_edge, neg_edge);

  // Set start value in pcnt_value
  state->pcnt_value = cfg.start_value;
  state->last_count = 0;
  state->is_counting = 1;

  return true;
}

/* ============================================================================
 * MAIN LOOP - READ PCNT AND TRACK CHANGES
 * ============================================================================ */

void counter_hw_loop(uint8_t id) {
  if (id < 1 || id > COUNTER_COUNT) return;

  CounterConfig cfg;
  if (!counter_config_get(id, &cfg)) return;

  if (!cfg.enabled || cfg.hw_mode != COUNTER_HW_PCNT) {
    return;
  }

  CounterHWState* state = &hw_state[id - 1];
  uint8_t pcnt_unit = counter_to_pcnt[id - 1];

  // Read current PCNT count
  uint32_t hw_count = pcnt_unit_get_count(pcnt_unit);

  // Simple accumulation: just add the difference to our value
  // (In a real implementation, would handle PCNT register overflow)
  if (hw_count != state->last_count) {
    state->pcnt_value += (hw_count - state->last_count);
    state->last_count = hw_count;
  }

  // Check for overflow based on bit width
  uint64_t max_val = 0xFFFFFFFFFFFFFFFFULL;
  switch (cfg.bit_width) {
    case 8:
      max_val = 0xFFULL;
      break;
    case 16:
      max_val = 0xFFFFULL;
      break;
    case 32:
      max_val = 0xFFFFFFFFULL;
      break;
  }

  if (state->pcnt_value > max_val) {
    state->pcnt_value = cfg.start_value & max_val;
    state->overflow_count++;
  }
}

/* ============================================================================
 * RESET
 * ============================================================================ */

void counter_hw_reset(uint8_t id) {
  if (id < 1 || id > COUNTER_COUNT) return;

  CounterConfig cfg;
  if (!counter_config_get(id, &cfg)) return;

  CounterHWState* state = &hw_state[id - 1];
  uint8_t pcnt_unit = counter_to_pcnt[id - 1];

  // Reset PCNT unit
  pcnt_unit_clear(pcnt_unit);

  // Reset software state
  state->pcnt_value = cfg.start_value;
  state->overflow_count = 0;
  state->last_count = 0;
}

/* ============================================================================
 * VALUE ACCESS
 * ============================================================================ */

uint64_t counter_hw_get_value(uint8_t id) {
  if (id < 1 || id > COUNTER_COUNT) return 0;

  CounterHWState* state = &hw_state[id - 1];
  return state->pcnt_value;
}

void counter_hw_set_value(uint8_t id, uint64_t value) {
  if (id < 1 || id > COUNTER_COUNT) return;

  CounterHWState* state = &hw_state[id - 1];
  state->pcnt_value = value;

  // Also clear PCNT so next loop will read from 0
  uint8_t pcnt_unit = counter_to_pcnt[id - 1];
  pcnt_unit_clear(pcnt_unit);
  state->last_count = 0;
}

uint8_t counter_hw_get_overflow(uint8_t id) {
  if (id < 1 || id > COUNTER_COUNT) return 0;
  // Return 1 if overflow has occurred
  return (hw_state[id - 1].overflow_count > 0) ? 1 : 0;
}

void counter_hw_clear_overflow(uint8_t id) {
  if (id < 1 || id > COUNTER_COUNT) return;
  hw_state[id - 1].overflow_count = 0;
}
