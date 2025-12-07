/**
 * @file console_telnet.h
 * @brief Telnet console implementation
 *
 * Implements Console interface for Telnet server
 */

#ifndef CONSOLE_TELNET_H
#define CONSOLE_TELNET_H

#include "console.h"
#include "telnet_server.h"

/* ============================================================================
 * TELNET CONSOLE INITIALIZATION
 * ============================================================================ */

/**
 * Create Telnet console instance
 * @param telnet_server TelnetServer instance to wrap
 * @return Console instance, NULL on error
 *
 * NOTE: The TelnetServer must already be created and started
 */
Console* console_telnet_create(TelnetServer *telnet_server);

/**
 * Destroy Telnet console instance
 * @param console Console instance to destroy
 *
 * NOTE: This does NOT destroy the underlying TelnetServer
 */
void console_telnet_destroy(Console *console);

#endif // CONSOLE_TELNET_H
