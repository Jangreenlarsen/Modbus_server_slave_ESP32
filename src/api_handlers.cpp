/**
 * @file api_handlers.cpp
 * @brief HTTP REST API endpoint handlers implementation (v6.0.0+)
 *
 * LAYER 1.5: Protocol (same level as Telnet server)
 * Responsibility: JSON response building and HTTP request handling
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <lwip/inet.h>

#include "api_handlers.h"
#include "http_server.h"
#include "constants.h"
#include "types.h"
#include "config_struct.h"
#include "registers.h"
#include "counter_engine.h"
#include "counter_config.h"
#include "timer_engine.h"
#include "st_logic_config.h"
#include "wifi_driver.h"
#include "build_version.h"
#include "debug_flags.h"
#include "debug.h"
#include "config_save.h"
#include "config_load.h"
#include "config_apply.h"
#include "gpio_driver.h"

static const char *TAG = "API_HDLR";

// External functions from http_server.cpp for stats
extern void http_server_stat_request(void);
extern void http_server_stat_success(void);
extern void http_server_stat_client_error(void);
extern void http_server_stat_server_error(void);
extern void http_server_stat_auth_failure(void);
extern bool http_server_check_auth(httpd_req_t *req);

// Forward declarations for handlers used in suffix routing (defined later in this file)
esp_err_t api_handler_logic_source_get(httpd_req_t *req);
esp_err_t api_handler_logic_source_post(httpd_req_t *req);
esp_err_t api_handler_logic_enable(httpd_req_t *req);
esp_err_t api_handler_logic_disable(httpd_req_t *req);
esp_err_t api_handler_logic_stats(httpd_req_t *req);
esp_err_t api_handler_counter_reset(httpd_req_t *req);
esp_err_t api_handler_counter_start(httpd_req_t *req);
esp_err_t api_handler_counter_stop(httpd_req_t *req);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

int api_extract_id_from_uri(httpd_req_t *req, const char *prefix)
{
  const char *uri = req->uri;
  size_t prefix_len = strlen(prefix);

  // Check if URI starts with prefix
  if (strncmp(uri, prefix, prefix_len) != 0) {
    return -1;
  }

  // Extract ID after prefix
  const char *id_str = uri + prefix_len;
  if (*id_str == '\0') {
    return -1;
  }

  // Find end of ID (before any query params)
  char id_buf[8];
  int i = 0;
  while (*id_str && *id_str != '?' && *id_str != '/' && i < 7) {
    id_buf[i++] = *id_str++;
  }
  id_buf[i] = '\0';

  return atoi(id_buf);
}

esp_err_t api_send_error(httpd_req_t *req, int status, const char *error_msg)
{
  DebugFlags* dbg = debug_flags_get();
  if (dbg->http_api) {
    debug_printf("[API] %s -> %d %s\n", req->uri, status, error_msg);
  }

  char buf[256];
  snprintf(buf, sizeof(buf), "{\"error\":\"%s\",\"status\":%d}", error_msg, status);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate");
  httpd_resp_set_hdr(req, "Connection", "keep-alive");
  httpd_resp_set_hdr(req, "Keep-Alive", "timeout=15, max=100");
  httpd_resp_set_status(req, status == 404 ? "404 Not Found" :
                             status == 400 ? "400 Bad Request" :
                             status == 401 ? "401 Unauthorized" :
                             status == 500 ? "500 Internal Server Error" : "400 Bad Request");

  // For 401, add WWW-Authenticate header to trigger browser login dialog
  if (status == 401) {
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Modbus ESP32\"");
  }

  httpd_resp_sendstr(req, buf);

  if (status == 401) {
    http_server_stat_auth_failure();
  } else if (status >= 500) {
    http_server_stat_server_error();
  } else {
    http_server_stat_client_error();
  }

  return ESP_OK;
}

esp_err_t api_send_json(httpd_req_t *req, const char *json_str)
{
  DebugFlags* dbg = debug_flags_get();
  if (dbg->http_api) {
    debug_printf("[API] %s -> 200 OK (%u bytes)\n", req->uri, (unsigned)strlen(json_str));
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate");
  httpd_resp_set_hdr(req, "Connection", "keep-alive");
  httpd_resp_set_hdr(req, "Keep-Alive", "timeout=15, max=100");
  httpd_resp_sendstr(req, json_str);
  http_server_stat_success();
  return ESP_OK;
}

/* ============================================================================
 * AUTHENTICATION CHECK MACRO
 * ============================================================================ */

#define CHECK_API_ENABLED(req) \
  do { \
    if (!g_persist_config.network.http.api_enabled) { \
      return api_send_error(req, 403, "API disabled"); \
    } \
  } while(0)

#define CHECK_AUTH(req) \
  do { \
    CHECK_API_ENABLED(req); \
    if (!http_server_check_auth(req)) { \
      return api_send_error(req, 401, "Authentication required"); \
    } \
  } while(0)

/* ============================================================================
 * GET /api/ - API Discovery (list all endpoints)
 * ============================================================================ */

