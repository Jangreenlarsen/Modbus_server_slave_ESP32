/**
 * @file registers.cpp
 * @brief Register and coil storage implementation
 *
 * Provides arrays for holding/input registers and coils/discrete inputs
 * All Modbus read/write operations go through these functions
 *
 * Also handles DYNAMIC register/coil updates from counter/timer sources
 */

#include "registers.h"
#include "counter_engine.h"
#include "counter_config.h"
#include "timer_engine.h"
#include "config_struct.h"
#include "st_logic_config.h"
#include "debug.h"
#include "types.h"
#include "constants.h"
#include <Arduino.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * STATIC STORAGE (all registers and coils in RAM)
 * ============================================================================ */

static uint16_t holding_regs[HOLDING_REGS_SIZE] = {0};      // 16-bit registers
static uint16_t input_regs[INPUT_REGS_SIZE] = {0};          // 16-bit registers
static uint8_t coils[COILS_SIZE] = {0};                     // Packed bits (8 per byte)
static uint8_t discrete_inputs[DISCRETE_INPUTS_SIZE] = {0}; // Packed bits (8 per byte)

/* ============================================================================
 * HOLDING REGISTERS (Read/Write)
 * ============================================================================ */

uint16_t registers_get_holding_register(uint16_t addr) {
  if (addr >= HOLDING_REGS_SIZE) return 0;
  return holding_regs[addr];
}

void registers_set_holding_register(uint16_t addr, uint16_t value) {
  if (addr >= HOLDING_REGS_SIZE) return;
  holding_regs[addr] = value;

  // Process ST Logic control registers
  if (addr >= ST_LOGIC_CONTROL_REG_BASE && addr < ST_LOGIC_CONTROL_REG_BASE + 4) {
    registers_process_st_logic_control(addr, value);
  }
}

uint16_t* registers_get_holding_regs(void) {
  return holding_regs;
}

/* ============================================================================
 * INPUT REGISTERS (Read-Only from Modbus, Write from drivers)
 * ============================================================================ */

uint16_t registers_get_input_register(uint16_t addr) {
  if (addr >= INPUT_REGS_SIZE) return 0;
  return input_regs[addr];
}

void registers_set_input_register(uint16_t addr, uint16_t value) {
  if (addr >= INPUT_REGS_SIZE) return;
  input_regs[addr] = value;
}

uint16_t* registers_get_input_regs(void) {
  return input_regs;
}

/* ============================================================================
 * COILS (Read/Write) - Packed bits
 * ============================================================================ */

uint8_t registers_get_coil(uint16_t idx) {
  if (idx >= (COILS_SIZE * 8)) return 0;
  uint16_t byte_idx = idx / 8;
  uint16_t bit_idx = idx % 8;
  return (coils[byte_idx] >> bit_idx) & 1;
}

void registers_set_coil(uint16_t idx, uint8_t value) {
  if (idx >= (COILS_SIZE * 8)) return;
  uint16_t byte_idx = idx / 8;
  uint16_t bit_idx = idx % 8;

  if (value) {
    coils[byte_idx] |= (1 << bit_idx);  // Set bit
  } else {
    coils[byte_idx] &= ~(1 << bit_idx); // Clear bit
  }
}

uint8_t* registers_get_coils(void) {
  return coils;
}

/* ============================================================================
 * DISCRETE INPUTS (Read-Only from Modbus, Write from GPIO/sensors)
 * ============================================================================ */

uint8_t registers_get_discrete_input(uint16_t idx) {
  if (idx >= (DISCRETE_INPUTS_SIZE * 8)) return 0;
  uint16_t byte_idx = idx / 8;
  uint16_t bit_idx = idx % 8;
  return (discrete_inputs[byte_idx] >> bit_idx) & 1;
}

void registers_set_discrete_input(uint16_t idx, uint8_t value) {
  if (idx >= (DISCRETE_INPUTS_SIZE * 8)) return;
  uint16_t byte_idx = idx / 8;
  uint16_t bit_idx = idx % 8;

  if (value) {
    discrete_inputs[byte_idx] |= (1 << bit_idx);  // Set bit
  } else {
    discrete_inputs[byte_idx] &= ~(1 << bit_idx); // Clear bit
  }
}

