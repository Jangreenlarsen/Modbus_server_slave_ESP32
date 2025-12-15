# Counter Configuration Templates - ESP32 Modbus RTU Server

**Version:** v4.2.0
**Dato:** 2025-12-15

---

## 📋 Overview

Dette dokument indeholder **pre-konfigurerede CLI templates** til almindelige counter setups. Kopier og tilpas efter dine behov.

### Smart Register Defaults (v4.2.0+)

Starting with v4.2.0, counters får **automatiske logiske register adresser** hvis ikke specificeret:

| Counter | Index Reg | Raw Reg | Freq Reg | Overload Reg | Ctrl Reg |
|---------|-----------|---------|----------|--------------|----------|
| 1       | HR100     | HR101   | HR102    | HR103        | HR104    |
| 2       | HR110     | HR111   | HR112    | HR113        | HR114    |
| 3       | HR120     | HR121   | HR122    | HR123        | HR124    |
| 4       | HR130     | HR131   | HR132    | HR133        | HR134    |

**Du behøver IKKE specificere disse**, men kan override hvis ønsket.

---

## 🔧 Configuration Templates

### **Template 1: HW Counter (PCNT Mode)**

Bruges til at tælle hardware pulser direkte via GPIO med PCNT (Pulse Counter).

**Hardware setup:**
- Counter 1: GPIO25 (ESP32 PCNT Unit 0)
- Counter 2: GPIO26 (ESP32 PCNT Unit 1)
- Counter 3: GPIO27 (ESP32 PCNT Unit 2)
- Counter 4: GPIO33 (ESP32 PCNT Unit 3)

**CLI Commands:**

```bash
# Counter 1 - HW Mode (PCNT)
set counter 1 mode 1 hw-mode:hw hw-gpio:25 edge:rising prescaler:1
set counter 1 control running:on auto-start:on
save

# Counter 2 - HW Mode (PCNT)
set counter 2 mode 1 hw-mode:hw hw-gpio:26 edge:rising prescaler:1
set counter 2 control running:off
save

# Counter 3 - HW Mode (PCNT)
set counter 3 mode 1 hw-mode:hw hw-gpio:27 edge:rising prescaler:1
set counter 3 control running:off
save

# Counter 4 - HW Mode (PCNT)
set counter 4 mode 1 hw-mode:hw hw-gpio:33 edge:rising prescaler:1
set counter 4 control running:off
save
```

**Forklaring:**
- `hw-mode:hw` - Brug hardware PCNT (mest præcis)
- `hw-gpio:25` - GPIO pin for PCNT input
- `edge:rising` - Tæl stijgende flanker
- `prescaler:1` - Ingen prescaler (tæl alle pulser)
- `running:on` - Start counter umiddelbart
- `auto-start:on` - Start automatically efter reboot

**Registre:**
- HR100: Index value (scaled)
- HR101: Raw value (prescaled)
- HR102: Frequency (Hz)
- HR103: Overload flag
- HR104: Control register

---

### **Template 2: SW Mode Counter (GPIO Polling)**

Bruges til at læse en diskret input fra GPIO periodisk (polling).

```bash
# Counter 2 - SW Mode (Polling)
set counter 2 mode 1 hw-mode:sw input-dis:45 edge:rising prescaler:1
set counter 2 control running:on
save
```

**Forklaring:**
- `hw-mode:sw` - Software polling (læs GPIO hver iteration)
- `input-dis:45` - Diskret input #45 (mappes til en GPIO)
- `edge:rising` - Detect stijgende flanker
- `prescaler:1` - Tæl alle flanker

**Fordele:**
- Fungerer på alle GPIO pins
- Ingen hardware-setup nødvendig

**Ulemper:**
- Mindre præcis end HW (kan misse pulser hvis for hurtig)
- Bruger mere CPU (polling i hver main loop iteration)

---

### **Template 3: SW-ISR Mode Counter (Interrupt-Driven)**

Bruges til høj-frekvens tælling med hardware interrupt (mest præcis software mode).

```bash
# Counter 3 - SW-ISR Mode (Interrupt-driven)
set counter 3 mode 1 hw-mode:sw-isr interrupt-pin:26 edge:rising prescaler:1
set counter 3 control running:on
save
```

**Forklaring:**
- `hw-mode:sw-isr` - Software ISR mode (interrupt-driven)
- `interrupt-pin:26` - GPIO pin som skal bruges til interrupt
- `edge:rising` - Trigger ISR på stijgende flanker
- `prescaler:1` - Tæl alle flanker

