/**
 * @file st_vm.cpp
 * @brief Structured Text Virtual Machine Implementation
 *
 * Stack-based interpreter for bytecode execution.
 */

#include "st_vm.h"
#include "st_builtins.h"
#include "st_builtin_modbus.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>

/* ============================================================================
 * INITIALIZATION & RESET
 * ============================================================================ */

void st_vm_init(st_vm_t *vm, const st_bytecode_program_t *program) {
  memset(vm, 0, sizeof(*vm));
  vm->program = program;
  vm->pc = 0;
  vm->sp = 0;
  vm->halted = 0;
  vm->error = 0;
  vm->var_count = program ? program->var_count : 0;

  // Initialize variables from program
  if (program && program->var_count > 0) {
    memcpy(vm->variables, program->variables, program->var_count * sizeof(st_value_t));
  }
}

void st_vm_reset(st_vm_t *vm) {
  if (!vm->program) return;

  vm->pc = 0;
  vm->sp = 0;
  vm->halted = 0;
  vm->error = 0;
  vm->step_count = 0;
  memset(vm->stack, 0, sizeof(vm->stack));
  memcpy(vm->variables, vm->program->variables, vm->var_count * sizeof(st_value_t));
}

/* ============================================================================
 * STACK OPERATIONS
 * ============================================================================ */

// BUG-050: Type-aware push (internal helper)
static bool st_vm_push_typed(st_vm_t *vm, st_value_t value, st_datatype_t type) {
  if (vm->sp >= 64) {
    snprintf(vm->error_msg, sizeof(vm->error_msg), "Stack overflow (max 64)");
    vm->error = 1;
    return false;
  }

  vm->stack[vm->sp] = value;
  vm->type_stack[vm->sp] = type;
  vm->sp++;

  if (vm->sp > vm->max_stack_depth) {
    vm->max_stack_depth = vm->sp;
  }

  return true;
}

bool st_vm_push(st_vm_t *vm, st_value_t value) {
  // Legacy: assume INT type
  return st_vm_push_typed(vm, value, ST_TYPE_INT);
}

// BUG-050: Type-aware pop (internal helper)
static bool st_vm_pop_typed(st_vm_t *vm, st_value_t *out_value, st_datatype_t *out_type) {
  if (vm->sp == 0) {
    snprintf(vm->error_msg, sizeof(vm->error_msg), "Stack underflow");
    vm->error = 1;
    return false;
  }

  vm->sp--;
  *out_value = vm->stack[vm->sp];
  *out_type = vm->type_stack[vm->sp];
  return true;
}

bool st_vm_pop(st_vm_t *vm, st_value_t *out_value) {
  st_datatype_t dummy_type;
  return st_vm_pop_typed(vm, out_value, &dummy_type);
}

st_value_t st_vm_peek(st_vm_t *vm) {
  if (vm->sp == 0) {
    st_value_t empty = {0};
    return empty;
  }
  return vm->stack[vm->sp - 1];
}

/* ============================================================================
 * VARIABLE OPERATIONS
 * ============================================================================ */

st_value_t st_vm_get_variable(st_vm_t *vm, uint8_t var_index) {
  st_value_t empty = {0};
  if (var_index >= vm->var_count) {
    snprintf(vm->error_msg, sizeof(vm->error_msg), "Variable index out of bounds: %d", var_index);
    vm->error = 1;
    return empty;
  }
  return vm->variables[var_index];
}

void st_vm_set_variable(st_vm_t *vm, uint8_t var_index, st_value_t value) {
  if (var_index >= vm->var_count) {
    snprintf(vm->error_msg, sizeof(vm->error_msg), "Variable index out of bounds: %d", var_index);
    vm->error = 1;
    return;
  }
  vm->variables[var_index] = value;
}

/* ============================================================================
 * INSTRUCTION EXECUTION
 * ============================================================================ */

static bool st_vm_exec_push_bool(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t val;
  val.bool_val = (instr->arg.int_arg != 0);
  return st_vm_push_typed(vm, val, ST_TYPE_BOOL);  // BUG-050
}

