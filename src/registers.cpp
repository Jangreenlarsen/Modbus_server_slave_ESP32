/**
 * @file registers.cpp
 * @brief Register and coil storage implementation
 *
 * Provides arrays for holding/input registers and coils/discrete inputs
 * All Modbus read/write operations go through these functions
 */

#include "registers.h"
#include <Arduino.h>
#include <string.h>

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
