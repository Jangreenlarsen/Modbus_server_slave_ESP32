# BUGS Index - Quick Reference

**Purpose:** Ultra-compact bug tracking for fast lookup. For detailed analysis, see BUGS.md sections.

## Bug Status Summary

| Bug ID | Title | Status | Priority | Version | Impact |
|--------|-------|--------|----------|---------|--------|
| BUG-001 | IR 220-251 ikke opdateret med ST Logic values | ✅ FIXED | 🔴 CRITICAL | v4.0.2 | ST Logic vars ikke synlig i Modbus |
| BUG-002 | Manglende type checking på ST variable bindings | ✅ FIXED | 🔴 CRITICAL | v4.0.2 | Data corruption ved type mismatch |
| BUG-003 | Manglende bounds checking på var index | ✅ FIXED | 🟡 HIGH | v4.0.2 | Buffer overflow risk |
| BUG-004 | Control register reset bit cleares ikke | ✅ FIXED | 🟡 HIGH | v4.0.2 | Error state persists incorrectly |
| BUG-005 | Inefficient variable binding count lookup | ✅ FIXED | 🟠 MEDIUM | v4.0.2 | Performance issue (O(n) lookup) |
| BUG-006 | Execution/error counters truncated til 16-bit | ✅ FIXED | 🔵 LOW | v4.0.2 | Counter wraps at 65535 |
| BUG-007 | Ingen timeout protection på program execution | ✅ FIXED | 🟠 MEDIUM | v4.0.2 | Runaway program can hang system |
| BUG-008 | IR 220-251 opdateres 1 iteration senere (latency) | ✅ FIXED | 🟠 MEDIUM | v4.1.0 | Stale data in Modbus registers |
| BUG-009 | Inkonsistent type handling (IR 220-251 vs gpio_mapping) | ✅ FIXED | 🔵 LOW | v4.1.0 | Confusing behavior, low priority |
| BUG-010 | Forkert bounds check for INPUT bindings | ✅ FIXED | 🔵 LOW | v4.1.0 | Cosmetic validation issue |
| BUG-011 | Variabelnavn `coil_reg` bruges til HR også (confusing) | ❌ OPEN | 🔵 LOW | v4.1.0 | Code clarity issue |
| BUG-012 | "both" mode binding skaber dobbelt output i 'show logic' | ✅ FIXED | 🟡 HIGH | v4.1.0 | Confusing UI display |
| BUG-013 | Binding visnings-rækkefølge matcher ikke var index | ✔️ NOT A BUG | 🔵 LOW | v4.1.0 | Design choice, not a bug |
| BUG-014 | execution_interval_ms bliver ikke gemt persistent | ✅ FIXED | 🟡 HIGH | v4.1.0 | Settings lost on reboot |
| BUG-015 | HW Counter PCNT ikke initialiseret uden hw_gpio | ✅ FIXED | 🔴 CRITICAL | v4.2.0 | HW counter doesn't work |
| BUG-016 | Running bit (bit 7) ignoreres | ✅ FIXED | 🔴 CRITICAL | v4.2.0 | Counter control broken |
| BUG-017 | Auto-start ikke implementeret | ✅ FIXED | 🔴 CRITICAL | v4.2.0 | Startup behavior inconsistent |
| BUG-018 | Show counters display respects bit-width | ✅ FIXED | 🟡 HIGH | v4.2.0 | Display truncation |
| BUG-019 | Show counters race condition (atomic reading) | ✅ FIXED | 🟡 HIGH | v4.2.0 | Intermittent display corruption |
| BUG-020 | Manual register configuration allowed (should be disabled) | ✅ FIXED | 🔴 CRITICAL | v4.2.0 | User can break system with bad config |
| BUG-021 | Delete counter command missing | ✅ FIXED | 🟡 HIGH | v4.2.0 | Can't reconfigure counters |
| BUG-022 | Auto-enable counter on running:on not working | ✅ FIXED | 🟡 HIGH | v4.2.0 | Counter state inconsistent |
| BUG-023 | Compare doesn't work when disabled | ✅ FIXED | 🟡 HIGH | v4.2.0 | Output stuck after disable |
| BUG-024 | PCNT counter truncated to 16-bit (raw reg limited to 2000) | ✅ FIXED | 🔴 CRITICAL | v4.2.0 | Counter value overflow |
| BUG-025 | Global register overlap not checked (Counter/Timer/ST overlap) | ✅ FIXED | 🔴 CRITICAL | v4.2.0 | Register conflicts possible |
| BUG-026 | ST Logic binding register allocator not freed on change | ✅ FIXED | 🔴 CRITICAL | v4.2.3 | Stale allocation persists across reboot (NOW FIXED) |
| BUG-027 | Counter display overflow - values above bit_width show incorrectly | ✅ FIXED | 🟠 MEDIUM | v4.2.3 | CLI display shows unclamped values |
| BUG-028 | Register spacing too small for 64-bit counters | ✅ FIXED | 🔴 CRITICAL | v4.2.3 | Counter 1 overlaps Counter 2 registers |
| BUG-029 | Compare modes use continuous check instead of edge detection | ✅ FIXED | 🔴 CRITICAL | v4.2.4 | Reset-on-read doesn't work, bit4 always set |
| BUG-030 | Compare value not accessible via Modbus | ✅ FIXED | 🔴 CRITICAL | v4.2.4 | Threshold only settable via CLI, not SCADA |
| BUG-031 | Counter write lock ikke brugt af Modbus FC handlers | ✅ FIXED | 🔴 CRITICAL | v4.2.5 | 64-bit counter read kan give korrupt data |
| BUG-032 | Buffer overflow i ST parser (strcpy uden bounds) | ✅ FIXED | 🔴 CRITICAL | v4.2.5 | Stack corruption ved lange variabelnavne |
| BUG-033 | Variable declaration bounds check efter increment | ✅ FIXED | 🔴 CRITICAL | v4.2.5 | Buffer overflow på 33. variable |
| BUG-034 | ISR state læsning uden volatile cast | ✅ FIXED | 🟡 HIGH | v4.2.6 | Sporadisk manglende pulser ved høj frekvens |
| BUG-035 | Overflow flag aldrig clearet automatisk | ✅ FIXED | 🟡 HIGH | v4.2.6 | Sticky overflow kræver manuel reset |
| BUG-036 | SW-ISR underflow wrapper ikke (inkonsistent med SW) | ✅ FIXED | 🟠 MEDIUM | v4.2.5 | DOWN mode stopper ved 0 i ISR mode |
| BUG-037 | Jump patch grænse 512 i stedet for 1024 | ✅ FIXED | 🟠 MEDIUM | v4.2.5 | Store CASE statements kan fejle |
| BUG-038 | ST Logic variable memcpy uden synchronization | ✅ FIXED | 🟡 HIGH | v4.2.6 | Race condition mellem execute og I/O |
| BUG-039 | CLI compare-enabled parameter ikke genkendt | ✅ FIXED | 🟠 MEDIUM | v4.2.7 | Kun "compare:1" virker, ikke "compare-enabled:1" |
| BUG-040 | Compare bruger rå counter værdi i stedet for prescaled | ✅ FIXED | 🟡 HIGH | v4.2.8 | Compare ignorerer prescaler/scale, ukonfigurérbar |
| BUG-041 | Reset-on-read parameter placering og navngivning forvirrende | ✅ FIXED | 🟠 MEDIUM | v4.2.9 | Samme parameter navn for counter og compare reset |
| BUG-042 | normalize_alias() håndterer ikke "auto-load" | ✅ FIXED | 🟡 HIGH | v4.3.0 | "set persist auto-load" ikke genkendt af parser |
| BUG-043 | "set persist enable on" case sensitivity bug | ✅ FIXED | 🟡 HIGH | v4.3.0 | enabled blev altid false → printer "DISABLED" |
| BUG-044 | cli_cmd_set_persist_auto_load() case sensitive strcmp | ✅ FIXED | 🟠 MEDIUM | v4.3.0 | "ENABLE" eller "Enable" ville ikke virke |
| BUG-045 | Upload mode ignorerer brugerens echo setting | ✅ FIXED | 🟡 HIGH | v4.3.0 | "set echo on" har ingen effekt i ST upload mode |
| BUG-046 | ST datatype keywords (INT, REAL) kolliderer med literals | ✅ FIXED | 🔴 CRITICAL | v4.3.1 | REAL/INT variable declarations fejler med "Unknown variable" |
| BUG-047 | Register allocator ikke frigivet ved program delete | ✅ FIXED | 🔴 CRITICAL | v4.3.2 | "Register already allocated" efter delete/recreate |
| BUG-048 | Bind direction parameter ignoreret | ✅ FIXED | 🟡 HIGH | v4.3.3 | "input" parameter ikke brugt, defaults altid til "output" |
| BUG-049 | ST Logic kan ikke læse fra Coils | ✅ FIXED | 🔴 CRITICAL | v4.3.3 | "coil:20 input" læser fra discrete input i stedet for coil |
| BUG-050 | VM aritmetiske operatorer understøtter ikke REAL | ✅ FIXED | 🔴 CRITICAL | v4.3.4 | MUL/ADD/SUB bruger altid int_val, REAL arithmetic giver 0 |
| BUG-051 | Expression chaining fejler for REAL | ✅ FIXED | 🟡 HIGH | v4.3.5 | "a := b * c / d" fejler, men separate statements virker |
| BUG-052 | VM operators mangler type tracking | ✅ FIXED | 🔴 CRITICAL | v4.3.6 | Comparison/logical/bitwise operators bruger st_vm_push() i stedet for st_vm_push_typed() |
| BUG-053 | SHL/SHR operators virker ikke | ✅ FIXED | 🔴 CRITICAL | v4.3.7 | Parser precedence chain mangler SHL/SHR tokens |
| BUG-054 | FOR loop body aldrig eksekveret | ✅ FIXED | 🔴 CRITICAL | v4.3.8 | Compiler bruger GT i stedet for LT i loop condition check |
| BUG-055 | Modbus Master CLI commands ikke virker | ✅ FIXED | 🔴 CRITICAL | v4.4.0 | normalize_alias() mangler parameter entries |
| BUG-056 | Buffer overflow i compiler symbol table | ✅ FIXED | 🔴 CRITICAL | v4.4.3 | strcpy uden bounds check i st_compiler_add_symbol() |
| BUG-057 | Buffer overflow i parser program name | ✅ FIXED | 🟠 MEDIUM | v4.4.3 | strcpy hardcoded string (low risk) |
| BUG-058 | Buffer overflow i compiler bytecode name | ✅ FIXED | 🟠 MEDIUM | v4.4.3 | strcpy program name til bytecode uden bounds check |
| BUG-059 | Comparison operators ignorerer REAL type | ✅ FIXED | 🔴 CRITICAL | v4.4.3 | EQ/NE/LT/GT/LE/GE bruger kun int_val, REAL comparison fejler |
| BUG-060 | NEG operator ignorerer REAL type | ✅ FIXED | 🟠 MEDIUM | v4.4.3 | Unary minus bruger kun int_val, -1.5 bliver til -1 |
| BUG-063 | Function argument overflow validation | ✅ FIXED | 🟡 HIGH | v4.4.3 | Parser bruger break i stedet for return NULL (compilation fejler ikke) |
| BUG-065 | SQRT mangler negative input validation | ✅ FIXED | 🟠 MEDIUM | v4.4.4 | sqrtf(negative) returnerer NaN, crasher beregninger |
| BUG-067 | Lexer strcpy buffer overflow risiko | ✅ FIXED | 🔴 CRITICAL | v4.4.4 | 12× strcpy uden bounds check (token value 256 bytes) |
| BUG-068 | String parsing mangler null terminator | ✅ FIXED | 🟡 HIGH | v4.4.4 | Loop limit 250 men buffer 256, strcpy kan fejle |
| BUG-072 | DUP operator mister type information | ✅ FIXED | 🟠 MEDIUM | v4.4.4 | REAL værdier duplikeres som INT → forkerte beregninger |
| BUG-073 | SHL/SHR mangler shift amount validation | ✅ FIXED | 🟠 MEDIUM | v4.4.4 | Shift >= 32 er undefined behavior på ESP32 |
| BUG-074 | Jump patch silent failure | ✅ FIXED | 🟠 MEDIUM | v4.4.4 | Bounds check returnerer uden fejlmelding → bytecode korruption |
| BUG-069 | INT literal parsing overflow | ✅ FIXED | 🟠 MEDIUM | v4.4.5 | strtol kan overflow uden errno check → undefined values |
| BUG-070 | REAL literal parsing overflow | ✅ FIXED | 🟠 MEDIUM | v4.4.5 | strtof kan overflow uden errno check → undefined values |
| BUG-083 | Modulo INT_MIN overflow | ✅ FIXED | 🔵 LOW | v4.4.5 | INT_MIN % -1 er undefined behavior i C/C++ |
| BUG-084 | Modbus slave_id mangler validation | ✅ FIXED | 🟠 MEDIUM | v4.4.5 | Kan sende requests til invalid slave (0, 248-255) |
| BUG-085 | Modbus address mangler validation | ✅ FIXED | 🟠 MEDIUM | v4.4.5 | Kan sende requests med negative addresser |
| BUG-066 | AST malloc fejl ikke håndteret | ✅ FIXED | 🟡 HIGH | v4.4.5 | 19× ast_node_alloc() uden NULL check → segfault på OOM |
| BUG-087 | NEG operator INT_MIN overflow | ✅ FIXED | 🔵 LOW | v4.4.5 | -INT_MIN er undefined behavior i C/C++ |
| BUG-081 | Memory leak ved parser error | ✅ FIXED | 🟠 MEDIUM | v4.4.5 | Expression parsing chain lækker AST ved fejl |
| BUG-077 | Function return type validation | ✅ FIXED | 🟠 MEDIUM | v4.4.5 | SEL/LIMIT polymorfiske funktioner bruger forkert type |
| BUG-088 | ABS funktion INT_MIN overflow | ✅ FIXED | 🔴 CRITICAL | v4.4.5 | ABS(-2147483648) returnerer -2147483648 (ikke positiv) |
| BUG-089 | ADD/SUB/MUL integer overflow | ✅ FIXED | 🔴 CRITICAL | v4.4.5 | Ingen overflow checks på arithmetic → silent overflow |
| BUG-104 | Function argument NULL pointer | ✅ FIXED | 🟠 MEDIUM | v4.4.5 | parser_parse_expression() NULL ikke håndteret |
| BUG-105 | INT type skal være 16-bit, ikke 32-bit (IEC 61131-3) | ✅ FIXED | 🔴 CRITICAL | v5.0.0 | INT overflow ikke korrekt, mangler DINT/multi-register |
| BUG-106 | Division by zero gemmer gamle værdier | ✅ FIXED | 🔴 CRITICAL | v5.0.0 | Variabler kopieres tilbage fra VM også ved runtime error |
| BUG-107 | CLI bind display viser "HR#X" for coil input | ✅ FIXED | 🔵 LOW | v5.0.0 | Forvirrende CLI output, men funktionalitet virker |
| BUG-108 | CLI mangler `write reg value real` kommando | ✅ FIXED | 🔴 CRITICAL | v5.0.0 | Kan ikke skrive REAL værdier korrekt via CLI |
| BUG-109 | Multi-register bindings ikke frigivet korrekt ved delete | ✅ FIXED | 🔴 CRITICAL | v5.0.0 | DINT/REAL bindings frigiver kun 1 register ved sletning |
| BUG-110 | SUM funktion ikke type-aware (returnerer kun første parameter) | ✅ FIXED | 🔴 CRITICAL | v5.0.0 | SUM(5,3) returnerer 5 i stedet for 8 |

