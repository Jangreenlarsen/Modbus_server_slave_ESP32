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

  // IMPROVEMENT: Smart register defaults (v4.2.0)
  // Assign logical spacing: Counter 1 → 100-104, Counter 2 → 110-114, etc.
  // This ensures counters have sane defaults without explicit CLI configuration
  uint16_t base = 100 + ((id - 1) * 10);
  cfg.index_reg = base + 0;     // 100, 110, 120, 130
  cfg.raw_reg = base + 1;       // 101, 111, 121, 131
  cfg.freq_reg = base + 2;      // 102, 112, 122, 132
  cfg.overload_reg = base + 3;  // 103, 113, 123, 133
  cfg.ctrl_reg = base + 4;      // 104, 114, 124, 134

  cfg.start_value = 0;
  cfg.debounce_enabled = 1;
  cfg.debounce_ms = 10;
  cfg.input_dis = 0;
  cfg.interrupt_pin = 0;
  cfg.hw_gpio = 0;  // BUG FIX 1.9: Default hw_gpio (0 = not configured)

  // COMPARE FEATURE DEFAULTS (v2.3+)
  cfg.compare_enabled = 0;       // Disabled by default
  cfg.compare_mode = 0;          // 0 = ≥ (greater-or-equal)
  cfg.compare_value = 0;         // No compare value set
  cfg.reset_on_read = 1;         // Auto-clear bit 4 on ctrl-reg read by default

  // Note: Compare status stored in ctrl_reg bit 4 (no register/bit config needed)

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

  // BUG FIX 2.3: Bit width validation with upper bound and valid values only
  // Valid bit widths: 8, 16, 32, 64
  if (cfg->bit_width < 8) cfg->bit_width = 8;
  if (cfg->bit_width > 64) cfg->bit_width = 64;

  // Round to nearest valid bit width (8, 16, 32, 64)
  if (cfg->bit_width <= 8) {
    cfg->bit_width = 8;
  } else if (cfg->bit_width <= 16) {
    cfg->bit_width = 16;
  } else if (cfg->bit_width <= 32) {
    cfg->bit_width = 32;
  } else {
    cfg->bit_width = 64;
  }

  if (cfg->debounce_ms < 0) cfg->debounce_ms = 0;

  // Ensure binary levels
  cfg->debounce_enabled = (cfg->debounce_enabled ? 1 : 0);

  // COMPARE FEATURE VALIDATION (v2.3+)
  cfg->compare_enabled = (cfg->compare_enabled ? 1 : 0);
  cfg->reset_on_read = (cfg->reset_on_read ? 1 : 0);

  // Clamp compare mode to valid values (0-2)
  if (cfg->compare_mode > 2) cfg->compare_mode = 0;

  // Note: Compare status stored in ctrl_reg bit 4, no additional validation needed
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
