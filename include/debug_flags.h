/**
 * @file debug_flags.h
 * @brief Runtime debug flag management
 */

#ifndef DEBUG_FLAGS_H
#define DEBUG_FLAGS_H

#include "types.h"

/**
 * @brief Get debug flags structure
 */
DebugFlags* debug_flags_get(void);

/**
 * @brief Enable/disable config save debug
 */
void debug_flags_set_config_save(uint8_t enabled);

/**
 * @brief Enable/disable config load debug
 */
void debug_flags_set_config_load(uint8_t enabled);

/**
 * @brief Enable/disable all debug flags
 */
void debug_flags_set_all(uint8_t enabled);

#endif // DEBUG_FLAGS_H
