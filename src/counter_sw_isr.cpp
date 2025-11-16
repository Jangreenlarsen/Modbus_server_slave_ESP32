/**
 * @file counter_sw_isr.cpp
 * @brief Software ISR (interrupt) mode counter (LAYER 5)
 *
 * Ported from: Mega2560 v3.6.5 modbus_counters.cpp
 * Adapted to: ESP32 GPIO interrupt handling
 *
 * Responsibility:
 * - GPIO interrupt attachment and handling
 * - ISR-based edge counting (INT0-INT5)
 * - Debounce in main loop
 */

#include "counter_sw_isr.h"
#include "counter_config.h"
#include "gpio_driver.h"
#include "registers.h"
#include "constants.h"
#include "types.h"
#include <string.h>

/* ============================================================================
 * ISR MODE RUNTIME STATE (per counter)
 * Minimal state for ISR-based counting
 * ============================================================================ */

static CounterSWState isr_state[COUNTER_COUNT] = {0};

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void counter_sw_isr_init(uint8_t id) {
  if (id < 1 || id > COUNTER_COUNT) return;

  CounterSWState* state = &isr_state[id - 1];
  state->counter_value = 0;
  state->last_level = 0;
  state->debounce_timer = 0;
  state->is_counting = 0;

  // Get config to set start value
  CounterConfig cfg;
  if (counter_config_get(id, &cfg)) {
    state->counter_value = cfg.start_value;
  }
}

/* ============================================================================
 * MAIN LOOP - DEBOUNCE HANDLING ONLY
 * ISR increments counter in interrupt context
 * ============================================================================ */

void counter_sw_isr_loop(uint8_t id) {
  if (id < 1 || id > COUNTER_COUNT) return;

  CounterConfig cfg;
  if (!counter_config_get(id, &cfg)) return;

  if (!cfg.enabled || cfg.hw_mode != COUNTER_HW_SW_ISR) {
    return;
  }

  CounterSWState* state = &isr_state[id - 1];

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

  if (state->counter_value > max_val) {
    state->counter_value = cfg.start_value;  // Wrap to start value
  }
}

/* ============================================================================
 * INTERRUPT ATTACHMENT
 * ============================================================================ */

void counter_sw_isr_attach(uint8_t id, uint8_t gpio_pin) {
  if (id < 1 || id > COUNTER_COUNT) return;

  CounterConfig cfg;
  if (!counter_config_get(id, &cfg)) return;

  // Map edge type to GPIO edge
  gpio_edge_t edge = GPIO_RISING;  // Default
  if (cfg.edge_type == COUNTER_EDGE_FALLING) {
    edge = GPIO_FALLING;
  } else if (cfg.edge_type == COUNTER_EDGE_BOTH) {
    edge = GPIO_BOTH;
  }

  // Attach interrupt (stub implementation will do nothing for now)
  // gpio_interrupt_attach(gpio_pin, edge, isr_handler);

  // For now, just mark as counting
  isr_state[id - 1].is_counting = 1;
}

void counter_sw_isr_detach(uint8_t id) {
  if (id < 1 || id > COUNTER_COUNT) return;
  isr_state[id - 1].is_counting = 0;
}

/* ============================================================================
 * RESET
 * ============================================================================ */

void counter_sw_isr_reset(uint8_t id) {
  if (id < 1 || id > COUNTER_COUNT) return;

  CounterConfig cfg;
  if (!counter_config_get(id, &cfg)) return;

  CounterSWState* state = &isr_state[id - 1];
  state->counter_value = cfg.start_value;
  state->debounce_timer = 0;
}

/* ============================================================================
 * VALUE ACCESS
 * ============================================================================ */

uint64_t counter_sw_isr_get_value(uint8_t id) {
  if (id < 1 || id > COUNTER_COUNT) return 0;
  return isr_state[id - 1].counter_value;
}

void counter_sw_isr_set_value(uint8_t id, uint64_t value) {
  if (id < 1 || id > COUNTER_COUNT) return;
  isr_state[id - 1].counter_value = value;
}

uint8_t counter_sw_isr_get_overflow(uint8_t id) {
  if (id < 1 || id > COUNTER_COUNT) return 0;
  // ISR mode doesn't track overflow flag
  return 0;
}

void counter_sw_isr_clear_overflow(uint8_t id) {
  // ISR mode doesn't track overflow flag
  (void)id;
}