static bool st_vm_exec_push_int(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t val;
  memset(&val, 0, sizeof(val));  // Initialize union to all zeros
  val.int_val = instr->arg.int_arg;
  return st_vm_push_typed(vm, val, ST_TYPE_INT);  // BUG-050
}

static bool st_vm_exec_push_dword(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t val;
  val.dword_val = (uint32_t)instr->arg.int_arg;
  return st_vm_push_typed(vm, val, ST_TYPE_DWORD);  // BUG-050
}

static bool st_vm_exec_push_real(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t val;
  memset(&val, 0, sizeof(val));  // BUG-050: Clear union
  // Retrieve float from int bits (hack from compiler)
  memcpy(&val.real_val, &instr->arg.int_arg, sizeof(float));
  return st_vm_push_typed(vm, val, ST_TYPE_REAL);  // BUG-050
}

static bool st_vm_exec_load_var(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t val = st_vm_get_variable(vm, instr->arg.var_index);
  if (vm->error) return false;
  // BUG-050: Push with correct type from program
  st_datatype_t var_type = vm->program->var_types[instr->arg.var_index];
  return st_vm_push_typed(vm, val, var_type);
}

static bool st_vm_exec_store_var(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t val;
  if (!st_vm_pop(vm, &val)) return false;
  st_vm_set_variable(vm, instr->arg.var_index, val);
  return !vm->error;
}

static bool st_vm_exec_dup(st_vm_t *vm, st_bytecode_instr_t *instr) {
  // Duplicate the top stack value
  if (vm->sp == 0) {
    snprintf(vm->error_msg, sizeof(vm->error_msg), "Stack underflow (DUP)");
    vm->error = 1;
    return false;
  }
  st_value_t top = vm->stack[vm->sp - 1];
  st_datatype_t top_type = vm->type_stack[vm->sp - 1];  // BUG-072: Preserve type
  return st_vm_push_typed(vm, top, top_type);
}

static bool st_vm_exec_pop(st_vm_t *vm, st_bytecode_instr_t *instr) {
  // Pop and discard top stack value
  st_value_t discard;
  return st_vm_pop(vm, &discard);
}

/* ============================================================================
 * ARITHMETIC OPERATIONS
 * ============================================================================ */

static bool st_vm_exec_add(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  st_datatype_t right_type, left_type;

  // BUG-050: Pop with type information
  if (!st_vm_pop_typed(vm, &right, &right_type)) return false;
  if (!st_vm_pop_typed(vm, &left, &left_type)) return false;

  // If either operand is REAL, result is REAL
  if (left_type == ST_TYPE_REAL || right_type == ST_TYPE_REAL) {
    float left_f = (left_type == ST_TYPE_REAL) ? left.real_val : (float)left.int_val;
    float right_f = (right_type == ST_TYPE_REAL) ? right.real_val : (float)right.int_val;
    result.real_val = left_f + right_f;
    return st_vm_push_typed(vm, result, ST_TYPE_REAL);
  } else {
    result.int_val = left.int_val + right.int_val;
    return st_vm_push_typed(vm, result, ST_TYPE_INT);
  }
}

static bool st_vm_exec_sub(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  st_datatype_t right_type, left_type;

  // BUG-050: Pop with type information
  if (!st_vm_pop_typed(vm, &right, &right_type)) return false;
  if (!st_vm_pop_typed(vm, &left, &left_type)) return false;

  // If either operand is REAL, result is REAL
  if (left_type == ST_TYPE_REAL || right_type == ST_TYPE_REAL) {
    float left_f = (left_type == ST_TYPE_REAL) ? left.real_val : (float)left.int_val;
    float right_f = (right_type == ST_TYPE_REAL) ? right.real_val : (float)right.int_val;
    result.real_val = left_f - right_f;
    return st_vm_push_typed(vm, result, ST_TYPE_REAL);
  } else {
    result.int_val = left.int_val - right.int_val;
    return st_vm_push_typed(vm, result, ST_TYPE_INT);
  }
}

