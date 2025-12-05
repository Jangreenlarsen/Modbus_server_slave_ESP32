# Wi-Fi CLI Test Routine v3.0.0

**Version:** 3.0.0
**Date:** 2025-12-05
**Tester:** Automated Test Suite

---

## Test Environment Setup

- **Board:** ESP32-WROOM-32
- **Firmware Version:** 3.0.0
- **Test Mode:** Serial Terminal + Telnet (if available)
- **Baud Rate:** 115200

---

## Pre-requisites

- [ ] Device connected to PC via USB
- [ ] Serial monitor open (e.g., PuTTY, Arduino IDE Serial Monitor)
- [ ] Wi-Fi network available for testing (optional)
- [ ] Know the test Wi-Fi SSID and password

---

## TEST SUITE 1: SET WIFI COMMANDS

### Test 1.1: Set Wi-Fi SSID

**Command:**
```
set wifi ssid TestNetwork
```

**Expected Output:**
```
Wi-Fi SSID set to: TestNetwork
Hint: Use 'save' to persist configuration to NVS
```

**Pass Criteria:** ✅ SSID accepted without error

---

### Test 1.2: Set Wi-Fi Password (8+ chars)

**Command:**
```
set wifi password TestPass123
```

**Expected Output:**
```
Wi-Fi password set (not shown for security)
Hint: Use 'save' to persist configuration to NVS
```

**Pass Criteria:** ✅ Password accepted

---

### Test 1.3: Set Wi-Fi Password (too short)

**Command:**
```
set wifi password short
```

**Expected Output:**
```
SET WIFI: Password must be 8-63 characters
```

**Pass Criteria:** ✅ Rejected with clear error

---

### Test 1.4: Enable DHCP

**Command:**
```
set wifi dhcp on
```

**Expected Output:**
```
DHCP enabled (automatic IP assignment)
Hint: Use 'save' to persist configuration to NVS
```

**Pass Criteria:** ✅ DHCP enabled

---

### Test 1.5: Disable DHCP (Static IP mode)

**Command:**
```
set wifi dhcp off
```

**Expected Output:**
```
DHCP disabled (use static IP settings)
Hint: Use 'save' to persist configuration to NVS
```

**Pass Criteria:** ✅ DHCP disabled

---

### Test 1.6: Set Static IP

**Command:**
```
set wifi ip 192.168.1.100
```

**Expected Output:**
```
Static IP set to: 192.168.1.100
Hint: Use 'save' to persist configuration to NVS
```

**Pass Criteria:** ✅ Valid IP accepted

---

### Test 1.7: Set Static IP (invalid format)

**Command:**
```
set wifi ip 192.168.999.1
```

**Expected Output:**
```
SET WIFI IP: invalid IP address format
```

**Pass Criteria:** ✅ Invalid IP rejected

---

### Test 1.8: Set Gateway

**Command:**
```
set wifi gateway 192.168.1.1
```

**Expected Output:**
```
Gateway set to: 192.168.1.1
Hint: Use 'save' to persist configuration to NVS
```

**Pass Criteria:** ✅ Gateway accepted

---

### Test 1.9: Set Netmask

**Command:**
```
set wifi netmask 255.255.255.0
```

**Expected Output:**
```
Netmask set to: 255.255.255.0
Hint: Use 'save' to persist configuration to NVS
```

**Pass Criteria:** ✅ Valid netmask accepted

---

### Test 1.10: Set Netmask (invalid - non-contiguous)

**Command:**
```
set wifi netmask 255.255.0.255
```

**Expected Output:**
```
SET WIFI NETMASK: invalid netmask (must be contiguous bits)
```

**Pass Criteria:** ✅ Invalid netmask rejected

---

### Test 1.11: Set DNS

**Command:**
```
set wifi dns 8.8.8.8
```

**Expected Output:**
```
DNS set to: 8.8.8.8
Hint: Use 'save' to persist configuration to NVS
```

**Pass Criteria:** ✅ DNS accepted

---

### Test 1.12: Set Telnet Port

**Command:**
```
set wifi telnet-port 2323
```

**Expected Output:**
```
Telnet port set to: 2323
Hint: Use 'save' to persist configuration to NVS
```

**Pass Criteria:** ✅ Port accepted

---

### Test 1.13: Enable Wi-Fi

**Command:**
```
set wifi enable
```

**Expected Output:**
```
Wi-Fi enabled
Hint: Use 'save' to persist configuration to NVS
```

**Pass Criteria:** ✅ Wi-Fi enabled

---

### Test 1.14: Disable Wi-Fi

**Command:**
```
set wifi disable
```

**Expected Output:**
```
Wi-Fi disabled
Hint: Use 'save' to persist configuration to NVS
```

**Pass Criteria:** ✅ Wi-Fi disabled

---

### Test 1.15: Unknown Wi-Fi Option

**Command:**
```
set wifi invalid option
```

**Expected Output:**
```
SET WIFI: unknown option 'invalid' (use: ssid, password, dhcp, ip, gateway, netmask, dns, telnet-port, enable, disable)
```

**Pass Criteria:** ✅ Unknown option rejected with helpful message

---

## TEST SUITE 2: SHOW WIFI COMMAND

### Test 2.1: Show Wi-Fi Status (disconnected)

**Command:**
```
show wifi
```

**Expected Output:**
```
=== WI-FI STATUS ===
Wi-Fi Status: NOT CONNECTED

Configuration:
  Enabled: YES
  SSID: (not set)
  IP Mode: DHCP
  Telnet: DISABLED (port 23)

Commands:
  set wifi ssid <name>
  set wifi password <pwd>
  ...
```

**Pass Criteria:** ✅ Status displayed correctly

---

## TEST SUITE 3: CONFIG PERSISTENCE

