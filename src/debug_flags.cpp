/**
 * @file debug_flags.cpp
 * @brief Runtime debug flag management
 *
 * Provides global debug flags that can be toggled at runtime
 */

#include "debug_flags.h"

// Global debug flags (default: all enabled for first diagnosis)
DebugFlags g_debug_flags = {
  .config_save = 1,
  .config_load = 1,
  .wifi_connect = 1,       // Enable WiFi debug by default
  .network_validate = 1,   // Enable network validation debug by default
};

DebugFlags* debug_flags_get(void) {
  return &g_debug_flags;
}

void debug_flags_set_config_save(uint8_t enabled) {
  g_debug_flags.config_save = enabled ? 1 : 0;
}

void debug_flags_set_config_load(uint8_t enabled) {
  g_debug_flags.config_load = enabled ? 1 : 0;
}

void debug_flags_set_wifi_connect(uint8_t enabled) {
  g_debug_flags.wifi_connect = enabled ? 1 : 0;
}

void debug_flags_set_network_validate(uint8_t enabled) {
  g_debug_flags.network_validate = enabled ? 1 : 0;
}

void debug_flags_set_all(uint8_t enabled) {
  g_debug_flags.config_save = enabled ? 1 : 0;
  g_debug_flags.config_load = enabled ? 1 : 0;
  g_debug_flags.wifi_connect = enabled ? 1 : 0;
  g_debug_flags.network_validate = enabled ? 1 : 0;
}
