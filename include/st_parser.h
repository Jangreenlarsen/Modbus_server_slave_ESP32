/**
 * @file st_parser.h
 * @brief Structured Text Parser
 *
 * Converts token stream into Abstract Syntax Tree (AST).
 * Implements IEC 61131-3 grammar (recursive descent parser).
 *
 * Usage:
 *   st_parser_t parser;
 *   st_parser_init(&parser, "VAR x: INT; END_VAR IF x > 0 THEN x := 1; END_IF;");
 *   st_program_t *program = st_parser_parse_program(&parser);
 */

#ifndef ST_PARSER_H
#define ST_PARSER_H

#include "st_types.h"
#include "st_lexer.h"

/* BUG-157 FIX: Maximum recursion depth to prevent stack overflow */
#define ST_MAX_RECURSION_DEPTH 32

/* Parser state machine */
typedef struct {
  st_lexer_t lexer;
  st_token_t current_token;
  st_token_t peek_token;
  uint32_t error_count;
  char error_msg[256];
  uint8_t recursion_depth;  // BUG-157 FIX: Track recursion depth to prevent stack overflow
} st_parser_t;

/**
 * @brief Initialize parser with input string
 * @param parser Parser state
 * @param input Source code
 */
void st_parser_init(st_parser_t *parser, const char *input);

/**
 * @brief Parse complete ST program
 * @param parser Parser state
 * @return Parsed program (allocated), NULL on error
 */
st_program_t *st_parser_parse_program(st_parser_t *parser);

/**
 * @brief Parse variable declarations (VAR ... END_VAR)
 * @param parser Parser state
 * @param variables Output variable array
 * @param var_count Output variable count
 * @return true if successful
 */
bool st_parser_parse_var_declarations(st_parser_t *parser, st_variable_decl_t *variables, uint8_t *var_count);

/**
 * @brief Parse statement list
 * @param parser Parser state
 * @return Root AST node (linked list of statements), NULL on error
 */
st_ast_node_t *st_parser_parse_statements(st_parser_t *parser);

/**
 * @brief Parse single statement
 * @param parser Parser state
 * @return AST node for statement, NULL on error
 */
st_ast_node_t *st_parser_parse_statement(st_parser_t *parser);

/**
 * @brief Parse expression
 * @param parser Parser state
 * @return AST node for expression, NULL on error
 */
st_ast_node_t *st_parser_parse_expression(st_parser_t *parser);

/**
 * @brief Free AST node (recursive)
 * @param node Root node to free
 */
void st_ast_node_free(st_ast_node_t *node);

/**
 * @brief Free entire program
 * @param program Program to free
 */
void st_program_free(st_program_t *program);

/**
 * @brief Print AST for debugging
 * @param node Root node
 * @param indent Indentation level
 */
void st_ast_node_print(st_ast_node_t *node, int indent);

#endif // ST_PARSER_H
