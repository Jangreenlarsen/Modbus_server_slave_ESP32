/**
 * @file http_server.cpp
 * @brief HTTP REST API server implementation (v6.0.0+)
 *
 * LAYER 1.5: Protocol (same level as Telnet server)
 * Responsibility: HTTP server lifecycle and request routing
 *
 * Uses ESP-IDF esp_http_server wrapped for simplicity.
 */

#include <string.h>
#include <stdio.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <Arduino.h>
#include <mbedtls/base64.h>

#include "http_server.h"
#include "api_handlers.h"
#include "constants.h"
#include "debug.h"

static const char *TAG = "HTTP_SRV";

/* ============================================================================
 * INTERNAL STATE
 * ============================================================================ */

static struct {
  httpd_handle_t server;
  HttpConfig config;
  HttpServerStats stats;
  uint8_t initialized;
  uint8_t running;
  uint8_t tls_active;
} http_state = {
  .server = NULL,
  .config = {0},
  .stats = {0},
  .initialized = 0,
  .running = 0,
  .tls_active = 0
};

/* ============================================================================
 * FORWARD DECLARATIONS FOR URI HANDLERS
 * ============================================================================ */

// These are implemented in api_handlers.cpp
extern esp_err_t api_handler_status(httpd_req_t *req);
extern esp_err_t api_handler_counters(httpd_req_t *req);
extern esp_err_t api_handler_counter_single(httpd_req_t *req);
extern esp_err_t api_handler_timers(httpd_req_t *req);
extern esp_err_t api_handler_timer_single(httpd_req_t *req);
extern esp_err_t api_handler_hr_read(httpd_req_t *req);
extern esp_err_t api_handler_hr_write(httpd_req_t *req);
extern esp_err_t api_handler_ir_read(httpd_req_t *req);
extern esp_err_t api_handler_coil_read(httpd_req_t *req);
extern esp_err_t api_handler_coil_write(httpd_req_t *req);
extern esp_err_t api_handler_di_read(httpd_req_t *req);
extern esp_err_t api_handler_logic(httpd_req_t *req);
extern esp_err_t api_handler_logic_single(httpd_req_t *req);

/* ============================================================================
 * URI DEFINITIONS
 * ============================================================================ */

static const httpd_uri_t uri_status = {
  .uri      = "/api/status",
  .method   = HTTP_GET,
  .handler  = api_handler_status,
  .user_ctx = NULL
};

static const httpd_uri_t uri_counters = {
  .uri      = "/api/counters",
  .method   = HTTP_GET,
  .handler  = api_handler_counters,
  .user_ctx = NULL
};

static const httpd_uri_t uri_counter_single = {
  .uri      = "/api/counters/*",
  .method   = HTTP_GET,
  .handler  = api_handler_counter_single,
  .user_ctx = NULL
};

static const httpd_uri_t uri_timers = {
  .uri      = "/api/timers",
  .method   = HTTP_GET,
  .handler  = api_handler_timers,
  .user_ctx = NULL
};

static const httpd_uri_t uri_timer_single = {
  .uri      = "/api/timers/*",
  .method   = HTTP_GET,
  .handler  = api_handler_timer_single,
  .user_ctx = NULL
};

static const httpd_uri_t uri_hr_read = {
  .uri      = "/api/registers/hr/*",
  .method   = HTTP_GET,
  .handler  = api_handler_hr_read,
  .user_ctx = NULL
};

static const httpd_uri_t uri_hr_write = {
  .uri      = "/api/registers/hr/*",
  .method   = HTTP_POST,
  .handler  = api_handler_hr_write,
  .user_ctx = NULL
};

static const httpd_uri_t uri_ir_read = {
  .uri      = "/api/registers/ir/*",
  .method   = HTTP_GET,
  .handler  = api_handler_ir_read,
  .user_ctx = NULL
};

