#pragma once
#include <cstdint>
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
struct esp_vfs_fat_sdmmc_mount_config_t {
  bool format_if_mount_failed{false};
  int max_files{0};
  unsigned allocation_unit_size{0};
  bool disk_status_check_enable{false};
  bool use_one_fat{false};
};
inline esp_err_t esp_vfs_fat_sdmmc_mount(const char *, const sdmmc_host_t *, const sdmmc_slot_config_t *,
                                         const esp_vfs_fat_sdmmc_mount_config_t *, sdmmc_card_t **) { return ESP_OK; }
inline esp_err_t esp_vfs_fat_sdcard_unmount(const char *, sdmmc_card_t *) { return ESP_OK; }
inline esp_err_t esp_vfs_fat_info(const char *, uint64_t *total_bytes, uint64_t *free_bytes) {
  *total_bytes = 0;
  *free_bytes = 0;
  return ESP_OK;
}
