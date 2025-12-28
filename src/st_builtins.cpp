/**
 * @file st_builtins.cpp
 * @brief Structured Text Built-in Functions Implementation
 *
 * IEC 61131-3 standard mathematical and conversion functions.
 */

#include "st_builtins.h"
#include "st_builtin_persist.h"
#include "st_builtin_modbus.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * MATHEMATICAL FUNCTIONS
 * ============================================================================ */

st_value_t st_builtin_abs(st_value_t x) {
  st_value_t result;
  // BUG-088 & BUG-105: Handle INT16_MIN overflow (-INT16_MIN = INT16_MIN, not positive)
  if (x.int_val == INT16_MIN) {
    result.int_val = INT16_MAX;  // Clamp to max positive value (32767)
  } else {
    result.int_val = (x.int_val < 0) ? -x.int_val : x.int_val;
  }
  return result;
}

st_value_t st_builtin_min(st_value_t a, st_value_t b) {
  st_value_t result;
  result.int_val = (a.int_val < b.int_val) ? a.int_val : b.int_val;
  return result;
}

st_value_t st_builtin_max(st_value_t a, st_value_t b) {
  st_value_t result;
  result.int_val = (a.int_val > b.int_val) ? a.int_val : b.int_val;
  return result;
}

st_value_t st_builtin_sqrt(st_value_t x) {
  st_value_t result;
  // BUG-065: Check for negative input (sqrtf(negative) returns NaN)
  if (x.real_val < 0.0f) {
    result.real_val = 0.0f;  // Return 0 for negative input
  } else {
    result.real_val = (float)sqrtf((float)x.real_val);
  }
  return result;
}

st_value_t st_builtin_round(st_value_t x) {
  st_value_t result;
  // BUG-105: Clamp to INT16 range (-32768 to 32767)
  int32_t temp = (int32_t)roundf(x.real_val);
  if (temp > INT16_MAX) temp = INT16_MAX;
  if (temp < INT16_MIN) temp = INT16_MIN;
  result.int_val = (int16_t)temp;
  return result;
}

st_value_t st_builtin_trunc(st_value_t x) {
  st_value_t result;
  // BUG-105: Clamp to INT16 range (-32768 to 32767)
  int32_t temp = (int32_t)truncf(x.real_val);
  if (temp > INT16_MAX) temp = INT16_MAX;
  if (temp < INT16_MIN) temp = INT16_MIN;
  result.int_val = (int16_t)temp;
  return result;
}

st_value_t st_builtin_floor(st_value_t x) {
  st_value_t result;
  // BUG-105: Clamp to INT16 range (-32768 to 32767)
  int32_t temp = (int32_t)floorf(x.real_val);
  if (temp > INT16_MAX) temp = INT16_MAX;
  if (temp < INT16_MIN) temp = INT16_MIN;
  result.int_val = (int16_t)temp;
  return result;
}

st_value_t st_builtin_ceil(st_value_t x) {
  st_value_t result;
  // BUG-105: Clamp to INT16 range (-32768 to 32767)
  int32_t temp = (int32_t)ceilf(x.real_val);
  if (temp > INT16_MAX) temp = INT16_MAX;
  if (temp < INT16_MIN) temp = INT16_MIN;
  result.int_val = (int16_t)temp;
  return result;
}

/* ============================================================================
 * CLAMPING & SELECTION FUNCTIONS (v4.4+)
 * ============================================================================ */

st_value_t st_builtin_limit(st_value_t min_val, st_value_t value, st_value_t max_val) {
  st_value_t result;

  // Clamp value between min and max
  if (value.int_val < min_val.int_val) {
    result.int_val = min_val.int_val;
  } else if (value.int_val > max_val.int_val) {
    result.int_val = max_val.int_val;
  } else {
    result.int_val = value.int_val;
  }

  return result;
}

st_value_t st_builtin_sel(st_value_t g, st_value_t in0, st_value_t in1) {
  // SEL: Boolean selector
  // g=false → return in0
  // g=true  → return in1
  return (g.bool_val) ? in1 : in0;
}

/* ============================================================================
 * TRIGONOMETRIC FUNCTIONS (v4.4+)
 * ============================================================================ */

st_value_t st_builtin_sin(st_value_t x) {
  st_value_t result;
  result.real_val = sinf(x.real_val);  // Input in radians
  return result;
}

