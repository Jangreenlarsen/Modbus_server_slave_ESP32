/**
 * @file st_compiler.cpp
 * @brief Structured Text Bytecode Compiler Implementation
 *
 * Converts AST (from parser) to bytecode (stack-based VM instructions).
 * Single-pass with symbol table and jump backpatching.
 */

#include "st_compiler.h"
#include "st_builtins.h"
#include "st_stateful.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * COMPILER INITIALIZATION
 * ============================================================================ */

void st_compiler_init(st_compiler_t *compiler) {
  memset(compiler, 0, sizeof(*compiler));
  compiler->symbol_table.count = 0;
  compiler->bytecode_ptr = 0;
  compiler->loop_depth = 0;
  compiler->patch_count = 0;
  compiler->exit_patch_total = 0;
  memset(compiler->exit_patch_count, 0, sizeof(compiler->exit_patch_count));

  // v4.7+: Initialize instance counters for stateful functions
  compiler->edge_instance_count = 0;
  compiler->timer_instance_count = 0;
  compiler->counter_instance_count = 0;
  compiler->latch_instance_count = 0;  // v4.7.3: SR/RS latches
  compiler->hysteresis_instance_count = 0;  // v4.8: Signal processing
  compiler->blink_instance_count = 0;
  compiler->filter_instance_count = 0;
}

/* ============================================================================
 * SYMBOL TABLE MANAGEMENT
 * ============================================================================ */

uint8_t st_compiler_add_symbol(st_compiler_t *compiler, const char *name,
                                st_datatype_t type, uint8_t is_input, uint8_t is_output) {
  if (compiler->symbol_table.count >= 32) {
    st_compiler_error(compiler, "Too many variables (max 32)");
    return 0xFF;
  }

  // Check for duplicate
  for (int i = 0; i < compiler->symbol_table.count; i++) {
    if (strcmp(compiler->symbol_table.symbols[i].name, name) == 0) {
      st_compiler_error(compiler, "Duplicate variable name");
      return 0xFF;
    }
  }

  st_symbol_t *sym = &compiler->symbol_table.symbols[compiler->symbol_table.count];
  strncpy(sym->name, name, sizeof(sym->name) - 1);
  sym->name[sizeof(sym->name) - 1] = '\0';
  sym->type = type;
  sym->is_input = is_input;
  sym->is_output = is_output;
  sym->index = compiler->symbol_table.count;

  debug_printf("[COMPILER] Added symbol[%d]: name='%s' type=%d input=%d output=%d\n",
               sym->index, sym->name, sym->type, sym->is_input, sym->is_output);

  return compiler->symbol_table.count++;
}

uint8_t st_compiler_lookup_symbol(st_compiler_t *compiler, const char *name) {
  for (int i = 0; i < compiler->symbol_table.count; i++) {
    if (strcmp(compiler->symbol_table.symbols[i].name, name) == 0) {
      return compiler->symbol_table.symbols[i].index;
    }
  }
  return 0xFF;
}

/* ============================================================================
 * BYTECODE EMISSION
 * ============================================================================ */

static bool st_compiler_ensure_space(st_compiler_t *compiler, int count) {
  if (compiler->bytecode_ptr + count >= 1024) {
    st_compiler_error(compiler, "Bytecode buffer overflow (max 1024 instructions)");
    return false;
  }
  return true;
}

bool st_compiler_emit(st_compiler_t *compiler, st_opcode_t opcode) {
  if (!st_compiler_ensure_space(compiler, 1)) return false;

  st_bytecode_instr_t *instr = &compiler->bytecode[compiler->bytecode_ptr++];
  instr->opcode = opcode;
  memset(&instr->arg, 0, sizeof(instr->arg));
  return true;
}

bool st_compiler_emit_int(st_compiler_t *compiler, st_opcode_t opcode, int32_t arg) {
  if (!st_compiler_ensure_space(compiler, 1)) return false;

  st_bytecode_instr_t *instr = &compiler->bytecode[compiler->bytecode_ptr++];
  instr->opcode = opcode;
  instr->arg.int_arg = arg;
  return true;
}

bool st_compiler_emit_var(st_compiler_t *compiler, st_opcode_t opcode, uint8_t var_index) {
  if (!st_compiler_ensure_space(compiler, 1)) return false;

  st_bytecode_instr_t *instr = &compiler->bytecode[compiler->bytecode_ptr++];
  instr->opcode = opcode;
  instr->arg.var_index = var_index;
  return true;
}

bool st_compiler_emit_builtin_call(st_compiler_t *compiler, int32_t func_id, uint8_t instance_id) {
  if (!st_compiler_ensure_space(compiler, 1)) return false;

  st_bytecode_instr_t *instr = &compiler->bytecode[compiler->bytecode_ptr++];
  instr->opcode = ST_OP_CALL_BUILTIN;
  instr->arg.builtin_call.func_id_low = (uint8_t)(func_id & 0xFF);  // Only lower byte (max 256 functions)
  instr->arg.builtin_call.instance_id = instance_id;
  instr->arg.builtin_call.padding = 0;  // Explicit zero padding
  return true;
}

uint16_t st_compiler_current_addr(st_compiler_t *compiler) {
  return compiler->bytecode_ptr;
}

uint16_t st_compiler_emit_jump(st_compiler_t *compiler, st_opcode_t opcode) {
  uint16_t addr = st_compiler_current_addr(compiler);
  st_compiler_emit_int(compiler, opcode, 0);  // Placeholder address
  return addr;
}

void st_compiler_patch_jump(st_compiler_t *compiler, uint16_t jump_addr, uint16_t target_addr) {
  // BUG-037 FIX: Changed from 512 to 1024 (matches bytecode array size in st_compiler.h)
  if (jump_addr >= 1024) {
    // BUG-074: Log error instead of silent fail
    st_compiler_error(compiler, "Jump patch address out of bounds");
    return;
  }

  // BUG-120: Detect self-loop (jump to same address)
  if (jump_addr == target_addr) {
    snprintf(compiler->error_msg, sizeof(compiler->error_msg),
             "Compiler bug: self-loop detected at address %u", jump_addr);
    compiler->error_count++;
    return;
  }

  compiler->bytecode[jump_addr].arg.int_arg = target_addr;
}

