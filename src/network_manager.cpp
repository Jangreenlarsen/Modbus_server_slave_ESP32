/**
 * @file network_manager.cpp
 * @brief Network subsystem orchestration implementation
 *
 * Coordinates Wi-Fi driver, TCP server, and Telnet protocol.
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <Arduino.h>
#include <lwip/inet.h>
#include <esp_log.h>

#include "network_manager.h"
#include "wifi_driver.h"
#include "tcp_server.h"
#include "telnet_server.h"
#include "network_config.h"
#include "constants.h"

static const char *TAG = "NET_MGR";

/* ============================================================================
 * INTERNAL STATE
 * ============================================================================ */

static struct {
  uint8_t initialized;
  TelnetServer *telnet_server;
  NetworkState state;
  NetworkConfig current_config;
} network_mgr = {
  .initialized = 0,
  .telnet_server = NULL,
  .state = {0},
  .current_config = {0}
};

/* ============================================================================
 * INITIALIZATION & CONTROL
 * ============================================================================ */

int network_manager_init(void)
{
  if (network_mgr.initialized) {
    ESP_LOGI(TAG, "Network manager already initialized");
    return 0;
  }

  memset(&network_mgr.state, 0, sizeof(NetworkState));
  network_mgr.state.telnet_socket = -1;

  // Initialize Wi-Fi driver
  if (wifi_driver_init() != 0) {
    ESP_LOGE(TAG, "Failed to initialize Wi-Fi driver");
    return -1;
  }

  // Create Telnet server (but don't start yet)
  network_mgr.telnet_server = telnet_server_create(TELNET_PORT);
  if (!network_mgr.telnet_server) {
    ESP_LOGE(TAG, "Failed to create Telnet server");
    return -1;
  }

  network_mgr.initialized = 1;
  ESP_LOGI(TAG, "Network manager initialized");

  return 0;
}

int network_manager_connect(const NetworkConfig *config)
{
  if (!config || !network_mgr.initialized) {
    ESP_LOGE(TAG, "Invalid config or not initialized");
    return -1;
  }

  // Validate config
  if (!network_config_validate(config)) {
    ESP_LOGE(TAG, "Invalid network config");
    return -1;
  }

  // Save config
  memcpy(&network_mgr.current_config, config, sizeof(NetworkConfig));

  // Configure DHCP vs static IP
  if (config->dhcp_enabled) {
    wifi_driver_enable_dhcp();
    ESP_LOGI(TAG, "DHCP enabled");
  } else {
    wifi_driver_set_static_ip(config->static_ip, config->static_gateway,
                             config->static_netmask, config->static_dns);
    ESP_LOGI(TAG, "Static IP configured");
  }

  // Start Telnet server
  if (config->telnet_enabled) {
    if (telnet_server_start(network_mgr.telnet_server) != 0) {
      ESP_LOGE(TAG, "Failed to start Telnet server");
      return -1;
    }
    ESP_LOGI(TAG, "Telnet server started on port %d", TELNET_PORT);
  }

  // Connect to Wi-Fi
  if (wifi_driver_connect(config->ssid, config->password) != 0) {
    ESP_LOGE(TAG, "Failed to start Wi-Fi connection");
    return -1;
  }

  ESP_LOGI(TAG, "Connecting to Wi-Fi network: %s", config->ssid);

  return 0;
}

int network_manager_stop(void)
{
  if (!network_mgr.initialized) {
    return -1;
  }

  // Stop Telnet server
  if (network_mgr.telnet_server) {
    telnet_server_stop(network_mgr.telnet_server);
  }

  // Disconnect Wi-Fi
  wifi_driver_disconnect();

  ESP_LOGI(TAG, "Network manager stopped");

  return 0;
}

/* ============================================================================
 * STATUS & INFORMATION
 * ============================================================================ */

uint8_t network_manager_is_wifi_connected(void)
{
  return wifi_driver_is_connected();
}

uint8_t network_manager_is_telnet_connected(void)
{
  if (!network_mgr.telnet_server) {
    return 0;
  }

  return telnet_server_client_connected(network_mgr.telnet_server);
}