st_value_t st_builtin_cos(st_value_t x) {
  st_value_t result;
  result.real_val = cosf(x.real_val);  // Input in radians
  return result;
}

st_value_t st_builtin_tan(st_value_t x) {
  st_value_t result;
  result.real_val = tanf(x.real_val);  // Input in radians
  return result;
}

/* ============================================================================
 * TYPE CONVERSION FUNCTIONS
 * ============================================================================ */

st_value_t st_builtin_int_to_real(st_value_t i) {
  st_value_t result;
  result.real_val = (float)i.int_val;
  return result;
}

st_value_t st_builtin_real_to_int(st_value_t r) {
  st_value_t result;
  // BUG-105: Clamp to INT16 range (-32768 to 32767)
  int32_t temp = (int32_t)r.real_val;
  if (temp > INT16_MAX) temp = INT16_MAX;
  if (temp < INT16_MIN) temp = INT16_MIN;
  result.int_val = (int16_t)temp;
  return result;
}

st_value_t st_builtin_bool_to_int(st_value_t b) {
  st_value_t result;
  result.int_val = (b.bool_val != 0) ? 1 : 0;
  return result;
}

st_value_t st_builtin_int_to_bool(st_value_t i) {
  st_value_t result;
  result.bool_val = (i.int_val != 0);
  return result;
}

st_value_t st_builtin_dword_to_int(st_value_t d) {
  st_value_t result;
  // BUG-105: Clamp to INT16 range (-32768 to 32767)
  if (d.dword_val > (uint32_t)INT16_MAX) {
    result.int_val = INT16_MAX;  // 32767
  } else {
    result.int_val = (int16_t)d.dword_val;
  }
  return result;
}

st_value_t st_builtin_int_to_dword(st_value_t i) {
  st_value_t result;
  // Unsigned conversion: negative → wrap around
  result.dword_val = (uint32_t)i.int_val;
  return result;
}

/* ============================================================================
 * DISPATCHER FUNCTION
 * ============================================================================ */

st_value_t st_builtin_call(st_builtin_func_t func_id, st_value_t arg1, st_value_t arg2) {
  st_value_t result = {0};

  switch (func_id) {
    // Mathematical
    case ST_BUILTIN_ABS:
      result = st_builtin_abs(arg1);
      break;

    case ST_BUILTIN_MIN:
      result = st_builtin_min(arg1, arg2);
      break;

    case ST_BUILTIN_MAX:
      result = st_builtin_max(arg1, arg2);
      break;

    case ST_BUILTIN_SUM:
      // Alias for addition
      result.int_val = arg1.int_val + arg2.int_val;
      break;

    case ST_BUILTIN_SQRT:
      result = st_builtin_sqrt(arg1);
      break;

    case ST_BUILTIN_ROUND:
      result = st_builtin_round(arg1);
      break;

    case ST_BUILTIN_TRUNC:
      result = st_builtin_trunc(arg1);
      break;

    case ST_BUILTIN_FLOOR:
      result = st_builtin_floor(arg1);
      break;

    case ST_BUILTIN_CEIL:
      result = st_builtin_ceil(arg1);
      break;

    // Clamping & Selection (v4.4+)
    case ST_BUILTIN_LIMIT:
      // NOTE: 3-arg functions are handled directly in VM (st_vm.cpp)
      // This case should not be reached
      result.int_val = 0;
      break;

    case ST_BUILTIN_SEL:
      // NOTE: 3-arg functions are handled directly in VM (st_vm.cpp)
      // This case should not be reached
      result.int_val = 0;
      break;

    // Trigonometric (v4.4+)
    case ST_BUILTIN_SIN:
      result = st_builtin_sin(arg1);
      break;

    case ST_BUILTIN_COS:
      result = st_builtin_cos(arg1);
      break;

    case ST_BUILTIN_TAN:
      result = st_builtin_tan(arg1);
      break;

    // Type conversions
    case ST_BUILTIN_INT_TO_REAL:
      result = st_builtin_int_to_real(arg1);
      break;

    case ST_BUILTIN_REAL_TO_INT:
      result = st_builtin_real_to_int(arg1);
      break;

    case ST_BUILTIN_BOOL_TO_INT:
      result = st_builtin_bool_to_int(arg1);
      break;

    case ST_BUILTIN_INT_TO_BOOL:
      result = st_builtin_int_to_bool(arg1);
      break;

    case ST_BUILTIN_DWORD_TO_INT:
      result = st_builtin_dword_to_int(arg1);
      break;

    case ST_BUILTIN_INT_TO_DWORD:
      result = st_builtin_int_to_dword(arg1);
      break;

    // Persistence (v4.0+)
    case ST_BUILTIN_PERSIST_SAVE:
      result = st_builtin_persist_save(arg1);  // arg1 = group_id
      break;

    case ST_BUILTIN_PERSIST_LOAD:
      result = st_builtin_persist_load(arg1);  // arg1 = group_id
      break;

    // Modbus Master (v4.4+)
    case ST_BUILTIN_MB_READ_COIL:
      result = st_builtin_mb_read_coil(arg1, arg2);
      break;

    case ST_BUILTIN_MB_READ_INPUT:
      result = st_builtin_mb_read_input(arg1, arg2);
      break;

    case ST_BUILTIN_MB_READ_HOLDING:
      result = st_builtin_mb_read_holding(arg1, arg2);
      break;

    case ST_BUILTIN_MB_READ_INPUT_REG:
      result = st_builtin_mb_read_input_reg(arg1, arg2);
      break;

    case ST_BUILTIN_MB_WRITE_COIL:
      // 3-argument function - handled in VM
      result.int_val = 0;
      break;

    case ST_BUILTIN_MB_WRITE_HOLDING:
      // 3-argument function - handled in VM
      result.int_val = 0;
      break;

    default:
      // Unknown function - return zero
      break;
  }

  return result;
}