static const httpd_uri_t uri_coil_read = {
  .uri      = "/api/registers/coils/*",
  .method   = HTTP_GET,
  .handler  = api_handler_coil_read,
  .user_ctx = NULL
};

static const httpd_uri_t uri_coil_write = {
  .uri      = "/api/registers/coils/*",
  .method   = HTTP_POST,
  .handler  = api_handler_coil_write,
  .user_ctx = NULL
};

static const httpd_uri_t uri_di_read = {
  .uri      = "/api/registers/di/*",
  .method   = HTTP_GET,
  .handler  = api_handler_di_read,
  .user_ctx = NULL
};

static const httpd_uri_t uri_logic = {
  .uri      = "/api/logic",
  .method   = HTTP_GET,
  .handler  = api_handler_logic,
  .user_ctx = NULL
};

static const httpd_uri_t uri_logic_single = {
  .uri      = "/api/logic/*",
  .method   = HTTP_GET,
  .handler  = api_handler_logic_single,
  .user_ctx = NULL
};

/* ============================================================================
 * INITIALIZATION & CONTROL
 * ============================================================================ */

int http_server_init(void)
{
  if (http_state.initialized) {
    ESP_LOGI(TAG, "HTTP server already initialized");
    return 0;
  }

  memset(&http_state.stats, 0, sizeof(HttpServerStats));
  memset(&http_state.config, 0, sizeof(HttpConfig));
  http_state.server = NULL;
  http_state.running = 0;
  http_state.initialized = 1;

  ESP_LOGI(TAG, "HTTP server initialized");
  return 0;
}

int http_server_start(const HttpConfig *config)
{
  if (!http_state.initialized) {
    ESP_LOGE(TAG, "HTTP server not initialized");
    return -1;
  }

  if (http_state.running) {
    ESP_LOGI(TAG, "HTTP server already running");
    return 0;
  }

  if (!config) {
    ESP_LOGE(TAG, "HTTP config is NULL");
    return -1;
  }

  // Store config
  memcpy(&http_state.config, config, sizeof(HttpConfig));

  // Configure HTTP server
  httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
  httpd_config.server_port = config->port;
  httpd_config.max_uri_handlers = 16;
  httpd_config.stack_size = 4096;
  httpd_config.uri_match_fn = httpd_uri_match_wildcard;

  // Start server
  esp_err_t err = httpd_start(&http_state.server, &httpd_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server: %d", err);
    return -1;
  }

  // Register URI handlers
  httpd_register_uri_handler(http_state.server, &uri_status);
  httpd_register_uri_handler(http_state.server, &uri_counters);
  httpd_register_uri_handler(http_state.server, &uri_counter_single);
  httpd_register_uri_handler(http_state.server, &uri_timers);
  httpd_register_uri_handler(http_state.server, &uri_timer_single);
  httpd_register_uri_handler(http_state.server, &uri_hr_read);
  httpd_register_uri_handler(http_state.server, &uri_hr_write);
  httpd_register_uri_handler(http_state.server, &uri_ir_read);
  httpd_register_uri_handler(http_state.server, &uri_coil_read);
  httpd_register_uri_handler(http_state.server, &uri_coil_write);
  httpd_register_uri_handler(http_state.server, &uri_di_read);
  httpd_register_uri_handler(http_state.server, &uri_logic);
  httpd_register_uri_handler(http_state.server, &uri_logic_single);

  http_state.running = 1;
  ESP_LOGI(TAG, "HTTP server started on port %d", config->port);

  return 0;
}

int http_server_stop(void)
{
  if (!http_state.running || !http_state.server) {
    ESP_LOGI(TAG, "HTTP server not running");
    return 0;
  }

  esp_err_t err = httpd_stop(http_state.server);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to stop HTTP server: %d", err);
    return -1;
  }

  http_state.server = NULL;
  http_state.running = 0;
  ESP_LOGI(TAG, "HTTP server stopped");

  return 0;
}

