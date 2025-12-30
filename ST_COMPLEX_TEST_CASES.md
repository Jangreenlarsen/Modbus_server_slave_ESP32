# ST Logic Complex Test Cases

**Version:** v4.4.0
**Build:** #881
**Date:** 2025-12-30
**Purpose:** Advanced integration testing - combining multiple ST Logic features

---

## 📋 Overview

These test cases combine multiple ST Logic features to validate:
- Multiple data types in same program (BOOL, INT, REAL)
- Complex state machines with arithmetic
- Type conversions in expressions
- Nested conditions and boolean logic
- Multi-register I/O with calculations
- Edge case handling (overflow, division by zero)
- Performance under complex operations

All test cases are **copy/paste ready** for Telnet CLI.

---

## ✅ Test Case 1: PID Temperature Controller with State Machine

**Kombinerer:** REAL arithmetic, state machine, type conversions, range validation

**Features:**
- 3-state control (IDLE, HEATING, COOLING)
- PID-style proportional control
- Temperature setpoint with deadband
- Power output calculation (0-100%)
- Alarm on out-of-range

**Variables:**
- `state` (INT): Current state (0=IDLE, 1=HEATING, 2=COOLING)
- `temp_actual` (REAL): Process temperature (°C)
- `temp_setpoint` (REAL): Target temperature (°C)
- `deadband` (REAL): Hysteresis band (°C)
- `power_output` (INT): Heater/cooler power (0-100%)
- `enable` (BOOL): System enable
- `alarm` (BOOL): Temperature alarm
- `error` (REAL): Setpoint - actual (calculated)

**Bindings:**
- IN: Coil 0 → enable, HR 20 → temp_setpoint (REAL at 20-21), HR 30 → temp_actual (REAL at 30-31)
- OUT: Coil 10 → alarm, HR 40 → power_output

**Test Commands:**

```bash
set logic 1 delete
set logic 1 upload
PROGRAM PID_Controller
VAR
  state: INT;
  temp_actual: REAL;
  temp_setpoint: REAL;
  deadband: REAL;
  power_output: INT;
  enable: BOOL;
  alarm: BOOL;
  error: REAL;
END_VAR
BEGIN
  (* PID-style temperature controller with state machine *)
  IF enable THEN
    (* Calculate error *)
    error := temp_setpoint - temp_actual;

    (* Alarm if out of range *)
    IF (temp_actual < 10.0) OR (temp_actual > 90.0) THEN
      alarm := TRUE;
    ELSE
      alarm := FALSE;
    END_IF;

    (* State machine with hysteresis *)
    CASE state OF
      0: (* IDLE *)
        power_output := 0;
        IF error > deadband THEN
          state := 1; (* HEATING *)
        ELSIF error < -deadband THEN
          state := 2; (* COOLING *)
        END_IF;

      1: (* HEATING *)
        (* Proportional control: P = error * 10 (max 100%) *)
        power_output := REAL_TO_INT(error * 10.0);
        IF power_output > 100 THEN
          power_output := 100;
        END_IF;

        IF error < 0.5 THEN
          state := 0; (* Return to IDLE *)
        END_IF;

      2: (* COOLING *)
        (* Negative power = cooling *)
        power_output := REAL_TO_INT(error * 10.0);
        IF power_output < -100 THEN
          power_output := -100;
        END_IF;

        IF error > -0.5 THEN
          state := 0; (* Return to IDLE *)
        END_IF;
    END_CASE;
  ELSE
    (* Disabled - all outputs off *)
    state := 0;
    power_output := 0;
    alarm := FALSE;
  END_IF;
END_PROGRAM
END_UPLOAD

set logic 1 bind enable coil:0
set logic 1 bind temp_setpoint reg:20 input
set logic 1 bind temp_actual reg:30 input
set logic 1 bind alarm coil:10 output
set logic 1 bind power_output reg:40 output

set logic 1 enabled:true

# Test scenarios
# Scenario 1: Cold start - needs heating
write coil 0 value 1
write reg 20 value uint 50
write reg 21 value uint 0
write reg 30 value uint 25
write reg 31 value uint 0
read reg 40
# Forventet: 250 → clamped to 100 (heating at max power)
read coil 10
# Forventet: 0 (alarm=FALSE, temp within range)

# Scenario 2: Approaching setpoint
write reg 30 value uint 48
write reg 31 value uint 0
read reg 40
# Forventet: 20 (proportional: (50-48)*10 = 20% power)

# Scenario 3: Too hot - needs cooling
write reg 30 value uint 60
write reg 31 value uint 0
read reg 40
# Forventet: -100 (max cooling, negative power)

# Scenario 4: Temperature alarm (out of range)
write reg 30 value uint 95
write reg 31 value uint 0
read coil 10
# Forventet: 1 (alarm=TRUE, temp > 90°C)
```