/* ============================================================================
 * METADATA FUNCTIONS
 * ============================================================================ */

const char *st_builtin_name(st_builtin_func_t func_id) {
  switch (func_id) {
    case ST_BUILTIN_ABS:           return "ABS";
    case ST_BUILTIN_MIN:           return "MIN";
    case ST_BUILTIN_MAX:           return "MAX";
    case ST_BUILTIN_SUM:           return "SUM";
    case ST_BUILTIN_SQRT:          return "SQRT";
    case ST_BUILTIN_ROUND:         return "ROUND";
    case ST_BUILTIN_TRUNC:         return "TRUNC";
    case ST_BUILTIN_FLOOR:         return "FLOOR";
    case ST_BUILTIN_CEIL:          return "CEIL";
    case ST_BUILTIN_LIMIT:         return "LIMIT";
    case ST_BUILTIN_SEL:           return "SEL";
    case ST_BUILTIN_SIN:           return "SIN";
    case ST_BUILTIN_COS:           return "COS";
    case ST_BUILTIN_TAN:           return "TAN";
    case ST_BUILTIN_INT_TO_REAL:   return "INT_TO_REAL";
    case ST_BUILTIN_REAL_TO_INT:   return "REAL_TO_INT";
    case ST_BUILTIN_BOOL_TO_INT:   return "BOOL_TO_INT";
    case ST_BUILTIN_INT_TO_BOOL:   return "INT_TO_BOOL";
    case ST_BUILTIN_DWORD_TO_INT:  return "DWORD_TO_INT";
    case ST_BUILTIN_INT_TO_DWORD:  return "INT_TO_DWORD";
    case ST_BUILTIN_PERSIST_SAVE:  return "SAVE";
    case ST_BUILTIN_PERSIST_LOAD:  return "LOAD";
    case ST_BUILTIN_MB_READ_COIL:      return "MB_READ_COIL";
    case ST_BUILTIN_MB_READ_INPUT:     return "MB_READ_INPUT";
    case ST_BUILTIN_MB_READ_HOLDING:   return "MB_READ_HOLDING";
    case ST_BUILTIN_MB_READ_INPUT_REG: return "MB_READ_INPUT_REG";
    case ST_BUILTIN_MB_WRITE_COIL:     return "MB_WRITE_COIL";
    case ST_BUILTIN_MB_WRITE_HOLDING:  return "MB_WRITE_HOLDING";
    default:                       return "UNKNOWN";
  }
}

