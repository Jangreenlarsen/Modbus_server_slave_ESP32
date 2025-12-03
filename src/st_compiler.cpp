/**
 * @file st_compiler.cpp
 * @brief Structured Text Bytecode Compiler Implementation
 *
 * Converts AST (from parser) to bytecode (stack-based VM instructions).
 * Single-pass with symbol table and jump backpatching.
 */

#include "st_compiler.h"
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
  strcpy(sym->name, name);
  sym->type = type;
  sym->is_input = is_input;
  sym->is_output = is_output;
  sym->index = compiler->symbol_table.count;

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
  if (compiler->bytecode_ptr + count >= 512) {
    st_compiler_error(compiler, "Bytecode buffer overflow (max 512 instructions)");
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

uint16_t st_compiler_current_addr(st_compiler_t *compiler) {
  return compiler->bytecode_ptr;
}

uint16_t st_compiler_emit_jump(st_compiler_t *compiler, st_opcode_t opcode) {
  uint16_t addr = st_compiler_current_addr(compiler);
  st_compiler_emit_int(compiler, opcode, 0);  // Placeholder address
  return addr;
}

void st_compiler_patch_jump(st_compiler_t *compiler, uint16_t jump_addr, uint16_t target_addr) {
  if (jump_addr >= 512) return;
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

  // Compile each case branch
  for (uint8_t i = 0; i < node->data.case_stmt.branch_count; i++) {
    st_case_branch_t *branch = &node->data.case_stmt.branches[i];

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

    // We matched this case - pop the duplicate expression value before executing branch
    if (!st_compiler_emit(compiler, ST_OP_POP)) {
      free(jump_end);
      return false;
    }

    // Compile branch body
    if (branch->body) {
      if (!st_compiler_compile_node(compiler, branch->body)) {
        free(jump_end);
        return false;
      }
    }

    // Jump to end of CASE
    jump_end[jump_count++] = st_compiler_emit_jump(compiler, ST_OP_JMP);

    // Patch next case jump to right after comparison (before duplicate pop)
    // This way, if we don't match, we skip the POP and go straight to next DUP
    st_compiler_patch_jump(compiler, jump_next, st_compiler_current_addr(compiler));
  }

  // Pop the expression value
  if (!st_compiler_emit(compiler, ST_OP_POP)) {
    free(jump_end);
    return false;
  }

  // Compile ELSE block if present
  if (node->data.case_stmt.else_body) {
    if (!st_compiler_compile_node(compiler, node->data.case_stmt.else_body)) {
      free(jump_end);
      return false;
    }
  }

  // Patch all end jumps
  uint16_t end_addr = st_compiler_current_addr(compiler);
  for (uint8_t i = 0; i < jump_count; i++) {
    st_compiler_patch_jump(compiler, jump_end[i], end_addr);
  }

  free(jump_end);
  return true;
}

static bool st_compiler_compile_if(st_compiler_t *compiler, st_ast_node_t *node) {
  // Compile condition (result on stack)
  if (!st_compiler_compile_expr(compiler, node->data.if_stmt.condition_expr)) {
    return false;
  }

  // Emit JMP_IF_FALSE (skip THEN block if condition false)
  uint16_t jump_then = st_compiler_emit_jump(compiler, ST_OP_JMP_IF_FALSE);

  // Compile THEN block
  if (node->data.if_stmt.then_body) {
    if (!st_compiler_compile_node(compiler, node->data.if_stmt.then_body)) {
      return false;
    }
  }

  // If ELSE block exists, emit JMP to skip it
  uint16_t jump_else = 0;
  if (node->data.if_stmt.else_body) {
    jump_else = st_compiler_emit_jump(compiler, ST_OP_JMP);
  }

  // Patch THEN jump to here
  st_compiler_patch_jump(compiler, jump_then, st_compiler_current_addr(compiler));

  // Compile ELSE block if present
  if (node->data.if_stmt.else_body) {
    if (!st_compiler_compile_node(compiler, node->data.if_stmt.else_body)) {
      return false;
    }
    // Patch ELSE jump to here
    st_compiler_patch_jump(compiler, jump_else, st_compiler_current_addr(compiler));
  }

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

  // Compile start expression
  if (!st_compiler_compile_expr(compiler, node->data.for_stmt.start)) {
    return false;
  }

  // Store to loop variable
  if (!st_compiler_emit_var(compiler, ST_OP_STORE_VAR, var_index)) {
    return false;
  }

  // Compile end expression
  if (!st_compiler_compile_expr(compiler, node->data.for_stmt.end)) {
    return false;
  }

  // Loop start address (for backjump)
  uint16_t loop_start = st_compiler_current_addr(compiler);

  // Compile loop body
  if (node->data.for_stmt.body) {
    if (!st_compiler_compile_node(compiler, node->data.for_stmt.body)) {
      return false;
    }
  }

  // Increment loop variable (simplified: always +1 for now)
  // TODO: Handle step (BY clause)
  if (!st_compiler_emit_var(compiler, ST_OP_LOAD_VAR, var_index)) {
    return false;
  }
  if (!st_compiler_emit_int(compiler, ST_OP_PUSH_INT, 1)) {
    return false;
  }
  if (!st_compiler_emit(compiler, ST_OP_ADD)) {
    return false;
  }
  if (!st_compiler_emit_var(compiler, ST_OP_STORE_VAR, var_index)) {
    return false;
  }

  // Loop condition check: if var < end, jump back to start
  // Load var, load end, compare
  if (!st_compiler_emit_var(compiler, ST_OP_LOAD_VAR, var_index)) {
    return false;
  }
  // TODO: Load stored end value (need to save it first)
  // For now, simplified - emit JMP back
  if (!st_compiler_emit_int(compiler, ST_OP_JMP, loop_start)) {
    return false;
  }

  return true;
}

static bool st_compiler_compile_while(st_compiler_t *compiler, st_ast_node_t *node) {
  // Loop start address
  uint16_t loop_start = st_compiler_current_addr(compiler);

  // Compile condition
  if (!st_compiler_compile_expr(compiler, node->data.while_stmt.condition)) {
    return false;
  }

  // Emit JMP_IF_FALSE (exit if false)
  uint16_t jump_exit = st_compiler_emit_jump(compiler, ST_OP_JMP_IF_FALSE);

  // Compile loop body
  if (node->data.while_stmt.body) {
    if (!st_compiler_compile_node(compiler, node->data.while_stmt.body)) {
      return false;
    }
  }

  // Jump back to condition check
  if (!st_compiler_emit_int(compiler, ST_OP_JMP, loop_start)) {
    return false;
  }

  // Patch exit jump to here
  st_compiler_patch_jump(compiler, jump_exit, st_compiler_current_addr(compiler));

  return true;
}

static bool st_compiler_compile_repeat(st_compiler_t *compiler, st_ast_node_t *node) {
  // Loop start address
  uint16_t loop_start = st_compiler_current_addr(compiler);

  // Compile loop body
  if (node->data.repeat_stmt.body) {
    if (!st_compiler_compile_node(compiler, node->data.repeat_stmt.body)) {
      return false;
    }
  }

  // Compile condition (exit if true)
  if (!st_compiler_compile_expr(compiler, node->data.repeat_stmt.condition)) {
    return false;
  }

  // Emit JMP_IF_FALSE (loop back if condition is false)
  if (!st_compiler_emit_int(compiler, ST_OP_JMP_IF_FALSE, loop_start)) {
    return false;
  }

  return true;
}

bool st_compiler_compile_node(st_compiler_t *compiler, st_ast_node_t *node) {
  if (!node) return true;  // NULL is OK (empty body)

  switch (node->type) {
    case ST_AST_ASSIGNMENT:
      if (!st_compiler_compile_assignment(compiler, node)) return false;
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

    case ST_AST_EXIT:
      // TODO: Implement EXIT (break from loop)
      if (!st_compiler_emit(compiler, ST_OP_NOP)) return false;
      break;

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
  strcpy(bytecode->name, program->name);
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
    // Save variable name for CLI binding by name
    strncpy(bytecode->var_names[i], sym->name, sizeof(bytecode->var_names[i]) - 1);
    bytecode->var_names[i][sizeof(bytecode->var_names[i]) - 1] = '\0';
  }

  if (compiler->error_count > 0) {
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
    printf("NULL bytecode\n");
    return;
  }

  printf("\n=== Bytecode Program: %s ===\n", bytecode->name);
  printf("Instructions: %d\n", bytecode->instr_count);
  printf("Variables: %d\n\n", bytecode->var_count);

  printf("Bytecode:\n");
  for (int i = 0; i < bytecode->instr_count; i++) {
    st_bytecode_instr_t *instr = &bytecode->instructions[i];
    printf("  [%3d] %-15s", i, st_opcode_to_string(instr->opcode));

    // Print argument based on opcode
    switch (instr->opcode) {
      case ST_OP_PUSH_INT:
      case ST_OP_PUSH_DWORD:
      case ST_OP_PUSH_BOOL:
      case ST_OP_PUSH_REAL:
      case ST_OP_JMP:
      case ST_OP_JMP_IF_FALSE:
      case ST_OP_JMP_IF_TRUE:
      case ST_OP_CALL_BUILTIN:
        printf(" %d\n", instr->arg.int_arg);
        break;

      case ST_OP_STORE_VAR:
      case ST_OP_LOAD_VAR:
      case ST_OP_PUSH_VAR:
        printf(" var[%d]\n", instr->arg.var_index);
        break;

      default:
        printf("\n");
        break;
    }
  }

  printf("\n");
}
