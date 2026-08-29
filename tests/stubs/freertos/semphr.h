#pragma once
#include "freertos/FreeRTOS.h"
using SemaphoreHandle_t = void *;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<void *>(1); }
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return reinterpret_cast<void *>(1); }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return pdPASS; }
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t) { return pdPASS; }
inline BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t, TickType_t) { return pdPASS; }
inline BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t) { return pdPASS; }
inline void vSemaphoreDelete(SemaphoreHandle_t) {}
