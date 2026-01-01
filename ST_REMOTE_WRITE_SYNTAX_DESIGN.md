# ST Logic Remote Write Syntax - Design Dokument

**Dato:** 2026-01-01
**Version:** v4.6.0 (Planned Feature)
**Formål:** Design af ny assignment-baseret syntax for Modbus Master write operations

---

## 📋 Executive Summary

Nuværende Modbus Master write syntax bruger function-call stil som returnerer success BOOL:
```structured-text
success := MB_WRITE_COIL(3, 20, TRUE);     (* 3 arguments, returns BOOL *)
success := MB_WRITE_HOLDING(1, 100, 500);  (* 3 arguments, returns BOOL *)
```

**Ønsket ny syntax** (mere IEC 61131-3 compliant):
```structured-text
MB_WRITE_COIL(3, 20) := TRUE;       (* 2 arguments, value from RHS *)
MB_WRITE_HOLDING(1, 100) := 500;    (* 2 arguments, value from RHS *)
```

**Fordele:**
- ✅ Mere intuitiv (ligner variable assignments)
- ✅ Symmetrisk med READ operations (`temp := MB_READ_HOLDING(1, 100)`)
- ✅ IEC 61131-3 stil for remote I/O
- ✅ Separerer adresse-specifikation fra værdi

**Ulemper:**
- ❌ Breaking change (gammel syntax skal stadig understøttes for kompatibilitet)
- ❌ Kræver parser, compiler og VM ændringer
- ❌ Success flag skal tilgås via global variable (`mb_success`)

---

## 🎯 Requirements

### FR-001: Support Assignment Syntax
**Beskrivelse:** ST Logic skal understøtte assignment-baseret syntax for remote write operations.

**Syntax:**
```structured-text
MB_WRITE_COIL(slave_id, address) := boolean_value;
MB_WRITE_HOLDING(slave_id, address) := int_value;
```

**Constraints:**
- `slave_id`: INT (1-247)
- `address`: INT (0-65535)
- `boolean_value`: BOOL expression
- `int_value`: INT/DINT/DWORD expression (konverteres til 16-bit)

### FR-002: Backward Compatibility
**Beskrivelse:** Gammel function-call syntax skal stadig virke.

**Syntax (gammel):**
```structured-text
success := MB_WRITE_COIL(3, 20, TRUE);
success := MB_WRITE_HOLDING(1, 100, 500);
```

**Krav:**
- Parser skal genkende begge syntaxer
- Compiler skal generere korrekt bytecode for begge
- VM skal eksekvere begge korrekt

### FR-003: Type Validation
**Beskrivelse:** Compiler skal validere type match mellem function og value.

**Rules:**
```structured-text
MB_WRITE_COIL(id, addr) := BOOL_EXPR;      (* ✅ OK *)
MB_WRITE_COIL(id, addr) := INT_EXPR;       (* ❌ ERROR: Type mismatch *)

MB_WRITE_HOLDING(id, addr) := INT_EXPR;    (* ✅ OK *)
MB_WRITE_HOLDING(id, addr) := DINT_EXPR;   (* ✅ OK - truncate to INT *)
MB_WRITE_HOLDING(id, addr) := BOOL_EXPR;   (* ❌ ERROR: Type mismatch *)
```

### FR-004: Success Feedback
**Beskrivelse:** Brugere skal kunne tjekke om write lykkedes.

**Method:**
```structured-text
MB_WRITE_COIL(3, 20) := TRUE;

IF mb_success THEN
  (* Write succeeded *)
ELSE
  (* Write failed - check mb_last_error *)
END_IF;
```

**Global Variables:**
- `mb_success` (BOOL) - TRUE hvis sidste operation lykkedes
- `mb_last_error` (INT) - Error code (0=OK, 1=TIMEOUT, 2=CRC, etc.)

---

## 🏗️ Architecture Design

### Option 1: New AST Node Type (ANBEFALET)

