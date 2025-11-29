# Logic Block System - High Level Design Document

**Version:** 0.1 (BRAINSTORM)
**Dato:** 2025-11-29
**Platform:** ESP32-WROOM-32
**Status:** Design Review / Community Feedback

---

## 1. EXECUTIVE SUMMARY

Et modulært logic-system som tillader bruger at:
- **Processkontrol:** Bygge sekventielle styrringslogik uden at programmere
- **Alarmlogik:** Kombinere flere inputs til komplekse alarmregler
- **Datakæde:** Forbinde inputs til outputs gennem logiske blokke

**Kernebegreber:**
- **16 Logic Blocks** (uafhængige, kan forbindes)
- **32 Variabler** (delt state mellem blokke)
- **Multiple Input/Output kilder** (Modbus reg, coils, discrete inputs, andre blokke)
- **Time Source** (millis(), ticks for delay/timers)

---

## 2. SYSTEM OVERVIEW

```
┌────────────────────────────────────────────────────────────────┐
│                   LOGIC BLOCK ENGINE                           │
├────────────────────────────────────────────────────────────────┤
│                                                                 │
│  INPUT SOURCES              LOGIC BLOCKS (1-16)  OUTPUT SINKS  │
│  ┌──────────────┐          ┌──────────────┐     ┌────────────┐│
│  │ Reg:0-159    │ ┐        │ Block 1      │ ┐   │ Coil:0-255 ││
│  │ Coil:0-255   │ │──────→ │ Block 2      │ │──→│ Reg:0-159  ││
│  │ Input:0-255  │ │        │ Block 3      │ │   │ Var:0-31   ││
│  │ Var:0-31     │ │        │ ...          │ │   │ Block.in   ││
│  │ Timer:1-4    │ │        │ Block 16     │ │   │ Timer:1-4  ││
│  │ Counter:1-4  │ ┘        └──────────────┘ ┘   │            ││
│  └──────────────┘          ┌──────────────┐     └────────────┘│
│                            │ TIME SOURCE  │                    │
│                            │ - millis()   │                    │
│                            │ - tick_1s    │                    │
│                            │ - tick_100ms │                    │
│                            └──────────────┘                    │
│                                                                 │
│  EVALUATION MODE: Topological sort (avoid circular deps)      │
│  PRIORITY: System processes blocks each main loop iteration   │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

---

## 3. USE CASES

### Use Case 1: Simple Threshold Alarm
```
Scenario: Alarm hvis temperatur > 60°C i mere end 2 sekunder

Block 1 (THRESHOLD):
  Input: Reg:100 (temperatur)
  Threshold: 600 (60.0°C * 10)
  Hysteresis: 50 (5°C)
  Output: Coil:200 (alarm_triggered)

Block 2 (DELAY):
  Input: Coil:200
  Delay: 2000ms
  Output: Coil:201 (alarm_active)

Result: Coil:201 = 1 når temp > 60°C i 2 sekunder
```

### Use Case 2: Complex Interlocking
```
Scenario: Start motor kun hvis:
  - Pressure > 100 bar (Reg:110)
  - AND Door closed (Coil:50)
  - AND Emergency stop NOT active (Coil:60)

Block 1 (THRESHOLD):
  Input: Reg:110
  Threshold: 1000
  Output: Coil:300 (pressure_ok)

Block 2 (NOT):
  Input: Coil:60
  Output: Coil:301 (e_stop_ok)

Block 3 (AND):
  Inputs: Coil:50, Coil:300, Coil:301
  Output: Coil:202 (motor_start)

Result: Motor starts only if all 3 conditions met
```

### Use Case 3: Data Processing Pipeline
```
Scenario: Process sensor value through 3 stages

Block 1 (RATELIMIT/DEBOUNCE):
  Input: Coil:60 (noisy sensor)
  Debounce: 50ms
  Output: Coil:301 (clean signal)