**Forventet Resultat:**
```
✅ Scenario 1: state=1 (HEATING), power_output=100 (clamped), alarm=FALSE
✅ Scenario 2: state=1 (HEATING), power_output=20 (proportional control)
✅ Scenario 3: state=2 (COOLING), power_output=-100 (max cooling)
✅ Scenario 4: alarm=TRUE (temperature out of range)

---

## ✅ Test Case 2: Multi-Tank Level Control with Cascading Logic

**Kombinerer:** Multiple REAL variables, nested IF-THEN-ELSE, boolean combinations, state transitions

**Features:**
- 3 tanks with level monitoring
- Cascade control (Tank1 → Tank2 → Tank3)
- Pump control based on multi-level logic
- Flow rate calculations
- Overflow/underflow protection

**Variables:**
- `tank1_level`, `tank2_level`, `tank3_level` (REAL): Level in % (0-100)
- `pump1_on`, `pump2_on`, `pump3_on` (BOOL): Pump states
- `flow_rate` (REAL): Current flow (L/min)
- `total_volume` (REAL): Accumulated volume (L)
- `overflow_alarm`, `underflow_alarm` (BOOL): Alarms
- `auto_mode` (BOOL): Automatic control enable

**Test Commands:**

```bash
set logic 2 delete
set logic 2 upload
PROGRAM Tank_Cascade
VAR
  tank1_level: REAL;
  tank2_level: REAL;
  tank3_level: REAL;
  pump1_on: BOOL;
  pump2_on: BOOL;
  pump3_on: BOOL;
  flow_rate: REAL;
  total_volume: REAL;
  overflow_alarm: BOOL;
  underflow_alarm: BOOL;
  auto_mode: BOOL;
END_VAR
BEGIN
  (* Multi-tank cascade control with overflow protection *)
  IF auto_mode THEN
    (* Overflow detection *)
    IF (tank1_level > 95.0) OR (tank2_level > 95.0) OR (tank3_level > 95.0) THEN
      overflow_alarm := TRUE;
      pump1_on := FALSE;
      pump2_on := FALSE;
      pump3_on := FALSE;
    ELSE
      overflow_alarm := FALSE;

      (* Pump 1: Fill Tank1 if below 30%, stop if above 80% *)
      IF tank1_level < 30.0 THEN
        pump1_on := TRUE;
      ELSIF tank1_level > 80.0 THEN
        pump1_on := FALSE;
      END_IF;

      (* Pump 2: Transfer Tank1→Tank2 if Tank1 high AND Tank2 low *)
      IF (tank1_level > 70.0) AND (tank2_level < 40.0) THEN
        pump2_on := TRUE;
      ELSIF (tank1_level < 50.0) OR (tank2_level > 85.0) THEN
        pump2_on := FALSE;
      END_IF;

      (* Pump 3: Transfer Tank2→Tank3 if Tank2 high AND Tank3 low *)
      IF (tank2_level > 70.0) AND (tank3_level < 40.0) THEN
        pump3_on := TRUE;
      ELSIF (tank2_level < 50.0) OR (tank3_level > 85.0) THEN
        pump3_on := FALSE;
      END_IF;
    END_IF;

    (* Underflow alarm if any tank critical low *)
    IF (tank1_level < 5.0) OR (tank2_level < 5.0) OR (tank3_level < 5.0) THEN
      underflow_alarm := TRUE;
    ELSE
      underflow_alarm := FALSE;
    END_IF;

    (* Calculate flow rate (simplified: 10 L/min per pump) *)
    flow_rate := 0.0;
    IF pump1_on THEN flow_rate := flow_rate + 10.0; END_IF;
    IF pump2_on THEN flow_rate := flow_rate + 10.0; END_IF;
    IF pump3_on THEN flow_rate := flow_rate + 10.0; END_IF;

    (* Accumulate volume (assume 100ms cycle = 0.1s) *)
    total_volume := total_volume + (flow_rate * 0.1 / 60.0);

  ELSE
    (* Manual mode - all off *)
    pump1_on := FALSE;
    pump2_on := FALSE;
    pump3_on := FALSE;
    overflow_alarm := FALSE;
    underflow_alarm := FALSE;
  END_IF;
