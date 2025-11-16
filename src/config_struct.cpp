/**
 * @file config_struct.cpp
 * @brief Configuration struct utilities implementation
 */

#include "config_struct.h"
#include <string.h>

// Note: This would normally allocate, but we'll return static for now
static PersistConfig default_config;

PersistConfig* config_struct_create_default(void) {
  memset(&default_config, 0, sizeof(PersistConfig));
  default_config.schema_version = 1;
  default_config.slave_id = 1;
  default_config.baudrate = 9600;
  return &default_config;
}