uint8_t* registers_get_discrete_inputs(void) {
  return discrete_inputs;
}

/* ============================================================================
 * UTILITY / INITIALIZATION
 * ============================================================================ */

void registers_init(void) {
  memset(holding_regs, 0, sizeof(holding_regs));
  memset(input_regs, 0, sizeof(input_regs));
  memset(coils, 0, sizeof(coils));
  memset(discrete_inputs, 0, sizeof(discrete_inputs));
}

uint32_t registers_get_millis(void) {
  return millis();
}

/* ============================================================================
 * DYNAMIC REGISTER/COIL UPDATES
 * ============================================================================ */

/**
 * @brief Update DYNAMIC registers from counter/timer sources
 *
 * Iterates through all DYNAMIC register mappings and:
 * 1. Gets value from counter or timer
 * 2. Writes to specified holding register
 *
 * Called once per main loop iteration
 */
void registers_update_dynamic_registers(void) {
  for (uint8_t i = 0; i < g_persist_config.dynamic_reg_count; i++) {
    const DynamicRegisterMapping* dyn = &g_persist_config.dynamic_regs[i];
    uint16_t reg_addr = dyn->register_address;
    uint16_t value = 0;

    if (dyn->source_type == DYNAMIC_SOURCE_COUNTER) {
      uint8_t counter_id = dyn->source_id;
      CounterConfig cfg;
      memset(&cfg, 0, sizeof(cfg));

      if (!counter_engine_get_config(counter_id, &cfg) || !cfg.enabled) {
        continue;  // Counter not configured or disabled
      }

      // Get counter value based on function type
      uint64_t raw_value = counter_engine_get_value(counter_id);

      switch (dyn->source_function) {
        case COUNTER_FUNC_INDEX:
          // Scaled value = counterValue × scale_factor
          value = (uint16_t)(raw_value * cfg.scale_factor);
          break;

        case COUNTER_FUNC_RAW:
          // Prescaled value = counterValue / prescaler
          if (cfg.prescaler > 0) {
            value = (uint16_t)(raw_value / cfg.prescaler);
          } else {
            value = (uint16_t)raw_value;
          }
          break;

        case COUNTER_FUNC_FREQ:
          // Frequency in Hz (already updated by counter_frequency_update)
          // Read from freq_reg if configured
          if (cfg.freq_reg < HOLDING_REGS_SIZE) {
            value = registers_get_holding_register(cfg.freq_reg);
          } else {
            value = 0;
          }
          break;

        case COUNTER_FUNC_OVERFLOW:
          // Overflow flag (1 if overflow occurred)
          // Read from overload_reg if configured
          if (cfg.overload_reg < HOLDING_REGS_SIZE) {
            value = registers_get_holding_register(cfg.overload_reg);
          } else {
            value = 0;
          }
          break;

        case COUNTER_FUNC_CTRL:
          // Control register (read current value)
          if (cfg.ctrl_reg < HOLDING_REGS_SIZE) {
            value = registers_get_holding_register(cfg.ctrl_reg);
          } else {
            value = 0;
          }
          break;

        default:
          continue;
      }

      // Write value to holding register
      registers_set_holding_register(reg_addr, value);

    } else if (dyn->source_type == DYNAMIC_SOURCE_TIMER) {
      uint8_t timer_id = dyn->source_id;
      TimerConfig cfg;
      memset(&cfg, 0, sizeof(cfg));

      if (!timer_engine_get_config(timer_id, &cfg) || !cfg.enabled) {
        continue;  // Timer not configured or disabled
      }

      // Get timer value based on function type
      uint16_t value = 0;

      switch (dyn->source_function) {
        case TIMER_FUNC_OUTPUT:
          // Timer output state (0 or 1)
          // Read current coil state (timer writes to output_coil via set_coil_level)
          value = registers_get_coil(cfg.output_coil) ? 1 : 0;
          break;

        default:
          continue;
      }

      // Write value to holding register
      registers_set_holding_register(reg_addr, value);
    }
  }
}

