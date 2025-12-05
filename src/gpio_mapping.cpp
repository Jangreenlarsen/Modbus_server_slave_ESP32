/**
 * @file gpio_mapping.cpp
 * @brief UNIFIED VARIABLE MAPPING - sync GPIO pins + ST variables with Modbus registers/coils
 *
 * LAYER 4: Register/Coil Storage
 * Responsibility: Synchronize GPIO pins and ST Logic variables with Modbus data
 *
 * This handles ALL variable I/O:
 * - GPIO pins (INPUT/OUTPUT modes)
 * - ST Logic program variables (INPUT/OUTPUT modes)
 */

#include "gpio_mapping.h"
#include "config_struct.h"
#include "gpio_driver.h"
#include "registers.h"
#include "st_logic_config.h"

/**
 * @brief Update all variable mappings (GPIO + ST variables)
 *
 * This function is called once per main loop iteration.
 * It synchronizes ALL mapped sources with Modbus registers/coils:
 * - GPIO INPUT mode: Read GPIO pin → write to discrete input
 * - GPIO OUTPUT mode: Read coil → write to GPIO pin
 * - ST VAR INPUT mode: Read holding register → write to ST variable
 * - ST VAR OUTPUT mode: Read ST variable → write to holding register
 */
void gpio_mapping_update(void) {
  for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
    const VariableMapping* map = &g_persist_config.var_maps[i];

    // ========================================================================
    // GPIO MAPPING
    // ========================================================================
    if (map->source_type == MAPPING_SOURCE_GPIO) {
      // Skip mappings associated with counters/timers (handled by their engines)
      if (map->associated_counter != 0xff || map->associated_timer != 0xff) {
        continue;
      }

      if (map->is_input) {
        // INPUT mode: GPIO pin → discrete input
        if (map->input_reg != 65535) {
          uint8_t level;

          // Virtual GPIO support: pin >= 100 means read from COIL(pin - 100)
          if (map->gpio_pin >= 100) {
            uint16_t virtual_coil = map->gpio_pin - 100;
            level = registers_get_coil(virtual_coil) ? 1 : 0;
          } else {
            // Real GPIO: read from hardware pin
            level = gpio_read(map->gpio_pin);
          }

          registers_set_discrete_input(map->input_reg, level);
        }
      } else {
        // OUTPUT mode: Coil → GPIO pin
        if (map->coil_reg != 65535) {
          uint8_t value = registers_get_coil(map->coil_reg);
          gpio_write(map->gpio_pin, value);
        }
      }
    }
    // ========================================================================
    // ST LOGIC VARIABLE MAPPING
    // ========================================================================
    else if (map->source_type == MAPPING_SOURCE_ST_VAR) {
      st_logic_engine_state_t *st_state = st_logic_get_state();
      if (!st_state) continue;

      st_logic_program_config_t *prog = st_logic_get_program(st_state, map->st_program_id);
      if (!prog || !prog->compiled || map->st_var_index >= prog->bytecode.var_count) {
        continue;
      }

      if (map->is_input) {
        // INPUT mode: Read from Modbus, write to ST variable
        if (map->input_reg != 65535 && map->input_reg < HOLDING_REGS_SIZE) {
          uint16_t reg_value;

          // Check input_type: 0 = Holding Register (HR), 1 = Discrete Input (DI)
          if (map->input_type == 1) {
            // Discrete Input: read as BOOL (0 or 1)
            reg_value = registers_get_discrete_input(map->input_reg) ? 1 : 0;
          } else {
            // Holding Register: read as INT
            reg_value = registers_get_holding_register(map->input_reg);
          }

          // Store as INT (simple type conversion)
          prog->bytecode.variables[map->st_var_index].int_val = (int16_t)reg_value;
        }
      } else {
        // OUTPUT mode: Read from ST variable, write to Modbus
        int16_t var_value = prog->bytecode.variables[map->st_var_index].int_val;

        // Output can go to either coil or holding register
        if (map->coil_reg != 65535) {
          // Output to coil (used for BOOL variables mapped to coils)
          uint8_t coil_value = (var_value != 0) ? 1 : 0;
          registers_set_coil(map->coil_reg, coil_value);
        } else if (map->input_reg != 65535 && map->input_reg < HOLDING_REGS_SIZE) {
          // Output to holding register (used for INT variables)
          registers_set_holding_register(map->input_reg, (uint16_t)var_value);
        }
      }
    }
  }
}

