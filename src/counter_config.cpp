/**
 * @file counter_config.cpp
 * @brief Counter configuration and validation (LAYER 5)
 *
 * Minimal implementation - stores counter configuration
 */

#include "counter_config.h"
#include "registers.h"
#include "constants.h"
#include "types.h"
#include <string.h>

/* ============================================================================
 * GLOBAL STATE
 * ============================================================================ */

static CounterConfig counter_configs[COUNTER_COUNT];

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void counter_config_init(void) {
  for (uint8_t i = 0; i < COUNTER_COUNT; i++) {
    counter_configs[i] = counter_config_defaults(i + 1);
  }
}

/* ============================================================================
 * DEFAULTS
 * ============================================================================ */

CounterConfig counter_config_defaults(uint8_t id) {
  CounterConfig cfg = {0};

  cfg.enabled = 0;
  cfg.mode_enable = COUNTER_MODE_DISABLED;
  cfg.edge_type = COUNTER_EDGE_RISING;
  cfg.direction = COUNTER_DIR_UP;
  cfg.hw_mode = COUNTER_HW_SW;

  cfg.prescaler = 1;
  cfg.bit_width = 32;
  cfg.scale_factor = 1.0f;

  cfg.index_reg = 0;
  cfg.raw_reg = 0;
  cfg.freq_reg = 0;
  cfg.overload_reg = 0;
  cfg.ctrl_reg = 0;

  cfg.start_value = 0;
  cfg.debounce_enabled = 1;
  cfg.debounce_ms = 10;
  cfg.input_dis = 0;
  cfg.interrupt_pin = 0;

  return cfg;
}

/* ============================================================================
 * VALIDATION
 * ============================================================================ */

bool counter_config_validate(const CounterConfig* cfg) {
  if (cfg == NULL) return false;

  // Basic validation
  if (cfg->prescaler < 1) return false;
  if (cfg->index_reg >= HOLDING_REGS_SIZE && cfg->index_reg != 0) return false;

  return true;
}

void counter_config_sanitize(CounterConfig* cfg) {
  if (cfg == NULL) return;

  // Clamp values
  if (cfg->prescaler < 1) cfg->prescaler = 1;
  if (cfg->bit_width < 8) cfg->bit_width = 8;
  if (cfg->debounce_ms < 0) cfg->debounce_ms = 0;

  // Ensure binary levels
  cfg->debounce_enabled = (cfg->debounce_enabled ? 1 : 0);
}

/* ============================================================================
 * CONFIGURATION ACCESS
 * ============================================================================ */

bool counter_config_get(uint8_t id, CounterConfig* out) {
  if (id < 1 || id > COUNTER_COUNT || out == NULL) return false;

  *out = counter_configs[id - 1];
  return true;
}

bool counter_config_set(uint8_t id, const CounterConfig* cfg) {
  if (id < 1 || id > COUNTER_COUNT || cfg == NULL) return false;

  // Validate before setting
  if (!counter_config_validate(cfg)) {
    return false;
  }

  // Sanitize and store
  CounterConfig sanitized = *cfg;
  counter_config_sanitize(&sanitized);
  counter_configs[id - 1] = sanitized;

  return true;
}

CounterConfig* counter_config_get_all(void) {
  return counter_configs;
}
