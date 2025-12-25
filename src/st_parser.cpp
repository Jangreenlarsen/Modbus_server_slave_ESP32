/**
 * @file st_parser.cpp
 * @brief Structured Text Parser implementation
 *
 * Recursive descent parser converting token stream to AST.
 * Implements IEC 61131-3 grammar with operator precedence.
 */

#include "st_parser.h"
#include "st_lexer.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>     // BUG-069/070: For overflow detection
#include <stdint.h>    // BUG-069: For INT32_MAX/MIN

/* ============================================================================
 * PARSER UTILITIES
 * ============================================================================ */

void st_parser_init(st_parser_t *parser, const char *input) {
  st_lexer_init(&parser->lexer, input);
  parser->error_count = 0;
  memset(parser->error_msg, 0, sizeof(parser->error_msg));

  // Load first token
  st_lexer_next_token(&parser->lexer, &parser->current_token);
  st_lexer_next_token(&parser->lexer, &parser->peek_token);
}

/* Advance to next token */
static void parser_advance(st_parser_t *parser) {
  parser->current_token = parser->peek_token;
  st_lexer_next_token(&parser->lexer, &parser->peek_token);
}

/* Check if current token matches type */
static bool parser_match(st_parser_t *parser, st_token_type_t type) {
  return parser->current_token.type == type;
}

/* Expect token of specific type, advance if matches */
static bool parser_expect(st_parser_t *parser, st_token_type_t type) {
  if (!parser_match(parser, type)) {
    snprintf(parser->error_msg, sizeof(parser->error_msg),
             "Expected token %s at line %u, got %s",
             st_token_type_to_string(type),
             parser->current_token.line,
             st_token_type_to_string(parser->current_token.type));
    parser->error_count++;
    return false;
  }
  parser_advance(parser);
  return true;
}

/* Report error */
static void parser_error(st_parser_t *parser, const char *msg) {
  snprintf(parser->error_msg, sizeof(parser->error_msg),
           "Parse error at line %u: %s",
           parser->current_token.line, msg);
  parser->error_count++;
}

/* ============================================================================
 * AST NODE ALLOCATION
 * ============================================================================ */

static st_ast_node_t *ast_node_alloc(st_ast_node_type_t type, uint32_t line) {
  st_ast_node_t *node = (st_ast_node_t *)malloc(sizeof(st_ast_node_t));
  if (!node) return NULL;

  memset(node, 0, sizeof(*node));
  node->type = type;
  node->line = line;
  return node;
}

void st_ast_node_free(st_ast_node_t *node) {
  if (!node) return;

  // Free children based on node type
  switch (node->type) {
    case ST_AST_BINARY_OP:
      st_ast_node_free(node->data.binary_op.left);
      st_ast_node_free(node->data.binary_op.right);
      break;

    case ST_AST_UNARY_OP:
      st_ast_node_free(node->data.unary_op.operand);
      break;

    case ST_AST_IF:
      st_ast_node_free(node->data.if_stmt.condition_expr);
      st_ast_node_free(node->data.if_stmt.then_body);
      st_ast_node_free(node->data.if_stmt.else_body);
      break;

    case ST_AST_CASE:
      st_ast_node_free(node->data.case_stmt.expr);
      // Free all case branches
      for (uint8_t i = 0; i < node->data.case_stmt.branch_count; i++) {
        st_ast_node_free(node->data.case_stmt.branches[i].body);
      }
      // Free ELSE body
      st_ast_node_free(node->data.case_stmt.else_body);
      break;

    case ST_AST_FOR:
      st_ast_node_free(node->data.for_stmt.start);
      st_ast_node_free(node->data.for_stmt.end);
      st_ast_node_free(node->data.for_stmt.step);
      st_ast_node_free(node->data.for_stmt.body);
      break;

    case ST_AST_WHILE:
      st_ast_node_free(node->data.while_stmt.condition);
      st_ast_node_free(node->data.while_stmt.body);
      break;

    case ST_AST_REPEAT:
      st_ast_node_free(node->data.repeat_stmt.body);
      st_ast_node_free(node->data.repeat_stmt.condition);
      break;

    case ST_AST_ASSIGNMENT:
      st_ast_node_free(node->data.assignment.expr);
      break;

    case ST_AST_FUNCTION_CALL:
      // Free all function arguments
      for (uint8_t i = 0; i < node->data.function_call.arg_count; i++) {
        st_ast_node_free(node->data.function_call.args[i]);
      }
      break;

    default:
      break;
  }

  // Free next statement in list
  if (node->next) {
    st_ast_node_free(node->next);
  }

  free(node);
}

void st_program_free(st_program_t *program) {
  if (!program) return;
  st_ast_node_free(program->body);
  free(program);
}

/* ============================================================================
 * EXPRESSION PARSING (with operator precedence)
 * ============================================================================ */

/* Forward declarations */
static st_ast_node_t *parser_parse_expression(st_parser_t *parser);
static st_ast_node_t *parser_parse_primary(st_parser_t *parser);
static st_ast_node_t *parser_parse_unary(st_parser_t *parser);
static st_ast_node_t *parser_parse_multiplicative(st_parser_t *parser);
static st_ast_node_t *parser_parse_additive(st_parser_t *parser);
static st_ast_node_t *parser_parse_relational(st_parser_t *parser);
static st_ast_node_t *parser_parse_logical_and(st_parser_t *parser);
static st_ast_node_t *parser_parse_logical_or(st_parser_t *parser);