Block 2 (COMPARE):
  Input1: Coil:301
  Input2: Var:10 (threshold)
  Operator: >
  Output: Var:20 (is_above_threshold)

Block 3 (ARITHMETIC):
  Input1: Reg:100 (raw value)
  Input2: Var:30 (scale factor)
  Operator: *
  Output: Reg:101 (scaled_value)

Result: Pipeline transforms raw sensor → clean → processed → output
```

### Use Case 4: Sequential Process Control
```
Scenario: Start pump sequence with delays

Block 1 (AND):
  Inputs: Coil:10 (start_request), Coil:20 (water_available)
  Output: Coil:300 (ready)

Block 2 (DELAY):
  Input: Coil:300
  Delay: 500ms (pre-pressurize)
  Output: Coil:301 (pump_start_allowed)

Block 3 (LATCH):
  Set: Coil:301
  Reset: Coil:11 (stop_request)
  Output: Coil:202 (pump_active)

Result: Pump starts with 500ms pre-delay, stops on request
```

### Use Case 5: Multi-Register Data Processing Pipeline ✨ **NEW**
```
Scenario: Read 3 sensor registers, convert/calculate, execute sequence if condition met

Block 1 (ARITHMETIC):
  Inputs: Reg:100 (sensor1), Const:10 (scale)
  Operator: * (multiply)
  Output: Var:10 (scaled_sensor1)

Block 2 (ARITHMETIC):
  Inputs: Var:10, Reg:101 (sensor2)
  Operator: + (add)
  Output: Var:11 (combined_value)

Block 3 (COMPARE):
  Inputs: Var:11, Const:500 (threshold)
  Operator: >
  Output: Var:20 (threshold_exceeded)

Block 4 (SEQUENCE) - MODBUS COMMAND SEQUENCE:
  Trigger: Var:20 (if threshold exceeded)
  Commands:
    1. write coil 200 value 1      (Alarm on)
    2. write reg 102 value 1000    (Log event)
    3. set timer 1 ctrl-reg:60 1   (Start timer)
  Output: Coil:210 (sequence_executed)

Result: When sensor1×10 + sensor2 > 500 → Execute Modbus command sequence
```

**Key Feature:** Blocks can issue MODBUS COMMANDS, not just update internal state

---

### Use Case 6: State Machine with Timer/Counter Triggers ✨ **NEW**
```
Scenario: Multi-state process controlled by timers and counter overflow

STATE MACHINE BLOCK (Type: STATE_MACHINE):
  States: IDLE → PRESSURIZING → RUNNING → COOLING → IDLE

  State: IDLE
    On-Entry: set coil 100 value 0
    Trigger: Input Coil:50 (start_request)
    Next: PRESSURIZING

  State: PRESSURIZING
    On-Entry:
      - set coil 101 value 1 (pump on)
      - start timer 1 with ctrl-reg:60
    Trigger: Timer:1 overflow (5 second timeout)
    Next: RUNNING

  State: RUNNING
    On-Entry:
      - set coil 102 value 1 (process active)
      - reset counter 1
    Trigger: Counter:1 reaches 1000 (count-based)
    Next: COOLING

  State: COOLING
    On-Entry:
      - set coil 101 value 0 (pump off)
      - start timer 2 with ctrl-reg:61
    Trigger: Timer:2 overflow (10 second timeout)
    Next: IDLE

  Emergency Exit: If Coil:51 (e-stop) → IDLE immediately

Result:
  - Each state is active for defined duration
  - State transitions happen on timer/counter events
  - Each state executes on-entry Modbus commands
  - Can exit early on emergency condition