void st_compiler_error(st_compiler_t *compiler, const char *msg) {
  snprintf(compiler->error_msg, sizeof(compiler->error_msg),
           "Compile error: %s", msg);
  compiler->error_count++;
}

/* ============================================================================
 * EXPRESSION COMPILATION
 * ============================================================================ */

/* Forward declaration */
bool st_compiler_compile_expr(st_compiler_t *compiler, st_ast_node_t *node);

static bool st_compiler_compile_binary_op(st_compiler_t *compiler, st_ast_node_t *node) {
  // Compile left operand
  if (!st_compiler_compile_expr(compiler, node->data.binary_op.left)) {
    return false;
  }

  // Compile right operand
  if (!st_compiler_compile_expr(compiler, node->data.binary_op.right)) {
    return false;
  }

  // Emit operator instruction
  st_opcode_t opcode;
  switch (node->data.binary_op.op) {
    case ST_TOK_PLUS:     opcode = ST_OP_ADD; break;
    case ST_TOK_MINUS:    opcode = ST_OP_SUB; break;
    case ST_TOK_MUL:      opcode = ST_OP_MUL; break;
    case ST_TOK_DIV:      opcode = ST_OP_DIV; break;
    case ST_TOK_MOD:      opcode = ST_OP_MOD; break;
    case ST_TOK_AND:      opcode = ST_OP_AND; break;
    case ST_TOK_OR:       opcode = ST_OP_OR; break;
    case ST_TOK_EQ:       opcode = ST_OP_EQ; break;
    case ST_TOK_NE:       opcode = ST_OP_NE; break;
    case ST_TOK_LT:       opcode = ST_OP_LT; break;
    case ST_TOK_GT:       opcode = ST_OP_GT; break;
    case ST_TOK_LE:       opcode = ST_OP_LE; break;
    case ST_TOK_GE:       opcode = ST_OP_GE; break;
    case ST_TOK_SHL:      opcode = ST_OP_SHL; break;
    case ST_TOK_SHR:      opcode = ST_OP_SHR; break;
    case ST_TOK_XOR:      opcode = ST_OP_XOR; break;
    default:
      st_compiler_error(compiler, "Unknown binary operator");
      return false;
  }

  return st_compiler_emit(compiler, opcode);
}

static bool st_compiler_compile_unary_op(st_compiler_t *compiler, st_ast_node_t *node) {
  // Compile operand
  if (!st_compiler_compile_expr(compiler, node->data.unary_op.operand)) {
    return false;
  }

  // Emit operator instruction
  st_opcode_t opcode;
  switch (node->data.unary_op.op) {
    case ST_TOK_MINUS:    opcode = ST_OP_NEG; break;
    case ST_TOK_NOT:      opcode = ST_OP_NOT; break;
    default:
      st_compiler_error(compiler, "Unknown unary operator");
      return false;
  }

  return st_compiler_emit(compiler, opcode);
}

