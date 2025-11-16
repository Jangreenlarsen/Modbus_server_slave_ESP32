/**
 * @file heartbeat.cpp
 * @brief Heartbeat/watchdog implementation - blink LED and monitor system
 */

#include "heartbeat.h"
#include "debug.h"
#include <Arduino.h>

#define LED_PIN 2  // ESP32 onboard LED on GPIO2
#define BLINK_INTERVAL 1000  // 1 second

static uint32_t last_blink = 0;
static uint8_t led_state = 0;

void heartbeat_init(void) {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  debug_println("Heartbeat initialized (LED on GPIO2)");
}

void heartbeat_loop(void) {
  uint32_t now = millis();
  
  if (now - last_blink >= BLINK_INTERVAL) {
    led_state = !led_state;
    digitalWrite(LED_PIN, led_state ? HIGH : LOW);
    last_blink = now;
  }
  
  // TODO: Add watchdog timer reset
  // TODO: Add system monitoring (RAM, CPU, etc.)
}