/* Parse primary expression: literal, variable, (expr) */
static st_ast_node_t *parser_parse_primary(st_parser_t *parser) {
  uint32_t line = parser->current_token.line;

  // Boolean literal
  if (parser_match(parser, ST_TOK_BOOL_TRUE)) {
    st_ast_node_t *node = ast_node_alloc(ST_AST_LITERAL, line);
    // BUG-066: Check malloc failure
    if (!node) {
      parser_error(parser, "Out of memory");
      return NULL;
    }
    node->data.literal.type = ST_TYPE_BOOL;
    node->data.literal.value.bool_val = true;
    parser_advance(parser);
    return node;
  }

  if (parser_match(parser, ST_TOK_BOOL_FALSE)) {
    st_ast_node_t *node = ast_node_alloc(ST_AST_LITERAL, line);
    // BUG-066: Check malloc failure
    if (!node) {
      parser_error(parser, "Out of memory");
      return NULL;
    }
    node->data.literal.type = ST_TYPE_BOOL;
    node->data.literal.value.bool_val = false;
    parser_advance(parser);
    return node;
  }

  // Integer literal
  if (parser_match(parser, ST_TOK_INT)) {
    st_ast_node_t *node = ast_node_alloc(ST_AST_LITERAL, line);
    // BUG-066: Check malloc failure
    if (!node) {
      parser_error(parser, "Out of memory");
      return NULL;
    }
    node->data.literal.type = ST_TYPE_INT;
    // BUG-069: Check for overflow
    errno = 0;
    long val = strtol(parser->current_token.value, NULL, 0);
    if (errno == ERANGE || val > INT32_MAX || val < INT32_MIN) {
      parser_error(parser, "Integer literal overflow");
      free(node);
      return NULL;
    }
    node->data.literal.value.int_val = (int32_t)val;
    parser_advance(parser);
    return node;
  }

  // Real literal
  if (parser_match(parser, ST_TOK_REAL)) {
    st_ast_node_t *node = ast_node_alloc(ST_AST_LITERAL, line);
    // BUG-066: Check malloc failure
    if (!node) {
      parser_error(parser, "Out of memory");
      return NULL;
    }
    node->data.literal.type = ST_TYPE_REAL;
    // BUG-070: Check for overflow/underflow
    errno = 0;
    float fval = strtof(parser->current_token.value, NULL);
    if (errno == ERANGE) {
      parser_error(parser, "Real literal overflow/underflow");
      free(node);
      return NULL;
    }
    node->data.literal.value.real_val = fval;
    parser_advance(parser);
    return node;
  }

  // Variable or Function Call
  if (parser_match(parser, ST_TOK_IDENT)) {
    char identifier[64];
    strncpy(identifier, parser->current_token.value, 63);
    identifier[63] = '\0';
    parser_advance(parser);

    // Check if this is a function call (followed by '(')
    if (parser_match(parser, ST_TOK_LPAREN)) {
      parser_advance(parser); // consume '('

      st_ast_node_t *node = ast_node_alloc(ST_AST_FUNCTION_CALL, line);
      // BUG-066: Check malloc failure
      if (!node) {
        parser_error(parser, "Out of memory");
        return NULL;
      }
      strncpy(node->data.function_call.func_name, identifier, 63);
      node->data.function_call.func_name[63] = '\0';
      node->data.function_call.arg_count = 0;

      // Parse arguments (if any)
      if (!parser_match(parser, ST_TOK_RPAREN)) {
        // Parse first argument
        st_ast_node_t *arg = parser_parse_expression(parser);
        if (arg && node->data.function_call.arg_count < 4) {
          node->data.function_call.args[node->data.function_call.arg_count++] = arg;
        }

        // Parse additional arguments (comma-separated)
        while (parser_match(parser, ST_TOK_COMMA)) {
          parser_advance(parser); // consume ','
          arg = parser_parse_expression(parser);
          if (arg && node->data.function_call.arg_count < 4) {
            node->data.function_call.args[node->data.function_call.arg_count++] = arg;
          } else if (node->data.function_call.arg_count >= 4) {
            parser_error(parser, "Too many function arguments (max 4)");
            return NULL;  // BUG-063: Return NULL instead of break to fail parsing
          }
        }
      }

      // Expect closing ')'
      if (!parser_expect(parser, ST_TOK_RPAREN)) {
        parser_error(parser, "Expected ')' after function arguments");
      }

      return node;
    } else {
      // It's a variable reference
      st_ast_node_t *node = ast_node_alloc(ST_AST_VARIABLE, line);
      // BUG-066: Check malloc failure
      if (!node) {
        parser_error(parser, "Out of memory");
        return NULL;
      }
      strncpy(node->data.variable.var_name, identifier, 63);
      node->data.variable.var_name[63] = '\0';
      return node;
    }
  }

  // Parenthesized expression
  if (parser_match(parser, ST_TOK_LPAREN)) {
    parser_advance(parser);
    st_ast_node_t *expr = parser_parse_expression(parser);
    parser_expect(parser, ST_TOK_RPAREN);
    return expr;
  }

  parser_error(parser, "Expected expression (literal, variable, or parenthesized expression)");
  return NULL;
}

/* Parse unary expression: -expr, NOT expr */
static st_ast_node_t *parser_parse_unary(st_parser_t *parser) {
  uint32_t line = parser->current_token.line;

  if (parser_match(parser, ST_TOK_MINUS) || parser_match(parser, ST_TOK_NOT)) {
    st_token_type_t op = parser->current_token.type;
    parser_advance(parser);

    st_ast_node_t *node = ast_node_alloc(ST_AST_UNARY_OP, line);
    // BUG-066: Check malloc failure
    if (!node) {
      parser_error(parser, "Out of memory");
      return NULL;
    }
    node->data.unary_op.op = op;
    node->data.unary_op.operand = parser_parse_unary(parser);
    // BUG-081: Check if operand parsing failed
    if (!node->data.unary_op.operand) {
      free(node);
      return NULL;
    }
    return node;
  }

  return parser_parse_primary(parser);
}