**Ny AST Node:**
```c
// In include/st_types.h
typedef enum {
  // ... existing types ...
  ST_AST_REMOTE_WRITE,      // MB_WRITE_XXX(args) := value
} st_ast_node_type_t;

typedef struct {
  char func_name[64];        // "MB_WRITE_COIL" eller "MB_WRITE_HOLDING"
  st_ast_node_t *slave_id;   // Slave ID expression
  st_ast_node_t *address;    // Address expression
  st_ast_node_t *value;      // Value expression (right side of :=)
  st_builtin_func_t func_id; // ST_BUILTIN_MB_WRITE_COIL eller ST_BUILTIN_MB_WRITE_HOLDING
} st_remote_write_t;

// Update st_ast_node_t union:
struct st_ast_node {
  st_ast_node_type_t type;
  uint32_t line;
  union {
    st_assignment_t assignment;
    st_if_stmt_t if_stmt;
    // ... existing ...
    st_remote_write_t remote_write;  // ← NEW
    st_function_call_t function_call;
    // ... rest ...
  } data;
  struct st_ast_node *next;
};
```

**Fordele:**
- ✅ Semantisk klar adskillelse fra normale assignments
- ✅ Lettere type checking
- ✅ Mere vedligeholdelig kode

**Ulemper:**
- ❌ Flere ændringer i codebase
- ❌ Ny node type skal håndteres i free/print/compile

### Option 2: Extend ST_AST_ASSIGNMENT

**Udvidet Assignment Node:**
```c
typedef struct {
  char var_name[64];          // Variable name (empty hvis remote write)
  st_ast_node_t *lvalue_call; // NULL hvis normal var, function call hvis remote
  st_ast_node_t *expr;        // Right hand side expression
  bool is_remote_write;       // TRUE hvis remote write
} st_assignment_t;
```

**Fordele:**
- ✅ Færre ændringer i node types
- ✅ Genbruger eksisterende assignment logik

**Ulemper:**
- ❌ Overloading af assignment semantik
- ❌ Sværere type checking
- ❌ Mindre klart intent

**ANBEFALING:** **Option 1** (New AST Node Type)

---

## 📝 Implementation Plan

### Phase 1: Parser Changes

**File:** `src/st_parser.cpp`

**Ændringer i `parser_parse_statement()`:**
```c
static st_ast_node_t *parser_parse_statement(st_parser_t *parser) {
  // ... existing IF/CASE/FOR/WHILE/REPEAT checks ...

  if (parser_match(parser, ST_TOK_IDENT)) {
    // Peek ahead to determine if it's assignment or remote write
    char ident_name[64];
    strncpy(ident_name, parser->current_token.value.string, sizeof(ident_name));
    uint32_t line = parser->current_token.line;
    parser_advance(parser);

    if (parser_match(parser, ST_TOK_LPAREN)) {
      // Function call syntax - could be remote write!
      // Check if it's MB_WRITE_COIL or MB_WRITE_HOLDING
      if (strcasecmp(ident_name, "MB_WRITE_COIL") == 0 ||
          strcasecmp(ident_name, "MB_WRITE_HOLDING") == 0) {

        // Parse arguments: (slave_id, address)
        parser_advance(); // consume '('

        st_ast_node_t *slave_id = parser_parse_expression(parser);
        if (!slave_id) return NULL;

        if (!parser_expect(parser, ST_TOK_COMMA)) {
          parser_error(parser, "Expected comma in MB_WRITE function");
          return NULL;
        }

        st_ast_node_t *address = parser_parse_expression(parser);
        if (!address) return NULL;

        if (!parser_expect(parser, ST_TOK_RPAREN)) {
          parser_error(parser, "Expected ')' in MB_WRITE function");
          return NULL;
        }

        // Check for ':=' (remote write syntax)
        if (parser_match(parser, ST_TOK_ASSIGN)) {
          parser_advance(); // consume ':='

          // Parse value expression
          st_ast_node_t *value = parser_parse_expression(parser);
          if (!value) return NULL;

          if (!parser_expect(parser, ST_TOK_SEMICOLON)) {
            parser_error(parser, "Expected ';' after remote write");
            return NULL;
          }

          // Create ST_AST_REMOTE_WRITE node
          st_ast_node_t *node = ast_node_alloc(ST_AST_REMOTE_WRITE, line);
          if (!node) {
            parser_error(parser, "Out of memory");
            return NULL;
          }

          strncpy(node->data.remote_write.func_name, ident_name, sizeof(node->data.remote_write.func_name));
          node->data.remote_write.slave_id = slave_id;
          node->data.remote_write.address = address;
          node->data.remote_write.value = value;

          // Set func_id
          if (strcasecmp(ident_name, "MB_WRITE_COIL") == 0) {
            node->data.remote_write.func_id = ST_BUILTIN_MB_WRITE_COIL;
          } else {
            node->data.remote_write.func_id = ST_BUILTIN_MB_WRITE_HOLDING;
          }

          return node;
        } else {
          // OLD SYNTAX: MB_WRITE_XXX(slave, addr, value) used as expression
          // This needs 3 arguments - parse third argument
          if (!parser_expect(parser, ST_TOK_COMMA)) {
            parser_error(parser, "Old MB_WRITE syntax requires 3 arguments");
            return NULL;
          }

          st_ast_node_t *value = parser_parse_expression(parser);
          if (!value) return NULL;

          if (!parser_expect(parser, ST_TOK_RPAREN)) {
            parser_error(parser, "Expected ')' in MB_WRITE function");
            return NULL;
          }

          // Create function call node (old syntax - returns BOOL)
          st_ast_node_t *call_node = ast_node_alloc(ST_AST_FUNCTION_CALL, line);
          if (!call_node) {
            parser_error(parser, "Out of memory");
            return NULL;
          }

          strncpy(call_node->data.function_call.func_name, ident_name, sizeof(call_node->data.function_call.func_name));
          call_node->data.function_call.args[0] = slave_id;
          call_node->data.function_call.args[1] = address;
          call_node->data.function_call.args[2] = value;
          call_node->data.function_call.arg_count = 3;

          // This is an expression, not a statement!
          // Needs to be wrapped in assignment or discarded
          parser_error(parser, "MB_WRITE function call must be assigned or use new syntax");
          return NULL;
        }
      } else {
        // Other function calls - continue as before
        // ... existing code ...
      }
    } else if (parser_match(parser, ST_TOK_ASSIGN)) {
      // Normal variable assignment
      return parser_parse_assignment_from_ident(parser, ident_name, line);
    }
  }

  // ... rest of function ...
}
```