```

**Key Features:**
- Multiple states with on-entry actions
- Transition triggers from timers/counters/conditions
- Automatic timeout per state
- Emergency exit conditions

---

## STATE MACHINE IMPLEMENTATION CONCEPT

Instead of simple blocks, state machine is a **composite block** with:

```c
typedef struct {
  uint8_t id;
  uint8_t current_state;           // 0-15 (max 16 states)
  uint8_t state_count;

  struct {
    char name[32];                 // e.g., "IDLE", "PRESSURIZING"

    // On-entry actions (Modbus commands)
    struct {
      uint8_t type;                // WRITE_COIL, WRITE_REG, SET_TIMER, etc
      uint16_t addr;
      uint16_t value;
    } actions[8];                  // Up to 8 actions per state
    uint8_t action_count;

    // Transition to next state
    uint8_t next_state;

    // Trigger condition for transition
    struct {
      uint8_t type;                // TIMER_OVERFLOW, COUNTER_OVERFLOW, COIL, REG_COMPARE
      uint8_t source_id;           // Timer 1-4, Counter 1-4, etc
      uint16_t timeout_ms;         // If using timeout instead of trigger
    } trigger;

    // Emergency exit (always)
    struct {
      uint8_t enabled;
      uint8_t source_type;         // COIL, REG_COMPARE
      uint16_t source_id;
    } emergency_exit;

  } states[16];                    // Max 16 states per machine

  // Runtime
  uint32_t state_start_time;
  uint32_t state_timeout;

} StateBlock;
```

---

## 4. BLOCK TYPES

### **Core Blocks (MUST HAVE)**

| Type | Inputs | Function | Use Case |
|------|--------|----------|----------|
| **AND** | 2-4 | Output = 1 if ALL inputs = 1 | Safety interlocks |
| **OR** | 2-4 | Output = 1 if ANY input = 1 | Alarm combining |
| **NOT** | 1 | Output = NOT input | Negation logic |
| **THRESHOLD** | 1 | Trigger if value > threshold | Alarm levels |
| **COMPARE** | 2 | Compare: ==, !=, <, >, <=, >= | Value comparison |
| **DELAY** | 1 | Delay output by N ms | Start delays |
| **LATCH** | 2 (set, reset) | Set/reset bistable | State holding |
| **ARITHMETIC** | 2 | +, -, *, /, min, max, abs | Data processing |

### **Advanced Blocks (PHASE 2)**

| Type | Function | Capability |
|------|----------|-----------|
| **SEQUENCE** | Execute Modbus command sequence on trigger | Write coils, regs, start timers |
| **RATELIMIT** | Limit change rate (debounce) | Noise filtering |
| **HYSTERESIS** | Threshold with dead-band | Multi-level alarms |
| **STATE_MACHINE** | Multi-state process controller | Timers/counter triggers, on-entry actions |
| **XOR** | Exclusive OR | Logic combination |
| **MUX** | Multiplexer (select input 0-3) | Input selection |
| **LUT** | Lookup table | Complex mappings |

---

## 5. INPUT/OUTPUT SOURCES

### **Input Sources (What can feed a block?)**
```
REG:<0-159>          - Holding register
COIL:<0-255>         - Coil value
INPUT:<0-255>        - Discrete input
VAR:<0-31>           - Variable
TIMER:<1-4>          - Timer output status
COUNTER:<1-4>        - Counter value
BLOCK:<1-16>         - Output from another block
CONST:<value>        - Constant (hardcoded)
```

### **Output Destinations (Where can a block write?)**
```
COIL:<0-255>         - Set coil value
REG:<0-159>          - Write holding register
VAR:<0-31>           - Update variable
BLOCK:<1-16>         - Feed to another block
TIMER:<1-4>          - Trigger timer
```

---

## 6. DATA STRUCTURES

### **Logic Block Configuration**
```c
typedef struct {
  uint8_t id;                          // 1-16
  uint8_t enabled;                     // 0=disabled, 1=enabled
  LogicBlockType type;                 // AND, OR, THRESHOLD, etc

  // Input configuration (up to 4 inputs)
  struct {
    uint8_t source_type;               // REG|COIL|INPUT|VAR|BLOCK|CONST
    uint16_t source_id;                // Address or ID
    uint16_t const_value;              // If CONST type
  } inputs[4];
  uint8_t input_count;                 // Actual # of inputs used

  // Output configuration (up to 2 outputs)
  struct {
    uint8_t dest_type;                 // COIL|REG|VAR|BLOCK
    uint16_t dest_id;
  } outputs[2];
  uint8_t output_count;                // Actual # of outputs used

  // Block-specific parameters
  union {
    struct {
      uint16_t threshold;
      uint16_t hysteresis;
      uint8_t logic;                   // ABOVE|BELOW|BETWEEN
    } threshold;

    struct {
      uint32_t delay_ms;
      uint32_t start_time;             // When delay started
    } delay;

    struct {
      uint8_t operator;                // ==|!=|<|>|<=|>=
    } compare;

    struct {
      uint8_t operator;                // +|-|*|/|min|max|abs
    } arithmetic;

    struct {
      uint8_t logic;                   // SET_DOMINANT|RESET_DOMINANT
    } latch;

    struct {
      uint32_t debounce_ms;
      uint32_t last_change_time;
      uint32_t last_value;
    } ratelimit;
  } params;

  // Runtime state
  uint32_t last_output;                // Last computed output value
  uint32_t state;                      // Internal state (if needed)

} LogicBlock;

