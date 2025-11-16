/**
 * @file modbus_server.cpp
 * @brief Modbus server main state machine implementation (LAYER 3)
 *
 * Main orchestration: Idle → RX → Process → TX → Idle
 */

#include "modbus_server.h"
#include "modbus_rx.h"
#include "modbus_tx.h"
#include "modbus_fc_dispatch.h"
#include "modbus_frame.h"
#include "constants.h"
#include "debug.h"
#include <Arduino.h>

/* ============================================================================
 * STATIC STATE
 * ============================================================================ */

static modbus_server_state_t server_state = MODBUS_STATE_IDLE;
static uint8_t slave_id = SLAVE_ID;
static ModbusFrame request_frame;
static ModbusFrame response_frame;

/* ============================================================================
 * MODBUS SERVER FUNCTIONS
 * ============================================================================ */

void modbus_server_init(uint8_t sid) {
  slave_id = sid;
  server_state = MODBUS_STATE_IDLE;

  // Initialize subsystems
  modbus_rx_init();
  modbus_tx_init();

  debug_print("Modbus server initialized (Slave ID: ");
  debug_print_uint(slave_id);
  debug_println(")");
}

void modbus_server_loop(void) {
  switch (server_state) {
    case MODBUS_STATE_IDLE:
      // Reset RX state and wait for request
      modbus_rx_reset();
      server_state = MODBUS_STATE_RX;
      break;

    case MODBUS_STATE_RX:
      // Process RX
      {
        modbus_rx_state_t rx_state = modbus_rx_process(&request_frame);

        if (rx_state == MODBUS_RX_COMPLETE) {
          // Frame received successfully
          // Check if frame is for this slave (or broadcast 0)
          if (request_frame.slave_id == slave_id || request_frame.slave_id == 0) {
            debug_print("Modbus request received: FC=0x");
            debug_print_uint(request_frame.function_code);
            debug_newline();
            server_state = MODBUS_STATE_PROCESS;
          } else {
            // Not for this slave - ignore and return to idle
            debug_print("Modbus request for different slave (ID: ");
            debug_print_uint(request_frame.slave_id);
            debug_println("), ignoring");
            server_state = MODBUS_STATE_IDLE;
          }
        } else if (rx_state == MODBUS_RX_ERROR) {
          // RX error - return to idle
          debug_println("Modbus RX error, returning to idle");
          server_state = MODBUS_STATE_IDLE;
        }
        // Otherwise stay in RX state
      }
      break;

    case MODBUS_STATE_PROCESS:
      // Process request and generate response
      {
        bool success = modbus_dispatch_function_code(&request_frame, &response_frame);

        if (success) {
          // Broadcast requests (slave_id == 0) should NOT generate responses
          if (request_frame.slave_id == 0) {
            debug_println("Broadcast request - no response sent");
            server_state = MODBUS_STATE_IDLE;
          } else {
            debug_println("Processing complete, sending response");
            server_state = MODBUS_STATE_TX;
          }
        } else {
          // Error occurred - response_frame contains error response
          // Broadcast requests should NOT send error responses
          if (request_frame.slave_id == 0) {
            debug_println("Broadcast request error - no response sent");
            server_state = MODBUS_STATE_IDLE;
          } else {
            debug_println("Processing error, sending error response");
            server_state = MODBUS_STATE_TX;
          }
        }
      }
      break;

    case MODBUS_STATE_TX:
      // Transmit response
      {
        bool success = modbus_tx_send_frame(&response_frame);

        if (success) {
          debug_println("Response transmitted");
        } else {
          debug_println("TX error");
        }

        server_state = MODBUS_STATE_IDLE;
      }
      break;

    case MODBUS_STATE_ERROR:
      // Error state - reset to idle
      debug_println("Modbus server error, resetting to idle");
      server_state = MODBUS_STATE_IDLE;
      break;
  }
}

modbus_server_state_t modbus_server_get_state(void) {
  return server_state;
}

void modbus_server_set_slave_id(uint8_t sid) {
  if (sid >= 1 && sid <= 247) {
    slave_id = sid;
    debug_print("Modbus slave ID changed to: ");
    debug_print_uint(slave_id);
    debug_newline();
  } else {
    debug_println("ERROR: Invalid slave ID (must be 1-247)");
  }
}

uint8_t modbus_server_get_slave_id(void) {
  return slave_id;
}