bool st_compiler_compile_expr(st_compiler_t *compiler, st_ast_node_t *node) {
  if (!node) {
    st_compiler_error(compiler, "NULL expression node");
    return false;
  }

  switch (node->type) {
    case ST_AST_LITERAL: {
      switch (node->data.literal.type) {
        case ST_TYPE_BOOL:
          return st_compiler_emit_int(compiler, ST_OP_PUSH_BOOL, node->data.literal.value.bool_val);
        case ST_TYPE_INT:
          return st_compiler_emit_int(compiler, ST_OP_PUSH_INT, node->data.literal.value.int_val);
        case ST_TYPE_DWORD:
          return st_compiler_emit_int(compiler, ST_OP_PUSH_DWORD, (int32_t)node->data.literal.value.dword_val);
        case ST_TYPE_REAL: {
          // Store float as bits in int_arg (hack, but works for 32-bit)
          int32_t bits;
          memcpy(&bits, &node->data.literal.value.real_val, sizeof(float));
          return st_compiler_emit_int(compiler, ST_OP_PUSH_REAL, bits);
        }
        default:
          st_compiler_error(compiler, "Unknown literal type");
          return false;
      }
    }

    case ST_AST_VARIABLE: {
      uint8_t var_index = st_compiler_lookup_symbol(compiler, node->data.variable.var_name);
      if (var_index == 0xFF) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Unknown variable: %s", node->data.variable.var_name);
        st_compiler_error(compiler, msg);
        return false;
      }
      return st_compiler_emit_var(compiler, ST_OP_LOAD_VAR, var_index);
    }

    case ST_AST_BINARY_OP:
      return st_compiler_compile_binary_op(compiler, node);

    case ST_AST_UNARY_OP:
      return st_compiler_compile_unary_op(compiler, node);

    case ST_AST_FUNCTION_CALL: {
      // Map function name to built-in ID
      st_builtin_func_t func_id;
      if (strcasecmp(node->data.function_call.func_name, "ABS") == 0) func_id = ST_BUILTIN_ABS;
      else if (strcasecmp(node->data.function_call.func_name, "MIN") == 0) func_id = ST_BUILTIN_MIN;
      else if (strcasecmp(node->data.function_call.func_name, "MAX") == 0) func_id = ST_BUILTIN_MAX;
      else if (strcasecmp(node->data.function_call.func_name, "SUM") == 0) func_id = ST_BUILTIN_SUM;
      else if (strcasecmp(node->data.function_call.func_name, "SQRT") == 0) func_id = ST_BUILTIN_SQRT;
      else if (strcasecmp(node->data.function_call.func_name, "ROUND") == 0) func_id = ST_BUILTIN_ROUND;
      else if (strcasecmp(node->data.function_call.func_name, "TRUNC") == 0) func_id = ST_BUILTIN_TRUNC;
      else if (strcasecmp(node->data.function_call.func_name, "FLOOR") == 0) func_id = ST_BUILTIN_FLOOR;
      else if (strcasecmp(node->data.function_call.func_name, "CEIL") == 0) func_id = ST_BUILTIN_CEIL;
      else if (strcasecmp(node->data.function_call.func_name, "LIMIT") == 0) func_id = ST_BUILTIN_LIMIT;
      else if (strcasecmp(node->data.function_call.func_name, "SEL") == 0) func_id = ST_BUILTIN_SEL;
      else if (strcasecmp(node->data.function_call.func_name, "SIN") == 0) func_id = ST_BUILTIN_SIN;
      else if (strcasecmp(node->data.function_call.func_name, "COS") == 0) func_id = ST_BUILTIN_COS;
      else if (strcasecmp(node->data.function_call.func_name, "TAN") == 0) func_id = ST_BUILTIN_TAN;
      else if (strcasecmp(node->data.function_call.func_name, "EXP") == 0) func_id = ST_BUILTIN_EXP;
      else if (strcasecmp(node->data.function_call.func_name, "LN") == 0) func_id = ST_BUILTIN_LN;
      else if (strcasecmp(node->data.function_call.func_name, "LOG") == 0) func_id = ST_BUILTIN_LOG;
      else if (strcasecmp(node->data.function_call.func_name, "POW") == 0) func_id = ST_BUILTIN_POW;
      else if (strcasecmp(node->data.function_call.func_name, "INT_TO_REAL") == 0) func_id = ST_BUILTIN_INT_TO_REAL;
      else if (strcasecmp(node->data.function_call.func_name, "REAL_TO_INT") == 0) func_id = ST_BUILTIN_REAL_TO_INT;
      else if (strcasecmp(node->data.function_call.func_name, "BOOL_TO_INT") == 0) func_id = ST_BUILTIN_BOOL_TO_INT;
      else if (strcasecmp(node->data.function_call.func_name, "INT_TO_BOOL") == 0) func_id = ST_BUILTIN_INT_TO_BOOL;
      else if (strcasecmp(node->data.function_call.func_name, "DWORD_TO_INT") == 0) func_id = ST_BUILTIN_DWORD_TO_INT;
      else if (strcasecmp(node->data.function_call.func_name, "INT_TO_DWORD") == 0) func_id = ST_BUILTIN_INT_TO_DWORD;
      else if (strcasecmp(node->data.function_call.func_name, "SAVE") == 0) func_id = ST_BUILTIN_PERSIST_SAVE;
      else if (strcasecmp(node->data.function_call.func_name, "LOAD") == 0) func_id = ST_BUILTIN_PERSIST_LOAD;
      // BUG-116 FIX: Modbus Master functions (v4.4+)
      else if (strcasecmp(node->data.function_call.func_name, "MB_READ_COIL") == 0) func_id = ST_BUILTIN_MB_READ_COIL;
      else if (strcasecmp(node->data.function_call.func_name, "MB_READ_INPUT") == 0) func_id = ST_BUILTIN_MB_READ_INPUT;
      else if (strcasecmp(node->data.function_call.func_name, "MB_READ_HOLDING") == 0) func_id = ST_BUILTIN_MB_READ_HOLDING;
      else if (strcasecmp(node->data.function_call.func_name, "MB_READ_INPUT_REG") == 0) func_id = ST_BUILTIN_MB_READ_INPUT_REG;
      else if (strcasecmp(node->data.function_call.func_name, "MB_WRITE_COIL") == 0) func_id = ST_BUILTIN_MB_WRITE_COIL;
      else if (strcasecmp(node->data.function_call.func_name, "MB_WRITE_HOLDING") == 0) func_id = ST_BUILTIN_MB_WRITE_HOLDING;
      // Stateful functions (v4.7+)
      else if (strcasecmp(node->data.function_call.func_name, "R_TRIG") == 0) func_id = ST_BUILTIN_R_TRIG;
      else if (strcasecmp(node->data.function_call.func_name, "F_TRIG") == 0) func_id = ST_BUILTIN_F_TRIG;
      else if (strcasecmp(node->data.function_call.func_name, "TON") == 0) func_id = ST_BUILTIN_TON;
      else if (strcasecmp(node->data.function_call.func_name, "TOF") == 0) func_id = ST_BUILTIN_TOF;
      else if (strcasecmp(node->data.function_call.func_name, "TP") == 0) func_id = ST_BUILTIN_TP;
      else if (strcasecmp(node->data.function_call.func_name, "CTU") == 0) func_id = ST_BUILTIN_CTU;
      else if (strcasecmp(node->data.function_call.func_name, "CTD") == 0) func_id = ST_BUILTIN_CTD;
      else if (strcasecmp(node->data.function_call.func_name, "CTUD") == 0) func_id = ST_BUILTIN_CTUD;
      // v4.7.3: Bistable latches
      else if (strcasecmp(node->data.function_call.func_name, "SR") == 0) func_id = ST_BUILTIN_SR;
      else if (strcasecmp(node->data.function_call.func_name, "RS") == 0) func_id = ST_BUILTIN_RS;
      // v4.8: Signal processing
      else if (strcasecmp(node->data.function_call.func_name, "SCALE") == 0) func_id = ST_BUILTIN_SCALE;
      else if (strcasecmp(node->data.function_call.func_name, "HYSTERESIS") == 0) func_id = ST_BUILTIN_HYSTERESIS;
      else if (strcasecmp(node->data.function_call.func_name, "BLINK") == 0) func_id = ST_BUILTIN_BLINK;
      else if (strcasecmp(node->data.function_call.func_name, "FILTER") == 0) func_id = ST_BUILTIN_FILTER;
      else {
        char msg[128];
        snprintf(msg, sizeof(msg), "Unknown function: %s", node->data.function_call.func_name);
        st_compiler_error(compiler, msg);
        return false;
      }

      // Compile arguments (push onto stack)
      for (uint8_t i = 0; i < node->data.function_call.arg_count; i++) {
        if (!st_compiler_compile_expr(compiler, node->data.function_call.args[i])) {
          return false;
        }
      }

      // v4.7+: Allocate instance ID for stateful functions
      uint8_t instance_id = 0;

      // Edge detection functions
      if (func_id == ST_BUILTIN_R_TRIG || func_id == ST_BUILTIN_F_TRIG) {
        if (compiler->edge_instance_count >= 8) {
          st_compiler_error(compiler, "Too many edge detector instances (max 8)");
          return false;
        }
        instance_id = compiler->edge_instance_count++;
        debug_printf("[COMPILER] Allocated edge instance %d for %s\n",
                     instance_id, node->data.function_call.func_name);
      }
      // Timer functions
      else if (func_id == ST_BUILTIN_TON || func_id == ST_BUILTIN_TOF || func_id == ST_BUILTIN_TP) {
        if (compiler->timer_instance_count >= 8) {
          st_compiler_error(compiler, "Too many timer instances (max 8)");
          return false;
        }
        instance_id = compiler->timer_instance_count++;
        debug_printf("[COMPILER] Allocated timer instance %d for %s\n",
                     instance_id, node->data.function_call.func_name);
      }
      // Counter functions
      else if (func_id == ST_BUILTIN_CTU || func_id == ST_BUILTIN_CTD || func_id == ST_BUILTIN_CTUD) {
        if (compiler->counter_instance_count >= 8) {
          st_compiler_error(compiler, "Too many counter instances (max 8)");
          return false;
        }
        instance_id = compiler->counter_instance_count++;
        debug_printf("[COMPILER] Allocated counter instance %d for %s\n",
                     instance_id, node->data.function_call.func_name);
      }
      // Latch functions (v4.7.3)
      else if (func_id == ST_BUILTIN_SR || func_id == ST_BUILTIN_RS) {
        if (compiler->latch_instance_count >= 8) {
          st_compiler_error(compiler, "Too many latch instances (max 8)");
          return false;
        }
        instance_id = compiler->latch_instance_count++;
        debug_printf("[COMPILER] Allocated latch instance %d for %s\n",
                     instance_id, node->data.function_call.func_name);
      }
      // Signal processing functions (v4.8)
      else if (func_id == ST_BUILTIN_HYSTERESIS) {
        if (compiler->hysteresis_instance_count >= 8) {
          st_compiler_error(compiler, "Too many hysteresis instances (max 8)");
          return false;
        }
        instance_id = compiler->hysteresis_instance_count++;
        debug_printf("[COMPILER] Allocated hysteresis instance %d for %s\n",
                     instance_id, node->data.function_call.func_name);
      }
      else if (func_id == ST_BUILTIN_BLINK) {
        if (compiler->blink_instance_count >= 8) {
          st_compiler_error(compiler, "Too many blink instances (max 8)");
          return false;
        }
        instance_id = compiler->blink_instance_count++;
        debug_printf("[COMPILER] Allocated blink instance %d for %s\n",
                     instance_id, node->data.function_call.func_name);
      }
      else if (func_id == ST_BUILTIN_FILTER) {
        if (compiler->filter_instance_count >= 8) {
          st_compiler_error(compiler, "Too many filter instances (max 8)");
          return false;
        }
        instance_id = compiler->filter_instance_count++;
        debug_printf("[COMPILER] Allocated filter instance %d for %s\n",
                     instance_id, node->data.function_call.func_name);
      }
      // Stateless functions (SCALE) use instance_id = 0

      // Emit CALL_BUILTIN instruction with instance ID
      return st_compiler_emit_builtin_call(compiler, (int32_t)func_id, instance_id);
    }

    default:
      st_compiler_error(compiler, "Expression node type not supported");
      return false;
  }
}

