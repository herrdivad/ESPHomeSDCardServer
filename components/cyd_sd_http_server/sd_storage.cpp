#include "sd_storage.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_vfs_fat.h"
#include "esphome/core/log.h"
#include "esphome/core/millis_internal.h"
#include "path_utils.h"

namespace esphome {
namespace cyd_sd_http_server {

static const char *const TAG = "cyd_sd_storage";
static constexpr size_t MAX_CLEANUP_DEPTH = 16;

namespace {
class MutexLock {
 public:
  explicit MutexLock(SemaphoreHandle_t mutex) : mutex_(mutex), locked_(mutex != nullptr) {
    if (this->locked_)
      xSemaphoreTakeRecursive(this->mutex_, portMAX_DELAY);
  }
  ~MutexLock() {
    if (this->locked_)
      xSemaphoreGiveRecursive(this->mutex_);
  }

 private:
  SemaphoreHandle_t mutex_;
  bool locked_;
};

StorageResult failure(StorageError error, int system_errno = 0, size_t bytes = 0) {
  return {error, system_errno, bytes};
}

StorageError errno_to_open_error(int value) {
  if (value == ENOENT)
    return StorageError::NOT_FOUND;
  if (value == ENOSPC)
    return StorageError::NO_SPACE;
  return StorageError::OPEN_FAILED;
}

bool has_suffix_marker(const std::string &name, const char *marker, size_t *marker_offset) {
  const size_t offset = name.rfind(marker);
  if (offset == std::string::npos || name.size() < offset + strlen(marker) + 6 || name.compare(name.size() - 5, 5, ".part") != 0)
    return false;
  for (size_t index = offset + strlen(marker); index < name.size() - 5; index++) {
    if (name[index] < '0' || name[index] > '9')
      return false;
  }
  *marker_offset = offset;
  return true;
}
}  // namespace

const char *storage_error_to_string(StorageError error) {
  switch (error) {
    case StorageError::NONE: return "NONE";
    case StorageError::NOT_MOUNTED: return "NOT_MOUNTED";
    case StorageError::INVALID_PATH: return "INVALID_PATH";
    case StorageError::NOT_FOUND: return "NOT_FOUND";
    case StorageError::ALREADY_EXISTS: return "ALREADY_EXISTS";
    case StorageError::NOT_A_FILE: return "NOT_A_FILE";
    case StorageError::NOT_A_DIRECTORY: return "NOT_A_DIRECTORY";
    case StorageError::OPEN_FAILED: return "OPEN_FAILED";
    case StorageError::READ_FAILED: return "READ_FAILED";
    case StorageError::SEEK_FAILED: return "SEEK_FAILED";
    case StorageError::WRITE_FAILED: return "WRITE_FAILED";
    case StorageError::SHORT_WRITE: return "SHORT_WRITE";
    case StorageError::FLUSH_FAILED: return "FLUSH_FAILED";
    case StorageError::CLOSE_FAILED: return "CLOSE_FAILED";
    case StorageError::RENAME_FAILED: return "RENAME_FAILED";
    case StorageError::DELETE_FAILED: return "DELETE_FAILED";
    case StorageError::CREATE_DIRECTORY_FAILED: return "CREATE_DIRECTORY_FAILED";
    case StorageError::NO_SPACE: return "NO_SPACE";
    case StorageError::BUSY: return "BUSY";
    case StorageError::SIZE_MISMATCH: return "SIZE_MISMATCH";
    case StorageError::LIST_LIMIT_REACHED: return "LIST_LIMIT_REACHED";
  }
  return "UNKNOWN";
}

const char *storage_error_message(StorageError error) {
  switch (error) {
    case StorageError::NONE: return "Operation completed";
    case StorageError::NOT_MOUNTED: return "SD card is not mounted";
    case StorageError::INVALID_PATH: return "Path is invalid";
    case StorageError::NOT_FOUND: return "Path was not found";
    case StorageError::ALREADY_EXISTS: return "Target already exists";
    case StorageError::NOT_A_FILE: return "Path is not a regular file";
    case StorageError::NOT_A_DIRECTORY: return "Path is not a directory";
    case StorageError::OPEN_FAILED: return "File could not be opened";
    case StorageError::READ_FAILED: return "SD card read failed";
    case StorageError::SEEK_FAILED: return "SD card seek failed";
    case StorageError::WRITE_FAILED: return "SD card write failed";
    case StorageError::SHORT_WRITE: return "SD card accepted fewer bytes than requested";
    case StorageError::FLUSH_FAILED: return "File flush failed";
    case StorageError::CLOSE_FAILED: return "File close failed";
    case StorageError::RENAME_FAILED: return "Atomic rename step failed";
    case StorageError::DELETE_FAILED: return "Delete failed";
    case StorageError::CREATE_DIRECTORY_FAILED: return "Directory creation failed";
    case StorageError::NO_SPACE: return "Not enough free space";
    case StorageError::BUSY: return "SD card operation conflicts with an active reader or upload";
    case StorageError::SIZE_MISMATCH: return "Stored size differs from Content-Length";
    case StorageError::LIST_LIMIT_REACHED: return "Directory result limit reached";
  }
  return "Unknown storage error";
}

SdStorage::SdStorage() {
  this->mutex_ = xSemaphoreCreateRecursiveMutex();
  this->upload_mutex_ = xSemaphoreCreateRecursiveMutex();
}

SdStorage::~SdStorage() {
  this->unmount();
  if (this->mutex_ != nullptr)
    vSemaphoreDelete(this->mutex_);
  if (this->upload_mutex_ != nullptr)
    vSemaphoreDelete(this->upload_mutex_);
}

void SdStorage::configure(const std::string &mount_point, bool format_if_mount_failed, bool internal_pullups,
                          int bus_width, int clk_pin, int cmd_pin, int data0_pin, int data1_pin, int data2_pin,
                          int data3_pin) {
  this->mount_point_ = mount_point;
  this->format_if_mount_failed_ = format_if_mount_failed;
  this->internal_pullups_ = internal_pullups;
  this->bus_width_ = bus_width;
  this->clk_pin_ = clk_pin;
  this->cmd_pin_ = cmd_pin;
  this->data0_pin_ = data0_pin;
  this->data1_pin_ = data1_pin;
  this->data2_pin_ = data2_pin;
  this->data3_pin_ = data3_pin;
}

esp_err_t SdStorage::mount() {
  MutexLock lock(this->mutex_);
  if (this->mounted_)
    return ESP_OK;

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = this->bus_width_;
  slot_config.clk = static_cast<gpio_num_t>(this->clk_pin_);
  slot_config.cmd = static_cast<gpio_num_t>(this->cmd_pin_);
  slot_config.d0 = static_cast<gpio_num_t>(this->data0_pin_);
  if (this->bus_width_ == 4) {
    slot_config.d1 = static_cast<gpio_num_t>(this->data1_pin_);
    slot_config.d2 = static_cast<gpio_num_t>(this->data2_pin_);
    slot_config.d3 = static_cast<gpio_num_t>(this->data3_pin_);
  }
  if (this->internal_pullups_)
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  esp_vfs_fat_sdmmc_mount_config_t mount_config{};
  mount_config.format_if_mount_failed = this->format_if_mount_failed_;
  mount_config.max_files = 8;
  mount_config.allocation_unit_size = 16 * 1024;

  ESP_LOGI(TAG, "Mounting SDMMC at %s width=%d CLK=%d CMD=%d D0=%d D1=%d D2=%d D3=%d pullups=%s format=%s",
           this->mount_point_.c_str(), this->bus_width_, this->clk_pin_, this->cmd_pin_, this->data0_pin_,
           this->data1_pin_, this->data2_pin_, this->data3_pin_, YESNO(this->internal_pullups_),
           YESNO(this->format_if_mount_failed_));
  const esp_err_t error =
      esp_vfs_fat_sdmmc_mount(this->mount_point_.c_str(), &host, &slot_config, &mount_config, &this->card_);
  if (error != ESP_OK) {
    this->card_ = nullptr;
    ESP_LOGE(TAG, "SDMMC mount failed: %s (0x%x)", esp_err_to_name(error), static_cast<unsigned>(error));
    return error;
  }
  this->mounted_ = true;
  StorageInfo info;
  this->get_info(&info);
  ESP_LOGI(TAG, "SD mounted: type=%s total=%llu free=%llu", info.card_type.c_str(),
           static_cast<unsigned long long>(info.total_bytes), static_cast<unsigned long long>(info.free_bytes));
  const StorageResult cleanup = this->cleanup_stale_files_();
  if (!cleanup.ok())
    ESP_LOGW(TAG, "Stale upload cleanup incomplete: %s errno=%d", storage_error_to_string(cleanup.error),
             cleanup.system_errno);
  return ESP_OK;
}

void SdStorage::unmount() {
  MutexLock lock(this->mutex_);
  if (!this->mounted_)
    return;
  if (!this->active_readers_.empty()) {
    ESP_LOGW(TAG, "SD unmount deferred: %u active reader(s)", static_cast<unsigned>(this->active_readers_.size()));
    return;
  }
  const esp_err_t error = esp_vfs_fat_sdcard_unmount(this->mount_point_.c_str(), this->card_);
  if (error != ESP_OK)
    ESP_LOGE(TAG, "SD unmount failed: %s", esp_err_to_name(error));
  this->mounted_ = false;
  this->card_ = nullptr;
}

StorageResult SdStorage::get_info(StorageInfo *info) {
  MutexLock lock(this->mutex_);
  info->mounted = this->mounted_;
  info->card_type = "unavailable";
  info->total_bytes = info->used_bytes = info->free_bytes = 0;
  if (!this->mounted_)
    return failure(StorageError::NOT_MOUNTED);

  if (this->card_ != nullptr) {
    if (this->card_->is_mmc)
      info->card_type = "MMC";
    else if (this->card_->is_sdio)
      info->card_type = "SDIO";
    else
      info->card_type = "SD";
  }
  uint64_t total_bytes = 0;
  uint64_t free_bytes = 0;
  const esp_err_t info_error = esp_vfs_fat_info(this->mount_point_.c_str(), &total_bytes, &free_bytes);
  if (info_error != ESP_OK) {
    ESP_LOGE(TAG, "esp_vfs_fat_info failed path=%s error=%s (0x%x)", this->mount_point_.c_str(),
             esp_err_to_name(info_error), static_cast<unsigned>(info_error));
    return failure(StorageError::READ_FAILED, static_cast<int>(info_error));
  }
  info->total_bytes = total_bytes;
  info->free_bytes = free_bytes;
  info->used_bytes = total_bytes >= free_bytes ? total_bytes - free_bytes : 0;
  return StorageResult::success();
}

StorageResult SdStorage::list(const std::string &virtual_path, size_t offset, size_t limit,
                              std::vector<DirectoryEntry> *entries, bool *has_more) {
  MutexLock lock(this->mutex_);
  entries->clear();
  *has_more = false;
  if (!this->mounted_)
    return failure(StorageError::NOT_MOUNTED);
  const std::string physical = join_mount_path(this->mount_point_, virtual_path);
  struct stat root_stat {};
  if (stat(physical.c_str(), &root_stat) != 0)
    return failure(errno == ENOENT ? StorageError::NOT_FOUND : StorageError::READ_FAILED, errno);
  if (!S_ISDIR(root_stat.st_mode))
    return failure(StorageError::NOT_A_DIRECTORY);
  DIR *directory = opendir(physical.c_str());
  if (directory == nullptr)
    return failure(errno_to_open_error(errno), errno);

  size_t visible_index = 0;
  StorageResult result = StorageResult::success();
  while (dirent *entry = readdir(directory)) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    if (strstr(entry->d_name, ".cyd-upload-") != nullptr || strstr(entry->d_name, ".cyd-backup-") != nullptr)
      continue;
    if (visible_index++ < offset)
      continue;
    if (entries->size() >= limit) {
      *has_more = true;
      break;
    }
    const std::string child_virtual = virtual_path == "/" ? "/" + std::string(entry->d_name)
                                                            : virtual_path + "/" + entry->d_name;
    const std::string child_physical = join_mount_path(this->mount_point_, child_virtual);
    struct stat child_stat {};
    if (stat(child_physical.c_str(), &child_stat) != 0) {
      result = failure(StorageError::READ_FAILED, errno, entries->size());
      ESP_LOGE(TAG, "stat failed path=%s errno=%d", child_virtual.c_str(), errno);
      break;
    }
    entries->push_back({entry->d_name, child_virtual, S_ISDIR(child_stat.st_mode),
                        S_ISREG(child_stat.st_mode) ? static_cast<uint64_t>(child_stat.st_size) : 0,
                        static_cast<int64_t>(child_stat.st_mtime)});
  }
  const int close_result = closedir(directory);
  if (close_result != 0 && result.ok())
    result = failure(StorageError::CLOSE_FAILED, errno, entries->size());
  return result;
}

StorageResult SdStorage::make_directory(const std::string &virtual_path) {
  MutexLock lock(this->mutex_);
  if (!this->mounted_)
    return failure(StorageError::NOT_MOUNTED);
  if (this->upload_active_)
    return failure(StorageError::BUSY);
  const std::string physical = join_mount_path(this->mount_point_, virtual_path);
  if (mkdir(physical.c_str(), 0775) != 0) {
    const int saved_errno = errno;
    const StorageError error = saved_errno == EEXIST ? StorageError::ALREADY_EXISTS : StorageError::CREATE_DIRECTORY_FAILED;
    ESP_LOGE(TAG, "mkdir failed path=%s errno=%d", virtual_path.c_str(), saved_errno);
    return failure(error, saved_errno);
  }
  return StorageResult::success();
}

StorageResult SdStorage::delete_directory(const std::string &virtual_path) {
  MutexLock lock(this->mutex_);
  if (!this->mounted_)
    return failure(StorageError::NOT_MOUNTED);
  if (this->upload_active_)
    return failure(StorageError::BUSY);
  if (this->path_has_active_reader_(virtual_path, true))
    return failure(StorageError::BUSY);
  const std::string physical = join_mount_path(this->mount_point_, virtual_path);
  struct stat info {};
  if (stat(physical.c_str(), &info) != 0)
    return failure(errno == ENOENT ? StorageError::NOT_FOUND : StorageError::DELETE_FAILED, errno);
  if (!S_ISDIR(info.st_mode))
    return failure(StorageError::NOT_A_DIRECTORY);
  if (rmdir(physical.c_str()) != 0) {
    const int saved_errno = errno;
    ESP_LOGE(TAG, "rmdir failed path=%s errno=%d", virtual_path.c_str(), saved_errno);
    return failure(StorageError::DELETE_FAILED, saved_errno);
  }
  return StorageResult::success();
}

StorageResult SdStorage::delete_file(const std::string &virtual_path) {
  MutexLock lock(this->mutex_);
  if (!this->mounted_)
    return failure(StorageError::NOT_MOUNTED);
  if (this->upload_active_)
    return failure(StorageError::BUSY);
  if (this->path_has_active_reader_(virtual_path))
    return failure(StorageError::BUSY);
  const std::string physical = join_mount_path(this->mount_point_, virtual_path);
  struct stat info {};
  if (stat(physical.c_str(), &info) != 0)
    return failure(errno == ENOENT ? StorageError::NOT_FOUND : StorageError::DELETE_FAILED, errno);
  if (!S_ISREG(info.st_mode))
    return failure(StorageError::NOT_A_FILE);
  if (unlink(physical.c_str()) != 0) {
    const int saved_errno = errno;
    ESP_LOGE(TAG, "unlink failed path=%s errno=%d", virtual_path.c_str(), saved_errno);
    return failure(StorageError::DELETE_FAILED, saved_errno);
  }
  return StorageResult::success(static_cast<size_t>(info.st_size));
}

std::string SdStorage::make_backup_path_(const std::string &target, uint32_t request_id) const {
  return target + ".cyd-backup-" + std::to_string(request_id) + ".part";
}

StorageResult SdStorage::begin_upload(const std::string &virtual_path, bool overwrite, uint64_t expected_bytes,
                                      uint32_t request_id, UploadContext *context) {
  {
    MutexLock upload_lock(this->upload_mutex_);
    if (this->upload_active_)
      return failure(StorageError::BUSY);
    this->upload_active_ = true;
    context->active = true;
  }

  MutexLock lock(this->mutex_);
  if (!this->mounted_) {
    this->release_upload_(context);
    return failure(StorageError::NOT_MOUNTED);
  }
  if (this->path_has_active_reader_(virtual_path)) {
    this->release_upload_(context);
    return failure(StorageError::BUSY);
  }
  const std::string target = join_mount_path(this->mount_point_, virtual_path);
  struct stat target_stat {};
  if (stat(target.c_str(), &target_stat) == 0) {
    if (!S_ISREG(target_stat.st_mode)) {
      this->release_upload_(context);
      return failure(StorageError::NOT_A_FILE);
    }
    if (!overwrite) {
      this->release_upload_(context);
      return failure(StorageError::ALREADY_EXISTS);
    }
  } else if (errno != ENOENT) {
    const int saved_errno = errno;
    this->release_upload_(context);
    return failure(StorageError::OPEN_FAILED, saved_errno);
  }

  StorageInfo info;
  const StorageResult info_result = this->get_info(&info);
  if (!info_result.ok()) {
    this->release_upload_(context);
    return info_result;
  }
  if (expected_bytes > info.free_bytes) {
    this->release_upload_(context);
    return failure(StorageError::NO_SPACE);
  }

  context->target_virtual_path = virtual_path;
  context->target_physical_path = target;
  context->temporary_physical_path = this->mount_point_ + "/.cyd-upload-" + std::to_string(request_id) + ".part";
  context->backup_physical_path = this->make_backup_path_(target, request_id);
  context->expected_bytes = expected_bytes;
  context->received_bytes = 0;
  context->request_id = request_id;
  context->started_ms = millis();
  context->overwrite = overwrite;

  unlink(context->temporary_physical_path.c_str());
  context->file = fopen(context->temporary_physical_path.c_str(), "wb");
  if (context->file == nullptr) {
    const int saved_errno = errno;
    ESP_LOGE(TAG, "[req=%" PRIu32 "] open temporary upload failed path=%s errno=%d", request_id,
             context->temporary_physical_path.c_str(), saved_errno);
    this->release_upload_(context);
    return failure(errno_to_open_error(saved_errno), saved_errno);
  }
  ESP_LOGD(TAG, "[req=%" PRIu32 "] temporary file=%s", request_id, context->temporary_physical_path.c_str());
  return StorageResult::success();
}

StorageResult SdStorage::write_upload(UploadContext *context, const uint8_t *data, size_t length) {
  MutexLock lock(this->mutex_);
  if (!context->active || context->file == nullptr)
    return failure(StorageError::WRITE_FAILED, EBADF, context->received_bytes);
  errno = 0;
  const size_t written = fwrite(data, 1, length, context->file);
  context->received_bytes += written;
  if (written != length) {
    const int saved_errno = errno;
    ESP_LOGE(TAG, "[req=%" PRIu32 "] write failed path=%s requested=%u written=%u errno=%d", context->request_id,
             context->temporary_physical_path.c_str(), static_cast<unsigned>(length), static_cast<unsigned>(written),
             saved_errno);
    return failure(saved_errno == ENOSPC ? StorageError::NO_SPACE : StorageError::SHORT_WRITE, saved_errno,
                   context->received_bytes);
  }
  if (ferror(context->file))
    return failure(StorageError::WRITE_FAILED, errno, context->received_bytes);
  return StorageResult::success(context->received_bytes);
}

StorageResult SdStorage::finish_upload(UploadContext *context) {
  MutexLock lock(this->mutex_);
  if (!context->active || context->file == nullptr)
    return failure(StorageError::WRITE_FAILED, EBADF, context->received_bytes);
  StorageResult result = StorageResult::success(context->received_bytes);
  if (context->received_bytes != context->expected_bytes) {
    result = failure(StorageError::SIZE_MISMATCH, 0, context->received_bytes);
  } else if (fflush(context->file) != 0) {
    result = failure(StorageError::FLUSH_FAILED, errno, context->received_bytes);
  } else if (fsync(fileno(context->file)) != 0) {
    result = failure(StorageError::FLUSH_FAILED, errno, context->received_bytes);
  }
  if (fclose(context->file) != 0 && result.ok())
    result = failure(StorageError::CLOSE_FAILED, errno, context->received_bytes);
  context->file = nullptr;

  struct stat temp_stat {};
  if (result.ok() && (stat(context->temporary_physical_path.c_str(), &temp_stat) != 0 ||
                      static_cast<uint64_t>(temp_stat.st_size) != context->expected_bytes))
    result = failure(StorageError::SIZE_MISMATCH, errno, context->received_bytes);

  bool backup_created = false;
  if (result.ok() && context->overwrite) {
    struct stat target_stat {};
    if (stat(context->target_physical_path.c_str(), &target_stat) == 0) {
      unlink(context->backup_physical_path.c_str());
      if (rename(context->target_physical_path.c_str(), context->backup_physical_path.c_str()) != 0)
        result = failure(StorageError::RENAME_FAILED, errno, context->received_bytes);
      else
        backup_created = true;
    }
  }
  if (result.ok() && rename(context->temporary_physical_path.c_str(), context->target_physical_path.c_str()) != 0) {
    result = failure(StorageError::RENAME_FAILED, errno, context->received_bytes);
    if (backup_created && rename(context->backup_physical_path.c_str(), context->target_physical_path.c_str()) != 0)
      ESP_LOGE(TAG, "[req=%" PRIu32 "] CRITICAL: failed to restore backup path=%s errno=%d", context->request_id,
               context->target_virtual_path.c_str(), errno);
  } else if (result.ok() && backup_created && unlink(context->backup_physical_path.c_str()) != 0) {
    // The new target is already durable and complete. Keep the operation successful;
    // boot cleanup will remove this stale hidden copy after confirming the target exists.
    ESP_LOGW(TAG, "[req=%" PRIu32 "] committed upload but backup cleanup is deferred path=%s errno=%d",
             context->request_id,
             context->backup_physical_path.c_str(), errno);
  }

  if (!result.ok()) {
    ESP_LOGE(TAG, "[req=%" PRIu32 "] upload finalize failed target=%s error=%s errno=%d", context->request_id,
             context->target_virtual_path.c_str(), storage_error_to_string(result.error), result.system_errno);
    unlink(context->temporary_physical_path.c_str());
  }
  this->release_upload_(context);
  return result;
}

void SdStorage::release_upload_(UploadContext *context) {
  context->active = false;
  MutexLock upload_lock(this->upload_mutex_);
  this->upload_active_ = false;
}

void SdStorage::abort_upload(UploadContext *context) {
  MutexLock lock(this->mutex_);
  if (context->file != nullptr) {
    if (fclose(context->file) != 0)
      ESP_LOGE(TAG, "[req=%" PRIu32 "] close after abort failed errno=%d", context->request_id, errno);
    context->file = nullptr;
  }
  if (!context->temporary_physical_path.empty() && unlink(context->temporary_physical_path.c_str()) != 0 && errno != ENOENT)
    ESP_LOGE(TAG, "[req=%" PRIu32 "] temporary cleanup failed errno=%d", context->request_id, errno);
  ESP_LOGW(TAG, "[req=%" PRIu32 "] upload aborted target=%s received=%llu", context->request_id,
           context->target_virtual_path.c_str(), static_cast<unsigned long long>(context->received_bytes));
  this->release_upload_(context);
}

StorageResult SdStorage::open_download(const std::string &virtual_path, DownloadContext *context) {
  MutexLock lock(this->mutex_);
  if (!this->mounted_)
    return failure(StorageError::NOT_MOUNTED);
  if (this->upload_active_)
    return failure(StorageError::BUSY);
  const std::string physical = join_mount_path(this->mount_point_, virtual_path);
  struct stat info {};
  if (stat(physical.c_str(), &info) != 0)
    return failure(errno == ENOENT ? StorageError::NOT_FOUND : StorageError::OPEN_FAILED, errno);
  if (!S_ISREG(info.st_mode))
    return failure(StorageError::NOT_A_FILE);
  context->file = fopen(physical.c_str(), "rb");
  if (context->file == nullptr)
    return failure(errno_to_open_error(errno), errno);
  context->virtual_path = virtual_path;
  context->size = info.st_size;
  context->bytes_read = 0;
  context->reader_registered = true;
  this->active_readers_[virtual_path]++;
  return StorageResult::success();
}

StorageResult SdStorage::read_download(DownloadContext *context, uint8_t *buffer, size_t capacity,
                                       size_t *bytes_read) {
  MutexLock lock(this->mutex_);
  *bytes_read = fread(buffer, 1, capacity, context->file);
  context->bytes_read += *bytes_read;
  if (*bytes_read < capacity && ferror(context->file)) {
    const int saved_errno = errno;
    ESP_LOGE(TAG, "download read failed path=%s errno=%d", context->virtual_path.c_str(), saved_errno);
    return failure(StorageError::READ_FAILED, saved_errno, context->bytes_read);
  }
  return StorageResult::success(context->bytes_read);
}

StorageResult SdStorage::seek_download(DownloadContext *context, uint32_t position) {
  MutexLock lock(this->mutex_);
  if (context == nullptr || context->file == nullptr)
    return failure(StorageError::SEEK_FAILED, EBADF);
  if (position > context->size)
    return failure(StorageError::SEEK_FAILED, EINVAL, context->bytes_read);
  if (fseek(context->file, static_cast<long>(position), SEEK_SET) != 0)
    return failure(StorageError::SEEK_FAILED, errno, context->bytes_read);
  context->bytes_read = position;
  return StorageResult::success(context->bytes_read);
}

StorageResult SdStorage::tell_download(DownloadContext *context, uint32_t *position) {
  MutexLock lock(this->mutex_);
  if (context == nullptr || context->file == nullptr || position == nullptr)
    return failure(StorageError::SEEK_FAILED, EINVAL);
  const long current = ftell(context->file);
  if (current < 0)
    return failure(StorageError::SEEK_FAILED, errno, context->bytes_read);
  *position = static_cast<uint32_t>(current);
  return StorageResult::success(*position);
}

StorageResult SdStorage::close_download(DownloadContext *context) {
  MutexLock lock(this->mutex_);
  if (context == nullptr)
    return StorageResult::success();
  if (context->file == nullptr) {
    this->release_reader_(context);
    return StorageResult::success(context->bytes_read);
  }
  if (fclose(context->file) != 0) {
    const int saved_errno = errno;
    context->file = nullptr;
    this->release_reader_(context);
    return failure(StorageError::CLOSE_FAILED, saved_errno, context->bytes_read);
  }
  context->file = nullptr;
  this->release_reader_(context);
  return StorageResult::success(context->bytes_read);
}

bool SdStorage::path_has_active_reader_(const std::string &virtual_path, bool include_descendants) const {
  for (const auto &reader : this->active_readers_) {
    if (reader.second == 0)
      continue;
    if (reader.first == virtual_path)
      return true;
    if (include_descendants && virtual_path == "/")
      return true;
    if (include_descendants && reader.first.size() > virtual_path.size() &&
        reader.first.compare(0, virtual_path.size(), virtual_path) == 0 && reader.first[virtual_path.size()] == '/')
      return true;
  }
  return false;
}

void SdStorage::release_reader_(DownloadContext *context) {
  if (!context->reader_registered)
    return;
  const auto reader = this->active_readers_.find(context->virtual_path);
  if (reader != this->active_readers_.end()) {
    if (reader->second > 1)
      reader->second--;
    else
      this->active_readers_.erase(reader);
  }
  context->reader_registered = false;
}

StorageResult SdStorage::cleanup_stale_files_() { return this->cleanup_directory_(this->mount_point_, 0); }

StorageResult SdStorage::cleanup_directory_(const std::string &physical_path, size_t depth) {
  if (depth > MAX_CLEANUP_DEPTH)
    return StorageResult::success();
  DIR *directory = opendir(physical_path.c_str());
  if (directory == nullptr)
    return failure(StorageError::OPEN_FAILED, errno);
  StorageResult result = StorageResult::success();
  while (dirent *entry = readdir(directory)) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    const std::string child = physical_path + "/" + entry->d_name;
    struct stat info {};
    if (stat(child.c_str(), &info) != 0) {
      result = failure(StorageError::READ_FAILED, errno);
      break;
    }
    if (S_ISDIR(info.st_mode)) {
      result = this->cleanup_directory_(child, depth + 1);
      if (!result.ok())
        break;
      continue;
    }
    size_t marker_offset = 0;
    const std::string name(entry->d_name);
    if (has_suffix_marker(name, ".cyd-upload-", &marker_offset)) {
      if (unlink(child.c_str()) != 0) {
        result = failure(StorageError::DELETE_FAILED, errno);
        break;
      }
      ESP_LOGW(TAG, "Removed stale upload file %s", child.c_str());
    } else if (has_suffix_marker(name, ".cyd-backup-", &marker_offset)) {
      const std::string original = physical_path + "/" + name.substr(0, marker_offset);
      struct stat original_info {};
      if (stat(original.c_str(), &original_info) == 0) {
        if (unlink(child.c_str()) != 0) {
          result = failure(StorageError::DELETE_FAILED, errno);
          break;
        }
        ESP_LOGW(TAG, "Removed completed overwrite backup %s", child.c_str());
      } else if (errno == ENOENT) {
        if (rename(child.c_str(), original.c_str()) != 0) {
          result = failure(StorageError::RENAME_FAILED, errno);
          break;
        }
        ESP_LOGW(TAG, "Restored interrupted overwrite backup %s", original.c_str());
      }
    }
  }
  if (closedir(directory) != 0 && result.ok())
    result = failure(StorageError::CLOSE_FAILED, errno);
  return result;
}

}  // namespace cyd_sd_http_server
}  // namespace esphome
