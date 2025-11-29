/**
 * @file config_apply.cpp
 * @brief Configuration apply - activate config in system
 *
 * LAYER 6: Persistence
 * Responsibility: Apply loaded configuration to running system
 */

#include "config_apply.h"
#include "counter_engine.h"
#include "timer_engine.h"
#include "gpio_driver.h"
#include "config_struct.h"
#include "registers.h"
#include "heartbeat.h"
#include "debug.h"
#include <cstddef>

bool config_apply(const PersistConfig* cfg) {
  if (cfg == NULL) return false;

  debug_println("CONFIG APPLY: Applying configuration to system");

  // Display Modbus slave ID (NOTE: already set in main.cpp via modbus_server_init())
  debug_print("  Slave ID: ");
  debug_print_uint(cfg->slave_id);
  debug_println("");

  // Apply baudrate (NOTE: requires reboot for UART reinit)
  debug_print("  Baudrate: ");
  debug_print_uint(cfg->baudrate);
  debug_println(" (requires reboot)");

  // Apply GPIO2 heartbeat configuration
  debug_print("  GPIO2 user mode: ");
  debug_print_uint(cfg->gpio2_user_mode);
  debug_println("");
  if (cfg->gpio2_user_mode) {
    heartbeat_disable();
    debug_println("    Heartbeat disabled (GPIO2 available for user)");
  } else {
    heartbeat_enable();
    debug_println("    Heartbeat enabled (GPIO2 LED blink)");
  }

  // Apply GPIO mappings (initialize GPIO pins)
  debug_print("  GPIO mappings: ");
  debug_print_uint(cfg->gpio_map_count);
  debug_println("");

  for (uint8_t i = 0; i < cfg->gpio_map_count; i++) {
    const GPIOMapping* map = &cfg->gpio_maps[i];

    // Skip mappings associated with counters/timers (they will be initialized by their engines)
    if (map->associated_counter != 0xff || map->associated_timer != 0xff) {
      continue;
    }

    // Initialize GPIO pin direction for STATIC mappings
    if (map->is_input) {
      gpio_set_direction(map->gpio_pin, GPIO_INPUT);
      debug_print("    GPIO");
      debug_print_uint(map->gpio_pin);
      debug_print(" - INPUT:");
      debug_print_uint(map->input_reg);
      debug_println("");
    } else {
      gpio_set_direction(map->gpio_pin, GPIO_OUTPUT);
      debug_print("    GPIO");
      debug_print_uint(map->gpio_pin);
      debug_print(" - COIL:");
      debug_print_uint(map->coil_reg);
      debug_println("");
    }
  }

  // Apply STATIC register mappings (initialize holding register values)
  debug_print("  STATIC registers: ");
  debug_print_uint(cfg->static_reg_count);
  debug_println("");
  for (uint8_t i = 0; i < cfg->static_reg_count; i++) {
    registers_set_holding_register(
      cfg->static_regs[i].register_address,
      cfg->static_regs[i].static_value
    );
    debug_print("    Reg[");
    debug_print_uint(cfg->static_regs[i].register_address);
    debug_print("] = ");
    debug_print_uint(cfg->static_regs[i].static_value);
    debug_println("");
  }

  // Apply STATIC coil mappings (initialize coil values)
  debug_print("  STATIC coils: ");
  debug_print_uint(cfg->static_coil_count);
  debug_println("");
  for (uint8_t i = 0; i < cfg->static_coil_count; i++) {
    registers_set_coil(
      cfg->static_coils[i].coil_address,
      cfg->static_coils[i].static_value
    );
    debug_print("    Coil[");
    debug_print_uint(cfg->static_coils[i].coil_address);
    debug_print("] = ");
    debug_print_uint(cfg->static_coils[i].static_value);
    debug_println("");
  }

  // Apply DYNAMIC register mappings (info only, actual updates happen in registers.cpp loop)
  debug_print("  DYNAMIC registers: ");
  debug_print_uint(cfg->dynamic_reg_count);
  debug_println("");

  // Apply DYNAMIC coil mappings (info only, actual updates happen in registers.cpp loop)
  debug_print("  DYNAMIC coils: ");
  debug_print_uint(cfg->dynamic_coil_count);
  debug_println("");

  // Apply counter configs
  debug_println("  Counters:");
  for (uint8_t i = 0; i < COUNTER_COUNT; i++) {
    if (cfg->counters[i].enabled) {
      debug_print("    Counter ");
      debug_print_uint(i + 1);
      debug_println(" enabled - configuring...");
      counter_engine_configure(i + 1, &cfg->counters[i]);
    }
  }

  // Apply timer configs
  debug_println("  Timers:");
  for (uint8_t i = 0; i < TIMER_COUNT; i++) {
    if (cfg->timers[i].enabled) {
      debug_print("    Timer ");
      debug_print_uint(i + 1);
      debug_println(" enabled - configured");
      timer_engine_configure(i + 1, &cfg->timers[i]);
    }
  }

  debug_println("CONFIG APPLY: Done");
  return true;
}