## Quick Lookup by Category

### ⚠️ CRITICAL Bugs (MUST FIX)
- **BUG-001:** ST Logic vars not visible in Modbus (IR 220-251)
- **BUG-002:** Type checking on variable bindings
- **BUG-015:** HW Counter initialization
- **BUG-016:** Running bit control
- **BUG-017:** Auto-start feature
- **BUG-020:** Manual register config disabled
- **BUG-024:** Counter truncation fix
- **BUG-025:** Register overlap checking
- **BUG-026:** Binding allocator cleanup
- **BUG-028:** Register spacing for 64-bit counters
- **BUG-029:** Compare edge detection
- **BUG-030:** Compare value Modbus access
- **BUG-031:** Counter write lock i Modbus handlers (FIXED v4.2.5)
- **BUG-032:** ST parser buffer overflow (FIXED v4.2.5)
- **BUG-033:** Variable declaration bounds (FIXED v4.2.5)
- **BUG-046:** ST datatype keywords collision (FIXED v4.3.1 Build #676)
- **BUG-047:** Register allocator not freed on delete (FIXED v4.3.2 Build #691)
- **BUG-049:** ST Logic cannot read from Coils (FIXED v4.3.3 Build #703)
- **BUG-050:** VM arithmetic operators don't support REAL (FIXED v4.3.4 Build #708)
- **BUG-052:** VM operators mangler type tracking (FIXED v4.3.6 Build #714)
- **BUG-053:** SHL/SHR operators virker ikke (FIXED v4.3.7 Build #717)
- **BUG-054:** FOR loop body aldrig eksekveret (FIXED v4.3.8 Build #720)
- **BUG-055:** Modbus Master CLI commands ikke virker (FIXED v4.4.0 Build #744)
- **BUG-056:** Buffer overflow i compiler symbol table (FIXED v4.4.3)
- **BUG-059:** Comparison operators ignorerer REAL type (FIXED v4.4.3)
- **BUG-067:** Lexer strcpy buffer overflow (FIXED v4.4.4)

### 🟡 HIGH Priority (SHOULD FIX)
- **BUG-003:** Bounds checking on var index
- **BUG-004:** Reset bit in control register
- **BUG-014:** Persistent interval save
- **BUG-018:** Show counters bit-width
- **BUG-019:** Race condition in display
- **BUG-021:** Delete counter command
- **BUG-022:** Auto-enable counter
- **BUG-023:** Compare after disable
- **BUG-034:** ISR state volatile cast (FIXED v4.2.6)
- **BUG-035:** Overflow flag auto-clear (FIXED v4.2.6)
- **BUG-038:** ST Logic variable race condition (FIXED v4.2.6)
- **BUG-042:** normalize_alias() missing "auto-load" (FIXED v4.3.0)
- **BUG-043:** "set persist enable on" case sensitivity (FIXED v4.3.0)
- **BUG-045:** Upload mode echo setting (FIXED v4.3.0)
- **BUG-048:** Bind direction parameter ignored (FIXED v4.3.3 Build #698)
- **BUG-051:** Expression chaining fejler for REAL (FIXED v4.3.5 Build #712)
- **BUG-063:** Function argument overflow validation (FIXED v4.4.3)
- **BUG-068:** String parsing null terminator (FIXED v4.4.4)
- **BUG-CLI-1:** Parameter keyword clarification
- **BUG-CLI-2:** GPIO validation

### 🟠 MEDIUM Priority (NICE TO HAVE)
- **BUG-005:** Binding count lookup optimization
- **BUG-007:** Execution timeout protection
- **BUG-008:** IR update latency
- **BUG-027:** Counter display overflow clamping
- **BUG-039:** CLI compare-enabled parameter (FIXED v4.2.7)
- **BUG-040:** Compare source configurability (FIXED v4.2.8)
- **BUG-041:** Reset-on-read parameter structure (FIXED v4.2.9)
- **BUG-044:** cli_cmd_set_persist_auto_load() case sensitive (FIXED v4.3.0)
- **BUG-057:** Buffer overflow i parser program name (FIXED v4.4.3)
- **BUG-058:** Buffer overflow i compiler bytecode name (FIXED v4.4.3)
- **BUG-060:** NEG operator ignorerer REAL type (FIXED v4.4.3)
- **BUG-065:** SQRT negative validation (FIXED v4.4.4)
- **BUG-072:** DUP type stack sync (FIXED v4.4.4)
- **BUG-073:** SHL/SHR shift overflow (FIXED v4.4.4)
- **BUG-074:** Jump patch silent fail (FIXED v4.4.4)

### 🔵 LOW Priority (COSMETIC)
- **BUG-006:** Counter wrapping at 65535
- **BUG-011:** Variable naming (`coil_reg`)

### ✔️ NOT BUGS (DESIGN CHOICES)
- **BUG-013:** Binding display order (intentional)

## Status Legend

| Symbol | Meaning |
|--------|---------|
| ✅ FIXED | Bug implemented and verified |
| ❌ OPEN | Bug identified but not fixed |
| ✔️ NOT A BUG | Determined to be design choice |
| 🔴 CRITICAL | System breaks without fix |
| 🟡 HIGH | Significant impact, should fix soon |
| 🟠 MEDIUM | Noticeable impact, nice to fix |
| 🔵 LOW | Minor issue, cosmetic only |

## How Claude Should Use This

**When working on code changes:**
1. Check BUGS_INDEX.md first (this file) - ~10 seconds to skim
2. Note which bugs might be affected by your changes
3. If you need details, go to BUGS.md and search for specific BUG-ID
4. Before committing: Update BUGS_INDEX.md status if you fixed a bug
5. Update BUGS.md detailed section only if implementing significant fix

**Example workflow:**
```
Task: Modify ST Logic binding logic
1. Skim BUGS_INDEX.md → See BUG-002, BUG-005, BUG-012, BUG-026
2. Check if your changes impact these areas
3. If modifying binding code → might affect BUG-026
4. Read BUGS.md § BUG-026 for implementation details
5. After fix → Update BUGS_INDEX.md row for BUG-026
```

## File Organization

- **BUGS_INDEX.md** (THIS FILE): Compact 1-liner summary of all bugs (tokens: ~500)
- **BUGS.md**: Full detailed analysis with root causes, test results, code references (tokens: ~5000+)

Use BUGS_INDEX.md for quick navigation, BUGS.md for deep dives.