**Ny Helper Function:**
```c
static st_ast_node_t *parser_parse_assignment_from_ident(
  st_parser_t *parser,
  const char *var_name,
  uint32_t line
) {
  // Assumes current token is ':=' and parser already advanced past ident
  parser_advance(); // consume ':='

  st_ast_node_t *expr = parser_parse_expression(parser);
  if (!expr) return NULL;

  if (!parser_expect(parser, ST_TOK_SEMICOLON)) {
    parser_error(parser, "Expected ';' after assignment");
    return NULL;
  }

  st_ast_node_t *node = ast_node_alloc(ST_AST_ASSIGNMENT, line);
  if (!node) {
    parser_error(parser, "Out of memory");
    return NULL;
  }

  strncpy(node->data.assignment.var_name, var_name, sizeof(node->data.assignment.var_name));
  node->data.assignment.expr = expr;

  return node;
}
```

### Phase 2: Compiler Changes

**File:** `src/st_compiler.cpp`

**Tilføj case i `st_compiler_compile_stmt()`:**
```c
bool st_compiler_compile_stmt(st_compiler_t *compiler, st_ast_node_t *node) {
  // ... existing cases ...

  case ST_AST_REMOTE_WRITE:
    return st_compiler_compile_remote_write(compiler, node);

  // ... rest ...
}
```

