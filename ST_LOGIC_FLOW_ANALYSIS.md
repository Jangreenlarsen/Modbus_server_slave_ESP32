# ST Logic - Complete Flow Analysis & Issues

**Dato:** 2025-12-03
**Status:** CRITICAL ANALYSIS - Multiple Issues Found

---

## 🎯 Problem Statement

User reports successful compilation but binding fails:

```
>>> set logic 1 upload
✓ COMPILATION SUCCESSFUL  ← Says compiled

>>> set logic 1 bind timer reg:100
ERROR: Program not compiled  ← But says NOT compiled!
```

This is a critical state inconsistency.

---

## 📊 Complete ST Logic Flow

### Phase 1: Upload (`set logic 1 upload`)
```
User Input:    "set logic 1 upload"
         ↓
cli_parser.cpp:401-444
  → Recognize "logic" command
  → Extract program_id = 1 (converted to 0)
  → Call: cli_shell_start_st_upload(0)
         ↓
cli_shell.cpp:115-121
  → Set cli_mode = CLI_MODE_ST_UPLOAD
  → Set cli_upload_program_id = 0
  → Clear upload buffer
  → Show prompt ">>> "
```

### Phase 2: Line Collection
```
Lines typed by user:
  >>> VAR
  >>> state: INT;
  >>> red: BOOL;
  >>> ... (more lines)
  >>> END_UPLOAD
         ↓
cli_shell.cpp:338-384
  For each line:
    1. User enters line, presses ENTER
    2. System reads into cli_input_buffer
    3. Create trimmed_check copy (after FIX)
    4. Check if trimmed_check == "END_UPLOAD"
    5. If NOT END_UPLOAD: copy original to upload buffer
    6. If END_UPLOAD: call cli_parser_execute_st_upload()
```

### Phase 3: Compilation
```
cli_parser_execute_st_upload(program_id=0, source_code=<collected>)
  (cli_parser.cpp:626-647)
         ↓
cli_cmd_set_logic_upload(logic_state, 0, source_code)
  (cli_commands_logic.cpp:50-97)
         ↓
st_logic_upload(logic_state, 0, source_code, size)
  (st_logic_config.cpp:55-66)
  → Store source_code in g_logic_state.programs[0].source_code
  → Set programs[0].compiled = 0  ← RESET!
         ↓
st_logic_compile(logic_state, 0)
  (st_logic_config.cpp:68-107)
  → Parse source
  → Compile to bytecode
  → IF SUCCESS: Set programs[0].compiled = 1 ✓
  → IF FAIL: Return false, keep compiled=0
         ↓
Output: "COMPILATION SUCCESSFUL"
```

### Phase 4: Binding
```
User: "set logic 1 bind timer reg:100"
         ↓
cli_parser.cpp:460-470
  → Extract program_id = 1 → Convert to 0
  → Extract var_name = "timer"
  → Extract binding = "reg:100"
         ↓
cli_cmd_set_logic_bind_by_name(logic_state, 0, "timer", "reg:100")
  (cli_commands_logic.cpp:162-200)
         ↓
  LINE 169: prog = st_logic_get_program(logic_state, 0)
           → Returns &g_logic_state.programs[0]

  LINE 170: if (!prog || !prog->compiled)
           → Check if prog is NULL (it's not)
           → Check if prog->compiled == 0 ← HERE'S THE PROBLEM!
         ↓
  If prog->compiled == 0:
    ERROR: "Program not compiled"
```

---

## 🔍 Issue Analysis

### The Core Problem

After successful compilation, `programs[0].compiled` should be **1**.
But bind command sees it as **0**.

### Possible Root Causes

#### 1. **State Instance Mismatch** (UNLIKELY)
- Upload uses `st_logic_get_state()` → gets `&g_logic_state`
- Bind uses `st_logic_get_state()` → gets same `&g_logic_state`
- Both should access same global instance ✓

#### 2. **Program ID Off-by-One** (NEEDS VERIFICATION)
- Upload: `set logic 1` → program_id=1 → converted to 0 in execution ✓
- Bind: `set logic 1` → program_id=1 → converted to 0 in execution ✓
- Should both use index 0 ✓

#### 3. **Compilation Status Not Saved** (POSSIBLE)
- `st_logic_compile()` sets `prog->compiled = 1` in RAM
- But if there's a reset or reinit between upload and bind?
- **NEED TO CHECK**: Is there something that clears `compiled` flag?

#### 4. **Memory Corruption** (UNLIKELY)
- Trim function was modifying source buffer
- Could have corrupted prog structure?
- **FIXED** in latest commit

#### 5. **Program Index Calculation** (NEEDS INVESTIGATION)
User uploaded to "Logic1" - that's user-facing name
- User-facing: 1, 2, 3, 4
- Array index: 0, 1, 2, 3

How are these conversions done everywhere?

---

## 🔎 Specific Code Locations to Check

