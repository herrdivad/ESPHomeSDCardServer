#include "cyd_sd_http_server.h"

#ifdef USE_CYD_SD_LVGL
#include "esphome/core/log.h"

namespace esphome {
namespace cyd_sd_http_server {

static const char *const TAG = "cyd_sd_image_select";

void CydSdImageSelect::control(size_t index) {
  if (!this->parent_->select_image(this->widget_index_, index))
    ESP_LOGW(TAG, "Display image selection widget=%u image=%u failed", static_cast<unsigned>(this->widget_index_),
             static_cast<unsigned>(index));
}

void CydSdImageSelect::publish_image_state(size_t index) {
#ifdef USE_CYD_SD_HIDDEN_SELECT
  // Publish before Select callbacks, which may synchronously select another image.
  if (this->image_status_ != nullptr)
    this->image_status_->publish_state(this->option_at(index));
#endif
  this->publish_state(index);
}

#ifdef USE_CYD_SD_HIDDEN_SELECT
void CydSdImageSelect::register_hidden_action() {
  if (this->image_status_ == nullptr || this->action_registered_)
    return;
  this->register_service(&CydSdImageSelect::select_option_, this->action_name_, {"option"});
  this->action_registered_ = true;
}

void CydSdImageSelect::select_option_(std::string option) {
  // SelectCall validates the name against the shared allow-list before control().
  auto call = this->make_call();
  call.set_option(option);
  call.perform();
}
#endif

}  // namespace cyd_sd_http_server
}  // namespace esphome
#endif
