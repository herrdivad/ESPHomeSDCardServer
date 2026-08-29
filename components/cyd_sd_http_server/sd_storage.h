#pragma once

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdmmc_cmd.h"

namespace esphome {
namespace cyd_sd_http_server {

enum class StorageError {
  NONE,
  NOT_MOUNTED,
  INVALID_PATH,
  NOT_FOUND,
  ALREADY_EXISTS,
  NOT_A_FILE,
  NOT_A_DIRECTORY,
  OPEN_FAILED,
  READ_FAILED,
  WRITE_FAILED,
  SHORT_WRITE,
  FLUSH_FAILED,
  CLOSE_FAILED,
  RENAME_FAILED,
  DELETE_FAILED,
  CREATE_DIRECTORY_FAILED,
  NO_SPACE,
  BUSY,
  SIZE_MISMATCH,
  LIST_LIMIT_REACHED,
};

struct StorageResult {
  StorageError error{StorageError::NONE};
  int system_errno{0};
  size_t bytes_processed{0};

  bool ok() const { return this->error == StorageError::NONE; }
  static StorageResult success(size_t bytes = 0) { return {StorageError::NONE, 0, bytes}; }
};

struct StorageInfo {
  bool mounted{false};
  std::string card_type{"unavailable"};
  uint64_t total_bytes{0};
  uint64_t used_bytes{0};
  uint64_t free_bytes{0};
};

struct DirectoryEntry {
  std::string name;
  std::string path;
  bool is_directory{false};
  uint64_t size{0};
  int64_t modified_time{0};
};

struct UploadContext {
  FILE *file{nullptr};
  std::string target_virtual_path;
  std::string target_physical_path;
  std::string temporary_physical_path;
  std::string backup_physical_path;
  uint64_t expected_bytes{0};
  uint64_t received_bytes{0};
  uint32_t request_id{0};
  uint32_t started_ms{0};
  bool overwrite{false};
  bool active{false};
};

struct DownloadContext {
  FILE *file{nullptr};
  std::string virtual_path;
  uint64_t size{0};
  uint64_t bytes_read{0};
};

const char *storage_error_to_string(StorageError error);
const char *storage_error_message(StorageError error);

class SdStorage {
 public:
  SdStorage();
  ~SdStorage();

  void configure(const std::string &mount_point, bool format_if_mount_failed, bool internal_pullups, int bus_width,
                 int clk_pin, int cmd_pin, int data0_pin, int data1_pin, int data2_pin, int data3_pin);
  esp_err_t mount();
  void unmount();
  bool is_mounted() const { return this->mounted_; }
  bool upload_active() const { return this->upload_active_; }

  StorageResult get_info(StorageInfo *info);
  StorageResult list(const std::string &virtual_path, size_t offset, size_t limit,
                     std::vector<DirectoryEntry> *entries, bool *has_more);
  StorageResult make_directory(const std::string &virtual_path);
  StorageResult delete_directory(const std::string &virtual_path);
  StorageResult delete_file(const std::string &virtual_path);

  StorageResult begin_upload(const std::string &virtual_path, bool overwrite, uint64_t expected_bytes,
                             uint32_t request_id, UploadContext *context);
  StorageResult write_upload(UploadContext *context, const uint8_t *data, size_t length);
  StorageResult finish_upload(UploadContext *context);
  void abort_upload(UploadContext *context);

  StorageResult open_download(const std::string &virtual_path, DownloadContext *context);
  StorageResult read_download(DownloadContext *context, uint8_t *buffer, size_t capacity, size_t *bytes_read);
  StorageResult close_download(DownloadContext *context);

 private:
  StorageResult cleanup_stale_files_();
  StorageResult cleanup_directory_(const std::string &physical_path, size_t depth);
  void release_upload_(UploadContext *context);
  std::string make_backup_path_(const std::string &target, uint32_t request_id) const;

  std::string mount_point_{"/sdcard"};
  bool format_if_mount_failed_{false};
  bool internal_pullups_{true};
  int bus_width_{4};
  int clk_pin_{-1};
  int cmd_pin_{-1};
  int data0_pin_{-1};
  int data1_pin_{-1};
  int data2_pin_{-1};
  int data3_pin_{-1};
  bool mounted_{false};
  volatile bool upload_active_{false};
  sdmmc_card_t *card_{nullptr};
  SemaphoreHandle_t mutex_{nullptr};
  SemaphoreHandle_t upload_mutex_{nullptr};
};

}  // namespace cyd_sd_http_server
}  // namespace esphome