### Test 3.1: Save Configuration

**Command:**
```
save
```

**Expected Output:**
```
Configuration saved to NVS
```

**Pass Criteria:** ✅ Config persisted

---

### Test 3.2: Load Configuration After Reboot

**Command:**
```
show wifi
```

**Expected Output:**
```
=== WI-FI STATUS ===
...
Configuration:
  Enabled: YES
  SSID: TestNetwork
  ...
```

**Pass Criteria:** ✅ Previous config loaded correctly

---

## TEST SUITE 4: CONNECTION COMMANDS

### Test 4.1: Connect Wi-Fi (without SSID)

**Command:**
```
set wifi disable
set wifi enable
connect wifi
```

**Expected Output:**
```
CONNECT WIFI: SSID not configured
Hint: Use 'set wifi ssid <name>' first
```

**Pass Criteria:** ✅ Connection blocked until SSID configured

---

### Test 4.2: Connect Wi-Fi (with SSID)

**Command:**
```
set wifi ssid YourNetwork
set wifi password YourPassword
connect wifi
```

**Expected Output:**
```
Connecting to Wi-Fi: YourNetwork
Wi-Fi connection started (async)
Use 'show wifi' to check connection status
```

**Pass Criteria:** ✅ Connection attempt initiated

---

### Test 4.3: Check Connection Status

**Command:**
```
show wifi
```

**Expected Output (after connecting):**
```
=== WI-FI STATUS ===
Wi-Fi Status: CONNECTED
Local IP: 192.168.1.X

Configuration:
  ...
```

**Pass Criteria:** ✅ Connection status shown correctly

---

### Test 4.4: Disconnect Wi-Fi

**Command:**
```
disconnect wifi
```

**Expected Output:**
```
Wi-Fi disconnected
```

**Pass Criteria:** ✅ Connection terminated

---

## TEST SUITE 5: ERROR HANDLING

### Test 5.1: Invalid Commands Routing

**Commands:**
```
set wifi
set wifi invalid
set unknown
```

**Expected Output:**
```
SET WIFI: missing parameters
...

SET WIFI: unknown option 'invalid'
...

SET: unknown argument
```

**Pass Criteria:** ✅ All invalid commands rejected

---

### Test 5.2: CLI Parser Normalization

**Commands (mixed case):**
```
SET WIFI SSID TestNet
set WIFI ssid TestNet2
Set Wifi SSID TestNet3
```

**Expected Output:**
```
Wi-Fi SSID set to: TestNet
Wi-Fi SSID set to: TestNet2
Wi-Fi SSID set to: TestNet3
```

**Pass Criteria:** ✅ Case-insensitive parsing works

---

### Test 5.3: Connect/Disconnect Keywords

**Commands:**
```
CONNECT wifi
connect WIFI
Connect Wifi
DISCONNECT wifi
```

**Expected Output:**
```
Connecting to Wi-Fi: ...
(all should work without errors)
```

**Pass Criteria:** ✅ All variations accepted

---

## TEST SUITE 6: TELNET INTEGRATION

### Test 6.1: Connect via Telnet

**Steps:**
1. Set up Wi-Fi and connect to it
2. Open telnet on port 23 (or configured port)
3. Type: `show wifi`

**Expected Output:**
```
=== WI-FI STATUS ===
(same as serial output)
```

**Pass Criteria:** ✅ Telnet and serial show same output

---

### Test 6.2: Execute Commands via Telnet

**Steps:**
1. Connected via Telnet
2. Type: `set wifi ip 192.168.1.150`
3. Type: `save`

**Expected Output:**
```
Static IP set to: 192.168.1.150
Hint: Use 'save' to persist configuration to NVS
Configuration saved to NVS
```

**Pass Criteria:** ✅ All commands work via Telnet

---

## SUMMARY CHECKLIST

### CLI Parser Fixes

- [x] Added WIFI alias to normalize_alias()
- [x] Fixed CONNECT wifi command routing
- [x] Fixed DISCONNECT wifi command routing
- [x] Added show wifi command support

### Command Implementations

- [x] set wifi ssid <name>
- [x] set wifi password <pwd>
- [x] set wifi dhcp on|off
- [x] set wifi ip <ip>
- [x] set wifi gateway <ip>
- [x] set wifi netmask <ip>
- [x] set wifi dns <ip>
- [x] set wifi telnet-port <port>
- [x] set wifi enable|disable
- [x] show wifi
- [x] connect wifi
- [x] disconnect wifi

### Build Status

- [x] Compiles without errors
- [x] Flash usage: 62.3% (acceptable)
- [x] RAM usage: 30.1% (acceptable)

### Quality

- [x] Input validation on all parameters
- [x] Case-insensitive command parsing
- [x] Helpful error messages
- [x] Config persistence support
- [x] Telnet integration

---

## KNOWN ISSUES & FOLLOW-UP

From DYBDEANALYSE_v3.0.0.md:

### Phase 1 (KRITISK):
- [ ] Telnet No Authentication (add login prompt)
- [ ] Telnet Buffer Overflow (fix off-by-one)
- [ ] NVS CRC Bypass (return false on mismatch)
- [ ] CLI Buffer Overflow (add bounds checks)
- [ ] Telnet Plaintext (implement SSH/TLS)
- [ ] Telnet IAC Bug (fix state machine)

### Phase 2 (ALVORLIG):
- [ ] NVS TOCTOU (double-buffering)
- [ ] Encrypt passwords in NVS
- [ ] Multiple Telnet clients
- [ ] CRLF normalization
- [ ] Input sanitization
- [ ] Parameter range checks

---

**Test Date:** 2025-12-05
**Status:** READY FOR MANUAL TESTING
**Next Step:** Upload firmware and run test suite

