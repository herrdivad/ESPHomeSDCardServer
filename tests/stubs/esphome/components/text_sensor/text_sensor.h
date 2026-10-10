#pragma once
#include <string>

namespace esphome {
namespace text_sensor {
class TextSensor {
 public:
  void publish_state(const std::string &value) {
    this->state = value;
    ++this->publications;
  }
  std::string state;
  size_t publications{0};
};
}  // namespace text_sensor
}  // namespace esphome