END_PROGRAM
END_UPLOAD

set logic 2 bind tank1_level reg:50 input
set logic 2 bind tank2_level reg:52 input
set logic 2 bind tank3_level reg:54 input
set logic 2 bind auto_mode coil:0
set logic 2 bind pump1_on coil:20 output
set logic 2 bind pump2_on coil:21 output
set logic 2 bind pump3_on coil:22 output
set logic 2 bind overflow_alarm coil:23 output
set logic 2 bind underflow_alarm coil:24 output
set logic 2 bind total_volume reg:56 output

set logic 2 enabled:true

# Test scenarios
# Scenario 1: All tanks empty - start fill
write coil 0 value 1
write reg 50 value uint 10
write reg 51 value uint 0
write reg 52 value uint 10
write reg 53 value uint 0
write reg 54 value uint 10
write reg 55 value uint 0
read coil 20 3
# Forventet: 1 0 0 (pump1_on=TRUE, pump2_on=FALSE, pump3_on=FALSE)

# Scenario 2: Tank1 full, Tank2 empty - cascade
write reg 50 value uint 75
write reg 52 value uint 20
write reg 54 value uint 30
read coil 20 3
# Forventet: 1 1 0 (pump1_on=TRUE, pump2_on=TRUE cascade, pump3_on=FALSE)

# Scenario 3: Overflow condition
write reg 50 value uint 97
read coil 23
# Forventet: 1 (overflow_alarm=TRUE)
read coil 20 3
# Forventet: 0 0 0 (all pumps OFF on overflow)

# Scenario 4: Underflow alarm
write reg 50 value uint 30
write reg 52 value uint 3
read coil 24
# Forventet: 1 (underflow_alarm=TRUE, tank2 < 5%)
```

**Forventet Resultat:**
```
✅ Scenario 1: pump1_on=TRUE (fill Tank1 <30%), pump2/3=FALSE
✅ Scenario 2: pump1_on=TRUE, pump2_on=TRUE (cascade active), pump3_on=FALSE
✅ Scenario 3: overflow_alarm=TRUE (tank1 >95%), all pumps stop
✅ Scenario 4: underflow_alarm=TRUE (tank2 <5%)

---

## ✅ Test Case 3: Production Line Counter with Quality Control

**Kombinerer:** Integer arithmetic, division, percentage calculation, CASE state machine, alarms

**Features:**
- Production counting with good/bad parts
- Quality percentage calculation
- Batch control (100 parts per batch)
- Automatic stop on quality failure
- Running statistics

**Variables:**
- `total_count`, `good_count`, `bad_count`, `batch_number` (INT)
- `quality_percent` (REAL): Calculated (good/total * 100)
- `running`, `batch_complete`, `quality_alarm` (BOOL)
- `state` (INT): 0=STOPPED, 1=RUNNING, 2=BATCH_DONE, 3=ALARM

**Test Commands:**

