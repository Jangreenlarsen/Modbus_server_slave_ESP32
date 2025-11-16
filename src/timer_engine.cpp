/**
 * @file timer_engine.cpp
 * @brief Timer orchestration and state machine (LAYER 5)
 *
 * Ported from: Mega2560 v3.6.5 modbus_timers.cpp
 * Adapted to: ESP32 modular architecture
 *
 * Minimal implementation matching types.h TimerConfig
 */

#include "timer_engine.h"
#include "timer_config.h"
#include "registers.h"
#include "debug.h"
#include "constants.h"
#include "types.h"
#include <string.h>

/* ============================================================================
 * TIMER RUNTIME STATE (per timer)
 * ============================================================================ */

typedef struct {
  uint32_t phase_start_ms;
  uint8_t current_phase;
  uint8_t is_active;
} TimerRuntimeState;

static TimerRuntimeState timer_state[TIMER_COUNT] = {0};

/* ============================================================================
 * HELPER - SET COIL LEVEL
 * ============================================================================ */

static void set_coil_level(uint16_t coil_idx, uint8_t high) {
  if (coil_idx >= (COILS_SIZE * 8)) return;
  registers_set_coil(coil_idx, high ? 1 : 0);
}

static uint8_t get_discrete_input_level(uint16_t input_idx) {
  if (input_idx >= (DISCRETE_INPUTS_SIZE * 8)) return 0;
  return registers_get_discrete_input(input_idx) ? 1 : 0;
}

/* ============================================================================
 * MODE 1: ONE-SHOT (3-phase sequence)
 * ============================================================================ */

static void mode_one_shot(uint8_t id, TimerConfig* cfg, uint32_t now_ms) {
  TimerRuntimeState* state = &timer_state[id - 1];

  if (!state->is_active) return;

  switch (state->current_phase) {
    case 0:
      // Phase 1: set P1 level, start timing
      set_coil_level(cfg->output_coil, cfg->phase1_output_state);
      if (cfg->phase1_duration_ms == 0 || (now_ms - state->phase_start_ms >= cfg->phase1_duration_ms)) {
        state->current_phase = 1;
        state->phase_start_ms = now_ms;
      }
      break;

    case 1:
      // Phase 2: set P2 level
      set_coil_level(cfg->output_coil, cfg->phase2_output_state);
      if (cfg->phase2_duration_ms == 0 || (now_ms - state->phase_start_ms >= cfg->phase2_duration_ms)) {
        state->current_phase = 2;
        state->phase_start_ms = now_ms;
      }
      break;

    case 2:
      // Phase 3: set P3 level
      set_coil_level(cfg->output_coil, cfg->phase3_output_state);
      if (cfg->phase3_duration_ms == 0 || (now_ms - state->phase_start_ms >= cfg->phase3_duration_ms)) {
        // Sequence complete
        state->is_active = 0;
        state->current_phase = 0;
      }
      break;

    default:
      state->is_active = 0;
      state->current_phase = 0;
      break;
  }
}

/* ============================================================================
 * MODE 2: MONOSTABLE (retriggerable pulse)
 * ============================================================================ */

static void mode_monostable(uint8_t id, TimerConfig* cfg, uint32_t now_ms) {
  TimerRuntimeState* state = &timer_state[id - 1];

  if (!state->is_active) {
    // Not active: output at P1 level
    set_coil_level(cfg->output_coil, cfg->phase1_output_state);
    return;
  }

  // Active: start pulse
  if (state->current_phase == 0) {
    state->current_phase = 1;
    state->phase_start_ms = now_ms;
    set_coil_level(cfg->output_coil, cfg->phase2_output_state);
  }
  // Pulse ongoing: check if time expired
  else if (state->current_phase == 1 && (now_ms - state->phase_start_ms >= cfg->pulse_duration_ms)) {
    // Pulse complete: return to P1 level
    set_coil_level(cfg->output_coil, cfg->phase1_output_state);
    state->is_active = 0;
    state->current_phase = 0;
  }
}

/* ============================================================================
 * MODE 3: ASTABLE (oscillate between P1/P2)
 * ============================================================================ */

