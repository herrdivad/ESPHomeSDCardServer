#pragma once
#include <cstddef>
#include <cstdint>
#include <sys/types.h>
#include "esp_err.h"

using httpd_handle_t = void *;
enum httpd_method_t { HTTP_GET, HTTP_POST, HTTP_DELETE, HTTP_OPTIONS };
struct httpd_req {
  void *user_ctx{nullptr};
  int content_len{0};
};
using httpd_req_t = httpd_req;
using httpd_uri_match_func_t = bool (*)(const char *, const char *, size_t);
struct httpd_config_t {
  uint16_t server_port{80};
  uint16_t ctrl_port{32768};
  size_t max_uri_handlers{8};
  size_t stack_size{4096};
  size_t max_open_sockets{7};
  bool lru_purge_enable{false};
  httpd_uri_match_func_t uri_match_fn{nullptr};
};
struct httpd_uri_t {
  const char *uri{nullptr};
  httpd_method_t method{HTTP_GET};
  esp_err_t (*handler)(httpd_req_t *){nullptr};
  void *user_ctx{nullptr};
};
inline httpd_config_t HTTPD_DEFAULT_CONFIG() { return {}; }
inline bool httpd_uri_match_wildcard(const char *, const char *, size_t) { return true; }
inline esp_err_t httpd_start(httpd_handle_t *, const httpd_config_t *) { return ESP_OK; }
inline esp_err_t httpd_stop(httpd_handle_t) { return ESP_OK; }
inline esp_err_t httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t *) { return ESP_OK; }
inline size_t httpd_req_get_hdr_value_len(httpd_req_t *, const char *) { return 1; }
inline esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *, const char *, char *, size_t) { return ESP_OK; }
inline size_t httpd_req_get_url_query_len(httpd_req_t *) { return 1; }
inline esp_err_t httpd_req_get_url_query_str(httpd_req_t *, char *, size_t) { return ESP_OK; }
inline esp_err_t httpd_query_key_value(const char *, const char *, char *, size_t) { return ESP_OK; }
inline esp_err_t httpd_resp_set_status(httpd_req_t *, const char *) { return ESP_OK; }
inline esp_err_t httpd_resp_set_type(httpd_req_t *, const char *) { return ESP_OK; }
inline esp_err_t httpd_resp_set_hdr(httpd_req_t *, const char *, const char *) { return ESP_OK; }
inline esp_err_t httpd_resp_send(httpd_req_t *, const char *, ssize_t) { return ESP_OK; }
inline esp_err_t httpd_resp_send_chunk(httpd_req_t *, const char *, ssize_t) { return ESP_OK; }
inline int httpd_send(httpd_req_t *, const char *, size_t length) { return static_cast<int>(length); }
inline int httpd_req_recv(httpd_req_t *, char *, size_t) { return 0; }
inline esp_err_t httpd_req_async_handler_begin(httpd_req_t *request, httpd_req_t **output) {
  *output = request;
  return ESP_OK;
}
inline esp_err_t httpd_req_async_handler_complete(httpd_req_t *) { return ESP_OK; }
static constexpr ssize_t HTTPD_RESP_USE_STRLEN = -1;
static constexpr int HTTPD_SOCK_ERR_TIMEOUT = -3;
