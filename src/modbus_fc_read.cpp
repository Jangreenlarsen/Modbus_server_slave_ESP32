/**
 * @file modbus_fc_read.cpp
 * @brief Modbus read function code handlers implementation (LAYER 2)
 *
 * Implements FC01-04: Read Coils, Discrete Inputs, Holding Registers, Input Registers
 */

#include "modbus_fc_read.h"
#include "modbus_parser.h"
#include "modbus_serializer.h"
#include "registers.h"
#include "counter_config.h"
#include "constants.h"
#include "debug.h"
#include <string.h>

/* ============================================================================
 * HELPER FUNCTION: RESET-ON-READ HANDLING
 * ============================================================================ */

/**
 * @brief Handle reset-on-read for counter compare status bits (FC03)
 *
 * After reading holding registers, checks if any counter has reset_on_read enabled
 * and ctrl_reg matches the read address range. If so, clears bit 4 (compare status bit).
 *
 * This implements the auto-clear behavior: when Modbus master reads the control register,
 * the compare status bit (bit 4) is automatically cleared for next comparison cycle.
 */
static void modbus_fc03_handle_reset_on_read(uint16_t starting_address, uint16_t quantity) {
  // Iterate through all counters to find those with reset-on-read enabled
  for (uint8_t id = 1; id <= 4; id++) {
    CounterConfig cfg;
    if (!counter_config_get(id, &cfg)) continue;
    if (!cfg.compare_enabled || !cfg.reset_on_read) continue;

    // Check if this counter's control register was read
    uint16_t ctrl_reg = cfg.ctrl_reg;
    if (ctrl_reg >= HOLDING_REGS_SIZE) continue;  // Invalid register
    if (ctrl_reg < starting_address || ctrl_reg >= starting_address + quantity) {
      continue;  // Control register not in read range
    }

    // Clear bit 4 (compare status bit) in control register
    uint16_t ctrl_val = registers_get_holding_register(ctrl_reg);
    ctrl_val &= ~(1 << 4);  // Clear bit 4
    registers_set_holding_register(ctrl_reg, ctrl_val);

    debug_print("FC03 reset-on-read: Counter ");
    debug_print_uint(id);
    debug_print(" compare bit cleared (ctrl-reg ");
    debug_print_uint(ctrl_reg);
    debug_println(")");
  }
}

/* ============================================================================
 * FC01: READ COILS
 * ============================================================================ */

