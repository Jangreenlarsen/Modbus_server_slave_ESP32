/**
 * @file config_load.cpp
 * @brief Configuration load from NVS with CRC validation
 *
 * LAYER 6: Persistence
 * Responsibility: Load configuration from NVS and validate integrity
 */

#include "config_load.h"
#include "config_save.h"
#include "constants.h"
#include "debug.h"
#include "debug_flags.h"
#include "network_config.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <cstddef>
#include <cstring>

// NVS key for storing config
#define NVS_CONFIG_KEY "modbus_cfg"
#define NVS_NAMESPACE  "modbus"

/**
 * @brief Initialize configuration with factory defaults
 */
static void config_init_defaults(PersistConfig* cfg) {
  memset(cfg, 0, sizeof(PersistConfig));
  cfg->schema_version = CONFIG_SCHEMA_VERSION;  // Current schema version
  cfg->slave_id = 1;
  cfg->baudrate = 9600;

  // Initialize network config with defaults (v3.0+)
  network_config_init_defaults(&cfg->network);

  // Initialize all GPIO mappings as unused (all 64 slots)
  for (uint8_t i = 0; i < 64; i++) {
    cfg->var_maps[i].input_reg = 65535;
    cfg->var_maps[i].coil_reg = 65535;
    cfg->var_maps[i].associated_counter = 0xff;
    cfg->var_maps[i].associated_timer = 0xff;
    cfg->var_maps[i].source_type = 0xff;  // Mark as unused
  }
  cfg->var_map_count = 0;  // No mappings in default config
}

bool config_load_from_nvs(PersistConfig* out) {
  if (out == NULL) {
    debug_println("ERROR: config_load_from_nvs - NULL output");
    return false;
  }

  DebugFlags* dbg = debug_flags_get();
  if (dbg->config_load) {
    debug_println("[LOAD_START] Loading config from NVS...");
  }

  // Open NVS
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    debug_println("CONFIG LOAD: NVS namespace not found, using defaults");
    config_init_defaults(out);
    return true;
  } else if (err != ESP_OK) {
    debug_print("ERROR: NVS open failed: ");
    debug_print_uint(err);
    debug_println(", using defaults");
    config_init_defaults(out);
    return true;
  }

  // Read config blob from NVS
  size_t required_size = sizeof(PersistConfig);
  if (dbg->config_load) {
    debug_print("[LOAD_DEBUG] Reading blob, size=");
    debug_print_uint(required_size);
    debug_println("");
  }
  err = nvs_get_blob(handle, NVS_CONFIG_KEY, out, &required_size);
  if (dbg->config_load) {
    debug_print("[LOAD_DEBUG] nvs_get_blob returned err=");
    debug_print_uint(err);
    debug_print(" required_size=");
    debug_print_uint(required_size);
    debug_println("");
  }
  nvs_close(handle);

  if (err == ESP_ERR_NVS_NOT_FOUND) {
    debug_println("CONFIG LOAD: Config key not found, using defaults");
    config_init_defaults(out);
    return true;
  } else if (err != ESP_OK) {
    debug_print("ERROR: NVS get_blob failed: ");
    debug_print_uint(err);
    debug_println(", using defaults");
    config_init_defaults(out);
    return true;
  }

  if (dbg->config_load) {
    debug_print("[LOAD_DEBUG] After nvs_get_blob: var_map_count=");
    debug_print_uint(out->var_map_count);
    debug_print(" schema_version=");
    debug_print_uint(out->schema_version);
    debug_print(" crc16=");
    debug_print_uint(out->crc16);
    debug_println("");

    // DEBUG: Dump first few bytes of loaded config
    debug_println("[LOAD_DEBUG] First 20 bytes of loaded data:");
    uint8_t* data = (uint8_t*)out;
    for (int i = 0; i < 20; i++) {
      debug_print("  [");
      debug_print_uint(i);
      debug_print("]=0x");
      if (data[i] < 16) debug_print("0");
      debug_print_uint(data[i]);
      debug_print(" ");
    }
    debug_println("");
  }

  // Validate schema version (MUST be checked before CRC to prevent struct misalignment)
  if (out->schema_version != CONFIG_SCHEMA_VERSION) {
    debug_print("ERROR: Schema version mismatch (stored=");
    debug_print_uint(out->schema_version);
    debug_print(", current=");
    debug_print_uint(CONFIG_SCHEMA_VERSION);
    debug_println("), reinitializing with defaults");
    config_init_defaults(out);
    return true;
  }

  if (dbg->config_load) {
    debug_println("[LOAD_DEBUG] Schema version OK, checking CRC...");
  }

  // Validate CRC
  uint16_t stored_crc = out->crc16;
  uint16_t calculated_crc = config_calculate_crc16(out);

  if (stored_crc != calculated_crc) {
    debug_print("ERROR: CRC mismatch (stored=");
    debug_print_uint(stored_crc);
    debug_print(", calculated=");
    debug_print_uint(calculated_crc);
    debug_print(") var_map_count=");
    debug_print_uint(out->var_map_count);
    debug_println("");

    // CRITICAL FIX: If var_map_count > 0, it means struct is misaligned (old data format)
    // Reinitialize var_maps and clear the count to prevent corruption
    if (out->var_map_count > 0) {
      debug_println("  WARNING: Detected misaligned var_maps (old structure format)");
      debug_println("  Clearing variable mappings, user must reconfigure GPIO bindings");
      for (uint8_t i = 0; i < 64; i++) {
        out->var_maps[i].input_reg = 65535;
        out->var_maps[i].coil_reg = 65535;
        out->var_maps[i].associated_counter = 0xff;
        out->var_maps[i].associated_timer = 0xff;
        out->var_maps[i].source_type = 0xff;
      }
      out->var_map_count = 0;
      // Recalculate CRC with cleared var_maps
      calculated_crc = config_calculate_crc16(out);
      out->crc16 = calculated_crc;
      debug_println("  Variable mappings cleared and CRC recalculated");
    } else {
      debug_println("  Using default configuration");
      config_init_defaults(out);
    }
    return true;
  }

  // Print summary
  debug_print("CONFIG LOADED: schema=");
  debug_print_uint(out->schema_version);
  debug_print(", slave_id=");
  debug_print_uint(out->slave_id);
  debug_print(", baudrate=");
  debug_print_uint(out->baudrate);
  debug_print(", var_maps=");
  debug_print_uint(out->var_map_count);
  debug_print(", static_regs=");
  debug_print_uint(out->static_reg_count);
  debug_print(", static_coils=");
  debug_print_uint(out->static_coil_count);
  debug_print(", CRC=");
  debug_print_uint(calculated_crc);
  debug_println(" OK");

  // Debug: Print loaded GPIO mappings
  if (out->var_map_count > 0) {
    debug_println("  Loaded variable mappings:");
    for (uint8_t i = 0; i < out->var_map_count; i++) {
      const VariableMapping* map = &out->var_maps[i];
      debug_print("    [");
      debug_print_uint(i);
      debug_print("] source_type=");
      debug_print_uint(map->source_type);
      debug_print(" gpio_pin=");
      debug_print_uint(map->gpio_pin);
      debug_print(" is_input=");
      debug_print_uint(map->is_input);
      debug_print(" input_reg=");
      debug_print_uint(map->input_reg);
      debug_print(" coil_reg=");
      debug_print_uint(map->coil_reg);
      debug_println("");
    }
  }

  return true;
}
