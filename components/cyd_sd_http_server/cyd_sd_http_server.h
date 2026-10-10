#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "esp_http_server.h"
#include "esphome/core/component.h"
#include "freertos/task.h"
#include "sd_storage.h"

#ifdef USE_CYD_SD_LVGL
#include "esphome/components/select/select.h"
#include <lvgl.h>
#endif

#ifdef USE_CYD_SD_HIDDEN_SELECT
#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace cyd_sd_http_server {

#ifdef USE_CYD_SD_LVGL
class CydSdImageSelect;
#endif

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

#ifdef USE_CYD_SD_LVGL
  void set_lvgl_drive_letter(const std::string &value) { this->lvgl_drive_letter_ = value[0]; }
  void set_decoded_image_cache_size(size_t value) { this->decoded_image_cache_size_ = value; }
  void set_lvgl_file_cache_size(size_t value) { this->lvgl_file_cache_size_ = value; }
  void add_display_image(const std::string &name, const std::string &path) {
    this->display_images_.push_back({name, path});
  }
  void add_display_image_widget(lv_obj_t *widget, size_t initial_image_index, const std::string &name);
  void set_image_select(size_t widget_index, CydSdImageSelect *image_select);
  bool select_image(size_t widget_index, size_t image_index);
#endif

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

#ifdef USE_CYD_SD_LVGL
  struct DisplayImage {
    std::string name;
    std::string virtual_path;
  };

  struct CachedImage {
    std::string virtual_path;
    uint32_t generation{0};
    uint64_t last_used{0};
    size_t data_size{0};
    uint8_t *data{nullptr};
    lv_image_dsc_t descriptor{};

    ~CachedImage();
    const void *source() const { return &this->descriptor; }
  };

  struct DisplayImageWidget {
    lv_obj_t *widget{nullptr};
    CydSdImageSelect *image_select{nullptr};
    std::string name;
    size_t initial_image_index{0};
    size_t selected_image_index{0};
    CachedImage *active_cached_image{nullptr};
    std::string active_lvgl_source;
  };

  bool register_lvgl_file_system_();
  bool is_mapped_image_path_(const std::string &virtual_path) const;
  bool is_cache_entry_referenced_(const CachedImage *entry) const;
  CachedImage *find_cached_image_(const std::string &virtual_path, uint32_t generation);
  std::unique_ptr<CachedImage> decode_image_to_cache_(const std::string &virtual_path, uint32_t generation);
  bool copy_decoded_area_(uint8_t *destination, uint32_t destination_stride, const lv_area_t &area,
                          const lv_draw_buf_t *decoded);
  bool ensure_cache_space_(size_t required);
  void evict_cache_entry_(CachedImage *entry);
  void evict_stale_cache_entries_(uint32_t generation);
  void mark_mapped_image_changed_(const std::string &virtual_path);
  static bool lvgl_ready_cb_(lv_fs_drv_t *driver);
  static void *lvgl_open_cb_(lv_fs_drv_t *driver, const char *path, lv_fs_mode_t mode);
  static lv_fs_res_t lvgl_close_cb_(lv_fs_drv_t *driver, void *file);
  static lv_fs_res_t lvgl_read_cb_(lv_fs_drv_t *driver, void *file, void *buffer, uint32_t bytes_to_read,
                                   uint32_t *bytes_read);
  static lv_fs_res_t lvgl_seek_cb_(lv_fs_drv_t *driver, void *file, uint32_t position, lv_fs_whence_t whence);
  static lv_fs_res_t lvgl_tell_cb_(lv_fs_drv_t *driver, void *file, uint32_t *position);
#endif

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

#ifdef USE_CYD_SD_LVGL
  char lvgl_drive_letter_{'S'};
  lv_fs_drv_t lvgl_fs_driver_{};
  bool lvgl_fs_registered_{false};
  std::vector<DisplayImage> display_images_;
  std::vector<DisplayImageWidget> display_image_widgets_;
  size_t decoded_image_cache_size_{0};
  size_t decoded_image_cache_used_{0};
  size_t lvgl_file_cache_size_{0};
  uint64_t cache_use_counter_{0};
  std::atomic<uint32_t> mapped_image_generation_{0};
  std::vector<std::unique_ptr<CachedImage>> decoded_image_cache_;
#endif
};

#ifdef USE_CYD_SD_LVGL
class CydSdImageSelect : public select::Select
#ifdef USE_CYD_SD_HIDDEN_SELECT
                       , public api::CustomAPIDevice
#endif
{
 public:
  CydSdImageSelect(CydSdHttpServer *parent, size_t widget_index)
      : parent_(parent), widget_index_(widget_index) {}
  void publish_image_state(size_t index);
#ifdef USE_CYD_SD_HIDDEN_SELECT
  void set_hidden_status(text_sensor::TextSensor *status, const std::string &action_name) {
    this->image_status_ = status;
    this->action_name_ = action_name;
  }
  void register_hidden_action();
  bool is_hidden() const { return this->image_status_ != nullptr; }
#endif

 protected:
  void control(size_t index) override;
  CydSdHttpServer *parent_;
  size_t widget_index_;
#ifdef USE_CYD_SD_HIDDEN_SELECT
  void select_option_(std::string option);
  text_sensor::TextSensor *image_status_{nullptr};
  std::string action_name_;
  bool action_registered_{false};
#endif
};
#endif

}  // namespace cyd_sd_http_server
}  // namespace esphome