### Upload Flow
1. **cli_parser.cpp:401-444** - "set logic" command parsing
   - Check program_id extraction
   - Check if it converts 1→0

2. **cli_shell.cpp:338-384** - Multi-line upload mode
   - Check buffer copying
   - Check program_id passed to execute_st_upload

3. **cli_parser_execute_st_upload(program_id, source)** - line 626-647
   - Check program_id passed to cli_cmd_set_logic_upload

4. **cli_cmd_set_logic_upload()** - line 50-97
   - Calls `st_logic_compile(logic_state, program_id)`
   - Checks: does it actually call compile?
   - Does compile succeed and set `prog->compiled = 1`?

### Binding Flow
1. **cli_parser.cpp:460-470** - "set logic ... bind" parsing
   - Check program_id extraction
   - Check if it converts 1→0

2. **cli_cmd_set_logic_bind_by_name()** - line 162-200
   - Gets program: `prog = st_logic_get_program(logic_state, program_id)`
   - Checks: `if (!prog || !prog->compiled)`
   - If this fails, throws error

---

## 🧪 Testing Strategy

Need to verify:

### Test 1: Check Compilation Actually Happens
```cpp
// Add debug output in st_logic_compile()
debug_printf("[DEBUG] st_logic_compile(program_id=%d)\n", program_id);
debug_printf("[DEBUG] Before: compiled=%d\n", prog->compiled);
// ... compile process ...
debug_printf("[DEBUG] After: compiled=%d\n", prog->compiled);
```

### Test 2: Check Program ID Consistency
```cpp
// In upload
debug_printf("[DEBUG] Upload: program_id=%d\n", program_id);

// In bind
debug_printf("[DEBUG] Bind: program_id=%d\n", program_id);
```

### Test 3: Check State Instance
```cpp
// Both places
debug_printf("[DEBUG] state=%p\n", logic_state);
```

---

## ⚠️ Critical Questions

1. **Does `st_logic_compile()` actually get called?**
   - Verify with debug output
   - Check if compilation succeeds

2. **Is `prog->compiled` actually set to 1?**
   - Add debug output after compile
   - Verify in next bind command

3. **Is the program_id consistent between upload and bind?**
   - Add debug output in both places
   - User types "1" in both - should both convert to 0

4. **Is there any state reset between upload and bind?**
   - Check if something clears `compiled` flag
   - Check main loop for resets

---

## 🚨 Issues Found in User's Input

### Issue 1: Extra `>` First Line
```
>>> >              ← Extra prompt character
>>> VAR
```

This first line contains just `>` - it's part of the source code!
- Could confuse parser
- Parser should reject it
- But compilation succeeded - why?

### Issue 2: Indentation in VAR Block
```
>>> VAR
>>> state: INT;    ← No indentation (correct)
```

Looks correct now, good!

### Issue 3: CASE Formatting
```
>>> CASE state OF
>>>   0:
>>>     red := TRUE;
```

Looks correct, good!

---

## 📋 Recommended Fixes

### Fix 1: Add Debug Output
Add to `st_logic_compile()`:
```cpp
bool st_logic_compile(st_logic_engine_state_t *state, uint8_t program_id) {
  st_logic_program_config_t *prog = &state->programs[program_id];

  debug_printf("[ST_LOGIC] Compiling program %d\n", program_id);
  // ... compilation ...

  if (success) {
    debug_printf("[ST_LOGIC] Compile SUCCESS - setting compiled=1\n");
    prog->compiled = 1;
  } else {
    debug_printf("[ST_LOGIC] Compile FAILED\n");
  }
}
```

### Fix 2: Add Debug in Bind
```cpp
int cli_cmd_set_logic_bind_by_name(...) {
  st_logic_program_config_t *prog = st_logic_get_program(logic_state, program_id);

  debug_printf("[BIND] program_id=%d, prog->compiled=%d\n",
               program_id, prog->compiled);

  if (!prog || !prog->compiled) {
    debug_println("ERROR: Program not compiled");
    return -1;
  }
}
```

### Fix 3: Verify Program ID Conversion
```cpp
// In cli_parser.cpp, "set logic" command:
uint8_t program_id = (uint8_t)argv[2][0] - '1';  // Convert 1→0
debug_printf("[CLI] User 'logic %c' → program_id=%d\n",
             argv[2][0], program_id);
```

---

## 🎯 Next Steps

To solve this properly, I need to:

1. Add comprehensive debug output to both upload and bind flows
2. Verify that `prog->compiled` is actually being set to 1
3. Verify that same program_id is used in both cases
4. Check if there's any state reset between the two operations
5. Handle the extra `>` character in the source code

---

## Summary

**Problem:** Upload says "SUCCESSFUL" but bind says "NOT COMPILED"

**Root Cause:** TBD - need debug output to determine

**Most Likely:**
- `st_logic_compile()` not actually being called
- OR `prog->compiled` not being set to 1
- OR program_id mismatch between upload and bind

**Next Action:** Add debug output to trace flow and identify exact failure point.