uint8_t st_builtin_arg_count(st_builtin_func_t func_id) {
  switch (func_id) {
    // 1-argument functions
    case ST_BUILTIN_ABS:
    case ST_BUILTIN_SQRT:
    case ST_BUILTIN_ROUND:
    case ST_BUILTIN_TRUNC:
    case ST_BUILTIN_FLOOR:
    case ST_BUILTIN_CEIL:
    case ST_BUILTIN_INT_TO_REAL:
    case ST_BUILTIN_REAL_TO_INT:
    case ST_BUILTIN_BOOL_TO_INT:
    case ST_BUILTIN_INT_TO_BOOL:
    case ST_BUILTIN_DWORD_TO_INT:
    case ST_BUILTIN_INT_TO_DWORD:
    case ST_BUILTIN_SIN:
    case ST_BUILTIN_COS:
    case ST_BUILTIN_TAN:
      return 1;

    // 2-argument functions
    case ST_BUILTIN_MIN:
    case ST_BUILTIN_MAX:
    case ST_BUILTIN_SUM:
    case ST_BUILTIN_MB_READ_COIL:      // MB_READ_COIL(slave_id, address)
    case ST_BUILTIN_MB_READ_INPUT:     // MB_READ_INPUT(slave_id, address)
    case ST_BUILTIN_MB_READ_HOLDING:   // MB_READ_HOLDING(slave_id, address)
    case ST_BUILTIN_MB_READ_INPUT_REG: // MB_READ_INPUT_REG(slave_id, address)
      return 2;

    // 3-argument functions (v4.4+)
    case ST_BUILTIN_LIMIT:  // LIMIT(min, val, max)
    case ST_BUILTIN_SEL:     // SEL(g, in0, in1)
    case ST_BUILTIN_MB_WRITE_COIL:     // MB_WRITE_COIL(slave_id, address, value)
    case ST_BUILTIN_MB_WRITE_HOLDING:  // MB_WRITE_HOLDING(slave_id, address, value)
      return 3;

    // 1-argument functions (v4.0+)
    case ST_BUILTIN_PERSIST_SAVE:
    case ST_BUILTIN_PERSIST_LOAD:
      return 1;  // arg: group_id (0=all, 1-8=specific)

    default:
      return 0;
  }
}

st_datatype_t st_builtin_return_type(st_builtin_func_t func_id) {
  switch (func_id) {
    // Returns REAL
    case ST_BUILTIN_SQRT:
    case ST_BUILTIN_SIN:
    case ST_BUILTIN_COS:
    case ST_BUILTIN_TAN:
    case ST_BUILTIN_INT_TO_REAL:
      return ST_TYPE_REAL;

    // Returns BOOL
    case ST_BUILTIN_INT_TO_BOOL:
    case ST_BUILTIN_MB_READ_COIL:      // MB_READ_COIL → BOOL
    case ST_BUILTIN_MB_READ_INPUT:     // MB_READ_INPUT → BOOL
    case ST_BUILTIN_MB_WRITE_COIL:     // MB_WRITE_COIL → BOOL (success flag)
    case ST_BUILTIN_MB_WRITE_HOLDING:  // MB_WRITE_HOLDING → BOOL (success flag)
      return ST_TYPE_BOOL;

    // Returns DWORD
    case ST_BUILTIN_INT_TO_DWORD:
      return ST_TYPE_DWORD;

    // Returns INT (most functions)
    case ST_BUILTIN_ABS:
    case ST_BUILTIN_MIN:
    case ST_BUILTIN_MAX:
    case ST_BUILTIN_SUM:
    case ST_BUILTIN_ROUND:
    case ST_BUILTIN_TRUNC:
    case ST_BUILTIN_FLOOR:
    case ST_BUILTIN_CEIL:
    case ST_BUILTIN_LIMIT:
    case ST_BUILTIN_SEL:
    case ST_BUILTIN_REAL_TO_INT:
    case ST_BUILTIN_BOOL_TO_INT:
    case ST_BUILTIN_DWORD_TO_INT:
    case ST_BUILTIN_PERSIST_SAVE:
    case ST_BUILTIN_PERSIST_LOAD:
    case ST_BUILTIN_MB_READ_HOLDING:   // MB_READ_HOLDING → INT
    case ST_BUILTIN_MB_READ_INPUT_REG: // MB_READ_INPUT_REG → INT
    default:
      return ST_TYPE_INT;
  }
}