```bash
set logic 3 delete
set logic 3 upload
PROGRAM Quality_Control
VAR
  total_count: INT;
  good_count: INT;
  bad_count: INT;
  batch_number: INT;
  quality_percent: REAL;
  running: BOOL;
  batch_complete: BOOL;
  quality_alarm: BOOL;
  state: INT;
END_VAR
BEGIN
  (* Production line with quality monitoring *)
  CASE state OF
    0: (* STOPPED *)
      batch_complete := FALSE;
      quality_alarm := FALSE;

      IF running THEN
        state := 1;
        total_count := 0;
      END_IF;

    1: (* RUNNING *)
      (* Update totals *)
      total_count := good_count + bad_count;

      (* Calculate quality percentage (avoid division by zero) *)
      IF total_count > 0 THEN
        quality_percent := INT_TO_REAL(good_count * 100) / INT_TO_REAL(total_count);
      ELSE
        quality_percent := 100.0;
      END_IF;

      (* Check quality threshold (must be > 95%) *)
      IF (total_count > 10) AND (quality_percent < 95.0) THEN
        state := 3; (* ALARM *)
        quality_alarm := TRUE;
      END_IF;

      (* Check batch complete (100 parts) *)
      IF total_count >= 100 THEN
        state := 2; (* BATCH_DONE *)
        batch_complete := TRUE;
        batch_number := batch_number + 1;
      END_IF;

      IF NOT running THEN
        state := 0; (* STOPPED *)
      END_IF;

    2: (* BATCH_DONE *)
      (* Wait for reset *)
      IF NOT running THEN
        state := 0;
        total_count := 0;
        batch_complete := FALSE;
      END_IF;

    3: (* ALARM *)
      (* Quality failure - stop production *)
      quality_alarm := TRUE;

      IF NOT running THEN
        state := 0;
        quality_alarm := FALSE;
      END_IF;
  END_CASE;
END_PROGRAM
END_UPLOAD

set logic 3 bind running coil:0
set logic 3 bind good_count reg:60 input
set logic 3 bind bad_count reg:61 input
set logic 3 bind batch_complete coil:30 output
set logic 3 bind quality_alarm coil:31 output
set logic 3 bind quality_percent reg:70 output
set logic 3 bind batch_number reg:72 output

set logic 3 enabled:true

# Test scenarios
# Scenario 1: Start production - good quality
write coil 0 value 1
write reg 60 value int 45
write reg 61 value int 2
read reg 70 2
# Forventet: 95.74% as REAL (45/(45+2)*100 = 95.74)

# Scenario 2: Batch complete
write reg 60 value int 97
write reg 61 value int 3
read coil 30
# Forventet: 1 (batch_complete=TRUE, 100 parts reached)
read reg 72
# Forventet: 1 (batch_number incremented)

# Scenario 3: Poor quality alarm
write coil 0 value 0
write coil 0 value 1
write reg 60 value int 15
write reg 61 value int 10
read coil 31
# Forventet: 1 (quality_alarm=TRUE, 60% < 95%)
read reg 70 2
# Forventet: 60.0% as REAL (15/25*100 = 60%)

# Scenario 4: Reset
write coil 0 value 0
read coil 30
# Forventet: 0 (batch_complete=FALSE after reset)
read coil 31
# Forventet: 0 (quality_alarm=FALSE after reset)
```

**Forventet Resultat:**
```
✅ Scenario 1: quality_percent=95.74% (45/47), state=RUNNING
✅ Scenario 2: batch_complete=TRUE, batch_number=1, state=BATCH_DONE
✅ Scenario 3: quality_alarm=TRUE (60% < 95%), state=ALARM
✅ Scenario 4: state=STOPPED, all alarms cleared

---

## ✅ Test Case 4: HVAC System with Multiple Zones

**Kombinerer:** Multiple REAL inputs, complex boolean logic, averaging, min/max functions (via logic)

**Features:**
- 4 temperature zones
- Average temperature calculation
- Heating/cooling based on average
- Individual zone alarms
- Energy-saving mode

**Test Commands:**

```bash
set logic 4 delete
set logic 4 upload
PROGRAM HVAC_Control
VAR
  zone1_temp: REAL;
  zone2_temp: REAL;
  zone3_temp: REAL;
  zone4_temp: REAL;
  avg_temp: REAL;
  setpoint: REAL;
  heating_on: BOOL;
  cooling_on: BOOL;
  zone1_alarm: BOOL;
  zone2_alarm: BOOL;
  zone3_alarm: BOOL;
  zone4_alarm: BOOL;
  system_enable: BOOL;
  eco_mode: BOOL;