**Ny Compiler Function:**
```c
static bool st_compiler_compile_remote_write(st_compiler_t *compiler, st_ast_node_t *node) {
  // Type checking
  st_datatype_t expected_type;
  if (node->data.remote_write.func_id == ST_BUILTIN_MB_WRITE_COIL) {
    expected_type = ST_TYPE_BOOL;
  } else { // MB_WRITE_HOLDING
    expected_type = ST_TYPE_INT; // Also accepts DINT/DWORD (will truncate)
  }

  // Compile arguments onto stack:
  // 1. slave_id
  if (!st_compiler_compile_expr(compiler, node->data.remote_write.slave_id)) {
    return false;
  }

  // 2. address
  if (!st_compiler_compile_expr(compiler, node->data.remote_write.address)) {
    return false;
  }

  // 3. value (from right side of :=)
  if (!st_compiler_compile_expr(compiler, node->data.remote_write.value)) {
    return false;
  }

  // Type check value expression
  // TODO: Implement type inference for expressions
  // For now, trust user input (runtime will handle conversion)

  // Emit OP_CALL_BUILTIN opcode (same as function call, but ignore result)
  st_opcode_t opcode = {
    .op = ST_OP_CALL_BUILTIN,
    .operand1 = (uint16_t)node->data.remote_write.func_id,
    .operand2 = 3,  // Argument count
    .line = node->line
  };

  st_compiler_emit(compiler, opcode);

  // Pop result from stack (we don't use it in statement context)
  st_opcode_t pop_opcode = {
    .op = ST_OP_POP,
    .operand1 = 0,
    .operand2 = 0,
    .line = node->line
  };

  st_compiler_emit(compiler, pop_opcode);

  return true;
}
```

**Opdater `st_ast_node_free()`:**
```c
void st_ast_node_free(st_ast_node_t *node) {
  // ... existing cases ...

  case ST_AST_REMOTE_WRITE:
    st_ast_node_free(node->data.remote_write.slave_id);
    st_ast_node_free(node->data.remote_write.address);
    st_ast_node_free(node->data.remote_write.value);
    break;

  // ... rest ...
}
```

**Opdater `st_ast_node_print()` (debug):**
```c
void st_ast_node_print(st_ast_node_t *node, uint8_t indent) {
  // ... existing cases ...

  case ST_AST_REMOTE_WRITE:
    debug_printf("%sREMOTE_WRITE: %s(\n", padding, node->data.remote_write.func_name);
    debug_printf("%s  slave_id:\n", padding);
    st_ast_node_print(node->data.remote_write.slave_id, indent + 4);
    debug_printf("%s  address:\n", padding);
    st_ast_node_print(node->data.remote_write.address, indent + 4);
    debug_printf("%s) := \n", padding);
    st_ast_node_print(node->data.remote_write.value, indent + 2);
    break;

  // ... rest ...
}
```

### Phase 3: VM Changes

**File:** `src/st_vm.cpp`

**Ingen ændringer nødvendige!**

VM kører allerede `OP_CALL_BUILTIN` korrekt. Forskellen er kun at:
- Gammel syntax: Result bliver pushed til stack og brugt i assignment
- Ny syntax: Result bliver pushed til stack, men POP'd med det samme (ignoreret)

Global variables `g_mb_success` og `g_mb_last_error` opdateres af begge.

### Phase 4: Documentation Updates

**Files:**
- `README.md` - Update MB_WRITE syntax examples
- `docs/ST_USAGE_GUIDE.md` - Add new syntax section
- `CHANGELOG.md` - Document syntax change in v4.6.0

---

## 🧪 Test Cases

### Test 1: New Syntax - Basic Usage
```structured-text
VAR
  heating_on: BOOL;
END_VAR

heating_on := TRUE;
MB_WRITE_COIL(3, 20) := heating_on;

IF mb_success THEN
  (* Write succeeded *)
END_IF;
```

**Expected:**
- ✅ Compiles successfully
- ✅ Sends Modbus write request
- ✅ `mb_success` set correctly

### Test 1b: Variable Arguments (slave_id og address)
```structured-text
VAR
  REMOTE_IO: INT := 3;
  COIL_ADDR: INT := 20;
  heating_on: BOOL;
END_VAR

heating_on := TRUE;
MB_WRITE_COIL(REMOTE_IO, COIL_ADDR) := heating_on;

IF mb_success THEN
  (* Write succeeded to slave 3, coil 20 *)
END_IF;
```

**Expected:**
- ✅ Compiles successfully (variabler i argumenter understøttes)
- ✅ Evaluerer REMOTE_IO → 3, COIL_ADDR → 20 ved runtime
- ✅ Sends Modbus write request til korrekt slave/adresse

### Test 2: New Syntax - Inline Expression
```structured-text
MB_WRITE_HOLDING(1, 100) := 500 + 100;
```

**Expected:**
- ✅ Compiles successfully
- ✅ Expression evaluated to 600
- ✅ Writes 600 to remote register