/* ============================================================================
 * STATEMENT COMPILATION
 * ============================================================================ */

static bool st_compiler_compile_assignment(st_compiler_t *compiler, st_ast_node_t *node) {
  // Compile RHS expression (result on stack)
  if (!st_compiler_compile_expr(compiler, node->data.assignment.expr)) {
    return false;
  }

  // Look up variable
  uint8_t var_index = st_compiler_lookup_symbol(compiler, node->data.assignment.var_name);
  if (var_index == 0xFF) {
    char msg[128];
    snprintf(msg, sizeof(msg), "Unknown variable: %s", node->data.assignment.var_name);
    st_compiler_error(compiler, msg);
    return false;
  }

  // Emit STORE instruction
  return st_compiler_emit_var(compiler, ST_OP_STORE_VAR, var_index);
}

/* v4.6.0: Compile remote write: MB_WRITE_XXX(id, addr) := value */
static bool st_compiler_compile_remote_write(st_compiler_t *compiler, st_ast_node_t *node) {
  // Compile slave_id expression onto stack
  if (!st_compiler_compile_expr(compiler, node->data.remote_write.slave_id)) {
    return false;
  }

  // Compile address expression onto stack
  if (!st_compiler_compile_expr(compiler, node->data.remote_write.address)) {
    return false;
  }

  // Compile value expression onto stack
  if (!st_compiler_compile_expr(compiler, node->data.remote_write.value)) {
    return false;
  }

  // Emit OP_CALL_BUILTIN with function ID (VM knows arg count from builtin table)
  if (!st_compiler_emit_int(compiler, ST_OP_CALL_BUILTIN, (int32_t)node->data.remote_write.func_id)) {
    return false;
  }

  // Pop result from stack (we don't use it in statement context)
  // The result is the success BOOL, but user checks mb_success global instead
  return st_compiler_emit(compiler, ST_OP_POP);
}

