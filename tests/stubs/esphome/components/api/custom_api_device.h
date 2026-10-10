#pragma once
#include <array>
#include <functional>
#include <map>
#include <string>

namespace esphome {
namespace api {
class CustomAPIDevice {
 public:
  template<typename T>
  void register_service(void (T::*callback)(std::string), const std::string &name,
                        const std::array<std::string, 1> &arguments) {
    ++this->registrations;
    this->argument_name = arguments[0];
    this->services[name] = [this, callback](std::string value) {
      (static_cast<T *>(this)->*callback)(value);
    };
  }
  size_t registrations{0};
  std::string argument_name;
  std::map<std::string, std::function<void(std::string)>> services;
};
}  // namespace api
}  // namespace esphome
