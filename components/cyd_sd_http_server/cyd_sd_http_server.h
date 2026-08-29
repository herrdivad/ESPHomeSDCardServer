#pragma once

#include <cstdint>
#include <string>

#include "esp_http_server.h"
#include "esphome/core/component.h"
#include "freertos/task.h"
#include "sd_storage.h"

namespace esphome {
namespace cyd_sd_http_server {

class CydSdHttpServer : public Component {
 public:
  void set_port(uint16_t value) { this->port_ = value; }
  void set_control_port(uint16_t value) { this->control_port_ = value; }
  void set_mount_point(const std::string &value) { this->mount_point_ = value; }
  void set_url_prefix(const std::string &value) { this->url_prefix_ = value; }
  void set_format_if_mount_failed(bool value) { this->format_if_mount_failed_ = value; }
  void set_internal_pullups(bool value) { this->internal_pullups_ = value; }
  void set_bus_width(int value) { this->bus_width_ = value; }
  void set_clk_pin(int value) { this->clk_pin_ = value; }
  void set_cmd_pin(int value) { this->cmd_pin_ = value; }
  void set_data0_pin(int value) { this->data0_pin_ = value; }
  void set_data1_pin(int value) { this->data1_pin_ = value; }
  void set_data2_pin(int value) { this->data2_pin_ = value; }
  void set_data3_pin(int value) { this->data3_pin_ = value; }
  void set_max_upload_size(uint32_t value) { this->max_upload_size_ = value; }
  void set_api_key(const std::string &value) { this->api_key_ = value; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  bool teardown() override;

 protected:
  bool start_server_();
  void stop_server_();
  bool authenticate_(httpd_req_t *request);
  bool request_path_(httpd_req_t *request, bool allow_root, std::string *path, std::string *error);
  bool query_value_(httpd_req_t *request, const char *name, std::string *value);
  esp_err_t send_json_(httpd_req_t *request, const char *status, const std::string &json);
  esp_err_t send_error_(httpd_req_t *request, const char *operation, const std::string &path,
                        const StorageResult &result, uint64_t expected_bytes = 0);
  esp_err_t send_request_error_(httpd_req_t *request, const char *status, const char *operation,
                                const std::string &path, const char *error, const std::string &message);
  uint32_t next_request_id_() { return ++this->request_counter_; }

  esp_err_t handle_ui_(httpd_req_t *request);
  esp_err_t handle_redirect_(httpd_req_t *request);
  esp_err_t handle_health_(httpd_req_t *request);
  esp_err_t handle_list_(httpd_req_t *request);
  esp_err_t handle_download_(httpd_req_t *request);
  esp_err_t handle_upload_(httpd_req_t *request);
  esp_err_t handle_delete_file_(httpd_req_t *request);
  esp_err_t handle_make_directory_(httpd_req_t *request);
  esp_err_t handle_delete_directory_(httpd_req_t *request);
  esp_err_t handle_options_(httpd_req_t *request);

  struct AsyncUploadContext {
    CydSdHttpServer *server;
    httpd_req_t *request;
    UploadContext upload;
    std::string path;
    uint64_t content_length;
    uint32_t request_id;
  };
  static void upload_task_(void *parameter);
  void process_upload_(AsyncUploadContext *context);

  static esp_err_t ui_handler_(httpd_req_t *request);
  static esp_err_t redirect_handler_(httpd_req_t *request);
  static esp_err_t health_handler_(httpd_req_t *request);
  static esp_err_t list_handler_(httpd_req_t *request);
  static esp_err_t download_handler_(httpd_req_t *request);
  static esp_err_t upload_handler_(httpd_req_t *request);
  static esp_err_t delete_file_handler_(httpd_req_t *request);
  static esp_err_t make_directory_handler_(httpd_req_t *request);
  static esp_err_t delete_directory_handler_(httpd_req_t *request);
  static esp_err_t options_handler_(httpd_req_t *request);

  uint16_t port_{8080};
  uint16_t control_port_{32769};
  std::string mount_point_{"/sdcard"};
  std::string url_prefix_{"/sd"};
  bool format_if_mount_failed_{false};
  bool internal_pullups_{true};
  int bus_width_{4};
  int clk_pin_{-1};
  int cmd_pin_{-1};
  int data0_pin_{-1};
  int data1_pin_{-1};
  int data2_pin_{-1};
  int data3_pin_{-1};
  uint32_t max_upload_size_{200000000};
  std::string api_key_;
  httpd_handle_t server_{nullptr};
  TaskHandle_t upload_task_handle_{nullptr};
  volatile bool stop_requested_{false};
  uint32_t request_counter_{0};
  SdStorage storage_;
};

}  // namespace cyd_sd_http_server
}  // namespace esphome
