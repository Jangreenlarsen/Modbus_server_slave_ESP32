/**
 * @file config_struct.cpp
 * @brief Configuration struct utilities implementation
 *
 * Contains the global PersistConfig structure used throughout the system
 */

#include "config_struct.h"
#include "constants.h"
#include <string.h>

// Global persistent configuration (accessible to all modules)
PersistConfig g_persist_config = {0};

PersistConfig* config_struct_create_default(void) {
  memset(&g_persist_config, 0, sizeof(PersistConfig));
  g_persist_config.schema_version = CONFIG_SCHEMA_VERSION;  // Current schema version
  g_persist_config.slave_id = 1;
  g_persist_config.baudrate = 115200;

  // Initialize all var_maps as unused (important for CRC stability)
  for (uint8_t i = 0; i < 64; i++) {
    g_persist_config.var_maps[i].input_reg = 65535;
    g_persist_config.var_maps[i].coil_reg = 65535;
    g_persist_config.var_maps[i].associated_counter = 0xff;
    g_persist_config.var_maps[i].associated_timer = 0xff;
    g_persist_config.var_maps[i].source_type = 0xff;
  }
  g_persist_config.var_map_count = 0;

  return &g_persist_config;
}
