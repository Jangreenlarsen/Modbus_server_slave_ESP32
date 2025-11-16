/**
 * @file version.cpp
 * @brief Version information and changelog implementation
 */

#include "version.h"
#include "debug.h"

void version_print_changelog(void) {
  debug_println("\n=== Changelog ===\n");
  debug_println("v1.0.0 - Initial release");
  debug_println("  - Counter Engine (SW, SW-ISR, HW-PCNT modes)");
  debug_println("  - Timer Engine (One-shot, Monostable, Astable, Input-triggered)");
  debug_println("  - CLI interface (show/set commands)");
  debug_println("  - GPIO driver abstraction");
  debug_println("  - PCNT hardware counter support");
  debug_println("  - Configuration persistence (NVS)");
  debug_println("\nv0.1.0 - Mega2560 port from v3.6.5");
  debug_println("  - Baseline architecture porting complete\n");
}