/* Parse multiplicative and bitwise shift: *, /, MOD, SHL, SHR */
static st_ast_node_t *parser_parse_multiplicative(st_parser_t *parser) {
  st_ast_node_t *left = parser_parse_unary(parser);
  if (!left) return NULL;  // BUG-081: Early exit if left fails

  while (parser_match(parser, ST_TOK_MUL) || parser_match(parser, ST_TOK_DIV) || parser_match(parser, ST_TOK_MOD) ||
         parser_match(parser, ST_TOK_SHL) || parser_match(parser, ST_TOK_SHR)) {
    uint32_t line = parser->current_token.line;
    st_token_type_t op = parser->current_token.type;
    parser_advance(parser);

    st_ast_node_t *right = parser_parse_unary(parser);
    // BUG-081: Check if right parsing failed
    if (!right) {
      st_ast_node_free(left);
      return NULL;
    }
    st_ast_node_t *node = ast_node_alloc(ST_AST_BINARY_OP, line);
    // BUG-066: Check malloc failure
    if (!node) {
      parser_error(parser, "Out of memory");
      st_ast_node_free(left);
      st_ast_node_free(right);
      return NULL;
    }
    node->data.binary_op.op = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;
    left = node;
  }

  return left;
}

/* Parse additive: +, - */
static st_ast_node_t *parser_parse_additive(st_parser_t *parser) {
  st_ast_node_t *left = parser_parse_multiplicative(parser);
  if (!left) return NULL;  // BUG-081: Early exit if left fails

  while (parser_match(parser, ST_TOK_PLUS) || parser_match(parser, ST_TOK_MINUS)) {
    uint32_t line = parser->current_token.line;
    st_token_type_t op = parser->current_token.type;
    parser_advance(parser);

    st_ast_node_t *right = parser_parse_multiplicative(parser);
    // BUG-081: Check if right parsing failed
    if (!right) {
      st_ast_node_free(left);
      return NULL;
    }
    st_ast_node_t *node = ast_node_alloc(ST_AST_BINARY_OP, line);
    // BUG-066: Check malloc failure
    if (!node) {
      parser_error(parser, "Out of memory");
      st_ast_node_free(left);
      st_ast_node_free(right);
      return NULL;
    }
    node->data.binary_op.op = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;
    left = node;
  }

  return left;
}

/* Parse relational: <, >, <=, >=, =, <> */
static st_ast_node_t *parser_parse_relational(st_parser_t *parser) {
  st_ast_node_t *left = parser_parse_additive(parser);
  if (!left) return NULL;  // BUG-081: Early exit if left fails

  while (parser_match(parser, ST_TOK_LT) || parser_match(parser, ST_TOK_GT) ||
         parser_match(parser, ST_TOK_LE) || parser_match(parser, ST_TOK_GE) ||
         parser_match(parser, ST_TOK_EQ) || parser_match(parser, ST_TOK_NE)) {
    uint32_t line = parser->current_token.line;
    st_token_type_t op = parser->current_token.type;
    parser_advance(parser);

    st_ast_node_t *right = parser_parse_additive(parser);
    // BUG-081: Check if right parsing failed
    if (!right) {
      st_ast_node_free(left);
      return NULL;
    }
    st_ast_node_t *node = ast_node_alloc(ST_AST_BINARY_OP, line);
    // BUG-066: Check malloc failure
    if (!node) {
      parser_error(parser, "Out of memory");
      st_ast_node_free(left);
      st_ast_node_free(right);
      return NULL;
    }
    node->data.binary_op.op = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;
    left = node;
  }

  return left;
}

/* Parse logical AND */
static st_ast_node_t *parser_parse_logical_and(st_parser_t *parser) {
  st_ast_node_t *left = parser_parse_relational(parser);
  if (!left) return NULL;  // BUG-081: Early exit if left fails

  while (parser_match(parser, ST_TOK_AND)) {
    uint32_t line = parser->current_token.line;
    parser_advance(parser);

    st_ast_node_t *right = parser_parse_relational(parser);
    // BUG-081: Check if right parsing failed
    if (!right) {
      st_ast_node_free(left);
      return NULL;
    }
    st_ast_node_t *node = ast_node_alloc(ST_AST_BINARY_OP, line);
    // BUG-066: Check malloc failure
    if (!node) {
      parser_error(parser, "Out of memory");
      st_ast_node_free(left);
      st_ast_node_free(right);
      return NULL;
    }
    node->data.binary_op.op = ST_TOK_AND;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;
    left = node;
  }

  return left;
}

/* Parse logical OR */
static st_ast_node_t *parser_parse_logical_or(st_parser_t *parser) {
  st_ast_node_t *left = parser_parse_logical_and(parser);
  if (!left) return NULL;  // BUG-081: Early exit if left fails

  while (parser_match(parser, ST_TOK_OR) || parser_match(parser, ST_TOK_XOR)) {
    uint32_t line = parser->current_token.line;
    st_token_type_t op = parser->current_token.type;
    parser_advance(parser);

    st_ast_node_t *right = parser_parse_logical_and(parser);
    // BUG-081: Check if right parsing failed
    if (!right) {
      st_ast_node_free(left);
      return NULL;
    }
    st_ast_node_t *node = ast_node_alloc(ST_AST_BINARY_OP, line);
    // BUG-066: Check malloc failure
    if (!node) {
      parser_error(parser, "Out of memory");
      st_ast_node_free(left);
      st_ast_node_free(right);
      return NULL;
    }
    node->data.binary_op.op = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;
    left = node;
  }

  return left;
}

/* Parse full expression (top level) */
static st_ast_node_t *parser_parse_expression(st_parser_t *parser) {
  return parser_parse_logical_or(parser);
}