uint32_t network_manager_get_local_ip(void)
{
  return wifi_driver_get_local_ip();
}

const NetworkState* network_manager_get_state(void)
{
  return &network_mgr.state;
}

/* ============================================================================
 * TELNET I/O
 * ============================================================================ */

int network_manager_telnet_readline(char *buf, size_t max_len)
{
  if (!network_mgr.telnet_server || !buf || !max_len) {
    return -1;
  }

  return telnet_server_readline(network_mgr.telnet_server, buf, max_len);
}

int network_manager_telnet_writeline(const char *line)
{
  if (!network_mgr.telnet_server || !line) {
    return -1;
  }

  return telnet_server_writeline(network_mgr.telnet_server, line);
}

int network_manager_telnet_writelinef(const char *format, ...)
{
  if (!network_mgr.telnet_server || !format) {
    return -1;
  }

  char buf[256];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buf, sizeof(buf) - 1, format, args);
  va_end(args);

  if (len < 0 || len >= (int)sizeof(buf)) {
    return -1;
  }

  return telnet_server_writeline(network_mgr.telnet_server, buf);
}

int network_manager_telnet_write(const char *text)
{
  if (!network_mgr.telnet_server || !text) {
    return -1;
  }

  return telnet_server_write(network_mgr.telnet_server, text);
}

uint8_t network_manager_telnet_has_input(void)
{
  if (!network_mgr.telnet_server) {
    return 0;
  }

  return telnet_server_has_input(network_mgr.telnet_server);
}

/* ============================================================================
 * BACKGROUND TASKS
 * ============================================================================ */

int network_manager_loop(void)
{
  int events = 0;

  if (!network_mgr.initialized) {
    return 0;
  }

  // Process Wi-Fi events (auto-reconnect, etc.)
  events += wifi_driver_loop();

  // Update runtime state from Wi-Fi driver
  if (wifi_driver_is_connected()) {
    network_mgr.state.wifi_connected = 1;
    network_mgr.state.local_ip = wifi_driver_get_local_ip();
    network_mgr.state.gateway = wifi_driver_get_gateway();
    network_mgr.state.netmask = wifi_driver_get_netmask();
    network_mgr.state.dns = wifi_driver_get_dns();
  } else {
    network_mgr.state.wifi_connected = 0;
    network_mgr.state.local_ip = 0;
  }

  // Process Telnet server events (only if Wi-Fi connected)
  if (network_mgr.telnet_server && wifi_driver_is_connected()) {
    events += telnet_server_loop(network_mgr.telnet_server);

    // Update Telnet client state
    if (telnet_server_client_connected(network_mgr.telnet_server)) {
      network_mgr.state.telnet_client_connected = 1;
    } else {
      network_mgr.state.telnet_client_connected = 0;
    }
  }

  return events;
}

/* ============================================================================
 * DEBUGGING & STATUS
 * ============================================================================ */

void network_manager_print_status(void)
{
  printf("\n╔════════════════════════════════════════╗\n");
  printf("║     NETWORK MANAGER STATUS            ║\n");
  printf("╚════════════════════════════════════════╝\n\n");

  printf("Wi-Fi Status: %s\n", network_manager_get_wifi_state_string());

  if (network_mgr.state.wifi_connected) {
    char ip_str[16];
    struct in_addr addr;
    addr.s_addr = network_mgr.state.local_ip;
    printf("Local IP:     %s\n", inet_ntoa(addr));

    addr.s_addr = network_mgr.state.gateway;
    printf("Gateway:      %s\n", inet_ntoa(addr));

    int8_t rssi = wifi_driver_get_rssi();
    printf("Signal:       %d dBm\n", rssi);
  }

  printf("Telnet:       %s\n", network_mgr.state.telnet_client_connected ? "Connected" : "Waiting");

  if (network_mgr.telnet_server) {
    printf("Telnet Port:  %d\n", TELNET_PORT);
  }

  printf("\n");
}

const char* network_manager_get_wifi_state_string(void)
{
  return wifi_driver_get_state_string();
}
