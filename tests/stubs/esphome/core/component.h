#pragma once
#include <cstdint>
namespace esphome {
struct LogString {};
namespace setup_priority { static constexpr float AFTER_WIFI = 200.0f; }
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void dump_config() {}
  virtual float get_setup_priority() const { return 0; }
  virtual bool teardown() { return true; }
  void status_set_error(const LogString *) {}
  void mark_failed(const LogString *) {}
};
}  // namespace esphome