/* ============================================================================
 * STATEMENT PARSING
 * ============================================================================ */

/* Forward declarations */
static st_ast_node_t *parser_parse_if_statement(st_parser_t *parser);
static st_ast_node_t *parser_parse_case_statement(st_parser_t *parser);
static st_ast_node_t *parser_parse_for_statement(st_parser_t *parser);
static st_ast_node_t *parser_parse_while_statement(st_parser_t *parser);
static st_ast_node_t *parser_parse_repeat_statement(st_parser_t *parser);
static st_ast_node_t *st_parser_parse_statements_for_case(st_parser_t *parser);

/* Parse assignment: var := expr; */
static st_ast_node_t *parser_parse_assignment(st_parser_t *parser) {
  uint32_t line = parser->current_token.line;
  char var_name[64] = {0};

  if (!parser_match(parser, ST_TOK_IDENT)) {
    parser_error(parser, "Expected variable name in assignment");
    return NULL;
  }

  // BUG-032 FIX: Use strncpy to prevent buffer overflow
  strncpy(var_name, parser->current_token.value, 63);
  var_name[63] = '\0';
  parser_advance(parser);

  if (!parser_expect(parser, ST_TOK_ASSIGN)) {
    parser_error(parser, "Expected := in assignment");
    return NULL;
  }

  st_ast_node_t *expr = parser_parse_expression(parser);
  if (!expr) return NULL;

  st_ast_node_t *node = ast_node_alloc(ST_AST_ASSIGNMENT, line);
  // BUG-066: Check malloc failure
  if (!node) {
    parser_error(parser, "Out of memory");
    st_ast_node_free(expr);
    return NULL;
  }
  // BUG-032 FIX: Use strncpy to prevent buffer overflow
  strncpy(node->data.assignment.var_name, var_name, 63);
  node->data.assignment.var_name[63] = '\0';
  node->data.assignment.expr = expr;

  // Consume optional semicolon
  if (parser_match(parser, ST_TOK_SEMICOLON)) {
    parser_advance(parser);
  }

  return node;
}

/* Parse IF statement: IF expr THEN ... [ELSE ...] END_IF */
static st_ast_node_t *parser_parse_if_statement(st_parser_t *parser) {
  uint32_t line = parser->current_token.line;

  parser_expect(parser, ST_TOK_IF);

  st_ast_node_t *condition = parser_parse_expression(parser);
  if (!condition) return NULL;

  if (!parser_expect(parser, ST_TOK_THEN)) {
    parser_error(parser, "Expected THEN after IF condition");
    st_ast_node_free(condition);
    return NULL;
  }

  st_ast_node_t *then_body = NULL;
  if (!parser_match(parser, ST_TOK_ELSE) && !parser_match(parser, ST_TOK_ELSIF) && !parser_match(parser, ST_TOK_END_IF)) {
    then_body = st_parser_parse_statements(parser);
  }

  st_ast_node_t *else_body = NULL;

  // Handle ELSIF chain recursively
  while (parser_match(parser, ST_TOK_ELSIF)) {
    parser_advance(parser);

    st_ast_node_t *elsif_condition = parser_parse_expression(parser);
    if (!elsif_condition) {
      st_ast_node_free(condition);
      st_ast_node_free(then_body);
      st_ast_node_free(else_body);
      return NULL;
    }

    if (!parser_expect(parser, ST_TOK_THEN)) {
      parser_error(parser, "Expected THEN after ELSIF condition");
      st_ast_node_free(condition);
      st_ast_node_free(then_body);
      st_ast_node_free(else_body);
      st_ast_node_free(elsif_condition);
      return NULL;
    }

    st_ast_node_t *elsif_then = NULL;
    if (!parser_match(parser, ST_TOK_ELSE) && !parser_match(parser, ST_TOK_ELSIF) && !parser_match(parser, ST_TOK_END_IF)) {
      elsif_then = st_parser_parse_statements(parser);
    }

    // Create IF node for this ELSIF
    st_ast_node_t *elsif_node = ast_node_alloc(ST_AST_IF, elsif_condition->line);
    // BUG-066: Check malloc failure
    if (!elsif_node) {
      parser_error(parser, "Out of memory");
      st_ast_node_free(condition);
      st_ast_node_free(then_body);
      st_ast_node_free(else_body);
      st_ast_node_free(elsif_condition);
      st_ast_node_free(elsif_then);
      return NULL;
    }
    elsif_node->data.if_stmt.condition_expr = elsif_condition;
    elsif_node->data.if_stmt.then_body = elsif_then;
    elsif_node->data.if_stmt.else_body = NULL;

    // Chain ELSIFs: first ELSIF goes into original else_body, subsequent into previous ELSIF's else_body
    if (else_body == NULL) {
      else_body = elsif_node;
    } else {
      // Find last node in ELSIF chain
      st_ast_node_t *last = else_body;
      while (last->data.if_stmt.else_body != NULL) {
        last = last->data.if_stmt.else_body;
      }
      last->data.if_stmt.else_body = elsif_node;
    }
  }

  // Handle final ELSE (if any)
  if (parser_match(parser, ST_TOK_ELSE)) {
    parser_advance(parser);
    st_ast_node_t *final_else = NULL;
    if (!parser_match(parser, ST_TOK_END_IF)) {
      final_else = st_parser_parse_statements(parser);
    }

    // Attach to last ELSIF (if any), otherwise to main IF
    if (else_body != NULL) {
      st_ast_node_t *last = else_body;
      while (last->data.if_stmt.else_body != NULL) {
        last = last->data.if_stmt.else_body;
      }
      last->data.if_stmt.else_body = final_else;
    } else {
      else_body = final_else;
    }
  }

  if (!parser_expect(parser, ST_TOK_END_IF)) {
    parser_error(parser, "Expected END_IF");
    st_ast_node_free(condition);
    st_ast_node_free(then_body);
    st_ast_node_free(else_body);
    return NULL;
  }

  // Consume optional semicolon
  if (parser_match(parser, ST_TOK_SEMICOLON)) {
    parser_advance(parser);
  }

  st_ast_node_t *node = ast_node_alloc(ST_AST_IF, line);
  // BUG-066: Check malloc failure
  if (!node) {
    parser_error(parser, "Out of memory");
    st_ast_node_free(condition);
    st_ast_node_free(then_body);
    st_ast_node_free(else_body);
    return NULL;
  }
  node->data.if_stmt.condition_expr = condition;
  node->data.if_stmt.then_body = then_body;
  node->data.if_stmt.else_body = else_body;

  return node;
}