static bool st_compiler_compile_case(st_compiler_t *compiler, st_ast_node_t *node) {
  // Compile the expression being tested
  if (!st_compiler_compile_expr(compiler, node->data.case_stmt.expr)) {
    return false;
  }

  // Store expression result temporarily in a temp register
  // (Stack: expr_result on top)

  uint16_t *jump_end = (uint16_t *)malloc(sizeof(uint16_t) * (node->data.case_stmt.branch_count + 1));
  if (!jump_end) {
    st_compiler_error(compiler, "Memory allocation failed for CASE jumps");
    return false;
  }
  uint8_t jump_count = 0;

  debug_printf("[CASE] Compiling CASE with %d branches\n", node->data.case_stmt.branch_count);

  // Compile each case branch
  for (uint8_t i = 0; i < node->data.case_stmt.branch_count; i++) {
    st_case_branch_t *branch = &node->data.case_stmt.branches[i];

    debug_printf("[CASE] Branch %d (value=%d) at PC %d\n", i, branch->value, compiler->bytecode_ptr);

    // Duplicate the expression value on stack for comparison
    if (!st_compiler_emit(compiler, ST_OP_DUP)) {
      free(jump_end);
      return false;
    }

    // Push case value and compare
    if (!st_compiler_emit_int(compiler, ST_OP_PUSH_INT, branch->value)) {
      free(jump_end);
      return false;
    }

    if (!st_compiler_emit(compiler, ST_OP_EQ)) {
      free(jump_end);
      return false;
    }

    // Jump to next case if not equal
    // JMP_IF_FALSE will pop the comparison result
    uint16_t jump_next = st_compiler_emit_jump(compiler, ST_OP_JMP_IF_FALSE);
    debug_printf("[CASE]   JMP_IF_FALSE at PC %d\n", jump_next);

    // We matched this case - pop the duplicate expression value before executing branch
    if (!st_compiler_emit(compiler, ST_OP_POP)) {
      free(jump_end);
      return false;
    }

    // Compile branch body
    if (branch->body) {
      debug_printf("[CASE]   Compiling branch body at PC %d\n", compiler->bytecode_ptr);
      if (!st_compiler_compile_node(compiler, branch->body)) {
        free(jump_end);
        return false;
      }
    }

    // Jump to end of CASE
    uint16_t jump_addr = st_compiler_emit_jump(compiler, ST_OP_JMP);
    jump_end[jump_count++] = jump_addr;
    debug_printf("[CASE]   JMP to end at PC %d (target will be patched)\n", jump_addr);

    // Patch next case jump (JMP_IF_FALSE) from THIS iteration to point to NEXT CASE or END
    // This must happen AFTER we emit the JMP so JMP_IF_FALSE skips the case body
    uint16_t patch_target = st_compiler_current_addr(compiler);
    st_compiler_patch_jump(compiler, jump_next, patch_target);
    debug_printf("[CASE]   Patched JMP_IF_FALSE[%d] to PC %d\n", jump_next, patch_target);
  }

  // Pop the expression value
  if (!st_compiler_emit(compiler, ST_OP_POP)) {
    free(jump_end);
    return false;
  }

  // Compile ELSE block if present
  if (node->data.case_stmt.else_body) {
    debug_printf("[CASE] Compiling ELSE block at PC %d\n", compiler->bytecode_ptr);
    if (!st_compiler_compile_node(compiler, node->data.case_stmt.else_body)) {
      free(jump_end);
      return false;
    }
  }

  // Patch all end jumps
  uint16_t end_addr = st_compiler_current_addr(compiler);
  debug_printf("[CASE] Patching %d JMP instructions to end at PC %d\n", jump_count, end_addr);
  for (uint8_t i = 0; i < jump_count; i++) {
    debug_printf("[CASE]   Patching JMP[%d] to PC %d\n", jump_end[i], end_addr);
    st_compiler_patch_jump(compiler, jump_end[i], end_addr);
  }

  free(jump_end);
  debug_printf("[CASE] CASE compilation complete at PC %d\n", compiler->bytecode_ptr);
  return true;
}

static bool st_compiler_compile_if(st_compiler_t *compiler, st_ast_node_t *node) {
  debug_printf("[IF] Starting IF compilation at PC %d\n", compiler->bytecode_ptr);

  // Compile condition (result on stack)
  if (!st_compiler_compile_expr(compiler, node->data.if_stmt.condition_expr)) {
    return false;
  }

  // Emit JMP_IF_FALSE (skip THEN block if condition false)
  uint16_t jump_then = st_compiler_emit_jump(compiler, ST_OP_JMP_IF_FALSE);
  debug_printf("[IF] Emitted JMP_IF_FALSE at PC %d (placeholder)\n", jump_then);

  // Compile THEN block
  if (node->data.if_stmt.then_body) {
    debug_printf("[IF] Compiling THEN block at PC %d\n", compiler->bytecode_ptr);
    if (!st_compiler_compile_node(compiler, node->data.if_stmt.then_body)) {
      return false;
    }
  }

  // If ELSE block exists, emit JMP to skip it
  uint16_t jump_else = 0;
  if (node->data.if_stmt.else_body) {
    jump_else = st_compiler_emit_jump(compiler, ST_OP_JMP);
    debug_printf("[IF] Emitted JMP (skip ELSE) at PC %d (placeholder)\n", jump_else);
  }

  // Patch THEN jump to here
  uint16_t patch_addr = st_compiler_current_addr(compiler);
  st_compiler_patch_jump(compiler, jump_then, patch_addr);
  debug_printf("[IF] Patching JMP_IF_FALSE[%d] to PC %d\n", jump_then, patch_addr);

  // Compile ELSE block if present
  if (node->data.if_stmt.else_body) {
    debug_printf("[IF] Compiling ELSE block at PC %d\n", compiler->bytecode_ptr);
    if (!st_compiler_compile_node(compiler, node->data.if_stmt.else_body)) {
      return false;
    }
    // Patch ELSE jump to here
    uint16_t patch_addr_else = st_compiler_current_addr(compiler);
    st_compiler_patch_jump(compiler, jump_else, patch_addr_else);
    debug_printf("[IF] Patching JMP[%d] to PC %d\n", jump_else, patch_addr_else);
  }

  debug_printf("[IF] IF compilation complete at PC %d\n", compiler->bytecode_ptr);
  return true;
}

