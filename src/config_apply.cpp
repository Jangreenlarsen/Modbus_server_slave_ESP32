/**
 * @file config_apply.cpp
 * @brief Configuration apply - activate config in system (stub)
 */

#include "config_apply.h"
#include "counter_engine.h"
#include "timer_engine.h"
#include "debug.h"
#include <cstddef>

bool config_apply(const PersistConfig* cfg) {
  if (cfg == NULL) return false;
  
  debug_println("CONFIG APPLY: Applying configuration to system");
  
  // TODO: Apply counter configs
  for (uint8_t i = 0; i < COUNTER_COUNT; i++) {
    // counter_engine_configure(i + 1, &cfg->counters[i]);
  }
  
  // TODO: Apply timer configs
  for (uint8_t i = 0; i < TIMER_COUNT; i++) {
    // timer_engine_configure(i + 1, &cfg->timers[i]);
  }
  
  return true;
}