### Test 2b: Dynamic Address Calculation
```structured-text
VAR
  REMOTE_SLAVE: INT := 1;
  BASE_ADDR: INT := 100;
  OFFSET: INT := 5;
  value: INT := 750;
END_VAR

(* Dynamic address: 100 + 5 = 105 *)
MB_WRITE_HOLDING(REMOTE_SLAVE, BASE_ADDR + OFFSET) := value;
```

**Expected:**
- ✅ Compiles successfully (expressions i argumenter)
- ✅ Address beregnes til 105 ved runtime
- ✅ Writes value til register 105 på slave 1

### Test 2c: Loop med Variable Addressing
```structured-text
VAR
  i: INT;
  SLAVE_ID: INT := 2;
  START_ADDR: INT := 200;
  values: ARRAY[0..9] OF INT; (* Future: ARRAY support *)
END_VAR

(* Write 10 registers sequentially *)
FOR i := 0 TO 9 DO
  MB_WRITE_HOLDING(SLAVE_ID, START_ADDR + i) := i * 10;
  (* Writes: 0, 10, 20, 30, ..., 90 til registers 200-209 *)
END_FOR;
```

**Expected:**
- ✅ Compiles successfully
- ✅ Loop kører 10 gange
- ✅ Hver iteration sender write til korrekt address (200+i)

### Test 3: Old Syntax - Backward Compatibility
```structured-text
VAR
  success: BOOL;
END_VAR

success := MB_WRITE_COIL(3, 20, TRUE);

IF success THEN
  (* Write succeeded *)
END_IF;
```

**Expected:**
- ✅ Compiles successfully (backward compatible)
- ✅ `success` variable contains result
- ✅ Sends Modbus write request

### Test 4: Type Mismatch Error
```structured-text
MB_WRITE_COIL(3, 20) := 123;  (* INT instead of BOOL *)
```

**Expected:**
- ❌ Compile error: "Type mismatch: MB_WRITE_COIL expects BOOL"

### Test 5: Read/Write Symmetry
```structured-text
VAR
  temp: INT;
END_VAR

temp := MB_READ_HOLDING(1, 100);   (* Read *)
MB_WRITE_HOLDING(1, 100) := temp;  (* Write *)
```

**Expected:**
- ✅ Compiles successfully
- ✅ Read returns value in `temp`
- ✅ Write sends `temp` value to remote

---

## 📊 Impact Analysis

### Files to Modify

| File | Changes | Complexity | Lines Changed |
|------|---------|-----------|---------------|
| `include/st_types.h` | Add `ST_AST_REMOTE_WRITE` node type | LOW | ~20 |
| `src/st_parser.cpp` | Parse new syntax, maintain backward compat | HIGH | ~100 |
| `src/st_compiler.cpp` | Compile remote write nodes | MEDIUM | ~60 |
| `src/st_vm.cpp` | **No changes** (reuse existing opcodes) | NONE | 0 |
| `README.md` | Update syntax examples | LOW | ~10 |
| `docs/ST_USAGE_GUIDE.md` | Document new syntax | LOW | ~30 |
| `CHANGELOG.md` | Add v4.6.0 entry | LOW | ~5 |

**Total Estimated Lines:** ~225 lines

### Backward Compatibility

**Breaking Changes:** ❌ NONE

**Migration Path:**
- Gammel syntax (`success := MB_WRITE_COIL(3, 20, TRUE)`) virker stadig
- Ny syntax (`MB_WRITE_COIL(3, 20) := TRUE`) er optional
- Brugere kan gradvist migrere deres kode

### Performance Impact

**Runtime:** ✅ INGEN impact (samme bytecode eksekvering)

**Compile Time:** ✅ MINIMAL impact (ekstra parser check)

**Memory:** ✅ INGEN ekstra RAM usage (samme antal opcodes)

---

## ⚠️ Risks & Mitigation

### Risk 1: Parser Ambiguity
**Issue:** Parser skal skelne mellem:
- `MB_WRITE_COIL(3, 20)` (statement, 2 args) vs
- `MB_WRITE_COIL(3, 20, TRUE)` (expression, 3 args)

**Mitigation:**
- Parse function call først
- Check antal argumenter EFTER ')'
- Check for ':=' token for at bestemme intent