typedef struct {
  LogicBlock blocks[16];               // Blocks 1-16
  uint32_t variables[32];              // Shared variables
  uint8_t var_changed[32];             // Change flags

  // Time sources
  uint32_t time_source;                // Current millis()
  uint8_t tick_1s;                     // Pulse every 1 second
  uint8_t tick_100ms;                  // Pulse every 100ms

  // Dependency tracking
  uint8_t eval_order[16];              // Block evaluation order
  uint8_t eval_count;                  // How many blocks enabled

} LogicEngine;
```

---

## 7. CLI COMMANDS

### **Configuration**
```bash
# Set up a block
set logic <id> type:<type> \
  input:<source1> [input:<source2>] \
  [input2:<source2>] [input3:<source3>] [input4:<source4>] \
  output:<dest> [output2:<dest2>] \
  [param1:<value>] [param2:<value>] \
  enabled:<0|1>

# Examples:
set logic 1 type:threshold input:reg:100 output:coil:200 \
  threshold:500 hysteresis:50 enabled:1

set logic 2 type:and input:coil:50 input2:coil:60 \
  output:coil:201 enabled:1

set logic 3 type:delay input:block:2 output:coil:202 \
  delay-ms:1000 enabled:1

set logic 4 type:compare input:reg:110 input2:const:1000 \
  operator:> output:coil:300 enabled:1
```

### **Viewing**
```bash
# Show all blocks
show logic

# Show specific block with live values
show logic 1

# Show all 32 variables
show logic-vars

# Show blocks in evaluation order
show logic-order

# Monitor in real-time (live updates)
show logic-monitor
```

### **Variable Management**
```bash
# Set variable manually
set logic-var <id> <value>

# Example:
set logic-var 10 1000

# Reset all variables
set logic-vars reset
```

### **Persistence**
```bash
# Save logic configuration
save

