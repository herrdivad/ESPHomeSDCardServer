#include "cyd_sd_http_server.h"

#ifdef USE_CYD_SD_DIAGNOSIS

#include "esp_heap_caps.h"
#include "esphome/core/log.h"
#include "esphome/core/millis_internal.h"

namespace esphome {
namespace cyd_sd_http_server {

static const char *const TAG = "cyd_sd_diagnosis";
static constexpr uint32_t DIAGNOSIS_INTERVAL_MS = 10000;
static constexpr uint32_t PSRAM_CAPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

namespace {
void publish_changed(sensor::Sensor *target, size_t bytes) {
  if (target == nullptr)
    return;
  const float value = static_cast<float>(bytes);
  if (!target->has_state() || target->state != value)
    target->publish_state(value);
}
}  // namespace

void CydSdHttpServer::publish_diagnosis_() {
  size_t reclaimable = 0;
  for (const auto &entry : this->decoded_image_cache_) {
    if (!this->is_cache_entry_referenced_(entry.get()))
      reclaimable += entry->data_size;
  }

  publish_changed(this->image_cache_limit_sensor_, this->configured_image_cache_size_);
  publish_changed(this->image_cache_used_sensor_, this->decoded_image_cache_used_);
  publish_changed(this->image_cache_reclaimable_sensor_, reclaimable);
  publish_changed(this->psram_total_sensor_, heap_caps_get_total_size(PSRAM_CAPS));
  publish_changed(this->psram_free_sensor_, heap_caps_get_free_size(PSRAM_CAPS));
  publish_changed(this->psram_largest_free_sensor_, heap_caps_get_largest_free_block(PSRAM_CAPS));
}

void CydSdHttpServer::loop() {
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - this->last_diagnosis_publish_ms_) < DIAGNOSIS_INTERVAL_MS)
    return;
  this->last_diagnosis_publish_ms_ = now;
  this->publish_diagnosis_();
}

void CydSdHttpServer::release_unused_image_cache() {
  size_t released_entries = 0;
  size_t released_bytes = 0;
  while (true) {
    CachedImage *candidate = nullptr;
    for (const auto &entry : this->decoded_image_cache_) {
      if (!this->is_cache_entry_referenced_(entry.get())) {
        candidate = entry.get();
        released_bytes += entry->data_size;
        released_entries++;
        break;
      }
    }
    if (candidate == nullptr)
      break;
    this->evict_cache_entry_(candidate);
  }
  ESP_LOGI(TAG, "Released unused decoded images: %u entries, %u bytes", static_cast<unsigned>(released_entries),
           static_cast<unsigned>(released_bytes));
  this->publish_diagnosis_();
}

void CydSdCacheReleaseButton::press_action() {
  if (this->parent_ != nullptr)
    this->parent_->release_unused_image_cache();
}

}  // namespace cyd_sd_http_server
}  // namespace esphome

#endif
