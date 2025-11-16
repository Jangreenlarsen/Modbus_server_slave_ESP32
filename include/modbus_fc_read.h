/**
 * @file modbus_fc_read.h
 * @brief Modbus read function code handlers (LAYER 2)
 *
 * LAYER 2: Function Code Handlers - Read Operations
 * Responsibility: Implement FC01-04 (read coils, discrete inputs, holding regs, input regs)
 *
 * This file handles:
 * - FC01: Read Coils (0x01)
 * - FC02: Read Discrete Inputs (0x02)
 * - FC03: Read Holding Registers (0x03)
 * - FC04: Read Input Registers (0x04)
 *
 * Does NOT handle:
 * - Write operations (→ modbus_fc_write.h)
 * - Frame parsing (→ modbus_parser.h)
 * - Response serialization (→ modbus_serializer.h)
 */

#ifndef modbus_fc_read_H
#define modbus_fc_read_H

#include <stdint.h>
#include <stdbool.h>
#include "modbus_frame.h"

/* ============================================================================
 * READ FUNCTION CODE HANDLERS (FC01-04)
 * ============================================================================ */

/**
 * @brief Handle FC01: Read Coils
 * @param request_frame Input request frame
 * @param response_frame Output response frame
 * @return true if handled successfully, false otherwise
 */
bool modbus_fc01_read_coils(const ModbusFrame* request_frame, ModbusFrame* response_frame);

/**
 * @brief Handle FC02: Read Discrete Inputs
 * @param request_frame Input request frame
 * @param response_frame Output response frame
 * @return true if handled successfully, false otherwise
 */
bool modbus_fc02_read_discrete_inputs(const ModbusFrame* request_frame, ModbusFrame* response_frame);

/**
 * @brief Handle FC03: Read Holding Registers
 * @param request_frame Input request frame
 * @param response_frame Output response frame
 * @return true if handled successfully, false otherwise
 */
bool modbus_fc03_read_holding_registers(const ModbusFrame* request_frame, ModbusFrame* response_frame);

/**
 * @brief Handle FC04: Read Input Registers
 * @param request_frame Input request frame
 * @param response_frame Output response frame
 * @return true if handled successfully, false otherwise
 */
bool modbus_fc04_read_input_registers(const ModbusFrame* request_frame, ModbusFrame* response_frame);

#endif // modbus_fc_read_H