static bool st_compiler_compile_for(st_compiler_t *compiler, st_ast_node_t *node) {
  // Look up loop variable
  uint8_t var_index = st_compiler_lookup_symbol(compiler, node->data.for_stmt.var_name);
  if (var_index == 0xFF) {
    char msg[128];
    snprintf(msg, sizeof(msg), "Unknown loop variable: %s", node->data.for_stmt.var_name);
    st_compiler_error(compiler, msg);
    return false;
  }

  // Enter loop (for EXIT statement tracking)
  if (compiler->loop_depth >= 8) {
    st_compiler_error(compiler, "Loop nesting too deep (max 8)");
    return false;
  }
  uint8_t exit_count_before = compiler->exit_patch_total;
  compiler->loop_depth++;

  // Compile start expression
  if (!st_compiler_compile_expr(compiler, node->data.for_stmt.start)) {
    compiler->loop_depth--;
    return false;
  }

  // Store to loop variable
  if (!st_compiler_emit_var(compiler, ST_OP_STORE_VAR, var_index)) {
    compiler->loop_depth--;
    return false;
  }

  // Compile end expression and keep on stack
  if (!st_compiler_compile_expr(compiler, node->data.for_stmt.end)) {
    compiler->loop_depth--;
    return false;
  }
  // Stack: [end_value]

  // Loop start address
  uint16_t loop_start = st_compiler_current_addr(compiler);

  // Duplicate end value for comparison (keep original on stack for next iteration)
  if (!st_compiler_emit(compiler, ST_OP_DUP)) {
    return false;
  }
  // Stack: [end_value, end_value_dup]

  // Load loop variable
  if (!st_compiler_emit_var(compiler, ST_OP_LOAD_VAR, var_index)) {
    return false;
  }
  // Stack: [end_value, end_value_dup, var]

  // Compare: var > end (exit condition for TO loops)
  // Stack: [end_dup, var]
  // LT pops: right=var, left=end_dup → Result: end_dup < var (which is var > end_dup)
  if (!st_compiler_emit(compiler, ST_OP_LT)) {
    return false;
  }
  // Stack: [end_value, (var > end)]

  // If var > end, exit loop
  uint16_t jump_exit = st_compiler_emit_jump(compiler, ST_OP_JMP_IF_TRUE);
  // Stack: [end_value]

  // Compile loop body
  if (node->data.for_stmt.body) {
    if (!st_compiler_compile_node(compiler, node->data.for_stmt.body)) {
      return false;
    }
  }

  // Increment loop variable (BY step or default 1)
  if (!st_compiler_emit_var(compiler, ST_OP_LOAD_VAR, var_index)) {
    return false;
  }

  // Push step value (default 1 if no BY clause)
  if (node->data.for_stmt.step) {
    if (!st_compiler_compile_expr(compiler, node->data.for_stmt.step)) {
      compiler->loop_depth--;
      return false;
    }
  } else {
    if (!st_compiler_emit_int(compiler, ST_OP_PUSH_INT, 1)) {
      return false;
    }
  }

  if (!st_compiler_emit(compiler, ST_OP_ADD)) {
    return false;
  }
  if (!st_compiler_emit_var(compiler, ST_OP_STORE_VAR, var_index)) {
    return false;
  }

  // Jump back to loop start
  if (!st_compiler_emit_int(compiler, ST_OP_JMP, loop_start)) {
    return false;
  }

  // Loop exit - backpatch exit jump and pop end value
  uint16_t loop_exit_addr = st_compiler_current_addr(compiler);
  st_compiler_patch_jump(compiler, jump_exit, loop_exit_addr);

  // Backpatch all EXIT statements in this loop
  uint8_t exit_count = compiler->exit_patch_count[compiler->loop_depth - 1];
  for (uint8_t i = 0; i < exit_count; i++) {
    uint16_t exit_jump_addr = compiler->exit_patch_stack[exit_count_before + i];
    st_compiler_patch_jump(compiler, exit_jump_addr, loop_exit_addr);
  }

  // Leave loop
  compiler->loop_depth--;
  compiler->exit_patch_total = exit_count_before;
  compiler->exit_patch_count[compiler->loop_depth] = 0;

  if (!st_compiler_emit(compiler, ST_OP_POP)) {
    return false;
  }

  return true;
}

static bool st_compiler_compile_while(st_compiler_t *compiler, st_ast_node_t *node) {
  // Enter loop
  if (compiler->loop_depth >= 8) {
    st_compiler_error(compiler, "Loop nesting too deep (max 8)");
    return false;
  }
  uint8_t exit_count_before = compiler->exit_patch_total;
  compiler->loop_depth++;

  // Loop start address
  uint16_t loop_start = st_compiler_current_addr(compiler);

  // Compile condition
  if (!st_compiler_compile_expr(compiler, node->data.while_stmt.condition)) {
    compiler->loop_depth--;
    return false;
  }

  // Emit JMP_IF_FALSE (exit if false)
  uint16_t jump_exit = st_compiler_emit_jump(compiler, ST_OP_JMP_IF_FALSE);

  // Compile loop body
  if (node->data.while_stmt.body) {
    if (!st_compiler_compile_node(compiler, node->data.while_stmt.body)) {
      compiler->loop_depth--;
      return false;
    }
  }

  // Jump back to condition check
  if (!st_compiler_emit_int(compiler, ST_OP_JMP, loop_start)) {
    compiler->loop_depth--;
    return false;
  }

  // Patch exit jump to here
  uint16_t loop_exit_addr = st_compiler_current_addr(compiler);
  st_compiler_patch_jump(compiler, jump_exit, loop_exit_addr);

  // Backpatch all EXIT statements
  uint8_t exit_count = compiler->exit_patch_count[compiler->loop_depth - 1];
  for (uint8_t i = 0; i < exit_count; i++) {
    uint16_t exit_jump_addr = compiler->exit_patch_stack[exit_count_before + i];
    st_compiler_patch_jump(compiler, exit_jump_addr, loop_exit_addr);
  }

  // Leave loop
  compiler->loop_depth--;
  compiler->exit_patch_total = exit_count_before;
  compiler->exit_patch_count[compiler->loop_depth] = 0;

  return true;
}