static bool st_vm_exec_mul(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  st_datatype_t right_type, left_type;

  // BUG-050: Pop with type information
  if (!st_vm_pop_typed(vm, &right, &right_type)) return false;
  if (!st_vm_pop_typed(vm, &left, &left_type)) return false;

  // If either operand is REAL, result is REAL
  if (left_type == ST_TYPE_REAL || right_type == ST_TYPE_REAL) {
    float left_f = (left_type == ST_TYPE_REAL) ? left.real_val : (float)left.int_val;
    float right_f = (right_type == ST_TYPE_REAL) ? right.real_val : (float)right.int_val;
    result.real_val = left_f * right_f;
    return st_vm_push_typed(vm, result, ST_TYPE_REAL);
  } else {
    result.int_val = left.int_val * right.int_val;
    return st_vm_push_typed(vm, result, ST_TYPE_INT);
  }
}

static bool st_vm_exec_div(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  st_datatype_t right_type, left_type;

  // BUG-050: Pop with type information
  if (!st_vm_pop_typed(vm, &right, &right_type)) return false;
  if (!st_vm_pop_typed(vm, &left, &left_type)) return false;

  // Division always returns REAL (to preserve precision)
  float left_f = (left_type == ST_TYPE_REAL) ? left.real_val : (float)left.int_val;
  float right_f = (right_type == ST_TYPE_REAL) ? right.real_val : (float)right.int_val;

  if (right_f == 0.0f) {
    snprintf(vm->error_msg, sizeof(vm->error_msg), "Division by zero");
    vm->error = 1;
    return false;
  }

  result.real_val = left_f / right_f;
  return st_vm_push_typed(vm, result, ST_TYPE_REAL);
}

static bool st_vm_exec_mod(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  if (!st_vm_pop(vm, &right)) return false;
  if (!st_vm_pop(vm, &left)) return false;

  if (right.int_val == 0) {
    snprintf(vm->error_msg, sizeof(vm->error_msg), "Modulo by zero");
    vm->error = 1;
    return false;
  }

  // BUG-083: Handle INT_MIN % -1 overflow (undefined behavior in C/C++)
  if (left.int_val == INT32_MIN && right.int_val == -1) {
    result.int_val = 0;  // Mathematically correct (INT_MIN % -1 = 0)
    return st_vm_push_typed(vm, result, ST_TYPE_INT);
  }

  result.int_val = left.int_val % right.int_val;
  return st_vm_push_typed(vm, result, ST_TYPE_INT);
}

static bool st_vm_exec_neg(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t val, result;
  st_datatype_t val_type;

  // BUG-060: Pop with type information
  if (!st_vm_pop_typed(vm, &val, &val_type)) return false;

  // Negate based on type
  if (val_type == ST_TYPE_REAL) {
    result.real_val = -val.real_val;
    return st_vm_push_typed(vm, result, ST_TYPE_REAL);
  } else {
    // BUG-087: Handle INT_MIN negation (undefined behavior in C/C++)
    if (val.int_val == INT32_MIN) {
      // -INT_MIN overflows to INT_MAX+1, convert to REAL for safe negation
      result.real_val = -(float)val.int_val;  // 2147483648.0
      return st_vm_push_typed(vm, result, ST_TYPE_REAL);
    }
    result.int_val = -val.int_val;
    return st_vm_push_typed(vm, result, ST_TYPE_INT);
  }
}

/* ============================================================================
 * LOGICAL OPERATIONS
 * ============================================================================ */

static bool st_vm_exec_and(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  if (!st_vm_pop(vm, &right)) return false;
  if (!st_vm_pop(vm, &left)) return false;

  result.bool_val = (left.bool_val != 0) && (right.bool_val != 0);
  return st_vm_push_typed(vm, result, ST_TYPE_BOOL);
}

static bool st_vm_exec_or(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  if (!st_vm_pop(vm, &right)) return false;
  if (!st_vm_pop(vm, &left)) return false;

  result.bool_val = (left.bool_val != 0) || (right.bool_val != 0);
  return st_vm_push_typed(vm, result, ST_TYPE_BOOL);
}

static bool st_vm_exec_xor(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  if (!st_vm_pop(vm, &right)) return false;
  if (!st_vm_pop(vm, &left)) return false;

  result.bool_val = (left.bool_val != 0) != (right.bool_val != 0);
  return st_vm_push_typed(vm, result, ST_TYPE_BOOL);
}