/* Parse CASE statement: CASE expr OF ... END_CASE */
static st_ast_node_t *parser_parse_case_statement(st_parser_t *parser) {
  uint32_t line = parser->current_token.line;

  parser_expect(parser, ST_TOK_CASE);

  st_ast_node_t *expr = parser_parse_expression(parser);
  if (!expr) return NULL;

  if (!parser_expect(parser, ST_TOK_OF)) {
    parser_error(parser, "Expected OF in CASE statement");
    st_ast_node_free(expr);
    return NULL;
  }

  // Create CASE node
  st_ast_node_t *node = ast_node_alloc(ST_AST_CASE, line);
  // BUG-066: Check malloc failure
  if (!node) {
    parser_error(parser, "Out of memory");
    st_ast_node_free(expr);
    return NULL;
  }
  node->data.case_stmt.expr = expr;
  node->data.case_stmt.branch_count = 0;
  node->data.case_stmt.else_body = NULL;

  // Parse case branches until END_CASE or ELSE
  while (!parser_match(parser, ST_TOK_END_CASE) &&
         !parser_match(parser, ST_TOK_ELSE) &&
         !parser_match(parser, ST_TOK_EOF) &&
         node->data.case_stmt.branch_count < 16) {

    // Parse case label (number constant)
    if (!parser_match(parser, ST_TOK_INT)) {
      parser_error(parser, "Expected case value (integer constant)");
      break;
    }

    int32_t case_value = (int32_t)strtol(parser->current_token.value, NULL, 0);
    parser_advance(parser);

    if (!parser_expect(parser, ST_TOK_COLON)) {
      parser_error(parser, "Expected : after case value");
      break;
    }

    // Parse statements for this case (until next label, ELSE, or END_CASE)
    // Use special case parser that stops at case labels
    st_ast_node_t *case_body = st_parser_parse_statements_for_case(parser);

    // Store case branch
    node->data.case_stmt.branches[node->data.case_stmt.branch_count].value = case_value;
    node->data.case_stmt.branches[node->data.case_stmt.branch_count].body = case_body;
    node->data.case_stmt.branch_count++;
  }

  // Parse optional ELSE clause
  if (parser_match(parser, ST_TOK_ELSE)) {
    parser_advance(parser);

    if (!parser_expect(parser, ST_TOK_COLON)) {
      parser_error(parser, "Expected : after ELSE");
    } else {
      node->data.case_stmt.else_body = st_parser_parse_statements(parser);
    }
  }

  if (!parser_expect(parser, ST_TOK_END_CASE)) {
    parser_error(parser, "Expected END_CASE");
  }

  // Consume optional semicolon
  if (parser_match(parser, ST_TOK_SEMICOLON)) {
    parser_advance(parser);
  }

  return node;
}

/* Parse FOR loop: FOR i := start TO end [BY step] DO ... END_FOR */
static st_ast_node_t *parser_parse_for_statement(st_parser_t *parser) {
  uint32_t line = parser->current_token.line;

  parser_expect(parser, ST_TOK_FOR);

  if (!parser_match(parser, ST_TOK_IDENT)) {
    parser_error(parser, "Expected loop variable");
    return NULL;
  }

  char var_name[64] = {0};
  // BUG-032 FIX: Use strncpy to prevent buffer overflow
  strncpy(var_name, parser->current_token.value, 63);
  var_name[63] = '\0';
  parser_advance(parser);

  if (!parser_expect(parser, ST_TOK_ASSIGN)) {
    parser_error(parser, "Expected := in FOR loop");
    return NULL;
  }

  st_ast_node_t *start = parser_parse_expression(parser);
  if (!start) return NULL;

  if (!parser_expect(parser, ST_TOK_TO)) {
    parser_error(parser, "Expected TO in FOR loop");
    st_ast_node_free(start);
    return NULL;
  }

  st_ast_node_t *end = parser_parse_expression(parser);
  if (!end) {
    st_ast_node_free(start);
    return NULL;
  }

  st_ast_node_t *step = NULL;
  if (parser_match(parser, ST_TOK_BY)) {
    parser_advance(parser);
    step = parser_parse_expression(parser);
  }

  if (!parser_expect(parser, ST_TOK_DO)) {
    parser_error(parser, "Expected DO in FOR loop");
    st_ast_node_free(start);
    st_ast_node_free(end);
    st_ast_node_free(step);
    return NULL;
  }

  st_ast_node_t *body = st_parser_parse_statements(parser);

  if (!parser_expect(parser, ST_TOK_END_FOR)) {
    parser_error(parser, "Expected END_FOR");
    st_ast_node_free(start);
    st_ast_node_free(end);
    st_ast_node_free(step);
    st_ast_node_free(body);
    return NULL;
  }

  // Consume optional semicolon
  if (parser_match(parser, ST_TOK_SEMICOLON)) {
    parser_advance(parser);
  }

  st_ast_node_t *node = ast_node_alloc(ST_AST_FOR, line);
  // BUG-066: Check malloc failure
  if (!node) {
    parser_error(parser, "Out of memory");
    st_ast_node_free(start);
    st_ast_node_free(end);
    st_ast_node_free(step);
    st_ast_node_free(body);
    return NULL;
  }
  // BUG-032 FIX: Use strncpy to prevent buffer overflow
  strncpy(node->data.for_stmt.var_name, var_name, 63);
  node->data.for_stmt.var_name[63] = '\0';
  node->data.for_stmt.start = start;
  node->data.for_stmt.end = end;
  node->data.for_stmt.step = step;
  node->data.for_stmt.body = body;

  return node;
}

