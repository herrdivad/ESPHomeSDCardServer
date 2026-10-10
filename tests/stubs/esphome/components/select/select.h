#pragma once
#include <functional>
#include <string>
#include <vector>

namespace esphome {
namespace select {

class Select;
class SelectCall {
 public:
  explicit SelectCall(Select *parent) : parent_(parent) {}
  void set_option(const std::string &option) { this->option_ = option; }
  void perform();

 private:
  Select *parent_;
  std::string option_;
};

class Select {
 public:
  virtual ~Select() = default;
  SelectCall make_call() { return SelectCall(this); }
  const char *option_at(size_t index) const { return this->options.at(index).c_str(); }
  void publish_state(size_t index) {
    this->state = this->options.at(index);
    ++this->publications;
    if (this->callback)
      this->callback(index);
  }
  std::vector<std::string> options;
  std::string state;
  size_t publications{0};
  std::function<void(size_t)> callback;

 protected:
  friend class SelectCall;
  virtual void control(size_t index) = 0;
};

inline void SelectCall::perform() {
  for (size_t index = 0; index < this->parent_->options.size(); ++index) {
    if (this->parent_->options[index] == this->option_) {
      this->parent_->control(index);
      return;
    }
  }
}

}  // namespace select
}  // namespace esphome