static bool st_vm_exec_not(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t val, result;
  if (!st_vm_pop(vm, &val)) return false;

  result.bool_val = (val.bool_val == 0);
  return st_vm_push_typed(vm, result, ST_TYPE_BOOL);
}

/* ============================================================================
 * COMPARISON OPERATIONS
 * ============================================================================ */

static bool st_vm_exec_eq(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  st_datatype_t right_type, left_type;

  // BUG-059: Pop with type information
  if (!st_vm_pop_typed(vm, &right, &right_type)) return false;
  if (!st_vm_pop_typed(vm, &left, &left_type)) return false;

  // If either operand is REAL, compare as REAL
  if (left_type == ST_TYPE_REAL || right_type == ST_TYPE_REAL) {
    float left_f = (left_type == ST_TYPE_REAL) ? left.real_val : (float)left.int_val;
    float right_f = (right_type == ST_TYPE_REAL) ? right.real_val : (float)right.int_val;
    result.bool_val = (left_f == right_f);
  } else {
    result.bool_val = (left.int_val == right.int_val);
  }
  return st_vm_push_typed(vm, result, ST_TYPE_BOOL);
}

static bool st_vm_exec_ne(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  st_datatype_t right_type, left_type;

  // BUG-059: Pop with type information
  if (!st_vm_pop_typed(vm, &right, &right_type)) return false;
  if (!st_vm_pop_typed(vm, &left, &left_type)) return false;

  // If either operand is REAL, compare as REAL
  if (left_type == ST_TYPE_REAL || right_type == ST_TYPE_REAL) {
    float left_f = (left_type == ST_TYPE_REAL) ? left.real_val : (float)left.int_val;
    float right_f = (right_type == ST_TYPE_REAL) ? right.real_val : (float)right.int_val;
    result.bool_val = (left_f != right_f);
  } else {
    result.bool_val = (left.int_val != right.int_val);
  }
  return st_vm_push_typed(vm, result, ST_TYPE_BOOL);
}

static bool st_vm_exec_lt(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  st_datatype_t right_type, left_type;

  // BUG-059: Pop with type information
  if (!st_vm_pop_typed(vm, &right, &right_type)) return false;
  if (!st_vm_pop_typed(vm, &left, &left_type)) return false;

  // If either operand is REAL, compare as REAL
  if (left_type == ST_TYPE_REAL || right_type == ST_TYPE_REAL) {
    float left_f = (left_type == ST_TYPE_REAL) ? left.real_val : (float)left.int_val;
    float right_f = (right_type == ST_TYPE_REAL) ? right.real_val : (float)right.int_val;
    result.bool_val = (left_f < right_f);
  } else {
    result.bool_val = (left.int_val < right.int_val);
  }
  return st_vm_push_typed(vm, result, ST_TYPE_BOOL);
}

static bool st_vm_exec_gt(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  st_datatype_t right_type, left_type;

  // BUG-059: Pop with type information
  if (!st_vm_pop_typed(vm, &right, &right_type)) return false;
  if (!st_vm_pop_typed(vm, &left, &left_type)) return false;

  // If either operand is REAL, compare as REAL
  if (left_type == ST_TYPE_REAL || right_type == ST_TYPE_REAL) {
    float left_f = (left_type == ST_TYPE_REAL) ? left.real_val : (float)left.int_val;
    float right_f = (right_type == ST_TYPE_REAL) ? right.real_val : (float)right.int_val;
    result.bool_val = (left_f > right_f);
  } else {
    result.bool_val = (left.int_val > right.int_val);
  }
  return st_vm_push_typed(vm, result, ST_TYPE_BOOL);
}

static bool st_vm_exec_le(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  st_datatype_t right_type, left_type;

  // BUG-059: Pop with type information
  if (!st_vm_pop_typed(vm, &right, &right_type)) return false;
  if (!st_vm_pop_typed(vm, &left, &left_type)) return false;

  // If either operand is REAL, compare as REAL
  if (left_type == ST_TYPE_REAL || right_type == ST_TYPE_REAL) {
    float left_f = (left_type == ST_TYPE_REAL) ? left.real_val : (float)left.int_val;
    float right_f = (right_type == ST_TYPE_REAL) ? right.real_val : (float)right.int_val;
    result.bool_val = (left_f <= right_f);
  } else {
    result.bool_val = (left.int_val <= right.int_val);
  }
  return st_vm_push_typed(vm, result, ST_TYPE_BOOL);
}