uint8_t http_server_is_running(void)
{
  return http_state.running;
}

uint8_t http_server_is_tls_active(void)
{
  return http_state.tls_active;
}

const HttpConfig* http_server_get_config(void)
{
  if (!http_state.initialized) {
    return NULL;
  }
  return &http_state.config;
}

/* ============================================================================
 * STATISTICS
 * ============================================================================ */

const HttpServerStats* http_server_get_stats(void)
{
  return &http_state.stats;
}

void http_server_reset_stats(void)
{
  memset(&http_state.stats, 0, sizeof(HttpServerStats));
  ESP_LOGI(TAG, "HTTP server statistics reset");
}

// Called by api_handlers to update stats
void http_server_stat_request(void)
{
  http_state.stats.total_requests++;
}

void http_server_stat_success(void)
{
  http_state.stats.successful_requests++;
}

void http_server_stat_client_error(void)
{
  http_state.stats.client_errors++;
}

void http_server_stat_server_error(void)
{
  http_state.stats.server_errors++;
}

void http_server_stat_auth_failure(void)
{
  http_state.stats.auth_failures++;
}

/* ============================================================================
 * AUTHENTICATION
 * ============================================================================ */

// Check Basic Authentication (returns true if OK or auth not required)
bool http_server_check_auth(httpd_req_t *req)
{
  if (!http_state.config.auth_enabled) {
    return true;  // Auth not required
  }

  char auth_header[128];
  esp_err_t err = httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header));
  if (err != ESP_OK) {
    return false;  // No auth header
  }

  // Check for "Basic " prefix
  if (strncmp(auth_header, "Basic ", 6) != 0) {
    return false;
  }

  // Decode Base64 credentials
  // Format: base64(username:password)
  char* b64_creds = auth_header + 6;

  // Simple Base64 decode (ESP-IDF has mbedtls_base64_decode)
  // For simplicity, we'll use a manual check
  // Expected: base64 of "username:password"

  // Build expected credentials
  char expected[128];
  snprintf(expected, sizeof(expected), "%s:%s",
           http_state.config.username, http_state.config.password);

  // Base64 encode expected
  unsigned char encoded[128];
  size_t encoded_len = 0;
  int ret = mbedtls_base64_encode(encoded, sizeof(encoded), &encoded_len,
                                   (unsigned char*)expected, strlen(expected));
  if (ret != 0) {
    return false;
  }

  // Compare
  if (strncmp(b64_creds, (char*)encoded, encoded_len) == 0) {
    return true;
  }

  return false;
}

/* ============================================================================
 * DEBUGGING
 * ============================================================================ */

void http_server_print_status(void)
{
  debug_printf("\n╔════════════════════════════════════════╗\n");
  debug_printf("║        HTTP SERVER STATUS             ║\n");
  debug_printf("╚════════════════════════════════════════╝\n\n");

  debug_printf("Status:           %s\n", http_state.running ? "Running" : "Stopped");

  if (http_state.running) {
    debug_printf("Port:             %d\n", http_state.config.port);
    debug_printf("Auth Enabled:     %s\n", http_state.config.auth_enabled ? "Yes" : "No");
    if (http_state.config.auth_enabled) {
      debug_printf("Username:         %s\n", http_state.config.username);
    }
  }

  debug_printf("\nStatistics:\n");
  debug_printf("  Total Requests:     %lu\n", http_state.stats.total_requests);
  debug_printf("  Successful (2xx):   %lu\n", http_state.stats.successful_requests);
  debug_printf("  Client Errors (4xx):%lu\n", http_state.stats.client_errors);
  debug_printf("  Server Errors (5xx):%lu\n", http_state.stats.server_errors);
  if (http_state.config.auth_enabled) {
    debug_printf("  Auth Failures:      %lu\n", http_state.stats.auth_failures);
  }

  debug_printf("\n");
}