esp_err_t api_handler_endpoints(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  // Use heap allocation for larger response (endpoints list ~5KB with v6.0.4+ additions)
  char *buf = (char *)malloc(6144);
  if (!buf) {
    return api_send_error(req, 500, "Out of memory");
  }

  // Build JSON manually for efficiency
  int len = snprintf(buf, 6144,
    "{"
    "\"name\":\"Modbus ESP32 REST API\","
    "\"version\":\"%s\","
    "\"build\":%d,"
    "\"endpoints\":["
    "{\"method\":\"GET\",\"path\":\"/api/\",\"desc\":\"List endpoints\"},"
    "{\"method\":\"GET\",\"path\":\"/api/status\",\"desc\":\"System status\"},"
    "{\"method\":\"GET\",\"path\":\"/api/config\",\"desc\":\"Full configuration\"},"
    "{\"method\":\"GET\",\"path\":\"/api/counters\",\"desc\":\"All counters\"},"
    "{\"method\":\"GET\",\"path\":\"/api/counters/{1-4}\",\"desc\":\"Single counter\"},"
    "{\"method\":\"POST\",\"path\":\"/api/counters/{1-4}/reset\",\"desc\":\"Reset counter\"},"
    "{\"method\":\"POST\",\"path\":\"/api/counters/{1-4}/start\",\"desc\":\"Start counter\"},"
    "{\"method\":\"POST\",\"path\":\"/api/counters/{1-4}/stop\",\"desc\":\"Stop counter\"},"
    "{\"method\":\"GET\",\"path\":\"/api/timers\",\"desc\":\"All timers\"},"
    "{\"method\":\"GET\",\"path\":\"/api/timers/{1-4}\",\"desc\":\"Single timer\"},"
    "{\"method\":\"GET\",\"path\":\"/api/registers/hr/{addr}\",\"desc\":\"Read HR\"},"
    "{\"method\":\"POST\",\"path\":\"/api/registers/hr/{addr}\",\"desc\":\"Write HR\"},"
    "{\"method\":\"GET\",\"path\":\"/api/registers/ir/{addr}\",\"desc\":\"Read IR\"},"
    "{\"method\":\"GET\",\"path\":\"/api/registers/coils/{addr}\",\"desc\":\"Read coil\"},"
    "{\"method\":\"POST\",\"path\":\"/api/registers/coils/{addr}\",\"desc\":\"Write coil\"},"
    "{\"method\":\"GET\",\"path\":\"/api/registers/di/{addr}\",\"desc\":\"Read DI\"},"
    "{\"method\":\"GET\",\"path\":\"/api/gpio\",\"desc\":\"All GPIO mappings\"},"
    "{\"method\":\"GET\",\"path\":\"/api/gpio/{pin}\",\"desc\":\"Single GPIO\"},"
    "{\"method\":\"POST\",\"path\":\"/api/gpio/{pin}\",\"desc\":\"Write GPIO\"},"
    "{\"method\":\"GET\",\"path\":\"/api/logic\",\"desc\":\"ST Logic programs\"},"
    "{\"method\":\"GET\",\"path\":\"/api/logic/{1-4}\",\"desc\":\"Single program\"},"
    "{\"method\":\"GET\",\"path\":\"/api/logic/{1-4}/source\",\"desc\":\"Download ST code\"},"
    "{\"method\":\"POST\",\"path\":\"/api/logic/{1-4}/source\",\"desc\":\"Upload ST code\"},"
    "{\"method\":\"POST\",\"path\":\"/api/logic/{1-4}/enable\",\"desc\":\"Enable program\"},"
    "{\"method\":\"POST\",\"path\":\"/api/logic/{1-4}/disable\",\"desc\":\"Disable program\"},"
    "{\"method\":\"DELETE\",\"path\":\"/api/logic/{1-4}\",\"desc\":\"Delete program\"},"
    "{\"method\":\"GET\",\"path\":\"/api/logic/{1-4}/stats\",\"desc\":\"Program stats\"},"
    "{\"method\":\"GET\",\"path\":\"/api/debug\",\"desc\":\"Debug flags\"},"
    "{\"method\":\"POST\",\"path\":\"/api/debug\",\"desc\":\"Set debug flags\"},"
    "{\"method\":\"POST\",\"path\":\"/api/system/reboot\",\"desc\":\"Reboot ESP32\"},"
    "{\"method\":\"POST\",\"path\":\"/api/system/save\",\"desc\":\"Save config to NVS\"},"
    "{\"method\":\"POST\",\"path\":\"/api/system/load\",\"desc\":\"Load config from NVS\"},"
    "{\"method\":\"POST\",\"path\":\"/api/system/defaults\",\"desc\":\"Reset to defaults\"}"
    "]"
    "}",
    PROJECT_VERSION, BUILD_NUMBER);

  if (len < 0 || len >= 6144) {
    free(buf);
    return api_send_error(req, 500, "Buffer overflow");
  }

  esp_err_t ret = api_send_json(req, buf);
  free(buf);
  return ret;
}

/* ============================================================================
 * GET /api/status
 * ============================================================================ */