END_VAR
BEGIN
  (* Multi-zone HVAC with averaging and alarms *)
  IF system_enable THEN
    (* Calculate average temperature *)
    avg_temp := (zone1_temp + zone2_temp + zone3_temp + zone4_temp) / 4.0;

    (* Individual zone alarms (out of range 15-30°C) *)
    zone1_alarm := (zone1_temp < 15.0) OR (zone1_temp > 30.0);
    zone2_alarm := (zone2_temp < 15.0) OR (zone2_temp > 30.0);
    zone3_alarm := (zone3_temp < 15.0) OR (zone3_temp > 30.0);
    zone4_alarm := (zone4_temp < 15.0) OR (zone4_temp > 30.0);

    (* Heating/cooling control with deadband *)
    IF eco_mode THEN
      (* Eco mode: wider deadband (±2°C) *)
      IF avg_temp < (INT_TO_REAL(REAL_TO_INT(setpoint)) - 2.0) THEN
        heating_on := TRUE;
        cooling_on := FALSE;
      ELSIF avg_temp > (INT_TO_REAL(REAL_TO_INT(setpoint)) + 2.0) THEN
        heating_on := FALSE;
        cooling_on := TRUE;
      ELSE
        heating_on := FALSE;
        cooling_on := FALSE;
      END_IF;
    ELSE
      (* Normal mode: tight deadband (±0.5°C) *)
      IF avg_temp < (INT_TO_REAL(REAL_TO_INT(setpoint)) - 0.5) THEN
        heating_on := TRUE;
        cooling_on := FALSE;
      ELSIF avg_temp > (INT_TO_REAL(REAL_TO_INT(setpoint)) + 0.5) THEN
        heating_on := FALSE;
        cooling_on := TRUE;
      END_IF;
    END_IF;

  ELSE
    (* System disabled *)
    heating_on := FALSE;
    cooling_on := FALSE;
    zone1_alarm := FALSE;
    zone2_alarm := FALSE;
    zone3_alarm := FALSE;
    zone4_alarm := FALSE;
  END_IF;
END_PROGRAM
END_UPLOAD

set logic 4 bind zone1_temp reg:80 input
set logic 4 bind zone2_temp reg:82 input
set logic 4 bind zone3_temp reg:84 input
set logic 4 bind zone4_temp reg:86 input
set logic 4 bind setpoint reg:88 input
set logic 4 bind system_enable coil:0
set logic 4 bind eco_mode coil:1
set logic 4 bind heating_on coil:40 output
set logic 4 bind cooling_on coil:41 output
set logic 4 bind zone1_alarm coil:42 output
set logic 4 bind zone2_alarm coil:43 output
set logic 4 bind zone3_alarm coil:44 output
set logic 4 bind zone4_alarm coil:45 output
set logic 4 bind avg_temp reg:90 output

set logic 4 enabled:true

# Test scenarios
# Scenario 1: All zones normal, avg < setpoint
write coil 0 value 1
write coil 1 value 0
write reg 88 value int 22
write reg 80 value uint 20
write reg 81 value uint 0
write reg 82 value uint 21
write reg 83 value uint 0
write reg 84 value uint 19
write reg 85 value uint 0
write reg 86 value uint 20
write reg 87 value uint 0
read reg 90 2
# Forventet: 20.0 as REAL (avg = (20+21+19+20)/4)
read coil 40 2
# Forventet: 1 0 (heating_on=TRUE, cooling_on=FALSE, 20 < 22-0.5)

# Scenario 2: Eco mode - wider deadband
write coil 1 value 1
read coil 40 2
# Forventet: 0 0 (heating_on=FALSE, cooling_on=FALSE, 20 in deadband 22±2)

# Scenario 3: Zone alarm (zone1 too cold)
write reg 80 value uint 12
write reg 81 value uint 0
read coil 42
# Forventet: 1 (zone1_alarm=TRUE, 12°C < 15°C)

# Scenario 4: Too hot - cooling needed
write coil 1 value 0
write reg 80 value uint 25
write reg 82 value uint 26
write reg 84 value uint 24
write reg 86 value uint 25
read reg 90 2
# Forventet: 25.0 as REAL (avg = (25+26+24+25)/4)
read coil 40 2
# Forventet: 0 1 (heating_on=FALSE, cooling_on=TRUE, 25 > 22+0.5)
```

**Forventet Resultat:**
```
✅ Scenario 1: avg_temp=20.0, heating_on=TRUE (normal mode, 20 < 21.5)
✅ Scenario 2: heating_on=FALSE, cooling_on=FALSE (eco mode, within deadband)
✅ Scenario 3: zone1_alarm=TRUE (zone1 temp 12°C < 15°C minimum)
✅ Scenario 4: avg_temp=25.0, cooling_on=TRUE (25 > 22.5)

