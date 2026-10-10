#include <cassert>
#include "cyd_sd_http_server.h"

using namespace esphome;
using namespace esphome::cyd_sd_http_server;

// Replace storage/LVGL with a controller that can accept or reject a selection.
// This test exercises the real Select/API adapter, not physical image rendering.
static CydSdImageSelect *selections[2];
static bool storage_available = true;
static size_t controller_calls = 0;

namespace esphome {
namespace cyd_sd_http_server {
SdStorage::SdStorage() = default;
SdStorage::~SdStorage() = default;
CydSdHttpServer::CachedImage::~CachedImage() = default;
void CydSdHttpServer::setup() {}
void CydSdHttpServer::dump_config() {}
bool CydSdHttpServer::teardown() { return true; }
bool CydSdHttpServer::select_image(size_t widget_index, size_t image_index) {
  ++controller_calls;
  if (!storage_available)
    return false;
  selections[widget_index]->publish_image_state(image_index);
  return true;
}
}  // namespace cyd_sd_http_server
}  // namespace esphome

int main() {
  CydSdHttpServer server;
  CydSdImageSelect hidden(&server, 0);
  CydSdImageSelect visible(&server, 1);
  selections[0] = &hidden;
  selections[1] = &visible;
  hidden.options = visible.options = {"Day", "Night"};
  text_sensor::TextSensor status;
  hidden.set_hidden_status(&status, "cyd_sd_set_image");

  hidden.register_hidden_action();
  hidden.register_hidden_action();
  visible.register_hidden_action();
  assert(hidden.registrations == 1);
  assert(visible.registrations == 0);
  assert(hidden.argument_name == "option");
  assert(status.publications == 0);
  assert(hidden.is_hidden() && !visible.is_hidden());

  // Initial image publication and subsequent API changes share the same state.
  hidden.publish_image_state(0);
  assert(status.state == "Day" && hidden.state == "Day");
  auto invoke = hidden.services.at("cyd_sd_set_image");
  invoke("Night");
  assert(status.state == "Night" && hidden.state == "Night");
  assert(visible.publications == 0);
  assert(controller_calls == 1);

  // Unknown names and paths never reach the controller.
  invoke("Missing");
  invoke("/images/Day.bin");
  invoke("../Day.bin");
  invoke("");
  assert(controller_calls == 1);
  assert(status.publications == 2);

  // A failed controller selection must not publish an optimistic state.
  storage_available = false;
  invoke("Day");
  assert(status.state == "Night" && hidden.state == "Night");
  assert(status.publications == 2);
  storage_available = true;

  // Reselecting an option still reaches the controller to reload replaced files.
  invoke("Night");
  assert(controller_calls == 3);
  assert(status.publications == 3);

  // Local Select calls remain supported for both modes.
  auto local = hidden.make_call();
  local.set_option("Day");
  local.perform();
  assert(status.state == "Day");
  auto manual = visible.make_call();
  manual.set_option("Night");
  manual.perform();
  assert(visible.state == "Night" && status.state == "Day");

  // A synchronous on_value action can change the image again without stale status.
  hidden.callback = [&hidden](size_t index) {
    if (index == 0) {
      auto next = hidden.make_call();
      next.set_option("Night");
      next.perform();
    }
  };
  invoke("Day");
  assert(hidden.state == "Night" && status.state == "Night");
}
