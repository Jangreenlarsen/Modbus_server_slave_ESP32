/**
 * @file config_struct.cpp
 * @brief Configuration struct utilities implementation
 *
 * Contains the global PersistConfig structure used throughout the system
 */

#include "config_struct.h"
#include <string.h>

// Global persistent configuration (accessible to all modules)
PersistConfig g_persist_config = {0};

PersistConfig* config_struct_create_default(void) {
  memset(&g_persist_config, 0, sizeof(PersistConfig));
  g_persist_config.schema_version = 2;  // Updated schema version
  g_persist_config.slave_id = 1;
  g_persist_config.baudrate = 115200;
  return &g_persist_config;
}