---

## ✅ Test Case 5: Batch Mixer with Recipe Control

**Kombinerer:** Multi-step sequence, timing simulation, weight calculations, safety interlocks

**Features:**
- 5-step recipe (FILL, MIX, HEAT, COOL, DRAIN)
- Weight-based control
- Temperature monitoring
- Safety interlocks (door, emergency stop)
- Step completion tracking

**Test Commands:**

```bash
set logic 1 delete
set logic 1 upload
PROGRAM Batch_Recipe
VAR
  step: INT;
  weight: REAL;
  temperature: REAL;
  target_weight: REAL;
  target_temp: REAL;
  fill_valve: BOOL;
  mix_motor: BOOL;
  heater: BOOL;
  cooler: BOOL;
  drain_valve: BOOL;
  door_closed: BOOL;
  estop: BOOL;
  recipe_running: BOOL;
  batch_complete: BOOL;
  safety_alarm: BOOL;
END_VAR
BEGIN
  (* 5-step batch recipe with safety interlocks *)

  (* Safety check - must have door closed and no e-stop *)
  IF (NOT door_closed) OR estop THEN
    safety_alarm := TRUE;
    fill_valve := FALSE;
    mix_motor := FALSE;
    heater := FALSE;
    cooler := FALSE;
    drain_valve := FALSE;
    step := 0;
  ELSE
    safety_alarm := FALSE;

    IF recipe_running THEN
      batch_complete := FALSE;

      CASE step OF
        0: (* FILL *)
          fill_valve := TRUE;

          IF weight >= INT_TO_REAL(REAL_TO_INT(target_weight)) THEN
            fill_valve := FALSE;
            step := 1;
          END_IF;

        1: (* MIX *)
          mix_motor := TRUE;

          (* Simulate: mix for 10 cycles (weight stable check) *)
          IF weight >= INT_TO_REAL(REAL_TO_INT(target_weight)) THEN
            step := 2;
          END_IF;

        2: (* HEAT *)
          mix_motor := TRUE;
          heater := TRUE;

          IF temperature >= INT_TO_REAL(REAL_TO_INT(target_temp)) THEN
            heater := FALSE;
            step := 3;
          END_IF;

        3: (* COOL *)
          mix_motor := TRUE;
          cooler := TRUE;

          IF temperature <= 30.0 THEN
            cooler := FALSE;
            mix_motor := FALSE;
            step := 4;
          END_IF;

        4: (* DRAIN *)
          drain_valve := TRUE;

          IF weight < 5.0 THEN
            drain_valve := FALSE;
            step := 5;
          END_IF;

        5: (* COMPLETE *)
          batch_complete := TRUE;

          IF NOT recipe_running THEN
            step := 0;
            batch_complete := FALSE;
          END_IF;
      END_CASE;

    ELSE
      (* Not running - all off *)
      fill_valve := FALSE;
      mix_motor := FALSE;
      heater := FALSE;
      cooler := FALSE;
      drain_valve := FALSE;
      step := 0;
      batch_complete := FALSE;
    END_IF;
  END_IF;
END_PROGRAM
END_UPLOAD

set logic 1 bind weight reg:100 input
set logic 1 bind temperature reg:102 input
set logic 1 bind target_weight reg:104 input
set logic 1 bind target_temp reg:105 input
set logic 1 bind recipe_running coil:0
set logic 1 bind door_closed coil:1
set logic 1 bind estop coil:2
set logic 1 bind fill_valve coil:50 output
set logic 1 bind mix_motor coil:51 output
set logic 1 bind heater coil:52 output
set logic 1 bind cooler coil:53 output
set logic 1 bind drain_valve coil:54 output
set logic 1 bind batch_complete coil:55 output
set logic 1 bind safety_alarm coil:56 output

set logic 1 enabled:true

# Test scenarios
# Scenario 1: Start fill (safety OK)
write coil 0 value 1
write coil 1 value 1
write coil 2 value 0
write reg 104 value int 100
write reg 105 value int 60
write reg 100 value uint 0
write reg 101 value uint 0
write reg 102 value uint 20
write reg 103 value uint 0
read coil 50
# Forventet: 1 (fill_valve=TRUE, step=0 FILL)

# Scenario 2: Weight reached - move to mix
write reg 100 value uint 100
write reg 101 value uint 0
read coil 50 2
# Forventet: 0 1 (fill_valve=FALSE, mix_motor=TRUE, step=1 MIX)

# Scenario 3: Heating phase
read coil 51 2
# Forventet: 1 1 (mix_motor=TRUE, heater=TRUE, step=2 HEAT)

# Scenario 4: Safety interlock (door open)
write coil 1 value 0
read coil 56
# Forventet: 1 (safety_alarm=TRUE)
read coil 50 5
# Forventet: 0 0 0 0 0 (all valves OFF on safety alarm)

# Scenario 5: Resume and complete
write coil 1 value 1
write reg 102 value uint 65
write reg 103 value uint 0
read coil 51 2
# Forventet: 1 0 (mix_motor=TRUE, heater=FALSE, temp reached, step=3 COOL)
write reg 102 value uint 25
read coil 51 2
# Forventet: 0 1 (mix_motor=FALSE, cooler=TRUE, cooled down, step=4 DRAIN)
write reg 100 value uint 2
read coil 54
# Forventet: 1 (drain_valve=TRUE, draining)
write reg 100 value uint 0
read coil 55
# Forventet: 1 (batch_complete=TRUE, step=5 COMPLETE)
```