/**
 * @brief Update DYNAMIC coils from counter/timer sources
 *
 * Iterates through all DYNAMIC coil mappings and:
 * 1. Gets state from counter or timer
 * 2. Writes to specified coil
 *
 * Called once per main loop iteration
 */
void registers_update_dynamic_coils(void) {
  for (uint8_t i = 0; i < g_persist_config.dynamic_coil_count; i++) {
    const DynamicCoilMapping* dyn = &g_persist_config.dynamic_coils[i];
    uint16_t coil_addr = dyn->coil_address;
    uint8_t value = 0;

    if (dyn->source_type == DYNAMIC_SOURCE_COUNTER) {
      uint8_t counter_id = dyn->source_id;
      CounterConfig cfg;
      memset(&cfg, 0, sizeof(cfg));

      if (!counter_engine_get_config(counter_id, &cfg) || !cfg.enabled) {
        continue;  // Counter not configured or disabled
      }

      // Get counter state based on function type
      switch (dyn->source_function) {
        case COUNTER_FUNC_OVERFLOW:
          // Overflow flag
          // TODO: Get overflow state from counter state
          value = 0;  // Placeholder
          break;

        default:
          continue;
      }

      // Write value to coil
      registers_set_coil(coil_addr, value);

    } else if (dyn->source_type == DYNAMIC_SOURCE_TIMER) {
      uint8_t timer_id = dyn->source_id;
      TimerConfig cfg;
      memset(&cfg, 0, sizeof(cfg));

      if (!timer_engine_get_config(timer_id, &cfg) || !cfg.enabled) {
        continue;  // Timer not configured or disabled
      }

      // Get timer state based on function type
      uint8_t value = 0;

      switch (dyn->source_function) {
        case TIMER_FUNC_OUTPUT:
          // Timer output state (0 or 1)
          // Read current coil state (timer writes to output_coil via set_coil_level)
          value = registers_get_coil(cfg.output_coil) ? 1 : 0;
          break;

        default:
          continue;
      }

      // Write value to coil
      registers_set_coil(coil_addr, value);
    }
  }
}

/* ============================================================================
 * ST LOGIC STATUS REGISTERS (200-251)
 * ============================================================================ */

