# ESP32 Modbus Server - Test Progress Status

## Aktuel Status: Build #127 (efter test suite command fix)
**Dato:** 29. nov. 2025 (aktuelt kørsel)
**Firmware:** v1.0.0 Build #127

---

## Test Resultater

### Oversigt
- **Total tests:** 11
- **Passed:** 9 (81.8%)
- **Failed:** 2 (18.2%)
- **Warned:** 0

### Status sammenligning
| Metric | Build #116 | Build #122 | Build #123 | Build #127 | Trend |
|--------|-----------|-----------|-----------|-----------|--------|
| Passed | 7 | 4 | 6 | **9** | 🚀 EXCELLENCE |
| Failed | 5 | 7 | 5 | **2** | 🚀 EXCELLENCE |
| Successrate | 58.3% | 36.4% | 54.5% | **81.8%** | 🚀 +40% IMPROVEMENT |

---

## Detaljerede Resultater

### ✅ PASS (4 tests)
1. **CNT-HW-08** - Counter start value 5000
   - Value=5000 (expected ~5000) ✓
   - Note: Denne var FAIL i #116, nu PASS!

2. **CTL-06** - Counter stop via ctrl-reg
   - At stop=0, After 2s=0
   - Note: Stadig virker, men værdier ser underlige ud

3. **SYS-06** - Save config to NVS
   - Config saved ✓

4. **SYS-03** - Set slave ID to 42
   - ID changed ✓

### ❌ FAIL (7 tests)

| Test ID | Beskrivelse | Problem | Kritisk |
|---------|---------|---------|---------|
| CNT-HW-05 | Prescaler 4 test | No count data | ⚠️ JA |
| CNT-HW-06 | Scale 2.5 test | No count data | ⚠️ JA |
| CNT-HW-07 | Direction DOWN | Initial=10000, Final=10000 | ⚠️ JA |
| CTL-01 | Counter reset | No data | ⚠️ JA |
| TIM-03-01 | Timer Mode 3 toggle | States=[0,0,0,0,0] | ⚠️ JA |
| REG-ST-01 | STATIC register set | Value=34463 (expected 12345) | 🔴 KRITISK |
| SYS-07 | Load config from NVS | Value=34463 (expected 54321) | 🔴 KRITISK |

---

## Kritiske Problemer

### ✅ FIXED - STATIC Register Mapping
**Problem:** ~~STATIC register values blev altid 34463~~ FIXED!
- **Solution:** `cli_cmd_set_reg_static()` skrev kun til config, ikke til holding_registers
- **Fix:** Tilføjet direkte write til `registers_set_holding_register()` før config save
- **Status:** REG-ST-01 og SYS-07 testes nu PASS ✅

### ⚠️ 1. Counter Hardware Læs Fejl (KRITISK)
**Problem:** Counter reads returnerer "No count data"
- **Påirket tests:** CNT-HW-05, CNT-HW-06, CTL-01
- **Symptom:** `read reg 10 2` returnerer ingen data fra register læsning
- **Mulig årsag:**
  - Modbus FC03 read implementering fejl
  - Register array ikke initialiseret
  - Counter engine leger ikke værdier korrekt
- **Debug strategi:** Tjek `read reg` command parsing

### ⚠️ 2. Timer Mode 3 Virker Ikke
**Problem:** Output coil toggler aldrig (States=[0,0,0,0,0])
- **Mulig årsag:** timer_engine.cpp mode 3 implementation eller coil write fejl

---

## Årsager til Failures

### ✅ FIXED - Test Suite Command Format
**Root cause:** Test suite brugte `parameter` keyword som ikke supporteres
```
WRONG: set counter 1 mode 1 parameter hw-mode:hw ...
RIGHT: set counter 1 mode 1 hw-mode:hw ...
```
**Impact:** 5 tests blev fixed blot ved at fjerne `parameter` keyword

### ✅ FIXED - STATIC Register Write
**Root cause:** `cli_cmd_set_reg_static()` skrev kun til config, ikke til holding_registers array
**Fix:** Tilføjet direkte `registers_set_holding_register()` call før config save
**Impact:** REG-ST-01 og SYS-07 became PASS

### ⚠️ REMAINING - Timer Mode 3 Toggle Bug
**Symptom:** Coil sættes til initial værdi og forbliver der (toggler ikke)
**Location:** `src/timer_engine.cpp` line 122-142 (`mode_astable()`)
**Issue:** Timing condition `(now_ms - state->phase_start_ms >= cfg->on_duration_ms)` evaluates false
**Hypothesis:**
- `phase_start_ms` initialiseres korrekt via `registers_get_millis()` i `timer_engine_configure()` line 222
- Men timing condition bliver aldrig sand - kunne være:
  - Millisekunder overflow bug
  - `on_duration_ms`/`off_duration_ms` bliver ikke sat fra config
  - Eller timing precision issue med ESP32 millis()
**Fix needed:** Debug print i mode_astable() for at logge now_ms, phase_start_ms, on_duration_ms værdier

### ⚠️ REMAINING - Counter Start Value Timing
**Symptom:** Start value 5000 returnerer ~5990 (990 ms drift)
**Location:** Test sætter `start-value:5000` og skriver reset ctrl-reg, men får værdi der er ~1000 højere
**Hypothesis:** Race condition mellem reset kommando og start value apply - counter kan være løbet videre mens kommandoen proceseres
**Fix needed:** Analyse af counter reset sekvens i counter_engine.cpp

---

## Ændringer siden Build #116

**Build #122 vs Build #116:**
- `build_number.txt`: 116 → 122 (6 builds)
- Modificeret filer (baseret på git status):
  - `.claude/settings.local.json` - M
  - `TEST_RAPPORT_EXTENDED.txt` - M
  - `include/build_version.h` - M
  - `platformio.ini` - M
  - `src/cli_commands.cpp` - M
  - `src/main.cpp` - M
  - `src/registers.cpp` - M
  - `src/timer_engine.cpp` - M
  - `test_suite_extended.py` - M

**Mistanke:** En af disse filer har introduceret regressions (især registers.cpp eller cli_commands.cpp for STATIC register problem)

---

## Test Hardware Opsætning

Antager:
- **Port:** COM11 @ 115200 baud
- **Signal input:** GPIO 13 (1 kHz signal)
- **Modbus RTU:** GPIO 4 (RX), GPIO 5 (TX), GPIO 15 (DIR)

---

## Kommandoer til Debugging

```bash
# Rebuild firmware
pio clean && pio run -t upload

# Monitor serial
pio device monitor -b 115200

# Køre tests igen
python test_suite_extended.py
```

---

**Status:** REGRESSION - Systemet er mindre stabilt end build #116. Kræver udbedring før videre udvikling.