### Risk 2: Type Checking Complexity
**Issue:** Compiler skal validere type match mellem function og value expression.

**Mitigation:**
- Implement basic type inference for expressions
- Runtime will handle type coercion (DINT → INT truncation)
- Clear error messages for obvious mismatches

### Risk 3: Regression i Gammel Syntax
**Issue:** Ændringer kan bryde eksisterende kode der bruger gammel syntax.

**Mitigation:**
- Ekstensiv testing af backward compatibility
- Test suite med gamle syntax eksempler
- Beta release før production deploy

---

## 📅 Implementation Roadmap

### Sprint 1: Core Implementation (v4.6.0)
**Estimat:** 2 dage

**Tasks:**
1. ✅ Add `ST_AST_REMOTE_WRITE` node type til `st_types.h` (30 min)
2. ✅ Implement parser changes i `st_parser.cpp` (4 timer)
3. ✅ Implement compiler changes i `st_compiler.cpp` (3 timer)
4. ✅ Update AST helper functions (free, print) (1 time)
5. ✅ Write test cases (2 timer)
6. ✅ Test backward compatibility (1 time)

### Sprint 2: Documentation & Polish (v4.6.1)
**Estimat:** 1 dag

**Tasks:**
1. ✅ Update `README.md` med nye syntax eksempler (1 time)
2. ✅ Update `ST_USAGE_GUIDE.md` med section om remote writes (2 timer)
3. ✅ Update `CHANGELOG.md` (30 min)
4. ✅ Create migration guide for users (1 time)
5. ✅ Final testing og bug fixes (2 timer)

**Total Effort:** 3 dage

---

## ✅ Success Criteria

### Must Have (v4.6.0)
- ✅ `MB_WRITE_COIL(id, addr) := value` syntax works
- ✅ `MB_WRITE_HOLDING(id, addr) := value` syntax works
- ✅ Backward compatibility with old 3-argument syntax
- ✅ Type validation (BOOL for coil, INT for holding)
- ✅ `mb_success` and `mb_last_error` opdateres korrekt

### Should Have (v4.6.1)
- ✅ Comprehensive documentation
- ✅ Migration guide
- ✅ Clear error messages for type mismatches
- ✅ Test suite coverage

### Nice to Have (v4.7.0+)
- ✅ Support for multi-register writes (DINT/REAL)
- ✅ Deprecation warning for old syntax
- ✅ Auto-migration tool

---

## 📝 Example Migration

### Before (Old Syntax)
```structured-text
VAR
  heating_success: BOOL;
  valve_success: BOOL;
  setpoint_success: BOOL;
END_VAR

heating_success := MB_WRITE_COIL(3, 20, TRUE);
valve_success := MB_WRITE_COIL(3, 21, FALSE);
setpoint_success := MB_WRITE_HOLDING(1, 100, 750);

IF NOT heating_success THEN
  (* Handle error *)
END_IF;
```

### After (New Syntax)
```structured-text
MB_WRITE_COIL(3, 20) := TRUE;
IF NOT mb_success THEN
  (* Handle error *)
END_IF;

MB_WRITE_COIL(3, 21) := FALSE;
IF NOT mb_success THEN
  (* Handle error *)
END_IF;

MB_WRITE_HOLDING(1, 100) := 750;
IF NOT mb_success THEN
  (* Handle error *)
END_IF;
```

**Benefits:**
- ✅ Mere læsbar (klart adskiller adresse fra værdi)
- ✅ Symmetrisk med read operations
- ✅ Færre variabler nødvendige
- ✅ IEC 61131-3 compliant stil

---

## 🔗 Related Work

**Dependencies:**
- ✅ BUG-133 fix (request counter reset) - SKAL fixes først!
- ✅ ENHANCE-001 (retry logic) - OPTIONAL men synergistisk

**Future Enhancements:**
- 📦 Multi-register write support (v5.0+)
- 🔤 STRING type write support (v5.0+)
- 📐 ARRAY bulk write support (v5.1+)

---

**Designet af:** Claude Sonnet 4.5
**Review Status:** ✅ KOMPLET
**Ready for Implementation:** JA (efter BUG-133 fix)
**Target Release:** v4.6.0