/* Parse WHILE loop: WHILE expr DO ... END_WHILE */
static st_ast_node_t *parser_parse_while_statement(st_parser_t *parser) {
  uint32_t line = parser->current_token.line;

  parser_expect(parser, ST_TOK_WHILE);

  st_ast_node_t *condition = parser_parse_expression(parser);
  if (!condition) return NULL;

  if (!parser_expect(parser, ST_TOK_DO)) {
    parser_error(parser, "Expected DO in WHILE loop");
    st_ast_node_free(condition);
    return NULL;
  }

  st_ast_node_t *body = st_parser_parse_statements(parser);

  if (!parser_expect(parser, ST_TOK_END_WHILE)) {
    parser_error(parser, "Expected END_WHILE");
    st_ast_node_free(condition);
    st_ast_node_free(body);
    return NULL;
  }

  // Consume optional semicolon
  if (parser_match(parser, ST_TOK_SEMICOLON)) {
    parser_advance(parser);
  }

  st_ast_node_t *node = ast_node_alloc(ST_AST_WHILE, line);
  // BUG-066: Check malloc failure
  if (!node) {
    parser_error(parser, "Out of memory");
    st_ast_node_free(condition);
    st_ast_node_free(body);
    return NULL;
  }
  node->data.while_stmt.condition = condition;
  node->data.while_stmt.body = body;

  return node;
}

/* Parse REPEAT loop: REPEAT ... UNTIL expr END_REPEAT */
static st_ast_node_t *parser_parse_repeat_statement(st_parser_t *parser) {
  uint32_t line = parser->current_token.line;

  parser_expect(parser, ST_TOK_REPEAT);

  st_ast_node_t *body = st_parser_parse_statements(parser);

  if (!parser_expect(parser, ST_TOK_UNTIL)) {
    parser_error(parser, "Expected UNTIL in REPEAT loop");
    st_ast_node_free(body);
    return NULL;
  }

  st_ast_node_t *condition = parser_parse_expression(parser);
  if (!condition) {
    st_ast_node_free(body);
    return NULL;
  }

  if (!parser_expect(parser, ST_TOK_END_REPEAT)) {
    parser_error(parser, "Expected END_REPEAT");
    st_ast_node_free(body);
    st_ast_node_free(condition);
    return NULL;
  }

  // Consume optional semicolon
  if (parser_match(parser, ST_TOK_SEMICOLON)) {
    parser_advance(parser);
  }

  st_ast_node_t *node = ast_node_alloc(ST_AST_REPEAT, line);
  // BUG-066: Check malloc failure
  if (!node) {
    parser_error(parser, "Out of memory");
    st_ast_node_free(body);
    st_ast_node_free(condition);
    return NULL;
  }
  node->data.repeat_stmt.body = body;
  node->data.repeat_stmt.condition = condition;

  return node;
}

/* Parse single statement */
st_ast_node_t *st_parser_parse_statement(st_parser_t *parser) {
  if (parser_match(parser, ST_TOK_IF)) {
    return parser_parse_if_statement(parser);
  } else if (parser_match(parser, ST_TOK_CASE)) {
    return parser_parse_case_statement(parser);
  } else if (parser_match(parser, ST_TOK_FOR)) {
    return parser_parse_for_statement(parser);
  } else if (parser_match(parser, ST_TOK_WHILE)) {
    return parser_parse_while_statement(parser);
  } else if (parser_match(parser, ST_TOK_REPEAT)) {
    return parser_parse_repeat_statement(parser);
  } else if (parser_match(parser, ST_TOK_IDENT)) {
    return parser_parse_assignment(parser);
  } else if (parser_match(parser, ST_TOK_EXIT)) {
    uint32_t line = parser->current_token.line;
    parser_advance(parser);
    if (parser_match(parser, ST_TOK_SEMICOLON)) {
      parser_advance(parser);
    }
    st_ast_node_t *node = ast_node_alloc(ST_AST_EXIT, line);
    // BUG-066: Check malloc failure
    if (!node) {
      parser_error(parser, "Out of memory");
      return NULL;
    }
    return node;
  } else {
    return NULL;  // Not a statement
  }
}

/* Parse statement list for CASE branches (stops at next case label or END_CASE) */
static st_ast_node_t *st_parser_parse_statements_for_case(st_parser_t *parser) {
  st_ast_node_t *head = NULL;
  st_ast_node_t *tail = NULL;

  while (!parser_match(parser, ST_TOK_EOF) &&
         !parser_match(parser, ST_TOK_END_CASE) &&
         !parser_match(parser, ST_TOK_ELSE)) {

    // Check for next case label: INT token followed by COLON
    // Use peek_token to look ahead without consuming tokens
    if (parser_match(parser, ST_TOK_INT) &&
        parser->peek_token.type == ST_TOK_COLON) {
      // This is a case label! Stop parsing and return
      break;
    }

    st_ast_node_t *stmt = st_parser_parse_statement(parser);
    if (!stmt) {
      // Skip to next semicolon to recover
      while (!parser_match(parser, ST_TOK_SEMICOLON) && !parser_match(parser, ST_TOK_EOF)) {
        parser_advance(parser);
      }
      if (parser_match(parser, ST_TOK_SEMICOLON)) {
        parser_advance(parser);
      }
      continue;
    }

    if (!head) {
      head = stmt;
      tail = stmt;
    } else {
      tail->next = stmt;
      tail = stmt;
    }
  }

  return head;
}