esp_err_t api_handler_status(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  JsonDocument doc;

  doc["version"] = PROJECT_VERSION;
  doc["build"] = BUILD_NUMBER;
  doc["uptime_ms"] = millis();
  doc["heap_free"] = ESP.getFreeHeap();
  doc["wifi_connected"] = wifi_driver_is_connected() ? true : false;

  // IP address
  if (wifi_driver_is_connected()) {
    uint32_t ip = wifi_driver_get_local_ip();
    struct in_addr addr;
    addr.s_addr = ip;
    doc["ip"] = inet_ntoa(addr);
  } else {
    doc["ip"] = nullptr;
  }

  doc["modbus_slave_id"] = g_persist_config.modbus_slave.slave_id;
  doc["https"] = http_server_is_tls_active() ? true : false;

  char buf[HTTP_JSON_DOC_SIZE];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * GET /api/counters
 * ============================================================================ */

esp_err_t api_handler_counters(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  JsonDocument doc;
  JsonArray counters = doc["counters"].to<JsonArray>();

  for (int i = 0; i < COUNTER_COUNT; i++) {
    CounterConfig cfg;
    if (counter_engine_get_config(i + 1, &cfg)) {
      JsonObject counter = counters.add<JsonObject>();
      counter["id"] = i + 1;
      counter["enabled"] = cfg.enabled ? true : false;

      const char *mode_str = "DISABLED";
      switch (cfg.hw_mode) {
        case COUNTER_HW_SW:     mode_str = "SW"; break;
        case COUNTER_HW_SW_ISR: mode_str = "SW_ISR"; break;
        case COUNTER_HW_PCNT:   mode_str = "HW_PCNT"; break;
      }
      counter["mode"] = mode_str;
      counter["value"] = counter_engine_get_value(i + 1);
    }
  }

  char buf[HTTP_JSON_DOC_SIZE];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * GET /api/counters/{id}
 * ============================================================================ */

esp_err_t api_handler_counter_single(httpd_req_t *req)
{
  // Check for action suffixes (ESP-IDF wildcard only matches at end of URI,
  // so /api/counters/*/reset etc. never match - we must route internally)
  const char *uri = req->uri;
  size_t uri_len = strlen(uri);

  if (req->method == HTTP_POST) {
    if (uri_len >= 6 && strcmp(uri + uri_len - 6, "/reset") == 0) {
      return api_handler_counter_reset(req);
    }
    if (uri_len >= 6 && strcmp(uri + uri_len - 6, "/start") == 0) {
      return api_handler_counter_start(req);
    }
    if (uri_len >= 5 && strcmp(uri + uri_len - 5, "/stop") == 0) {
      return api_handler_counter_stop(req);
    }
  }

  http_server_stat_request();
  CHECK_AUTH(req);

  int id = api_extract_id_from_uri(req, "/api/counters/");
  if (id < 1 || id > COUNTER_COUNT) {
    return api_send_error(req, 400, "Invalid counter ID (must be 1-4)");
  }

  CounterConfig cfg;
  if (!counter_engine_get_config(id, &cfg)) {
    return api_send_error(req, 404, "Counter not found");
  }

  JsonDocument doc;

  doc["id"] = id;
  doc["enabled"] = cfg.enabled ? true : false;

  const char *mode_str = "DISABLED";
  switch (cfg.hw_mode) {
    case COUNTER_HW_SW:     mode_str = "SW"; break;
    case COUNTER_HW_SW_ISR: mode_str = "SW_ISR"; break;
    case COUNTER_HW_PCNT:   mode_str = "HW_PCNT"; break;
  }
  doc["mode"] = mode_str;

  uint64_t value = counter_engine_get_value(id);
  doc["value"] = value;

  // Read raw value from raw register
  if (cfg.raw_reg != 0xFFFF) {
    uint16_t raw = registers_get_holding_register(cfg.raw_reg);
    doc["raw"] = raw;
  }

  // Frequency
  if (cfg.freq_reg != 0xFFFF) {
    uint16_t freq = registers_get_holding_register(cfg.freq_reg);
    doc["frequency"] = freq;
  }

  // Control register flags
  if (cfg.ctrl_reg != 0xFFFF) {
    uint16_t ctrl = registers_get_holding_register(cfg.ctrl_reg);
    doc["running"] = (ctrl & 0x04) ? true : false;
    doc["overflow"] = (ctrl & 0x08) ? true : false;
    doc["compare_triggered"] = (ctrl & 0x10) ? true : false;
  }

  char buf[HTTP_JSON_DOC_SIZE];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * GET /api/timers
 * ============================================================================ */

esp_err_t api_handler_timers(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  JsonDocument doc;
  JsonArray timers = doc["timers"].to<JsonArray>();

  for (int i = 0; i < TIMER_COUNT; i++) {
    TimerConfig cfg;
    if (timer_engine_get_config(i + 1, &cfg)) {
      JsonObject timer = timers.add<JsonObject>();
      timer["id"] = i + 1;
      timer["enabled"] = cfg.enabled ? true : false;

      const char *mode_str = "DISABLED";
      switch (cfg.mode) {
        case TIMER_MODE_DISABLED: mode_str = "DISABLED"; break;
        case TIMER_MODE_1_ONESHOT: mode_str = "ONESHOT"; break;
        case TIMER_MODE_2_MONOSTABLE: mode_str = "MONOSTABLE"; break;
        case TIMER_MODE_3_ASTABLE: mode_str = "ASTABLE"; break;
        case TIMER_MODE_4_INPUT_TRIGGERED: mode_str = "INPUT_TRIGGERED"; break;
      }
      timer["mode"] = mode_str;

      // Read output coil state
      if (cfg.output_coil != 0xFFFF) {
        timer["output"] = registers_get_coil(cfg.output_coil) ? true : false;
      }
    }
  }

  char buf[HTTP_JSON_DOC_SIZE];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * GET /api/timers/{id}
 * ============================================================================ */

esp_err_t api_handler_timer_single(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int id = api_extract_id_from_uri(req, "/api/timers/");
  if (id < 1 || id > TIMER_COUNT) {
    return api_send_error(req, 400, "Invalid timer ID (must be 1-4)");
  }

  TimerConfig cfg;
  if (!timer_engine_get_config(id, &cfg)) {
    return api_send_error(req, 404, "Timer not found");
  }

  JsonDocument doc;

  doc["id"] = id;
  doc["enabled"] = cfg.enabled ? true : false;

  const char *mode_str = "DISABLED";
  switch (cfg.mode) {
    case TIMER_MODE_DISABLED: mode_str = "DISABLED"; break;
    case TIMER_MODE_1_ONESHOT: mode_str = "ONESHOT"; break;
    case TIMER_MODE_2_MONOSTABLE: mode_str = "MONOSTABLE"; break;
    case TIMER_MODE_3_ASTABLE: mode_str = "ASTABLE"; break;
    case TIMER_MODE_4_INPUT_TRIGGERED: mode_str = "INPUT_TRIGGERED"; break;
  }
  doc["mode"] = mode_str;

  // Output coil
  if (cfg.output_coil != 0xFFFF) {
    doc["output_coil"] = cfg.output_coil;
    doc["output"] = registers_get_coil(cfg.output_coil) ? true : false;
  }

  // Mode-specific parameters
  switch (cfg.mode) {
    case TIMER_MODE_1_ONESHOT:
      doc["phase1_duration_ms"] = cfg.phase1_duration_ms;
      doc["phase2_duration_ms"] = cfg.phase2_duration_ms;
      doc["phase3_duration_ms"] = cfg.phase3_duration_ms;
      break;
    case TIMER_MODE_2_MONOSTABLE:
      doc["pulse_duration_ms"] = cfg.pulse_duration_ms;
      break;
    case TIMER_MODE_3_ASTABLE:
      doc["on_duration_ms"] = cfg.on_duration_ms;
      doc["off_duration_ms"] = cfg.off_duration_ms;
      break;
    case TIMER_MODE_4_INPUT_TRIGGERED:
      doc["input_dis"] = cfg.input_dis;
      doc["delay_ms"] = cfg.delay_ms;
      break;
    default:
      break;
  }

  char buf[HTTP_JSON_DOC_SIZE];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * GET /api/registers/hr/{addr}
 * ============================================================================ */

esp_err_t api_handler_hr_read(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int addr = api_extract_id_from_uri(req, "/api/registers/hr/");
  if (addr < 0 || addr >= HOLDING_REGS_SIZE) {
    return api_send_error(req, 400, "Invalid register address");
  }

  uint16_t value = registers_get_holding_register(addr);

  JsonDocument doc;
  doc["address"] = addr;
  doc["value"] = value;

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * POST /api/registers/hr/{addr}
 * ============================================================================ */

esp_err_t api_handler_hr_write(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int addr = api_extract_id_from_uri(req, "/api/registers/hr/");
  if (addr < 0 || addr >= HOLDING_REGS_SIZE) {
    return api_send_error(req, 400, "Invalid register address");
  }

  // Read request body
  char content[128];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    return api_send_error(req, 400, "Failed to read request body");
  }
  content[ret] = '\0';

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, content);
  if (error) {
    return api_send_error(req, 400, "Invalid JSON");
  }

  if (!doc.containsKey("value")) {
    return api_send_error(req, 400, "Missing 'value' field");
  }

  uint16_t value = doc["value"].as<uint16_t>();

  // Write register
  registers_set_holding_register(addr, value);

  // Response
  JsonDocument resp;
  resp["address"] = addr;
  resp["value"] = value;
  resp["status"] = "ok";

  char buf[256];
  serializeJson(resp, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * GET /api/registers/ir/{addr}
 * ============================================================================ */

esp_err_t api_handler_ir_read(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int addr = api_extract_id_from_uri(req, "/api/registers/ir/");
  if (addr < 0 || addr >= INPUT_REGS_SIZE) {
    return api_send_error(req, 400, "Invalid register address");
  }

  uint16_t value = registers_get_input_register(addr);

  JsonDocument doc;
  doc["address"] = addr;
  doc["value"] = value;

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * GET /api/registers/coils/{addr}
 * ============================================================================ */

esp_err_t api_handler_coil_read(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int addr = api_extract_id_from_uri(req, "/api/registers/coils/");
  if (addr < 0 || addr >= COILS_SIZE * 8) {
    return api_send_error(req, 400, "Invalid coil address");
  }

  uint8_t value = registers_get_coil(addr);

  JsonDocument doc;
  doc["address"] = addr;
  doc["value"] = value ? true : false;

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * POST /api/registers/coils/{addr}
 * ============================================================================ */

esp_err_t api_handler_coil_write(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int addr = api_extract_id_from_uri(req, "/api/registers/coils/");
  if (addr < 0 || addr >= COILS_SIZE * 8) {
    return api_send_error(req, 400, "Invalid coil address");
  }

  // Read request body
  char content[128];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    return api_send_error(req, 400, "Failed to read request body");
  }
  content[ret] = '\0';

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, content);
  if (error) {
    return api_send_error(req, 400, "Invalid JSON");
  }

  if (!doc.containsKey("value")) {
    return api_send_error(req, 400, "Missing 'value' field");
  }

  // Accept bool or int
  uint8_t value = 0;
  if (doc["value"].is<bool>()) {
    value = doc["value"].as<bool>() ? 1 : 0;
  } else {
    value = doc["value"].as<int>() ? 1 : 0;
  }

  // Write coil
  registers_set_coil(addr, value);

  // Response
  JsonDocument resp;
  resp["address"] = addr;
  resp["value"] = value ? true : false;
  resp["status"] = "ok";

  char buf[256];
  serializeJson(resp, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * GET /api/registers/di/{addr}
 * ============================================================================ */

esp_err_t api_handler_di_read(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int addr = api_extract_id_from_uri(req, "/api/registers/di/");
  if (addr < 0 || addr >= DISCRETE_INPUTS_SIZE * 8) {
    return api_send_error(req, 400, "Invalid discrete input address");
  }

  uint8_t value = registers_get_discrete_input(addr);

  JsonDocument doc;
  doc["address"] = addr;
  doc["value"] = value ? true : false;

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * GET /api/logic
 * ============================================================================ */

esp_err_t api_handler_logic(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  st_logic_engine_state_t *state = st_logic_get_state();
  if (!state) {
    return api_send_error(req, 500, "ST Logic not initialized");
  }

  JsonDocument doc;
  doc["enabled"] = state->enabled ? true : false;
  doc["execution_interval_ms"] = state->execution_interval_ms;
  doc["total_cycles"] = state->total_cycles;

  JsonArray programs = doc["programs"].to<JsonArray>();

  for (int i = 0; i < ST_LOGIC_MAX_PROGRAMS; i++) {
    st_logic_program_config_t *prog = &state->programs[i];
    JsonObject p = programs.add<JsonObject>();
    p["id"] = i + 1;
    p["name"] = prog->name;
    p["enabled"] = prog->enabled ? true : false;
    p["compiled"] = prog->compiled ? true : false;
    p["execution_count"] = prog->execution_count;
    p["error_count"] = prog->error_count;

    if (prog->last_error[0] != '\0') {
      p["last_error"] = prog->last_error;
    }
  }

  char buf[HTTP_JSON_DOC_SIZE];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * GET /api/logic/{id}
 * ============================================================================ */

esp_err_t api_handler_logic_single(httpd_req_t *req)
{
  // Internal suffix routing - ESP-IDF wildcard only supports * at end of URI,
  // so /api/logic/*/source, /api/logic/*/enable etc. never match.
  // All requests to /api/logic/{id}/xxx land here via /api/logic/* wildcard.
  const char *uri = req->uri;
  size_t uri_len = strlen(uri);

  // GET suffixes
  if (req->method == HTTP_GET) {
    if (uri_len >= 7 && strcmp(uri + uri_len - 7, "/source") == 0) {
      return api_handler_logic_source_get(req);
    }
    if (uri_len >= 6 && strcmp(uri + uri_len - 6, "/stats") == 0) {
      return api_handler_logic_stats(req);
    }
  }

  // POST suffixes
  if (req->method == HTTP_POST) {
    if (uri_len >= 7 && strcmp(uri + uri_len - 7, "/source") == 0) {
      return api_handler_logic_source_post(req);
    }
    if (uri_len >= 7 && strcmp(uri + uri_len - 7, "/enable") == 0) {
      return api_handler_logic_enable(req);
    }
    if (uri_len >= 8 && strcmp(uri + uri_len - 8, "/disable") == 0) {
      return api_handler_logic_disable(req);
    }
  }

  // Normal logic/{id} handling
  http_server_stat_request();
  CHECK_AUTH(req);

  int id = api_extract_id_from_uri(req, "/api/logic/");
  if (id < 1 || id > ST_LOGIC_MAX_PROGRAMS) {
    return api_send_error(req, 400, "Invalid logic program ID");
  }

  st_logic_engine_state_t *state = st_logic_get_state();
  if (!state) {
    return api_send_error(req, 500, "ST Logic not initialized");
  }

  st_logic_program_config_t *prog = &state->programs[id - 1];

  JsonDocument doc;

  doc["id"] = id;
  doc["name"] = prog->name;
  doc["enabled"] = prog->enabled ? true : false;
  doc["compiled"] = prog->compiled ? true : false;
  doc["execution_count"] = prog->execution_count;
  doc["error_count"] = prog->error_count;
  doc["last_execution_us"] = prog->last_execution_us;
  doc["min_execution_us"] = prog->min_execution_us;
  doc["max_execution_us"] = prog->max_execution_us;
  doc["overrun_count"] = prog->overrun_count;

  if (prog->last_error[0] != '\0') {
    doc["last_error"] = prog->last_error;
  }

  // Variables (if compiled)
  if (prog->compiled && prog->bytecode.var_count > 0) {
    JsonArray vars = doc["variables"].to<JsonArray>();
    for (int i = 0; i < prog->bytecode.var_count && i < 32; i++) {
      JsonObject v = vars.add<JsonObject>();
      v["index"] = i;
      v["name"] = prog->bytecode.var_names[i];

      const char *type_str = "INT";
      switch (prog->bytecode.var_types[i]) {
        case ST_TYPE_BOOL: type_str = "BOOL"; break;
        case ST_TYPE_INT:  type_str = "INT"; break;
        case ST_TYPE_DINT: type_str = "DINT"; break;
        case ST_TYPE_REAL: type_str = "REAL"; break;
        default: break;
      }
      v["type"] = type_str;

      // Get current value
      st_value_t val = prog->bytecode.variables[i];
      switch (prog->bytecode.var_types[i]) {
        case ST_TYPE_BOOL:
          v["value"] = val.bool_val ? true : false;
          break;
        case ST_TYPE_INT:
          v["value"] = val.int_val;
          break;
        case ST_TYPE_DINT:
          v["value"] = val.dint_val;
          break;
        case ST_TYPE_REAL:
          v["value"] = val.real_val;
          break;
        default:
          v["value"] = val.int_val;
          break;
      }
    }
  }

  char buf[HTTP_SERVER_MAX_RESP_SIZE];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * GET /api/logic/{id}/source - Download ST source code
 * ============================================================================ */

esp_err_t api_handler_logic_source_get(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  // Extract program ID from URI (e.g., "/api/logic/1/source" -> 1)
  int id = api_extract_id_from_uri(req, "/api/logic/");
  if (id < 1 || id > ST_LOGIC_MAX_PROGRAMS) {
    return api_send_error(req, 400, "Invalid logic program ID");
  }

  st_logic_engine_state_t *state = st_logic_get_state();
  if (!state) {
    return api_send_error(req, 500, "ST Logic not initialized");
  }

  st_logic_program_config_t *prog = &state->programs[id - 1];
  const char *source = st_logic_get_source_code(state, id - 1);
  if (!source || prog->source_size == 0) {
    return api_send_error(req, 404, "No source code uploaded for this program");
  }

  // IMPORTANT: source_pool entries are NOT null-terminated (contiguous in shared pool).
  // Must use prog->source_size, NOT strlen(source).
  size_t source_len = prog->source_size;
  size_t buf_size = source_len + 256;  // Extra space for JSON wrapper
  char *buf = (char *)malloc(buf_size);
  if (!buf) {
    return api_send_error(req, 500, "Out of memory");
  }

  // Make null-terminated copy for JSON serialization
  char *source_copy = (char *)malloc(source_len + 1);
  if (!source_copy) {
    free(buf);
    return api_send_error(req, 500, "Out of memory");
  }
  memcpy(source_copy, source, source_len);
  source_copy[source_len] = '\0';

  JsonDocument doc;
  doc["id"] = id;
  doc["name"] = prog->name;
  doc["source"] = source_copy;
  doc["size"] = source_len;

  size_t json_len = serializeJson(doc, buf, buf_size);
  free(source_copy);

  if (json_len >= buf_size) {
    free(buf);
    return api_send_error(req, 500, "Response too large");
  }

  esp_err_t ret = api_send_json(req, buf);
  free(buf);
  return ret;
}

/* ============================================================================
 * POST /api/logic/{id}/source - Upload ST source code
 * ============================================================================ */

esp_err_t api_handler_logic_source_post(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  // Extract program ID from URI
  int id = api_extract_id_from_uri(req, "/api/logic/");
  if (id < 1 || id > ST_LOGIC_MAX_PROGRAMS) {
    return api_send_error(req, 400, "Invalid logic program ID");
  }

  st_logic_engine_state_t *state = st_logic_get_state();
  if (!state) {
    return api_send_error(req, 500, "ST Logic not initialized");
  }

  // Get content length
  size_t content_len = req->content_len;
  if (content_len == 0) {
    return api_send_error(req, 400, "Empty request body");
  }
  if (content_len > 8192) {
    return api_send_error(req, 400, "Request too large (max 8KB)");
  }

  // Allocate buffer for request body
  char *content = (char *)malloc(content_len + 1);
  if (!content) {
    return api_send_error(req, 500, "Out of memory");
  }

  // Read request body
  int received = 0;
  while (received < content_len) {
    int ret = httpd_req_recv(req, content + received, content_len - received);
    if (ret <= 0) {
      free(content);
      return api_send_error(req, 400, "Failed to read request body");
    }
    received += ret;
  }
  content[content_len] = '\0';

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, content);
  if (error) {
    free(content);
    return api_send_error(req, 400, "Invalid JSON");
  }

  if (!doc.containsKey("source")) {
    free(content);
    return api_send_error(req, 400, "Missing 'source' field");
  }

  const char *source = doc["source"].as<const char *>();
  if (!source || strlen(source) == 0) {
    free(content);
    return api_send_error(req, 400, "Empty source code");
  }

  // Upload source code
  uint32_t source_len = strlen(source);
  bool success = st_logic_upload(state, id - 1, source, source_len);
  free(content);

  if (!success) {
    return api_send_error(req, 500, "Failed to upload - pool full or compile error");
  }

  // Build success response
  st_logic_program_config_t *prog = &state->programs[id - 1];

  JsonDocument resp;
  resp["status"] = "ok";
  resp["id"] = id;
  resp["name"] = prog->name;
  resp["compiled"] = prog->compiled ? true : false;
  resp["source_size"] = source_len;

  if (prog->last_error[0] != '\0') {
    resp["error"] = prog->last_error;
  }

  char buf[512];
  serializeJson(resp, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * SYSTEM ENDPOINTS (v6.0.4+)
 * ============================================================================ */

esp_err_t api_handler_system_reboot(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  JsonDocument doc;
  doc["status"] = "ok";
  doc["message"] = "Rebooting in 1 second...";

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  esp_err_t ret = api_send_json(req, buf);

  // Schedule reboot after response is sent
  delay(1000);
  ESP.restart();

  return ret;
}

esp_err_t api_handler_system_save(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  bool success = config_save_to_nvs(&g_persist_config);

  JsonDocument doc;
  doc["status"] = success ? "ok" : "error";
  doc["message"] = success ? "Configuration saved to NVS" : "Failed to save configuration";

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

esp_err_t api_handler_system_load(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  bool success = config_load_from_nvs(&g_persist_config);
  if (success) {
    success = config_apply(&g_persist_config);
  }

  JsonDocument doc;
  doc["status"] = success ? "ok" : "error";
  doc["message"] = success ? "Configuration loaded and applied" : "Failed to load configuration";

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

esp_err_t api_handler_system_defaults(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  config_struct_create_default();
  bool success = config_apply(&g_persist_config);

  JsonDocument doc;
  doc["status"] = success ? "ok" : "error";
  doc["message"] = success ? "Reset to factory defaults (not saved)" : "Failed to apply defaults";

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * COUNTER CONTROL ENDPOINTS (v6.0.4+)
 * ============================================================================ */

esp_err_t api_handler_counter_reset(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int id = api_extract_id_from_uri(req, "/api/counters/");
  if (id < 1 || id > COUNTER_COUNT) {
    return api_send_error(req, 400, "Invalid counter ID (must be 1-4)");
  }

  counter_engine_reset(id);

  JsonDocument doc;
  doc["status"] = "ok";
  doc["counter"] = id;
  doc["message"] = "Counter reset to start value";

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

esp_err_t api_handler_counter_start(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int id = api_extract_id_from_uri(req, "/api/counters/");
  if (id < 1 || id > COUNTER_COUNT) {
    return api_send_error(req, 400, "Invalid counter ID (must be 1-4)");
  }

  CounterConfig cfg;
  if (!counter_engine_get_config(id, &cfg)) {
    return api_send_error(req, 404, "Counter not configured");
  }

  if (cfg.ctrl_reg >= HOLDING_REGS_SIZE) {
    return api_send_error(req, 500, "Counter has no control register");
  }

  // Set start bit (bit 1)
  uint16_t ctrl_val = registers_get_holding_register(cfg.ctrl_reg);
  ctrl_val |= 0x0002;
  registers_set_holding_register(cfg.ctrl_reg, ctrl_val);

  JsonDocument doc;
  doc["status"] = "ok";
  doc["counter"] = id;
  doc["message"] = "Counter started";

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

esp_err_t api_handler_counter_stop(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int id = api_extract_id_from_uri(req, "/api/counters/");
  if (id < 1 || id > COUNTER_COUNT) {
    return api_send_error(req, 400, "Invalid counter ID (must be 1-4)");
  }

  CounterConfig cfg;
  if (!counter_engine_get_config(id, &cfg)) {
    return api_send_error(req, 404, "Counter not configured");
  }

  if (cfg.ctrl_reg >= HOLDING_REGS_SIZE) {
    return api_send_error(req, 500, "Counter has no control register");
  }

  // Set stop bit (bit 2)
  uint16_t ctrl_val = registers_get_holding_register(cfg.ctrl_reg);
  ctrl_val |= 0x0004;
  registers_set_holding_register(cfg.ctrl_reg, ctrl_val);

  JsonDocument doc;
  doc["status"] = "ok";
  doc["counter"] = id;
  doc["message"] = "Counter stopped";

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * GPIO ENDPOINTS (v6.0.4+)
 * ============================================================================ */

esp_err_t api_handler_gpio(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  JsonDocument doc;
  JsonArray gpios = doc["gpios"].to<JsonArray>();

  // List GPIO mappings from var_maps array
  for (int i = 0; i < g_persist_config.var_map_count; i++) {
    const VariableMapping *m = &g_persist_config.var_maps[i];
    if (m->source_type != MAPPING_SOURCE_GPIO) continue;
    if (m->gpio_pin == 0 || m->gpio_pin == 0xFF) continue;

    JsonObject gpio = gpios.add<JsonObject>();
    gpio["pin"] = m->gpio_pin;
    gpio["direction"] = m->is_input ? "input" : "output";

    // Read current value
    uint8_t level = gpio_read(m->gpio_pin);
    gpio["value"] = level ? 1 : 0;

    // Show register binding if configured
    if (m->coil_reg != 0xFFFF) {
      gpio["coil"] = m->coil_reg;
    }
    if (m->input_reg != 0xFFFF) {
      gpio["register"] = m->input_reg;
    }
  }

  char buf[HTTP_JSON_DOC_SIZE];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

esp_err_t api_handler_gpio_single(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int pin = api_extract_id_from_uri(req, "/api/gpio/");
  if (pin < 0 || pin > 39) {
    return api_send_error(req, 400, "Invalid GPIO pin (must be 0-39)");
  }

  // Find GPIO mapping in var_maps
  const VariableMapping *found = NULL;
  for (int i = 0; i < g_persist_config.var_map_count; i++) {
    const VariableMapping *m = &g_persist_config.var_maps[i];
    if (m->source_type == MAPPING_SOURCE_GPIO && m->gpio_pin == pin) {
      found = m;
      break;
    }
  }

  JsonDocument doc;
  doc["pin"] = pin;
  doc["value"] = gpio_read(pin) ? 1 : 0;

  if (found) {
    doc["configured"] = true;
    doc["direction"] = found->is_input ? "input" : "output";
    if (found->coil_reg != 0xFFFF) {
      doc["coil"] = found->coil_reg;
    }
    if (found->input_reg != 0xFFFF) {
      doc["register"] = found->input_reg;
    }
  } else {
    doc["configured"] = false;
  }

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

esp_err_t api_handler_gpio_write(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int pin = api_extract_id_from_uri(req, "/api/gpio/");
  if (pin < 0 || pin > 39) {
    return api_send_error(req, 400, "Invalid GPIO pin (must be 0-39)");
  }

  // Read request body
  char content[128];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    return api_send_error(req, 400, "Failed to read request body");
  }
  content[ret] = '\0';

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, content);
  if (error) {
    return api_send_error(req, 400, "Invalid JSON");
  }

  if (!doc.containsKey("value")) {
    return api_send_error(req, 400, "Missing 'value' field");
  }

  uint8_t value = 0;
  if (doc["value"].is<bool>()) {
    value = doc["value"].as<bool>() ? 1 : 0;
  } else {
    value = doc["value"].as<int>() ? 1 : 0;
  }

  // Validate pin is configured as output in var_maps
  bool pin_configured_output = false;
  for (int i = 0; i < g_persist_config.var_map_count; i++) {
    const VariableMapping *m = &g_persist_config.var_maps[i];
    if (m->gpio_pin == pin && m->is_input == 0) {
      pin_configured_output = true;
      break;
    }
  }
  if (!pin_configured_output) {
    return api_send_error(req, 400, "GPIO pin not configured as output");
  }

  // Write GPIO
  gpio_write(pin, value);

  JsonDocument resp;
  resp["status"] = "ok";
  resp["pin"] = pin;
  resp["value"] = value ? 1 : 0;

  char buf[256];
  serializeJson(resp, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * LOGIC CONTROL ENDPOINTS (v6.0.4+)
 * ============================================================================ */

esp_err_t api_handler_logic_enable(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int id = api_extract_id_from_uri(req, "/api/logic/");
  if (id < 1 || id > ST_LOGIC_MAX_PROGRAMS) {
    return api_send_error(req, 400, "Invalid logic program ID");
  }

  st_logic_engine_state_t *state = st_logic_get_state();
  if (!state) {
    return api_send_error(req, 500, "ST Logic not initialized");
  }

  bool success = st_logic_set_enabled(state, id - 1, 1);

  JsonDocument doc;
  doc["status"] = success ? "ok" : "error";
  doc["program"] = id;
  doc["enabled"] = true;
  doc["message"] = success ? "Program enabled" : "Failed to enable program";

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

esp_err_t api_handler_logic_disable(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int id = api_extract_id_from_uri(req, "/api/logic/");
  if (id < 1 || id > ST_LOGIC_MAX_PROGRAMS) {
    return api_send_error(req, 400, "Invalid logic program ID");
  }

  st_logic_engine_state_t *state = st_logic_get_state();
  if (!state) {
    return api_send_error(req, 500, "ST Logic not initialized");
  }

  bool success = st_logic_set_enabled(state, id - 1, 0);

  JsonDocument doc;
  doc["status"] = success ? "ok" : "error";
  doc["program"] = id;
  doc["enabled"] = false;
  doc["message"] = success ? "Program disabled" : "Failed to disable program";

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

esp_err_t api_handler_logic_delete(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int id = api_extract_id_from_uri(req, "/api/logic/");
  if (id < 1 || id > ST_LOGIC_MAX_PROGRAMS) {
    return api_send_error(req, 400, "Invalid logic program ID");
  }

  st_logic_engine_state_t *state = st_logic_get_state();
  if (!state) {
    return api_send_error(req, 500, "ST Logic not initialized");
  }

  bool success = st_logic_delete(state, id - 1);

  JsonDocument doc;
  doc["status"] = success ? "ok" : "error";
  doc["program"] = id;
  doc["message"] = success ? "Program deleted" : "Failed to delete program";

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

esp_err_t api_handler_logic_stats(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  int id = api_extract_id_from_uri(req, "/api/logic/");
  if (id < 1 || id > ST_LOGIC_MAX_PROGRAMS) {
    return api_send_error(req, 400, "Invalid logic program ID");
  }

  st_logic_engine_state_t *state = st_logic_get_state();
  if (!state) {
    return api_send_error(req, 500, "ST Logic not initialized");
  }

  st_logic_program_config_t *prog = &state->programs[id - 1];

  JsonDocument doc;
  doc["program"] = id;
  doc["name"] = prog->name;
  doc["enabled"] = prog->enabled ? true : false;
  doc["compiled"] = prog->compiled ? true : false;
  doc["execution_count"] = prog->execution_count;
  doc["error_count"] = prog->error_count;
  doc["last_execution_us"] = prog->last_execution_us;
  doc["min_execution_us"] = prog->min_execution_us;
  doc["max_execution_us"] = prog->max_execution_us;
  doc["overrun_count"] = prog->overrun_count;

  // Calculate average if we have executions
  if (prog->execution_count > 0) {
    doc["avg_execution_us"] = prog->total_execution_us / prog->execution_count;
  } else {
    doc["avg_execution_us"] = 0;
  }

  char buf[HTTP_JSON_DOC_SIZE];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

/* ============================================================================
 * CONFIG & DEBUG ENDPOINTS (v6.0.4+)
 * ============================================================================ */

esp_err_t api_handler_config_get(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  // Full config can be 6-8KB JSON — allocate on heap
  const size_t BUF_SIZE = 8192;
  char *buf = (char *)malloc(BUF_SIZE);
  if (!buf) {
    return api_send_error(req, 500, "Out of memory");
  }

  JsonDocument doc;

  // ── SYSTEM ──
  JsonObject sys = doc["system"].to<JsonObject>();
  sys["version"] = PROJECT_VERSION;
  sys["build"] = BUILD_NUMBER;
  sys["hostname"] = g_persist_config.hostname;
  sys["schema_version"] = g_persist_config.schema_version;

  // ── MODBUS SLAVE ──
  JsonObject slave = doc["modbus_slave"].to<JsonObject>();
  slave["enabled"] = g_persist_config.modbus_slave.enabled ? true : false;
  slave["slave_id"] = g_persist_config.modbus_slave.slave_id;
  slave["baudrate"] = g_persist_config.modbus_slave.baudrate;
  const char *par_str = "NONE";
  if (g_persist_config.modbus_slave.parity == 1) par_str = "EVEN";
  else if (g_persist_config.modbus_slave.parity == 2) par_str = "ODD";
  slave["parity"] = par_str;
  slave["stop_bits"] = g_persist_config.modbus_slave.stop_bits;
  slave["inter_frame_delay_ms"] = g_persist_config.modbus_slave.inter_frame_delay;

  // ── MODBUS MASTER ──
  JsonObject master = doc["modbus_master"].to<JsonObject>();
  master["enabled"] = g_persist_config.modbus_master.enabled ? true : false;
  if (g_persist_config.modbus_master.enabled) {
    master["baudrate"] = g_persist_config.modbus_master.baudrate;
    const char *mpar = "NONE";
    if (g_persist_config.modbus_master.parity == 1) mpar = "EVEN";
    else if (g_persist_config.modbus_master.parity == 2) mpar = "ODD";
    master["parity"] = mpar;
    master["stop_bits"] = g_persist_config.modbus_master.stop_bits;
    master["timeout_ms"] = g_persist_config.modbus_master.timeout_ms;
    master["inter_frame_delay_ms"] = g_persist_config.modbus_master.inter_frame_delay;
    master["max_requests_per_cycle"] = g_persist_config.modbus_master.max_requests_per_cycle;
  }

  // ── NETWORK ──
  JsonObject network = doc["network"].to<JsonObject>();
  network["enabled"] = g_persist_config.network.enabled ? true : false;
  network["ssid"] = g_persist_config.network.ssid;
  network["dhcp"] = g_persist_config.network.dhcp_enabled ? true : false;
  network["power_save"] = g_persist_config.network.wifi_power_save ? true : false;
  if (!g_persist_config.network.dhcp_enabled) {
    char ip_buf[16];
    struct in_addr addr;
    addr.s_addr = g_persist_config.network.static_ip;
    strncpy(ip_buf, inet_ntoa(addr), 15); ip_buf[15] = '\0';
    network["static_ip"] = ip_buf;
    addr.s_addr = g_persist_config.network.static_gateway;
    strncpy(ip_buf, inet_ntoa(addr), 15); ip_buf[15] = '\0';
    network["static_gateway"] = ip_buf;
    addr.s_addr = g_persist_config.network.static_netmask;
    strncpy(ip_buf, inet_ntoa(addr), 15); ip_buf[15] = '\0';
    network["static_netmask"] = ip_buf;
    addr.s_addr = g_persist_config.network.static_dns;
    strncpy(ip_buf, inet_ntoa(addr), 15); ip_buf[15] = '\0';
    network["static_dns"] = ip_buf;
  }

  // ── TELNET ──
  JsonObject telnet = doc["telnet"].to<JsonObject>();
  telnet["enabled"] = g_persist_config.network.telnet_enabled ? true : false;
  telnet["port"] = g_persist_config.network.telnet_port;

  // ── HTTP API ──
  JsonObject http = doc["http"].to<JsonObject>();
  http["enabled"] = g_persist_config.network.http.enabled ? true : false;
  http["port"] = g_persist_config.network.http.port;
  http["tls_enabled"] = g_persist_config.network.http.tls_enabled ? true : false;
  http["api_enabled"] = g_persist_config.network.http.api_enabled ? true : false;
  http["auth_enabled"] = g_persist_config.network.http.auth_enabled ? true : false;
  const char *prio_str = "NORMAL";
  if (g_persist_config.network.http.priority == 0) prio_str = "LOW";
  else if (g_persist_config.network.http.priority == 2) prio_str = "HIGH";
  http["priority"] = prio_str;

  // ── COUNTERS ──
  JsonArray counters = doc["counters"].to<JsonArray>();
  for (int i = 0; i < COUNTER_COUNT; i++) {
    const CounterConfig *c = &g_persist_config.counters[i];
    if (!c->enabled) continue;
    JsonObject co = counters.add<JsonObject>();
    co["id"] = i + 1;
    const char *hw = "SW";
    if (c->hw_mode == COUNTER_HW_SW_ISR) hw = "SW_ISR";
    else if (c->hw_mode == COUNTER_HW_PCNT) hw = "HW_PCNT";
    co["hw_mode"] = hw;
    const char *edge = "rising";
    if (c->edge_type == COUNTER_EDGE_FALLING) edge = "falling";
    else if (c->edge_type == COUNTER_EDGE_BOTH) edge = "both";
    co["edge"] = edge;
    co["direction"] = (c->direction == COUNTER_DIR_DOWN) ? "down" : "up";
    co["prescaler"] = c->prescaler;
    co["bit_width"] = c->bit_width;
    co["scale_factor"] = c->scale_factor;
    co["input_dis"] = c->input_dis;
    co["value_reg"] = c->value_reg;
    if (c->raw_reg != 0xFFFF) co["raw_reg"] = c->raw_reg;
    if (c->freq_reg != 0xFFFF) co["freq_reg"] = c->freq_reg;
    if (c->ctrl_reg != 0xFFFF) co["ctrl_reg"] = c->ctrl_reg;
    co["start_value"] = c->start_value;
    if (c->hw_gpio > 0) co["hw_gpio"] = c->hw_gpio;
    if (c->interrupt_pin > 0) co["interrupt_pin"] = c->interrupt_pin;
    co["debounce"] = (c->debounce_enabled && c->debounce_ms > 0) ? true : false;
    if (c->debounce_enabled && c->debounce_ms > 0) co["debounce_ms"] = c->debounce_ms;
  }

  // ── TIMERS ──
  JsonArray timers = doc["timers"].to<JsonArray>();
  for (int i = 0; i < TIMER_COUNT; i++) {
    const TimerConfig *t = &g_persist_config.timers[i];
    if (!t->enabled) continue;
    JsonObject ti = timers.add<JsonObject>();
    ti["id"] = i + 1;
    const char *tmode = "DISABLED";
    switch (t->mode) {
      case TIMER_MODE_1_ONESHOT:         tmode = "ONESHOT"; break;
      case TIMER_MODE_2_MONOSTABLE:      tmode = "MONOSTABLE"; break;
      case TIMER_MODE_3_ASTABLE:         tmode = "ASTABLE"; break;
      case TIMER_MODE_4_INPUT_TRIGGERED: tmode = "INPUT_TRIGGERED"; break;
      default: break;
    }
    ti["mode"] = tmode;
    ti["output_coil"] = t->output_coil;
    if (t->ctrl_reg != 0xFFFF) ti["ctrl_reg"] = t->ctrl_reg;
    switch (t->mode) {
      case TIMER_MODE_1_ONESHOT:
        ti["phase1_ms"] = t->phase1_duration_ms;
        ti["phase2_ms"] = t->phase2_duration_ms;
        ti["phase3_ms"] = t->phase3_duration_ms;
        break;
      case TIMER_MODE_2_MONOSTABLE:
        ti["pulse_ms"] = t->pulse_duration_ms;
        break;
      case TIMER_MODE_3_ASTABLE:
        ti["on_ms"] = t->on_duration_ms;
        ti["off_ms"] = t->off_duration_ms;
        break;
      case TIMER_MODE_4_INPUT_TRIGGERED:
        ti["input_dis"] = t->input_dis;
        ti["delay_ms"] = t->delay_ms;
        break;
      default: break;
    }
  }

  // ── GPIO MAPPINGS ──
  JsonArray gpios = doc["gpio"].to<JsonArray>();
  for (int i = 0; i < g_persist_config.var_map_count; i++) {
    const VariableMapping *m = &g_persist_config.var_maps[i];
    if (m->source_type != MAPPING_SOURCE_GPIO) continue;
    JsonObject g = gpios.add<JsonObject>();
    g["pin"] = m->gpio_pin;
    g["direction"] = m->is_input ? "input" : "output";
    if (m->is_input) {
      g["register"] = m->input_reg;
    } else {
      g["coil"] = m->coil_reg;
    }
  }

  // ── ST LOGIC ──
  JsonObject logic = doc["st_logic"].to<JsonObject>();
  logic["interval_ms"] = g_persist_config.st_logic_interval_ms;
  st_logic_engine_state_t *st_state = st_logic_get_state();
  if (st_state) {
    logic["enabled"] = st_state->enabled ? true : false;
    JsonArray progs = logic["programs"].to<JsonArray>();
    for (int i = 0; i < ST_LOGIC_MAX_PROGRAMS; i++) {
      st_logic_program_config_t *p = &st_state->programs[i];
      if (p->source_size == 0 && !p->compiled) continue;
      JsonObject pr = progs.add<JsonObject>();
      pr["id"] = i + 1;
      pr["name"] = p->name;
      pr["enabled"] = p->enabled ? true : false;
      pr["compiled"] = p->compiled ? true : false;
      pr["source_size"] = p->source_size;
      pr["bindings"] = p->binding_count;
    }
  }

  // ── MODULES ──
  JsonObject modules = doc["modules"].to<JsonObject>();
  modules["counters"] = (g_persist_config.module_flags & MODULE_FLAG_COUNTERS_DISABLED) ? false : true;
  modules["timers"] = (g_persist_config.module_flags & MODULE_FLAG_TIMERS_DISABLED) ? false : true;
  modules["st_logic"] = (g_persist_config.module_flags & MODULE_FLAG_ST_LOGIC_DISABLED) ? false : true;

  // ── PERSISTENCE ──
  JsonObject persist = doc["persistence"].to<JsonObject>();
  persist["enabled"] = g_persist_config.persist_regs.enabled ? true : false;
  uint8_t grp_count = g_persist_config.persist_regs.group_count;
  if (grp_count > PERSIST_MAX_GROUPS) grp_count = PERSIST_MAX_GROUPS;
  persist["group_count"] = grp_count;

  size_t json_len = serializeJson(doc, buf, BUF_SIZE);
  if (json_len >= BUF_SIZE) {
    free(buf);
    return api_send_error(req, 500, "Response too large");
  }

  esp_err_t ret = api_send_json(req, buf);
  free(buf);
  return ret;
}

esp_err_t api_handler_debug_get(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  DebugFlags *dbg = debug_flags_get();

  JsonDocument doc;
  doc["all"] = dbg->all ? true : false;
  doc["config_save"] = dbg->config_save ? true : false;
  doc["config_load"] = dbg->config_load ? true : false;
  doc["wifi_connect"] = dbg->wifi_connect ? true : false;
  doc["network_validate"] = dbg->network_validate ? true : false;
  doc["http_server"] = dbg->http_server ? true : false;
  doc["http_api"] = dbg->http_api ? true : false;

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));

  return api_send_json(req, buf);
}

esp_err_t api_handler_debug_set(httpd_req_t *req)
{
  http_server_stat_request();
  CHECK_AUTH(req);

  // Read request body
  char content[256];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    return api_send_error(req, 400, "Failed to read request body");
  }
  content[ret] = '\0';

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, content);
  if (error) {
    return api_send_error(req, 400, "Invalid JSON");
  }

  // Apply debug flags
  if (doc.containsKey("all")) {
    debug_flags_set_all(doc["all"].as<bool>() ? 1 : 0);
  }
  if (doc.containsKey("config_save")) {
    debug_flags_set_config_save(doc["config_save"].as<bool>() ? 1 : 0);
  }
  if (doc.containsKey("config_load")) {
    debug_flags_set_config_load(doc["config_load"].as<bool>() ? 1 : 0);
  }
  if (doc.containsKey("wifi_connect")) {
    debug_flags_set_wifi_connect(doc["wifi_connect"].as<bool>() ? 1 : 0);
  }
  if (doc.containsKey("network_validate")) {
    debug_flags_set_network_validate(doc["network_validate"].as<bool>() ? 1 : 0);
  }
  if (doc.containsKey("http_server")) {
    debug_flags_set_http_server(doc["http_server"].as<bool>() ? 1 : 0);
  }
  if (doc.containsKey("http_api")) {
    debug_flags_set_http_api(doc["http_api"].as<bool>() ? 1 : 0);
  }

  // Return updated state
  DebugFlags *dbg = debug_flags_get();

  JsonDocument resp;
  resp["status"] = "ok";
  resp["all"] = dbg->all ? true : false;
  resp["config_save"] = dbg->config_save ? true : false;
  resp["config_load"] = dbg->config_load ? true : false;
  resp["wifi_connect"] = dbg->wifi_connect ? true : false;
  resp["network_validate"] = dbg->network_validate ? true : false;
  resp["http_server"] = dbg->http_server ? true : false;
  resp["http_api"] = dbg->http_api ? true : false;

  char buf[256];
  serializeJson(resp, buf, sizeof(buf));

  return api_send_json(req, buf);
}