static bool st_vm_exec_ge(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  st_datatype_t right_type, left_type;

  // BUG-059: Pop with type information
  if (!st_vm_pop_typed(vm, &right, &right_type)) return false;
  if (!st_vm_pop_typed(vm, &left, &left_type)) return false;

  // If either operand is REAL, compare as REAL
  if (left_type == ST_TYPE_REAL || right_type == ST_TYPE_REAL) {
    float left_f = (left_type == ST_TYPE_REAL) ? left.real_val : (float)left.int_val;
    float right_f = (right_type == ST_TYPE_REAL) ? right.real_val : (float)right.int_val;
    result.bool_val = (left_f >= right_f);
  } else {
    result.bool_val = (left.int_val >= right.int_val);
  }
  return st_vm_push_typed(vm, result, ST_TYPE_BOOL);
}

/* ============================================================================
 * BITWISE OPERATIONS
 * ============================================================================ */

static bool st_vm_exec_shl(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  if (!st_vm_pop(vm, &right)) return false;
  if (!st_vm_pop(vm, &left)) return false;

  // BUG-073: Check shift amount (undefined behavior if >= 32)
  if (right.int_val < 0 || right.int_val >= 32) {
    snprintf(vm->error_msg, sizeof(vm->error_msg), "Shift amount out of range (0-31)");
    vm->error = 1;
    return false;
  }

  result.int_val = left.int_val << right.int_val;
  return st_vm_push_typed(vm, result, ST_TYPE_INT);
}

static bool st_vm_exec_shr(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t right, left, result;
  if (!st_vm_pop(vm, &right)) return false;
  if (!st_vm_pop(vm, &left)) return false;

  // BUG-073: Check shift amount (undefined behavior if >= 32)
  if (right.int_val < 0 || right.int_val >= 32) {
    snprintf(vm->error_msg, sizeof(vm->error_msg), "Shift amount out of range (0-31)");
    vm->error = 1;
    return false;
  }

  result.int_val = left.int_val >> right.int_val;
  return st_vm_push_typed(vm, result, ST_TYPE_INT);
}

/* ============================================================================
 * FUNCTION CALLS
 * ============================================================================ */

static bool st_vm_exec_call_builtin(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_builtin_func_t func_id = (st_builtin_func_t)instr->arg.int_arg;
  uint8_t arg_count = st_builtin_arg_count(func_id);

  st_value_t arg1 = {0}, arg2 = {0}, arg3 = {0};
  st_datatype_t arg1_type = ST_TYPE_INT;
  st_datatype_t arg2_type = ST_TYPE_INT;
  st_datatype_t arg3_type = ST_TYPE_INT;

  // Pop arguments with type information (in reverse order: arg3, arg2, arg1)
  // Stack layout: [arg1, arg2, arg3] (top)
  if (arg_count >= 3) {
    if (!st_vm_pop_typed(vm, &arg3, &arg3_type)) return false;
  }
  if (arg_count >= 2) {
    if (!st_vm_pop_typed(vm, &arg2, &arg2_type)) return false;
  }
  if (arg_count >= 1) {
    if (!st_vm_pop_typed(vm, &arg1, &arg1_type)) return false;
  }

  // Call the function (handle 3-arg functions specially)
  st_value_t result;
  if (arg_count == 3) {
    // Special handling for 3-arg functions
    if (func_id == ST_BUILTIN_LIMIT) {
      result = st_builtin_limit(arg1, arg2, arg3);
    } else if (func_id == ST_BUILTIN_SEL) {
      result = st_builtin_sel(arg1, arg2, arg3);
    } else if (func_id == ST_BUILTIN_MB_WRITE_COIL) {
      result = st_builtin_mb_write_coil(arg1, arg2, arg3);
    } else if (func_id == ST_BUILTIN_MB_WRITE_HOLDING) {
      result = st_builtin_mb_write_holding(arg1, arg2, arg3);
    } else {
      result = st_builtin_call(func_id, arg1, arg2);
    }
  } else {
    result = st_builtin_call(func_id, arg1, arg2);
  }

  // BUG-077: Infer return type for polymorphic functions (SEL, LIMIT)
  st_datatype_t return_type;
  if (func_id == ST_BUILTIN_SEL) {
    // SEL returns same type as in0/in1 (arg2 and arg3)
    // If either is REAL, return REAL (type promotion)
    return_type = (arg2_type == ST_TYPE_REAL || arg3_type == ST_TYPE_REAL) ? ST_TYPE_REAL : arg2_type;
  } else if (func_id == ST_BUILTIN_LIMIT) {
    // LIMIT returns same type as min/val/max (arg1, arg2, arg3)
    // If any is REAL, return REAL (type promotion)
    if (arg1_type == ST_TYPE_REAL || arg2_type == ST_TYPE_REAL || arg3_type == ST_TYPE_REAL) {
      return_type = ST_TYPE_REAL;
    } else {
      return_type = arg1_type;  // All are INT/BOOL/DWORD → use first
    }
  } else {
    // Non-polymorphic function: use static return type
    return_type = st_builtin_return_type(func_id);
  }

  return st_vm_push_typed(vm, result, return_type);
}

