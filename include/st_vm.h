/**
 * @file st_vm.h
 * @brief Structured Text Stack-based Virtual Machine
 *
 * Executes compiled bytecode. Stack-based architecture (similar to Java bytecode).
 *
 * Usage:
 *   st_vm_t vm;
 *   st_vm_init(&vm, bytecode_program);
 *
 *   // Execute step-by-step (for async integration)
 *   while (!vm.halted && !vm.error) {
 *     st_vm_step(&vm);
 *   }
 *
 *   // Or execute all at once
 *   st_vm_run(&vm, MAX_STEPS);
 *
 *   // Access results
 *   int result = vm.variables[0].int_val;
 */

#ifndef ST_VM_H
#define ST_VM_H

#include "st_types.h"

/* VM execution state */
typedef struct {
  // Bytecode being executed
  const st_bytecode_program_t *program;

  // Execution state
  uint16_t pc;                // Program counter
  uint8_t halted;             // Execution halted (HALT instruction)
  uint8_t error;              // Error flag
  char error_msg[256];        // Error message

  // Stack (for expression evaluation)
  st_value_t stack[64];       // Value stack (max 64 depth)
  st_datatype_t type_stack[64]; // Type stack (BUG-050: track value types for arithmetic)
  uint8_t sp;                 // Stack pointer (index of next free slot)

  // Variable storage (local to this execution)
  st_value_t variables[32];   // Local variables (mirrors bytecode->variables)
  uint8_t var_count;

  // Execution statistics (optional)
  uint32_t step_count;        // Total steps executed
  uint32_t max_stack_depth;   // Peak stack usage
} st_vm_t;

/**
 * @brief Initialize VM with bytecode program
 * @param vm VM state
 * @param program Compiled bytecode program
 */
void st_vm_init(st_vm_t *vm, const st_bytecode_program_t *program);

/**
 * @brief Execute one instruction and advance PC
 * @param vm VM state
 * @return true if continue, false if halted or error
 */
bool st_vm_step(st_vm_t *vm);

/**
 * @brief Execute all instructions until halt or error
 * @param vm VM state
 * @param max_steps Maximum steps to execute (0 = unlimited)
 * @return true if completed successfully, false if error
 */
bool st_vm_run(st_vm_t *vm, uint32_t max_steps);

/**
 * @brief Reset VM to initial state (keeps program reference)
 * @param vm VM state
 */
void st_vm_reset(st_vm_t *vm);

/**
 * @brief Get variable value by name
 * @param vm VM state
 * @param var_index Variable index
 * @return Variable value
 */
st_value_t st_vm_get_variable(st_vm_t *vm, uint8_t var_index);

/**
 * @brief Set variable value by index
 * @param vm VM state
 * @param var_index Variable index
 * @param value New value
 */
void st_vm_set_variable(st_vm_t *vm, uint8_t var_index, st_value_t value);

/**
 * @brief Push value onto stack
 * @param vm VM state
 * @param value Value to push
 * @return true if successful, false if stack overflow
 */
bool st_vm_push(st_vm_t *vm, st_value_t value);

/**
 * @brief Pop value from stack
 * @param vm VM state
 * @param out_value Pointer to store popped value
 * @return true if successful, false if stack underflow
 */
bool st_vm_pop(st_vm_t *vm, st_value_t *out_value);

/**
 * @brief Peek at top of stack without popping
 * @param vm VM state
 * @return Top stack value (undefined if stack empty)
 */
st_value_t st_vm_peek(st_vm_t *vm);

/**
 * @brief Print VM state (for debugging)
 * @param vm VM state
 */
void st_vm_print_state(st_vm_t *vm);

/**
 * @brief Print stack contents
 * @param vm VM state
 */
void st_vm_print_stack(st_vm_t *vm);

/**
 * @brief Print variables
 * @param vm VM state
 */
void st_vm_print_variables(st_vm_t *vm);

#endif // ST_VM_H
