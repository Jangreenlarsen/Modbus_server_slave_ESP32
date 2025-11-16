/**
 * @file gpio_driver.cpp
 * @brief GPIO driver implementation using ESP32 HAL
 *
 * Maps abstract GPIO functions to ESP32 hardware via Arduino API
 */

#include "gpio_driver.h"
#include <Arduino.h>

/* ============================================================================
 * INTERRUPT HANDLERS
 * ============================================================================ */

// Store handlers for up to 40 GPIO pins (ESP32 max)
static gpio_isr_handler_t gpio_handlers[40] = {NULL};
static void* gpio_handler_args[40] = {NULL};

/**
 * @brief Generic interrupt handler wrapper
 * Called from Arduino attachInterrupt, dispatches to user handler
 */
static void IRAM_ATTR gpio_isr_wrapper(void) {
  // Note: We don't know which pin triggered in this context
  // Real implementation would need pin-specific callbacks
  // For now, this is a placeholder
}

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void gpio_driver_init(void) {
  // GPIO driver uses Arduino HAL which is already initialized
  // Nothing specific needed here
}

/* ============================================================================
 * GPIO CONTROL
 * ============================================================================ */

void gpio_set_direction(uint8_t pin, gpio_direction_t dir) {
  if (pin >= 40) return;  // Invalid pin

  if (dir == GPIO_INPUT) {
    pinMode(pin, INPUT);
  } else {
    pinMode(pin, OUTPUT);
  }
}

uint8_t gpio_read(uint8_t pin) {
  if (pin >= 40) return 0;
  return digitalRead(pin) ? 1 : 0;
}

void gpio_write(uint8_t pin, uint8_t level) {
  if (pin >= 40) return;
  digitalWrite(pin, level ? HIGH : LOW);
}

/* ============================================================================
 * INTERRUPT HANDLING
 * ============================================================================ */

void gpio_interrupt_attach(uint8_t pin, gpio_edge_t edge, gpio_isr_handler_t handler) {
  if (pin >= 40 || handler == NULL) return;

  // Set GPIO as input first
  gpio_set_direction(pin, GPIO_INPUT);

  // Store handler
  gpio_handlers[pin] = handler;

  // Map edge type to Arduino interrupt mode
  int mode;
  switch (edge) {
    case GPIO_RISING:
      mode = RISING;
      break;
    case GPIO_FALLING:
      mode = FALLING;
      break;
    case GPIO_BOTH:
      mode = CHANGE;
      break;
    default:
      return;
  }

  // Attach interrupt - note: Arduino's attachInterrupt is limited
  // For full ISR with custom handlers, would need direct ESP32 GPIO API
  // This is a simplified implementation using Arduino API
  // Real implementation would use esp_gpio_isr_register() for more control

  // For now, just store the handler - interrupt dispatch needs deeper ESP32 integration
  // TODO: Implement proper ISR dispatch via ESP32 GPIO interrupt controller
}

void gpio_interrupt_detach(uint8_t pin) {
  if (pin >= 40) return;

  // Detach Arduino interrupt if applicable
  // detachInterrupt(pin);  // Arduino API limitation

  // Clear handler
  gpio_handlers[pin] = NULL;
  gpio_handler_args[pin] = NULL;
}