/* ============================================================================
 * CONTROL FLOW
 * ============================================================================ */

static bool st_vm_exec_jmp(st_vm_t *vm, st_bytecode_instr_t *instr) {
  vm->pc = (uint16_t)instr->arg.int_arg;
  return true;
}

static bool st_vm_exec_jmp_if_false(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t cond;
  if (!st_vm_pop(vm, &cond)) return false;

  if (cond.bool_val == 0) {
    vm->pc = (uint16_t)instr->arg.int_arg;  // Jump to target
  } else {
    vm->pc = vm->pc + 1;  // Continue to next instruction
  }
  return true;
}

static bool st_vm_exec_jmp_if_true(st_vm_t *vm, st_bytecode_instr_t *instr) {
  st_value_t cond;
  if (!st_vm_pop(vm, &cond)) return false;

  if (cond.bool_val != 0) {
    vm->pc = (uint16_t)instr->arg.int_arg;  // Jump to target
  } else {
    vm->pc = vm->pc + 1;  // Continue to next instruction
  }
  return true;
}

/* ============================================================================
 * MAIN EXECUTION ENGINE
 * ============================================================================ */

bool st_vm_step(st_vm_t *vm) {
  if (!vm->program) {
    snprintf(vm->error_msg, sizeof(vm->error_msg), "No program loaded");
    vm->error = 1;
    return false;
  }

  if (vm->pc >= vm->program->instr_count) {
    vm->halted = 1;
    return false;
  }

  const st_bytecode_instr_t *const_instr = &vm->program->instructions[vm->pc];
  st_bytecode_instr_t *instr = const_cast<st_bytecode_instr_t *>(const_instr);

  // Execute instruction
  bool result = true;
  switch (instr->opcode) {
    case ST_OP_PUSH_BOOL:       result = st_vm_exec_push_bool(vm, instr); break;
    case ST_OP_PUSH_INT:        result = st_vm_exec_push_int(vm, instr); break;
    case ST_OP_PUSH_DWORD:      result = st_vm_exec_push_dword(vm, instr); break;
    case ST_OP_PUSH_REAL:       result = st_vm_exec_push_real(vm, instr); break;
    case ST_OP_LOAD_VAR:        result = st_vm_exec_load_var(vm, instr); break;
    case ST_OP_STORE_VAR:       result = st_vm_exec_store_var(vm, instr); break;
    case ST_OP_DUP:             result = st_vm_exec_dup(vm, instr); break;
    case ST_OP_POP:             result = st_vm_exec_pop(vm, instr); break;
    case ST_OP_ADD:             result = st_vm_exec_add(vm, instr); break;
    case ST_OP_SUB:             result = st_vm_exec_sub(vm, instr); break;
    case ST_OP_MUL:             result = st_vm_exec_mul(vm, instr); break;
    case ST_OP_DIV:             result = st_vm_exec_div(vm, instr); break;
    case ST_OP_MOD:             result = st_vm_exec_mod(vm, instr); break;
    case ST_OP_NEG:             result = st_vm_exec_neg(vm, instr); break;
    case ST_OP_AND:             result = st_vm_exec_and(vm, instr); break;
    case ST_OP_OR:              result = st_vm_exec_or(vm, instr); break;
    case ST_OP_XOR:             result = st_vm_exec_xor(vm, instr); break;
    case ST_OP_NOT:             result = st_vm_exec_not(vm, instr); break;
    case ST_OP_EQ:              result = st_vm_exec_eq(vm, instr); break;
    case ST_OP_NE:              result = st_vm_exec_ne(vm, instr); break;
    case ST_OP_LT:              result = st_vm_exec_lt(vm, instr); break;
    case ST_OP_GT:              result = st_vm_exec_gt(vm, instr); break;
    case ST_OP_LE:              result = st_vm_exec_le(vm, instr); break;
    case ST_OP_GE:              result = st_vm_exec_ge(vm, instr); break;
    case ST_OP_SHL:             result = st_vm_exec_shl(vm, instr); break;
    case ST_OP_SHR:             result = st_vm_exec_shr(vm, instr); break;
    case ST_OP_CALL_BUILTIN:    result = st_vm_exec_call_builtin(vm, instr); break;
    case ST_OP_JMP:             result = st_vm_exec_jmp(vm, instr); break;
    case ST_OP_JMP_IF_FALSE:    result = st_vm_exec_jmp_if_false(vm, instr); break;
    case ST_OP_JMP_IF_TRUE:     result = st_vm_exec_jmp_if_true(vm, instr); break;
    case ST_OP_NOP:             break;
    case ST_OP_HALT:
      vm->halted = 1;
      return false;
    default:
      snprintf(vm->error_msg, sizeof(vm->error_msg), "Unknown opcode: %d", instr->opcode);
      vm->error = 1;
      return false;
  }

  if (!result) {
    vm->error = 1;
    return false;
  }

  // Advance PC (unless instruction changed it)
  if (instr->opcode != ST_OP_JMP && instr->opcode != ST_OP_JMP_IF_FALSE && instr->opcode != ST_OP_JMP_IF_TRUE) {
    vm->pc++;
  } else {
    // PC was already set by jump instruction
  }

  vm->step_count++;
  return !vm->error;
}

