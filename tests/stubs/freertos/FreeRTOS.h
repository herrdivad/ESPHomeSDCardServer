#pragma once
#include <cstdint>
using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;
static constexpr BaseType_t pdPASS = 1;
static constexpr TickType_t portMAX_DELAY = 0xffffffffU;
static constexpr UBaseType_t tskIDLE_PRIORITY = 0;