/* Parse statement list */
st_ast_node_t *st_parser_parse_statements(st_parser_t *parser) {
  st_ast_node_t *head = NULL;
  st_ast_node_t *tail = NULL;

  while (!parser_match(parser, ST_TOK_EOF) &&
         !parser_match(parser, ST_TOK_END_IF) &&
         !parser_match(parser, ST_TOK_ELSE) &&
         !parser_match(parser, ST_TOK_ELSIF) &&
         !parser_match(parser, ST_TOK_END_CASE) &&
         !parser_match(parser, ST_TOK_END_FOR) &&
         !parser_match(parser, ST_TOK_END_WHILE) &&
         !parser_match(parser, ST_TOK_END_REPEAT) &&
         !parser_match(parser, ST_TOK_END_PROGRAM) &&
         !parser_match(parser, ST_TOK_END)) {

    st_ast_node_t *stmt = st_parser_parse_statement(parser);
    if (!stmt) {
      // Skip to next semicolon to recover
      while (!parser_match(parser, ST_TOK_SEMICOLON) && !parser_match(parser, ST_TOK_EOF)) {
        parser_advance(parser);
      }
      if (parser_match(parser, ST_TOK_SEMICOLON)) {
        parser_advance(parser);
      }
      continue;
    }

    if (!head) {
      head = stmt;
      tail = stmt;
    } else {
      tail->next = stmt;
      tail = stmt;
    }
  }

  return head;
}

/* ============================================================================
 * VARIABLE DECLARATION PARSING
 * ============================================================================ */

bool st_parser_parse_var_declarations(st_parser_t *parser, st_variable_decl_t *variables, uint8_t *var_count) {
  *var_count = 0;

  while (parser_match(parser, ST_TOK_VAR) || parser_match(parser, ST_TOK_VAR_INPUT) || parser_match(parser, ST_TOK_VAR_OUTPUT)) {
    st_token_type_t var_type_token = parser->current_token.type;
    parser_advance(parser);

    // Parse variable declarations until END_VAR
    while (!parser_match(parser, ST_TOK_END_VAR) &&
           !parser_match(parser, ST_TOK_VAR) &&
           !parser_match(parser, ST_TOK_VAR_INPUT) &&
           !parser_match(parser, ST_TOK_VAR_OUTPUT) &&
           !parser_match(parser, ST_TOK_END_PROGRAM) &&
           !parser_match(parser, ST_TOK_BEGIN) &&
           !parser_match(parser, ST_TOK_EOF)) {

      // Expect identifier
      if (!parser_match(parser, ST_TOK_IDENT)) {
        if (parser_match(parser, ST_TOK_VAR_INPUT) || parser_match(parser, ST_TOK_VAR_OUTPUT)) {
          break;  // Next VAR block
        }
        parser_error(parser, "Expected variable name");
        return false;
      }

      // BUG-033 FIX: Check bounds BEFORE incrementing to prevent buffer overflow
      if (*var_count >= 32) {
        parser_error(parser, "Too many variables (max 32)");
        return false;
      }
      st_variable_decl_t *var = &variables[(*var_count)++];
      // BUG-032 FIX: Use strncpy to prevent buffer overflow (name is 64 bytes, token is 256)
      strncpy(var->name, parser->current_token.value, 63);
      var->name[63] = '\0';
      parser_advance(parser);

      // Expect colon
      if (!parser_expect(parser, ST_TOK_COLON)) {
        parser_error(parser, "Expected : after variable name");
        return false;
      }

      // Expect data type
      st_datatype_t datatype = ST_TYPE_NONE;
      if (parser_match(parser, ST_TOK_BOOL)) {
        datatype = ST_TYPE_BOOL;
        parser_advance(parser);
      } else if (parser_match(parser, ST_TOK_INT_KW)) {
        datatype = ST_TYPE_INT;
        parser_advance(parser);
      } else if (parser_match(parser, ST_TOK_DWORD)) {
        datatype = ST_TYPE_DWORD;
        parser_advance(parser);
      } else if (parser_match(parser, ST_TOK_REAL_KW)) {
        datatype = ST_TYPE_REAL;
        parser_advance(parser);
      } else {
        parser_error(parser, "Expected data type (BOOL, INT, DWORD, REAL)");
        return false;
      }

      var->type = datatype;
      var->is_input = (var_type_token == ST_TOK_VAR_INPUT);
      var->is_output = (var_type_token == ST_TOK_VAR_OUTPUT);

      // Optional initial value
      if (parser_match(parser, ST_TOK_ASSIGN)) {
        parser_advance(parser);
        st_ast_node_t *init_expr = parser_parse_expression(parser);
        if (init_expr && init_expr->type == ST_AST_LITERAL) {
          var->initial_value = init_expr->data.literal.value;
          st_ast_node_free(init_expr);
        }
      }

      // Expect semicolon
      if (parser_match(parser, ST_TOK_SEMICOLON)) {
        parser_advance(parser);
      }

      if (*var_count >= 32) {
        parser_error(parser, "Too many variables (max 32)");
        return false;
      }
    }

    // Expect END_VAR to close the VAR block
    if (parser_match(parser, ST_TOK_END_VAR)) {
      parser_advance(parser);
    } else if (!parser_match(parser, ST_TOK_EOF) &&
               !parser_match(parser, ST_TOK_END_PROGRAM) &&
               !parser_match(parser, ST_TOK_BEGIN)) {
      // Only error if not at EOF, END_PROGRAM, or BEGIN (multiple VAR blocks allowed)
      parser_error(parser, "Expected END_VAR to close variable declaration block");
      return false;
    }
  }

  return true;
}

