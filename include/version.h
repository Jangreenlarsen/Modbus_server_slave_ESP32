/**
 * @file version.h
 * @brief Version information and changelog
 */

#ifndef version_H
#define version_H

// Auto-generated build information (updated by generate_build_info.py)
#include "build_version.h"

#define PROJECT_VERSION "2.2.0"
#define PROJECT_BUILD_DATE __DATE__
#define PROJECT_NAME "Modbus RTU Server (ESP32)"

void version_print_changelog(void);

#endif // version_H