void registers_update_st_logic_status(void) {
  st_logic_engine_state_t *st_state = st_logic_get_state();

  // Update status for each of 4 logic programs
  for (uint8_t prog_id = 0; prog_id < 4; prog_id++) {
    st_logic_program_config_t *prog = st_logic_get_program(st_state, prog_id);

    if (!prog) continue;

    // =========================================================================
    // INPUT REGISTERS (Status - Read Only)
    // =========================================================================

    // 200-203: Status Register (Status of Logic1-4)
    uint16_t status_reg = 0;
    if (prog->enabled)    status_reg |= ST_LOGIC_STATUS_ENABLED;   // Bit 0
    if (prog->compiled)   status_reg |= ST_LOGIC_STATUS_COMPILED;  // Bit 1
    // Bit 2: Running - will be set during execution (not persistent)
    if (prog->error_count > 0) status_reg |= ST_LOGIC_STATUS_ERROR; // Bit 3
    registers_set_input_register(ST_LOGIC_STATUS_REG_BASE + prog_id, status_reg);

    // 204-207: Execution Count (16-bit)
    registers_set_input_register(ST_LOGIC_EXEC_COUNT_REG_BASE + prog_id,
                                   (uint16_t)(prog->execution_count & 0xFFFF));

    // 208-211: Error Count (16-bit)
    registers_set_input_register(ST_LOGIC_ERROR_COUNT_REG_BASE + prog_id,
                                   (uint16_t)(prog->error_count & 0xFFFF));

    // 212-215: Last Error Code (simple encoding of error string)
    // For now: 0 = no error, 1-255 = error present
    uint16_t error_code = (prog->last_error[0] != '\0') ? 1 : 0;
    registers_set_input_register(ST_LOGIC_ERROR_CODE_REG_BASE + prog_id, error_code);

    // 216-219: Variable Count
    // Count non-zero variable bindings for this program
    uint16_t var_count = 0;
    for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
      const VariableMapping *map = &g_persist_config.var_maps[i];
      if (map->source_type == MAPPING_SOURCE_ST_VAR &&
          map->st_program_id == prog_id) {
        var_count++;
      }
    }
    registers_set_input_register(ST_LOGIC_VAR_COUNT_REG_BASE + prog_id, var_count);

    // 220-251: Variable Values (32 registers total for 4 programs * 8 vars each)
    // Map ST Logic variables to registers
    for (uint8_t i = 0; i < g_persist_config.var_map_count; i++) {
      const VariableMapping *map = &g_persist_config.var_maps[i];
      if (map->source_type == MAPPING_SOURCE_ST_VAR &&
          map->st_program_id == prog_id) {

        // Get variable value from program bytecode
        // Note: This requires access to the bytecode variables
        // For now, we'll store variable values in the ST Logic program context
        // This is a placeholder - the actual variable storage will be handled
        // by the ST Logic VM during execution

        uint16_t var_reg_offset = ST_LOGIC_VAR_VALUES_REG_BASE +
                                  (prog_id * 8) +
                                  map->st_var_index;

        if (var_reg_offset < INPUT_REGS_SIZE) {
          // Variable values should be updated by ST Logic engine during execution
          // Here we just read/preserve them
          // (actual update happens in st_logic_engine.cpp)
        }
      }
    }

    // =========================================================================
    // HOLDING REGISTERS (Control - Read/Write)
    // =========================================================================

    // 200-203: Control Register (Control of Logic1-4)
    // This register is read-write and interpreted by the holding_reg write handler
    // Bit 0: Enable/Disable
    // Bit 1: Start/Stop
    // Bit 2: Reset Error
    // The actual control logic should be handled when these registers are written

  }
}

/* ============================================================================
 * ST LOGIC CONTROL REGISTER HANDLER
 * ============================================================================ */

void registers_process_st_logic_control(uint16_t addr, uint16_t value) {
  // Determine which program this control register is for
  if (addr < ST_LOGIC_CONTROL_REG_BASE || addr >= ST_LOGIC_CONTROL_REG_BASE + 4) {
    return;  // Not a control register
  }

  uint8_t prog_id = addr - ST_LOGIC_CONTROL_REG_BASE;  // 0-3 for Logic1-4
  st_logic_engine_state_t *st_state = st_logic_get_state();
  st_logic_program_config_t *prog = st_logic_get_program(st_state, prog_id);

  if (!prog) return;

  // Bit 0: Enable/Disable program
  if (value & ST_LOGIC_CONTROL_ENABLE) {
    if (!prog->enabled) {
      st_logic_set_enabled(st_state, prog_id, 1);
      debug_print("[ST_LOGIC] Logic");
      debug_print_uint(prog_id + 1);
      debug_println(" ENABLED via Modbus");
    }
  } else {
    if (prog->enabled) {
      st_logic_set_enabled(st_state, prog_id, 0);
      debug_print("[ST_LOGIC] Logic");
      debug_print_uint(prog_id + 1);
      debug_println(" DISABLED via Modbus");
    }
  }

  // Bit 1: Start/Stop (note: this is for future use)
  // Currently, programs run continuously if enabled
  // This bit could control a "pause" state

  // Bit 2: Reset Error flag
  if (value & ST_LOGIC_CONTROL_RESET_ERROR) {
    if (prog->error_count > 0) {
      prog->error_count = 0;
      prog->last_error[0] = '\0';
      debug_print("[ST_LOGIC] Logic");
      debug_print_uint(prog_id + 1);
      debug_println(" error cleared via Modbus");
    }
  }
}
