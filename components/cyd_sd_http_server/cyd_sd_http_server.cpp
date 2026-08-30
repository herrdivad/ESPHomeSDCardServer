#include "cyd_sd_http_server.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <inttypes.h>
#include <new>
#include <vector>

#include "esp_timer.h"
#include "esphome/core/log.h"
#include "esphome/core/millis_internal.h"
#include "path_utils.h"
#include "web_ui.h"

#ifdef USE_CYD_SD_LVGL
#include "misc/cache/instance/lv_image_cache.h"
#endif

namespace esphome {
namespace cyd_sd_http_server {

static const char *const TAG = "cyd_sd_http";
static const char *const SERVER_VERSION = "0.2.0";
static constexpr size_t STREAM_BUFFER_SIZE = 8192;
static constexpr size_t MAX_QUERY_LENGTH = 1024;
static constexpr size_t MAX_LIST_LIMIT = 100;

namespace {
std::string json_escape(const std::string &value) {
  std::string output;
  output.reserve(value.size() + 8);
  static const char HEX[] = "0123456789abcdef";
  for (unsigned char character : value) {
    switch (character) {
      case '"': output += "\\\""; break;
      case '\\': output += "\\\\"; break;
      case '\b': output += "\\b"; break;
      case '\f': output += "\\f"; break;
      case '\n': output += "\\n"; break;
      case '\r': output += "\\r"; break;
      case '\t': output += "\\t"; break;
      default:
        if (character < 0x20) {
          output += "\\u00";
          output.push_back(HEX[character >> 4]);
          output.push_back(HEX[character & 0x0F]);
        } else {
          output.push_back(static_cast<char>(character));
        }
    }
  }
  return output;
}

std::string url_encode(const std::string &value) {
  static const char HEX[] = "0123456789ABCDEF";
  std::string output;
  for (unsigned char character : value) {
    if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9') || character == '-' || character == '_' || character == '.') {
      output.push_back(static_cast<char>(character));
    } else {
      output.push_back('%');
      output.push_back(HEX[character >> 4]);
      output.push_back(HEX[character & 0x0F]);
    }
  }
  return output;
}

const char *http_status_for(StorageError error) {
  switch (error) {
    case StorageError::NOT_MOUNTED: return "503 Service Unavailable";
    case StorageError::INVALID_PATH: return "400 Bad Request";
    case StorageError::NOT_FOUND: return "404 Not Found";
    case StorageError::ALREADY_EXISTS:
    case StorageError::BUSY: return "409 Conflict";
    case StorageError::NOT_A_FILE:
    case StorageError::NOT_A_DIRECTORY: return "422 Unprocessable Entity";
    case StorageError::NO_SPACE: return "507 Insufficient Storage";
    default: return "500 Internal Server Error";
  }
}

bool parse_size(const std::string &value, size_t maximum, size_t *output) {
  if (value.empty())
    return false;
  char *end = nullptr;
  errno = 0;
  const unsigned long parsed = strtoul(value.c_str(), &end, 10);
  if (errno != 0 || end == value.c_str() || *end != '\0' || parsed > maximum)
    return false;
  *output = static_cast<size_t>(parsed);
  return true;
}

bool parse_boolean(const std::string &value, bool *output) {
  if (value == "true" || value == "1") {
    *output = true;
    return true;
  }
  if (value == "false" || value == "0") {
    *output = false;
    return true;
  }
  return false;
}

bool send_all(httpd_req_t *request, const char *data, size_t length, uint64_t *payload_bytes_sent = nullptr) {
  while (length > 0) {
    const int sent = httpd_send(request, data, length);
    if (sent <= 0 || static_cast<size_t>(sent) > length)
      return false;
    data += sent;
    length -= static_cast<size_t>(sent);
    if (payload_bytes_sent != nullptr)
      *payload_bytes_sent += static_cast<uint64_t>(sent);
  }
  return true;
}
}  // namespace

void CydSdHttpServer::setup() {
  this->stop_requested_ = false;
  this->storage_.configure(this->mount_point_, this->format_if_mount_failed_, this->internal_pullups_, this->bus_width_,
                           this->clk_pin_, this->cmd_pin_, this->data0_pin_, this->data1_pin_, this->data2_pin_,
                           this->data3_pin_);
  const esp_err_t mount_error = this->storage_.mount();
  if (mount_error != ESP_OK)
    this->status_set_error(LOG_STR("SD mount failed"));
#ifdef USE_CYD_SD_LVGL
  if (mount_error == ESP_OK && !this->display_images_.empty()) {
    if (!this->register_lvgl_file_system_()) {
      ESP_LOGE(TAG, "Direct LVGL SD image access could not be initialized");
    } else if (!this->select_image(this->initial_image_index_)) {
      ESP_LOGW(TAG, "Initial SD display image is unavailable; keeping the configured LVGL placeholder");
    }
  }
#endif
  if (!this->start_server_()) {
    this->mark_failed(LOG_STR("Native HTTP server failed to start"));
    return;
  }
  if (mount_error == ESP_OK)
    ESP_LOGI(TAG, "SD server ready on port %u", this->port_);
  else
    ESP_LOGW(TAG, "SD server listening on port %u in unavailable mode", this->port_);
}

void CydSdHttpServer::dump_config() {
  ESP_LOGCONFIG(TAG, "CYD SD HTTP Server:");
  ESP_LOGCONFIG(TAG, "  Version: %s", SERVER_VERSION);
  ESP_LOGCONFIG(TAG, "  URL: http://<device>:%u%s/", this->port_, this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Control port: %u", this->control_port_);
  ESP_LOGCONFIG(TAG, "  Mount point: %s", this->mount_point_.c_str());
  ESP_LOGCONFIG(TAG, "  SDMMC width: %d", this->bus_width_);
  ESP_LOGCONFIG(TAG, "  Pins: CLK=%d CMD=%d D0=%d D1=%d D2=%d D3=%d", this->clk_pin_, this->cmd_pin_,
                this->data0_pin_, this->data1_pin_, this->data2_pin_, this->data3_pin_);
  ESP_LOGCONFIG(TAG, "  Internal pull-ups: %s", YESNO(this->internal_pullups_));
  ESP_LOGCONFIG(TAG, "  Format on mount failure: %s", YESNO(this->format_if_mount_failed_));
  ESP_LOGCONFIG(TAG, "  Upload limit: %" PRIu32 " bytes", this->max_upload_size_);
  ESP_LOGCONFIG(TAG, "  Authentication: enabled");
  ESP_LOGCONFIG(TAG, "  Mounted: %s", YESNO(this->storage_.is_mounted()));
#ifdef USE_CYD_SD_LVGL
  ESP_LOGCONFIG(TAG, "  LVGL SD drive: %c:", this->lvgl_drive_letter_);
  ESP_LOGCONFIG(TAG, "  Display image mappings: %u", static_cast<unsigned>(this->display_images_.size()));
  for (const auto &image : this->display_images_)
    ESP_LOGCONFIG(TAG, "    %s -> %c:%s", image.name.c_str(), this->lvgl_drive_letter_, image.virtual_path.c_str());
#endif
}

bool CydSdHttpServer::teardown() {
  this->stop_requested_ = true;
  if (this->upload_task_handle_ != nullptr)
    return false;
  this->stop_server_();
  this->storage_.unmount();
  return true;
}

#ifdef USE_CYD_SD_LVGL
void CydSdImageSelect::control(size_t index) {
  if (!this->parent_->select_image(index))
    ESP_LOGW(TAG, "Display image selection index %u failed", static_cast<unsigned>(index));
}

bool CydSdHttpServer::register_lvgl_file_system_() {
  if (this->lvgl_fs_registered_)
    return true;
  if (lv_fs_get_drv(this->lvgl_drive_letter_) != nullptr) {
    ESP_LOGE(TAG, "LVGL drive %c: is already registered", this->lvgl_drive_letter_);
    return false;
  }

  lv_fs_drv_init(&this->lvgl_fs_driver_);
  this->lvgl_fs_driver_.letter = this->lvgl_drive_letter_;
  this->lvgl_fs_driver_.cache_size = 0;
  this->lvgl_fs_driver_.user_data = this;
  this->lvgl_fs_driver_.ready_cb = lvgl_ready_cb_;
  this->lvgl_fs_driver_.open_cb = lvgl_open_cb_;
  this->lvgl_fs_driver_.close_cb = lvgl_close_cb_;
  this->lvgl_fs_driver_.read_cb = lvgl_read_cb_;
  this->lvgl_fs_driver_.seek_cb = lvgl_seek_cb_;
  this->lvgl_fs_driver_.tell_cb = lvgl_tell_cb_;
  lv_fs_drv_register(&this->lvgl_fs_driver_);
  this->lvgl_fs_registered_ = true;
  ESP_LOGI(TAG, "Registered read-only LVGL SD drive %c:", this->lvgl_drive_letter_);
  return true;
}

bool CydSdHttpServer::is_mapped_image_path_(const std::string &virtual_path) const {
  return std::any_of(this->display_images_.begin(), this->display_images_.end(),
                     [&virtual_path](const DisplayImage &image) { return image.virtual_path == virtual_path; });
}

bool CydSdHttpServer::select_image(size_t index) {
  if (!this->lvgl_fs_registered_ || this->image_widget_ == nullptr || index >= this->display_images_.size())
    return false;

  const DisplayImage &image = this->display_images_[index];
  DownloadContext probe;
  const StorageResult open_result = this->storage_.open_download(image.virtual_path, &probe);
  if (!open_result.ok()) {
    ESP_LOGW(TAG, "Cannot select SD image %s: %s", image.virtual_path.c_str(),
             storage_error_message(open_result.error));
    return false;
  }
  const StorageResult close_result = this->storage_.close_download(&probe);
  if (!close_result.ok()) {
    ESP_LOGW(TAG, "Cannot close SD image probe %s: %s", image.virtual_path.c_str(),
             storage_error_message(close_result.error));
    return false;
  }

  if (!this->active_lvgl_source_.empty())
    lv_image_cache_drop(this->active_lvgl_source_.c_str());
  this->active_lvgl_source_ = std::string(1, this->lvgl_drive_letter_) + ":" + image.virtual_path;
  ::lv_image_set_src(this->image_widget_, this->active_lvgl_source_.c_str());
  lv_obj_invalidate(this->image_widget_);
  this->selected_image_index_ = index;
  if (this->image_select_ != nullptr)
    this->image_select_->publish_state(index);
  ESP_LOGI(TAG, "Display image selected: %s (%s)", image.name.c_str(), this->active_lvgl_source_.c_str());
  return true;
}

bool CydSdHttpServer::lvgl_ready_cb_(lv_fs_drv_t *driver) {
  auto *server = static_cast<CydSdHttpServer *>(driver->user_data);
  return server != nullptr && server->storage_.is_mounted();
}

void *CydSdHttpServer::lvgl_open_cb_(lv_fs_drv_t *driver, const char *path, lv_fs_mode_t mode) {
  auto *server = static_cast<CydSdHttpServer *>(driver->user_data);
  if (server == nullptr || path == nullptr || (mode & LV_FS_MODE_RD) == 0 || (mode & LV_FS_MODE_WR) != 0)
    return nullptr;

  std::string requested(path);
  if (requested.empty() || requested.front() != '/')
    requested.insert(requested.begin(), '/');
  std::string normalized;
  if (normalize_virtual_path(requested, false, &normalized) != PathError::NONE ||
      !server->is_mapped_image_path_(normalized))
    return nullptr;

  auto *context = new (std::nothrow) DownloadContext();
  if (context == nullptr)
    return nullptr;
  const StorageResult result = server->storage_.open_download(normalized, context);
  if (!result.ok()) {
    delete context;
    return nullptr;
  }
  return context;
}

lv_fs_res_t CydSdHttpServer::lvgl_close_cb_(lv_fs_drv_t *driver, void *file) {
  auto *server = static_cast<CydSdHttpServer *>(driver->user_data);
  auto *context = static_cast<DownloadContext *>(file);
  if (server == nullptr || context == nullptr)
    return LV_FS_RES_INV_PARAM;
  const StorageResult result = server->storage_.close_download(context);
  delete context;
  return result.ok() ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

lv_fs_res_t CydSdHttpServer::lvgl_read_cb_(lv_fs_drv_t *driver, void *file, void *buffer, uint32_t bytes_to_read,
                                           uint32_t *bytes_read) {
  auto *server = static_cast<CydSdHttpServer *>(driver->user_data);
  auto *context = static_cast<DownloadContext *>(file);
  if (server == nullptr || context == nullptr || buffer == nullptr || bytes_read == nullptr)
    return LV_FS_RES_INV_PARAM;
  size_t actual = 0;
  const StorageResult result =
      server->storage_.read_download(context, static_cast<uint8_t *>(buffer), bytes_to_read, &actual);
  *bytes_read = static_cast<uint32_t>(actual);
  return result.ok() ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

lv_fs_res_t CydSdHttpServer::lvgl_seek_cb_(lv_fs_drv_t *driver, void *file, uint32_t position,
                                           lv_fs_whence_t whence) {
  auto *server = static_cast<CydSdHttpServer *>(driver->user_data);
  auto *context = static_cast<DownloadContext *>(file);
  if (server == nullptr || context == nullptr)
    return LV_FS_RES_INV_PARAM;
  if (whence != LV_FS_SEEK_SET)
    return LV_FS_RES_NOT_IMP;
  return server->storage_.seek_download(context, position).ok() ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

lv_fs_res_t CydSdHttpServer::lvgl_tell_cb_(lv_fs_drv_t *driver, void *file, uint32_t *position) {
  auto *server = static_cast<CydSdHttpServer *>(driver->user_data);
  auto *context = static_cast<DownloadContext *>(file);
  if (server == nullptr || context == nullptr || position == nullptr)
    return LV_FS_RES_INV_PARAM;
  return server->storage_.tell_download(context, position).ok() ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}
#endif

bool CydSdHttpServer::start_server_() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = this->port_;
  config.ctrl_port = this->control_port_;
  config.max_uri_handlers = 12;
  config.max_open_sockets = 4;
  config.stack_size = 8192;
  config.lru_purge_enable = true;
  config.uri_match_fn = httpd_uri_match_wildcard;
  const esp_err_t start_error = httpd_start(&this->server_, &config);
  if (start_error != ESP_OK) {
    ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(start_error));
    this->server_ = nullptr;
    return false;
  }

  const struct Registration {
    const char *uri;
    httpd_method_t method;
    esp_err_t (*handler)(httpd_req_t *);
  } registrations[] = {
      {"/sd", HTTP_GET, redirect_handler_},
      {"/sd/", HTTP_GET, ui_handler_},
      {"/sd/api/health", HTTP_GET, health_handler_},
      {"/sd/api/list", HTTP_GET, list_handler_},
      {"/sd/api/file", HTTP_GET, download_handler_},
      {"/sd/api/file", HTTP_POST, upload_handler_},
      {"/sd/api/file", HTTP_DELETE, delete_file_handler_},
      {"/sd/api/directory", HTTP_POST, make_directory_handler_},
      {"/sd/api/directory", HTTP_DELETE, delete_directory_handler_},
      {"/sd/*", HTTP_OPTIONS, options_handler_},
  };
  for (const auto &registration : registrations) {
    httpd_uri_t uri{};
    uri.uri = registration.uri;
    uri.method = registration.method;
    uri.handler = registration.handler;
    uri.user_ctx = this;
    const esp_err_t error = httpd_register_uri_handler(this->server_, &uri);
    if (error != ESP_OK) {
      ESP_LOGE(TAG, "URI registration failed uri=%s method=%d error=%s", registration.uri,
               static_cast<int>(registration.method), esp_err_to_name(error));
      this->stop_server_();
      return false;
    }
  }
  return true;
}

void CydSdHttpServer::stop_server_() {
  if (this->server_ == nullptr)
    return;
  const esp_err_t error = httpd_stop(this->server_);
  if (error != ESP_OK)
    ESP_LOGE(TAG, "httpd_stop failed: %s", esp_err_to_name(error));
  this->server_ = nullptr;
}

bool CydSdHttpServer::authenticate_(httpd_req_t *request) {
  const size_t supplied_length = httpd_req_get_hdr_value_len(request, "X-API-Key");
  std::vector<char> supplied(supplied_length + 1, '\0');
  if (supplied_length == 0 ||
      httpd_req_get_hdr_value_str(request, "X-API-Key", supplied.data(), supplied.size()) != ESP_OK) {
    this->send_request_error_(request, "401 Unauthorized", "authenticate", "", "UNAUTHORIZED",
                              "X-API-Key is missing or invalid");
    return false;
  }
  const size_t expected_length = this->api_key_.size();
  const size_t compare_length = std::max(expected_length, supplied_length);
  unsigned difference = static_cast<unsigned>(expected_length ^ supplied_length);
  for (size_t index = 0; index < compare_length; index++) {
    const unsigned char expected = index < expected_length ? this->api_key_[index] : 0;
    const unsigned char actual = index < supplied_length ? supplied[index] : 0;
    difference |= expected ^ actual;
  }
  if (difference != 0) {
    this->send_request_error_(request, "401 Unauthorized", "authenticate", "", "UNAUTHORIZED",
                              "X-API-Key is missing or invalid");
    return false;
  }
  return true;
}

bool CydSdHttpServer::query_value_(httpd_req_t *request, const char *name, std::string *value) {
  const size_t query_length = httpd_req_get_url_query_len(request);
  if (query_length == 0 || query_length > MAX_QUERY_LENGTH)
    return false;
  std::vector<char> query(query_length + 1, '\0');
  if (httpd_req_get_url_query_str(request, query.data(), query.size()) != ESP_OK)
    return false;
  std::vector<char> raw(query_length + 1, '\0');
  if (httpd_query_key_value(query.data(), name, raw.data(), raw.size()) != ESP_OK)
    return false;
  *value = raw.data();
  return true;
}

bool CydSdHttpServer::request_path_(httpd_req_t *request, bool allow_root, std::string *path, std::string *error) {
  std::string raw;
  if (!this->query_value_(request, "path", &raw)) {
    *error = "Required query parameter 'path' is missing";
    return false;
  }
  std::string decoded;
  PathError path_error = percent_decode(raw, &decoded);
  if (path_error == PathError::NONE)
    path_error = normalize_virtual_path(decoded, allow_root, path);
  if (path_error != PathError::NONE) {
    *error = std::string("Invalid path: ") + path_error_to_string(path_error);
    return false;
  }
  return true;
}

esp_err_t CydSdHttpServer::send_json_(httpd_req_t *request, const char *status, const std::string &json) {
  httpd_resp_set_status(request, status);
  httpd_resp_set_type(request, "application/json");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  return httpd_resp_send(request, json.data(), json.size());
}

esp_err_t CydSdHttpServer::send_request_error_(httpd_req_t *request, const char *status, const char *operation,
                                               const std::string &path, const char *error,
                                               const std::string &message) {
  ESP_LOGW(TAG, "request rejected operation=%s path=%s error=%s", operation, path.c_str(), error);
  const std::string json = "{\"ok\":false,\"operation\":\"" + json_escape(operation) + "\",\"path\":\"" +
                           json_escape(path) + "\",\"error\":\"" + error + "\",\"message\":\"" +
                           json_escape(message) + "\"}";
  return this->send_json_(request, status, json);
}

esp_err_t CydSdHttpServer::send_error_(httpd_req_t *request, const char *operation, const std::string &path,
                                       const StorageResult &result, uint64_t expected_bytes) {
  ESP_LOGW(TAG, "storage error operation=%s path=%s error=%s errno=%d processed=%u", operation, path.c_str(),
           storage_error_to_string(result.error), result.system_errno, static_cast<unsigned>(result.bytes_processed));
  std::string json = "{\"ok\":false,\"operation\":\"" + json_escape(operation) + "\",\"path\":\"" +
                     json_escape(path) + "\",\"error\":\"" + storage_error_to_string(result.error) +
                     "\",\"message\":\"" + json_escape(storage_error_message(result.error)) + "\",\"errno\":" +
                     std::to_string(result.system_errno) + ",\"processed_bytes\":" +
                     std::to_string(result.bytes_processed);
  if (expected_bytes != 0 || strcmp(operation, "upload") == 0)
    json += ",\"expected_bytes\":" + std::to_string(expected_bytes);
  json += "}";
  return this->send_json_(request, http_status_for(result.error), json);
}

esp_err_t CydSdHttpServer::handle_redirect_(httpd_req_t *request) {
  httpd_resp_set_status(request, "307 Temporary Redirect");
  httpd_resp_set_hdr(request, "Location", "/sd/");
  return httpd_resp_send(request, nullptr, 0);
}

esp_err_t CydSdHttpServer::handle_ui_(httpd_req_t *request) {
  httpd_resp_set_type(request, "text/html; charset=utf-8");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
  httpd_resp_set_hdr(request, "Content-Security-Policy",
                     "default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; object-src 'none'");
  return httpd_resp_send(request, WEB_UI, HTTPD_RESP_USE_STRLEN);
}

esp_err_t CydSdHttpServer::handle_health_(httpd_req_t *request) {
  if (!this->authenticate_(request))
    return ESP_OK;
  StorageInfo info;
  const StorageResult result = this->storage_.get_info(&info);
  if (!result.ok() && result.error != StorageError::NOT_MOUNTED)
    return this->send_error_(request, "health", "/", result);
  const uint64_t uptime_ms = static_cast<uint64_t>(esp_timer_get_time()) / 1000ULL;
  const std::string json = "{\"ok\":true,\"operation\":\"health\",\"mounted\":" +
                           std::string(info.mounted ? "true" : "false") + ",\"card_type\":\"" +
                           json_escape(info.card_type) + "\",\"total_bytes\":" + std::to_string(info.total_bytes) +
                           ",\"used_bytes\":" + std::to_string(info.used_bytes) + ",\"free_bytes\":" +
                           std::to_string(info.free_bytes) + ",\"server_version\":\"" + SERVER_VERSION +
                           "\",\"upload_active\":" + (this->storage_.upload_active() ? "true" : "false") +
                           ",\"uptime_ms\":" + std::to_string(uptime_ms) + "}";
  return this->send_json_(request, "200 OK", json);
}

esp_err_t CydSdHttpServer::handle_list_(httpd_req_t *request) {
  if (!this->authenticate_(request))
    return ESP_OK;
  std::string path, path_error;
  if (!this->request_path_(request, true, &path, &path_error))
    return this->send_request_error_(request, "400 Bad Request", "list", "", "INVALID_PATH", path_error);
  size_t offset = 0;
  size_t limit = MAX_LIST_LIMIT;
  std::string raw;
  if (this->query_value_(request, "offset", &raw) && !parse_size(raw, SIZE_MAX, &offset))
    return this->send_request_error_(request, "400 Bad Request", "list", path, "INVALID_OFFSET", "offset is invalid");
  if (this->query_value_(request, "limit", &raw) && (!parse_size(raw, MAX_LIST_LIMIT, &limit) || limit == 0))
    return this->send_request_error_(request, "400 Bad Request", "list", path, "INVALID_LIMIT", "limit must be 1..100");
  std::vector<DirectoryEntry> entries;
  bool has_more = false;
  const StorageResult result = this->storage_.list(path, offset, limit, &entries, &has_more);
  if (!result.ok())
    return this->send_error_(request, "list", path, result);
  std::string json = "{\"ok\":true,\"operation\":\"list\",\"path\":\"" + json_escape(path) +
                     "\",\"offset\":" + std::to_string(offset) + ",\"limit\":" + std::to_string(limit) +
                     ",\"has_more\":" + (has_more ? "true" : "false") + ",\"entries\":[";
  for (size_t index = 0; index < entries.size(); index++) {
    const auto &entry = entries[index];
    if (index != 0)
      json.push_back(',');
    json += "{\"name\":\"" + json_escape(entry.name) + "\",\"path\":\"" + json_escape(entry.path) +
            "\",\"type\":\"" + (entry.is_directory ? "directory" : "file") + "\",\"size\":" +
            std::to_string(entry.size) + ",\"modified_time\":" + std::to_string(entry.modified_time) + "}";
  }
  json += "]}";
  return this->send_json_(request, "200 OK", json);
}

esp_err_t CydSdHttpServer::handle_download_(httpd_req_t *request) {
  if (!this->authenticate_(request))
    return ESP_OK;
  std::string path, path_error;
  if (!this->request_path_(request, false, &path, &path_error))
    return this->send_request_error_(request, "400 Bad Request", "download", "", "INVALID_PATH", path_error);
  const uint32_t request_id = this->next_request_id_();
  DownloadContext context;
  const StorageResult open_result = this->storage_.open_download(path, &context);
  if (!open_result.ok())
    return this->send_error_(request, "download", path, open_result);
  ESP_LOGI(TAG, "[req=%" PRIu32 "] download start path=%s bytes=%llu", request_id, path.c_str(),
           static_cast<unsigned long long>(context.size));
  const std::string disposition = "attachment; filename=\"download\"; filename*=UTF-8''" +
                                  url_encode(virtual_basename(path));
  const std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/octet-stream\r\n"
      "Content-Length: " +
      std::to_string(context.size) +
      "\r\n"
      "Cache-Control: no-store\r\n"
      "Content-Disposition: " +
      disposition + "\r\n\r\n";
  if (!send_all(request, response_headers.data(), response_headers.size())) {
    ESP_LOGW(TAG, "[req=%" PRIu32 "] client disconnected before download body path=%s", request_id,
             path.c_str());
    this->storage_.close_download(&context);
    return ESP_FAIL;
  }

  std::vector<uint8_t> buffer(STREAM_BUFFER_SIZE);
  StorageResult result = StorageResult::success();
  uint64_t bytes_sent = 0;
  while (context.bytes_read < context.size) {
    size_t bytes_read = 0;
    result = this->storage_.read_download(&context, buffer.data(), buffer.size(), &bytes_read);
    if (!result.ok() || bytes_read == 0)
      break;
    if (!send_all(request, reinterpret_cast<const char *>(buffer.data()), bytes_read, &bytes_sent)) {
      ESP_LOGW(TAG, "[req=%" PRIu32 "] client disconnected during download path=%s sent=%llu", request_id,
               path.c_str(), static_cast<unsigned long long>(bytes_sent));
      this->storage_.close_download(&context);
      return ESP_FAIL;
    }
  }
  const StorageResult close_result = this->storage_.close_download(&context);
  if (!result.ok() || !close_result.ok() || context.bytes_read != context.size || bytes_sent != context.size) {
    ESP_LOGE(TAG, "[req=%" PRIu32 "] download failed path=%s sent=%llu expected=%llu", request_id, path.c_str(),
             static_cast<unsigned long long>(bytes_sent), static_cast<unsigned long long>(context.size));
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "[req=%" PRIu32 "] download complete path=%s bytes=%llu", request_id, path.c_str(),
           static_cast<unsigned long long>(bytes_sent));
  return ESP_OK;
}

esp_err_t CydSdHttpServer::handle_upload_(httpd_req_t *request) {
  if (!this->authenticate_(request))
    return ESP_OK;
  std::string path, path_error;
  if (!this->request_path_(request, false, &path, &path_error))
    return this->send_request_error_(request, "400 Bad Request", "upload", "", "INVALID_PATH", path_error);
  const size_t content_length_header = httpd_req_get_hdr_value_len(request, "Content-Length");
  if (content_length_header == 0)
    return this->send_request_error_(request, "411 Length Required", "upload", path, "LENGTH_REQUIRED",
                                     "Content-Length is required");
  std::vector<char> length_text(content_length_header + 1, '\0');
  if (httpd_req_get_hdr_value_str(request, "Content-Length", length_text.data(), length_text.size()) != ESP_OK)
    return this->send_request_error_(request, "400 Bad Request", "upload", path, "INVALID_LENGTH",
                                     "Content-Length is invalid");
  char *length_end = nullptr;
  errno = 0;
  const unsigned long long parsed_length = strtoull(length_text.data(), &length_end, 10);
  if (errno != 0 || length_end == length_text.data() || *length_end != '\0' || parsed_length > INT_MAX ||
      static_cast<int>(parsed_length) != request->content_len)
    return this->send_request_error_(request, "400 Bad Request", "upload", path, "INVALID_LENGTH",
                                     "Content-Length is invalid or inconsistent");
  if (parsed_length > this->max_upload_size_)
    return this->send_request_error_(request, "413 Content Too Large", "upload", path, "UPLOAD_TOO_LARGE",
                                     "Upload exceeds max_upload_size");
  const size_t content_type_length = httpd_req_get_hdr_value_len(request, "Content-Type");
  std::vector<char> content_type(content_type_length + 1, '\0');
  const std::string required_content_type = "application/octet-stream";
  if (content_type_length == 0 ||
      httpd_req_get_hdr_value_str(request, "Content-Type", content_type.data(), content_type.size()) != ESP_OK ||
      (std::string(content_type.data()) != required_content_type &&
       std::string(content_type.data()).rfind(required_content_type + ";", 0) != 0))
    return this->send_request_error_(request, "400 Bad Request", "upload", path, "INVALID_CONTENT_TYPE",
                                     "Content-Type must be application/octet-stream");

  bool overwrite = false;
  std::string overwrite_text;
  if (this->query_value_(request, "overwrite", &overwrite_text) && !parse_boolean(overwrite_text, &overwrite))
    return this->send_request_error_(request, "400 Bad Request", "upload", path, "INVALID_OVERWRITE",
                                     "overwrite must be true or false");
  const uint32_t request_id = this->next_request_id_();
  ESP_LOGI(TAG, "[req=%" PRIu32 "] upload start path=%s bytes=%llu overwrite=%s", request_id, path.c_str(),
           static_cast<unsigned long long>(parsed_length), YESNO(overwrite));
  UploadContext context;
  StorageResult result = this->storage_.begin_upload(path, overwrite, parsed_length, request_id, &context);
  if (!result.ok())
    return this->send_error_(request, "upload", path, result, parsed_length);

  auto *async_context = new (std::nothrow) AsyncUploadContext{this, nullptr, context, path, parsed_length, request_id};
  if (async_context == nullptr) {
    this->storage_.abort_upload(&context);
    return this->send_request_error_(request, "500 Internal Server Error", "upload", path, "NO_MEMORY",
                                     "Could not allocate asynchronous upload context");
  }
  const esp_err_t async_error = httpd_req_async_handler_begin(request, &async_context->request);
  if (async_error != ESP_OK) {
    ESP_LOGE(TAG, "[req=%" PRIu32 "] async upload handoff failed: %s", request_id,
             esp_err_to_name(async_error));
    this->storage_.abort_upload(&async_context->upload);
    delete async_context;
    return this->send_request_error_(request, "500 Internal Server Error", "upload", path, "ASYNC_FAILED",
                                     "Could not detach upload from the HTTP server task");
  }
  this->upload_task_handle_ = nullptr;
  if (xTaskCreate(upload_task_, "cyd_sd_upload", 8192, async_context, tskIDLE_PRIORITY + 4,
                  &this->upload_task_handle_) != pdPASS) {
    ESP_LOGE(TAG, "[req=%" PRIu32 "] upload task creation failed", request_id);
    this->storage_.abort_upload(&async_context->upload);
    this->send_request_error_(async_context->request, "500 Internal Server Error", "upload", path, "NO_MEMORY",
                              "Could not start upload worker");
    httpd_req_async_handler_complete(async_context->request);
    delete async_context;
  }
  return ESP_OK;
}

void CydSdHttpServer::process_upload_(AsyncUploadContext *async_context) {
  httpd_req_t *request = async_context->request;
  UploadContext &context = async_context->upload;
  const std::string &path = async_context->path;
  const uint64_t parsed_length = async_context->content_length;
  const uint32_t request_id = async_context->request_id;
  std::vector<uint8_t> buffer(STREAM_BUFFER_SIZE);
  uint64_t remaining = parsed_length;
  unsigned timeout_count = 0;
  StorageResult result = StorageResult::success();
  while (remaining > 0) {
    if (this->stop_requested_) {
      result = {StorageError::BUSY, ECANCELED, static_cast<size_t>(context.received_bytes)};
      this->storage_.abort_upload(&context);
      this->send_error_(request, "upload", path, result, parsed_length);
      return;
    }
    const size_t requested = std::min<uint64_t>(buffer.size(), remaining);
    const int received = httpd_req_recv(request, reinterpret_cast<char *>(buffer.data()), requested);
    if (received == HTTPD_SOCK_ERR_TIMEOUT && timeout_count++ < 3)
      continue;
    if (received <= 0) {
      result = {StorageError::READ_FAILED, received, static_cast<size_t>(context.received_bytes)};
      this->storage_.abort_upload(&context);
      this->send_error_(request, "upload", path, result, parsed_length);
      return;
    }
    timeout_count = 0;
    result = this->storage_.write_upload(&context, buffer.data(), received);
    if (!result.ok()) {
      this->storage_.abort_upload(&context);
      this->send_error_(request, "upload", path, result, parsed_length);
      return;
    }
    remaining -= received;
  }
  result = this->storage_.finish_upload(&context);
  if (!result.ok()) {
    this->send_error_(request, "upload", path, result, parsed_length);
    return;
  }
  ESP_LOGI(TAG, "[req=%" PRIu32 "] upload complete path=%s bytes=%llu duration_ms=%" PRIu32, request_id,
           path.c_str(), static_cast<unsigned long long>(parsed_length), millis() - context.started_ms);
  const std::string json = "{\"ok\":true,\"operation\":\"upload\",\"path\":\"" + json_escape(path) +
                           "\",\"bytes\":" + std::to_string(parsed_length) + "}";
  this->send_json_(request, "200 OK", json);
}

void CydSdHttpServer::upload_task_(void *parameter) {
  auto *context = static_cast<AsyncUploadContext *>(parameter);
  CydSdHttpServer *server = context->server;
  server->process_upload_(context);
  httpd_req_async_handler_complete(context->request);
  delete context;
  server->upload_task_handle_ = nullptr;
  vTaskDelete(nullptr);
}

esp_err_t CydSdHttpServer::handle_delete_file_(httpd_req_t *request) {
  if (!this->authenticate_(request))
    return ESP_OK;
  std::string path, path_error;
  if (!this->request_path_(request, false, &path, &path_error))
    return this->send_request_error_(request, "400 Bad Request", "delete", "", "INVALID_PATH", path_error);
  const uint32_t request_id = this->next_request_id_();
  const StorageResult result = this->storage_.delete_file(path);
  if (!result.ok())
    return this->send_error_(request, "delete", path, result);
  ESP_LOGI(TAG, "[req=%" PRIu32 "] delete complete path=%s", request_id, path.c_str());
  const std::string json = "{\"ok\":true,\"operation\":\"delete\",\"path\":\"" + json_escape(path) +
                           "\",\"bytes\":" + std::to_string(result.bytes_processed) + "}";
  return this->send_json_(request, "200 OK", json);
}

esp_err_t CydSdHttpServer::handle_make_directory_(httpd_req_t *request) {
  if (!this->authenticate_(request))
    return ESP_OK;
  std::string path, path_error;
  if (!this->request_path_(request, false, &path, &path_error))
    return this->send_request_error_(request, "400 Bad Request", "mkdir", "", "INVALID_PATH", path_error);
  if (request->content_len != 0)
    return this->send_request_error_(request, "422 Unprocessable Entity", "mkdir", path, "UNEXPECTED_BODY",
                                     "Directory creation requires an empty body");
  const StorageResult result = this->storage_.make_directory(path);
  if (!result.ok())
    return this->send_error_(request, "mkdir", path, result);
  const std::string json = "{\"ok\":true,\"operation\":\"mkdir\",\"path\":\"" + json_escape(path) + "\"}";
  return this->send_json_(request, "200 OK", json);
}

esp_err_t CydSdHttpServer::handle_delete_directory_(httpd_req_t *request) {
  if (!this->authenticate_(request))
    return ESP_OK;
  std::string path, path_error;
  if (!this->request_path_(request, false, &path, &path_error))
    return this->send_request_error_(request, "400 Bad Request", "rmdir", "", "INVALID_PATH", path_error);
  const StorageResult result = this->storage_.delete_directory(path);
  if (!result.ok())
    return this->send_error_(request, "rmdir", path, result);
  const std::string json = "{\"ok\":true,\"operation\":\"rmdir\",\"path\":\"" + json_escape(path) + "\"}";
  return this->send_json_(request, "200 OK", json);
}

esp_err_t CydSdHttpServer::handle_options_(httpd_req_t *request) {
  httpd_resp_set_hdr(request, "Allow", "GET, POST, DELETE, OPTIONS");
  return httpd_resp_send(request, nullptr, 0);
}

#define CYD_HANDLER(name, method)                               \
  esp_err_t CydSdHttpServer::name(httpd_req_t *request) {       \
    auto *server = static_cast<CydSdHttpServer *>(request->user_ctx); \
    return server->method(request);                             \
  }

CYD_HANDLER(ui_handler_, handle_ui_)
CYD_HANDLER(redirect_handler_, handle_redirect_)
CYD_HANDLER(health_handler_, handle_health_)
CYD_HANDLER(list_handler_, handle_list_)
CYD_HANDLER(download_handler_, handle_download_)
CYD_HANDLER(upload_handler_, handle_upload_)
CYD_HANDLER(delete_file_handler_, handle_delete_file_)
CYD_HANDLER(make_directory_handler_, handle_make_directory_)
CYD_HANDLER(delete_directory_handler_, handle_delete_directory_)
CYD_HANDLER(options_handler_, handle_options_)

#undef CYD_HANDLER

}  // namespace cyd_sd_http_server
}  // namespace esphome