**Fordele:**
- Høj præcision (interrupt-baseret)
- CPU-effektiv (ingen polling)

**Ulemper:**
- Kræver disponibel interrupt pin
- Kan have jitter ved meget høje frekvenser

---

### **Template 4: Prescaler Setup**

Hvis du vil tælle kun hver N'te puls (for at reducere overflow):

```bash
# Counter 1 - Prescaler 10 (tæl kun hver 10. puls)
set counter 1 mode 1 hw-mode:hw hw-gpio:25 edge:rising prescaler:10
set counter 1 control running:on
save
```

**Eksempel:**
- Uden prescaler: Counter tæller 0 til 65535 (16-bit), derefter overløb
- Med prescaler:10: Counter tæller 0 til 655350 før overløb (10× mere range)

**Note:** Prescaler påvirker IKKE frekvens-måling (Hz register) - det er altid præcist.

---

### **Template 5: Compare Feature (Alarm ved værdi)**

Trig alarm når counter når en bestemt værdi:

```bash
# Counter 1 - Alert når værdi ≥ 1000
set counter 1 mode 1 hw-mode:hw hw-gpio:25 edge:rising prescaler:1
set counter 1 mode 2 compare:on compare-value:1000 compare-mode:0
set counter 1 control running:on
save
```

**Forklaring:**
- `compare:on` - Aktivér compare feature
- `compare-value:1000` - Sammenlign med denne værdi
- `compare-mode:0` - Mode: 0=≥ (greater-or-equal), 1=> (greater), 2=== (exact)

**Resultatet:**
- Når counter ≥ 1000: Bit 4 i control register sættes til 1
- Master kan læse HR104 bit 4 for at detektere alarm
- `reset-on-read:1` - Auto-clear alarm ved læsning (default)

---

### **Template 6: Reset on Read Feature**

Counter resets til start-værdi når den læses af Modbus master:

```bash
# Counter 1 - Reset to 0 efter læsning
set counter 1 mode 1 hw-mode:hw hw-gpio:25 edge:rising prescaler:1
set counter 1 mode 2 start-value:0 reset-on-read:1
set counter 1 control running:on
save
```

**Use case:**
- Master læser HR100 (counter værdi)
- Counter resets til start-value:0
- Næste iteration tæller fra 0 igen
- Effektivt: "read and reset" i en operation

---

### **Template 7: Scale Factor**

Multiplicer counter værdi med scale factor for unit conversion:

```bash
# Counter 1 - Scale 0.5 (hver puls = 0.5 enheder)
set counter 1 mode 1 hw-mode:hw hw-gpio:25 edge:rising prescaler:1
set counter 1 mode 2 scale:0.5
set counter 1 control running:on
save
```

**Eksempel:**
- 100 pulses tælt
- Scale factor: 0.5
- HR100 (index register): 100 × 0.5 = 50 (scaled)
- HR101 (raw register): 100 / 1 = 100 (prescaled)

**Use case:**
- Puls-frekvens til flow-rate konvertering
- Tand-ratio kompensation
- Unit scaling

---

### **Template 8: Full Setup (Alle 4 Counters)**

Komplet konfiguration af alle 4 counters med forskellige modes:

```bash
# ==========================================
# COUNTER 1: HW Mode (PCNT) - Høj præcision
# ==========================================
set counter 1 mode 1 hw-mode:hw hw-gpio:25 edge:rising prescaler:1
set counter 1 mode 2 start-value:0 scale:1.0
set counter 1 control running:on auto-start:on
save

# ==========================================
# COUNTER 2: SW-ISR Mode - Interrupt-driven
# ==========================================
set counter 2 mode 1 hw-mode:sw-isr interrupt-pin:26 edge:rising prescaler:1
set counter 2 control running:off
save

# ==========================================
# COUNTER 3: SW Mode - Polling
# ==========================================
set counter 3 mode 1 hw-mode:sw input-dis:45 edge:rising prescaler:1
set counter 3 control running:off
save

# ==========================================
# COUNTER 4: Reserved for future use
# ==========================================
# (not configured)

# Save persistent configuration
save
```

**Output (show counters):**

```
COUNTER CONFIGURATION
Counter 1: HW (PCNT) on GPIO25, index-reg=100, running, auto-start enabled
Counter 2: SW-ISR on GPIO26, index-reg=110, stopped
Counter 3: SW on DI#45, index-reg=120, stopped
Counter 4: Not configured
```