static bool st_compiler_compile_repeat(st_compiler_t *compiler, st_ast_node_t *node) {
  // Enter loop
  if (compiler->loop_depth >= 8) {
    st_compiler_error(compiler, "Loop nesting too deep (max 8)");
    return false;
  }
  uint8_t exit_count_before = compiler->exit_patch_total;
  compiler->loop_depth++;

  // Loop start address
  uint16_t loop_start = st_compiler_current_addr(compiler);

  // Compile loop body
  if (node->data.repeat_stmt.body) {
    if (!st_compiler_compile_node(compiler, node->data.repeat_stmt.body)) {
      compiler->loop_depth--;
      return false;
    }
  }

  // Compile condition (exit if true)
  if (!st_compiler_compile_expr(compiler, node->data.repeat_stmt.condition)) {
    compiler->loop_depth--;
    return false;
  }

  // Emit JMP_IF_FALSE (loop back if condition is false)
  if (!st_compiler_emit_int(compiler, ST_OP_JMP_IF_FALSE, loop_start)) {
    compiler->loop_depth--;
    return false;
  }

  // Loop exit (condition was true)
  uint16_t loop_exit_addr = st_compiler_current_addr(compiler);

  // Backpatch all EXIT statements
  uint8_t exit_count = compiler->exit_patch_count[compiler->loop_depth - 1];
  for (uint8_t i = 0; i < exit_count; i++) {
    uint16_t exit_jump_addr = compiler->exit_patch_stack[exit_count_before + i];
    st_compiler_patch_jump(compiler, exit_jump_addr, loop_exit_addr);
  }

  // Leave loop
  compiler->loop_depth--;
  compiler->exit_patch_total = exit_count_before;
  compiler->exit_patch_count[compiler->loop_depth] = 0;

  return true;
}

bool st_compiler_compile_node(st_compiler_t *compiler, st_ast_node_t *node) {
  if (!node) return true;  // NULL is OK (empty body)

  switch (node->type) {
    case ST_AST_ASSIGNMENT:
      if (!st_compiler_compile_assignment(compiler, node)) return false;
      break;

    case ST_AST_REMOTE_WRITE:  // v4.6.0: MB_WRITE_XXX(id, addr) := value
      if (!st_compiler_compile_remote_write(compiler, node)) return false;
      break;

    case ST_AST_IF:
      if (!st_compiler_compile_if(compiler, node)) return false;
      break;

    case ST_AST_CASE:
      if (!st_compiler_compile_case(compiler, node)) return false;
      break;

    case ST_AST_FOR:
      if (!st_compiler_compile_for(compiler, node)) return false;
      break;

    case ST_AST_WHILE:
      if (!st_compiler_compile_while(compiler, node)) return false;
      break;

    case ST_AST_REPEAT:
      if (!st_compiler_compile_repeat(compiler, node)) return false;
      break;

    case ST_AST_EXIT: {
      // EXIT statement - jump to end of current loop
      if (compiler->loop_depth == 0) {
        st_compiler_error(compiler, "EXIT outside of loop");
        return false;
      }
      // Emit JMP and save address for backpatching
      uint16_t exit_jump = st_compiler_emit_jump(compiler, ST_OP_JMP);
      if (compiler->exit_patch_total >= 32) {
        st_compiler_error(compiler, "Too many EXIT statements (max 32)");
        return false;
      }
      compiler->exit_patch_stack[compiler->exit_patch_total++] = exit_jump;
      compiler->exit_patch_count[compiler->loop_depth - 1]++;
      break;
    }

    default:
      // Ignore other node types (they're part of expressions)
      break;
  }

  // Compile next statement in list
  if (node->next) {
    if (!st_compiler_compile_node(compiler, node->next)) return false;
  }

  return true;
}

/* ============================================================================
 * MAIN COMPILATION
 * ============================================================================ */

