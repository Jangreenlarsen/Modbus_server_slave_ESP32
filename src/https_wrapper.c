/**
 * @file https_wrapper.c
 * @brief Custom HTTPS/TLS wrapper with heap-based connection limiting (FEAT-016)
 *
 * Uses httpd_start() + esp_tls directly (instead of esp_https_server) to get
 * full control over the open_fn callback. This allows checking free heap
 * BEFORE starting a TLS handshake, gracefully denying connections when memory
 * is insufficient instead of flooding mbedTLS allocation errors.
 *
 * IMPORTANT: This file MUST be compiled as C (not C++) because esp_tls.h
 * triggers a lwIP header conflict in C++ mode on Arduino-ESP32 v2.x.
 */

#include "https_wrapper.h"
#include <string.h>
#include <stdio.h>
#include <esp_tls.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include "debug_flags.h"
#include "debug.h"

static const char *TAG = "HTTPS_WRAP";

/* Minimum heap thresholds for accepting a new TLS connection.
 * ECDSA P-256 handshake peak usage: ~50KB (2x 16KB I/O buffers + SSL context + MPI workspace).
 * Arduino-ESP32 pre-compiles mbedTLS with 16KB I/O buffers (CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN=16384).
 * We cannot reduce these without rebuilding the framework.
 * With max_open_sockets=1, httpd uses less pre-allocated heap. */
#define HTTPS_MIN_FREE_HEAP     55000
#define HTTPS_MIN_CONTIG_BLOCK  30000

/* Embedded TLS certificates (generated at build time via board_build.embed_txtfiles)
 * Symbol names include path prefix: certs/ -> certs_ */
extern const uint8_t servercert_pem_start[] asm("_binary_certs_servercert_pem_start");
extern const uint8_t servercert_pem_end[]   asm("_binary_certs_servercert_pem_end");
extern const uint8_t prvtkey_pem_start[]    asm("_binary_certs_prvtkey_pem_start");
extern const uint8_t prvtkey_pem_end[]      asm("_binary_certs_prvtkey_pem_end");

/* Server TLS configuration (initialized once at start) */
static esp_tls_cfg_server_t s_tls_cfg;

/* ============================================================================
 * TLS TRANSPORT OVERRIDES
 * ============================================================================ */

static int https_send(httpd_handle_t hd, int sockfd,
                      const char *buf, size_t buf_len, int flags)
{
  (void)flags;
  esp_tls_t *tls = (esp_tls_t *)httpd_sess_get_transport_ctx(hd, sockfd);
  if (!tls) return -1;
  return (int)esp_tls_conn_write(tls, buf, buf_len);
}

static int https_recv(httpd_handle_t hd, int sockfd,
                      char *buf, size_t buf_len, int flags)
{
  (void)flags;
  esp_tls_t *tls = (esp_tls_t *)httpd_sess_get_transport_ctx(hd, sockfd);
  if (!tls) return -1;
  return (int)esp_tls_conn_read(tls, (void *)buf, buf_len);
}

static int https_pending(httpd_handle_t hd, int sockfd)
{
  esp_tls_t *tls = (esp_tls_t *)httpd_sess_get_transport_ctx(hd, sockfd);
  if (!tls) return 0;
  return (int)esp_tls_get_bytes_avail(tls);
}

/* ============================================================================
 * CONNECTION LIFECYCLE
 * ============================================================================ */

/**
 * @brief Open callback - checks heap BEFORE TLS handshake
 *
 * Called by httpd when a new TCP connection is accepted.
 * If free heap is insufficient, returns ESP_FAIL to deny the connection
 * immediately (no TLS allocation attempted, no error flooding).
 */
static esp_err_t https_open_fn(httpd_handle_t hd, int sockfd)
{
  size_t free_heap = esp_get_free_heap_size();
  size_t largest   = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  DebugFlags *dbg = debug_flags_get();

  if (dbg->http_server) {
    debug_printf("[HTTPS] TLS open fd=%d heap=%u largest=%u\n", sockfd, (unsigned)free_heap, (unsigned)largest);
  }

  if (free_heap < HTTPS_MIN_FREE_HEAP || largest < HTTPS_MIN_CONTIG_BLOCK) {
    ESP_LOGW(TAG, "TLS denied: heap=%u largest=%u (need %uKB)",
             free_heap, largest, HTTPS_MIN_FREE_HEAP / 1024);
    if (dbg->http_server) {
      debug_printf("[HTTPS] TLS DENIED - insufficient heap\n");
    }
    return ESP_FAIL;
  }

  esp_tls_t *tls = esp_tls_init();
  if (!tls) {
    ESP_LOGE(TAG, "esp_tls_init() failed");
    if (dbg->http_server) {
      debug_printf("[HTTPS] esp_tls_init() FAILED\n");
    }
    return ESP_FAIL;
  }

  if (dbg->http_server) {
    debug_printf("[HTTPS] Starting handshake (heap=%u before)\n", (unsigned)esp_get_free_heap_size());
  }

  int ret = esp_tls_server_session_create(&s_tls_cfg, sockfd, tls);
  if (ret != 0) {
    ESP_LOGW(TAG, "TLS handshake failed (err=%d, heap=%u)", ret, esp_get_free_heap_size());
    if (dbg->http_server) {
      debug_printf("[HTTPS] TLS handshake FAILED err=%d heap=%u\n", ret, (unsigned)esp_get_free_heap_size());
    }
    /* Prevent esp_tls_server_session_delete from closing the socket -
     * httpd owns the socket and will close it when we return ESP_FAIL */
    tls->sockfd = -1;
    esp_tls_server_session_delete(tls);
    return ESP_FAIL;
  }

  /* Store TLS context and override transport functions */
  httpd_sess_set_transport_ctx(hd, sockfd, (void *)tls, NULL);
  httpd_sess_set_send_override(hd, sockfd, https_send);
  httpd_sess_set_recv_override(hd, sockfd, https_recv);
  httpd_sess_set_pending_override(hd, sockfd, https_pending);

  if (dbg->http_server) {
    debug_printf("[HTTPS] TLS handshake OK fd=%d heap=%u\n", sockfd, (unsigned)esp_get_free_heap_size());
  }
  return ESP_OK;
}

