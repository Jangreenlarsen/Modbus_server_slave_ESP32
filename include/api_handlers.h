/**
 * @file api_handlers.h
 * @brief HTTP REST API endpoint handlers (v6.0.0+)
 *
 * JSON response builders and HTTP request handlers for REST API.
 * Uses ArduinoJson for efficient JSON serialization.
 */

#ifndef API_HANDLERS_H
#define API_HANDLERS_H

#include <esp_http_server.h>

/* ============================================================================
 * API ENDPOINT HANDLERS
 *
 * All handlers follow ESP-IDF esp_http_server pattern:
 *   esp_err_t handler(httpd_req_t *req)
 *
 * Return ESP_OK on success, ESP_FAIL on error.
 * ============================================================================ */

/**
 * GET /api/status
 * System status (version, uptime, heap, wifi, modbus_slave_id)
 */
esp_err_t api_handler_status(httpd_req_t *req);

/**
 * GET /api/counters
 * All 4 counters summary
 */
esp_err_t api_handler_counters(httpd_req_t *req);

/**
 * GET /api/counters/{id}
 * Single counter details (1-4)
 */
esp_err_t api_handler_counter_single(httpd_req_t *req);

/**
 * GET /api/timers
 * All 4 timers summary
 */
esp_err_t api_handler_timers(httpd_req_t *req);

/**
 * GET /api/timers/{id}
 * Single timer details (1-4)
 */
esp_err_t api_handler_timer_single(httpd_req_t *req);

/**
 * GET /api/registers/hr/{addr}
 * Read holding register
 */
esp_err_t api_handler_hr_read(httpd_req_t *req);

/**
 * POST /api/registers/hr/{addr}
 * Write holding register
 * Request body: {"value": 12345}
 */
esp_err_t api_handler_hr_write(httpd_req_t *req);

/**
 * GET /api/registers/ir/{addr}
 * Read input register
 */
esp_err_t api_handler_ir_read(httpd_req_t *req);

/**
 * GET /api/registers/coils/{addr}
 * Read coil
 */
esp_err_t api_handler_coil_read(httpd_req_t *req);

/**
 * POST /api/registers/coils/{addr}
 * Write coil
 * Request body: {"value": true} or {"value": 1}
 */
esp_err_t api_handler_coil_write(httpd_req_t *req);

/**
 * GET /api/registers/di/{addr}
 * Read discrete input
 */
esp_err_t api_handler_di_read(httpd_req_t *req);

/**
 * GET /api/logic
 * All ST Logic programs status
 */
esp_err_t api_handler_logic(httpd_req_t *req);

/**
 * GET /api/logic/{id}
 * Single ST Logic program details with variables (1-4)
 */
esp_err_t api_handler_logic_single(httpd_req_t *req);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * Extract ID from wildcard URI (e.g., "/api/counters/2" → 2)
 * @param req HTTP request
 * @param prefix URI prefix (e.g., "/api/counters/")
 * @return ID as integer, or -1 on error
 */
int api_extract_id_from_uri(httpd_req_t *req, const char *prefix);

/**
 * Send JSON error response
 * @param req HTTP request
 * @param status HTTP status code (e.g., 400, 404, 500)
 * @param error_msg Error message
 */
esp_err_t api_send_error(httpd_req_t *req, int status, const char *error_msg);

/**
 * Send JSON success response
 * @param req HTTP request
 * @param json_str JSON string to send
 */
esp_err_t api_send_json(httpd_req_t *req, const char *json_str);

#endif // API_HANDLERS_H