**Forventet Resultat:**
```
✅ Scenario 1: step=FILL, fill_valve=TRUE (safety OK, starting fill)
✅ Scenario 2: step=MIX, fill_valve=FALSE, mix_motor=TRUE (weight reached)
✅ Scenario 3: step=HEAT, heater=TRUE, mix_motor=TRUE (mixing while heating)
✅ Scenario 4: safety_alarm=TRUE, all outputs=FALSE (door open interlock)
✅ Scenario 5: COOL→DRAIN→COMPLETE, batch_complete=TRUE (full cycle)

---

## 📊 Performance Expectations

| Test Case | Variables | Instructions (Est.) | Complexity | Exec Time (Est.) |
|-----------|-----------|---------------------|------------|------------------|
| TC1: PID Controller | 8 | 80-100 | ⭐⭐⭐ | <2ms |
| TC2: Tank Cascade | 11 | 100-120 | ⭐⭐⭐⭐ | <3ms |
| TC3: Quality Control | 9 | 60-80 | ⭐⭐⭐ | <2ms |
| TC4: HVAC System | 14 | 70-90 | ⭐⭐⭐ | <2ms |
| TC5: Batch Recipe | 15 | 90-110 | ⭐⭐⭐⭐ | <3ms |

**All tests should complete within ST Logic execution interval (default 100ms).**

---

## ✅ Validation Checklist

For each test case, verify:

- [ ] Compilation successful (no syntax errors)
- [ ] All variables bound correctly (input/output)
- [ ] State transitions work as expected
- [ ] REAL arithmetic produces correct results
- [ ] Type conversions (INT_TO_REAL, REAL_TO_INT) work
- [ ] Boolean logic evaluates correctly
- [ ] Edge cases handled (division by zero, overflow)
- [ ] Alarms activate on correct conditions
- [ ] Multi-register REAL I/O synchronized
- [ ] Performance: `show logic <id> timing` shows <5ms avg

---

## 🎯 Success Criteria

**PASS if:**
1. All 5 test cases compile without errors
2. All scenarios produce expected outputs
3. No crashes or watchdog resets
4. Execution times within limits
5. Memory usage stable (no leaks)

**BONUS VALIDATION:**
- Run all 5 programs simultaneously (Logic1-4 + one repeated)
- Monitor global cycle stats: `show logic stats`
- Verify no overruns: `read input 288 2` (cycle overrun count = 0)

---

**End of Complex Test Cases**
**Version:** v4.4.0 | **Build:** #881 | **Date:** 2025-12-30