static void mode_astable(uint8_t id, TimerConfig* cfg, uint32_t now_ms) {
  TimerRuntimeState* state = &timer_state[id - 1];

  if (!state->is_active) return;  // Not running, stay silent

  if (state->current_phase == 0) {
    // Phase 0: output P1, timing T1
    set_coil_level(cfg->output_coil, cfg->phase1_output_state);
    if (cfg->on_duration_ms == 0 || (now_ms - state->phase_start_ms >= cfg->on_duration_ms)) {
      state->current_phase = 1;
      state->phase_start_ms = now_ms;
    }
  } else {
    // Phase 1: output P2, timing T2
    set_coil_level(cfg->output_coil, cfg->phase2_output_state);
    if (cfg->off_duration_ms == 0 || (now_ms - state->phase_start_ms >= cfg->off_duration_ms)) {
      state->current_phase = 0;
      state->phase_start_ms = now_ms;
    }
  }
}

/* ============================================================================
 * MODE 4: TRIGGER-DRIVEN (edge-triggered, runs subMode)
 * ============================================================================ */

static void mode_trigger(uint8_t id, TimerConfig* cfg, uint32_t now_ms) {
  // Stub: trigger not fully implemented yet
  // Would check discrete input for edge and fire appropriate submode
}

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void timer_engine_init(void) {
  timer_config_init();
  memset(timer_state, 0, sizeof(timer_state));
}

/* ============================================================================
 * MAIN LOOP
 * ============================================================================ */

void timer_engine_loop(void) {
  uint32_t now_ms = registers_get_millis();

  for (uint8_t i = 0; i < TIMER_COUNT; i++) {
    TimerConfig cfg;
    if (!timer_config_get(i + 1, &cfg) || !cfg.enabled) {
      continue;
    }

    // Run mode-specific state machine
    switch (cfg.mode) {
      case TIMER_MODE_1_ONESHOT:
        if (timer_state[i].is_active) {
          mode_one_shot(i + 1, &cfg, now_ms);
        }
        break;

      case TIMER_MODE_2_MONOSTABLE:
        mode_monostable(i + 1, &cfg, now_ms);
        break;

      case TIMER_MODE_3_ASTABLE:
        mode_astable(i + 1, &cfg, now_ms);
        break;

      case TIMER_MODE_4_INPUT_TRIGGERED:
        mode_trigger(i + 1, &cfg, now_ms);
        break;

      default:
        break;
    }
  }
}

/* ============================================================================
 * CONFIGURATION & CONTROL
 * ============================================================================ */

bool timer_engine_configure(uint8_t id, const TimerConfig* cfg) {
  if (cfg == NULL) return false;

  // Validate and set
  if (!timer_config_set(id, cfg)) {
    return false;
  }

  // Initialize runtime state
  if (id >= 1 && id <= TIMER_COUNT) {
    timer_state[id - 1].is_active = 0;
    timer_state[id - 1].current_phase = 0;
    timer_state[id - 1].phase_start_ms = 0;
  }

  return true;
}

bool timer_engine_get_config(uint8_t id, TimerConfig* out) {
  return timer_config_get(id, out);
}

/* ============================================================================
 * COIL WRITE CALLBACK
 * ============================================================================ */

void timer_engine_on_coil_write(uint16_t coil_idx, uint8_t value) {
  (void)value;  // Value not used, just trigger on write
  uint32_t now_ms = registers_get_millis();

  for (uint8_t i = 0; i < TIMER_COUNT; i++) {
    TimerConfig cfg;
    if (!timer_config_get(i + 1, &cfg) || !cfg.enabled) {
      continue;
    }

    if (cfg.output_coil != coil_idx) continue;

    // Skip if astable already running (no retrigger for astable)
    if (cfg.mode == TIMER_MODE_3_ASTABLE && timer_state[i].is_active) continue;

    // Trigger this timer
    timer_state[i].is_active = 1;
    timer_state[i].current_phase = 0;
    timer_state[i].phase_start_ms = now_ms;
  }
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

bool timer_engine_has_coil(uint16_t coil_idx) {
  for (uint8_t i = 0; i < TIMER_COUNT; i++) {
    TimerConfig cfg;
    if (!timer_config_get(i + 1, &cfg) || !cfg.enabled) {
      continue;
    }

    if (cfg.output_coil == coil_idx) return true;
  }

  return false;
}

void timer_engine_disable_all(void) {
  for (uint8_t i = 0; i < TIMER_COUNT; i++) {
    TimerConfig cfg;
    if (!timer_config_get(i + 1, &cfg)) continue;

    cfg.enabled = 0;
    timer_config_set(i + 1, &cfg);
    timer_state[i].is_active = 0;
  }
}

void timer_engine_clear_alarms(void) {
  // Stub: alarm handling not yet implemented
}