bool st_vm_run(st_vm_t *vm, uint32_t max_steps) {
  uint32_t steps = 0;

  while (!vm->halted && !vm->error) {
    if (max_steps > 0 && steps >= max_steps) {
      snprintf(vm->error_msg, sizeof(vm->error_msg), "Max steps exceeded (%u)", max_steps);
      vm->error = 1;
      return false;
    }

    if (!st_vm_step(vm)) {
      break;
    }

    steps++;
  }

  return !vm->error;
}

/* ============================================================================
 * DEBUGGING
 * ============================================================================ */

void st_vm_print_state(st_vm_t *vm) {
  if (!vm || !vm->program) {
    debug_printf("VM not initialized\n");
    return;
  }

  debug_printf("\n=== VM State ===\n");
  debug_printf("Program: %s\n", vm->program->name);
  debug_printf("PC: %d / %d\n", vm->pc, vm->program->instr_count);
  debug_printf("Stack pointer: %d / 64\n", vm->sp);
  debug_printf("Halted: %s\n", vm->halted ? "Yes" : "No");
  debug_printf("Error: %s\n", vm->error ? vm->error_msg : "None");
  debug_printf("Steps: %u\n", vm->step_count);
  debug_printf("Max stack: %u\n\n", vm->max_stack_depth);
}

void st_vm_print_stack(st_vm_t *vm) {
  debug_printf("\n=== Stack (depth %d) ===\n", vm->sp);
  for (int i = vm->sp - 1; i >= 0; i--) {
    debug_printf("  [%d] INT: %d\n", i, vm->stack[i].int_val);
  }
  debug_printf("\n");
}

void st_vm_print_variables(st_vm_t *vm) {
  debug_printf("\n=== Variables (%d) ===\n", vm->var_count);
  for (int i = 0; i < vm->var_count; i++) {
    debug_printf("  [%d] INT: %d\n", i, vm->variables[i].int_val);
  }
  debug_printf("\n");
}
