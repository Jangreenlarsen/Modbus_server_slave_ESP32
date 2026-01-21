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
#include "timer_engine.h"
#include "st_logic_config.h"
#include "wifi_driver.h"
#include "build_version.h"

static const char *TAG = "API_HDLR";

// External functions from http_server.cpp for stats
extern void http_server_stat_request(void);
extern void http_server_stat_success(void);
extern void http_server_stat_client_error(void);
extern void http_server_stat_server_error(void);
extern void http_server_stat_auth_failure(void);
extern bool http_server_check_auth(httpd_req_t *req);

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
  char buf[256];
  snprintf(buf, sizeof(buf), "{\"error\":\"%s\",\"status\":%d}", error_msg, status);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_status(req, status == 404 ? "404 Not Found" :
                             status == 400 ? "400 Bad Request" :
                             status == 401 ? "401 Unauthorized" :
                             status == 500 ? "500 Internal Server Error" : "400 Bad Request");
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
  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, json_str);
  http_server_stat_success();
  return ESP_OK;
}

/* ============================================================================
 * AUTHENTICATION CHECK MACRO
 * ============================================================================ */

#define CHECK_AUTH(req) \
  do { \
    if (!http_server_check_auth(req)) { \
      httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Modbus ESP32\""); \
      return api_send_error(req, 401, "Authentication required"); \
    } \
  } while(0)

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

  for (int i = 0; i < 4; i++) {
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
  http_server_stat_request();
  CHECK_AUTH(req);

  int id = api_extract_id_from_uri(req, "/api/logic/");
  if (id < 1 || id > 4) {
    return api_send_error(req, 400, "Invalid logic program ID (must be 1-4)");
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