# Load from NVS
load
```

---

## 8. IMPLEMENTATION STRATEGY

### **Phase 1: Core (MVP)**
- [ ] Logic block structure & storage
- [ ] AND, OR, NOT, THRESHOLD blocks
- [ ] Input source reading
- [ ] Output sink writing
- [ ] CLI set/show commands
- [ ] Main loop integration

### **Phase 2: Enhanced**
- [ ] DELAY, LATCH, COMPARE blocks
- [ ] Topological sort (dependency ordering)
- [ ] ARITHMETIC blocks
- [ ] NVS persistence

### **Phase 3: Advanced (Future)**
- [ ] RATELIMIT, HYSTERESIS
- [ ] Timer integration
- [ ] Counter integration
- [ ] LUT, MUX blocks

---

## 9. OPEN QUESTIONS FOR BRAINSTORM

### **Architecture**
- [ ] Should blocks evaluate in parallel or sequential order?
- [ ] How to handle circular dependencies? (Block A → Block B → Block A)
- [ ] Should block outputs persist between loops or re-evaluate each time?

### **Functionality**
- [ ] Should variables support floating point or just uint32?
- [ ] Should we have block-to-block feedback with delay?
- [ ] Do we need input validation/range checking per block?
- [ ] Should blocks support "state" (remembering previous value)?

### **CLI/UX**
- [ ] Is the CLI syntax clear enough? (alternatives?)
- [ ] Should we support block templates/presets?
- [ ] Need live debugging/monitoring mode?
- [ ] Should blocks have descriptions/comments?

### **Performance**
- [ ] How many blocks can we evaluate per loop iteration?
- [ ] Should we limit input count to 2 instead of 4?
- [ ] Any RAM constraints? (16 blocks × struct size)?

### **Integration**
- [ ] Should logic blocks be able to START/STOP timers?
- [ ] Should logic blocks read counter values directly?
- [ ] Should GPIO mappings be integrated with logic system?

---

## 10. RESOURCE ESTIMATES

### **Memory (Approximate)**
```
LogicBlock struct:        ~100 bytes each
16 blocks:                ~1.6 KB
Variables (32 × 4):       ~128 bytes
Logic Engine struct:      ~200 bytes
───────────────────────────────
TOTAL:                    ~2 KB (well within 320KB RAM)
```

### **Performance**
```
Evaluation per block:     ~10-20 μs (estimated)
All 16 blocks:            ~160-320 μs per loop
@ 1000 Hz loop:           0.16-0.32% CPU usage
```

---

## 11. FILE STRUCTURE

```
src/
  logic_block.cpp/h         - Block struct, enums, types
  logic_engine.cpp/h        - Main engine, eval loop
  logic_evaluator.cpp/h     - Block type implementations
  logic_source.cpp/h        - Read inputs (reg/coil/var/etc)
  logic_sink.cpp/h          - Write outputs
  logic_parser.cpp          - Parse CLI commands
  cli_logic.cpp             - CLI show/set logic
  logic_config.cpp/h        - Load/save NVS

include/
  logic_block.h             - Public API
```

---

## 12. INTEGRATION POINTS

### **In main loop:**
```cpp
void loop() {
  // ... existing code ...

  // Update time sources
  logic_engine_update_time();

  // Evaluate all logic blocks
  logic_engine_evaluate_all();

  // ... rest of loop ...
}
```

### **With existing systems:**
- **Registers:** Read from holding/input registers directly
- **Coils:** Read/write to coil array
- **Timers:** Could trigger timers from blocks (future)
- **Counters:** Could read counter values (future)
- **GPIO:** Could use GPIO mappings as inputs (future)

---

## 13. SUCCESS CRITERIA

- [ ] System can handle 16 blocks with 4 inputs each
- [ ] All block types evaluate in < 1ms total
- [ ] CLI commands are intuitive and documented
- [ ] Configuration persists across reboot
- [ ] No circular dependency crashes
- [ ] 32 variables available for inter-block communication

---

## 14. NEXT STEPS

1. **Review & Feedback:** Comments on design above?
2. **Clarify Questions:** Any open questions need answering?
3. **Finalize Spec:** Lock down block types and parameters
4. **Start Coding:** Phase 1 implementation

---

**END OF HLD DOCUMENT**

---

### **How to Use This Document:**

1. **Read the use cases** - Do they match your needs?
2. **Comment on block types** - Are these enough? Need more?
3. **Flag concerns** - Circular dependencies? Performance?
4. **Suggest improvements** - Better CLI syntax? Different structure?
5. **Approve when ready** - Then we code Phase 1!
