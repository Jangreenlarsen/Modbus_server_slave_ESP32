# ESP32 Modbus RTU Server - Setup Guide

## Prerequisites

- **ESP32-DOIT-DEVKIT-V1** med USB kabel
- **PlatformIO** installeret (CLI eller VS Code extension)
- **Windows/Linux/macOS**

---

## Step 1: Clone Repository

```bash
git clone https://github.com/Jangreenlarsen/Modbus_server_slave_ESP32.git
cd Modbus_server_slave_ESP32
```

---

## Step 2: Check PlatformIO Version

```bash
pio --version
```

Expected: `PlatformIO Core, version 6.0+`

If not installed:
```bash
python -m pip install platformio --upgrade
```

---

## Step 3: Configure Serial Port

Edit `platformio.ini`:

```ini
monitor_port = COM10      # Change to your ESP32 COM port
upload_port = COM10       # Same as monitor_port
```

**Find your COM port:**
- Windows: Open "Device Manager" → "Ports (COM & LPT)" → Look for "USB" or "CH340" (UART chip)
- Linux: `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`
- macOS: `ls /dev/cu.usb*`

---

## Step 4: Build Project

```bash
pio run
```

Expected output:
```
RAM:   [=         ]   6.5% (used 21400 bytes from 327680 bytes)
Flash: [==        ]  20.0% (used 262273 bytes from 1310720 bytes)
========================= [SUCCESS] Took 10.14 seconds =========================
```

If build fails, check:
- [ ] PlatformIO installed: `pio --version`
- [ ] Correct COM port in `platformio.ini`
- [ ] Internet connection (first build downloads toolchains)

---

## Step 5: Upload to ESP32 Hardware

**Connections:**
1. Connect ESP32 to computer via USB
2. Wait 2 seconds for USB driver to initialize
3. Check Device Manager to confirm COM port

**Upload firmware:**

```bash
pio run -t upload
```

Expected output:
```
Uploading .pio/build/esp32/firmware.bin
...
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

**Troubleshooting:**
- If upload fails with "timeout", try:
  1. Hold BOOT button on ESP32 during upload
  2. Check USB cable is connected properly
  3. Try different USB port on computer

---

## Step 6: Monitor Serial Output

Open serial monitor:

```bash
pio device monitor
```

Expected output:
```
=== Modbus RTU Server (ESP32) ===
Version: 1.0.0
Setup complete.
```

Exit monitor: Press `Ctrl+]` (PlatformIO) or close terminal

---

## Useful Commands

```bash
# Full rebuild (clean + build)
pio run --target clean
pio run

# Upload without building
pio run -t upload

# Monitor with filtering (show only DEBUG lines)
pio device monitor --filter=log

# Show memory usage details
pio run --verbose

# Open IDE
pio home
```

---

## Project Structure

```
C:\Projekter\Modbus_server_slave_ESP32\
├── src/                    # Source files (35 .cpp files)
├── include/                # Header files (37 .h files)
├── docs/                   # Documentation
├── .pio/                   # Build artifacts (auto-generated)
├── platformio.ini          # Build configuration
├── CLAUDE.md               # Project architecture (DANISH)
└── .gitignore
```

---

## Next Steps

Once build & upload works:

1. **Implement Layer 0 (Hardware Drivers)**
   - Start with: `src/uart_driver.cpp`
   - Test serial communication

2. **Implement Layer 1 (Protocol Core)**
   - Port from Mega2560: `modbus_frame.cpp`, `modbus_parser.cpp`

3. **Test on Hardware**
   - Verify Modbus RTU communication over RS-485

---

## Common Issues

### Issue: "Unknown configuration option `include_dir`"

**Fix:** These are warnings only. Ignore them. Build will succeed.

### Issue: "Failed to open serial device"

**Solution:**
```bash
# List available ports
pio device list

# Use correct port in platformio.ini
monitor_port = COM4  # Update to your port
```

### Issue: "No module named 'platformio'"

**Solution:**
```bash
python -m pip install platformio --upgrade
pio --version
```

### Issue: Build takes forever (first time)

**Expected:** First build downloads ~500MB of toolchains and compilers. This can take 5-15 minutes.

Subsequent builds will be fast (~10 seconds).

---

## Hardware: ESP32-DOIT-DEVKIT-V1 Specs

- **Microcontroller:** ESP32 (Dual-core, 240 MHz)
- **RAM:** 320 KB (327 KB free)
- **Flash:** 4 MB (1.3 MB usable, 1.04 MB free)
- **USB:** CH340 UART chip
- **Voltage:** 3.3V logic (5V tolerant on inputs via protection)
- **GPIO:** 34 usable pins

---

## References

- [PlatformIO Docs](https://docs.platformio.org/)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [ESP32-DOIT-DEVKIT-V1 Pinout](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32_devkitc.html)

---

## Support

- Check CLAUDE.md for architecture documentation
- See docs/ESP32_Module_Architecture.md for module structure
- GitHub Issues: [Link to repo issues]

God dag! Happy coding! 🚀
