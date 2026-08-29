#pragma once
#include "freertos/FreeRTOS.h"
using TaskHandle_t = void *;
using TaskFunction_t = void (*)(void *);
inline BaseType_t xTaskCreate(TaskFunction_t, const char *, uint32_t, void *, UBaseType_t, TaskHandle_t *handle) {
  *handle = reinterpret_cast<void *>(1);
  return pdPASS;
}
inline void vTaskDelete(TaskHandle_t) {}