st_bytecode_program_t *st_compiler_compile(st_compiler_t *compiler, st_program_t *program) {
  if (!program) {
    st_compiler_error(compiler, "NULL program");
    return NULL;
  }

  // Phase 1: Build symbol table from variable declarations
  for (int i = 0; i < program->var_count; i++) {
    st_variable_decl_t *var = &program->variables[i];
    uint8_t index = st_compiler_add_symbol(compiler, var->name, var->type,
                                            var->is_input, var->is_output);
    if (index == 0xFF) {
      return NULL;  // Error already reported
    }
  }

  // Phase 2: Compile statements
  if (!st_compiler_compile_node(compiler, program->body)) {
    return NULL;  // Error already reported
  }

  // Phase 3: Emit HALT
  if (!st_compiler_emit(compiler, ST_OP_HALT)) {
    return NULL;
  }

  // Phase 4: Build bytecode program structure
  st_bytecode_program_t *bytecode = (st_bytecode_program_t *)malloc(sizeof(*bytecode));
  if (!bytecode) {
    st_compiler_error(compiler, "Memory allocation failed");
    return NULL;
  }

  memset(bytecode, 0, sizeof(*bytecode));
  strncpy(bytecode->name, program->name, sizeof(bytecode->name) - 1);
  bytecode->name[sizeof(bytecode->name) - 1] = '\0';
  bytecode->enabled = 1;
  bytecode->instr_count = compiler->bytecode_ptr;
  bytecode->var_count = compiler->symbol_table.count;

  // Copy bytecode instructions
  memcpy(bytecode->instructions, compiler->bytecode,
         compiler->bytecode_ptr * sizeof(st_bytecode_instr_t));

  // Copy variable declarations
  for (int i = 0; i < compiler->symbol_table.count; i++) {
    st_symbol_t *sym = &compiler->symbol_table.symbols[i];
    bytecode->variables[i].int_val = 0;  // Initialize all to 0
    // Save variable name and type for CLI binding
    strncpy(bytecode->var_names[i], sym->name, sizeof(bytecode->var_names[i]) - 1);
    bytecode->var_names[i][sizeof(bytecode->var_names[i]) - 1] = '\0';
    bytecode->var_types[i] = sym->type;  // Store variable type (BOOL, INT, etc.)
    debug_printf("[COMPILER] Copied to bytecode: var[%d] name='%s' type=%d\n",
                 i, bytecode->var_names[i], bytecode->var_types[i]);
  }

  // v4.7+: Allocate stateful storage if any stateful functions were used
  if (compiler->edge_instance_count > 0 ||
      compiler->timer_instance_count > 0 ||
      compiler->counter_instance_count > 0) {
    st_stateful_storage_t *stateful = (st_stateful_storage_t *)malloc(sizeof(st_stateful_storage_t));
    if (!stateful) {
      st_compiler_error(compiler, "Failed to allocate stateful storage");
      free(bytecode);
      return NULL;
    }
    st_stateful_init(stateful);

    // Pre-allocate instances based on compiler counts
    stateful->edge_count = compiler->edge_instance_count;
    stateful->timer_count = compiler->timer_instance_count;
    stateful->counter_count = compiler->counter_instance_count;

    bytecode->stateful = (struct st_stateful_storage*)stateful;  // Cast to opaque pointer

    debug_printf("[COMPILER] Allocated stateful storage: edges=%d timers=%d counters=%d\n",
                 compiler->edge_instance_count, compiler->timer_instance_count,
                 compiler->counter_instance_count);
  } else {
    bytecode->stateful = NULL;
  }

  if (compiler->error_count > 0) {
    if (bytecode->stateful) free(bytecode->stateful);
    free(bytecode);
    return NULL;
  }

  return bytecode;
}

/* ============================================================================
 * DEBUGGING
 * ============================================================================ */

const char *st_opcode_to_string(st_opcode_t opcode) {
  switch (opcode) {
    case ST_OP_PUSH_BOOL:       return "PUSH_BOOL";
    case ST_OP_PUSH_INT:        return "PUSH_INT";
    case ST_OP_PUSH_DWORD:      return "PUSH_DWORD";
    case ST_OP_PUSH_REAL:       return "PUSH_REAL";
    case ST_OP_PUSH_VAR:        return "PUSH_VAR";
    case ST_OP_ADD:             return "ADD";
    case ST_OP_SUB:             return "SUB";
    case ST_OP_MUL:             return "MUL";
    case ST_OP_DIV:             return "DIV";
    case ST_OP_MOD:             return "MOD";
    case ST_OP_NEG:             return "NEG";
    case ST_OP_AND:             return "AND";
    case ST_OP_OR:              return "OR";
    case ST_OP_NOT:             return "NOT";
    case ST_OP_XOR:             return "XOR";
    case ST_OP_SHL:             return "SHL";
    case ST_OP_SHR:             return "SHR";
    case ST_OP_EQ:              return "EQ";
    case ST_OP_NE:              return "NE";
    case ST_OP_LT:              return "LT";
    case ST_OP_GT:              return "GT";
    case ST_OP_LE:              return "LE";
    case ST_OP_GE:              return "GE";
    case ST_OP_JMP:             return "JMP";
    case ST_OP_JMP_IF_FALSE:    return "JMP_IF_FALSE";
    case ST_OP_JMP_IF_TRUE:     return "JMP_IF_TRUE";
    case ST_OP_STORE_VAR:       return "STORE_VAR";
    case ST_OP_LOAD_VAR:        return "LOAD_VAR";
    case ST_OP_DUP:             return "DUP";
    case ST_OP_POP:             return "POP";
    case ST_OP_LOOP_INIT:       return "LOOP_INIT";
    case ST_OP_LOOP_TEST:       return "LOOP_TEST";
    case ST_OP_LOOP_NEXT:       return "LOOP_NEXT";
    case ST_OP_CALL_BUILTIN:    return "CALL_BUILTIN";
    case ST_OP_NOP:             return "NOP";
    case ST_OP_HALT:            return "HALT";
    default:                    return "UNKNOWN";
  }
}

void st_bytecode_print(st_bytecode_program_t *bytecode) {
  if (!bytecode) {
    debug_println("NULL bytecode");
    return;
  }

  debug_println("");
  debug_printf("=== Bytecode Program: %s ===\n", bytecode->name);
  debug_printf("Instructions: %d\n", bytecode->instr_count);
  debug_printf("Variables: %d\n", bytecode->var_count);
  debug_println("");

  debug_println("Bytecode (detailed):");
  for (int i = 0; i < bytecode->instr_count; i++) {
    st_bytecode_instr_t *instr = &bytecode->instructions[i];
    char line[256];
    const char *opname = st_opcode_to_string(instr->opcode);

    // Build instruction line with argument
    switch (instr->opcode) {
      case ST_OP_PUSH_INT:
      case ST_OP_PUSH_DWORD:
      case ST_OP_PUSH_BOOL:
      case ST_OP_PUSH_REAL:
      case ST_OP_JMP:
      case ST_OP_JMP_IF_FALSE:
      case ST_OP_JMP_IF_TRUE:
      case ST_OP_CALL_BUILTIN:
        snprintf(line, sizeof(line), "  [%3d] %-18s %d", i, opname, instr->arg.int_arg);
        break;

      case ST_OP_STORE_VAR:
      case ST_OP_LOAD_VAR:
      case ST_OP_PUSH_VAR:
        snprintf(line, sizeof(line), "  [%3d] %-18s var[%d]", i, opname, instr->arg.var_index);
        break;

      default:
        snprintf(line, sizeof(line), "  [%3d] %-18s", i, opname);
        break;
    }

    debug_println(line);
  }

  debug_println("");
}