---

## 🔍 Troubleshooting

### Problem: "ERROR: Invalid GPIO pin (must be 1-39 for ESP32-WROOM-32)"

**Årsag:** GPIO pin uden for gyldig range.

**Løsning:** Brug kun GPIO pins 1-39. Undgå strapping pins (0, 2, 15).

```bash
# FEJL: GPIO 0 er invalid
set counter 1 mode 1 hw-mode:hw hw-gpio:0

# KORREKT: Brug GPIO 25
set counter 1 mode 1 hw-mode:hw hw-gpio:25
```

---

### Problem: "WARNING: GPIO 2 is a strapping pin - may affect boot behavior!"

**Årsag:** GPIO 2, 15 påvirker boot.

**Løsning:** Undgå hvis muligt. Hvis nødvendig, vær forsigtig.

```bash
# Brug GPIO 25 i stedet
set counter 1 mode 1 hw-mode:hw hw-gpio:25
```

---

### Problem: Counter tæller ikke

**Tjekklist:**
1. `show counter 1` - Verify `en=1` (enabled)
2. `show counter 1` - Verify control register `running:on`
3. Check GPIO pin har signal
4. Verify mode (HW/SW/ISR) matcher hardware setup
5. Reboot: `reboot`

---

### Problem: Frequency register (Hz) viser 0

**Årsag:** Counter har været stoppet eller nul pulses.

**Løsning:**
1. Verify counter tæller (check value register)
2. Ensure signal på GPIO pin
3. Frekvens opdateres hver ~1 sekund

---

### Problem: Counter værdi meget høj (overflow)?

**Løsning 1:** Øg prescaler for at reducere tælle-rate

```bash
set counter 1 mode 1 hw-mode:hw hw-gpio:25 prescaler:10
```

**Løsning 2:** Aktivér reset-on-read for periodisk reset

```bash
set counter 1 mode 2 reset-on-read:1
```

---

## 📊 Register Map Reference

Hver counter bruger **5 Holding Registers:**

```
Counter 1: HR100 (index) - HR101 (raw) - HR102 (freq) - HR103 (overload) - HR104 (ctrl)
Counter 2: HR110 (index) - HR111 (raw) - HR112 (freq) - HR113 (overload) - HR114 (ctrl)
Counter 3: HR120 (index) - HR121 (raw) - HR122 (freq) - HR123 (overload) - HR124 (ctrl)
Counter 4: HR130 (index) - HR131 (raw) - HR132 (freq) - HR133 (overload) - HR134 (ctrl)
```

**Register typer:**
- **Index Register (HR100, HR110, HR120, HR130):** Scaled counter value (value × scale_factor)
- **Raw Register (HR101, HR111, HR121, HR131):** Prescaled counter value (value / prescaler)
- **Frequency Register (HR102, HR112, HR122, HR132):** Measured frequency in Hz (updated ~1/sec)
- **Overload Register (HR103, HR113, HR123, HR133):** Overflow flag (1 = overflow occurred)
- **Control Register (HR104, HR114, HR124, HR134):** Bit-mapped control flags

---

## 🎯 Best Practices

1. **Altid sæt `running:on` eller `auto-start:on`** - Counter starter ikke automatisk uden dette
2. **Brug HW (PCNT) mode for høj-frekvens** - Mest præcis og CPU-effektiv
3. **Brug SW-ISR mode for medium-frekvens** - Gode præcision uden hardware-bindinger
4. **Brug SW mode kun for lav-frekvens** - Polling er CPU-dyrere
5. **Alltid `save` efter config** - Persistent efter reboot
6. **Verify med `show counter`** - Confirmer setup før produktion

---

## 📝 Quick Reference

```bash
# Minimal HW counter setup
set counter 1 mode 1 hw-mode:hw hw-gpio:25 edge:rising prescaler:1
set counter 1 control running:on
save

# Minimal SW-ISR counter setup
set counter 2 mode 1 hw-mode:sw-isr interrupt-pin:26 edge:rising prescaler:1
set counter 2 control running:on
save

# Minimal SW counter setup
set counter 3 mode 1 hw-mode:sw input-dis:45 edge:rising prescaler:1
set counter 3 control running:on
save

# View all counters
show counters

# View specific counter config
show config
```

---

**Sidst opdateret:** 2025-12-15 (v4.2.0)
**Maintainer:** Claude Code