/* ============================================================================
 * MAIN PROGRAM PARSING
 * ============================================================================ */

st_program_t *st_parser_parse_program(st_parser_t *parser) {
  st_program_t *program = (st_program_t *)malloc(sizeof(st_program_t));
  if (!program) return NULL;

  memset(program, 0, sizeof(*program));
  strncpy(program->name, "Logic", sizeof(program->name) - 1);  // Default program name
  program->name[sizeof(program->name) - 1] = '\0';

  bool has_program_keyword = false;
  bool has_begin_keyword = false;

  // Optional: Parse PROGRAM <identifier>
  if (parser_match(parser, ST_TOK_PROGRAM)) {
    has_program_keyword = true;
    parser_advance(parser);

    // Expect program name (identifier)
    if (parser_match(parser, ST_TOK_IDENT)) {
      strncpy(program->name, parser->current_token.value, 63);
      program->name[63] = '\0';
      parser_advance(parser);
    } else {
      parser_error(parser, "Expected program name after PROGRAM keyword");
      free(program);
      return NULL;
    }

    // Optional semicolon after PROGRAM name
    if (parser_match(parser, ST_TOK_SEMICOLON)) {
      parser_advance(parser);
    }
  }

  // Parse variable declarations
  if (!st_parser_parse_var_declarations(parser, program->variables, &program->var_count)) {
    free(program);
    return NULL;
  }

  // Optional: Parse BEGIN keyword
  if (parser_match(parser, ST_TOK_BEGIN)) {
    has_begin_keyword = true;
    parser_advance(parser);
  }

  // Parse statements
  program->body = st_parser_parse_statements(parser);

  // Optional: Parse END or END_PROGRAM if PROGRAM or BEGIN was used
  if (has_program_keyword || has_begin_keyword) {
    if (parser_match(parser, ST_TOK_END)) {
      parser_advance(parser);
    } else if (parser_match(parser, ST_TOK_END_PROGRAM)) {
      parser_advance(parser);
    }
    // Note: For backward compatibility, END/END_PROGRAM is optional even if PROGRAM/BEGIN was present

    // Optional semicolon after END
    if (parser_match(parser, ST_TOK_SEMICOLON)) {
      parser_advance(parser);
    }
  }

  if (parser->error_count > 0) {
    st_program_free(program);
    return NULL;
  }

  return program;
}

/* ============================================================================
 * DEBUGGING
 * ============================================================================ */

void st_ast_node_print(st_ast_node_t *node, int indent) {
  if (!node) return;

  char padding[128] = {0};
  for (int i = 0; i < indent && i < 120; i++) padding[i] = ' ';

  switch (node->type) {
    case ST_AST_ASSIGNMENT:
      debug_printf("%sASSIGN: %s :=\n", padding, node->data.assignment.var_name);
      st_ast_node_print(node->data.assignment.expr, indent + 2);
      break;

    case ST_AST_LITERAL:
      debug_printf("%sLITERAL: ", padding);
      switch (node->data.literal.type) {
        case ST_TYPE_BOOL: debug_printf("BOOL(%d)\n", node->data.literal.value.bool_val); break;
        case ST_TYPE_INT: debug_printf("INT(%d)\n", node->data.literal.value.int_val); break;
        case ST_TYPE_DWORD: debug_printf("DWORD(%u)\n", node->data.literal.value.dword_val); break;
        case ST_TYPE_REAL: debug_printf("REAL(%f)\n", node->data.literal.value.real_val); break;
        default: debug_printf("?\n");
      }
      break;

    case ST_AST_VARIABLE:
      debug_printf("%sVAR: %s\n", padding, node->data.variable.var_name);
      break;

    case ST_AST_BINARY_OP:
      debug_printf("%sBINOP: %s\n", padding, st_token_type_to_string(node->data.binary_op.op));
      st_ast_node_print(node->data.binary_op.left, indent + 2);
      st_ast_node_print(node->data.binary_op.right, indent + 2);
      break;

    case ST_AST_IF:
      debug_printf("%sIF\n", padding);
      debug_printf("%s  CONDITION:\n", padding);
      st_ast_node_print(node->data.if_stmt.condition_expr, indent + 4);
      debug_printf("%s  THEN:\n", padding);
      st_ast_node_print(node->data.if_stmt.then_body, indent + 4);
      if (node->data.if_stmt.else_body) {
        debug_printf("%s  ELSE:\n", padding);
        st_ast_node_print(node->data.if_stmt.else_body, indent + 4);
      }
      break;

    case ST_AST_FUNCTION_CALL:
      debug_printf("%sCALL: %s(", padding, node->data.function_call.func_name);
      for (uint8_t i = 0; i < node->data.function_call.arg_count; i++) {
        debug_printf("%s", i > 0 ? ", " : "");
      }
      debug_printf(")\n");
      for (uint8_t i = 0; i < node->data.function_call.arg_count; i++) {
        debug_printf("%s  ARG%d:\n", padding, i);
        st_ast_node_print(node->data.function_call.args[i], indent + 4);
      }
      break;

    default:
      debug_printf("%s[%s]\n", padding, "node");
  }

  if (node->next) {
    st_ast_node_print(node->next, indent);
  }
}

/* Implementation of public API */
st_ast_node_t *st_parser_parse_expression(st_parser_t *parser) {
  return parser_parse_expression(parser);
}
