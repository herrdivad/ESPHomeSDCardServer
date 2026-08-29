#pragma once
#include "esphome/core/component.h"
template<typename... Args> inline void esphome_stub_log(const char *, const char *, Args...) {}
#define ESP_LOGI(tag, format, ...) esphome_stub_log(tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) esphome_stub_log(tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) esphome_stub_log(tag, format, ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) esphome_stub_log(tag, format, ##__VA_ARGS__)
#define ESP_LOGCONFIG(tag, format, ...) esphome_stub_log(tag, format, ##__VA_ARGS__)
#define YESNO(value) ((value) ? "YES" : "NO")
#define LOG_STR(value) (static_cast<const esphome::LogString *>(nullptr))