/**
 * @brief Close callback - tears down TLS session
 *
 * Called by httpd when a session is terminated (timeout, LRU purge, client close).
 * IMPORTANT: httpd closes the socket fd AFTER calling close_fn, so we must
 * prevent esp_tls_server_session_delete() from also closing it (double-close
 * causes heap corruption when the fd is reused by another TLS session).
 */
static void https_close_fn(httpd_handle_t hd, int sockfd)
{
  esp_tls_t *tls = (esp_tls_t *)httpd_sess_get_transport_ctx(hd, sockfd);
  if (tls) {
    httpd_sess_set_transport_ctx(hd, sockfd, NULL, NULL);
    /* Prevent esp_tls from closing the socket - httpd owns it */
    tls->sockfd = -1;
    esp_tls_server_session_delete(tls);
    DebugFlags *dbg = debug_flags_get();
    if (dbg->http_server) {
      debug_printf("[HTTPS] TLS session closed fd=%d heap=%u\n", sockfd, (unsigned)esp_get_free_heap_size());
    }
  } else {
    /* No TLS context - httpd will close sockfd after this callback returns.
     * Do NOT call close(sockfd) here - that would cause double-close heap corruption. */
  }
}

/* ============================================================================
 * PUBLIC API
 * ============================================================================ */

int https_wrapper_start(httpd_handle_t *handle,
                        uint16_t port,
                        uint16_t max_uri,
                        uint32_t stack_size,
                        uint8_t priority,
                        int core_id)
{
  if (!handle) {
    ESP_LOGE(TAG, "handle is NULL");
    return -1;
  }

  ESP_LOGI(TAG, "Free heap: %u bytes (largest block: %u)",
           esp_get_free_heap_size(),
           heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  /* Initialize TLS server config (cert + key) */
  memset(&s_tls_cfg, 0, sizeof(s_tls_cfg));
  s_tls_cfg.servercert_buf  = servercert_pem_start;
  s_tls_cfg.servercert_bytes = (unsigned int)(servercert_pem_end - servercert_pem_start);
  s_tls_cfg.serverkey_buf   = prvtkey_pem_start;
  s_tls_cfg.serverkey_bytes = (unsigned int)(prvtkey_pem_end - prvtkey_pem_start);

  /* Use plain httpd with custom TLS open/close callbacks.
   * max_open_sockets = 4 (default), but the heap check in open_fn
   * will deny connections when memory is insufficient. */
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port      = port;
  config.max_uri_handlers = max_uri;
  config.stack_size       = stack_size;
  config.max_open_sockets = 1;
  config.backlog_conn     = 1;
  config.uri_match_fn     = httpd_uri_match_wildcard;
  config.lru_purge_enable = true;
  config.recv_wait_timeout  = 2;   // 2 sec (API calls complete in <100ms)
  config.send_wait_timeout  = 2;
  config.core_id          = core_id;
  config.task_priority    = priority;
  config.open_fn          = https_open_fn;
  config.close_fn         = https_close_fn;

  esp_err_t err = httpd_start(handle, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "httpd_start failed: %d (0x%x)", err, err);
    return -1;
  }

  ESP_LOGI(TAG, "HTTPS server on port %d (custom TLS, heap-limited)", port);
  return 0;
}

void https_wrapper_stop(httpd_handle_t handle)
{
  if (handle) {
    httpd_stop(handle);
    ESP_LOGI(TAG, "HTTPS server stopped");
  }
}

int https_wrapper_get_cert_info(char *buf, size_t buflen)
{
  if (!buf || buflen == 0) return -1;

  mbedtls_x509_crt crt;
  mbedtls_x509_crt_init(&crt);

  size_t cert_len = (size_t)(servercert_pem_end - servercert_pem_start);
  int ret = mbedtls_x509_crt_parse(&crt, servercert_pem_start, cert_len);
  if (ret != 0) {
    mbedtls_x509_crt_free(&crt);
    snprintf(buf, buflen, "parse error (%d)", ret);
    return -1;
  }

  mbedtls_pk_type_t pk_type = mbedtls_pk_get_type(&crt.pk);
  size_t key_bits = mbedtls_pk_get_bitlen(&crt.pk);

  if (pk_type == MBEDTLS_PK_ECKEY || pk_type == MBEDTLS_PK_ECDSA) {
    const char *curve = "unknown";
    mbedtls_ecp_keypair *ec = mbedtls_pk_ec(crt.pk);
    if (ec) {
      mbedtls_ecp_group_id gid = ec->grp.id;
      if (gid == MBEDTLS_ECP_DP_SECP256R1)      curve = "P-256";
      else if (gid == MBEDTLS_ECP_DP_SECP384R1)  curve = "P-384";
      else if (gid == MBEDTLS_ECP_DP_SECP521R1)  curve = "P-521";
    }
    snprintf(buf, buflen, "ECDSA %s (%u-bit)", curve, (unsigned)key_bits);
  } else if (pk_type == MBEDTLS_PK_RSA) {
    snprintf(buf, buflen, "RSA (%u-bit)", (unsigned)key_bits);
  } else {
    snprintf(buf, buflen, "%s (%u-bit)", mbedtls_pk_get_name(&crt.pk), (unsigned)key_bits);
  }

  mbedtls_x509_crt_free(&crt);
  return 0;
}
