/**
 * @file version.h
 * @brief Version information and changelog
 */

#ifndef version_H
#define version_H

#define PROJECT_VERSION "1.0.0"
#define PROJECT_BUILD_DATE __DATE__
#define PROJECT_NAME "Modbus RTU Server (ESP32)"

void version_print_changelog(void);

#endif // version_H
