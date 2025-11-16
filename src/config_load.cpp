/**
 * @file config_load.cpp
 * @brief Configuration load from NVS (stub)
 */

#include "config_load.h"
#include "config_save.h"
#include "debug.h"
#include <cstddef>
#include <cstring>

bool config_load_from_nvs(PersistConfig* out) {
  if (out == NULL) return false;
  
  // TODO: Implement NVS loading with ESP32 IDF
  // For now, initialize with defaults
  memset(out, 0, sizeof(PersistConfig));
  out->schema_version = 1;
  out->slave_id = 1;
  out->baudrate = 9600;
  
  debug_println("CONFIG LOAD: Using default config (NVS not yet implemented)");
  return true;
}