bool modbus_fc01_read_coils(const ModbusFrame* request_frame, ModbusFrame* response_frame) {
  if (request_frame == NULL || response_frame == NULL) return false;

  // Parse request
  ModbusReadRequest req;
  if (!modbus_parse_read_request(request_frame, &req)) {
    modbus_serialize_error_response(response_frame, request_frame->slave_id,
                                     FC_READ_COILS, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    return false;
  }

  // Validate address range
  if (req.starting_address + req.quantity > (COILS_SIZE * 8)) {
    modbus_serialize_error_response(response_frame, request_frame->slave_id,
                                     FC_READ_COILS, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    return false;
  }

  // Read coils from storage
  uint8_t byte_count = (req.quantity + 7) / 8;  // Round up to nearest byte
  uint8_t coil_data[256];
  memset(coil_data, 0, sizeof(coil_data));

  for (uint16_t i = 0; i < req.quantity; i++) {
    uint8_t coil_value = registers_get_coil(req.starting_address + i);
    if (coil_value) {
      uint16_t byte_idx = i / 8;
      uint16_t bit_idx = i % 8;
      coil_data[byte_idx] |= (1 << bit_idx);
    }
  }

  // Serialize response
  return modbus_serialize_read_bits_response(response_frame, request_frame->slave_id,
                                              FC_READ_COILS, coil_data, byte_count);
}

/* ============================================================================
 * FC02: READ DISCRETE INPUTS
 * ============================================================================ */

bool modbus_fc02_read_discrete_inputs(const ModbusFrame* request_frame, ModbusFrame* response_frame) {
  if (request_frame == NULL || response_frame == NULL) return false;

  // Parse request
  ModbusReadRequest req;
  if (!modbus_parse_read_request(request_frame, &req)) {
    modbus_serialize_error_response(response_frame, request_frame->slave_id,
                                     FC_READ_DISCRETE_INPUTS, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    return false;
  }

  // Validate address range
  if (req.starting_address + req.quantity > (DISCRETE_INPUTS_SIZE * 8)) {
    modbus_serialize_error_response(response_frame, request_frame->slave_id,
                                     FC_READ_DISCRETE_INPUTS, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    return false;
  }

  // Read discrete inputs from storage
  uint8_t byte_count = (req.quantity + 7) / 8;  // Round up to nearest byte
  uint8_t input_data[256];
  memset(input_data, 0, sizeof(input_data));

  for (uint16_t i = 0; i < req.quantity; i++) {
    uint8_t input_value = registers_get_discrete_input(req.starting_address + i);
    if (input_value) {
      uint16_t byte_idx = i / 8;
      uint16_t bit_idx = i % 8;
      input_data[byte_idx] |= (1 << bit_idx);
    }
  }

  // Serialize response
  return modbus_serialize_read_bits_response(response_frame, request_frame->slave_id,
                                              FC_READ_DISCRETE_INPUTS, input_data, byte_count);
}

/* ============================================================================
 * FC03: READ HOLDING REGISTERS
 * ============================================================================ */

bool modbus_fc03_read_holding_registers(const ModbusFrame* request_frame, ModbusFrame* response_frame) {
  if (request_frame == NULL || response_frame == NULL) return false;

  // Parse request
  ModbusReadRequest req;
  if (!modbus_parse_read_request(request_frame, &req)) {
    modbus_serialize_error_response(response_frame, request_frame->slave_id,
                                     FC_READ_HOLDING_REGS, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    return false;
  }

  // Validate address range
  if (req.starting_address + req.quantity > HOLDING_REGS_SIZE) {
    modbus_serialize_error_response(response_frame, request_frame->slave_id,
                                     FC_READ_HOLDING_REGS, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    return false;
  }

  // Read holding registers from storage
  uint16_t register_data[125];
  for (uint16_t i = 0; i < req.quantity; i++) {
    register_data[i] = registers_get_holding_register(req.starting_address + i);
  }

  // Handle reset-on-read for counter compare status bits (v2.3+)
  // This must happen AFTER reading registers but BEFORE sending response
  // so the master receives the current value, then clears bit 4 for next cycle
  modbus_fc03_handle_reset_on_read(req.starting_address, req.quantity);

  // Serialize response
  return modbus_serialize_read_registers_response(response_frame, request_frame->slave_id,
                                                   FC_READ_HOLDING_REGS, register_data, req.quantity);
}

/* ============================================================================
 * FC04: READ INPUT REGISTERS
 * ============================================================================ */

bool modbus_fc04_read_input_registers(const ModbusFrame* request_frame, ModbusFrame* response_frame) {
  if (request_frame == NULL || response_frame == NULL) return false;

  // Parse request
  ModbusReadRequest req;
  if (!modbus_parse_read_request(request_frame, &req)) {
    modbus_serialize_error_response(response_frame, request_frame->slave_id,
                                     FC_READ_INPUT_REGS, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    return false;
  }

  // Validate address range
  if (req.starting_address + req.quantity > INPUT_REGS_SIZE) {
    modbus_serialize_error_response(response_frame, request_frame->slave_id,
                                     FC_READ_INPUT_REGS, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    return false;
  }

  // Read input registers from storage
  uint16_t register_data[125];
  for (uint16_t i = 0; i < req.quantity; i++) {
    register_data[i] = registers_get_input_register(req.starting_address + i);
  }

  // Serialize response
  return modbus_serialize_read_registers_response(response_frame, request_frame->slave_id,
                                                   FC_READ_INPUT_REGS, register_data, req.quantity);
}

